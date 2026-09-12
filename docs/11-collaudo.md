# Foyer Panel — Piano di collaudo

"Indistinguibile dai mockup" è un criterio sull'aspetto. Qui ci sono i
criteri sul **comportamento**, che è dove il codice si rompe davvero.

Ogni voce si verifica con un sì o un no. Se una richiede giudizio, è
scritta male.

---

## 1. Criteri di accettazione per fase

### Fase 1 — Interfaccia nel simulatore

- [x] Ogni schermata della specifica si apre **in entrambi i profili**
- [x] Il tasto HOME riporta alla home da qualunque sezione e da qualunque
      modale aperto
- [x] Il timer di inattività si azzera a ogni tocco, anche su un'area
      inattiva dello schermo
- [x] Dopo 60 s in una sezione si torna alla home; dalla schermata Wi-Fi
      dopo 120 s
- [x] Standby dopo 120 s, spegnimento dopo 600 s, risveglio al tocco
      senza che il tocco di risveglio attivi un comando
- [x] Nello standby **la temperatura della stanza è grande come l'ora** e
      non più piccola: il pannello sta al posto di un termostato
      — `--stato standby` in tutti i profili
- [x] In verticale i due numeri si impilano invece di affiancarsi, e la
      fascia in fondo passa da quattro in riga a due per due
- [x] La temperatura si legge dallo **stato** o da un **attributo**, a
      scelta della configurazione — `prova_ha` legge una entità `climate`
      il cui stato è una parola e il cui `current_temperature` è un numero:
      leggendo lo stato la prova fallirebbe
- [x] Un dato che manca **non compare**, e quelli che restano si
      ridistribuiscono senza lasciare il buco: niente trattini in mezzo ai
      numeri
- [x] La pastiglia delle aperture c'è solo quando qualcosa è aperto
      — `--casi apertura`
- [x] La paginazione mantiene le schede della stessa dimensione anche
      nell'ultima pagina incompleta
- [x] Nessuna schermata alloca memoria a ogni ridisegno: l'heap dopo
      dieci minuti di navigazione è pari a quello iniziale entro il 2%
      — **con un riscaldamento**, vedi sotto
- [x] Cambiando profilo **non serve toccare alcun file oltre a
      `profile.h`**, compreso il passaggio fra orizzontale e verticale
- [x] In verticale la home **non mostra il dock** e la barra in basso è
      sempre visibile; in orizzontale il contrario
- [x] Ogni superficie video mantiene il rapporto 16:9 su tutti i profili,
      riquadro di errore compreso
- [x] Nessun numero di layout compare in un sorgente diverso da
      `profile.h`
- [x] **Nessun comando resta fuori dallo schermo**, in nessuno dei
      profili: se una schermata non ci sta, va impaginata, non tagliata

Quest'ultimo criterio mancava, e proprio per questo i profili sono
arrivati alla fine della Fase 1 con il dettaglio del condizionatore che
sborda. Le catture si guardavano una per una e l'occhio andava a quello
che c'era, non a quello che mancava sotto il bordo.

Il profilo orizzontale è a posto. Ci sono voluti quattro
provvedimenti insieme, e nessuno da solo sarebbe bastato:

1. il blocco del deflettore prende l'altezza che gli serve invece di una
   quota uguale agli altri — ne ha due, di righe di comandi;
2. "Deumidifica" e "Ventilatore" nei riquadri diventano "Deum." e "Vent.",
   così le sei modalità stanno su una riga sola;
3. le righe a pastiglia di oscillazione ed extra scendono a 46 px
   (§8 di `09-profili.md`), sopra il minimo di tocco;
4. il padding interno delle schede smette di essere quello di schermo e
   diventa quello dei mockup, che è più stretto (§3 di `09-profili.md`) —
   da solo vale 24 px sulla colonna destra.

Sul verticale non bastavano, e non per pochi pixel: in colonna unica il
dettaglio impila i contenuti di tutte e due le colonne dell'orizzontale, e
mancano più di duecento pixel. Era un problema di disposizione, non di
misure, e si è risolto **paginando**: due pagine, frecce in testata e
pallini in fondo, come in ogni altra schermata paginata del pannello. Prima
pagina quello che si guarda, seconda quello che si imposta
(`01-specifica-ui.md` §3.2).

