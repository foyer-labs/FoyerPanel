# Foyer Panel — Profili di pannello — unica fonte delle divergenze

> **REGOLA FONDAMENTALE DEL PACCHETTO**
>
> Questo è **l'unico documento** che può contenere numeri o comportamenti
> che cambiano fra un profilo e l'altro. Ogni altro file descrive la
> struttura, non le misure specifiche di uno schermo.
>
> Se trovi un numero dipendente dal pannello in un altro documento, **è un
> errore di quel documento**, non un'informazione da usare. Segnalalo.
>
> Nel codice, la controparte di questo documento è **un solo file**:
> `profile.h`. Nessun altro sorgente contiene misure di layout.

I profili sono lo stesso apparecchio in orientamenti diversi: stessa
interfaccia, stesse funzioni, stessi testi, stessi comportamenti. Cambiano
la risoluzione, l'orientamento e come si dispone ciò che ci sta dentro.

**Due profili**: una famiglia hardware in due orientamenti. Non sono due
progetti, sono due tabelle in questo documento e due possibili contenuti di
`profile.h`.

---

## 0. Una sola famiglia hardware

C'è una famiglia hardware sola: l'ESP32-P4 con il vetro da 10,1". Le
astrazioni che ne reggerebbero una seconda ci sono lo stesso —
`famiglia_t`, `scheda.h` che sceglie fra schede, il generatore che legge
l'elenco invece di saperlo a memoria — e non sono un lusso: toglierle
renderebbe caro il giorno che un secondo apparecchio serva davvero.

---

## 1. Identificatori

| Chiave (`system.profile`) | Pannello | Risoluzione | Orientamento |
|---|---|---|---|
| `p4-1280x800` | 10,1" ESP32-P4 | 1280×800 | orizzontale |
| `p4-800x1280` | 10,1" ESP32-P4 | 800×1280 | **verticale** |

---

## 1-bis. Orientamento — quanto costa

Il vetro ha un verso nativo, ed è la cosa da sapere prima di decidere come
montarlo.

| | pannello da 10,1" (P4) |
|---|---|
| Orientamento nativo del vetro | **verticale** 800×1280 |
| Verticale | **nativo, gratis** |
| Orizzontale | richiede rotazione |
| Costo della rotazione | basso: MIPI-DSI, 32 MB di PSRAM e nessuna contesa di banda |
| Fps attesi ruotato | 60, invariati |

**Conseguenza pratica**: il verticale è il verso nativo e non costa niente,
ed è quello montato al muro. L'orizzontale richiede una rotazione, che su
questo silicio è a buon mercato — MIPI-DSI, 32 MB di PSRAM, nessuna contesa
di banda — e resta il profilo predefinito solo per chi lo montasse coricato.

Sul pannello vero la rotazione non passa da `lv_display_set_rotation`: la
rotazione software di LVGL 9 è **incompatibile** con la modalità di
disegno diretta che questo firmware usa, e la si ottiene specchiando in
hardware (`esp_lcd_panel_mirror`), che costa zero. Vedi
05-architettura-firmware.md.

---

## 1-ter. Cosa cambia con l'orientamento

Non è una rotazione del layout: è una riorganizzazione.

| | Orizzontale | Verticale |
|---|---|---|
| Navigazione | rail verticale a sinistra | **barra orizzontale in basso** |
| Impostazioni | ingranaggio in fondo al rail | ingranaggio nella testata della home |
| Dock in home | presente, un pulsante per sezione | **assente**: la barra lo sostituisce |
| Griglie luci e clima | 3 colonne | **2 colonne**, più righe |
| Condizionatori | 4 colonne affiancate | **griglia 2×2** |
| Agenda | 3 o 4 colonne per giorno | **elenco unico** con intestazioni di giorno |
| Accessi | comandi a righe piene | comandi a righe piene |
| Avviso dopo un comando | sovrapposto in basso | **in fondo alla colonna**, non sovrapposto |
| Menu impostazioni | due colonne | elenco unico scorrevole |
| Standby | informazioni in riga | informazioni impilate |

## 2. Hardware

| | `p4-1280x800` |
|---|---|
| Scheda | Guition JC8012P4A1C, 10,1" |
| SoC | ESP32-P4, RISC-V 2×400 MHz |
| Risoluzione | 1280×800 |
| Densità | 149 ppi |
| Interfaccia display | MIPI-DSI, 2 corsie a 832 Mbps |
| Refresh atteso | 60 fps |
| PSRAM | 32 MB |
| RAM interna | 768 KB L2MEM |
| Flash | 16 MB |
| Decodifica JPEG | **hardware**, più PPA e H264 |
| Wi-Fi | **ESP32-C6 via SDIO** (ESP-Hosted), Wi-Fi 6 e BLE |

