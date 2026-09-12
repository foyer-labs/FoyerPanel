# Foyer Panel — Contratto di configurazione

> **GERARCHIA DELLE FONTI**
>
> `config.schema.json` è la **fonte autorevole** per struttura, tipi,
> intervalli e obbligatorietà: è il *cosa*.
>
> Questo documento spiega il *perché* delle regole e descrive quelle che
> JSON Schema non sa esprimere — i legami fra campi. Dove i due
> divergessero sui numeri, **vale lo schema**: qui il numero va corretto,
> non discusso.
>
> `tools/verifica_config.py` applica entrambi i livelli.

Vale per entrambi i pannelli. I limiti che dipendono dal profilo sono
indicati con due colonne; le misure di layout non compaiono qui, stanno in
`09-profili.md`.

Questo documento definisce come firmware e pagina web si accordano sulla
configurazione. È il pezzo che conviene fissare adesso: cambiarlo dopo
significa toccare entrambi i lati.

---

## 1. Dove stanno le cose

| Contenuto | Dove | Perché lì |
|---|---|---|
| Configurazione (`config.json`) | LittleFS, partizione dedicata | leggibile, sostituibile, salvabile come file |
| Copia di sicurezza (`config.bak.json`) | LittleFS | ripristino automatico se il file principale è corrotto |
| Segreti | NVS, spazio dei nomi `secrets` | non finiscono mai in un file leggibile né tornano indietro dalla pagina web |

**Segreti** (mai nel JSON): password Wi-Fi, token di Home Assistant e le
password delle reti condivisibili. La pagina web può scriverli, non
rileggerli: in lettura restituisce `"impostato": true/false` e basta.

I nomi sono un **elenco chiuso** (`main/cfg/segreti.h`), non composti a
runtime: un nome che arrivasse dalla rete, o costruito da un indice di
configurazione, diventerebbe una chiave NVS arbitraria.

| Chiave NVS | Che cos'è |
|---|---|
| `wifi_pw` | password della rete a cui si collega il pannello |
| `ha_token` | token a lunga durata di Home Assistant |
| `wifi_share1_pw` | password della rete ospiti, quella nel QR |
| `wifi_share2_pw` … `wifi_share5_pw` | password delle altre reti condivise, nell'ordine di `wifi_sharing.networks` |

Le chiavi sono corte perché NVS si ferma a quindici caratteri: un nome più
lungo verrebbe troncato in silenzio, e due segreti finirebbero nella stessa
chiave.

---

## 2. Regole di salvataggio

1. La pagina web invia il JSON **completo**, non differenze.
2. Il firmware valida prima di scrivere. Se la validazione fallisce,
   risponde con l'elenco dei campi rifiutati e **non tocca il file**.
3. Scrittura atomica: `config.tmp` → `fsync` → rinomina su `config.json`.
   Il precedente diventa `config.bak.json`.
4. All'avvio: se `config.json` manca o non si analizza, si usa
   `config.bak.json`; se manca anche quello, si parte con i valori
   predefiniti e si va alla procedura di primo avvio.
5. Dimensione massima 24 KB. Oltre, la scrittura viene rifiutata.

**Cosa richiede riavvio e cosa no:**

| Modifica | Effetto |
|---|---|
| Rete Wi-Fi | riavvio |
| Indirizzo o token di Home Assistant | riconnessione, niente riavvio |
| Entità, nomi, zone, calendari | ricostruzione della schermata, immediata |
| Luminosità e tempi dello schermo | immediata |
| Ordine delle sezioni | immediata |

---

## 3. Versionamento

Il campo `schema` è un intero. Il firmware conosce la propria versione:

- `config.schema == firmware.schema` → si procede
- `config.schema < firmware.schema` → migrazione automatica, poi
  riscrittura del file con la versione nuova
- `config.schema > firmware.schema` → **si rifiuta** e si avvia in sola
  lettura con un avviso a schermo: significa che il pannello è stato
  riportato a un firmware più vecchio

Le migrazioni sono funzioni separate `migra_1_2()`, `migra_2_3()`,
`migra_3_4()` e si applicano in sequenza. Una migrazione **toglie** un campo
solo quando lo schema ha smesso di ammetterlo: la regola di §8 dice di
conservare i campi sconosciuti, ma un campo che lo schema rifiuta farebbe
fallire il primo salvataggio su qualcosa che l'utente non ha mai scritto e
che la pagina non gli mostra. Ogni campo nuovo deve avere un valore predefinito
sensato, così una migrazione non chiede mai nulla all'utente.

La migrazione si applica **anche a un file importato dalla pagina web**, prima
di validarlo: un file esportato da un pannello più vecchio viene portato
avanti come quello letto dalla flash, invece di essere rifiutato per i nomi
che aveva il diritto di usare.

### 10 → 11: la configurazione parla inglese

Fino allo schema 10 ogni chiave e ogni valore enumerato erano italiani —
`sistema`, `nome_pannello`, `tutta_la_casa` — perché il pannello era nato per
una casa sola, in Italia. Dallo schema 11 sono inglesi: `system`,
`panel_name`, `whole_house`. La tabella completa è in `main/cfg/config.c`
(`CHIAVI_11` e `VALORI_11`).

Le chiavi si traducono **senza contesto**: lo stesso nome italiano diventa
lo stesso nome inglese dovunque compaia, ed è stato verificato sull'intero
schema che nessun nome italiano avesse due significati. I valori invece si
traducono per percorso, perché una parola come «acceso» è un valore in un
posto e testo libero in un altro.

È anche la porta dal progetto italiano da cui questo discende: un
`config.json` allo schema 10 si carica qui e ne esce in inglese. La prova
`configurazione` lo verifica nel modo più severo possibile: l'esempio com'era
allo schema 10 (`test/config-schema10-it.json`), migrato, deve essere
**identico** a `04-config.example.json`, chiave per chiave.

