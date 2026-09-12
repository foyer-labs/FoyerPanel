/* ------------------------------------------------------------------------
 * Il modello dei dati che l'interfaccia disegna.
 *
 * In Fase 1 lo riempie un finto fornitore (app/dati_finti.c) con l'impianto
 * vero di 04-config.example.json; in Fase 3 lo riempira il client WebSocket
 * di Home Assistant. L'interfaccia non sa quale dei due, e non deve saperlo:
 * e la ragione per cui questa e un'intestazione a se.
 *
 * Regola di onestà, da 01-specifica-ui.md: **non si mostra uno stato che il
 * sistema non conosce**. Un'entita che Home Assistant non ha si presenta
 * come non disponibile, non come spenta.
 * --------------------------------------------------------------------- */
#ifndef DATI_H
#define DATI_H

#include <stdbool.h>
#include <stdint.h>

/* --- da dove viene lo stato ---------------------------------------------
 *
 * La struttura viene da config.json (Fase 2), lo stato da Home Assistant
 * (Fase 3). Restava una domanda: cosa mostrare quando Home Assistant non
 * risponde.
 *
 * `dati_dal_vero()` e vero appena c'e un Home Assistant configurato, e da
 * quel momento **non si inventa piu niente**: un'entita di cui non e
 * arrivato nessun valore e "non disponibile", e i suoi comandi sono
 * disattivati. Mostrare un valore inventato su un pannello a muro
 * significherebbe far credere che una luce sia accesa perche di solito lo
 * e, ed e il difetto che questo progetto non fa.
 *
 * E falso soltanto quando nessuno ha configurato Home Assistant: nel
 * simulatore, per le catture e per i casi limite. Li i valori inventati
 * servono a guardare l'interfaccia, e non c'e nessuno a cui mentire.
 */
bool dati_dal_vero(void);

/* --- luci — 01-specifica-ui.md §3.1 ------------------------------------- */

typedef struct {
    const char *nome;
    const char *entita;        /* per mandare il comando a chi di dovere */
    bool        acceso;
    bool        dimmerabile;   /* senza, la scheda non ha la barra */
    uint8_t     percento;      /* significativo solo se dimmerabile */
    bool        disponibile;   /* falso: entita non trovata in Home Assistant */
} luce_t;

int           dati_luci(void);
const luce_t *dati_luce(int n);
int           dati_luci_accese(void);

/* --- comandi -------------------------------------------------------------
 *
 * Il pannello chiede, non decide. Queste funzioni mandano il servizio a
 * Home Assistant e tornano subito: falso vuol dire "non e nemmeno partito"
 * — collegamento assente, entita non disponibile — e va detto subito,
 * perche un'attesa che non finisce e la cosa peggiore che un pannello a
 * muro possa fare.
 *
 * **Il riscontro non arriva da qui.** Arriva quando lo stato dell'entita
 * cambia davvero e la schermata si ridisegna: e la differenza fra "l'ho
 * chiesto" e "e successo", e su un muro conta.
 */
bool dati_luce_accendi(int n, bool accesa);

/* --- comandare, non solo mostrare ---------------------------------------
 *
 * Aziona un accesso. Cosa voglia dire dipende dal tipo, e i due casi sono
 * davvero diversi:
 *
 * - **impulso**: accende e rilascia dopo `impulso_ms`, come un dito su un
 *   pulsante. Non c'e riscontro: il pannello non sa se il cancello si e
 *   aperto, e non deve fingere di saperlo.
 * - **interruttore**: commuta e segue lo stato vero.
 *
 * Falso se il comando non e partito — non collegati, entita mancante,
 * accesso non disponibile. Falso vuol dire "non e partito", mai "non ha
 * funzionato": quello, per un impulso, non si puo sapere. */
bool dati_accesso_aziona(int n);

/* Fa scadere i rilasci degli impulsi. Da chiamare spesso, come ha_gira():
   e l'unico pezzo del fornitore che ha bisogno di sapere che ora e. */
void dati_gira(uint32_t adesso_ms);

/* Scene: se non ce ne sono configurate la fascia non viene mostrata. */
int           dati_scene(void);
const char   *dati_scena(int n);
int           dati_scena_attiva(void);   /* -1 se nessuna */

/* --- clima — 01-specifica-ui.md §3.2 ------------------------------------
 *
 * L'impianto e fatto di due cose diverse e la sezione le tiene separate:
 * zone di riscaldamento, che hanno solo il setpoint, e condizionatori Gree,
 * che hanno modalita, ventilazione, deflettore e timer. Non condividono
 * nemmeno la scala delle temperature.
 *
 * Le temperature sono in decimi di grado, interi: sul pannello il virgola
 * mobile costa e non serve, e mezzo grado e il passo piu fine che l'impianto
 * accetta.
 */

typedef struct {
    /* La sua posizione nell'elenco: chi disegna un tasto deve poterla dire
       a chi lo ascoltera, quando il ciclo che l'ha disegnato e finito. */
    int         indice;
    const char *nome;
    const char *entita;        /* il climate.* della zona */
    int16_t     misurata;      /* decimi di grado; INT16_MIN = non nota */
    int16_t     richiesta;
    uint8_t     umidita;       /* percento; 0 = non nota                 */
    bool        chiama;        /* la zona sta davvero chiamando calore   */
    bool        accesa;        /* falso: nessuna richiesta               */
    bool        disponibile;
    /* Il piano a cui appartiene, come indice in dati_piani(). **-1 se non
       gliene e stato dato uno**: quelle zone non spariscono, finiscono in un
       gruppo loro — una zona che si smette di vedere e peggio di una zona in
       un gruppo sbagliato. */
    int         piano;
} zona_clima_t;