| Profilo | Dettaglio del condizionatore |
|---|---|
| `p4-1280x800` | ci sta per intero, una pagina |
| `p4-800x1280` | due pagine, tutto raggiungibile |

`tools/cattura.py` fotografa **tutte e due** le pagine: senza, il giorno che
oscillazione ed extra tornassero sotto la barra in basso non se ne
accorgerebbe nessuno. Sui profili orizzontali la seconda cattura viene
identica alla prima, ed è giusto così — lì la pagina è una sola.

#### Come sono verificati

Dodici prove in `ctest`, che si lanciano con
`ctest --test-dir build --output-on-failure`:

| Prova | Cosa verifica |
|---|---|
| `profilo`, `prf_*` | le tabelle dei profili, e ognuno compilato da solo |
| `numeri_di_layout` | nessun numero fuori da `profile.h` |
| `profilo_allineato` | `profile.h` rigenerato da `profili.json` |
| `misure_dai_mockup` | `profili.json` allineato al CSS dei mockup |
| `tempi_*` | ritorno alla home, standby, spegnimento, risveglio |
| `heap_*` | la memoria non cresce navigando |

Le schermate si controllano a vista con `python tools/cattura.py`, che
produce le immagini di ogni schermata in tutti i profili, da mettere
accanto ai mockup.

#### Le temporizzazioni hanno insegnato tre cose

La prova `--prova-tempi` e nata per spuntare quattro caselle e ne ha
trovate tre da correggere.

**Il ritorno automatico alla home azzerava il conto dell'inattivita.**
`ui_vai` chiamava `lv_display_trigger_activity`, che serviva ai tocchi veri
— ma quelli LVGL li registra da solo. Il risultato era che bastava lasciare
una sezione aperta perche il pannello non andasse **mai** in standby: a
60 s tornava a casa, il conto ripartiva, e cosi all'infinito.

**Sulla schermata Wi-Fi le due soglie coincidono.** Ritorno alla home a
120 s e standby a 120 s: chi vince decide se chi sta inquadrando il codice
QR si trova la vista di standby sopra il codice. Vince il ritorno alla
home, e lo standby arriva subito dopo.

**Dallo schermo spento non ci si risvegliava.** Il risveglio era appeso al
tocco sul velo dello standby; passando per lo spegnimento quel velo c'era
ancora ma lo stato no. Ora qualunque attivita recente riporta indietro, da
entrambi gli stati.

#### Nota sull'heap: serve un riscaldamento

Il criterio così com'è scritto non passa, e non perché ci sia una perdita.
Le cache di LVGL — glifi disegnati, buffer di tracciamento — si riempiono
nei primi giri di navigazione e poi si fermano: misurato, l'heap sale per
una ventina di giri completi e da lì in poi resta piatto entro poche
centinaia di byte.

Il criterio va quindi letto così: **dopo che le cache si sono assestate**,
l'heap non cresce più. Una perdita vera non si assesta mai, e resta
distinguibile.

`./build/pannello --profilo X --prova-heap N` lo verifica: venticinque giri
di riscaldamento, poi N giri misurati su tutte le ventuno destinazioni
dell'interfaccia, stati compresi. Entra in `ctest`. Con venti giri misurati
la crescita è di 1440 byte su entrambi gli orientamenti, contro una soglia di
circa 5700.

Una cosa che questa prova ha già insegnato: **riscaldamento e misura devono
fare esattamente le stesse cose**. La prima versione riscaldava solo le
schermate e misurava anche gli stati, e le cache degli stati si riempivano
dentro la finestra di misura — 8 kB di crescita che sembravano una perdita
e non lo erano.

### Fase 2 — Configurazione e pagina web

- [x] Un `config.json` valido viene letto e applicato senza riavvio, per
      tutto ciò che la specifica dichiara applicabile a caldo — salvando
      dalla pagina web, il pannello rilegge e ridisegna dov'era
- [x] La **struttura** dell'interfaccia — quali zone, quali unità, quali
      accessi, quali sezioni e in che ordine — viene da
      `config.json` e non dal codice. Il finto fornitore di dati inventa
      solo lo **stato**
- [x] Un `config.json` corrotto fa ripartire dalla copia di sicurezza
- [x] Corrotti entrambi, si va al primo avvio senza restare bloccati
- [x] Un salvataggio interrotto a metà (spegnimento durante la scrittura)
      non lascia il file in stato ibrido
