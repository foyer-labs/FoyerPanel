# Foyer Panel — Specifica dell'interfaccia

Vale per **entrambi i pannelli**. Descrive struttura, gerarchia e
comportamenti; **non contiene misure che dipendono dal pannello**: quelle
stanno tutte in `09-profili.md`, che è l'unica fonte.

Dove serve una misura, il testo **nomina il corpo tipografico o rimanda
alla tabella** di `09-profili.md`, non scrive un numero. La revisione del
2026-08-22 ha tolto una ventina di misure in pixel che erano finite qui:
erano quasi tutte i valori del profilo `p4-1280x800`, dichiarati per
sbaglio come comuni, e su un pannello di misura diversa erano
semplicemente sbagliate.


---

## 1. Impianto di navigazione

**Variante "C ibrida"**: home a tutta larghezza, navigazione compatta nelle
sezioni. La **forma** della navigazione dipende dall'orientamento
(`09-profili.md` §1-ter).

### In orizzontale — rail verticale a sinistra

- **HOME** in cima, sfondo card, icona e testo ambra (misure: §3)
- **le voci delle sezioni attive**: Luci, Interruttori, Clima, Energia,
  Accessi, Agenda, Wi-Fi, Robot
  — larghezza piena del rail, altezza distribuita sul numero di sezioni
  attive
- **Impostazioni** in fondo, separata da un bordo superiore
- la home ha in basso un **dock** con un pulsante per sezione attiva

### In verticale — barra orizzontale in basso

- **HOME** come prima voce a sinistra, in ambra, sempre presente
- le sezioni attive occupano il resto della larghezza in parti uguali
- **Impostazioni** si sposta nella testata della home, come icona a destra
- **la home non ha il dock**: la barra lo sostituisce, e lo spazio
  recuperato torna al contenuto

In entrambi i casi HOME è il **ritorno unico**: nessun tasto "indietro"
gerarchico, e la voce corrente è evidenziata.

### Testata delle sezioni
Titolo in `f_l`, sottotitolo in `f_s`, orologio a destra. Altezza e
padding: §3. Nelle schermate paginate l'orologio è sostituito dal
paginatore (misure dei tasti: §8).

## 2. Home

```
ORIZZONTALE
testata   ─ orologio + data | meteo
contenuto ─ [Presenza] [Energia] [Aperture e luci] [Agenda]
          ─ dock: un pulsante per sezione attiva

VERTICALE
testata   ─ orologio + data | temperatura interna | meteo | ingranaggio
contenuto ─ Energia, a tutta larghezza
          ─ [Presenza] [Aperture e luci]
          ─ Agenda del giorno
          ─ riga del robot             (se configurato e disponibile)
          ─ lavatrice e asciugatrice   (se configurate e disponibili)
          ─ striscia comandi rapidi
          (nessun dock: c'è la barra in basso)
```

Altezze e larghezze delle schede: `09-profili.md` §6.

### Scheda Presenza
Etichetta "IN CASA" in `f_xs`. Una riga per persona: pastiglia tonda con
l'iniziale (ambra se in casa), nome in `f_m`, stato in `f_s`.

### Scheda Energia
Valore grande nel corpo fuori scala "energia in home" (§4), in verde, con
"kW dal sole". Tre righe: Casa, In rete,
Oggi. In fondo la batteria: percentuale, barra a tutta larghezza,
sottotitolo con la potenza di carica.

### Scheda Aperture e luci
Numero grande in ambra, corpo `f_xxl`, elenco stanze con da quanto tempo,
tre righe in
fondo: luci accese, cancelli, allarme.

### Scheda Agenda
Fino a 3 eventi: ora in ambra in colonna a larghezza fissa, titolo in
`f_m`, provenienza in `f_s`. Eventi passati attenuati. In fondo il
conteggio di domani.

### Temperatura della stanza — in testata
Accanto all'orologio, prima del meteo: il numero grande in `f_xl`, con la
fiamma **solo mentre l'impianto scalda** — un'icona sempre accesa
smetterebbe di voler dire qualcosa. Sotto, in `f_xs`, i gradi chiesti quando
c'è un'entità `climate`, altrimenti il nome della stanza del termometro:
su un pannello in corridoio *di cosa* sia quella temperatura non è scontato.

Il sensore è **lo stesso dello standby**, letto da `dati_standby()`. Un
secondo posto dove dire qual è il termometro di casa potrebbe dire due cose
diverse sullo stesso vetro.

Elemento a sé e non dentro il riquadro del meteo: lì dentro si spezzava a
metà, perché quel riquadro è dimensionato per un numero solo.

Disattivabile con `home.indoor_temperature`, accesa di default.

### Riga del robot
Una riga sola, sotto le schede: icona, **il nome che gli avete dato**
(`robot.name`, "Robot" se vuoto), lo stato in `f_xs` sotto il nome, e la
percentuale di batteria a destra — ambra sotto il 20%, verde altrimenti,
accento mentre è in carica. Toccandola si apre la sezione Robot.

Sparisce quando il robot non è configurato o non risponde: una riga che
dicesse "stato ignoto" occuperebbe lo spazio di una che non serve.
Disattivabile con `home.robot`.

### Lavatrice e asciugatrice
Una scheda sola divisa in due metà, non due righe: sono una coppia e si
leggono come una coppia. Chi lavora sta a piena luce, chi è fermo è
attenuato — e l'attenuazione fa il lavoro che due righe di testo farebbero
in più spazio.

Costa **una riga sola tutto il giorno**, ed è il punto: questi due
apparecchi sono fermi quasi sempre, e una forma che ne costasse due sarebbe
pagata ventiquattro ore per essere utile venti minuti.

Per ciascuno: icona, nome, stato («ciclo in corso» / «ferma» / «non
risponde»), assorbimento a destra, e una barretta sotto il nome **solo
mentre lavora**.

- **Lo stato viene dall'entità del ciclo, non dai watt.** Una lavatrice in
  ammollo assorbe quanto una spenta, e dedurre il ciclo dall'assorbimento
  vorrebbe dire annunciare «finita» a metà lavaggio — il genere di errore
  che fa aprire l'oblò con dentro l'acqua.
- **La barretta solo con il pieno carico dichiarato**
  (`appliances[].max_power`). Senza quel numero non si sa
  rispetto a cosa riempirla, e una barra che finge di misurare è peggio di
  nessuna barra.
- **L'icona la decide la posizione nell'elenco**, non il nome: indovinare
  «asciugatrice» da una stringa che chi configura scrive come vuole
  sbaglierebbe il giorno che qualcuno scrive «Asciug.».
- Le due metà sono allineate **in cima** e non al centro: quella che lavora
  è più alta di una barretta, e centrandole i due nomi finivano a quote
  diverse.
- Sparisce se non sono configurate o se nessuna delle due risponde.
  Disattivabile con `home.appliances`.

### Striscia comandi rapidi
Tre pulsanti di apertura più un interruttore, tutti della stessa altezza.
La porta del garage porta scritto il suo stato, che è la sola cosa che si vuole
sapere prima di toccarla.

