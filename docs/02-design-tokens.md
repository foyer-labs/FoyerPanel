# Foyer Panel — Token di design

Valgono per **entrambi i pannelli**. Qui stanno palette, icone e regole di
animazione: tutto ciò che è identico.

**Corpi tipografici, geometria e aree di tocco NON stanno qui**: variano
per pannello e vivono in `09-profili.md` §3, §4 e §8. Le misure arrivano dal
profilo.

---

## La palette qui sotto è quella **predefinita**

Dal 09/09/2026 otto colori si scelgono in configurazione (`appearance`), e
tutti gli altri si ricavano da quelli. I valori di questa pagina sono
quelli con cui il pannello è disegnato quando nessuno ha scelto niente — e
sono anche quelli che i mockup rendono 1:1.

Restano quindi **il riferimento**, non più l'unica possibilità. «Le catture
sono indistinguibili dai mockup» va letto: *con la palette predefinita*.

Che questa tabella, `main/ui/tema.c` e i `default` dello schema dicano la
stessa cosa lo verifica `tools/verifica_palette.py`. È la prova che rende
sicuro mostrare un colore nella pagina senza scriverlo nel documento: se i
tre si allontanassero, la pagina mostrerebbe un colore e il pannello ne
userebbe un altro, senza un errore da nessuna parte.

## Palette

| Nome | Hex | RGB565 | Uso |
|---|---|---|---|
| `bg` | `#0D1014` | `0x0842` | sfondo generale |
| `card` | `#151A21` | `0x10C4` | schede |
| `card2` | `#11161C` | `0x08A3` | sfondi secondari, rail, barre |
| `line` | `#1E242D` | `0x1925` | bordi 1 px |
| `txt` | `#E8ECF2` | `0xE75E` | testo principale |
| `dim` | `#8A95A6` | `0x84B4` | testo secondario |
| `acc` | `#F0A835` | `0xF546` | ambra: accento, selezione, azione |
| `ok` | `#4FC39A` | `0x4E33` | verde |
| `warn` | `#E2664F` | `0xE329` | rosso |
| `luce` | `#F0A835` | `0xF546` | la lampadina di una luce accesa, e la barra della sua luminosità |
| `cool` | `#57B6E0` | `0x5DDC` | azzurro: impianto in raffrescamento |
| `caldo` | `#F0A835` | `0xF546` | ambra: impianto in riscaldamento, zona che chiama |
| `meteo_sole` | `#F0A835` | `0xF546` | sereno |
| `meteo_nuvola` | `#9AA7B5` | `0x9D36` | nuvoloso, nebbia |
| `meteo_pioggia` | `#57B6E0` | `0x5DDC` | pioggia |
| `meteo_neve` | `#BBD9E8` | `0xBEDD` | neve, grandine |
| `off` | `#232B35` | `0x2166` | tracce spente |
| `ink` | `#0D1014` | `0x0842` | testo sopra l'ambra |

### I primi nove si scelgono

`bg`, `card`, `line`, `txt`, `dim`, `acc`, `ok`, `warn` e `luce` sono le
voci del blocco `appearance` — vedi `03-config-contratto.md` §4.11.

`luce` è a parte perché **non è uno stato dell'interfaccia**: è una cosa
che si accende in casa. Chi guarda un lampadario verde non pensa al tema
del pannello, pensa che la luce sia di quel colore.

`cool` e `caldo` sono una coppia e non si scelgono: dicono **cosa sta
facendo l'impianto**. L'azzurro era già fisso; l'ambra era l'accento, e
sembrava giusta perché l'accento era ambra — la stessa coincidenza del
sole. Adesso `caldo` tiene quell'ambra per conto suo.

I quattro `meteo_*` non si scelgono e non si ricavano, per la stessa
ragione: l'icona del tempo deve dire **che tempo fa**.
Prima prendeva il colore dell'accento, e con l'ambra sembrava giusta perché
l'ambra sembra un sole — era una coincidenza, e si è vista appena l'accento
è diventato un altro. Il sole tiene l'ambra di sempre, così con la palette
predefinita quell'angolo non si muove.