- [x] La pagina web è irraggiungibile finché non si sblocca dal pannello,
      **tranne** `/api/status` e `/api/log`
- [x] Allo scadere dei 15 minuti la pagina si richiude da sola
- [x] Un campo non valido produce `422` con il nome del campo, e **il file
      non viene toccato**
- [x] `/api/status` e `/api/log` sono limitati a una richiesta al secondo
      per chiamante, e non contengono SSID né token
- [x] Con `diagnostics.status_endpoint` a falso non resta esposto neanche
      quello
- [x] I segreti scritti non tornano indietro in lettura — `segreti.h` non
      ha una funzione che ne restituisca uno, e un campo dal nome sospetto
      finito a mano in `config.json` viene oscurato in esportazione
- [x] Il `config.json` esportato dal pannello A si importa nel pannello B
      senza perdere campi
- [ ] Sul pannello vero: la pagina si raggiunge da telefono e da computer
      su `http://<nome-del-pannello>.local` — richiede mDNS e l'hardware
      (Fase 5). Il nome viene da `system.panel_name`

Due cose che questa fase ha già insegnato.

**Un confronto che passa sempre non prova niente.** La validazione scritta a
mano deve corrispondere a `config.schema.json` campo per campo, e niente lo
impone: si allontana un campo per volta e nessuno se ne accorge finché una
configurazione buona non viene rifiutata sul muro.
`tools/confronta_validazione.py` genera sessantacinque varianti sbagliate
della configurazione vera e pretende lo stesso verdetto dai due validatori —
ma prima di fidarsene è stata provata all'incontrario, allentando un limite
nel solo validatore C per vedere il confronto segnalare la discordanza.

**Due criteri erano spuntati senza una prova sotto**, e per un po' non se
n'era accorto nessuno: la scrittura atomica e la conservazione dei campi
sconosciuti erano vere *per costruzione* — il salvataggio passa da un
temporaneo e una rinomina, il documento resta un albero JSON e non una
struttura C — ma "vero per costruzione" e "verificato" non sono la stessa
cosa, e la differenza si scopre il giorno che qualcuno rifa il
salvataggio. Adesso `test/prova_config.c` interrompe davvero un
salvataggio a metà e fa il giro completo con una sezione che il firmware
non conosce.

**Le regole di accesso si provano chiamando la funzione, non aprendo un
socket.** `test/prova_web.c` chiama `web_servi()` con richieste costruite a
mano: quello che va verificato è *chi può cosa*, e un socket di mezzo
aggiunge soltanto modi di fallire che non c'entrano — porte occupate, tempi
di attesa, un ordine di esecuzione che cambia da una macchina all'altra. Il
trasporto è provato dal fatto che il simulatore serve la pagina davvero; le
regole no, e sono quelle che lasciano un pannello aperto sulla rete se si
sbagliano. Quarantasei asserzioni, e il tempo scorre a comando: la finestra
di quindici minuti si verifica in un microsecondo.

**Una prova sulla configurazione d'esempio non distingue un elenco che legge
da un elenco che indovina**, perché i valori di esempio *sono* quelli della
casa vera: un elenco rimasto agganciato al codice darebbe gli stessi
conteggi. `tools/prova_struttura.py` confronta con undici configurazioni
diverse — meno zone, scene aggiunte, zone rinominate, sezioni in altro
ordine, l'agenda accesa, la rete ospiti spenta — e anche questa è stata
provata all'incontrario fissando a mano il numero delle luci.

### Fase 3 — Home Assistant

- [x] Token errato: messaggio chiaro, non un riavvio in ciclo
- [x] Perdita del collegamento: fascia di riconnessione entro 15 s, dati
      sbiaditi con l'ora dell'ultimo valore valido
- [x] Oltre 60 s: schermata piena di irraggiungibilità
- [x] Al rientro si torna **alla schermata dov'eri**, non alla home
- [x] Un comando inviato mentre il collegamento cade produce "non
      riuscito", non un'attesa infinita
- [x] Le entità configurate ma inesistenti mostrano "non disponibile" e i
      loro comandi sono disattivati, senza bloccare il resto
- [x] Nessun polling: ci si sottoscrive a `state_changed` e si aspetta
- [x] Il riscontro di un comando arriva dal **cambio di stato**, non dalla
      risposta al comando: è la differenza fra "l'ho chiesto" e "è successo"