### Dock
Un pulsante per ogni sezione attiva: icona, nome in maiuscoletto con
spaziatura 0.08em, stato in `f_s`.
I pulsanti hanno `min-width: 0` e testo non a capo: se un'etichetta cresce,
il pulsante si stringe invece di spingere gli altri fuori schermo.

---

## 3. Sezioni

### 3.1 Luci
- Fascia scene in alto, fino a 5 riquadri (§5). **Se non ci sono
  scene configurate la fascia non viene mostrata.**
- Griglia di zone; colonne, righe e capienza di pagina: §5. Ogni scheda è
  **una riga**: lampadina in `xl` a sinistra — è insieme lo stato e il
  bersaglio — nome in `f_l`, e in fondo la percentuale in `f_xl` per le
  dimmerabili o la parola «accesa» / «spenta». Sotto, la barra di
  luminosità per le sole dimmerabili.
- Si tocca **tutta la scheda**, non un interruttore dentro di essa: un dito
  su un vetro verticale non ha la mira di un cursore.
- Il nome va a capo una volta e poi tronca con l'ellissi: senza quel tetto
  un nome lungo spinge la barra fuori dalla scheda, e in una griglia di
  schede uguali quella che sborda è l'unica che si nota.
- Zone spente in grigio; zone non dimmerabili senza barra.
- **Oltre la capienza di pagina**: paginazione con frecce in testata e
  pallini in basso. Nell'ultima pagina i posti liberi restano **vuoti e
  tratteggiati**: le schede non si allargano, così la posizione di ogni zona
  non cambia mai fra una pagina e l'altra.

> **Perché una riga e non una scheda alta.** La scheda era una colonna —
> nome in cima, lampadina al centro, stato in fondo — e la lampadina stava
> in un contenitore che si prendeva tutto lo spazio avanzato: su schede alte
> un quarto di schermo, avanzava quasi tutto. Tredici zone volevano due
> pagine per mostrare tredici parole e tredici lampadine. Non si è tolto
> niente di quello che si leggeva, e la lampadina è rimasta grande uguale:
> è cambiato che i tre pezzi stanno affiancati invece che impilati.

### 3.2 Clima

L'impianto è fatto di **due cose diverse** e la sezione le tiene separate:
12 zone di riscaldamento, che hanno solo il setpoint, e 4 condizionatori
Gree, che hanno modalità, ventilazione, deflettore e timer.

**Fascia superiore** : due schede di gruppo
— "Riscaldamento" con il conteggio delle zone e "Condizionatori" con il
numero di unità — più il riquadro della temperatura esterna a destra.
Non contiene modalità globali: nell'impianto non esistono.

**Limiti di temperatura, distinti per gruppo**: riscaldamento 15–26° con
passo 0,5°; condizionatori 16–30° con passo 1,0°, come accettano le unità
Gree. Sono due impianti diversi e non condividono la scala.

#### Gruppo Riscaldamento
Griglia di zone, paginata; capienza: §5. La scheda sta in **due righe**:
nome e umidità sopra; sotto, la temperatura misurata in `f_xl` a sinistra e
i comandi del setpoint a destra — `−`, valore richiesto, `+`. **Bordo e
valore ambra** quando la zona chiama davvero calore; etichetta piccola
"richiesta" / "raggiunta" / "nessuna richiesta".

> **Perché due righe.** Ne occupava tre, alte più del doppio, e la terza
> riga di contenuto non esisteva: erano tre blocchi distribuiti sull'altezza
> della cella, cioè due terzi di vuoto. Su un pannello verticale costava una
> pagina in più — sei zone per volta invece di dodici — e con le zone divise
> per piano voleva dire che per sapere se il piano di sotto stava scaldando
> bisognava toccare il vetro. Un dato che si guarda di sfuggita e che chiede
> un tocco per comparire è un dato che non si guarda.

**La pagina si conta in righe, non in zone.** Con le fasce dei piani, quanto
è alta una pagina dipende da come i piani si dividono le zone: sei zone in
due colonne occupano tre righe se stanno in un gruppo solo e cinque se i
gruppi sono due. Il profilo dice quante righe stanno in una pagina (§5), una
fascia ne costa una, e un gruppo più alto della pagina si spezza — con la
sua fascia ripetuta in cima alla pagina dopo, che è l'unico modo perché
quelle zone dicano di che piano sono.

#### Gruppo Condizionatori
Non una griglia ma **quattro colonne alte**, una per unità. Ogni colonna:

- nome e pastiglia della modalità corrente, con icona
- temperatura in stanza in grande
- setpoint con `−` e `+`
- tre righe di stato: ventilazione, deflettore, spegni al timer
- pulsante "Tutti i comandi" che apre il dettaglio

**Il colore segue la modalità**: azzurro `--cool` in raffrescamento, ambra
in riscaldamento, scheda attenuata al 68% da spenta.

#### Dettaglio del condizionatore
Vista a tutta pagina, con "← Zone" nella testata al posto del tasto
indietro generico.

*Colonna sinistra*, larghezza fissa:
- temperatura in stanza, cifra grande (§4), colore della modalità
- setpoint con tasti grandi (§8) e valore in evidenza
- riquadro timer: interruttore, durata regolabile, ora di spegnimento.

  «Regolabile» vuol dire **il timer che sta correndo**: `−` e `+` lo
  allungano o lo accorciano di un quarto d'ora con `timer.change`, che in
  Home Assistant si applica a un timer attivo e su uno fermo dà errore. A
  timer fermo i due tasti restano spenti e il numero grande mostra la
  durata di `default_duration`, che è quella con cui partirà; mentre
  corre il numero mostra quanto manca, perché è quello che i tasti
  muovono — vedere la durata impostata restare ferma sotto le dita
  farebbe sembrare il pannello bloccato

*Colonna destra*, quattro blocchi:
- **Modalità**, 6 riquadri: Spento, Caldo, Freddo, Auto, Deumidifica,
  Solo ventilatore (`off, heat, cool, auto, dry, fan_only`). Nei riquadri
  le due etichette lunghe sono abbreviate — **"Deum."** e **"Vent."** —
  perché sei riquadri in fila non lasciano spazio al nome intero e la
  seconda riga finirebbe fuori. Solo lì: nella pastiglia della modalità
  corrente, dove lo spazio c'è, il nome resta per esteso
- **Ventilazione**, 6 pulsanti con indicatore a barrette: Auto, Bassa,
  Medio-bassa, Media, Medio-alta, Alta
  (`auto, low, medium low, medium, medium high, high`)
- **Deflettore**: 5 posizioni con il disegno della lamella alla sua
  altezza reale (Alto, Medio-alto, Centro, Medio-basso, Basso), più 4
  tasti sotto: Oscillante, Fisso, Tutta l'escursione, Predefinito
- **Extra**, 4 interruttori Gree: Silenzioso, Luce pannello, Aria fresca,
  XFan