`card2` e `off` si ricavano dai neutri con una quota per canale misurata
dai valori qui sopra; `ink` — il testo sopra l'accento — è scuro o chiaro a
seconda di quanto è chiaro l'accento, e con l'ambra predefinita viene `bg`,
cioè quello che era scritto prima.

`cool` non si sceglie e non si ricava: dice **cosa sta facendo il
condizionatore**, non che aspetto ha il pannello, e un azzurro che segue
l'accento smetterebbe di voler dire freddo. Stessa cosa per i quattro dei
calendari, che devono distinguersi fra loro.

### Gli stati composti si ricavano ruotando la tinta

Non sono mescolanze, e non lo erano nemmeno prima: sono state misurate.
Cercando la quota di tinta che meglio li riproduce, lo scarto arriva a
**undici unità** e il pixel a sedici bit viene diverso in tredici casi su
sedici — cioè una formula del tipo «fondo più un decimo di accento» non li
ridarebbe: li cambierebbe.

Quello che li ridà esatti e li fa anche seguire un accento nuovo è ruotare
la tinta di quanto è girata quella della loro famiglia, lasciando dove
stanno la saturazione e la luminosità che erano state scelte a mano. Con i
colori predefiniti la rotazione è zero, e per una rotazione di zero il
colore torna indietro **senza passare da nessuna conversione**: l'identità
è un ramo scritto apposta, non un arrotondamento fortunato.

| Uso | Sfondo | Bordo | Testo | Famiglia |
|---|---|---|---|
|---|---|---|---|---|
| Selezione nel rail | `#221C0E` | `#4A3C1C` | `acc` | `acc` |
| Pulsante "in corso" | `#2A2313` | `#4A3C1C` | `#F0C987` | `acc` |
| Pulsante "riuscito" | `#16301F` | `#2C5A3D` | `#7DDBA6` | `ok` |
| Pulsante "fallito" | `#2E1A17` | `#5A2F28` | `#F09287` | `warn` |
| Fascia riconnessione | `#251D0E` | `#4A3C1C` | `#F0C987` | `acc` |
| Avviso positivo | `#13251C` | `#2C5A3D` | `#A9E3C3` | `ok` |
| Avviso negativo | `#271815` | `#5A2F28` | `#F3B5AC` | `warn` |

Calendari: `#F0A835`, `#4FC39A`, `#6F9FE0`, `#B98BD9`.

---

## Rimandi

| Cosa | Dove |
|---|---|
| Corpi tipografici | `09-profili.md` §4 |
| Geometria (rail, testate, raggi, gap) | `09-profili.md` §3 |
| Aree di tocco | `09-profili.md` §8 |

---

## Icone

In font-icona o immagini precompilate, identiche sui due pannelli. Tratto
1,6 px su griglia 24×24, terminazioni arrotondate.

Le dimensioni in uso scalano con il profilo: l'icona del dock e quella del
rail seguono la misura del rispettivo pulsante (§3, §8).

---

## Animazioni

Il pannello grande reggerebbe molto di più, ma **l'aspetto deve restare lo
stesso su entrambi**. Ammesse:

| Effetto | Uso |
|---|---|
| Rotellina | attesa, riconnessione |
| Pallino pulsante | indicatore "diretta" |
| Anello di avanzamento | pressione prolungata |
| Barra a scorrimento | miniatura in caricamento |
| Dissolvenza 150 ms | comparsa dei modali |

**Vietate**: transizioni di pagina a schermo intero, scorrimenti animati
fra sezioni, ombre sfumate. È una scelta, perché i due orientamenti devono
sembrare lo stesso apparecchio.

## Icone aggiunte per la sezione Clima

fiamma (caldo), fiocco di neve (freddo), ciclo (auto), goccia
(deumidifica), ventola (solo ventilatore e XFan), interruttore di
accensione (spento), cronometro (timer), lamella oscillante,
lamella verticale, lamella orizzontale, tratti fermi (fisso),
altoparlante attenuato (silenzioso), pannello (luce pannello),
flussi d'aria (aria fresca).

Le cinque posizioni del deflettore non sono icone di libreria ma un
disegno parametrico: rettangolo del corpo macchina più una lamella
posizionata a cinque altezze diverse. Va generato, non cercato.