Insieme cambiano i nomi dei segreti delle reti condivise — `wifi_osp_pw` e
`wifi_pv1_pw`…`wifi_pv4_pw` diventano `wifi_share1_pw`…`wifi_share5_pw` —
e il pannello sposta le password dai nomi vecchi ai nuovi all'avvio, senza
perderne nessuna; e cambiano i campi dell'API (§6) e di `/api/status`
(`10-diagnostica.md` §4).

---

## 4. Vincoli di validazione

### 4.1 Limiti che dipendono dal profilo di pannello

I limiti non sono numeri arbitrari: derivano dalle griglie disegnate, e le
griglie sono diverse sui quattro schermi. Il firmware espone il proprio
profilo in `GET /api/status` e la pagina web valida di conseguenza.

**I numeri non sono ripetuti qui.** Zone di luci e clima per pagina,
colonne dell'agenda e capienza del dock stanno in `09-profili.md` §5 e §6,
unica fonte, e
in `profili.json` per chi li legge da programma. La pagina web li ricava
dal profilo che `GET /api/status` dichiara, non da una tabella scritta a
mano.

I limiti che **non** dipendono dal profilo, e che quindi stanno qui:

| Voce | Limite |
|---|---|
| Accessi | max 6 |
| Scene luci | max 5 |
| Calendari | max 4 |
| Condizionatori | max 8 |
| Zone di riscaldamento e di luci | nessun limite, si impagina |

Il numero **totale** di zone non è limitato dal profilo: oltre la capienza
di una pagina si impagina. Cambiando profilo, quindi, una configurazione
resta valida — cambia solo come viene distribuita. È il motivo per cui
`config.json` **non contiene numeri di pagina**: contiene un elenco
ordinato, e l'impaginazione è una decisione del firmware.

### 4.2 Limiti comuni

Gli intervalli qui sotto sono un riepilogo leggibile di ciò che lo schema
già impone. Se ti serve il valore esatto, guarda lo schema.

| Campo | Regola |
|---|---|
| `display.brightness_*` | 0–100, `standby` ≤ `active` |
| `display.standby_after_s` | 15–3600 |
| `display.off_after_s` | 0 = mai, altrimenti ≥ `standby_after_s` |
| `display.long_press_ms` | 500–3000 |
| `home_assistant.host` | IP o nome host, senza schema né percorso |
| `access[].pulse_ms` | 200–5000 |
| `climate.heating_limits` | `step` 0.1/0.5/1.0, `min` ≥ 5, `max` ≤ 35 |
| `climate.ac_limits` | idem; le unità Gree accettano 16–30 con passo 1.0 |
| `wifi_sharing.networks` | al massimo **5** reti; ognuna usa la password `wifi_shareN_pw` della sua posizione |
| `sections` | sottoinsieme ordinato di `lights, switches, climate, energy, access, calendar, wifi, robot, schedules`. Una sezione assente **non compare nel rail né nel dock**: è così che oggi si esclude l'agenda |
| Qualsiasi `entity` | forma `dominio.oggetto`, dominio tra quelli attesi |

### 4.4 Sezioni senza sorgente dati

Una sezione la cui sorgente non esiste **non compare nel rail** e la
relativa scheda non compare in home. Vale oggi per l'Agenda
(`calendar.enabled: false`, nessuna entità `calendar` in Home Assistant) e
varrebbe per Energia se mancasse `energy.aggregate_sensor`.
Non è un caso speciale: è la regola generale.

### 4.5 Tipi di accesso

Ogni voce di `access` ha un campo `type`:

| `type` | Comportamento | Riscontro |
|---|---|---|
| `pulse` | chiama `switch.turn_on` e rilascia dopo `pulse_ms` | nessuno, salvo `state_sensor` |
| `switch` | commuta l'entità e ne segue lo stato | stato reale, mostrato con un interruttore |

Il campo facoltativo `state_sensor` collega un `binary_sensor` a un
accesso a impulso: dove c'è, la riga mostra lo stato vero invece di
tacere. Nell'esempio è valorizzato solo per la porta del garage.

---

### 4.6 Clima — due gruppi, due strutture

`climate.heating` è un elenco di zone con il solo campo `climate`.
`climate.air_conditioners` è un elenco di unità con campi aggiuntivi:

| Campo | Obbligatorio | Note |
|---|---|---|
| `climate` | sì | entità `climate` |
| `timer` | no | entità `timer` |
| `timer_automation` | no | entità `automation` |
| `default_duration` | no | formato `h:mm` |
| `extras.*` | no | quattro interruttori Gree, uno per chiave |
| `intermittent.*` | no | presente su una sola unità |

L'interruttore "Spegni al timer" compare **solo se sono valorizzati
entrambi** `timer` e `timer_automation`. Con uno solo dei due, la riga
non viene mostrata: mezza funzione è peggio di nessuna.

Limiti: massimo 8 condizionatori; le zone di riscaldamento non hanno
limite di numero, solo di impaginazione.

### 4.7 Robot

| Campo | Cosa fa |
|---|---|
| `robot.name` | Come lo chiamate in casa. Compare nella riga in home e nella testata della sua sezione. Vuoto: si legge "Robot" |
| `robot.entity` | L'entità `vacuum.` |
| `robot.battery` | Sensore della batteria. Senza, si ripiega sull'attributo `battery_level` dell'entità |
| `robot.rooms[]` | Nome e **numero di segmento**. I numeri arrivano da `roborock.get_maps`, e la pagina web li offre in un menu a tendina |
| `robot.rooms_format` | `segments` (predefinito) o `list`. Vedi sotto |

