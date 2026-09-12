/* ------------------------------------------------------------------------
 * Luci — 01-specifica-ui.md §3.1.
 *
 *   fascia scene in alto, fino a scene_max riquadri; se non ci sono scene
 *   configurate la fascia non viene mostrata
 *   griglia di zone; colonne, righe e capienza dal profilo (§5)
 *   oltre la capienza si impagina: frecce in testata, pallini in basso
 *
 * Nell'ultima pagina i posti liberi restano **vuoti e tratteggiati**: le
 * schede non si allargano, cosi la posizione di ogni zona non cambia mai fra
 * una pagina e l'altra.
 * --------------------------------------------------------------------- */
#include <stdint.h>

#include "comuni.h"
#include "sorveglianza.h"
#include "tempi.h"
#include "dati.h"
#include "schermate.h"
#include "ui.h"
#include "widgets/interruttore.h"
#include "widgets/paginatore.h"
#include "widgets/posto_vuoto.h"

static int pagina;

static void su_pagina(int p)
{
    pagina = p;
    ui_vai(SEZ_LUCI);   /* si ricostruisce: niente transizioni di pagina */
}

/* --- fascia delle scene ------------------------------------------------- */

static void scene(lv_obj_t *c)
{
    int quante = dati_scene();
    if (quante == 0) return;              /* nessuna scena, nessuna fascia */
    if (quante > PRF->griglia.scene_max) quante = PRF->griglia.scene_max;

    lv_obj_t *f = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, PRF->tocco.apertura.h);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    const int attiva = dati_scena_attiva();

    for (int n = 0; n < quante; n++) {
        const bool scelta = n == attiva;
        lv_obj_t *b = ui_pannello(f, scelta ? C_ACC : C_CARD2);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, LV_PCT(100));
        lv_obj_set_style_min_width(b, 0, 0);
        lv_obj_set_style_radius(b, PRF->geo.radius_tile, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, scelta ? C_ACC : C_LINE);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *l = ui_testo(b, dati_scena(n), scelta ? C_INK : C_DIM, FT_M);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
}

/* Il dito su un interruttore: si chiede, non si decide. Lo stato a schermo
   non si tocca — cambiera quando Home Assistant dira che e cambiato — e
   cosi non capita di vedere una luce accesa che non si e accesa. */
static void su_interruttore(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    const luce_t *z = dati_luce(n);
    if (!z) return;

    dati_luce_accendi(n, !z->acceso);
    tempi_risveglia();

    /* Senza Home Assistant il fornitore ha cambiato lo stato inventato: si
       ridisegna, se no il simulatore sembrerebbe rotto. Col collegamento
       vero il ridisegno arriva dall'evento, e questo non fa danno. */
    ui_vai_a(SEZ_LUCI, ui_vista());
}

/* --- una zona ------------------------------------------------------------
 *
 * La scheda e **una riga**: lampadina, nome, stato. Prima era una colonna
 * alta un quarto di schermo — nome in cima, lampadina al centro di un
 * vuoto, stato in fondo — e tredici zone volevano due pagine.
 *
 * Il vuoto non era una scelta tipografica: la lampadina stava in un
 * contenitore con flex_grow(1), cioe si prendeva tutto lo spazio che
 * avanzava nella cella. Con schede alte 260 px avanzava quasi tutto.
 *
 * Quello che si legge non cambia: il nome, la lampadina accesa o spenta —
 * stesso corpo di prima, IC_XL, perche e lo stato e si guarda da lontano —
 * la percentuale per le dimmerabili e la barra sotto. Cambia che stanno
 * affiancati invece che impilati, e che ci si sta tutti in una pagina.
 * --------------------------------------------------------------------- */