## 3. Geometria

| Elemento | `p4-1280x800` | `p4-800x1280` |
|---|---|---|
| Larghezza rail | 106 | — |
| Altezza barra in basso | — | 112 |
| Voce del rail | 88×88 | — |
| Voce HOME del rail | 88×66 | — |
| Voce ingranaggio del rail | 88×56 | — |
| Voce della barra | — | 100×112 ▷ |
| Altezza testata di sezione | 66 | 72 |
| Altezza testata della home | 102 | 116 |
| Padding del contenuto | 20 | 18 |
| Padding interno delle schede, ai lati | 18 | 17 |
| Padding interno delle schede, sopra e sotto | 16 | 15 |
| Gap fra schede | 14 | 14 |
| Raggio delle schede | 16 | 16 |
| Raggio dei riquadri | 13 | 13 |
| Raggio dei pulsanti | 11 | 11 |

▷ **misura derivata, non un dato di ingresso.** Le voci della barra si
dividono la larghezza in parti uguali: 75 e 100 sono il risultato con otto
voci (HOME più sette sezioni), non un numero da imporre. Lo stesso vale
per i tasti del tastierino PIN in §8. In `profile.h` non compaiono: li
calcola il layout.

Il **padding interno delle schede** non è quello del contenuto e non è
uguale sui due assi: nei mockup è più stretto sopra e sotto che ai lati
(`.card{padding:16px 18px}` in orizzontale, `15px 17px` in verticale). Non era mai stato dichiarato, e il codice riusava il padding
del contenuto per entrambi gli assi: da 4 a 8 pixel di troppo per scheda,
che sul dettaglio del condizionatore erano esattamente quelli che
mancavano.

Le colonne verticali di questa tabella sono confermate dai mockup
`mockup/verticale/`; le altezze di testata della home dei due profili
verticali erano sbagliate in `profili.json` (92 e 102, cioè i valori
orizzontali) e sono state corrette a 100 e 116.

## 4. Tipografia

Famiglia **Inter**, font bitmap compilati, cifre tabulari.
**Massimo 6 corpi per profilo.**

| Nome | `p4-1280x800` | Peso | Uso |
|---|---|---|---|
| `f_xxl` | 56 | 600 | orologio in home |
| `f_xl` | 38 | 600 | valori grandi |
| `f_l` | 26 | 600 | titoli di sezione |
| `f_m` | 17 | 500 | interfaccia, pulsanti |
| `f_s` | 14 | 400 | testo secondario |
| `f_xs` | 12 | 600 | maiuscoletto, spaziatura 0.16em |

Corpi fuori scala, da compilare con il solo insieme di caratteri
necessario:

| Uso | `p4` | Caratteri |
|---|---|---|
| I due numeri dello standby | 115 | cifre, due punti, virgola, grado, C |
| Energia in home | 52 | cifre, virgola, meno |
| Temperatura nel dettaglio clima | 96 | cifre, virgola, grado |
| Decimi accanto a un valore `f_xl` | 30 | cifre, virgola, punto, grado |
| Password Wi-Fi (monospaziato) | 34 | ASCII stampabile |

Il corpo dello standby era 132/160, quando l'orologio era solo. È sceso
quando gli si è affiancata la temperatura della stanza: **due** numeri di
quella taglia non ci stavano affiancati, e sono della stessa
taglia di proposito — su un pannello montato al posto di un termostato
nessuno dei due comanda sull'altro (01-specifica-ui.md §4.7).

Nello stesso momento l'insieme dei caratteri si è allargato: quel corpo
mostrava solo un orologio, adesso mostra anche `20,8°C`. Senza la virgola e
il grado si vedrebbero rettangoli vuoti nel numero più grande dello
schermo.

### La scala è per famiglia, non per profilo

La colonna è una: **il profilo verticale usa la scala del proprio
orizzontale**. Il mockup verticale disegna una scala leggermente diversa,
ma la differenza è di uno o due pixel, 12,5 px non è un corpo compilabile
come bitmap, e due insiemi di font invece di uno raddoppierebbero
l'occupazione in flash senza che si veda. Dove il mockup verticale scende
di un punto, si usa il corpo della famiglia.