- [ ] Sul pannello vero: mDNS col nome configurato, e la casa collegata
      davvero (Fase 5)

#### Come sono verificati

**Un protocollo si prova parlandolo.** `tools/finto_ha.py` è un finto Home
Assistant — non un'emulazione, un attrezzo — e serve a far *succedere* le
situazioni che l'elenco qui sopra richiede e che con Home Assistant vero si
possono solo aspettare: un token che viene rifiutato, un collegamento che
cade a metà di un comando, un'entità che non esiste, comandi che tornano
`success: false`. Sei scenari in `tools/prova_ha.py`, con il WebSocket vero
in mezzo.

**Le tre soglie si provano dando i due numeri a mano.** Aspettarle
vorrebbe dire un minuto e un quarto per ogni caso, e nessuno le proverebbe
più di una volta: `--prova-sorveglianza` ne verifica quattordici in un
millisecondo, i confini compresi — 14,9 s ancora normale, 15,0 s già
fascia, 59 s fascia, 60 s coperta.

Tre difetti che solo il collegarsi davvero ha fatto emergere, e vale la
pena averli scritti:

1. **`auth_invalid` arriva subito prima della chiusura.** Controllando la
   chiusura per prima si vedeva solo il socket morto, si diceva "caduto" e
   si riprovava ogni quindici secondi — esattamente il ciclo che il primo
   criterio vieta. Ora si svuota il buffer prima di dichiarare la caduta.
2. **Un identificatore di Home Assistant ha un punto solo.** Senza quel
   controllo `it.pool.ntp.org`, che sta nella stessa configurazione, veniva
   seguito come entità e restava per sempre fra quelle "configurate ma
   inesistenti" — rendendo inutile il conto che serve proprio a dire se la
   configurazione parla della casa collegata.
3. **Un ciclo di scrittura senza uscita.** `manda_tutto()` riprovava su
   `EAGAIN` all'infinito, e su un socket non bloccante la cui `connect` non
   è finita `EAGAIN` è la risposta normale: il pannello si sarebbe piantato
   senza dire niente. Un ciclo senza uscita in un trasporto di rete non è
   un errore che si vede.

### Fase 4 — Telecamere *(fatta, tolta, rifatta, e chiusa con una risposta)*

Sette criteri, sei spuntati e uno che aspettava l'hardware: le immagini che
si aggiornano solo a pagina visibile, l'età dichiarata che corrisponde al
fotogramma, i tre fallimenti che portano a «non raggiungibile» senza
fermare le altre, la cache che evita il riquadro nero, i comandi di
apertura vivi con la telecamera giù, il 16:9 su ogni superficie.

Erano veri quando sono stati spuntati, e la fase ha lasciato due lezioni
che valgono ancora: **uno slot caduto non riprovava mai**, perché la
scorciatoia «è già aperto sulla stessa sorgente» non controllava che il
socket fosse vivo — una telecamera che sbagliava una volta restava giù fino
al riavvio; e **il decoder JPEG di LVGL, da un buffer in memoria, non legge
le dimensioni dal JPEG**, le copia dal descrittore, che quindi va riempito
— lasciandole a zero l'immagine non compare e non c'è nessun errore da
nessuna parte.

**Le telecamere sono uscite dal progetto l'08/09/2026** su richiesta di chi
lo usa, e **rientrate lo stesso giorno in un'altra forma**: RTSP diretto,
H.264 decodificato in software, quattro flussi invece di otto, niente
go2rtc, niente striscia in home, niente vista dentro Accessi. I criteri qui
sopra non si spuntano più — descrivono una catena che non esiste più — e
non si cancellano, perché chi rilegge deve sapere che sono stati
considerati e come è andata.

I criteri della catena in RTSP sono stati percorsi, e questo è come è
finita:

- [x] Il protocollo, per intero, contro `tools/finta_telecamera.py`: Digest
      nella forma corta e in quella lunga, telecamera aperta, credenziali
      respinte, Main profile, marker mancante. Otto scenari
- [x] MD5 sui vettori dell'RFC 1321, anche dato a pezzi
- [x] Un indirizzo con le credenziali dentro rifiutato dallo schema **e**
      dal validatore del firmware
- [x] La stretta di mano contro la Foscam vera arriva fino in fondo: RTSP
      negoziato, fotogrammi ricevuti, decodificatore alimentato