static void zona(lv_obj_t *griglia, const luce_t *z, int indice,
                 int col, int rig)
{
    lv_obj_t *k = ui_scheda(griglia);
    lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, col, 1,
                            LV_GRID_ALIGN_STRETCH, rig, 1);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    /* Posto libero dell'ultima pagina: tratteggiato e vuoto. Ha la stessa
       dimensione delle altre schede, che e tutto il punto. */
    if (!z) { posto_vuoto(k); return; }

    const bool spenta = !z->acceso;
    const lv_color_t colore = z->disponibile
                            ? (spenta ? C_DIM : C_TXT) : C_DIM;

    /* --- si tocca la scheda, non un interruttore -----------------------
     *
     * Prima l'accensione stava in un interruttore in alto a destra: preciso
     * da guardare e scomodo da mirare, perche un dito su un vetro verticale
     * non ha la mira di un cursore. Adesso il bersaglio e **tutta la
     * scheda** — piu grande di qualunque icona ci si possa mettere dentro —
     * e la lampadina dice cosa succedera toccandola.
     *
     * Non serve una misura nuova nel profilo: il bersaglio e la cella della
     * griglia, che una misura ce l'ha gia. */
    const bool comandabile = z->disponibile && !sorveglianza_comandi_sospesi();
    if (comandabile) {
        /* L'indice viaggia nel dato dell'evento: la scheda si ricostruisce a
           ogni ridisegno, quindi un puntatore alla zona sarebbe vecchio al
           primo aggiornamento che arriva da Home Assistant. */
        lv_obj_add_event_cb(k, su_interruttore, LV_EVENT_CLICKED,
                            (void *)(intptr_t)indice);
    }

    /* la riga: lampadina, nome, stato */
    lv_obj_t *riga = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
    lv_obj_set_width(riga, LV_PCT(100));
    lv_obj_set_height(riga, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(riga, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(riga, PRF->geo.gap, 0);

    /* Se l'entita non risponde la lampadina non mostra l'ultimo stato noto:
       sarebbe uno stato che il sistema non conosce piu, e il pannello non
       finge di sapere. Resta spenta e la scheda non risponde. */
    /* C_LUCE e non C_ACC: una lampadina accesa non e uno stato
       dell'interfaccia, e una cosa che si accende in casa. Vedi tema.h. */
    ui_icona(riga, ICO_LIGHTBULB,
             z->acceso && z->disponibile ? C_LUCE : C_DIM, IC_XL);

    /* Nome da 40 caratteri: va a capo una volta, poi tronca con l'ellissi,
       e la scheda non cambia dimensione (11-collaudo.md §2). Il tetto
       all'altezza non e' un vezzo: senza, un nome lungo si prende tre righe
       e spinge la barra della luminosita' fuori dalla scheda — e in una
       griglia di schede tutte uguali quella che sborda e' l'unica che si
       nota. */
    lv_obj_t *nome = ui_testo(riga, z->nome, colore, FT_L);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(nome, 1);
    lv_obj_set_style_max_height(nome,
        lv_font_get_line_height(font(FT_L)) * COM.righe_nome_luce, 0);

    /* valore, in fondo alla riga: percentuale se dimmerabile, altrimenti la
       parola */
    lv_obj_t *stato = ui_pannello(riga, C_CARD);
    lv_obj_set_style_bg_opa(stato, LV_OPA_TRANSP, 0);
    lv_obj_set_size(stato, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(stato, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stato, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);

    if (!z->disponibile) {
        /* Non si mostra uno stato che il sistema non conosce. */
        ui_testo(stato, tr(TX_COMMON_UNAVAILABLE), C_DIM, FT_S);
    } else if (!z->dimmerabile) {
        /* Piu piccola del nome: e una conferma, non il titolo della scheda.
           La lampadina lo dice gia, e questa parola serve a chi vuole
           leggerlo a parole. */
        ui_testo(stato, z->acceso ? tr(TX_LIGHTS_ON) : tr(TX_LIGHTS_OFF), colore, FT_S);
    } else {
        static char pct[8];
        lv_snprintf(pct, sizeof pct, "%u", z->percento);
        ui_testo(stato, pct, colore, FT_XL);
        ui_testo(stato, "%", C_DIM, FT_M);
    }

    /* barra di luminosita: solo dove c'e da regolare */
    if (z->dimmerabile && z->disponibile) {
        lv_obj_t *traccia = ui_pannello(k, C_OFF);
        lv_obj_set_width(traccia, LV_PCT(100));
        lv_obj_set_height(traccia, COM.barra_percentuale_h);
        lv_obj_set_style_radius(traccia, LV_RADIUS_CIRCLE, 0);

        /* La barra segue la lampadina e non l'accento: sono la stessa cosa
           guardata due volte — quanto e accesa quella luce li. Con due
           colori diversi sulla stessa scheda si vedrebbe subito che uno dei
           due e sbagliato, senza capire quale. */
        lv_obj_t *piena = ui_pannello(traccia, z->acceso ? C_LUCE : C_OFF);
        lv_obj_set_size(piena, LV_PCT(z->percento), LV_PCT(100));
        lv_obj_set_style_radius(piena, LV_RADIUS_CIRCLE, 0);
    }

    /* Alla fine, quando il contenuto c'e tutto: da qui in poi il bersaglio
       e la scheda intera e non piu un pezzo di essa. Prima di questa riga
       funzionava solo il nome — le etichette nascono trasparenti al tocco, i
       contenitori no, e i contenitori se lo mangiavano. */
    if (comandabile) ui_tocco_su_tutto(k);
}

/* --- griglia ------------------------------------------------------------ */

static void griglia(lv_obj_t *c, int prima, int capienza)
{
    const uint8_t colonne = PRF->griglia.luci_col;
    const uint8_t righe = PRF->griglia.luci_rig;

    /* Le celle sono tutte uguali: e cosi che le schede mantengono la stessa
       dimensione anche nell'ultima pagina incompleta. */
    lv_obj_t *g = ui_griglia(c, colonne, righe);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_flex_grow(g, 1);

    for (int n = 0; n < capienza; n++)
        zona(g, dati_luce(prima + n), prima + n, n % colonne, n / colonne);
}

/* --- costruzione -------------------------------------------------------- */

void schermata_luci(lv_obj_t *c)
{
    const int quante = dati_luci();
    const int capienza = PRF->griglia.luci_col * PRF->griglia.luci_rig;
    const int pagine = paginatore_pagine(quante, capienza);
    if (pagina >= pagine) pagina = 0;

    static char sotto[64];
    const int accese = dati_luci_accese();
    /* "tutte spente", non "0 luci accese" (11-collaudo.md §2); e il singolare
       dove serve, perche "1 zone · 1 accese" e scritto male. */
    /* Two counts, two plural forms: composed from two pieces because
       "3 zones · 1 on" needs the form of each number, and a single
       translation cannot pick two forms at once. */
    char zone[24], luce[24];
    lv_snprintf(zone, sizeof zone, trn(TXN_LIGHTS_ZONES, quante), quante);
    lv_snprintf(luce, sizeof luce, trn(TXN_LIGHTS_ON_COUNT, accese), accese);
    if (quante == 0)
        lv_snprintf(sotto, sizeof sotto, "%s", tr(TX_LIGHTS_NO_ZONES));
    else
        lv_snprintf(sotto, sizeof sotto, "%s · %s", zone,
                    accese == 0 ? tr(TX_LIGHTS_ALL_OFF) : luce);
    ui_testata(tr(TX_SECTION_LIGHTS), sotto);

    /* In testata l'orologio lascia il posto al paginatore. */
    if (pagine > 1) paginatore_frecce(ui_testata_destra(), pagina, pagine, su_pagina);

    scene(c);
    griglia(c, pagina * capienza, capienza);
    paginatore_pallini(c, pagina, pagine);
}