/* --- i piani dell'impianto ----------------------------------------------
 *
 * Le valvole delle singole zone aprono il circuito; l'accensione vera la
 * comanda un interruttore per piano. Sono due cose diverse e si vedono
 * separate: una zona puo chiedere calore quanto vuole, se l'interruttore del
 * suo piano e spento non scalda niente.
 *
 * `chiedono` conta le zone di questo piano che stanno chiamando **adesso**.
 * Serve alla fascia, e serve a dire la contraddizione: piano spento con
 * zone che chiedono e l'unica cosa, in tutto questo, che oggi non si
 * poteva sapere da nessuna parte del pannello — si scopriva toccando i
 * termosifoni. */
typedef struct {
    const char *id;            /* come lo chiamano le zone                */
    const char *nome;          /* come compare sul vetro                  */
    const char *entita;        /* l'interruttore                          */
    bool        acceso;
    bool        disponibile;   /* falso: entita assente o non risponde    */
    int         zone;          /* quante zone gli appartengono            */
    int         chiedono;      /* quante stanno chiamando calore adesso   */
} piano_t;

int            dati_piani(void);
const piano_t *dati_piano(int n);

/* Il piano che **questo** pannello governa, o -1 se non gliene e stato
   detto. Da qui discendono l'icona del riscaldamento nello standby e nella
   testata: un pannello e il termostato del suo piano, e dice lo stato di
   cio che comanda — non di tutta la casa. */
int            dati_piano_del_pannello(void);

/* Accende o spegne il riscaldamento di un piano. Il servizio lo decide il
   dominio dell'entita, come per gli interruttori. */
bool           dati_piano_accendi(int n, bool acceso);

typedef enum {
    MODO_SPENTO = 0, MODO_CALDO, MODO_FREDDO,
    MODO_AUTO, MODO_DEUMIDIFICA, MODO_VENTILATORE,
    MODO_QUANTI,
} modo_clima_t;

typedef enum {
    VENT_AUTO = 0, VENT_BASSA, VENT_MEDIO_BASSA,
    VENT_MEDIA, VENT_MEDIO_ALTA, VENT_ALTA,
    VENT_QUANTE,
} ventilazione_t;

/* Cinque altezze della lamella. Le dodici combinazioni reali di swing_modes
   si ottengono da queste per fisso o oscillante, piu default e full_swing:
   mostrare dodici tasti sarebbe illeggibile. */
typedef enum {
    DEFL_ALTO = 0, DEFL_MEDIO_ALTO, DEFL_CENTRO, DEFL_MEDIO_BASSO, DEFL_BASSO,
    DEFL_QUANTE,
} deflettore_t;

typedef enum {
    OSC_FISSO = 0, OSC_OSCILLANTE, OSC_TUTTA, OSC_PREDEFINITO,
} oscillazione_t;

typedef struct {
    const char    *nome;
    const char    *entita;         /* il climate.* dell'unita */
    modo_clima_t   modo;
    int16_t        stanza;         /* decimi di grado */
    int16_t        richiesta;
    ventilazione_t ventilazione;
    deflettore_t   deflettore;
    oscillazione_t oscillazione;
    bool           disponibile;

    /* Timer e automazione sono due entita indipendenti: l'interruttore
       "Spegni al timer" agisce sull'automazione, non sul timer. La riga
       compare solo se la configurazione ha entrambi i campi. */
    bool     ha_timer;
    bool     timer_corre;
    bool     automazione_attiva;   /* falso: allo scadere non succede nulla */
    uint16_t durata_min;           /* durata impostata, in minuti           */
    /* Quanto manca, in minuti, **calcolato** dall'istante di fine e non
       letto: Home Assistant tiene fermo l'attributo `remaining` al valore
       d'inizio finche il timer corre, e chi lo mostrasse farebbe vedere un
       conto alla rovescia che non scende mai. */
    uint16_t restano_min;
    const char *spegne_alle;       /* "20:12", ora locale, gia formattata  */

    /* La barra del conto alla rovescia, e il numero che le da senso.
     *
     * `timer_totale_min` e la durata di **questo** conteggio, dall'attributo
     * `duration` del timer — non `durata_min`, che e' cio' che il pannello
     * userebbe se lo avviasse lui. Un timer avviato dall'app con tre ore,
     * diviso per l'ora e mezza della configurazione, darebbe una barra piu'
     * lunga della barra.
     *
     * `timer_quota_pct` e' quanto **manca** in centesimi, gia' calcolato qui
     * perche' la regola su quando non disegnare la barra deve stare in un
     * posto solo. **Zero vuol dire non disegnarla**: senza durata non si sa
     * rispetto a cosa riempirla, e una barra che finge di misurare e' peggio
     * di nessuna barra — la stessa regola dell'assorbimento di lavatrice e
     * asciugatrice. */
    uint16_t timer_totale_min;
    uint8_t  timer_quota_pct;

    bool extra[4];                 /* silenzioso, luce pannello, aria fresca, xfan */
    bool ha_extra[4];
} condizionatore_t;

/* Limiti distinti per gruppo: sono due impianti diversi e non condividono
   la scala (01-specifica-ui.md §3.2). In Fase 2 arrivano da config.json. */