**Perché `rooms_format` è configurabile.** `vacuum.send_command` con
`app_segment_clean` vuole la lista piatta — `params: [20]`. La forma a
oggetto sembra quella giusta, perché è quella dell'integrazione nativa, e
il robot **la accetta senza lamentarsi**: esce dalla base, gira qualche
secondo e rientra dicendo "pulizia terminata". Un comando che fallisce
dicendo di essere riuscito è la ragione per cui questo campo esiste invece
di essere una costante: su un altro modello la forma buona potrebbe essere
l'altra, e nessuno vuole scoprirlo ricompilando.

### 4.7-bis Robot — avvisi e manutenzione

```json
"robot": {
  "alerts": [
    { "entity": "sensor.robot_errore_base" },
    { "entity": "binary_sensor.robot_panno",
      "text": "Il panno non è agganciato", "when": "off" }
  ],
  "maintenance": [
    { "entity": "sensor.robot_spazzola_principale_residuo",
      "name": "Spazzola principale", "threshold": 10 }
  ]
}
```

| Campo | Obbligatorio | Regola |
|---|---|---|
| `alerts[].entity` | sì | `binary_sensor.` o `sensor.` |
| `alerts[].text` | no | serve per i sensori a due stati; per un sensore d'errore **vince il messaggio del robot** |
| `alerts[].when` | no | `acceso` (predefinito) o `spento`: quale dei due stati è l'allarme |
| `maintenance[].entity` | sì | `sensor.` del tempo residuo |
| `maintenance[].name` | no | senza, vale il `friendly_name` di Home Assistant |
| `maintenance[].threshold` | no | sotto questo valore la voce diventa un avviso. **In ore** |

Al massimo **dodici** per elenco.

**`when` è l'unico campo che non si può indovinare**, ed è anche l'unico
che se sbagliato dice il contrario del vero. Il pannello non prova a
dedurlo dal nome dell'entità: un nome è una parola di chi ha scritto
l'integrazione, e dedurne una polarità vorrebbe dire indovinare bene per
mesi e male il giorno in cui qualcuno rinomina un sensore.

**Tutto in ore, e anche la soglia.** I consumabili di un robot non hanno
tutti la stessa unità: alcuni danno le ore, altri i secondi. Mostrarli come
arrivano metteva «214 h» accanto a «770400 s» nella stessa schermata — lo
stesso dato in due lingue, e a un metro e mezzo da un vetro non si converte
a mente.

Il pannello porta in ore secondi, minuti e giorni, e confronta la soglia
sullo stesso metro: un `10` scritto qui vuol dire dieci ore su qualunque
sensore, che è quello che chi configura intende e non ha modo di sapere
quale sensore usi cosa.

Quello che **non** è un tempo — una percentuale, un conteggio — resta com'è,
con la sua unità, e la soglia si confronta con quello. Inventare le ore da
una percentuale darebbe un numero plausibile e falso, cioè quello che
nessuno va a controllare.

### 4.8-bis I piani del riscaldamento

```json
"climate": {
  "floors": [
    { "id": "terra", "name": "Piano terra",
      "switch": "switch.riscaldamento_piano_terra" },
    { "id": "primo", "name": "Primo piano",
      "switch": "switch.riscaldamento_primo_piano" }
  ],
  "panel_floor": "terra",
  "heating": [
    { "name": "Soggiorno", "climate": "climate.soggiorno", "floor": "terra" }
  ]
}
```

| Campo | Obbligatorio | Regola |
|---|---|---|
| `floors[].id` | sì | slug. **Ripetuto è un errore**: due piani con lo stesso id mandano le zone dell'uno sotto l'altro, con l'interruttore sbagliato accanto |
| `floors[].name` | sì | fino a 20 caratteri |
| `floors[].switch` | sì | `switch.` o `input_boolean.` — il servizio si ricava dal dominio |
| `panel_floor` | no | l'id del piano che **questo** pannello governa. Un id inesistente è un errore; senza, le due icone del riscaldamento non compaiono |
| `heating[].floor` | no | l'id del suo piano. Un id inesistente è un **avviso**, non un errore: la zona finisce nel gruppo «senza piano», che si vede |

Al massimo quattro piani: una casa con cinque impianti di riscaldamento
separati non è la casa per cui questo pannello è stato scritto.

**Senza `floors` non cambia niente**: la sezione Riscaldamento resta un elenco
solo, senza fasce e senza interruttori. È una cosa che si aggiunge, non una a
cui bisogna adeguarsi.

### 4.8-ter Programmazioni

```json
"schedules": [
  {
    "name": "Luci giardino",
    "icon": "lightbulb",
    "entity": "light.luci_giardino",
    "power": "sensor.luci_giardino_potenza",
    "windows": [
      { "on_time":  "input_datetime.giardino_on_1",
        "off_time": "input_datetime.giardino_off_1" }
    ],
    "automations": [
      { "entity": "automation.giardino_on_programmato",
        "name": "Accensione programmata" }
    ]
  }
]
```

| Campo | Obbligatorio | Regola |
|---|---|---|
| `name` | sì | fino a 24 caratteri |
| `icon` | no | lo **stesso elenco chiuso** degli interruttori: è la stessa domanda, e i caratteri-icona sono compilati nel firmware |
| `entity` | no | quello che gli orari accendono. Senza, il gruppo ha solo finestre di validità e automazioni |
| `power` | no | `sensor.` — l'assorbimento compare accanto allo stato |
| `windows[].on_time` / `.off_time` | sì | `input_datetime.` entrambi, e **diversi fra loro**: lo stesso orario ai due capi fa una finestra che dura zero, e succede copiando una riga |
| `windows[].validity` | no | vero = non accende, dà il permesso. Vedi `01-specifica-ui.md` §3.10 |
| `automations[].entity` | sì | `automation.` |
| `automations[].name` | no | senza, vale il `friendly_name` di Home Assistant: un nome cambiato là non va rincorso qui |