*In verticale le due colonne diventano **due pagine***, con le frecce in
testata e i pallini in fondo come in ogni altra schermata paginata.
Affiancarle su 800 px renderebbe i tasti più piccoli del minimo di
tocco; impilarle chiede più di duecento pixel oltre il vetro, e
oscillazione ed extra finirebbero sotto la barra in basso. La prima pagina
è **quello che serve quasi sempre** — temperatura in stanza, setpoint,
**modalità** e timer — la seconda **quello che si regola di rado**:
ventilazione, deflettore, extra. La modalità sta nella prima perché
accendere e spegnere il clima è il gesto più frequente, e non deve
chiedere un cambio di pagina; la temperatura in stanza prende lo spazio
che avanza. In verticale modalità e ventilazione vanno su due righe da
tre, e testo e icone dei riquadri salgono di un gradino: le pagine hanno
l'altezza intera e meno comandi della colonna orizzontale. Nelle due
pagine la testata perde il sottotitolo, che non ci sta accanto al
paginatore e a "← Zone": è l'unica cosa che si legge anche altrove, perché
modalità, richiesta e ventilazione sono scritte in chiaro lì sotto.

Le 12 combinazioni reali di `swing_modes` si ottengono così: 5 posizioni
× (fisso | oscillante) + `default` + `full_swing`. Mostrare dodici tasti
sarebbe illeggibile; mostrare quattro opzioni, come nella prima versione
del disegno, sarebbe stato sbagliato.

Non esiste oscillazione orizzontale: queste unità muovono solo la lamella
verticale.

#### Timer di spegnimento — comportamento

Il timer (`timer.condizionatore_*`) e l'automazione che spegne l'unità
(`automation.spegni_condizionatre_*_allo_scadere_del_timer`) sono **due
entità indipendenti**. L'interruttore "Spegni al timer" agisce
sull'automazione con `automation.turn_on` / `automation.turn_off`.

Da qui cinque regole:

1. **Se il timer corre ma l'automazione è disattivata**, il conto alla
   rovescia si mostra attenuato con la dicitura "allo scadere non
   succederà nulla". Vedere i minuti scorrere e credere che il
   condizionatore si spegnerà sarebbe un inganno.
2. **L'ora di spegnimento viene da `finishes_at`**, che è l'unico attributo
   del timer che dica qualcosa di vero mentre corre. Si mostra in ora
   locale, e resta giusta anche se il timer è stato avviato altrove.
3. **Quanto manca il pannello se lo calcola**: `finishes_at` meno adesso,
   arrotondato per eccesso al minuto. Non si legge da `remaining`, che Home
   Assistant tiene **fermo** al valore d'inizio finché il timer corre — e
   un conto alla rovescia che non scende è peggio di nessun conto alla
   rovescia. Per lo stesso motivo la schermata si rinfresca **al cambio di
   minuto** e non solo quando arriva un evento: mentre un timer corre, Home
   Assistant non manda niente.
4. **I due pulsanti +/− fanno due mestieri.** A timer che corre allungano o
   accorciano quello che sta correndo (`timer.change`, un quarto d'ora per
   pressione). A timer fermo scelgono per quanto avviarlo — da un quarto
   d'ora a dodici ore — e il pulsante sotto lo avvia (`timer.start` con
   quella durata, sempre esplicita: quella scritta in Home Assistant è la
   sua e non c'entra con quella scelta sul vetro). Mentre corre, quello
   stesso pulsante lo annulla con `timer.cancel` e **non** con
   `timer.finish`: finire vuol dire scatenare l'evento di scadenza, e
   l'automazione spegnerebbe il condizionatore. Annullare un timer non deve
   spegnere niente.

5. **Nella scheda del gruppo, sotto la riga del timer, c'è una barra.**
   Cala da piena a vuota e dice la stessa cosa del numero accanto — quanto
   manca — ma in un colpo d'occhio, che da un metro e mezzo arriva prima di
   due cifre. Sotto, l'ora di fine: «fino alle 20:12».

   Il denominatore è l'attributo **`duration` del timer**, mai
   `default_duration` della configurazione: quella è solo ciò che il
   pannello userebbe avviandolo lui, e un timer fatto partire dall'app con
   tre ore, diviso per un'ora e mezza, si riempirebbe oltre il fondo. Senza
   `duration` **la barra non si disegna** e resta il numero — una barra che
   non sa rispetto a cosa riempirsi mentirebbe con l'aria di misurare, ed è
   la stessa regola dell'assorbimento di lavatrice e asciugatrice. Oltre il
   cento per cento — succede con `timer.change`, che allunga il conteggio
   senza toccare `duration` — si ferma al fondo invece di sfondarlo.

   La barra prende il **colore della modalità**, non l'accento fisso:
   appartiene a quella scheda, e una striscia ambra dentro una scheda
   azzurra sembrerebbe arrivare da un'altra parte. Con l'automazione
   disattivata barra e ora di fine si mostrano attenuate, come già dice a
   parole la riga sopra.

L'interruttore compare solo se la configurazione ha **entrambi** i campi
`timer` e `timer_automation` per quell'unità.

> **Un attributo che non esiste non dà errore.** Il conto alla rovescia ha
> letto per un mese un attributo `remaining_s` che Home Assistant non ha mai
> avuto: nessun errore, nessun avviso, il valore di ripiego — zero — e la
> riga "mancano 0h00" su un timer che stava correndo. Gli attributi veri di
> un timer sono `duration`, `remaining` e `finishes_at`, e il finto server
> di `tools/finto_ha.py` adesso serve esattamente quelli.

#### Nota sugli identificatori
Gli entity_id delle automazioni contengono refusi
(`spegni_condizionatre_`, `..._camera_matirmoniale`). Vanno usati
**letteralmente**: non correggerli nel codice.

### 3.2-ter Il riscaldamento a piani

Le valvole delle singole zone aprono il circuito; **l'accensione vera la
comanda un interruttore per piano**. Sono due cose diverse, e il pannello le
tiene separate perché sul vetro si vedono separate: una zona può chiedere
calore quanto vuole, se l'interruttore del suo piano è spento non scalda
niente.

Un pannello **governa un piano**, quello scritto in
`climate.panel_floor`: sta al posto del termostato di quel piano e dice lo
stato di ciò che comanda. Il pannello del box avrà lo stesso firmware e una
riga di configurazione diversa.

**Nella sezione Riscaldamento**, ogni piano ha una fascia con nome, stato e
il proprio interruttore, e sotto le sue zone:

1. Le zone si raggruppano **per piano**, e i piani vengono nell'ordine in cui
   stanno in configurazione; dentro un piano, le zone restano nell'ordine del
   documento. Il primo tentativo raggruppava le zone *consecutive* dello
   stesso piano per non toccare l'ordine, e sul vetro era peggio: in una
   configurazione vera le zone dei due piani si alternano, e la pagina
   mostrava quattro fasce con «Piano 1» due volte.
2. **Le zone di un piano spento si attenuano**, e sotto il setpoint c'è
   scritto «piano spento» al posto di «raggiunta». Il motivo del piano viene
   prima di quello della zona: sono tutti e due veri, ma il primo spiega il
   secondo e vale per tutte le zone insieme.
3. **La contraddizione si dice.** Piano spento e zone che chiedono calore: la
   fascia lo scrive. È l'unica cosa, in tutto il riscaldamento, che prima non
   si poteva sapere da nessuna parte del pannello — si scopriva toccando i
   termosifoni.