typedef struct { int16_t min, max, passo; } limiti_t;

int                     dati_zone_clima(void);
const zona_clima_t     *dati_zona_clima(int n);
int                     dati_zone_in_richiesta(void);
limiti_t                dati_limiti_riscaldamento(void);

/* --- comandare il clima -------------------------------------------------
 *
 * Le temperature sono in decimi di grado come dappertutto qui, e vengono
 * **limitate** ai valori del gruppo prima di partire: un setpoint fuori
 * scala non lo rifiuta il pannello per pignoleria, lo rifiuta l'impianto —
 * e allora il comando parte, fallisce, e chi guarda non capisce perche.
 *
 * Tutte tornano falso se il comando non e partito. Falso non vuol dire "non
 * ha funzionato": vuol dire che non e nemmeno uscito di qui. */
bool dati_zona_clima_imposta(int n, int16_t decimi);
bool dati_zona_clima_accendi(int n, bool accesa);

bool dati_condizionatore_imposta(int n, int16_t decimi);
bool dati_condizionatore_modo(int n, modo_clima_t modo);
bool dati_condizionatore_ventilazione(int n, ventilazione_t v);
bool dati_condizionatore_deflettore(int n, deflettore_t pos,
                                    oscillazione_t osc);

/* `quale` e 0..3 nell'ordine di `extra[]`: silenzioso, luce pannello, aria
   fresca, xfan. Falso anche se quell'extra non e configurato. */
bool dati_condizionatore_extra(int n, int quale, bool acceso);

/* L'interruttore "Spegni al timer" agisce sull'**automazione**, non sul
   timer: sono due entita indipendenti, e 01-specifica-ui.md lo dice
   esplicitamente perche e il posto in cui e piu facile sbagliarsi. */
bool dati_condizionatore_automazione(int n, bool attiva);

/* Allunga o accorcia di `minuti` (negativi per accorciare) il timer che
   **sta correndo**.

   Solo quello che sta correndo: `timer.change` in Home Assistant si
   applica a un timer attivo e su uno fermo da errore. Percio a timer
   fermo questa torna falso senza mandare niente, e la schermata mostra i
   due pulsanti spenti invece che finti — un pulsante che non si puo
   premere dice qualcosa, uno che si preme e non fa niente no.

   La durata di partenza resta quella di `durata_predefinita` in
   configurazione: da qui non si cambia, si allunga quella in corso. */
bool dati_condizionatore_timer_regola(int n, int16_t minuti);

/* Avvia il timer per `minuti`, e lo annulla. Sono le due meta che
   mancavano: senza avvio il riquadro del timer si poteva solo guardare, e i
   due pulsanti +/- restavano spenti per sempre perche `timer.change` vuole
   un timer che gia corre. Dal pannello il timer non si poteva far partire,
   e chi lo avviava era sempre qualcun altro. */
bool dati_condizionatore_timer_avvia(int n, uint16_t minuti);
bool dati_condizionatore_timer_annulla(int n);

int                     dati_condizionatori(void);
const condizionatore_t *dati_condizionatore(int n);
int                     dati_condizionatori_accesi(void);
limiti_t                dati_limiti_condizionatori(void);

int16_t                 dati_temperatura_esterna(void);

/* Nomi da mostrare. Stanno qui e non nella schermata perche sono gli stessi
   nella griglia, nella colonna e nel dettaglio. */
const char *dati_modo_nome(modo_clima_t m);
/* Come sopra, ma per i sei riquadri affiancati del dettaglio, dove il nome
   per esteso andrebbe a capo e la seconda riga finirebbe fuori dal
   riquadro. Solo li: nella pastiglia della modalita corrente c'e spazio e
   il nome resta intero. */
const char *dati_modo_nome_corto(modo_clima_t m);
const char *dati_ventilazione_nome(ventilazione_t v);
const char *dati_deflettore_nome(deflettore_t d);
const char *dati_extra_nome(int n);

#define TEMP_IGNOTA INT16_MIN

/* --- energia — 01-specifica-ui.md §3.3 ----------------------------------
 *
 * Regola obbligatoria dei segni: il pannello **non mostra mai un numero
 * negativo all'utente**. Mostra il valore assoluto e cambia la parola.
 * Sotto i 50 W un valore e considerato nullo, cosi la batteria non alterna
 * fra carica e scarica a ogni aggiornamento.
 *
 * Il consumo di casa non e un sensore: si calcola come
 * produzione + rete - batteria, con i segni della tabella.
 */
#define ENERGIA_SOGLIA_W 50

typedef enum { VERSO_NULLO = 0, VERSO_POSITIVO, VERSO_NEGATIVO } verso_t;

typedef struct {
    const char *nome;
    uint16_t    tensione_decimi;  /* volt per dieci */
    uint16_t    corrente_centesimi;
    int32_t     potenza_w;        /* -1 = da calcolare come V x I */
    bool        disponibile;
} stringa_t;

typedef struct {
    int32_t produzione_w;
    int32_t rete_w;          /* positivo = prelievo, negativo = immissione */
    int32_t batteria_w;      /* positivo = carica, negativo = scarica      */
    uint8_t batteria_pct;
    int32_t oggi_wh;
    bool    disponibile;
} energia_t;