Al massimo **12 gruppi**, **4 finestre** e **12 automazioni** per gruppo.

**Gli orari non stanno in `config.json`.** Stanno negli `input_datetime` di
Home Assistant: il pannello li legge e ce li riscrive con
`input_datetime.set_datetime`, il solo campo `time` — quegli helper sono
tutti `has_date: false`. Tenerne una copia qui vorrebbe dire due idee di
quando si accende la pompa, e quella sbagliata sarebbe sempre quella che si
guarda.

Un gruppo senza finestre né automazioni è un **avviso**, non un errore:
compare sul pannello come una riga vuota. Non è un guasto, ma non è nemmeno
quello che voleva chi l'ha creato.

### 4.8-quater *(era: telecamere)*

Un elenco di indirizzi RTSP, con `name` e `url`, e il divieto della
chiocciola nell'indirizzo — le credenziali stavano in NVS, una coppia per
telecamera. Vissuto il 09/09/2026, schema 6, tolto dallo schema 7. Il
perché è in `05-architettura-firmware.md` §5.

Il numero resta vuoto perché possa continuare a non trovare niente.

> Una cosa di quel blocco vale oltre le telecamere, e va lasciata scritta:
> era **l'unica regola di questo contratto che rifiutava una forma che
> funziona**. `rtsp://utente:parola@casa/flusso` è come ogni guida in rete
> scrive un indirizzo, la telecamera l'accetta, e mette una password dentro
> un file fatto per essere esportato e copiato. Rifiutarla costava trenta
> secondi una volta sola; accettarla costava una password che nessuno sa
> più dov'è. Se un giorno tornasse un campo con dentro un indirizzo di
> rete, questa è la domanda da rifarsi.

### 4.11 Aspetto — i colori dell'interfaccia

```json
"appearance": {
  "accent": "#3580F0"
}
```

Nove colori facoltativi: `background`, `cards`, `borders`, `text`,
`text_dim`, `accent`, `positive`, `negative`, `lights_on`. Ognuno è
`#RRGGBB`.

`lights_on` è separato dall'accento di proposito: una lampadina non è uno
stato dell'interfaccia. L'icona del meteo, all'opposto, non si configura
affatto — deve dire che tempo fa, e un sole del colore scelto è un sole
solo per caso.

**Quello che manca vale il predefinito**, e i predefiniti sono la palette
con cui il pannello è disegnato — quella di `02-design-tokens.md`. Togliere
il blocco riporta tutto com'era: è il modo più semplice di tornare
indietro, e per questo la pagina di configurazione mostra i predefiniti nei
campi ma **non li scrive** finché non si tocca qualcosa.

Gli altri ventisei colori non si configurano: si ricavano. Gli stati
composti — selezione, pulsante in corso, riuscito, fallito, avvisi —
seguono la famiglia da cui dipendono ruotando la tinta. Chiederli uno per
uno vorrebbe dire ventun scelte da tenere coerenti, e basta sbagliarne una
perché una scritta diventi illeggibile.

**Il contrasto non è validato.** La pagina lo calcola, lo mostra accanto a
ogni coppia che conta e avvisa sotto la soglia; il pannello ubbidisce
comunque. Un pannello è di chi ce l'ha in casa, e rifiutare una
combinazione perché a un algoritmo non piace vorrebbe dire decidere al
posto suo. La via di ritorno c'è sempre: la pagina di configurazione non si
colora.

### 4.9 Interruttori

```json
"switches": [
  { "name": "Presa TV", "entity": "switch.presa_tv",
    "icon": "tv", "power": "sensor.presa_tv_potenza" },
  { "name": "Congelatore", "entity": "switch.congelatore", "icon": "ac_unit" }
]
```

| Campo | Obbligatorio | Regola |
|---|---|---|
| `name` | sì | fino a 24 caratteri: oltre, la riga tronca con i puntini invece di stringere l'interruttore |
| `entity` | sì | `switch.`, `light.` o `input_boolean.` — **niente `sensor.` né `button.`**: il servizio si ricava dal dominio, e `sensor.turn_on` non esiste |
| `icon` | no | uno dei nomi compilati nel firmware. Senza: una presa |
| `power` | no | un `sensor.`. Senza, la riga non lascia il posto vuoto |

Al massimo ventiquattro; dodici stanno in una pagina, oltre si impagina.

L'elenco delle icone **non si inventa**: è il gruppo `switches` di
`tools/genera_icone.py`, e lo schema e il validatore ne portano una copia che
`tools/verifica_icone_scelta.py` tiene allineata. Se divergessero, la pagina
offrirebbe un nome che il pannello accetta e che sul vetro diventa un
rettangolo vuoto — il modo peggiore di sbagliare, perché sembra un difetto
del disegno.

## 5. Entità mancanti

Un identificatore che non esiste in Home Assistant **non è un errore di
configurazione**: può essere un'integrazione temporaneamente giù. Il
pannello lo accetta, mostra il riquadro con "non disponibile" in grigio e
disabilita i comandi relativi. Nella pagina web quella riga viene
segnalata in giallo con "entità non trovata al momento".

Le entità che *sono* errori bloccanti sono solo quelle senza le quali una
schermata non ha senso: `energy.aggregate_sensor` per il fotovoltaico,
`calendar.sensor` per l'agenda. In quel caso la sezione sparisce dal rail
invece di mostrarsi vuota.

---

## 5-bis. Sensori aggregati

Tre schede della home — presenza, aperture, energia — leggono **un sensore
solo con tutto negli attributi**, invece di una entità per valore.

La ragione è che cinque numeri che cambiano insieme non meritano cinque
sottoscrizioni, e soprattutto che **chi scrive il template in Home
Assistant sa quali entità guardare meglio del pannello**. Dedurre chi è in
casa dalla somma di quattro `person` sarebbe una funzione in più da tenere
allineata, e sbagliata il giorno che qualcuno aggiunge un ospite.

