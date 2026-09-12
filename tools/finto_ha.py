#!/usr/bin/env python3
"""Un finto Home Assistant, quel tanto che basta a provare il pannello.

Serve a una cosa sola: far succedere davvero le situazioni che
`11-collaudo.md` §1 elenca per la Fase 3, e che con Home Assistant vero si
possono solo aspettare. Un token sbagliato si prova staccando un cavo e
rimettendolo; qui si prova con `--token-cattivo`. Una caduta del
collegamento a metà di un comando, con Home Assistant vero, è un problema
di tempismo; qui è `--cadi-dopo 3`.

Parla WebSocket (RFC 6455, solo frame di testo) e il protocollo di Home
Assistant per le quattro cose che il pannello usa: `auth`,
`subscribe_events`, `subscribe_entities`, `get_states`,
`call_service`. Non è un'emulazione: è
un attrezzo, e finisce dove finisce quello che il pannello chiede.

Gli stati iniziali li genera da `04-config.example.json`, così le entità
che risponde sono esattamente quelle che la configurazione nomina — che è
la condizione normale. Con `--manca` se ne tolgono alcune, e si vede il
pannello dire "non disponibile" senza smettere di funzionare.

    python3 tools/finto_ha.py --porta 8123
    python3 tools/finto_ha.py --token-cattivo
    python3 tools/finto_ha.py --cadi-dopo 5
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import re
import socket
import struct
import sys
import threading
import time
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
ESEMPIO = RADICE / "docs" / "04-config.example.json"
GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

# Uno stato verosimile per dominio: quello che Home Assistant risponderebbe
# per un'entita di quel tipo appena guardata.
PER_DOMINIO = {
    "light":         ("on",     {"brightness": 166, "friendly_name": ""}),
    "switch":        ("off",    {}),
    "binary_sensor": ("off",    {"device_class": "opening"}),
    "sensor":        ("21.4",   {"unit_of_measurement": "°C"}),
    "climate":       ("cool",   {"current_temperature": 30.0,
                                 "temperature": 28.0,
                                 "fan_mode": "medium",
                                 "swing_mode": "middle",
                                 "hvac_modes": ["off", "heat", "cool", "auto",
                                                "dry", "fan_only"]}),
    # Gli attributi veri di un timer fermo. `remaining` c'e e ha i secondi;
    # `finishes_at` **non c'e**, perche un timer fermo non finisce. Sono
    # esattamente questi: il pannello ha passato un mese a leggere un
    # `remaining_s` che qui non c'era e in Home Assistant nemmeno.
    "timer":         ("idle",   {"duration": "1:30:00",
                                 "remaining": "1:30:00",
                                 "editable": True, "restore": False}),
    "automation":    ("on",     {}),
    "calendar":      ("off",    {}),
    "camera":        ("idle",   {}),
    "person":        ("home",   {}),
    "scene":         ("unknown", {}),
    "weather":       ("sunny",  {"temperature": 30.4}),
    "alarm_control_panel": ("disarmed", {}),
    "input_boolean": ("off",    {}),
    "number":        ("21",     {}),
    "select":        ("auto",   {}),
}


def entita_dalla_configurazione() -> list[str]:
    """Ogni stringa del documento che somiglia a un identificatore."""
    doc = json.loads(ESEMPIO.read_text(encoding="utf-8"))
    trovate: list[str] = []
    ident = re.compile(r"^[a-z][a-z0-9_]*\.[a-z0-9_]+$")

    def scendi(n):
        if isinstance(n, dict):
            for v in n.values():
                scendi(v)
        elif isinstance(n, list):
            for v in n:
                scendi(v)
        elif isinstance(n, str) and ident.match(n):
            if n not in trovate:
                trovate.append(n)

    scendi(doc)
    return trovate


# I sensori aggregati della home: uno per scheda, con tutto negli
# attributi. E la stessa scelta gia fatta per l'energia — cinque numeri che
# cambiano insieme non meritano cinque sottoscrizioni — e il template che li
# compone lo scrive chi conosce la casa, non il pannello.
AGGREGATI = {
    # The names of the package since schema 11. `since` is a time, and the
    # panel puts the language's words around it; free text passes as it is.
    "presence": ("2", {"people": [
        {"name": "Person 1", "home": True, "since": "17:40"},
        {"name": "Person 2", "home": True, "since": "16:05"},
        {"name": "Person 3", "home": False, "since": "back around 19:15"},
    ]}),
    # Openings and energy keep the Italian names of the package before
    # schema 11, on purpose: the panel reads the English name first and the
    # Italian one after, and with one sensor of each kind this server walks
    # both roads in every run of tools/prova_ha.py.
    "openings": ("2", {"aperture": [
        {"nome": "Cucina", "da": "da 35 minuti"},
        {"nome": "Bagno", "da": "da 8 minuti"},
    ]}),
    "energy": ("3.42", {"casa": 1.20, "rete_w": -1.32, "batteria_w": 0.90,
                         "batteria_pct": 78, "oggi_kwh": 24.8}),
    "lights_on": ("4", {}),
}


# Un timer che sta correndo, per provare il conto alla rovescia. Ne basta
# uno: gli altri restano fermi, che e come sta una casa quasi sempre.
#
# `finishes_at` si calcola **al momento della richiesta** e non all'avvio del
# server: e un istante assoluto, e uno scritto una volta per tutte
# scadrebbe mentre le prove girano. Quarantadue minuti tondi, cosi il conto
# alla rovescia arrotondato per eccesso da esattamente 42.
#
# La durata dichiarata e **due ore**, non l'ora e mezza di
# `durata_predefinita` in configurazione: le due devono essere diverse, se no
# una barra che divide per il numero sbagliato darebbe lo stesso risultato di
# una che divide per quello giusto, e la prova non proverebbe niente.
TIMER_CHE_CORRE = "timer.condizionatore_mansarda"
TIMER_MINUTI = 42
TIMER_DURATA = "2:00:00"


def stato_di(entita: str) -> dict:
    dominio, oggetto = entita.split(".", 1)

    if entita == TIMER_CHE_CORRE:
        fine = time.gmtime(time.time() + TIMER_MINUTI * 60)
        return {
            "entity_id": entita,
            "state": "active",
            "attributes": {
                "duration": TIMER_DURATA,
                # Fermo al valore d'inizio: e cosi che si comporta Home
                # Assistant, ed e la ragione per cui il pannello non lo usa.
                "remaining": TIMER_DURATA,
                "finishes_at": time.strftime("%Y-%m-%dT%H:%M:%S+00:00", fine),
                "editable": True, "restore": False,
                "friendly_name": oggetto.replace("_", " ").title(),
            },
            "last_changed": "2026-08-23T18:42:00+00:00",
        }

    # I sensori aggregati si riconoscono dal nome — sensor.panel_openings e
    # gli altri: e come li chiama il pacchetto in 06-ha-package.yaml, e
    # quindi la configurazione d'esempio.
    for chiave, (stato, attributi) in AGGREGATI.items():
        if oggetto.endswith(chiave):
            a = dict(attributi)
            a["friendly_name"] = oggetto.replace("_", " ").title()
            return {"entity_id": entita, "state": stato, "attributes": a,
                    "last_changed": "2026-08-23T18:42:00+00:00"}

    stato, attributi = PER_DOMINIO.get(dominio, ("on", {}))
    attributi = dict(attributi)
    attributi["friendly_name"] = entita.split(".", 1)[1].replace("_", " ").title()
    return {"entity_id": entita, "state": stato, "attributes": attributi,
            "last_changed": "2026-08-23T18:42:00+00:00"}


# --- WebSocket, lato server ------------------------------------------------

def stretta(conn: socket.socket) -> bool:
    dati = b""
    while b"\r\n\r\n" not in dati:
        pezzo = conn.recv(4096)
        if not pezzo:
            return False
        dati += pezzo

    testa = dati.decode("latin-1")
    m = re.search(r"Sec-WebSocket-Key:\s*(\S+)", testa, re.I)
    if not m:
        conn.sendall(b"HTTP/1.1 400 Bad Request\r\n\r\n")
        return False

    accept = base64.b64encode(
        hashlib.sha1((m.group(1) + GUID).encode()).digest()).decode()
    conn.sendall(
        f"HTTP/1.1 101 Switching Protocols\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept}\r\n\r\n".encode())
    return True


def manda(conn: socket.socket, oggetto) -> None:
    carico = json.dumps(oggetto, ensure_ascii=False).encode("utf-8")
    n = len(carico)
    if n < 126:
        testa = struct.pack("!BB", 0x81, n)
    elif n <= 0xFFFF:
        testa = struct.pack("!BBH", 0x81, 126, n)
    else:
        testa = struct.pack("!BBQ", 0x81, 127, n)
    conn.sendall(testa + carico)


class Frammenti:
    """Riassembla i frame in arrivo. Il client maschera sempre."""

    def __init__(self):
        self.grezzi = b""
        self.parziale = b""

    def aggiungi(self, dati: bytes) -> list[str]:
        self.grezzi += dati
        fuori = []
        while True:
            if len(self.grezzi) < 2:
                break
            b0, b1 = self.grezzi[0], self.grezzi[1]
            fine, codice = bool(b0 & 0x80), b0 & 0x0F
            mascherato, lung = bool(b1 & 0x80), b1 & 0x7F
            t = 2
            if lung == 126:
                if len(self.grezzi) < 4:
                    break
                lung = struct.unpack("!H", self.grezzi[2:4])[0]
                t = 4
            elif lung == 127:
                if len(self.grezzi) < 10:
                    break
                lung = struct.unpack("!Q", self.grezzi[2:10])[0]
                t = 10
            chiave = b""
            if mascherato:
                if len(self.grezzi) < t + 4:
                    break
                chiave = self.grezzi[t:t + 4]
                t += 4
            if len(self.grezzi) < t + lung:
                break

            carico = bytearray(self.grezzi[t:t + lung])
            if chiave:
                for i in range(len(carico)):
                    carico[i] ^= chiave[i % 4]
            self.grezzi = self.grezzi[t + lung:]

            if codice == 0x8:
                raise ConnectionError("il client ha chiuso")
            if codice in (0x0, 0x1):
                self.parziale += bytes(carico)
                if fine:
                    fuori.append(self.parziale.decode("utf-8", "replace"))
                    self.parziale = b""
        return fuori


# --- il protocollo di Home Assistant --------------------------------------

class Sessione:
    def __init__(self, conn, arg, entita):
        self.conn = conn
        self.arg = arg
        self.entita = entita
        self.autenticato = False
        self.sottoscritto = None
        self.messaggi = 0
        self.comandi = []
        self.caduta_chiesta = False
        self.zitto = False
        self.ultimo_id = 0

    def servi(self):
        manda(self.conn, {"type": "auth_required", "ha_version": "2026.8.0"})
        f = Frammenti()
        self.conn.settimeout(0.2)
        inizio = time.time()

        while True:
            if self.arg.chiudi_dopo and time.time() - inizio > self.arg.chiudi_dopo:
                print("chiudo il collegamento come chiesto", file=sys.stderr)
                return
            try:
                dati = self.conn.recv(65536)
            except socket.timeout:
                continue
            if not dati:
                return
            for testo in f.aggiungi(dati):
                self.messaggi += 1
                # Ammutolito: il socket resta aperto, i byte si leggono, e
                # non si risponde piu a niente. E il guasto che un Home
                # Assistant vero fa quando si pianta senza morire, ed e
                # l'unico che nessun'altra opzione qui riproduce: --cadi-*
                # chiudono, e una chiusura il pannello la vede subito.
                #
                # Il segnale e il **primo battito** e non un numero di
                # messaggi: un ping arriva solo da un pannello che si e
                # gia collegato del tutto, quindi lo scenario parte sempre
                # da li. Contando i messaggi bisognerebbe sapere quanti ne
                # servono per collegarsi — oggi tre — e il giorno che
                # l'avvio ne aggiunge uno lo scenario ammutolirebbe a meta
                # collegamento, fallendo per il motivo sbagliato.
                if self.arg.muto_al_battito and self.zitto:
                    continue
                try:
                    m = json.loads(testo)
                except json.JSONDecodeError as e:
                    # Home Assistant vero, a un messaggio malformato, chiude
                    # e basta. Qui si chiude **dicendolo**: un finto server
                    # che muore in silenzio fa sembrare il difetto una
                    # caduta di rete, ed e successo davvero — un comando del
                    # clima mandava JSON invalido e la prova passava lo
                    # stesso, perche guardava solo se i byte erano partiti.
                    print(f"messaggio malformato ({e}): {testo[:120]}",
                          file=sys.stderr)
                    return
                self.uno(m)
                if self.arg.cadi_dopo and self.messaggi >= self.arg.cadi_dopo:
                    print(f"cado dopo {self.messaggi} messaggi", file=sys.stderr)
                    return
                if self.caduta_chiesta:
                    print("cado sul comando", file=sys.stderr)
                    return

    def uno(self, m: dict):
        tipo = m.get("type")

        # Home Assistant wants **increasing** ids on a connection: to one that
        # does not increase it answers "id_reuse" and does not run it. The
        # panel fell for it twice — the entity list and room requests, then
        # the heartbeat, which for months always sent 4 — and both times the
        # fault was silent, because this server answered everything.
        #
        # Here it answers like the real one **and then closes**, saying so:
        # an error the panel ignores is a test that passes for nothing, while
        # a drop is something the test sees.
        if tipo != "auth":
            ident = m.get("id")
            if not isinstance(ident, int) or ident <= self.ultimo_id:
                print(f"id not increasing: {ident} after {self.ultimo_id} "
                      f"({tipo})", file=sys.stderr)
                manda(self.conn, {"id": ident, "type": "result",
                                  "success": False,
                                  "error": {"code": "id_reuse",
                                            "message": "Identifier values "
                                                       "have to increase."}})
                self.caduta_chiesta = True
                return
            self.ultimo_id = ident

        if tipo == "auth":
            if self.arg.token_cattivo:
                manda(self.conn, {"type": "auth_invalid",
                                  "message": "Invalid access token or password"})
                raise ConnectionError("token rifiutato")
            self.autenticato = True
            manda(self.conn, {"type": "auth_ok", "ha_version": "2026.8.0"})

        elif tipo == "subscribe_events":
            self.sottoscritto = m.get("id")
            manda(self.conn, {"id": m["id"], "type": "result", "success": True,
                              "result": None})

        elif tipo == "get_states":
            manda(self.conn, {"id": m["id"], "type": "result", "success": True,
                              "result": [stato_di(e) for e in self.entita]})

        elif tipo == "subscribe_entities":
            # La fotografia filtrata: il pannello dice quali entità gli
            # servono e riceve solo quelle. È il comando che usa anche
            # l'interfaccia web di Home Assistant, e il pannello ci è passato
            # perché su un impianto vero `get_states` risponde con 712 kB —
            # tutta la casa, in un frame solo.
            #
            # La forma è più stretta: `s` per lo stato, `a` per gli attributi.
            # Non è pensata per essere letta da un umano, è pensata per
            # passare su una rete.
            if self.arg.senza_subscribe_entities:
                # Un Home Assistant più vecchio del comando. Il pannello deve
                # dirlo, non restare ad aspettare una fotografia che non
                # arriverà mai.
                manda(self.conn, {"id": m["id"], "type": "result",
                                  "success": False,
                                  "error": {"code": "unknown_command",
                                            "message": "Unknown command."}})
                return

            # Solo quelle che questo server ha davvero: il pannello chiede
            # il suo elenco, ma `--manca` serve proprio a provare cosa
            # succede quando una di quelle non esiste. Restituirla lo stesso
            # renderebbe lo scenario una prova di niente.
            chieste = [e for e in (m.get("entity_ids") or self.entita)
                       if e in self.entita]
            manda(self.conn, {"id": m["id"], "type": "result",
                              "success": True, "result": None})
            aggiunte = {}
            for e in chieste:
                s = stato_di(e)
                aggiunte[e] = {"s": s["state"], "a": s["attributes"],
                               "c": "01ABC", "lc": 1782000000.0,
                               "lu": 1782000000.0}
            manda(self.conn, {"id": m["id"], "type": "event",
                              "event": {"a": aggiunte}})

        elif tipo == "unsubscribe_events":
            manda(self.conn, {"id": m["id"], "type": "result",
                              "success": True, "result": None})

        elif tipo == "call_service":
            self.comandi.append(m)
            if self.arg.cadi_al_comando:
                # Si chiude **senza rispondere**, che e la caduta a meta di
                # un comando: il pannello deve accorgersene e contarlo come
                # fallito, non restare ad aspettare una risposta.
                #
                # Prima questo scenario si otteneva contando i messaggi
                # (`--cadi-dopo 4`), e il numero dipendeva da quanti ne
                # servivano per la stretta di mano. Aggiungendone uno alla
                # stretta, la prova ha cominciato a cadere nel posto
                # sbagliato: il conteggio diceva "al quarto messaggio" ma
                # voleva dire "al primo comando". Adesso lo dice.
                self.caduta_chiesta = True
                return
            print("comando: {} {} su {}".format(
                m.get("domain"), m.get("service"),
                (m.get("target") or {}).get("entity_id")), file=sys.stderr)
            if self.arg.comandi_falliscono:
                manda(self.conn, {"id": m["id"], "type": "result",
                                  "success": False,
                                  "error": {"code": "not_found",
                                            "message": "Entity not found"}})
                return
            manda(self.conn, {"id": m["id"], "type": "result", "success": True,
                              "result": {"context": {"id": "x"}}})
            # Un comando riuscito produce un cambio di stato, ed e da quello
            # che il pannello si accorge: il riscontro arriva quando la cosa
            # e successa, non quando il comando e partito.
            self.riscontro(m)

        elif tipo == "render_template":
            # L'elenco delle entità per i menu della pagina. Il pannello non
            # chiede più il registro — che contiene solo le entità con un
            # `unique_id`, cioè non quelle scritte in YAML — ma un modello
            # che mappa `states` sui soli identificatori.
            #
            # La risposta arriva in **due tempi**, come quella vera: prima
            # «sottoscritto», poi il risultato come evento. Il pannello che
            # aspettasse tutto nel `result` non vedrebbe mai niente, ed è un
            # difetto che solo un finto server fatto giusto sa far vedere.
            #
            # Se ne aggiungono un paio che *non* sono in configurazione:
            # servono a distinguere un elenco che arriva davvero da Home
            # Assistant da uno ricavato da quello che il pannello già
            # segue — che era la metà inutile della funzione, perché non
            # aiuta a sceglierne di nuove.
            extra = ["light.lampada_mai_configurata",
                     "sensor.umidita_cantina"]
            manda(self.conn, {"id": m.get("id"), "type": "result",
                              "success": True, "result": None})
            # Una riga sola di identificatori separati da virgole: è la
            # forma che il modello produce con `join(',')`, e il pannello
            # non deve indovinare se il risultato è testo o elenco.
            manda(self.conn, {
                "id": m.get("id"), "type": "event",
                "event": {"result": ",".join(self.entita + extra),
                          "listeners": {"all": True, "entities": [],
                                        "domains": [], "time": False}}})

        elif tipo == "config/entity_registry/list_for_display":
            # Il comando di prima, tenuto per dire che non si usa più: il
            # registro non elenca le entità scritte in YAML, e su un impianto
            # vero erano quasi tutte quelle che servivano.
            manda(self.conn, {"id": m.get("id"), "type": "result",
                              "success": False,
                              "error": {"code": "not_supported",
                                        "message": "il pannello chiede un modello"}})

        elif tipo == "ping":
            if self.arg.muto_al_battito:
                print("ammutolisco dal primo battito", file=sys.stderr)
                self.zitto = True
                return
            manda(self.conn, {"id": m.get("id"), "type": "pong"})

    def riscontro(self, m: dict):
        entita = (m.get("target") or {}).get("entity_id")
        if not entita or not self.sottoscritto:
            return
        s = stato_di(entita)
        servizio = m.get("service", "")
        if servizio.endswith("turn_on"):
            s["state"] = "on"
        elif servizio.endswith("turn_off"):
            s["state"] = "off"
        elif servizio == "set_temperature":
            s["attributes"]["temperature"] = \
                (m.get("service_data") or {}).get("temperature", 22)
        manda(self.conn, {
            "id": self.sottoscritto, "type": "event",
            "event": {"event_type": "state_changed",
                      "data": {"entity_id": entita, "new_state": s},
                      "origin": "LOCAL"}})


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--porta", type=int, default=8123)
    ap.add_argument("--token-cattivo", action="store_true",
                    help="risponde auth_invalid: il pannello deve fermarsi")
    ap.add_argument("--cadi-dopo", type=int, default=0, metavar="N",
                    help="chiude dopo N messaggi ricevuti")
    ap.add_argument("--muto-al-battito", action="store_true",
                    help="dal primo ping resta collegato e non risponde piu")
    ap.add_argument("--chiudi-dopo", type=float, default=0, metavar="S",
                    help="chiude dopo S secondi")
    ap.add_argument("--senza-subscribe-entities", action="store_true",
                    help="finge un Home Assistant piu vecchio del comando")
    ap.add_argument("--cadi-al-comando", action="store_true",
                    help="chiude senza rispondere al primo comando")
    ap.add_argument("--comandi-falliscono", action="store_true",
                    help="ogni call_service risponde success:false")
    ap.add_argument("--manca", action="append", default=[], metavar="ENTITA",
                    help="entita che questo Home Assistant non ha")
    ap.add_argument("--una-volta", action="store_true",
                    help="serve un collegamento solo e poi esce")
    arg = ap.parse_args()

    entita = [e for e in entita_dalla_configurazione() if e not in arg.manca]
    print(f"finto Home Assistant su :{arg.porta} — {len(entita)} entita",
          file=sys.stderr)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", arg.porta))
    s.listen(4)

    try:
        while True:
            conn, _ = s.accept()
            try:
                if stretta(conn):
                    Sessione(conn, arg, entita).servi()
            except (ConnectionError, OSError, ValueError) as e:
                print(f"collegamento finito: {e}", file=sys.stderr)
            finally:
                conn.close()
            if arg.una_volta:
                return 0
    except KeyboardInterrupt:
        return 0
    finally:
        s.close()


if __name__ == "__main__":
    raise SystemExit(main())
