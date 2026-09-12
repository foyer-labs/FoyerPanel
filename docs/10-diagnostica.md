# Foyer Panel — Diagnostica

Il pannello è incassato a muro: niente tastiera, niente porta seriale
raggiungibile. Se un giorno smette di aggiornare i valori o si
riavvia da solo, deve poterlo raccontare.

Tre pezzi: un registro a bordo, dei contatori, e un endpoint in sola
lettura che Home Assistant interroga.

---

## 1. Registro a bordo

**Buffer circolare in PSRAM**, 200 righe. Ogni riga: marcatore temporale,
livello, sorgente, messaggio (troncato a 96 caratteri).

| Livello | Quando |
|---|---|
| `ERRORE` | qualcosa non ha funzionato e l'utente se ne accorgerà |
| `AVVISO` | qualcosa è andato storto ma è stato recuperato |
| `INFO` | eventi di rilievo: connessione, riconnessione, comandi inviati, cambio configurazione |
| `DEBUG` | attivabile temporaneamente, spento di default |

**Le ultime 20 righe di livello `ERRORE` sono replicate in NVS**, così
sopravvivono a un riavvio. Senza, il messaggio che spiega un crash muore
proprio nel crash — che è il momento in cui serve.

Scrittura in NVS **al massimo una ogni 30 secondi** e solo per gli errori:
la flash ha un numero finito di cicli e un errore ripetuto in ciclo
stretto la consumerebbe.

### Regola non negoziabile

> Il registro **non contiene mai** token, password o SSID. Se un
> messaggio dovesse
> includerli, va troncato prima di scriverlo.

L'endpoint è in sola lettura ma raggiungibile da chiunque sia sulla rete
locale: un token finito in un registro è un token compromesso.

### Cosa va registrato, come minimo

- avvio: esito di ogni passo, con i tempi
- Wi-Fi: connessione, disconnessione, riconnessione, RSSI alla connessione
- Home Assistant: autenticazione, perdita del collegamento, riconnessione,
  numero di entità sottoscritte
- comandi: entità, servizio chiamato, esito, millisecondi di risposta
- configurazione: salvataggio, validazione fallita con i campi rifiutati
- OTA: inizio, esito, eventuale rollback
- memoria: quando l'heap libero scende sotto una soglia

---

## 2. Contatori

Raccolti in continuo, visibili a schermo e nell'endpoint:

| Contatore | Note |
|---|---|
| Tempo di accensione | dall'ultimo riavvio |
| Numero di riavvii | dal primo avvio, in NVS |
| **Motivo dell'ultimo riavvio** | da `esp_reset_reason()` |
| Heap libero, e minimo storico | il minimo storico è il numero che conta |
| PSRAM libera | idem |
| fps medio dell'interfaccia | media mobile su 10 s |
| Stato Home Assistant | connesso / in riconnessione / assente |
| Secondi dall'ultimo dato valido | |
| Comandi inviati / falliti | |
| RSSI Wi-Fi | |

Il **motivo del riavvio** è il dato più prezioso: distingue un watchdog da
un calo di tensione da un riavvio richiesto, e sono tre problemi diversi
con tre soluzioni diverse.

---

## 3. Schermata a bordo — Impostazioni → Sistema

Due parti:

**In alto, i contatori** in una griglia leggibile a colpo d'occhio, con i
valori fuori soglia in ambra: heap sotto 40 KB, fps sotto 10, ultimo dato
oltre 60 secondi.

**In basso, il registro**: elenco scorrevole delle righe più recenti in
alto, con quattro filtri (Tutti, Errori, Avvisi, Info) e il livello
distinto dal colore. Righe in `f_s`, monospaziate per i numeri.

In fondo due comandi: **Copia negli appunti** — che sul pannello non ha
senso, quindi diventa **"Mostra come QR"**, un codice che contiene l'URL
`/api/log` da inquadrare col telefono — e **Svuota registro**.

---

## 4. Endpoint in sola lettura

Due percorsi, **sempre attivi**, anche quando il server di configurazione
è spento:

| Metodo | Percorso | Restituisce |
|---|---|---|
| `GET` | `/api/status` | JSON con i contatori |
| `GET` | `/api/log?level=error&n=50` | le righe del registro |

**Sempre attivi è una deroga voluta** alla regola del server spento, e
regge solo perché questi due percorsi:

- non accettano scritture di alcun tipo
- non restituiscono segreti né configurazione
- sono limitati a 1 richiesta al secondo per indirizzo chiamante
- rispondono `404` a qualunque altro percorso finché la configurazione non
  è sbloccata dal pannello

Tutto il resto — `/api/config`, `/api/secrets`, `/api/entities`, OTA —
resta spento salvo sblocco fisico.

### Forma di `/api/status`

```json
{
  "panel": "Ingresso",
  "profile": "p4-800x1280",
  "firmware": "1.0.4",
  "uptime_s": 528340,
  "restarts": 3,
  "last_restart_reason": "power_on",
  "heap_free": 84120,
  "heap_min": 51208,
  "psram_free": 3980160,
  "fps": 24,
  "wifi_rssi": -52,
  "ha_state": "connected",
  "ha_last_data_s": 2,
  "commands_sent": 41,
  "commands_failed": 0,
  "recent_errors": 0,
  "network_trial": "",
  "network_trial_in_s": 0
}
```

`network_trial` dice come va la prova di una rete Wi-Fi appena cambiata
dalla pagina: `waiting` (parte fra `network_trial_in_s` secondi), `trying`,
oppure `reverted` quando quella nuova non ha risposto entro un minuto e il
pannello è tornato alla rete di prima. Vuoto quando non c'è niente da dire.
Sono valori per le macchine: la pagina li mette in parole nella propria
lingua (`network_trial.*` in `i18n/`). Mai il nome della rete: questo
percorso risponde a chiunque sia sulla rete di casa.

---

## 5. Sensore in Home Assistant

Un sensore REST per pannello, che rende lo stato visibile dal telefono e
permette di farsi avvisare. Il blocco è in `06-ha-package.yaml`, da
scommentare mettendo al posto di `INDIRIZZO_PANNELLO_1` l'indirizzo del
pannello.

Due automazioni che consiglio, non incluse ma banali da scrivere:

- **pannello non raggiungibile per più di 10 minuti** → notifica
- **heap minimo sceso sotto 40 KB** → notifica, perché è il primo sintomo
  di una perdita di memoria e si vede giorni prima del riavvio

---

## 6. Costo

| Voce | Costo |
|---|---|
| Buffer del registro, 200 righe | ~20 KB in PSRAM |
| Copia degli errori in NVS | ~2 KB |
| Contatori | trascurabile |
| Schermata a bordo | una schermata in più |
| Endpoint | task HTTP già presente |

I 20 KB in PSRAM non sono un problema; l'attenzione va semmai al fatto
che ogni scrittura di riga non deve allocare.

---

## 7. Configurazione

```json
"diagnostics": {
  "status_endpoint": true
}
```

> Fino al 30/08/2026 questa sezione dichiarava anche `livello_registro`,
> `righe_registro` e `soglia_heap_avviso`. Erano validate e mai lette: il
> livello del registro, la sua lunghezza e la soglia dell'heap sono
> costanti nel firmware. Sono state tolte invece di essere collegate,
> perché nessuno le aveva mai chieste — vedi `03-config-contratto.md` §11.

`status_endpoint: false` spegne anche i due percorsi in sola lettura, per
chi preferisce nessuna superficie di rete esposta. Il registro a bordo
resta.