| Configurazione | Stato | Attributi |
|---|---|---|
| `presence.sensor` | quante persone in casa | `people`: elenco di `{name, home, since}` |
| `house_state.openings_sensor` | quante aperture | `list`: elenco di nomi, oppure `openings`: elenco di `{name, since}` |
| `energy.aggregate_sensor` | produzione in kW | quelli che `energy.attributes` nomina: `house_w`, `grid_w`, `battery_w`, `battery_pct`, `today_kwh` |

`since` è **testo**: "da 35 minuti", "back around 19:15". Con una
sola eccezione: un orario nudo, "17:40", il pannello lo circonda con le
parole della sua lingua — "dalle 17:40", "since 17:40" — perché il
pacchetto dà l'ora e non sa in che lingua parla il pannello. Qualunque altro
testo lo scrive com'è: è il template a sapere se un rientro si può stimare,
e a decidere come si dice.

**I nomi italiani valgono ancora.** Fino allo schema 11 il pacchetto
`06-ha-package.yaml` diceva `persone`/`{nome, in_casa, da}`, `elenco`,
`aperture`, `casa`, `rete_w`, `batteria_w`, `batteria_pct`, `oggi_kwh`. Il
pannello legge prima il nome inglese e, se manca, quello italiano: una casa
che ha installato il pacchetto allora non deve toccare Home Assistant
perché il pannello ha cambiato lingua. Il pacchetto italiano aveva anche un
disaccordo con il pannello: scriveva l'ora in `da`, e il pannello la
cercava in `quando`, così sotto i nomi non compariva mai. Si leggono tutti
e tre.

Gli elenchi sono array di oggetti perché è la forma in cui una lista di
cose con dei campi si scrive senza inventare separatori da riparsare.

**Se il sensore non c'è la scheda dice che non lo sa**, e non resta
semplicemente vuota: una scheda vuota e una scheda che non sa si vedono
uguali, e sono cose diverse. Quando l'elenco manca ma il sensore c'è, resta
il conteggio del suo stato: è comunque un dato vero, e vale più di tre nomi
dedotti.

---

## 5-ter. Lo standby, e i sensori che si leggono in due modi

La schermata che il pannello mostra quando nessuno lo tocca — cioè quasi
tutto il tempo in cui è acceso. Il pannello sta al posto di un termostato,
quindi la temperatura della stanza è grande come l'ora.

```json
"standby": {
  "indoor_temperature": {
    "entity": "climate.soggiorno",
    "attribute": "current_temperature",
    "label": "in soggiorno"
  },
  "climate": "climate.soggiorno"
}
```

| Campo | Obbligatorio | Cosa fa |
|---|---|---|
| `indoor_temperature.entity` | sì, se c'è la sezione | l'entità che misura la stanza |
| `indoor_temperature.attribute` | no | se c'è, si legge questo invece dello stato |
| `indoor_temperature.label` | no | il nome sotto il numero. Senza: «in casa» |
| `climate` | no | da qui «chiesti 21,0°» e «sta scaldando» |

**Perché l'attributo si sceglie invece di indovinarlo.** Un sensore Zigbee
tiene la temperatura nello stato; una entità `climate` la tiene in
`current_temperature`, e il suo stato è una parola come «heat»; una sonda
dentro un dispositivo multiplo la tiene in un attributo col nome che le ha
dato chi ha scritto l'integrazione. Sono tutti sensori di temperatura, e
nessuno dei tre si legge come gli altri due.

Provare lo stato e ripiegare sull'attributo funzionerebbe quasi sempre, e
quel «quasi» è il problema: il giorno che uno stato dice `22.5` per motivi
suoi si mostrerebbe il numero sbagliato senza che niente protesti.

**La temperatura di fuori non sta qui**: sta in `weather`, dove c'era già.

```json
"weather": {
  "entity": "weather.casa",
  "local_temperature_sensor": "sensor.temperatura_esterna",
  "local_temperature_attribute": null
}
```

Il sensore locale vince sulla temperatura del servizio meteo — un sensore
sul balcone sa che tempo fa qui, il servizio sa che tempo fa in paese — e
`local_temperature_attribute` gli dà la stessa scelta stato/attributo. Del
servizio meteo lo standby usa solo la **condizione**, la parola accanto al
numero: se il servizio tace, il numero del sensore resta buono.

**I tre valori dell'energia** vengono dalla sezione `energy`, la stessa
della home. Non c'è un secondo posto dove dirli: due posti avrebbero potuto
dire due cose diverse sullo stesso vetro.

| Valore | Da dove |
|---|---|
| dal sole | lo **stato** di `energy.aggregate_sensor` |
| la casa | l'attributo `energy.attributes.house_w`, o calcolato se manca |
| batteria | l'attributo `energy.attributes.battery_pct` |

Il consumo di casa si calcola — produzione meno quello che va in rete e in
batteria — a meno che `house_w` non nomini un attributo che lo misura
davvero. Chi ha quel sensore ha un numero migliore del nostro conto.

Senza `battery_pct` la carica **non compare**: una casa senza accumulo non
ha quel numero, e uno zero al suo posto direbbe «scarica» invece di «non ce
n'è».

---

## 6. API tra pannello e pagina web

Il pannello espone un piccolo server HTTP sulla porta 80.

**Due percorsi sono sempre raggiungibili** (`/api/status` e `/api/log`):
sono in sola lettura, non restituiscono segreti e sono limitati a una
richiesta al secondo. È una deroga voluta, motivata in
`10-diagnostica.md` §4. Tutti gli altri percorsi rispondono `404` finché
la configurazione non viene sbloccata dal pannello.