/* --- i dispositivi monitorati ------------------------------------------
 *
 * Una presa di cui si conosce il consumo. Il pannello li tiene **ordinati
 * dal piu affamato al meno**, e l'ordine e il dato: la domanda davanti a un
 * pannello non e «quanto consuma il forno», e «chi mi sta mangiando la
 * corrente». Un elenco in ordine di configurazione la lascerebbe senza
 * risposta anche mostrando gli stessi numeri.
 *
 * `quota_pct` e la fetta sul totale dei monitorati, non sul consumo di
 * casa: la somma delle prese non fa il consumo della casa — mancano le
 * luci, le prese non monitorate, tutto il resto — e dire «43% della casa»
 * sarebbe un numero sbagliato con l'aria di essere giusto. */
typedef struct {
    const char *nome;
    int32_t     watt;
    uint8_t     quota_pct;    /* sul totale dei dispositivi monitorati */
    bool        disponibile;
} dispositivo_t;

/* --- i totali della giornata --------------------------------------------
 *
 * Energia, non potenza: rispondono a «oggi quanto», mentre i numeri in alto
 * rispondono a «adesso quanto». Ognuno puo mancare, e chi manca non si
 * disegna — meglio tre riquadri che quattro di cui uno mente. */
typedef struct {
    int32_t prodotto_wh, consumato_wh, prelevato_wh, immesso_wh;
    bool    prodotto_c_e, consumato_c_e, prelevato_c_e, immesso_c_e;
} giornata_t;

energia_t        dati_energia(void);
int32_t          dati_energia_casa_w(void);   /* calcolato, non un sensore */
verso_t          dati_verso(int32_t w);       /* con la soglia dei 50 W    */
int              dati_stringhe(void);
const stringa_t *dati_stringa(int n);
int32_t          dati_stringa_potenza_w(int n);  /* V x I se non dichiarata */

/* Gia ordinati: dati_dispositivo(0) e quello che consuma di piu. */
int                  dati_dispositivi(void);
const dispositivo_t *dati_dispositivo(int n);
int32_t              dati_dispositivi_totale_w(void);
giornata_t           dati_giornata(void);

/* Storico della giornata per il grafico: 24 valori orari in watt. */
int     dati_storico_punti(void);
/* Il valore dell'ora `n`, oppure **-1 se di quell'ora non si sa niente**.
   Diverso da zero, e chi disegna deve distinguerli: un impianto fermo e
   un'ora senza dati si assomigliano solo finche non si guarda il grafico
   alle sei del mattino. */
int32_t dati_storico(int n);
/* Vero quando la curva viene davvero da Home Assistant. Falso mentre si
   aspetta la prima risposta, e falso se il recorder non ha statistiche per
   quel sensore: chi disegna deve poterlo dire invece di mostrare il vuoto
   come se fosse una giornata senza sole. */
bool    dati_storico_vero(void);
bool    dati_storico_negato(void);

/* --- accessi — 01-specifica-ui.md §3.5 ----------------------------------
 *
 * Tre comandi a impulso piu le luci del giardino a interruttore. Il pannello
 * invia un impulso e **non sa** se il cancello si e aperto: nessuna riga
 * dichiara "chiuso" senza un sensore che lo affermi. Solo la porta del garage ha
 * un sensore vero, ed e l'unico che puo dire come sta.
 */
typedef enum { ACC_IMPULSO = 0, ACC_INTERRUTTORE } tipo_accesso_t;

typedef struct {
    /* La sua posizione nell'elenco, come la chiama chi comanda. Sta nella
       scheda e non solo nell'indice del ciclo perche chi disegna un
       pulsante deve poterlo dire a chi lo ascoltera piu tardi, quando il
       ciclo e finito da un pezzo. */
    int            indice;
    const char    *id;
    const char    *nome;
    tipo_accesso_t tipo;
    bool           conferma;      /* con conferma: TIENI PREMUTO         */
    bool           ha_sensore;    /* solo la porta del garage            */
    /* Il sensore c'e ma sta rispondendo? Sono due cose diverse: un sensore
       configurato e momentaneamente non disponibile non autorizza a dire
       "chiusa". Senza questo campo, "non lo so" e "e chiusa" sarebbero
       indistinguibili, che e proprio il difetto che questa sezione evita. */
    bool           stato_noto;
    bool           aperto;        /* significativo solo se stato_noto    */
    bool           acceso;        /* solo per tipo interruttore          */
    const char    *ultimo_uso;    /* "17:58", o NULL                     */
    bool           disponibile;
} accesso_t;

int              dati_accessi(void);
const accesso_t *dati_accesso(int n);

/* Registro degli eventi in fondo alla colonna dei comandi. */
int         dati_eventi(void);
const char *dati_evento_ora(int n);
const char *dati_evento_testo(int n);

/* --- agenda — 01-specifica-ui.md §3.6 -----------------------------------
 *
 * In Home Assistant non esiste nessuna entita calendar, quindi la sezione
 * resta nascosta. Il codice c'e lo stesso: quando un calendario comparira
 * bastera accendere `agenda.attiva`, e questa e la differenza fra una
 * sezione non configurata e una sezione non scritta.
 */
typedef struct {
    uint8_t     giorno;        /* 0 = oggi, 1 = domani, ...      */
    const char *ora;           /* "19:30", o NULL per tutto il giorno */
    const char *titolo;
    const char *calendario;
    uint8_t     colore;        /* 0..3, nell'ordine dei calendari */
    bool        passato;
} evento_t;

int             dati_eventi_agenda(void);
const evento_t *dati_evento_agenda(int n);
int             dati_calendari(void);
const char     *dati_calendario(int n);
bool            dati_agenda_attiva(void);
const char     *dati_giorno_nome(uint8_t giorno);

