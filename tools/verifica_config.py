#!/usr/bin/env python3
"""
Verifica una configurazione del pannello.

  python tools/verifica_config.py docs/04-config.example.json

Due livelli di controllo:
  1. lo schema formale (config.schema.json) — tipi, intervalli, obbligatorietà
  2. le regole fra campi, che JSON Schema non sa esprimere

Richiede jsonschema:  pip install jsonschema
Senza la libreria, esegue comunque i controlli del punto 2.
"""
import json, sys, re
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
DOCS = RADICE / "docs"
ROSSO, GIALLO, VERDE, FINE = "\033[31m", "\033[33m", "\033[32m", "\033[0m"

def carica(p):
    try:
        return json.loads(Path(p).read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"{ROSSO}JSON non valido:{FINE} {e}")
        sys.exit(2)

def schema(cfg):
    """Errori e avvisi dello schema, in due liste.

    Le chiavi che lo schema non conosce finiscono fra gli **avvisi** e non
    fra gli errori, perche' e' quello che fanno il firmware e la pagina web:
    entrambi iterano le proprieta' note dello schema e ignorano il resto, e
    il contratto (§2) dice che i campi sconosciuti si conservano. Un
    validatore di riferimento che rifiutasse un file che il pannello accetta
    direbbe la cosa sbagliata su quel pannello.

    L'avviso pero' resta: un campo scritto male non e' un motivo per
    rifiutare, ma e' precisamente la cosa che uno vuole sapere — dal muro si
    presenta come un'impostazione che non fa niente.
    """
    try:
        import jsonschema
    except ImportError:
        print(f"{GIALLO}jsonschema non installato: salto la verifica formale.{FINE}")
        print("  pip install jsonschema\n")
        return [], []
    s = carica(DOCS / "config.schema.json")
    v = jsonschema.Draft202012Validator(s)
    err, avv = [], []
    for e in sorted(v.iter_errors(cfg), key=lambda e: list(e.absolute_path)):
        dove = "/".join(str(x) for x in e.absolute_path) or "(radice)"
        riga = f"{dove}: {e.message}"
        (avv if e.validator == "additionalProperties" else err).append(riga)
    return err, avv

def regole(c):
    """Regole fra campi: JSON Schema non le esprime."""
    err, avv = [], []

    for a in c.get("access", []):
        if a.get("type") == "pulse" and not a.get("pulse_ms"):
            err.append(f"access/{a['id']}: tipo impulso senza pulse_ms")
        if a.get("type") == "switch" and a.get("pulse_ms"):
            avv.append(f"access/{a['id']}: pulse_ms ignorato su un interruttore")

    for k in ("heating_limits", "ac_limits"):
        lim = c.get("climate", {}).get(k, {})
        if lim and lim.get("min", 0) >= lim.get("max", 99):
            err.append(f"climate/{k}: min deve essere minore di max")

    for u in c.get("climate", {}).get("air_conditioners", []):
        ha_t, ha_a = bool(u.get("timer")), bool(u.get("timer_automation"))
        if ha_t != ha_a:
            avv.append(f"clima/{u['id']}: timer e automazione_timer vanno insieme, "
                       "altrimenti l'interruttore non compare")

    s = c.get("display", {})
    if s.get("brightness_standby", 0) > s.get("brightness_active", 100):
        err.append("schermo: luminosita_standby maggiore di luminosita_attiva")
    sp = s.get("off_after_s", 0)
    if sp and sp < s.get("standby_after_s", 0):
        err.append("schermo: spegnimento prima dello standby")

    sez = c.get("sections", [])
    ag = c.get("calendar", {})
    if "calendar" in sez and not ag.get("enabled"):
        err.append("sezioni: agenda elencata ma agenda.attiva è falso")
    if ag.get("enabled") and not ag.get("sensor"):
        err.append("agenda: attiva senza sensore")
    for n, i in enumerate(c.get("switches", []) or []):
        e = i.get("entity", "")
        if not any(e.startswith(d) for d in ("switch.", "light.", "input_boolean.")):
            err.append(f"interruttori/{n}/entita: serve switch., light. o input_boolean.")

    for n, g in enumerate(c.get("schedules", []) or []):
        dove = f"schedules/{n}"
        fin = g.get("windows", []) or []
        aut = g.get("automations", []) or []
        for k, f in enumerate(fin):
            if f.get("on_time") and f.get("on_time") == f.get("off_time"):
                err.append(f"{dove}/finestre/{k}/spegnimento: e lo stesso orario "
                           f"dell'accensione")
        if not fin and not aut:
            avv.append(f"{dove}: gruppo senza finestre ne automazioni")

    piani = c.get("climate", {}).get("floors", []) or []
    ids = [p.get("id") for p in piani]
    for n, i in enumerate(ids):
        if i in ids[:n]:
            err.append(f"clima/piani/{n}/id: gia usato da un altro piano")
    mio = c.get("climate", {}).get("panel_floor")
    if mio and mio not in ids:
        err.append("clima/piano_pannello: nessun piano ha questo id")
    for n, z in enumerate(c.get("climate", {}).get("heating", []) or []):
        zp = z.get("floor")
        if zp and zp not in ids:
            avv.append(f"clima/riscaldamento/{n}/piano: nessun piano ha questo id")

    doppi = [x for x in {a["entity"] for a in c.get("access", [])}
             if sum(1 for a in c["access"] if a["entity"] == x) > 1]
    for d in doppi:
        avv.append(f"accessi: entità {d} usata più volte")

    return err, avv

def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    cfg = carica(sys.argv[1])
    e1, avv1 = schema(cfg)
    e2, avv = regole(cfg)
    avv = avv1 + avv

    for e in e1: print(f"{ROSSO}schema{FINE}  {e}")
    for e in e2: print(f"{ROSSO}regola{FINE}  {e}")
    for a in avv: print(f"{GIALLO}avviso{FINE}  {a}")

    n = len(e1) + len(e2)
    print()
    if n:
        print(f"{ROSSO}{n} errori{FINE}, {len(avv)} avvisi")
        sys.exit(1)
    print(f"{VERDE}Configurazione valida{FINE}, {len(avv)} avvisi")

if __name__ == "__main__":
    main()