| Metodo | Percorso | Effetto |
|---|---|---|
| `GET` | `/api/config` | JSON attuale, segreti oscurati |
| `POST` | `/api/config` | JSON completo, validato; risponde `200` o `422` con i campi rifiutati |
| `POST` | `/api/secrets` | solo scrittura: `{"ha_token": "..."}` |
| `GET` | `/api/entities` | elenco entità da Home Assistant, per i menu a tendina |
| `GET` | `/api/status` | contatori di diagnostica — **sempre attivo**, vedi `10-diagnostica.md` |
| `GET` | `/api/log` | righe del registro — **sempre attivo**, mai contiene segreti |
| `POST` | `/api/reboot` | riavvio |
| `GET` | `/api/config/export` | scarica il file, per farne una copia |
| `GET` | `/api/texts?lang=it` | i testi della pagina in quella lingua, con le modifiche; senza `lang`, la lingua del pannello |
| `GET` | `/api/texts?lang=it&edit=1` | per l'editor: ogni testo con inglese, testo incluso e modifica |
| `POST` | `/api/texts?lang=it` | salva modifiche ai testi; risponde `200` o `422` come `/api/config` |
| `POST` | `/api/texts/reset?lang=it` | dimentica tutte le modifiche di quella lingua |
| `GET` | `/api/texts/export?lang=it` | la lingua intera nel formato di `i18n/<codice>.json` |

#### `/api/texts`: le traduzioni modificate sul pannello