#include "segreti.h"

/* --- condivisione Wi-Fi — 01-specifica-ui.md §3.7 -----------------------*/
typedef struct {
    const char *nome_mostrato;
    const char *ssid;
    /* Non la password: **quale** segreto la contiene. La password sta in
       NVS e non passa da qui, altrimenti basterebbe una struttura copiata
       nel posto sbagliato per farla finire dove non deve. Chi la mostra la
       prende con segreti_usa(), per il tempo di comporre una stringa. */
    segreto_t   segreto;
    const char *sicurezza;     /* "WPA" */
    bool        nascosta;
    bool        mostra_password;
    bool        attiva;
} rete_t;

/* Le reti condivisibili, da zero a RETI_MAX.

   **Nessuna e speciale.** C'era una rete «ospiti» tenuta a parte dalle
   «private», e la divisione veniva da come la funzione era nata — prima una
   sola, poi un elenco per le altre — non da qualcosa che esista in casa. Sul
   vetro diventava due schermate per la stessa cosa, con un pulsante «torna
   agli ospiti» che dava a quella rete un ruolo che non ha. Adesso e un
   elenco, e la prima e solo la prima. */
int    dati_reti(void);
rete_t dati_rete(int n);

/* --- riassunto della home — 01-specifica-ui.md §2 -----------------------
 *
 * Le schede della home riassumono quello che le sezioni mostrano per
 * intero. I dati arrivano da **sensori aggregati**, uno per scheda, con
 * tutto negli attributi: e la stessa scelta gia fatta per l'energia in
 * 03-config-contratto.md §4, e per lo stesso motivo — cinque numeri che
 * cambiano insieme non meritano cinque sottoscrizioni, e chi scrive il
 * template in Home Assistant sa quali entita guardare meglio del pannello.
 *
 * Se il sensore non c'e, la scheda resta vuota con una riga che lo dice.
 * Non si riempie con quello che si potrebbe dedurre: dedurre chi e in casa
 * dalla somma di quattro `person` sarebbe un'altra funzione da tenere
 * allineata a Home Assistant, e sbagliata il giorno che qualcuno aggiunge
 * un ospite.
 */

typedef struct {
    const char *nome;
    bool        in_casa;
    /* "dalle 17:40" per chi c'e, "rientro stimato 19:15" per chi no. Lo
       compone il template, non il pannello: e il template a sapere se quel
       rientro si puo stimare. */
    const char *quando;
} persona_t;

int              dati_persone(void);
const persona_t *dati_persona(int n);
int              dati_persone_in_casa(void);
bool             dati_presenza_nota(void);

/* --- il robot lavapavimenti — 01-specifica-ui.md §3.9 --------------------
 *
 * Lo stato lo normalizza Home Assistant in poche parole uguali per tutti gli
 * aspirapolvere, e sono quelle che si traducono qui. La frase del
 * costruttore — «lavaggio del panno», «svuotamento» — arriva a parte in
 * `dettaglio`, perche' dice cose che lo stato non dice e cambia da un
 * aggiornamento all'altro: mostrarla si, farci dipendere il comportamento
 * no.
 */
typedef enum {
    ROB_IGNOTO = 0,   /* entita assente, indisponibile, o stato mai visto  */
    ROB_FERMO,        /* acceso e in attesa, fuori dalla base              */
    ROB_PULISCE,
    ROB_RIENTRA,
    ROB_ALLA_BASE,
    ROB_IN_PAUSA,
    ROB_ERRORE,
} robot_stato_t;

typedef struct {
    bool          disponibile;   /* falso: l'entita non c'e o non risponde */
    robot_stato_t stato;
    /* Mai NULL, spesso vuota: la frase che il robot dice di se. */
    const char   *dettaglio;
    bool          batteria_c_e;
    int8_t        batteria;      /* 0-100, valido solo con batteria_c_e    */
    bool          in_carica;
} robot_t;

/* Una stanza della mappa del robot. `numero` e quello che il robot si e dato
   da solo; `nome` e la posizione li mette chi configura, perche' sono le due
   cose che solo chi abita li puo sapere.
   Colonna e riga partono da 1 su una griglia di 6x4; zero vuol dire "non
   collocata", e chi disegna la mette in coda invece di perderla. */
typedef struct {
    int         numero;
    const char *nome;
    uint8_t     col, riga, larghezza, altezza;
} robot_stanza_t;

/* --- avvisi e manutenzione del robot ------------------------------------
 *
 * Quello che l'applicazione del robot dice e il pannello taceva: il
 * serbatoio dell'acqua pulita da controllare, quella sporca da svuotare, il
 * panno che non e agganciato, il filtro da cambiare.
 *
 * **Le frasi non le scrive il pannello.** Le due entita d'errore — quella
 * dell'aspirapolvere e quella della base — hanno per stato il messaggio
 * stesso, gia in italiano: «Il sensore Hall dell'acqua pulita si e
 * attivato, controllare il serbatoio dell'acqua pulita». Riscriverlo qui
 * vorrebbe dire una seconda traduzione che invecchia da sola, e dire una
 * cosa diversa da quella che dice il telefono in mano a chi guarda.
 *
 * Per i sensori a due stati invece la frase serve, perche' `on` non e una
 * frase; e serve anche sapere **quale dei due stati e l'allarme**, che non
 * si puo indovinare: `dock_dirty_water_box` acceso vuol dire «svuotalo»,
 * ma `panno_attaccato` **spento** vuol dire «non c'e il panno». Un verso
 * sbagliato direbbe che manca il panno proprio quando c'e. */