La regola vale ancora con una famiglia sola, e non è una formalità: è ciò
che tiene i due orientamenti su un solo insieme di bitmap.

### Aggancio alla scala

I mockup usano molti più corpi di sei: 14, 15, 12, 13, 22, 26, 28, 31,
32, 36 e altri. Con sei corpi per profilo non si riproducono alla lettera,
ed è una scelta, non un limite: **ogni misura del mockup si aggancia al
corpo più vicino della scala**, e gli scarti restano entro 2 px. Chi
implementa una schermata non sceglie un corpo, lo aggancia:

Questa tabella non è scritta a mano: sono i corpi che compaiono davvero nel
CSS dei mockup del P4, ciascuno agganciato al corpo di scala più vicino.

| Corpo del mockup | Corpo di scala | Scarto massimo |
|---|---|---|
| 150 · 160 | fuori scala: i due numeri dello standby, oggi **115** | vedi sotto |
| 56 | `f_xxl` 56 | 0 |
| 50 · 52 | fuori scala: energia in home 52 | 2 |
| 36 · 38 · 40 · 44 | `f_xl` 38 | 6 |
| 31 · 32 · 34 | fuori scala: password Wi-Fi monospaziata 34 | 3 |
| 22 · 23 · 24 · 25 · 26 · 27 · 28 · 30 | `f_l` 26 | 4 |
| 15,5 · 16 · 16,5 · 17 · 18 · 20 · 21 | `f_m` 17 | 4 |
| 13 · 13,5 · 14 · 14,5 · 15 | `f_s` 14 | 1 |
| 10 · 10,5 · 11 · 11,5 · 12 · 12,5 | `f_xs` 12 | 2 |

I 150 e 160 dello standby sono l'unico scarto grosso, ed è **una decisione
e non una deriva**: i mockup disegnano l'orologio da solo, il pannello gli
ha affiancato la temperatura, e due numeri di quella taglia non ci stanno
accanto. Il paragrafo qui sopra lo racconta per esteso.

Gli scarti da 4 e 6 px sono l'unico prezzo dei sei corpi: un corpo in più
sarebbe un altro bitmap in flash per due pixel che nessuno distingue da un
metro e mezzo.

## 5. Griglie e impaginazione

| | `p4-1280x800` | `p4-800x1280` |
|---|---|---|
| Luci | 3×4 = **12** | 2×7 = **14** |
| Clima riscaldamento, senza piani | 3×4 = **12** | 2×6 = **12** |
| Clima riscaldamento, con le fasce dei piani | **5 righe** | **8 righe** |
| Clima condizionatori | 4 colonne | 2×2 |
| Agenda | 4 colonne | elenco |
| Scene luci | fino a 5 | fino a 5 |
| Fascia dello standby | 4×1 | 2×2 |

Il numero **totale** di elementi non dipende dal profilo: oltre la
capienza di pagina si impagina. Una configurazione resta quindi valida su
entrambi i pannelli.

**Il riscaldamento con le fasce si conta in righe** e non in schede, perché
quanto è alta una pagina dipende da come i piani si dividono le zone: sei
zone in due colonne sono tre righe in un gruppo solo e cinque in due gruppi.
Una fascia costa una riga di bilancio anche se è più bassa di una riga di
schede — è un arrotondamento in eccesso, e in eccesso è il verso giusto per
una misura che decide se qualcosa esce dal vetro. Con dodici zone su due
piani: in verticale ci stanno tutte in una pagina (2 fasce + 6 righe = 8);
in orizzontale no, perché 800 px di altezza tengono tre righe e una fascia,
e la seconda fascia va a pagina due.

Le due griglie arrivavano a **otto riquadri** e non oltre, per una regola
scritta in `prova_profilo`: le schermate scrivono i testi variabili in un
buffer di otto a giro, e — diceva la regola — l'etichetta di LVGL punta
dentro quel buffer invece di copiarlo. La premessa era falsa:
`lv_label_set_text()` fa `lv_malloc` e `lv_strcpy`, e
`lv_label_set_text_static()` — quella che punta davvero — in questo
programma non compare. La regola è caduta quando ha impedito di far stare
dodici zone in una pagina; i buffer a giro sono rimasti dove sono, che non
servono e non fanno danno.