Ogni testo del pannello e della pagina si può cambiare dalla pagina, in
qualunque lingua il firmware contenga. Il pannello tiene **solo le
differenze** dal testo incluso, in un file per lingua (`texts-<codice>.json`
nell'archivio, fino a 128 KB): un aggiornamento del firmware che migliora una
traduzione arriva comunque a tutti i testi che nessuno ha toccato.

Il corpo del `POST` ha la forma del file, ed è una fusione, non una
sostituzione — la pagina può mandare le modifiche a lotti sotto il limite di
32 KB del corpo:

```json
{"panel": {"robot.clean_all": "Pulisci ogni stanza",
           "robot.alerts": {"one": "%d avviso"},
           "access.named_open": null},
 "web":   {"section.home.title": "Casa"}}
```

`null` toglie una modifica, e anche un testo **uguale a quello incluso** la
toglie invece di salvarla. Di una chiave plurale si toccano solo le forme
nominate. Tutto o niente: se un testo non passa, il file non cambia e il
`422` dice quali e perché (`"field": "panel.access.named_open"`).

I controlli sono quelli di `tools/gen_texts.py`, rifatti sul pannello perché
un testo modificato dalla pagina non passa dalla compilazione:

- **gli stessi segnaposto dell'inglese.** Per il pannello le conversioni
  `printf`, nello stesso ordine e uguali in tutto (`%.1f` non è `%.2f`); per
  la pagina i `{nomi}`, in qualunque ordine;
- **solo caratteri che i font del pannello sanno disegnare** (Latino-1, `€`,
  virgolette e trattini tipografici). La pagina li disegna col browser, e il
  controllo lì è solo che sia UTF-8 valido;
- `keyboard.letters` sono tre righe di a–z separate da `|`, con tutte le 26
  lettere;
- al massimo 1024 byte per testo.

Gli stessi controlli si rifanno **quando il testo si usa**: un aggiornamento
può cambiare i segnaposto di un testo inglese, e una modifica salvata prima
passerebbe una stringa dove si aspetta un numero. Una modifica così resta nel
file, non si usa, e l'editor la mostra con `"stale": true`.

Dopo un salvataggio il pannello ridisegna le schermate, **senza** ricollegarsi
a Home Assistant come fa dopo un cambio di configurazione: sono cambiate solo
delle parole.

#### La pagina non ha testi suoi

Anche la pagina di configurazione chiede i propri testi a `/api/texts`, nella
lingua del pannello o in quella scelta in alto (`?lang=` nell'indirizzo, non
in localStorage). Stanno nella sezione `web` di `i18n/<codice>.json`:

- `field.<percorso>.label` per **ogni** campo dello schema, `.hint` per quelli
  che hanno un aiuto, `.option.<valore>` per le scelte che hanno un nome;
- `section.<id>.title` e `.lead`, `secret.<nome>.title` e `.hint`, e i
  messaggi della pagina.

Le vecchie `description` dello schema sono diventate gli aiuti dei campi e
dallo schema sono uscite: la pagina non le leggeva più, e un testo da tradurre
non può stare in un file che non si traduce. `tools/check_page_texts.py`
(ctest `page_texts`) verifica che ogni chiave chiesta dalla pagina esista, che
ogni campo dello schema abbia la sua etichetta e che nessun testo nomini un
campo che lo schema non ha più.

#### `/api/entities`, e perché non filtra per dominio

Prometteva `?domain=light`. Non lo prende: risponde con tutte, e a
separarle per dominio è la pagina. Una richiesta invece di una per ogni
tipo di campo, e il pannello non deve tenere né rileggere il filtro.

La forma è sempre la stessa, e il campo `state` c'è solo quando l'elenco è
vuoto — perché vuoto senza spiegazione e vuoto perché la casa non ha
entità si somigliano troppo:

```json
{"entities": ["light.salotto", "sensor.potenza"]}
{"entities": [], "state": "on its way"}
{"entities": [], "state": "not connected to Home Assistant"}
{"entities": [], "state": "Home Assistant cannot list them"}
```

**La prima domanda torna quasi sempre «in arrivo»**, e non è un difetto:
l'elenco arriva da Home Assistant su WebSocket, e questo server risponde
dal ciclo principale senza aspettare nessuno. La pagina richiede una volta
dopo un secondo e poi smette — se non arriva resta ai campi di testo, che
è come si stava prima.

Si chiede **quando la pagina lo domanda**, mai al collegamento: un pannello
a muro sta acceso per mesi senza che nessuno apra la configurazione, e
quell'elenco sono qualche decina di kilobyte.

Le tendine sono `datalist` e non `select`: il campo resta libero da
scrivere. Un'entità può esistere senza stare nel registro — quelle create
da template, per esempio — e un menu chiuso la renderebbe impossibile da
mettere.

L'elenco entità passa dal pannello e non dal browser: così la pagina web
funziona anche da un dispositivo che non ha il token.

---

## 7. Ripristino

Doppio tocco lungo sull'angolo in alto a sinistra della schermata
Impostazioni per dieci secondi → cancella NVS e LittleFS e riparte dal
primo avvio. Volutamente scomodo: è l'unica via se il Wi-Fi cambia e il
pannello non riesce più a connettersi a nulla.

---

## 8. Configurazione condivisa fra i due pannelli

I pannelli di casa usano lo stesso schema.
Un `config.json` esportato dall'uno si importa nell'altro: i campi
sconosciuti al profilo di destinazione (per esempio
un campo che il profilo montato non usa) vengono **conservati ma
ignorati**, non cancellati. Così l'esportazione fa da copia di sicurezza
per entrambi e un ritorno indietro non perde impostazioni.

Resta necessariamente diverso, e va lasciato fuori da un'eventuale copia,
`system.panel_name`.


---

## 9. Cosa NON sta nella configurazione

Larghezze delle immagini, cadenze, numero di colonne delle griglie,
dimensioni dei caratteri: sono **costanti di profilo**, non scelte
dell'utente. Vivono in `09-profili.md` e in `profile.h`.

La configurazione contiene solo ciò che dipende dalla casa: quali entità,
con che nome, in che ordine, con quali conferme. È la ragione per cui lo
stesso `config.json` funziona su entrambi i pannelli.


---

## 10. Regole fra campi

JSON Schema esprime bene tipi e intervalli, non i legami fra campi
distinti. Queste vanno implementate a mano, **sia nel firmware sia nella
pagina web**, e sono già codificate in `verifica_config.py`:

| Regola | Gravità |
|---|---|
| Un accesso di tipo `pulse` deve avere `pulse_ms` | errore |
| `limiti_*.min` deve essere minore di `limiti_*.max` | errore |
| `brightness_standby` non può superare `brightness_active` | errore |
| `off_after_s`, se diverso da 0, non può precedere lo standby | errore |
| Una sezione in `sections` deve avere la propria sorgente dati | errore |
| `timer` e `timer_automation` vanno valorizzati insieme | avviso |
| Stessa entità usata da due accessi | avviso |

La distinzione conta: un **errore** blocca il salvataggio, un **avviso**
no. Un avviso segnala una configurazione che funziona ma non fa quello
che probabilmente ti aspetti.


---

## 11. Chiavi accettate che nessuno legge

Misurate confrontando le foglie di `config.schema.json` con i percorsi che
il firmware chiede davvero. Sono la peggior specie di difetto: la pagina le
offre, il validatore le controlla, il salvataggio le conserva, e sul muro
non succede niente. Chi le imposta non ha nessun modo di capire che sta
parlando col vuoto, e conclude ragionevolmente che il pannello è rotto.

Il criterio per riconoscerle vale la pena dichiararlo: una chiave letta
**solo da `validazione.c`** è validata e mai usata, che dal muro è
indistinguibile da una inventata.

### Tolte l'08/09/2026: tutto il blocco `telecamere`

Le telecamere sono uscite dal progetto su richiesta di chi lo usa, e con
loro `telecamere` per intero, `home.striscia_telecamere`,
`accessi[].telecamera` e la voce `telecamere` in `sections`.

Queste **non** rientrano fra le «chiavi accettate che nessuno legge»: sono
tolte davvero, perché lo schema non ammette campi che non conosce e una
configurazione che se le portasse dietro verrebbe rifiutata al primo
salvataggio. Le toglie la migrazione allo **schema 5**, come era successo
col PIN. Chi aggiorna non deve fare niente.

Le credenziali in NVS — `go2rtc_user`, `go2rtc_pw` e le sedici `camN_*` —
le cancella `segreti_avvia()` al primo avvio, per nome: una password che
sopravvive alla funzione che la usava è il genere di cosa che si scopre
anni dopo leggendo una partizione.

### Ritirata il 06/09/2026: `clima.sensore_esterno`

Non era una chiave morta — il pannello la leggeva davvero — ed è peggio:
diceva **la stessa cosa** di `weather.local_temperature_sensor`, e la
sezione Clima leggeva da lei invece che dalla fonte comune.

Due chiavi per un fatto solo sono due numeri diversi sullo stesso vetro il
giorno che qualcuno ne cambia una. E finché nessuno la cambiava, il difetto
restava invisibile: le due chiavi puntavano allo stesso sensore e i due
numeri coincidevano.

C'era anche un valore di ripiego che mentiva. Se lo stato di quel sensore
non era un numero, la pastiglia «Esterno» scriveva **30,4°** — un numero
che sembra una temperatura di fuori. La home e lo standby, nella stessa
situazione, scrivevano «—», che è la verità.

Adesso la temperatura di fuori la risolve un posto solo, e nell'ordine:
il sensore locale di `weather` (dallo stato o dall'attributo dichiarato), poi
la temperatura del servizio meteo, poi **niente**. La chiave si conserva
come ogni chiave che lo schema non conosce più (§12), e il validatore
avvisa dove va messo il valore.

### Tolte il 30/08/2026

Sedici, su 132. Erano funzioni che non esistevano:
`stato_casa.{sensore_luci, allarme, modalita_casa}`,
`clima.timer_gruppo.{nome, timer, automazione_timer}`,
`diagnostica.{livello_registro, righe_registro, soglia_heap_avviso}`,
`sistema.lingua` (tornata allo schema 10 come `system.language`, quando il pannello ha imparato le lingue), `luci.sensore_conteggio`, `energia.storico_giornata`,
`meteo.ore_previsione`, `presenza.max_persone_home`,
`home_assistant.secondi_timeout`, `condivisione_wifi.luminosita_massima`.

> `clima.timer_gruppo` non va confuso con i campi `timer` e
> `timer_automation` delle **singole unità** clima, che sono vivi e hanno
> una regola loro in §10.

**Nessuna configurazione esistente si rompe.** Il validatore C e la pagina
web ignorano le chiavi che non conoscono — iterano le proprietà note dello
schema, non quelle del documento — quindi un `config.json` che le contiene
ancora continua a caricarsi e a salvarsi, e quei campi restano conservati e
inerti. L'unico severo è `verifica_config.py`, che usa `jsonschema` e
onora `additionalProperties: false`: su un file non ripulito segnala un
errore che il pannello non segnalerebbe. Vedi §12.

### Rimaste, e perché

- `calendar.{sensor, days, home_max_events}` e
  `calendar.calendars[].{id, name, color}` — la sezione Agenda esiste ed è
  raggiungibile, ma gira su dati finti. Non sono chiavi da togliere, sono
  una funzione da finire.

---

## 12. La chiave sconosciuta si conserva

C'è un validatore scritto a mano in C (`main/cfg/validazione.c`), che è
quello che gira sul pannello, e uno di riferimento in Python
(`verifica_config.py`) che usa `jsonschema`.
`tools/confronta_validazione.py` genera decine di varianti sbagliate della
configurazione vera e pretende lo stesso verdetto da entrambi.

Su una cosa divergevano, e non era coperta da quel confronto: una chiave
che lo schema non conosce. Il firmware e la pagina web l'accettavano —
iterano le proprietà **note dello schema**, non quelle del documento —
mentre `verifica_config.py` la rifiutava, onorando
`additionalProperties: false`.

**La regola vera è: si conserva.** È già quella dichiarata in §2, ed è
quella che rende indolore togliere una chiave dallo schema senza rompere le
configurazioni installate — come è successo con le sedici di §11.

Il segnale però non si butta. `additionalProperties: false` resta nello
schema e la violazione è diventata un **avviso**: un campo scritto male —
`sensore_apertureX` invece di `openings_sensor` — non è un motivo per
rifiutare un file, ma è precisamente la cosa che si vuole sapere, perché dal
muro si presenta come un'impostazione che non fa niente.

| | chiave in più |
|---|---|
| validatore C (sul pannello) | accetta |
| pagina web | accetta |
| `verifica_config.py` | accetta, **con avviso** |

Il confronto adesso copre anche questa variante, in due forme — una chiave
in più in una sezione e una annidata più a fondo. Prima generava soltanto
*valori* sbagliati per chiavi note, e per questo non poteva accorgersene:
una prova che non prova il caso non dice niente su quel caso.


## 13. Il campo facoltativo che perdeva il tipo

Un campo facoltativo, nello schema, si scrive così:

```json
"max_power": { "anyOf": [ { "type": "integer" }, { "type": "null" } ] }
```

È il modo corretto di dire «un intero, oppure niente». Ha un costo che non
si vede: **il tipo smette di stare al primo livello**. `s.type` diventa
`undefined`, e chiunque lo guardi non lo trova più.

La pagina web lo guardava in due punti — per decidere che campo disegnare e
per validare — e in tutti e due non lo trovava. Risultato: davanti a un
intero metteva una casella di testo, e salvava

```json
"max_power": "1000"
```

Il pannello lo legge con `cfg_intero_in()`, che di una stringa non sapeva
che farsene e ripiegava su zero. Con zero, la barretta del consumo di
lavatrice e asciugatrice **non si disegna** — è la regola giusta, perché
senza il pieno carico non si sa rispetto a cosa riempirla. Quindi: nessun
errore nella pagina, nessun avviso nel registro, nessun rifiuto dal
validatore, e sul muro una barra che non compare. Il difetto si è
presentato come un problema di disegno.

Erano trenta i campi scritti così, e per i ventotto di tipo `string` la
pagina in più **non controllava il formato**: un `sensor` scritto male
passava senza una parola.

**La regola, adesso, ha due metà.**

*Chi scrive è severo.* La pagina risolve l'`anyOf` prima di guardarlo
(`appiattisci()`, un solo ramo non-null sale di livello) e quindi salva un
intero come intero. `tools/verifica_pagina_tipi.py` lo pretende, ed è in
ctest: è una prova che si può fare senza aprire un browser, perché il
guasto sta in *quale schema* la pagina guarda, non in come lo disegna.

*Chi legge è tollerante.* `cfg_intero()` e `cfg_intero_in()` accettano anche
una stringa di sole cifre. Non è indulgenza verso i dati sbagliati: questo
documento lo scrivono versioni diverse della stessa pagina, e una
configurazione salvata ieri deve continuare a valere domani. Restano fuori
`"1000 W"`, `"12.5"` per un intero, e tutto ciò che numero non è: valgono il
ripiego, come prima.

> **Il modo in cui è stato trovato conta.** Nessuno dei controlli automatici
> poteva vederlo: lo schema era giusto, il documento era accettabile, il
> codice era corretto in ogni suo pezzo. Si è visto perché qualcuno ha
> guardato il muro e ha detto «manca una barra» — e la prima ipotesi, che
> fosse un problema di impaginazione, era sbagliata: il simulatore ha
> mostrato la barra al posto giusto, e a quel punto la domanda è diventata
> «allora cosa è diverso sul pannello» — che porta al dato invece che al
> disegno.