typedef struct {
    const char *testo;        /* la frase, sua o configurata. Mai NULL   */
    bool        attivo;       /* c'e qualcosa da guardare adesso         */
    bool        disponibile;  /* l'entita risponde                       */
} robot_avviso_t;

/* Un consumabile: spazzole, filtro, sensori. Il valore e il tempo che
   resta, e arriva gia con la sua unita da Home Assistant — il pannello non
   sa quante ore duri una spazzola, e non deve saperlo. */
typedef struct {
    const char *nome;
    const char *valore;       /* "214 h", gia composto. Mai NULL         */
    bool        agli_sgoccioli; /* sotto la soglia dichiarata            */
    bool        disponibile;
} robot_manutenzione_t;

int                         dati_robot_avvisi(void);
const robot_avviso_t       *dati_robot_avviso(int n);
/* Quanti ne sono accesi adesso. E il numero che decide se in home e nella
   testata compare il triangolo, e quindi l'unico che chi disegna chiede
   quando non sta disegnando la vista degli avvisi. */
int                         dati_robot_avvisi_accesi(void);

int                         dati_robot_manutenzioni(void);
const robot_manutenzione_t *dati_robot_manutenzione(int n);
int                         dati_robot_manutenzioni_agli_sgoccioli(void);

/* --- lavatrice e asciugatrice -------------------------------------------
 *
 * Due apparecchi che passano la giornata fermi e ogni tanto lavorano, e la
 * home dice quale delle due cose stanno facendo. Lo stato viene da
 * un'entita che qualcuno in Home Assistant accende quando il ciclo parte —
 * un input_boolean, di solito — e non dall'assorbimento: una lavatrice in
 * ammollo consuma quanto una spenta, e dedurre il ciclo dai watt vorrebbe
 * dire dire «finita» a meta lavaggio.
 *
 * `quota_pct` e l'assorbimento rispetto ai watt di pieno carico dichiarati.
 * Senza quel numero resta a zero e la barretta non si disegna: una barra che
 * non sa rispetto a cosa riempirsi mentirebbe con l'aria di misurare. */
typedef struct {
    const char *nome;
    bool        disponibile;   /* falso: entita assente o non risponde     */
    bool        in_ciclo;
    bool        potenza_c_e;
    int32_t     watt;
    uint8_t     quota_pct;     /* 0 = non si sa il pieno carico            */
    /* Quale icona disegnare. Lo dice la **posizione** nell'elenco, non
       il nome: indovinare «asciugatrice» da una stringa che chi
       configura puo scrivere come vuole e un indovinello, e sbaglierebbe
       il giorno che qualcuno scrive «Asciug.». */
    bool        asciuga;
} elettrodomestico_t;

/* --- interruttori -------------------------------------------------------
 *
 * Prese, luci smart, scaldini, pompe: cio che ha due stati e nient'altro.
 * Le luci di casa hanno una sezione loro perche hanno luminosita e scene;
 * qui c'e quello che si accende e si spegne, e proprio per questo non
 * meritava di essere modellato due volte.
 *
 * `icona` e il **nome** scritto in configurazione — "outlet", "mode_fan" —
 * e non il glifo. La traduzione la fa chi disegna, con icona_da_nome():
 * qui non si entra, perche i glifi stanno in icons.h che vuole LVGL, e i
 * fornitori di dati si compilano anche dove LVGL non c'e — nelle prove, per
 * dirne una. Un fornitore che tira dentro l'interfaccia grafica per una
 * stringa non e un fornitore.
 *
 * `potenza_c_e` distingue **«non ha un sensore»** da «assorbe zero watt»,
 * che sul vetro sono due cose diverse: la prima non si scrive, la seconda
 * si scrive «0 W». Una presa spenta che misura davvero zero e
 * un'informazione; una presa che non misura niente non lo e. */
typedef struct {
    const char *nome;
    const char *entita;
    const char *icona;         /* il nome, non il glifo: vedi sopra       */
    bool        disponibile;   /* falso: entita assente o non risponde    */
    bool        acceso;
    bool        potenza_c_e;
    int32_t     watt;
} interruttore_t;

int                    dati_interruttori(void);
const interruttore_t  *dati_interruttore(int n);

/* Accende o spegne. Il servizio lo decide il dominio dell'entita —
   switch., light., input_boolean. — e non la configurazione: chiederlo due
   volte vuol dire poterlo sbagliare la seconda. */
bool                   dati_interruttore_premi(int n, bool acceso);

/* --- programmazioni — 01-specifica-ui.md §3.10 --------------------------
 *
 * Le cose che vanno a orario. Un gruppo tiene insieme quello che in Home
 * Assistant sta in una scheda sola: il dispositivo che si accende, le
 * **finestre** — coppie di `input_datetime`, accensione e spegnimento — e
 * le **automazioni** che quelle finestre le fanno scattare.
 *
 * Il pannello non tiene una copia degli orari: li legge dalle entita e ce
 * li riscrive. Sono gli stessi che si vedono nella dashboard di Home
 * Assistant, e cambiarli da una parte li cambia dall'altra — che e tutto il
 * punto di rispecchiare quella logica invece di inventarne una seconda.
 *
 * `minuti` e minuti dalla mezzanotte, non "HH:MM": confrontare due orari
 * per sapere se la finestra e in corso adesso vuol dire fare un conto, e un
 * conto su due interi non sbaglia mai. La traduzione in testo la fa chi
 * disegna.
 *
 * `validita` non e un dettaglio di stile. Una finestra normale dice «qui si
 * accende»; una di validita dice «qui un'altra automazione ha il permesso
 * di scattare» — la luce a movimento del corridoio. Scrivere «si accende alle
 * 22:00» per una cosa che alle 22:00 non si accende e la bugia piu facile
 * da stampare. */