La fascia dello standby è l'eccezione, e per una ragione buona: i dati sono
**quattro esatti** — fuori, sole, casa, batteria — non un elenco che
cresce. Il prodotto deve fare quattro, e `prova_profilo` lo pretende: con
un prodotto diverso l'ultimo andrebbe a capo da solo, lasciando una riga
con un dato e tre buchi su un profilo solo, che è il genere di cosa che
nessuno guarda.

## 6. Home

| | `p4-1280x800` | `p4-800x1280` |
|---|---|---|
| Altezza fascia schede | 300 | flex |
| Scheda Presenza | 250 | metà larghezza |
| Scheda Energia | 296 | larghezza piena |
| Scheda Aperture e luci | 320 | metà larghezza |
| Scheda Agenda | flex | larghezza piena |
| Dock | presente | **assente** |
| Pulsanti nel dock, al massimo | **7** | — |
| Altezza del dock | residuo della colonna | — |

In verticale la home è una colonna sola: Energia a tutta larghezza,
Presenza e Aperture affiancate a metà, Agenda sotto. Le altezze sono
lasciate al flex, non fissate.

> Qui c'era anche la **striscia delle telecamere**, quattro anteprime in
> fondo alla home, con la nota sul perché non avesse un'altezza propria: la
> decideva il rapporto 16:9. È uscita l'08/09/2026 con tutto il resto del
> video.

**Capienza della barra, in verticale.** La barra tiene `barra_max` voci —
**sei** — e non si stringe mai. Il conto è semplice e va fatto: 800 px
divisi in parti uguali danno 100 px a voce con otto destinazioni, 88 con
nove, 80 con dieci. Il minimo di tocco non è mai in pericolo a quelle
misure: **quello che si rompe è la parola**, e una fila di icone senza nome
è un rebus finché non la si impara.

Quindi HOME, le prime quattro sezioni nell'ordine di `sections`, e **«Altro»**,
che apre un foglio dal basso con le rimanenti. Se le sezioni attive ci
stanno tutte, «Altro» non compare: un pulsante che apre un foglio vuoto è
peggio di nessun pulsante.

Due proprietà del foglio, e nessuna è cosmetica: si ferma **sopra la barra**,
così la voce da cui è uscito resta visibile e ripremibile; e quando si sta
guardando una sezione che nella barra non ha voce, è **«Altro» ad
accendersi** — una barra tutta spenta lascerebbe chi guarda senza sapere
dov'è.

**Capienza del dock, in orizzontale.** Le sezioni attive possono essere
nove, i pulsanti del dock al massimo sei. Quando non ci stanno, il dock mostra le
prime `dock_max` nell'ordine dell'array `sections` di `config.json`. Il
rail continua a mostrarle **tutte**, quindi nessuna sezione diventa
irraggiungibile: il dock è una scorciatoia, non l'unica via. In verticale
`dock_max` vale 0 perché il dock non c'è.

## 7. *(era: telecamere)*

Griglie e rapporto 16:9 dei riquadri video. Uscito due volte: con go2rtc
l'08/09/2026, con l'RTSP il 09/09/2026. Il numero resta vuoto — «§7» è
citato in parecchi punti di questo progetto e deve continuare a non trovare
niente invece di trovare un altro capitolo.

Il motivo della seconda rinuncia, coi numeri, è in
`05-architettura-firmware.md` §5.

## 8. Aree di tocco

Minimo assoluto **44×44** su entrambi.

| Elemento | `p4-1280x800` | `p4-800x1280` |
|---|---|---|
| Pulsante del dock ▷ | ~171×195 | — |
| Pulsante di apertura | 132×52 | 132×52 |
| Tasti `−` / `+` clima | 48×42 | 46×42 |
| Tasti `−` / `+` dettaglio clima | 78×78 | 78×78 |
| Riquadro modale del PIN | 740 | larghezza utile |
| Tastierino PIN ▷ | 108×72 | ×76 |
| Tasti della tastiera ▷ | 116×105 | **72×88** |
| Interruttore | 58×32 | 58×32 |
| Riga oscillazione, dettaglio clima | ×52 | ×48 ◁ |
| Riga extra, dettaglio clima | ×56 | ×48 ◁ |
| Frecce del paginatore | 48×44 | 48×44 |
| Riga di rete al primo avvio | ×110 | ×110 |

◁ **più basse del mockup, e apposta.** `clima-condizionatori.html` dà 52 e
56 a tutti i profili, ma con quelle misure la colonna destra del dettaglio
chiede più altezza di quanta ce ne sia. Queste due righe sono a pastiglia, una riga di testo e un'icona, e
scendere a 46 le lascia comunque sopra il minimo di tocco di 44. È la
misura che costa meno fra quelle che si potevano stringere.