- [x] **Il profilo del flusso di casa: sopra baseline.** Detto due volte e
      da due parti indipendenti — l'SDP prima di chiedere il video, e il
      decodificatore che rifiuta l'SPS (`failed to activate param sets`)
- [x] **La cadenza raggiungibile: nessuna.** Non «bassa»: quei flussi non
      si aprono affatto. E anche se si aprissero, il conto in
      `05-architettura-firmware.md` §5 dice che otto riquadri costano il
      60% più di quanto questo chip possa fare con la CPU tutta per sé

La domanda per cui la sezione esisteva ha ricevuto risposta, ed è no. Le
telecamere sono uscite dal progetto il 09/09/2026, la seconda volta — «quando
esisteranno microcontrollori migliori ci riproveremo».

**Due lezioni che restano**, e non riguardano le telecamere:

- il TCP in chiaro sul pannello scambiava «adesso non c'è niente da
  leggere» per «l'altro ha chiuso», e nessuno se n'era accorto perché in
  chiaro non parlava nessuno. La correzione è in `canale_esp.c` e resta
- un componente di terze parti che scrive un errore per fotogramma
  **riempie il registro diagnostico in trenta secondi** e lo rende inutile
  proprio mentre serve. Vale per qualunque libreria si accolga in futuro:
  il registro è una risorsa condivisa, e chi ci scrive dentro a cadenza
  video va messo a tacere prima di accenderlo

### Fase 5 — Hardware e OTA

- [x] Un aggiornamento OTA valido si installa e si conferma da solo —
      `prova_web`: l'immagine passa a pezzi, si installa, e il pannello
      chiede il riavvio. La conferma dopo il riavvio è in `main.c`: rete,
      Home Assistant e schermo, tutti e tre
- [x] Un binario non firmato viene rifiutato prima della scrittura —
      `prova_web`: un file che non comincia come un firmware non installa
      niente. Sul pannello a fermarlo è la firma, nello stesso punto: alla
      chiusura, prima che la partizione di avvio cambi
- [x] Un firmware che non si avvia riporta alla versione precedente entro
      il tempo previsto dal profilo — **provato sul ferro il 30/08/2026 sul
      profilo `p4-800x1280`**, esito in fondo a questa voce. È l'unico
      pezzo dell'aggiornamento che il PC non può provare, perché a farlo è
      il bootloader. Si prova così:

      1. `idf.py -B build-collaudo -D SDKCONFIG=sdkconfig.collaudo -D
         PANNELLO_ROLLBACK_FINTO=1 build` — un'immagine identica a quella
         buona tranne che non si conferma mai. La compilazione lo dice a
         voce alta. La cartella a parte non è pignoleria: CMake si ricorda
         le `-D` nella cache della cartella, e in `build/` resterebbero
         anche nella compilazione «pulita» del giorno dopo.

         La configurazione di collaudo si fa da quella del pannello:

             cp sdkconfig.p4 sdkconfig.collaudo.p4
             idf.py -B build-collaudo-p4 \
                    -D SDKCONFIG=sdkconfig.collaudo.p4 \
                    -D PANNELLO_PROFILO_SCELTA=PRF_P4_800X1280 \
                    -D PANNELLO_ROLLBACK_FINTO=1 \
                    -D PROJECT_VER=v0.5-collaudo build

         `PROJECT_VER` non è un vezzo. Le due immagini nascono dallo stesso
         commit, quindi `/api/status` le chiamerebbe allo stesso modo, e una
         prova che non sa distinguere le due versioni non distingue «è
         tornata indietro» da «non è mai partita».
      2. Si installa dalla pagina come un aggiornamento qualsiasi, e si
         guarda la diagnostica: la testata deve dire **(in prova)**.
      3. Non si tocca niente. Allo scadere della grazia — 120 secondi —
         il pannello si riavvia da solo e la testata
         torna a mostrare la versione di prima, senza «(in prova)».
      4. Non serve rimediare: la versione buona è già quella che gira.
         `build/` non è mai stata toccata.

      Se dopo il riavvio si legge ancora la versione di collaudo, il
      rollback non ha funzionato: il bootloader ha accettato una versione
      che non si era mai dichiarata buona. È il guasto peggiore di tutti,
      perché non rompe niente oggi — toglie la rete di sicurezza a ogni
      aggiornamento futuro, e lo si scopre il giorno in cui serve.

      **Esito, 30/08/2026, pannello «Ingresso» (p4-800x1280).** Dal registro
      seriale, che è il solo posto dove parla il bootloader:

          19:35:30  ota: aggiornamento su ota_0, 2308 kB attesi
          19:35:47  ota: installata su ota_0
          19:35:47  boot: Loaded app from partition at offset 0x3a0000
          19:35:51  versione v0.5-collaudo in prova: ha 120 secondi
          19:37:51  versione in prova non convince: rete si, casa si
          19:37:51  esp_ota_ops: Rollback to previously worked partition.
          19:37:52  boot: Loaded app from partition at offset 0x20000

      Centoventi secondi esatti, `ota_0` → `factory`, nessun intervento. Di
      fuori si vedeva la stessa cosa: la revisione in `/api/status` passata
      da `d67a50c` a `e80ff2c` e tornata indietro, e il contatore dei
      riavvii da 161 a 163. Il pannello è rimasto senza interfaccia due
      minuti e venti; configurazione e credenziali intatte.

      **La riga che conta è «rete si, casa si».** Le condizioni per
      confermarsi erano tutte soddisfatte — l'immagine di collaudo era un
      firmware sano, con la radio su e Home Assistant collegato — e le
      mancava soltanto la volontà di dichiararsi buona. Questo separa due
      cose che una prova sciatta avrebbe confuso: non è stato verificato che
      un firmware rotto cada, ma che **la conferma sia l'unica cosa che
      tiene in vita una versione nuova**. Un'immagine che non si avvia
      sarebbe caduta per mille motivi diversi, e nessuno di quelli avrebbe
      detto niente sul meccanismo.

      Prima di fidarsi dell'esito sono state controllate tre cose, perché
      una prova riuscita per il motivo sbagliato è peggio di una fallita:
      che `-DPANNELLO_ROLLBACK_FINTO=1` fosse arrivato **al compilatore** e
      non solo a CMake (104 unità di traduzione nell'immagine di collaudo,
      zero in quella buona), che CMake avesse stampato il suo avviso, e che
      i due binari differissero davvero.