#define ORARIO_IGNOTO (-1)

typedef struct {
    const char *accensione;    /* l'entita input_datetime                 */
    const char *spegnimento;
    int16_t     da_min;        /* minuti dalla mezzanotte, o ORARIO_IGNOTO */
    int16_t     a_min;
    bool        validita;      /* non accende: da il permesso             */
    bool        in_corso;      /* adesso siamo dentro questa finestra     */
} finestra_t;

typedef struct {
    const char *entita;
    const char *nome;          /* dalla configurazione o da Home Assistant */
    bool        attiva;        /* l'automazione e abilitata               */
    bool        disponibile;
    const char *ultimo_scatto; /* "18:32", "ieri 07:00", o NULL           */
} automazione_t;

typedef struct {
    const char *nome;
    const char *icona;         /* il nome, non il glifo                   */
    const char *entita;        /* cio che si accende, o NULL              */
    bool        c_e_entita;
    bool        acceso;
    bool        disponibile;
    bool        potenza_c_e;
    int32_t     watt;
    int         finestre;
    int         automazioni;
} programmazione_t;

int                     dati_programmazioni(void);
const programmazione_t *dati_programmazione(int n);
const finestra_t       *dati_finestra(int gruppo, int n);
const automazione_t    *dati_automazione(int gruppo, int n);

/* Accende o spegne il dispositivo del gruppo. Come per gli interruttori, il
   servizio lo decide il dominio dell'entita. */
bool dati_programmazione_premi(int gruppo, bool acceso);

/* Abilita o disabilita un'automazione: `automation.turn_on` / `turn_off`.
   Gli orari restano scritti dove sono — si riaccende e ricomincia. */
bool dati_automazione_abilita(int gruppo, int n, bool attiva);

/* Scrive un orario: `input_datetime.set_datetime` con il solo campo `time`,
   perche questi helper sono tutti has_date: false. `capo` vero e
   l'accensione, falso lo spegnimento. */
bool dati_finestra_imposta(int gruppo, int n, bool capo, int minuti);

int                       dati_elettrodomestici(void);
const elettrodomestico_t *dati_elettrodomestico(int n);

robot_t               dati_robot(void);
int                   dati_robot_stanze(void);
const robot_stanza_t *dati_robot_stanza(int n);

/* I comandi. Falso se il comando non e nemmeno partito — entita non
   configurata, o Home Assistant non collegato. */
/* Scrive in `fuori` i dati del servizio per la pulizia delle stanze, senza
   mandarli. Comporre e spedire sono due cose distinte perche' la forma di
   questi parametri dipende dall'integrazione, e vederla e l'unico modo di
   provarla: dedurla ha gia sbagliato due volte. */
bool dati_robot_comando_stanze_json(const int *numeri, int quanti,
                                    const char *forma_imposta,
                                    char *fuori, size_t max);

bool dati_robot_manda_json(const char *service_data);
bool dati_robot_pulisci(const int *numeri, int quanti);
bool dati_robot_tutto(void);
bool dati_robot_alla_base(void);

typedef struct {
    const char *nome;      /* "Cucina"          */
    const char *da;        /* "da 35 minuti"    */
} apertura_t;

int               dati_aperture(void);
const apertura_t *dati_apertura(int n);
bool              dati_aperture_note(void);

typedef struct {
    const char *stato;         /* "sunny", "cloudy", ... come Home Assistant */
    int16_t     temperatura;   /* decimi di grado; TEMP_IGNOTA se non nota   */
    bool        disponibile;
} meteo_t;

meteo_t dati_meteo(void);

/* --- la schermata di standby — 01-specifica-ui.md §4.7 -------------------
 *
 * Il pannello sta al posto di un termostato, e questa e la schermata che
 * si vede quasi tutto il tempo in cui e acceso. Percio il numero grande
 * non e piu solo l'ora: sono due, l'ora e la temperatura della stanza,
 * della stessa taglia.
 *
 * Ogni campo ha il suo `c_e`, e non e pignoleria: qui **niente si
 * inventa**. Una temperatura che non arriva non diventa un trattino in
 * mezzo agli altri numeri, diventa un pezzo di schermata che non c'e. Un
 * segnaposto che indovina e peggio di uno che sbaglia, e su una schermata
 * che si guarda di sfuggita passando in corridoio nessuno lo verificherebbe
 * mai.
 *
 * Da dove viene ogni pezzo:
 *
 *   interna       `standby/temperatura_interna` — entita, e se serve
 *                 l'attributo: quale dei due porti il valore cambia da
 *                 sensore a sensore, e lo si dice in configurazione invece
 *                 di indovinarlo qui
 *   esterna       il sensore locale di `meteo`, che gia esisteva e gia
 *                 vinceva sul servizio meteo; se non c'e, la temperatura
 *                 del servizio
 *   meteo         la condizione, dall'entita weather: la parola accanto al
 *                 numero, non il numero
 *   chiesta       `standby/clima` — il setpoint della zona di questa
 *                 stanza. Senza, non compare
 *   scalda        hvac_action della stessa entita
 *   energia       la sezione `energia`, la stessa della home: un secondo
 *                 posto dove dire quali sensori sono avrebbe potuto dire
 *                 due cose diverse sullo stesso vetro
 *   aperture      quante ne risultano aperte adesso. Zero non e un dato da
 *                 mostrare: e il motivo per cui il riquadro non c'e
 */