▷ **misure derivate**: pulsanti del dock, tasti del tastierino e della
tastiera si dividono lo spazio disponibile. I numeri sono il risultato
atteso, non un dato da imporre; in `profile.h` c'è ciò che li determina —
larghezza del riquadro modale, numero di righe e colonne — non il
risultato.

◁ Diverse voci scendono sotto i 44 px dichiarati come minimo: i tasti
`−`/`+` del clima ne sono l'esempio. La contraddizione è voluta e si
risolve **allargando l'area sensibile oltre il disegno**
(`lv_obj_set_ext_click_area`) invece di ingrandire il tasto: il disegno
approvato resta quello del mockup e il dito trova comunque 44×44. Vale
per ogni riga di questa tabella che sta sotto il minimo.

## 9. Comportamenti che divergono

| | `p4-1280x800` |
|---|---|
| Passi nella schermata di avvio | **6** (co-processore C6) |
| Attesa prima del rollback OTA | **120 s** |
| Partizione firmware co-processore | `c6_fw`, 2 MB |
| Transizioni e animazioni | **le stesse**: i pannelli devono sembrare lo stesso apparecchio |
| Rotazione software | **sì in orizzontale**, no in verticale |
| Spostamento pixel in standby | ±4 / ±5 px |

Tutto ciò che non compare in questo documento è **identico** sui due
pannelli: palette, gerarchia visiva, testi, temporizzazioni, logica di
navigazione, riscontro dei comandi, regole di onestà sugli stati non
conosciuti.

---

## 10. Come si usa nel codice

`profile.h` espone una struttura compilata a partire da queste tabelle:

```c
typedef struct {
    const char *chiave;          /* "p4-1280x800" | "p4-800x1280" */
    uint16_t larghezza, altezza;
    uint16_t rail_w, head_h, home_head_h, pad, gap, radius;
    const lv_font_t *f_xxl, *f_xl, *f_l, *f_m, *f_s, *f_xs;
    orientamento_t orientamento;       /* ORIZZONTALE | VERTICALE */
    uint16_t navbar_h;                 /* 0 in orizzontale */
    uint8_t luci_cols, luci_rows;
    uint8_t clima_cols, clima_rows;
    uint8_t agenda_cols;               /* 1 = elenco con intestazioni */
    bool     dock_in_home;
    bool     rotazione_software;
    uint16_t ota_rollback_s;
} profilo_t;
```

La struttura qui sopra è **indicativa e incompleta**: mancano i quattro
corpi fuori scala di §4, le aree di tocco di §8, le voci HOME e
ingranaggio del rail, le schede della home e `dock_max`. La struttura vera
è quella che `tools/genera_profilo.py` emette leggendo `profili.json`, ed
è organizzata in sotto-strutture per schermata (`PRF->home.fascia_h`,
`PRF->cam.w_grande`) perché una struttura piatta con centocinquanta campi
non si legge.

**Nessun altro file sorgente deve contenere misure di layout.** Se un
componente ha bisogno di sapere quanto è largo il rail, lo chiede al
profilo. È il vincolo che permette a una sola base di codice di servire
schermi diversi senza divergere. Vale anche per le
misure **comuni** a tutti i profili — il minimo di 44 px, lo spessore dei bordi, le opacità:
stanno nella sezione `comuni` di `profili.json` e finiscono in
`profile.h`, non sparse nei sorgenti.

Il file `profili.json` allegato contiene le stesse tabelle in forma
leggibile da programma: usalo per generare `profile.h` invece di
trascrivere i numeri a mano.

---

## 11. Da dove vengono i numeri

Le tabelle di questo documento sono state ricontrollate sul CSS dei
mockup, che sono rendering 1:1 e quindi la misura vera.

`tools/estrai_misure.py` rilegge i mockup e confronta ogni misura con
`profili.json`, riportando tre casi: **divergenza** (uno dei due è
sbagliato), **divergenza voluta** (con il motivo scritto nello script) e
**deriva fra mockup** — la stessa misura disegnata in modo diverso in
revisioni successive, entro due pixel, dove vince questo documento.

Va rieseguito ogni volta che si tocca `profili.json` o un mockup:

```
python tools/estrai_misure.py
```

Quando una misura serve e non è in nessuna tabella, la si legge dal
mockup e **la si aggiunge qui e in `profili.json`**, non nel codice.
