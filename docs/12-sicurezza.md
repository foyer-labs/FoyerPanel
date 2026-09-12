# Foyer Panel — Contratto di sicurezza

Questo documento è nato tardi, e si vede da come è stato trovato: due
commenti nel codice citavano `07-sicurezza.md §4` per giustificare cosa
**non** scrivere nel registro, e quel documento non è mai esistito. Le
regole c'erano — sono state applicate, e in un caso hanno impedito un
guaio — ma vivevano nella testa di chi scriveva e in commenti sparsi.

Il modello di minaccia è dichiarato per primo, perché quasi tutte le scelte
che seguono discendono da lì e senza di esso sembrano incoerenti.

## 0. Cosa si sta proteggendo, e da chi

**Questo è un pannello di casa, non un apparato aziendale.** Chi è nel
soggiorno ha già superato la porta di casa, che è una barriera migliore di
qualunque password. Metterne una davanti allo schermo non aggiungerebbe
sicurezza: aggiungerebbe attrito a chi ha diritto di stare lì, e verrebbe
aggirata con un foglietto attaccato dietro.

Quello che si protegge è più stretto e più concreto:

1. **Le credenziali non devono uscire dalla casa** — non nel file di
   configurazione che si esporta, non nel registro che si manda a chi
   aiuta, non nell'immagine che si scrive in flash.
2. **La configurazione non si cambia dalla rete e basta** — serve essere
   stati fisicamente davanti al pannello.
3. **Un firmware non nostro non si installa via rete.**

E, altrettanto importante, quello che **non** si protegge: vedi §6.

## 1. Cosa è un segreto, e dove sta

Sono segreti: le password Wi-Fi (rete di casa, rete ospiti e le reti
private condivisibili) e il token di Home Assistant.