- [x] Configurazione e credenziali sopravvivono all'aggiornamento — sono in
      `nvs` e in `storage`, che l'OTA non tocca: scrive solo su `ota_0` /
      `ota_1`. Le partizioni sono separate proprio per questo
- [x] Un trasferimento interrotto non lascia niente di installato, e dopo si
      può riprovare — `prova_web`. È il caso normale di un Wi-Fi domestico,
      non un caso limite
- [ ] Un'interruzione di corrente non lascia il pannello in uno stato da
      cui non riparte

---

## 2. Casi limite

La parte che di solito manca. Ogni riga descrive la condizione e il
comportamento atteso.

### Quantità

| Condizione | Atteso |
|---|---|
| Zero luci accese | la home scrive "tutte spente", non "0 luci accese" |
| Zero finestre aperte | "tutto chiuso", non "0 finestre aperte" |
| Zero persone in casa | "nessuno in casa", non un elenco vuoto |
| Una sola zona in una griglia | la scheda mantiene la sua dimensione; i posti liberi restano vuoti |
| Zone oltre tre pagine | la paginazione regge e i pallini non escono dalla fascia |
| Nessuna sezione attiva salvo una | il rail mostra HOME, quella sezione e l'ingranaggio |

### Testo

| Condizione | Atteso |
|---|---|
| Nome di stanza da 40 caratteri | troncato con ellissi, la scheda non cambia dimensione |
| Nome con accenti e apostrofi | reso correttamente col font compilato |
| Titolo di evento lunghissimo | va a capo al massimo su due righe, poi tronca |
| Nome di rete Wi-Fi con spazi ed emoji | il QR resta valido; l'emoji, se il font non la copre, diventa un segnaposto e non un rettangolo vuoto |

### Valori

| Condizione | Atteso |
|---|---|
| Produzione a 0 di notte | "nessuna produzione", non "0,00 kW" ripetuto |
| Batteria fra −50 e +50 W | "ferma", senza alternare carica e scarica |
| Temperatura non disponibile | "—", e il comando `+`/`−` disattivato |
| Setpoint al limite del range | i tasti oltre il limite non rispondono, non avvolgono |
| Potenza di stringa con corrente a 0 | 0 W, non un errore di divisione |
| Orologio prima della sincronizzazione NTP | "--:--", mai una data del 1970 |