4. Una zona che sta chiedendo calore è **ambra**: bordo, valori e un fondo
   appena tinto. Era il solo bordo, e da un metro e mezzo un bordo di un
   pixel non si vede. Il blu resta il raffrescamento e non si tocca.
5. `unavailable` non è «spento»: un interruttore che non risponde lo dice, e
   non si può premere.

**Nello standby e nella testata della home** compare `heat` — onde di calore
— quando l'interruttore del proprio piano è acceso. Nello standby è al corpo
`IC_XL`, accanto a una temperatura che è più grande: si deve vedere dalla
porta.

> **Perché non una fiamma.** Sullo stesso vetro c'è già
> `local_fire_department`, e vuol dire un'altra cosa: «questa zona sta
> chiedendo calore adesso», da `hvac_action`. Due fiamme quasi uguali a venti
> pixel di distanza renderebbero illeggibili tutti e due i significati. Onde
> contro fiamma si distinguono con la coda dell'occhio.
>
> Nella testata della home l'icona **c'era già** e voleva dire la seconda
> cosa. Adesso dice la prima, che è quella che serve a un pannello che sta al
> posto di un termostato; chi vuole sapere se una stanza sta chiedendo adesso
> lo legge nella sezione Clima, dove le zone che chiamano sono in ambra.

### 3.2-bis Interruttori

Quello che si accende e si spegne e basta, e che non appartiene a nessun
altro elenco: prese, luci smart, scaldini, pompe. Le luci di casa hanno una
sezione loro perché hanno luminosità e scene; qui c'è ciò che ha due stati.

**Un elenco di righe, non una griglia di piastrelle.** Una riga è alta 76 px
e ne stanno **dodici** in una pagina; le stesse dodici piastrelle sarebbero
due pagine e mezzo. Un interruttore ha due stati e nient'altro: dargli
l'area di una scheda delle Luci — che ha anche la percentuale — vorrebbe
dire pagare in paginazione uno spazio che non serve a dire niente.

Ogni riga: icona a sinistra, nome e stato a parole, **l'assorbimento** e
infine l'interruttore.

1. **L'interruttore dice cosa succede al tocco prima che lo tocchi.** Una
   piastrella va capita — si preme? apre un dettaglio? — e questo no. È lo
   stesso oggetto del dettaglio del clima e delle impostazioni.
2. **Il bersaglio però è tutta la riga**, non il solo interruttore: 76 px
   contro 34. L'interruttore non prende il tocco per sé, altrimenti premere
   proprio sopra di lui non farebbe niente — che è il punto in cui tutti
   premono.
3. **Il comando lo decide il dominio dell'entità**, non la configurazione:
   `switch.` vuole `switch.turn_on`, `light.` il suo, `input_boolean.` il
   suo. Chiedere anche il servizio a chi configura vorrebbe dire poterlo
   sbagliare, e uno `switch.` comandato con `light.turn_on` non dà errore
   sul vetro: dà un interruttore che si muove e non succede niente.
4. **L'assorbimento è facoltativo e non lascia il posto vuoto.** Quando c'è,
   il numero sta a sinistra dell'interruttore; quando non c'è, il nome
   cresce e basta. Un incolonnamento che tiene la colonna anche da vuota fa
   sembrare rotto ciò che è soltanto assente. La differenza fra «non ha un
   sensore» e «assorbe zero watt» si conserva: la prima non si scrive, la
   seconda si scrive «0 W».
5. **Un interruttore che non risponde non si finge spento.** `unavailable` e
   `unknown` sono la stessa cosa per il pannello — non si può comandare e
   non si può mostrare — e la riga lo dice invece di disegnare uno stato che
   non conosce.

**L'icona si sceglie, da un elenco chiuso.** I caratteri-icona sono bitmap
compilati alla costruzione del firmware: un nome fuori elenco non darebbe un
errore, darebbe un **rettangolo vuoto** sul vetro, che dal muro si scambia
per un difetto del disegno. L'elenco vive in tre posti che devono coincidere
— il generatore delle icone, lo schema, il validatore — e
`tools/verifica_icone_scelta.py` lo pretende a ogni `ctest`.

### 3.3 Energia
Cinque blocchi impilati, e l'ordine e il messaggio: **in alto la potenza
di adesso, in fondo l'energia della giornata**. Sono due domande diverse e
stanno in due posti diversi, cosi nessun numero significa due cose a seconda
di dove lo si guarda.

1. **I quattro valori istantanei** — dal sole, in casa, in rete, batteria.
   Valore in `f_l`: a `f_xl` la «W» di «3,42 kW» finiva tagliata dal bordo,
   perche quattro riquadri su ottocento pixel ne lasciano centottanta
   ciascuno. Era sbagliato da sempre e si e visto solo guardando una cattura.
2. **La banda dell'impianto** — la potenza a sinistra, la curva della
   giornata accanto, ad altezza dichiarata (`energia.banda_h`).
3. **I quattro riquadri delle stringhe** — tensione e corrente separate, due
   per stringa, in una griglia a due colonne. E la stessa lettura della
   scheda che questa casa gia guarda su Home Assistant: un pannello che
   chiedesse di impararne una nuova per gli stessi dati chiederebbe troppo.
4. **Chi sta consumando** — i dispositivi di `energy.devices[]`,
   ordinati **dal piu affamato al meno**, con una barra proporzionale al
   primo. E l'unico blocco che **scorre**: dodici dispositivi non stanno in
   nessuna altezza ragionevole, e scorrere batte le frecce di paginazione
   davanti a un pannello a muro — e lo stesso gesto con cui si scorre
   qualunque altra cosa, mentre delle frecce vanno trovate, capite e
   centrate col dito.
5. **I quattro totali della giornata** — prodotto, consumato, prelevato,
   immesso, da `energy.today`. Quelli che mancano non si disegnano.

**In orizzontale il terzo e il quarto blocco stanno affiancati**, a metà
larghezza ciascuno. Impilati non ci stavano: in ottocento pixel le stringhe
tengono la loro altezza dichiarata, e il blocco che doveva scorrere era
quello che restava schiacciato — una striscia dove non entrava nemmeno il
titolo. Accanto alle stringhe prende tutta la loro altezza.

**Il colore delle barre dice la posizione, non il valore**: il primo e
caldo, il secondo ambra, gli altri attenuati. Ottocento watt in una casa che
ne consuma mille sono un altro discorso rispetto a ottocento su diecimila, e
una scala assoluta non lo direbbe. Dal terzo in giu la barra e **attenuata e
non bianca**: bianca pesa piu dell'ambra che le sta sopra, e il colore
finirebbe per contraddire l'ordine.

**La percentuale e sul totale dei monitorati, non sul consumo di casa.** La
somma delle prese non fa il consumo della casa — mancano le luci, le prese
non monitorate, tutto il resto — e dire «43% della casa» sarebbe un numero
sbagliato con l'aria di essere giusto.

> **La curva della giornata e ancora finta.** `dati_storico()` legge un
> array costante nel codice: la forma e quella di una giornata di sole con
> le nuvole del pomeriggio, non quella di oggi. Chi la guarda sul vetro non
> ha modo di accorgersene, e per questo sta scritto qui.