> **Le credenziali delle telecamere sono state segrete due volte, e due
> volte hanno smesso di esserlo.** Prima `go2rtc_user`, `go2rtc_pw` e le
> sedici `camN_usr` / `camN_pw` (fino all'08/09/2026); poi le otto
> `camN_utente` / `camN_password` della sezione in RTSP, vissuta il
> 09/09/2026 dalla mattina al pomeriggio.
>
> Uscire dall'elenco non è uscire da NVS: quelle chiavi restavano scritte
> in una partizione che nessuna riga del programma avrebbe più nominato,
> cioè che nessuno avrebbe più cancellato. Le cancella `segreti_avvia()`
> al primo avvio del firmware nuovo, per nome, una volta sola.
>
> **La seconda tornata pesa più della prima**: `cam1_password` ha contenuto
> una password vera, scritta a mano da chi usa il pannello, per una
> telecamera che sta in casa. Non era un residuo di prova. Una password che
> sopravvive alla funzione che la usava è il genere di cosa che si scopre
> anni dopo leggendo una partizione.

Stanno in NVS, nello spazio `secrets`. **Non stanno in `config.json`**, e
la ragione è che `config.json` viaggia: si esporta dalla pagina web, si
importa su un altro pannello, si allega a un messaggio quando qualcosa non
va. Un segreto dentro un file fatto per essere copiato è un segreto
copiato.

> **Una volta è quasi andata storta.** L'immagine LittleFS veniva costruita
> dall'intera cartella `dati/`, che conteneva anche i segreti di prova: la
> password del Wi-Fi è finita dentro la partizione scritta in flash.
> Adesso l'immagine si costruisce da una cartella di allestimento che
> contiene **solo** `config.json`. La regola generale: non si impacchetta
> una cartella, si impacchetta un elenco di file.

## 2. La pagina li scrive, non li rilegge

`POST /api/secrets` accetta i segreti. `GET /api/secrets` restituisce
**soltanto** `{"nome": {"impostato": true}}` — mai il valore.

Non è pignoleria. Una pagina che li rileggesse trasformerebbe la finestra
di quindici minuti in una copia permanente: basterebbe aprirla una volta,
mentre è sbloccata, per portarsi via tutto. Con la scrittura sola, quella
finestra permette di *cambiare* le credenziali, non di *conoscerle*.

Un campo lasciato vuoto nella pagina non cancella il segreto: vuol dire
"non lo cambio". Cancellare si fa dal ripristino.

## 3. Lo sblocco fisico

Tutti i percorsi che leggono o scrivono la configurazione — compreso
`/api/ota` — rispondono **404** finché qualcuno non tocca «Consenti
configurazione» nelle impostazioni a bordo. Da quel tocco valgono quindici
minuti (`WEB_SBLOCCO_MS` in `main/web/web.h`).

Tre proprietà che vanno tenute insieme, perché ognuna senza le altre non
serve:

- **La finestra non si rinnova dalla rete.** Non esiste un percorso che la
  prolunghi. Per riaprirla bisogna tornare davanti al pannello, ed è tutto
  il punto.
- **Si perde al riavvio.** È una variabile in RAM, non uno stato salvato:
  un pannello che riparte riparte chiuso.
- **`/api/ota` controlla lo sblocco per conto suo**, dentro `ota_apri()`,
  e non si affida allo smistamento. Quel percorso è a flusso e scavalca il
  gestore comune: è proprio il genere di scorciatoia che lascia una porta
  aperta senza che nessuno se ne accorga.

### La deroga per il montaggio

`diagnostics.config_always_open` tiene la porta **sempre aperta**:
niente finestra, niente tocco sul vetro. Esiste perché il blocco dei quindici
minuti è giusto su un pannello finito e d'intralcio su uno che si sta ancora
montando — si tocca il vetro, si salva, si tocca di nuovo, dieci volte in un
pomeriggio.

Va detto per intero cosa spegne: **finché è accesa, chiunque sia sulla rete
di casa può cambiare la configurazione, scrivere i segreti e installare un
firmware**, senza essere mai stato davanti al pannello. È la barriera del §0
punto 2, e non c'è più.

Il valore di riposo è **spento**, e non è una formalità: un firmware che
nasce spalancato è esattamente la cosa che si dimentica accesa. Per la
stessa ragione, quando è accesa il pannello lo dice in **due** posti — la
riga nelle impostazioni a bordo («sempre aperta dalla configurazione — non
si richiude») e un avviso nel registro a ogni avvio. Spegnerla richiude la
porta **subito**, compresa la sessione da cui è arrivato il comando.

> I posti erano tre: c'era anche una fascia rossa in cima alla pagina web,
> tolta il 06/09/2026 su richiesta. Una fascia che non si può chiudere
> smette di essere un avviso e diventa arredamento: si impara a non
> vederla, e con lei si impara a non vedere quel posto della pagina. I due
> che restano si incontrano quando si va a cercare — nelle impostazioni e
> nel registro — invece di stare sempre lì.

Restano raggiungibili senza sblocco solo i percorsi di sola lettura sullo
stato (`/api/status`, `/api/log`), con un limite di **una richiesta al
secondo per chiamante**. Non è una difesa da chi insiste — chi insiste
insiste — ma da un incidente: un'integrazione che interrogasse in ciclo
stretto scalderebbe un apparecchio raffreddato passivamente dietro una
placca.

## 4. Cosa non entra nel registro

Il registro diagnostico si esporta e si manda a chi aiuta. Quindi:

**Non contiene mai** token, password né SSID.

Al loro posto ci va ciò che risponde alla domanda senza rivelare niente:

| Invece di | Si scrive |
|---|---|
| i nomi delle reti trovate | **quante** ne ha trovate, e quante avevano un nome |
| il token rifiutato | che l'autenticazione è stata rifiutata |

Un conteggio basta a distinguere «la radio non sente» da «le reti si
perdono per strada», che è l'unica domanda a cui quella riga deve
rispondere.

Il punto dove questa regola è applicata esplicitamente è
`firmware/main/sistema_esp.c`, la scansione delle reti.

> Il codice delle telecamere ne aveva un altro, e la mossa vale la pena
> ricordarla per quando servirà di nuovo: per firmare una richiesta Digest
> serve `MD5(utente:realm:password)`, e comporre quella stringa in un
> buffer sarebbe stato **l'unico posto del programma in cui la password
> sta in chiaro in una variabile con un nome**, lunga abbastanza da
> sopravvivere a un dump della pila. Si dava invece in pasto a MD5 un pezzo
> per volta, dentro la funzione che `segreti_usa()` chiama e basta.

## 5. Il firmware si firma

`CONFIG_SECURE_SIGNED_APPS` e `CONFIG_SECURE_SIGNED_ON_UPDATE` sono
attivi. La chiave è `firmware/chiave_firma.pem` e **non è versionata**.

ESP-IDF verifica la firma **alla chiusura del trasferimento**, su tutta
l'immagine, prima che la partizione di avvio cambi. Un'immagine non firmata
con quella chiave non arriva mai a essere avviabile.

Va detto con precisione cosa questo è: è **verifica delle immagini OTA**,
non Secure Boot. Gli efuse non sono bruciati. Chi ha in mano la scheda e un
cavo USB può scrivere quello che vuole — vedi §6.

## 6. Cosa questo modello NON protegge

Elencato apposta, perché una difesa di cui si sopravvaluta la portata è
peggio di una che non c'è.

- **L'accesso fisico.** Un cavo USB dà il controllo completo: si riscrive
  il firmware, si legge la flash, si leggono i segreti in NVS (che non è
  cifrata). È una scelta, non una dimenticanza: è un pannello avvitato a un
  muro dentro una casa, e chi è arrivato lì ha già problemi più grossi.
- **Chiunque sia sulla rete di casa** può leggere `/api/status` e il
  registro. Contengono lo stato della casa, non le credenziali.
- **Non c'è TLS** sulla pagina del pannello. Le credenziali che si scrivono
  dalla pagina passano in chiaro sulla rete locale.
- ~~Il banco di prova del robot~~ — **spento dal 31/08/2026**, quando le
  stanze sono state confermate. `/api/test/robot` non esiste più
  nell'immagine di riposo: il codice è ancora in albero, dietro
  `-D PANNELLO_BANCO_ROBOT=1`, come il banco del rollback OTA. Riaccenderlo
  riapre un comando **fisico** a chiunque sia sulla rete di casa, quindi si
  accende per una prova concordata e in una cartella di build sua.
- **`diagnostics.config_always_open`, mentre è accesa**, toglie la
  barriera del §3 per intero. È una deroga dichiarata e temporanea, con una
  data di scadenza che non è scritta da nessuna parte se non qui: va spenta
  quando il pannello è stabile.

## 7. Come si verifica che questo documento sia ancora vero

`tools/verifica_citazioni.py` controlla che ogni sezione citata dal codice
esista davvero. È nato proprio dalla scoperta che generò questo file: un
rimando a `07-sicurezza.md §4` che non puntava a niente, e un rimando del
robot a `§4.9` che puntava a «Primo avvio» — una citazione sbagliata è
peggio di una mancante, perché sembra giusta.