typedef struct {
    bool        interna_c_e;
    int16_t     interna;         /* decimi di grado                        */
    const char *stanza;          /* "in salotto". Mai NULL                 */

    bool        esterna_c_e;
    int16_t     esterna;         /* decimi di grado                        */
    const char *condizione;      /* "sunny", "cloudy"... "" se non nota    */

    bool        chiesta_c_e;
    int16_t     chiesta;         /* decimi di grado                        */
    bool        scalda;          /* questa zona sta chiamando adesso       */
    /* L'interruttore del piano che **questo pannello** governa. E un'altra
       cosa da `scalda`: quella dice che la stanza chiede calore, questa che
       l'impianto e abilitato. Con l'interruttore spento nessuna zona scalda,
       per quanto chieda. Falso anche quando il pannello non sa quale piano
       governa: senza saperlo non puo dire niente di vero. */
    bool        riscaldamento_acceso;

    bool        energia_c_e;
    int32_t     sole_w;
    int32_t     casa_w;
    bool        batteria_c_e;    /* separata: la casa puo non avere batteria */
    uint8_t     batteria_pct;

    int         aperture;        /* 0 = niente da dire                     */
    const char *apertura_nome;   /* la prima aperta. Mai NULL              */
} standby_t;

standby_t dati_standby(void);

/* --- diagnostica — 10-diagnostica.md ------------------------------------
 *
 * Il pannello e incassato a muro: niente tastiera, niente porta seriale
 * raggiungibile. Se un giorno smette di aggiornare le telecamere o si
 * riavvia da solo, deve poterlo raccontare.
 *
 * Il registro non contiene mai token, password, SSID o l'URL completo di
 * una telecamera: l'endpoint e raggiungibile da chiunque sia sulla rete
 * locale, e un token finito in un registro e un token compromesso.
 */
typedef enum { LOG_ERRORE = 0, LOG_AVVISO, LOG_INFO, LOG_DEBUG } livello_t;

typedef struct {
    const char *ora;
    livello_t   livello;
    const char *sorgente;
    const char *messaggio;
} riga_log_t;

typedef struct {
    uint32_t accensione_s;
    uint16_t riavvii;
    const char *motivo_ultimo_riavvio;   /* il dato piu prezioso */
    uint32_t heap_libero, heap_minimo;
    uint32_t psram_libera;
    uint8_t  fps;
    int8_t   wifi_rssi;
    const char *ha_stato;
    uint16_t ha_ultimo_dato_s;
    uint32_t comandi_inviati, comandi_falliti;
    uint16_t errori_recenti;
} contatori_t;

contatori_t       dati_contatori(void);
int               dati_log(void);
const riga_log_t *dati_log_riga(int n);

/* --- casi limite — 11-collaudo.md §2 ------------------------------------
 * Il finto fornitore sa mettersi nelle condizioni che il collaudo elenca.
 * Sul pannello questa parte non esiste.
 */
typedef enum {
    CASO_NORMALE = 0,
    CASO_ZERO_LUCI,        /* nessuna zona configurata            */
    CASO_UNA_LUCE,         /* una sola: la scheda non si allarga  */
    CASO_TUTTE_SPENTE,     /* "tutte spente", non "0 luci accese" */
    CASO_ZERO_SCENE,       /* la fascia scene non compare         */
    CASO_NOMI_LUNGHI,      /* 40 caratteri, accenti e apostrofi   */
    CASO_NON_DISPONIBILE,  /* entita che Home Assistant non ha    */
    CASO_TEMP_IGNOTA,      /* "—" e i tasti +/- disattivati       */
    CASO_TIMER_SENZA_AUTO, /* il conto scorre ma non succedera nulla */
    CASO_SETPOINT_AL_LIMITE, /* i tasti oltre il limite non rispondono */
    CASO_NOTTE,            /* produzione a zero: "nessuna produzione"  */
    CASO_BATTERIA_FERMA,   /* fra -50 e +50 W: "ferma", senza alternare */
    CASO_CORRENTE_ZERO,    /* stringa a 0 A: 0 W, non una divisione rotta */
    CASO_AGENDA,           /* un calendario c'e: la sezione compare     */
    CASO_TITOLI_LUNGHI,    /* evento lunghissimo: due righe, poi tronca */
    CASO_SSID_EMOJI,       /* nome di rete con spazi ed emoji           */
    CASO_APERTURA,         /* una finestra aperta: la pastiglia compare */
    CASO_QUANTI,
} caso_t;

void        dati_caso(caso_t c);

/* Rilegge dalla configurazione tutto cio che ne viene: quali zone,
   quali unita, quali accessi. Lo stato inventato
   riparte da dov'era, perche cambiare il nome di una luce non deve
   spegnerla. */
void        dati_ricarica(void);
caso_t      dati_caso_corrente(void);
const char *dati_caso_nome(caso_t c);
caso_t      dati_caso_da_nome(const char *nome);

#endif /* DATI_H */