#### Segni e diciture — regola obbligatoria

L'impianto usa questa convenzione, e il pannello **non deve mai mostrare
un numero negativo all'utente**: mostra il valore assoluto e cambia la
parola.

| Grandezza | Segno | Dicitura a schermo |
|---|---|---|
| Rete | positivo | "dalla rete 1,20 kW" |
| Rete | negativo | "in rete 2,20 kW" |
| Batteria | positivo | "in carica +0,90 kW" |
| Batteria | negativo | "in scarica 0,50 kW" |
| Batteria | ~zero | "ferma" |

La soglia sotto la quale un valore è considerato nullo è **50 W**: sotto,
si scrive "ferma" e non si alterna fra carica e scarica a ogni
aggiornamento.

Il consumo di casa non è un sensore: si calcola come
`produzione + rete − batteria`, con i segni della tabella.

#### Potenza per stringa — calcolata

L'inverter espone **tensione e corrente per stringa, ma non la potenza**.
Il pannello la calcola come `V × I` e la presenta come stimata. La somma
delle due stringhe sarà leggermente superiore alla produzione dichiarata
dall'inverter, perché quella è in corrente alternata e queste sono in
continua, prima del rendimento di conversione: **non è un errore e non va
"corretto" facendo quadrare i numeri.**

Se in configurazione il campo `power` di una stringa è valorizzato, si
usa quello e non si calcola nulla.