### Tempo e concorrenza

| Condizione | Atteso |
|---|---|
| HA cade **mentre** un comando è in volo | "non riuscito" dopo il timeout, con l'avviso persistente |
| Doppio tocco rapido su un pulsante di apertura | un solo impulso inviato |
| Pressione prolungata rilasciata a metà | nessun comando, anello di avanzamento che rientra |
| Configurazione salvata mentre la sua schermata è aperta | la schermata si ricostruisce senza chiudersi |
| Timer del condizionatore che scade con l'automazione disattivata | conto alla rovescia attenuato e dicitura esplicita |
| Cambio di durata mentre il timer corre | il timer riparte con il nuovo valore |
| Mezzanotte durante lo standby | data aggiornata senza uscire dallo standby |
| Ora legale che cambia | l'orario si aggiorna, gli eventi dell'agenda non slittano |

### Rete

| Condizione | Atteso |
|---|---|
| Wi-Fi presente ma HA spento | schermata di irraggiungibilità, orologio funzionante |
| Indirizzo IP cambiato dal DHCP | riconnessione automatica, nessun intervento |
| Rete satura, risposte oltre 8 s | comandi in "non riuscito", non blocco dell'interfaccia |

---

## Rimandato: il blocco di memoria di LVGL

Sessantaquattro kilobyte fissi in RAM interna, ed è di gran lunga l'oggetto
più grosso lì dentro: tolti la tabella delle entità e il buffer HTTP, è
rimasto il solo. Non si sa quanto ne serva davvero.

**Va misurato prima di toccarlo.** Il comando `stato` sulla seriale riporta
già il pezzo libero più grande del blocco. Si naviga tutte le sezioni sul
pannello vero — la diagnostica per ultima, che è quella che costruisce più
oggetti — e si guarda quanto scende. Se il minimo resta ampio, stringere il
blocco libera memoria interna a costo zero; se scende vicino allo zero, il
numero è già quello giusto e va lasciato dov'è.

Non è pignoleria: quel blocco è anche la rete che ha impedito un guasto
vero. Con troppi oggetti a schermo `lv_obj_create()` tornava NULL, nessuno
lo controllava, e il pannello si riavviava entrando in Impostazioni →
Sistema. Ridurlo senza misurare rimetterebbe quel guasto in gioco.

Spostarlo in PSRAM libererebbe tutti e sessantaquattro i kilobyte, ma LVGL
tocca quella memoria a ogni disegno, e la PSRAM è più lenta della RAM
interna: i fotogrammi al secondo possono calare. Da considerare solo se dopo la misura
servono ancora kilobyte.

---

## 3. Prove che si fanno solo dal vivo

Non anticipabili nel simulatore. Da fare col pannello montato, nella sua
posizione definitiva.

- **Leggibilità** a un metro e mezzo, in piedi: l'orologio di standby, i
  valori della home, i nomi nel dock. Se qualcosa non si legge, si corregge
  il corpo nel profilo, non il layout.
- **Precisione del tocco** ai quattro angoli e lungo i bordi: è dove i
  pannelli capacitivi economici sbagliano di più.
- **Tocco con le dita bagnate o unte** — realistico all'ingresso e in
  cucina.
- **Luminosità di notte** in una stanza buia: il minimo dello standby non
  deve illuminare il corridoio.
- **Riflessi** alla luce del giorno nella posizione reale.
- **Calore** dell'incasso dopo quattro ore accese: se la cornice scotta,
  serve ventilazione.
- **Ritenzione d'immagine** dopo una notte in standby: al risveglio,
  ombra visibile? Se sì, si accorcia il periodo di spostamento dei pixel.
- **Prova dell'ospite**: qualcuno che non ha mai visto il pannello deve
  riuscire ad aprire il cancello e a collegarsi al Wi-Fi senza
  spiegazioni. È la prova più severa e la più utile.
- **Rumore percepito**: nessuno, ma verifica che l'alimentatore non
  fischi di notte.

---

## 4. Come usarlo

Non serve spuntare tutto in un colpo. Il criterio è: **una fase non si
considera chiusa finché i suoi criteri non passano**, e i casi limite si
provano prima di passare alla fase successiva, non alla fine.

Quando un caso limite fallisce, aggiungilo come riga in questo documento
se non c'era: è il modo in cui l'elenco diventa migliore col tempo.