> **Qui c'erano le telecamere, due volte.** Il §3.3-bis era la regola
> delle superfici video 16:9, il §3.4 la sezione: prima otto Foscam servite
> da go2rtc in MJPEG (fino all'08/09/2026), poi quattro flussi RTSP presi
> diretti (per una giornata, il 09/09/2026).
>
> La seconda volta se ne è andata con una **risposta**, e la risposta sta
> in `05-architettura-firmware.md` §5 con i numeri: questo silicio l'H.264
> lo decodifica in software, otto riquadri costerebbero il 60% più di
> quanto il chip possa fare anche con la CPU tutta per sé, e i flussi di
> questa casa sono comunque sopra il profilo baseline — l'unico che quel
> decodificatore accetti.
>
> I numeri di paragrafo restano vuoti. Quando esisteranno microcontrollori
> con un decodificatore H.264 in hardware, la sezione si riscriverà — e
> ripartirà da quel §5, non da qui.

### 3.5 Accessi
- Una colonna sola, a larghezza piena: una riga per accesso.
- Pulsanti: misure in §8. Con `confirm: true` compare "TIENI PREMUTO";
  la pressione dura 1500 ms con anello di avanzamento.
- **Riga "Luci giardino"**, separata dalle tre di apertura da uno spazio
  maggiore: non è un impulso ma un interruttore di stato, quindi mostra un
  interruttore e non un pulsante "Apri". Il pannello conosce davvero questo
  stato e può dichiararlo.
- **La porta del garage ha un sensore di stato reale**
  (`binary_sensor.porta_garage_aperta`, device class `garage_door`):
  la sua riga mostra "aperta" o "chiusa" per davvero. È l'unico accesso per
  cui questo è possibile.
- **Gli altri accessi sono `switch`**: impulso senza riscontro. Nessuna
  riga deve dichiarare "chiuso" senza un sensore che lo affermi.

### 3.10 Programmazioni

Le cose che vanno a orario: campanello, stufetta, luci del giardino,
irrigazione, albero di Natale, luce del corridoio. **Rispecchia la logica che c'è
già in Home Assistant** e non ne inventa una seconda: là una scheda tiene
insieme il dispositivo, gli `input_datetime` degli orari e le automazioni
che li fanno scattare, e qui un gruppo tiene insieme le stesse cose.

**Un elenco solo che scorre**, con i gruppi che si aprono sul posto. È
l'unica sezione paginata a scorrimento invece che a pagine: un elenco che si
apre e si chiude, impaginato, farebbe saltare le pagine sotto il dito.

Ogni gruppo, **da chiuso**:

- icona, nome, e sotto lo stato — «acceso», «spento · 148 W», «non risponde»
- l'interruttore del dispositivo, che è un bersaglio suo
- **le finestre**, una riga ciascuna: `08:00 → 22:30`, con la durata a
  destra. Si vedono da chiuso perché sono la ragione per cui uno entra qui:
  un'intestazione col solo nome costringerebbe ad aprire tutti i gruppi per
  sapere a che ora parte la pompa.
- la finestra **in corso adesso** è in ambra, agli orari e al bordo

**Aperto**, sotto le finestre compaiono le automazioni: nome, quando è
scattata l'ultima volta, e un interruttore che la abilita. Una disattivata
si attenua **e lo dice a parole**: un interruttore grigio, da un metro e
mezzo, si distingue male da uno acceso.

**Il pannello non fa scattare niente.** Le automazioni le esegue Home
Assistant; da qui si abilitano e si disabilitano, che è il modo di sospendere
una pianificazione senza perderne i tempi. Un pulsante «esegui adesso»
sarebbe un secondo posto da cui la casa si comanda, e su un vetro che si
scorre col pollice un tocco per sbaglio farebbe partire un'automazione vera.

#### La finestra di validità

Una finestra normale dice «qui si accende». Una **di validità** dice «qui
un'altra automazione ha il permesso di scattare»: è l'intervallo che in Home
Assistant sta in una `condition: time`, e in casa è quello della luce
del corridoio a movimento. Si dichiara in configurazione, non si deduce: scrivere
«si accende alle 22:00» per una cosa che alle 22:00 non si accende è la bugia
più facile da stampare. Il pannello la etichetta «dalle … →» e, a gruppo
aperto, scrive che fuori da quelle ore il movimento non accende.

#### Il tastierino dell'orario

Ogni orario è una pastiglia che si tocca, e apre un tastierino a pagina
piena: ore e minuti con due coppie di tasti grandi, i minuti a passi di
cinque, e la scala che gira — da 23 a 00 è un passo solo.

**Non scrive finché non si conferma.** Cambiare un `input_datetime` fa
scattare le automazioni che lo guardano: mandare a ogni tocco di «+»
vorrebbe dire far partire la pompa passando dalle 15:00 mentre si va verso
le 16:00. In cima al tastierino sta scritto **cosa** si sta cambiando —
gruppo e capo della finestra — perché un orario sopra un velo, con quattro
finestre in casa, non si sa più di chi era.

Il bersaglio del tocco è la pastiglia e non il numero: un numero è alto
quanto il suo corpo, e mirare a venti pixel su un vetro verticale non
riesce.

### 3.6 Agenda

> **STATO ATTUALE: nessun calendario configurato in Home Assistant.**
> Finché non esiste almeno un'entità `calendar`, la sezione **non compare
> nel rail** e la scheda "Oggi" **non compare in home**: gli altri elementi
> si ridistribuiscono lo spazio. È il comportamento previsto dal contratto
> per le sezioni prive della propria sorgente dati, non un caso speciale.

- **Una colonna per giorno**: Oggi, Domani, il giorno seguente e — dove
  c'è spazio — il resto della settimana. Quante colonne, e se invece
  diventa un elenco unico con intestazioni di giorno, dipende dal profilo
  (§5 di `09-profili.md`).
- Evento: barretta colorata del calendario, ora in colonna a larghezza
  fissa, titolo in `f_m`, calendario in `f_s`. Passati al 45%.
- Legenda in fondo, massimo 4 calendari.

### 3.7 Wi-Fi

**Un elenco di reti, e nessuna e speciale.** C'erano una «rete ospiti» e
delle «reti private» tenute separate, e la divisione veniva da come la
funzione era nata — prima una sola, poi un elenco per le altre — non da
qualcosa che esista in casa. Sul vetro diventava due schermate per la stessa
cosa, con un pulsante «torna agli ospiti» che dava a quella rete un ruolo
che non ha.

Fino a **cinque** reti, tutte configurate dalla pagina web
(`wifi_sharing.networks[]`). Il limite non e estetico: cinque sono le voci
della tabella dei segreti, e la riga n-esima usa la password numero n.

**Entrando si vede l'elenco**, non un codice. Chi apre questa sezione ha in
mente *a chi* sta dando la rete, e quella e la prima domanda a cui
rispondere. Una riga per rete: icona, nome mostrato, SSID sotto, chevron.
Le reti spente non compaiono; con nessuna rete accesa la **sezione sparisce
dalla barra** da sola, e chi ci arriva lo stesso legge che non c'e niente da
condividere.

**Toccando una rete** si apre il suo codice:
- Sinistra: riquadro **bianco** con il codice QR. Il fondo bianco e
  obbligatorio: le fotocamere faticano su fondo scuro.
- Destra: nome della rete, password, e il pulsante **«Tutte le reti»** che
  riporta all'elenco.
- Formato `WIFI:T:WPA;S:<ssid>;P:<password>;H:<true|false>;;`
- Ritorno automatico alla home dopo 2 minuti senza tocchi, che basta a non
  lasciare il codice sul vetro per sempre: non serve un conto alla rovescia
  suo.

**La password si mostra**, se chi configura lascia la spunta. Il QR basta a
collegarsi, ma chi la scrive a mano su un portatile ha bisogno di leggerla;
chi non vuole che si possa trascrivere da sopra la spalla toglie la spunta e
resta solo il codice.

**Il codice non elenca piu le altre reti in fondo.** Erano un secondo modo
di fare la stessa cosa, e il secondo era quello che non si trovava. Da qui
si torna indietro, e l'indietro porta dove si era.

**Via anche l'avviso sull'isolamento.** C'era un riquadro che diceva «questa
rete da accesso a tutti i dispositivi di casa», e stava solo sulle reti
private. Ora che le reti sono un elenco senza tipo il pannello **non sa**
quale sia quella degli ospiti: dirlo di tutte sarebbe falso per meta, non
dirlo di nessuna e almeno onesto. Se servira tornare a dirlo, la strada e
una spunta per rete in configurazione — non un indovinello sul nome.

**Niente PIN.** C'e stato in questa specifica — quattro cifre, tre
tentativi, cinque minuti di blocco — ed e stato tolto. Chi sta davanti a
questo pannello e gia dentro casa: ha aperto la porta. Il PIN non proteggeva
da nessuno che non fosse gia oltre la protezione vera, e in cambio chiedeva
tre schermate, un tastierino e quattro cifre da ricordare per una cosa che
si fa due volte l'anno.

### 3.8 Impostazioni (a bordo)
Menu a sinistra (Rete, Home Assistant, Schermo, Sistema, Informazioni),
pannello a destra con righe alte.

Contiene solo ciò che serve quando la rete non funziona: rete e cambio
rete, IP, stato di Home Assistant, luminosità, **interruttore "Consenti
configurazione — 15 minuti"**, riavvio. In fondo il rimando alla pagina
web.

> **L'indirizzo non è fisso.** Il nome mDNS si ricava da
> `system.panel_name`: "PannelloIngresso" risponde a
> `pannelloingresso.local`, non a `pannello.local`. Per un po' queste
> schermate hanno scritto `pannello.local` a prescindere — il nome
> predefinito, giusto solo per chi non aveva cambiato il proprio — e chi
> leggeva quella riga sul pannello di casa leggeva un indirizzo che non
> risponde. La traduzione da nome leggibile a nome host sta in
> `cfg_nome_host()`, in un posto solo, e la usano sia mDNS sia le due
> schermate che scrivono l'indirizzo sul vetro.

La voce **Sistema** del menu apre la schermata di diagnostica: contatori
in alto, registro scorrevole in basso con i filtri per livello. È l'unico
modo per capire cosa non va su un pannello incassato a muro, ed è
specificata per intero in `10-diagnostica.md`.

La voce **Informazioni** è il «chi sono» del pannello: il logo di Foyer,
alto quanto una riga del numero grande del clima, con sotto «il pannello di
casa per Home Assistant»; poi versione, revisione — ritrova il codice esatto
che gira su quel muro — data di compilazione e profilo, e in fondo il perché
del nome. Home Assistant nel menu prende la casa come simbolo e lascia la
«i» a Informazioni, che è dove la si cerca.

### 3.9 Robot
La sezione del robot lavapavimenti. Testata col **nome che gli avete dato**
(`robot.name`), sottotitolo "pulizia per stanze" o "il robot non risponde".

- **La piantina, 6×4.** Le stanze come rettangoli con il nome centrato;
  toccandone una si seleziona e compare la spunta. La piantina è la scelta
  giusta contro un elenco: davanti a un pannello a muro si indica *dove*,
  non si legge un nome in una lista.
- **La batteria** come barra, con le misure dal profilo (`pila_w`,
  `pila_h`).
- **Due pulsanti** in fondo, dell'altezza `tocco.comando_robot_h`: avvio
  della pulizia sulle stanze selezionate, e ritorno alla base. **Senza
  pressione prolungata**: far partire una pulizia non è un'azione da cui
  serva proteggersi, e l'anello che si riempie è un ostacolo, non una
  garanzia.

**Come parte la pulizia.** `vacuum.send_command` con `app_segment_clean`.
I parametri vogliono la **lista piatta** — `params: [20]` — e non la forma
a oggetto: la forma a oggetto sembra quella giusta perché è quella
dell'integrazione nativa, il robot la accetta senza lamentarsi, esce dalla
base e rientra dopo pochi secondi dicendo "pulizia terminata". Questo è
stato dedotto una volta e sbagliato; la forma buona è quella misurata.
Resta configurabile (`robot.rooms_format`) perché su un altro modello
potrebbe non valere.

**I numeri delle stanze** non si indovinano: arrivano da
`roborock.get_maps` con `return_response`, e la pagina web li offre in un
menu a tendina invece di chiedere all'utente di inventarli.

---

## 4. Stati e comportamenti

#### Avvisi e manutenzione

Le stesse cose che dice l'applicazione del robot: il serbatoio dell'acqua
pulita da controllare, quella sporca da svuotare, il panno che non è
agganciato, il filtro da cambiare.

**Le frasi non le scrive il pannello.** Le due entità d'errore — quella
dell'aspirapolvere e quella della base — hanno per stato il messaggio
stesso, già in italiano: *«Il sensore Hall dell'acqua pulita si è attivato,
controllare il serbatoio dell'acqua pulita»*. Riscriverlo qui vorrebbe dire
una seconda traduzione che invecchia da sola e che dice una cosa diversa da
quella che dice il telefono in mano a chi guarda.

Per un sensore a due stati la frase invece serve, perché `on` non è una
frase. E serve anche sapere **quale dei due stati è l'allarme**, che non si
può indovinare: «acqua sporca» accesa vuol dire svuotala, ma «panno
attaccato» **spento** vuol dire che il panno non c'è. Un verso sbagliato
direbbe che manca il panno proprio quando c'è.

#### Dove si vede

Accanto al nome del robot — nella riga in home e nella testata della sua
sezione — compare una pastiglia rossa col numero, **solo quando c'è
qualcosa**. Dice anche la parola («2 avvisi») e non solo il numero: un «2»
da solo accanto al nome di un robot potrebbe essere qualunque conto, e una
pastiglia che si tocca deve dire dove porta.

Nella home quella pastiglia sta **dentro una riga che è già un bersaglio**:
toccando la pastiglia si va agli avvisi, toccando il resto alla sezione. È
il genere di cosa che si verifica premendo, non guardando — `--prova-tocco`
lo fa in tutti e due i posti.

#### La vista

A parte, e non in fondo alla piantina: le stanze sono ciò per cui si apre
questa sezione, e una spazzola con duecento ore davanti non deve rubare loro
spazio. Gli avvisi si annunciano da soli con la pastiglia; la manutenzione
si va a cercare, e si raggiunge dallo stesso posto anche quando non c'è
niente da segnalare — una porta che esiste solo quando la cosa è già rotta è
una porta che nessuno impara.

Dentro: prima gli avvisi accesi, poi gli spenti attenuati con una spunta.
**Gli spenti non si nascondono**: «l'acqua sporca sta bene» è
un'informazione, e serve a fidarsi di quelli accesi. Sotto, la manutenzione,
col tempo che resta **sempre in ore**.

Sempre in ore e non «come arriva», perché non arrivano tutti uguali: alcuni
di quei sensori danno le ore e altri i secondi, e il pannello mostrava
«214 h» accanto a «770400 s». Sono lo stesso dato detto in due lingue, e
davanti a un vetro non si converte a mente. Quanto duri una spazzola il
pannello continua a non saperlo — legge il numero e l'unità, e cambia solo
l'unità.

Una manutenzione sotto la propria soglia conta come un avviso: entra nel
numero della pastiglia, perché una spazzola finita è una cosa da fare come
un serbatoio da svuotare.


### 4.1 Avvio
Riquadro centrato: logo, nome del pannello (`system.panel_name`; se
manca, niente: il logo c'è già), versione, elenco dei passi con esito e
tempi. **C'è un passo in più rispetto a un pannello con la radio a bordo:
il co-processore
ESP32-C6**, che fornisce Wi-Fi e Bluetooth e può fallire da solo.
In fondo: "se qualcosa non risponde, il pannello parte lo stesso in sola
lettura".

I passi sono **veri**: schermo, co-processore, rete Wi-Fi, ora di rete,
Home Assistant. La schermata li controlla da sola ogni quarto di secondo,
spunta ognuno quando riesce e scrive quanto ci ha messo dal momento in cui
è comparsa; un passo già pronto a quel punto non ha tempo. Un token
rifiutato è l'unico guasto che si può riconoscere subito, e si segna.

Compare a ogni accensione che ha una configurazione — al primo avvio c'è
la procedura guidata — e se ne va da sola: 0,8 s dopo l'ultimo passo, al
più tardi dopo 20 s qualunque cosa sia successa, oppure a un tocco. Non
tiene mai fermo il pannello: da lì in poi dicono cosa manca la fascia di
riconnessione e §4.3.

> Per mesi è stata un disegno: sei passi con tempi scritti a mano, uno dei
> quali erano le telecamere tolte dal progetto, e sul pannello non la
> mostrava nessuno.

### 4.2 Riconnessione (transitoria)
Fascia ambra in testa alla sezione: rotellina, tentativo N, a
destra l'ora dell'ultimo dato valido e "i comandi sono sospesi". Valori al
45% di opacità. Voci del rail attenuate.

### 4.3 Home Assistant irraggiungibile (persistente)
Schermata piena: icona, titolo, ora dell'ultimo dato, frequenza di
ritentativo, due pulsanti. In alto restano rete e orologio — la rete com'è
davvero, non «collegata» per principio. In fondo il logo di Foyer,
attenuato: una firma e non un titolo — quando il pannello non ha niente di
buono da mostrare dice almeno di chi è.

### 4.3-bis Il Wi-Fi non si collega
Viene **prima** di §4.3: senza rete Home Assistant tace per forza, e dire
«Home Assistant non risponde» a chi ha cambiato la password del router lo
manderebbe a cercare dalla parte sbagliata. Compare dopo un minuto senza
rete — non durante la prova di una rete nuova, che ha il suo paracadute —
e dice il motivo com'è adesso: password rifiutata, rete non trovata,
tentativo in corso. Il pannello continua a riprovare da solo.

Due pulsanti: **Scegli la rete**, che apre i passi della rete del primo
avvio — elenco, password, prova — senza ripassare da Home Assistant, e
**Riprova adesso**. È l'unica strada che non chiede un cavo: la pagina di
configurazione arriva via Wi-Fi, cioè proprio da dove non si passa. In
fondo la stessa firma di §4.3.

### 4.4 Riscontro dei comandi
Il riscontro sta **nel pulsante toccato**.

| Stato | Aspetto | Durata |
|---|---|---|
| In corso | rotellina + "Invio…", `#2a2313`, riga evidenziata | fino alla risposta |
| Riuscito | spunta + "Inviato", `#16301f` | 3 s |
| Fallito | croce + "Non riuscito", `#2e1a17` | permanente |

Avviso in basso: quello di successo dichiara che Home Assistant ha
accettato il comando, non che il cancello si sia aperto.

### 4.5 Conferma di apertura
Velo al 76% e riquadro modale. Due pulsanti larghi uguali, distanti, e
annullamento automatico dopo 8 s.

Dietro il velo restava visibile la telecamera dell'accesso, dove lo spazio
lo consentiva: si vedeva chi si stava facendo entrare mentre si confermava.
È uscita con tutto il resto del video l'08/09/2026.

### 4.6 *(era: telecamera non raggiungibile)*

### 4.7 Standby

**Il fondo è nero puro** (`C_NERO`), non il fondo del tema. `C_BG` è un
grigio molto scuro con dentro un filo di blu, e su un pannello a muro di
notte quel filo si vede: un rettangolo debolmente illuminato in un corridoio
buio. È l'unica schermata senza schede sopra, quindi il nero non toglie
struttura — toglie solo luce. Ovunque altro `C_BG` resta giusto: separa il
fondo dalle schede, che sono più chiare.
Dopo 120 s: retroilluminazione al minimo e vista di standby. Opacità 62%.
Dopo 600 s schermo spento. Risveglio al tocco.

**Il pannello sta al posto di un termostato**, e la schermata lo dice: i
numeri grandi sono **due**, l'ora e la temperatura della stanza, della
stessa taglia (`font.standby`) e separati da un filo. In verticale non
stanno affiancati e si impilano, filo orizzontale in mezzo.

Sotto l'ora, quando c'è qualcosa di aperto, una pastiglia ambra col nome
della prima apertura. Sta lì e non nella fascia perché è l'unica cosa in
questa schermata che dica qualcosa di urgente: in mezzo ai numeri
dell'energia si leggerebbe come un numero fra gli altri.

Sotto la temperatura, due pastiglie: «sta scaldando» in ambra quando
l'impianto sta chiamando, e i gradi chiesti. Vengono da `standby.climate`, e
senza quell'entità non compaiono — restano i gradi misurati, che è una
schermata onesta e completa.

In fondo una fascia con quattro dati: la temperatura di fuori con la
condizione del meteo accanto al nome, la potenza dal sole, il consumo della
casa, la carica della batteria. Quanti per riga lo dice il profilo
(`griglie.standby`): quattro in orizzontale, due per due in verticale.

**Niente si inventa.** Ogni pezzo compare se il suo dato c'è e sparisce se
non c'è, invece di mostrare un trattino. Un trattino in mezzo ad altri
numeri sembra un valore, e questa è la schermata che si guarda di sfuggita
passando in corridoio: nessuno andrebbe a verificare. Quando un dato manca,
i rimanenti si ridistribuiscono e non resta il buco.

**Le due temperature si configurano**, e ognuna può stare nello **stato** o
in un **attributo** dell'entità — lo dice `standby.indoor_temperature` per
la stanza e `weather.local_temperature_sensor` per fuori. Non è un vezzo:
un sensore Zigbee tiene la temperatura nello stato, un termostato la tiene
in `current_temperature`, e sono entrambi sensori di temperatura.

**Spostamento dei pixel.** Ogni 3 minuti il contenuto della schermata di
standby trasla di pochi pixel lungo un ciclo chiuso, per evitare la
ritenzione d'immagine.

| Parametro | Valore |
|---|---|
| Periodo | 180 s |
| Ciclo (x, y) | sette posizioni entro l'escursione indicata in §9 |
| Applicazione | traslazione del contenitore radice dello standby |
| Transizione | immediata |
| Altre schermate | non traslano |

Margine interno minimo: 1 px oltre l'escursione massima, per lato.

### 4.8 Ritorno automatico
60 s di inattività in una sezione → home. Schermata Wi-Fi: 120 s.

### 4.9 Primo avvio
**Quattro** passi con pallini, e ognuno salva la sua parte appena è
completa — non tutto alla fine: se salta la corrente a metà configurazione
si riprende da dove si era invece di ricominciare.

1. **Le reti** che la radio vede davvero, la più forte per prima (altezza
   riga: §8). Mentre la scansione gira l'elenco lo dice, invece di restare
   vuoto: un elenco vuoto e un elenco che non arriverà mai si somigliano
   troppo.
2. **La password** della rete scelta. Si vede mentre si batte, ed è
   voluto: su una tastiera a schermo, con le dita, una password lunga si
   sbaglia quasi sempre, e l'errore che ne segue — "non si connette" — non
   dice mai che il problema era una lettera. Chi è davanti al pannello è in
   casa propria. Su una rete aperta il passo si salta.
   Finisce in NVS come tutti i segreti: si vede qui, e da lì in poi non la
   mostra più nessuno.
3. **Indirizzo e token** di Home Assistant. Il token si maschera già
   mentre lo si scrive — e si può **rimandare**: "Lo faccio dopo".
   Un token a lunga durata è un JWT, circa duecento caratteri di base64
   casuale, e batterlo su una tastiera a schermo è mezz'ora e tre
   tentativi — con l'errore che non si vede subito, ma due schermate dopo
   come "token rifiutato". A questo punto la rete c'è già da due passi,
   quindi la pagina web risponde e il token si incolla di là.
   Rimandare non è una scorciatoia: è la strada giusta.
   Un campo lasciato vuoto **non cancella** quello che c'era: "lo faccio
   dopo" e "toglilo" sono due cose diverse.
4. **La prova**, con i quattro controlli che si accendono da soli leggendo
   lo stato vero della rete e del collegamento. Se il token è stato
   rimandato le righe che dipendono da lui lo dicono — "da incollare su
   `http://<nome-del-pannello>.local`" — invece di mostrare una clessidra
   che non
   finirà mai. Tre esiti e non due —
   fatto, in corso, fallito — perché "token rifiutato" non è "sto ancora
   provando", e aspettare una clessidra che non finirà mai è il modo
   peggiore di scoprirlo. Si può uscire comunque: un pannello che non
   lascia uscire finché Home Assistant non risponde si può bloccare per un
   server spento, e da dentro non si aggiusta niente. Il pulsante dice
   quale delle due cose sta facendo.

Tastiera QWERTY (misure: §8), riga funzioni con MAIUSC, CANC, 123,
@#€, spazio e tasto d'azione ambra.

**Niente codice QR e niente punto di accesso temporaneo.** C'era un QR che
rimandava all'indirizzo predefinito dell'access point dell'ESP32, e
quell'indirizzo non è mai esistito: nel
progetto non c'è una riga che accenda la modalità AP. Chi lo inquadrava
trovava il telefono che non si collegava a niente.

E non si fa. Serviva a rompere un cerchio — un pannello senza rete non ha
una pagina raggiungibile — ma quel cerchio ora si rompe prima: la password
del Wi-Fi si batte al passo 2, è corta, e da quel momento la pagina web
risponde. La cosa lunga davvero è il token di Home
Assistant, ed è esattamente quella che il passo 3 lascia rimandare.

Il conto sarebbe: SoftAP, un secondo server, una pagina ridotta e il
passaggio fra le due reti, per il caso che resta — una rete nascosta, che la
scansione non mostra — e che si risolve dalla console. Non vale.

L'elenco intanto si prende tutta la larghezza: due reti in più senza
scorrere.

---

## 5. Temporizzazioni

| Comportamento | Valore |
|---|---|
| Pressione prolungata | 1500 ms |
| Timeout comando | 8 s |
| Conferma modale, annullo automatico | 8 s |
| Riscontro "riuscito" | 3 s |
| Ritorno automatico alla home | 60 s (Wi-Fi: 120 s) |
| Standby | 120 s |
| Spostamento pixel in standby | ogni 180 s |
| Spegnimento schermo | 600 s |
| Riconnessione a Home Assistant | ogni 15 s |
| Sblocco configurazione web | 15 minuti |
| PIN: blocco dopo 3 errori | 5 minuti |
| Rete privata: rientro automatico | 60 s |

---

## 6. Divergenze fra i pannelli

Sono raccolte in `09-profili.md`. Nessuna va cercata o dedotta da questo
documento. Il pannello di riferimento è `p4-800x1280`.
