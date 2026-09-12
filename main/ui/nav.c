#include "nav.h"

#include "comuni.h"
#include "i18n.h"
#include "ui.h"

/* Le voci costruite, per poterle evidenziare senza ricostruire la barra.
   Indicizzate per sezione: le destinazioni fisse stanno in coda. */
#define VOCI_MAX (SEZ_QUANTE + 2)

static lv_obj_t *voci[VOCI_MAX];
static sezione_t voci_sez[VOCI_MAX];
/* Lo sfondo che la voce ha quando **non** e scelta. Va ricordato perche non
   e uguale per tutte — casa ha la sua tinta — e chi deseleziona deve poterlo
   rimettere. Senza, dopo la prima navigazione casa restava della tinta delle
   altre e non tornava piu. */
static lv_color_t voci_sfondo[VOCI_MAX];
static int       n_voci;

/* Cambia lo sfondo a riposo di una voce gia costruita. */
static void voce_sfondo(lv_obj_t *v, lv_color_t colore)
{
    lv_obj_set_style_bg_color(v, colore, 0);
    for (int n = 0; n < n_voci; n++)
        if (voci[n] == v) voci_sfondo[n] = colore;
}

static void su_tocco(lv_event_t *e)
{
    ui_vai((sezione_t)(intptr_t)lv_event_get_user_data(e));
}

/* --- una voce ----------------------------------------------------------- */

/* Icona sopra, nome sotto. Nel rail e una colonna alta, nella barra una
   colonna larga: cambia solo chi la contiene. */
static lv_obj_t *voce(lv_obj_t *padre, sezione_t s, icona_corpo_t corpo)
{
    const sezione_info_t *info = sezione(s);

    lv_obj_t *v = ui_pannello(padre, C_CARD2);
    lv_obj_set_style_radius(v, PRF->geo.radius_tile, 0);
    lv_obj_set_flex_flow(v, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(v, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(v, COM.bordo, 0);
    lv_obj_add_flag(v, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v, su_tocco, LV_EVENT_CLICKED, (void *)(intptr_t)s);

    ui_icona(v, info->icona, C_DIM, corpo);

    /* Se un'etichetta cresce si stringe lei, non spinge fuori le altre:
       01-specifica-ui.md §2 lo chiede per il dock e vale ovunque. */
    lv_obj_t *l = ui_testo(v, tr(info->nome), C_DIM, FT_XS);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);

    if (n_voci < VOCI_MAX) {
        voci_sez[n_voci] = s;
        voci_sfondo[n_voci] = C_CARD2;
        voci[n_voci++] = v;
    }
    return v;
}

/* --- «Altro», e le sezioni che non stanno in barra ----------------------
 *
 * La barra tiene `barra_max` voci — sei — e non si stringe mai: a dieci
 * destinazioni ognuna avrebbe ottanta pixel, il dito ci starebbe ancora ma
 * **l'etichetta no**, e una fila di icone senza nome e' un rebus finche' non
 * la si impara.
 *
 * Quindi: HOME, le prime quattro sezioni nell'ordine di `sezioni` in
 * configurazione, e «Altro». Le altre stanno dietro un tocco in piu', ed e'
 * il prezzo dichiarato di questa forma. Quali siano le prime quattro lo
 * decide chi configura riordinando quell'elenco: nessuna impostazione nuova.
 *
 * Se le sezioni attive ci stanno tutte, «Altro» non compare: un pulsante che
 * apre un foglio vuoto e' peggio di nessun pulsante. */
#define ALTRO ((sezione_t)SEZ_QUANTE)   /* non e una sezione: e la voce      */

/* Quattro per riga. Non e' una misura di layout — le celle si dividono la
   larghezza disponibile, come le voci della barra — ma quante ce ne stanno
   per riga, che e' una scelta di impaginazione: con cinque sezioni nel
   foglio, quattro piu una e' meglio di tre piu due. */
#define ALTRO_COL 4

static lv_obj_t *foglio;      /* il velo col resto delle sezioni, o NULL   */
static int       in_barra;    /* quante sezioni ci sono entrate            */

static void chiudi_foglio(void)
{
    if (!foglio) return;
    lv_obj_delete(foglio);
    foglio = NULL;
}

static void su_scelta(lv_event_t *e)
{
    const sezione_t s = (sezione_t)(intptr_t)lv_event_get_user_data(e);
    chiudi_foglio();
    ui_vai(s);
}

/* Tocco sul velo, cioe fuori dal foglio: si chiude, ed e' il gesto che
   chiunque si aspetta. Se il dito era su una voce della barra — che sotto
   il velo si vede ancora — quella voce risponde, come per gli altri modali
   (11-collaudo.md §1). */
static void su_velo_foglio(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_indev_t *dito = lv_indev_active();
    lv_point_t p = { 0, 0 };
    if (dito) lv_indev_get_point(dito, &p);

    chiudi_foglio();
    nav_inoltra_tocco(p);
}

static void apri_foglio(lv_event_t *e)
{
    LV_UNUSED(e);
    if (foglio) { chiudi_foglio(); return; }

    foglio = ui_pannello(lv_layer_top(), C_BG);
    /* Alto quanto lo schermo **meno la barra**: con LV_PCT(100) e un
       padding in fondo lo sfondo del velo dipingeva lo stesso sopra la
       barra, perche in LVGL il fondo copre anche il padding. */
    lv_obj_set_size(foglio, LV_PCT(100),
                    PRF->schermo.altezza - PRF->geo.navbar_h);
    lv_obj_align(foglio, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(foglio, ui_opa(COM.velo_conferma_pct), 0);
    lv_obj_set_style_pad_all(foglio, 0, 0);
    lv_obj_set_flex_flow(foglio, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(foglio, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(foglio, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(foglio, su_velo_foglio, LV_EVENT_CLICKED, NULL);
    /* Il velo si ferma sopra la barra invece di coprirla: la voce «Altro»
       resta visibile e resta accesa, cosi si vede da dove il foglio e'
       uscito e dove ripremere per richiuderlo. Coprirla lascerebbe un
       foglio senza provenienza. */


    /* Il foglio sale dal basso, dove sta la barra da cui si e' aperto: e'
       l'unica posizione che non fa perdere il filo. Sopra la barra, non
       sopra tutto: la barra resta visibile e resta viva. */
    lv_obj_t *k = ui_pannello(foglio, C_CARD);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(k, PRF->geo.radius, 0);
    lv_obj_set_style_pad_all(k, PRF->geo.pad, 0);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
    /* Senza flusso i due figli si sovrappongono in alto a sinistra, e
       l'occhiello finisce sotto la griglia: si vedeva come un segno di due
       pixel sul bordo del foglio. */
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    ui_bordo(k, LV_BORDER_SIDE_TOP, C_LINE);
    /* Il tocco dentro il foglio non deve chiudere il foglio: senza questo,
       l'evento sale al velo e il foglio si chiude appena lo si sfiora. */
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);

    ui_occhiello(k, tr(TX_NAV_OTHER_SECTIONS));

    /* Le righe si contano prima: una griglia LVGL vuole sapere quante ne
       ha, e con zero il contenitore non sa quanto e' alto — l'occhiello qui
       sopra finiva tagliato dal bordo del foglio. */
    const int nascoste = sezioni_attive() - in_barra;
    const int righe = (nascoste + ALTRO_COL - 1) / ALTRO_COL;

    lv_obj_t *g = ui_griglia(k, ALTRO_COL, righe);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_height(g, LV_SIZE_CONTENT);

    int r = 0;
    for (int n = in_barra; n < sezioni_attive(); n++) {
        const sezione_t s = sezione_attiva(n);
        const sezione_info_t *info = sezione(s);
        const int col = (n - in_barra) % ALTRO_COL;

        lv_obj_t *b = ui_scheda(g);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, col, 1,
                                LV_GRID_ALIGN_STRETCH, r, 1);
        lv_obj_set_height(b, PRF->tocco.apertura.h + PRF->geo.pad);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(b, COM.bordo, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_scelta, LV_EVENT_CLICKED,
                            (void *)(intptr_t)s);

        ui_icona(b, info->icona, C_DIM, IC_M);
        lv_obj_t *l = ui_testo(b, tr(info->nome), C_TXT, FT_XS);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);

        if (col == ALTRO_COL - 1) r++;
    }
}

void nav_apri_altro(void)   { apri_foglio(NULL); }
void nav_chiudi_altro(void) { chiudi_foglio(); }

/* Come voce(), ma non porta a una sezione: apre il foglio. Registrata fra
   le voci con un identificatore che non e' una sezione, cosi nav_evidenzia()
   puo accenderla quando si sta guardando una delle sezioni nascoste — chi
   naviga deve poter vedere dove si trova, anche quando dove si trova non ha
   una voce sua. */
static lv_obj_t *voce_altro(lv_obj_t *padre)
{
    lv_obj_t *v = ui_pannello(padre, C_CARD2);
    lv_obj_set_style_radius(v, PRF->geo.radius_tile, 0);
    lv_obj_set_flex_flow(v, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(v, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(v, COM.bordo, 0);
    lv_obj_add_flag(v, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v, apri_foglio, LV_EVENT_CLICKED, NULL);

    ui_icona(v, ICO_MORE_HORIZ, C_DIM, IC_M);
    lv_obj_t *l = ui_testo(v, tr(TX_NAV_MORE), C_DIM, FT_XS);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);

    if (n_voci < VOCI_MAX) {
        voci_sez[n_voci] = ALTRO;
        voci_sfondo[n_voci] = C_CARD2;
        voci[n_voci++] = v;
    }
    return v;
}

/* --- rail, in orizzontale ----------------------------------------------- */

static lv_obj_t *rail(lv_obj_t *padre)
{
    lv_obj_t *r = ui_pannello(padre, C_CARD2);
    lv_obj_set_size(r, PRF->geo.rail_w, LV_PCT(100));
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_bordo(r, LV_BORDER_SIDE_RIGHT, C_LINE);

    lv_obj_t *casa = voce(r, SEZ_HOME, IC_M);
    lv_obj_set_size(casa, PRF->geo.rail_home.w, PRF->geo.rail_home.h);
    /* Nessun riquadro nemmeno attorno a casa: nel rail le voci si
       distinguono per tinta di fondo e colore dell'icona, e il bordo non si
       disegna mai. Restano solo i due divisori del rail — il suo filo destro
       e la riga sopra l'ingranaggio — che separano zone, non pulsanti. */
    voce_sfondo(casa, C_CARD);

    /* Le sezioni attive si dividono l'altezza rimasta: quante sono lo dice
       la configurazione, e il rail le mostra tutte. */
    for (int n = 0; n < sezioni_attive(); n++) {
        lv_obj_t *v = voce(r, sezione_attiva(n), IC_M);
        lv_obj_set_width(v, PRF->geo.rail_item.w);
        lv_obj_set_flex_grow(v, 1);
    }

    lv_obj_t *ing = voce(r, SEZ_IMPOSTAZIONI, IC_S);
    lv_obj_set_size(ing, PRF->geo.rail_gear.w, PRF->geo.rail_gear.h);
    lv_obj_set_style_radius(ing, 0, 0);
    ui_bordo(ing, LV_BORDER_SIDE_TOP, C_LINE);
    return r;
}

/* --- barra, in verticale ------------------------------------------------ */

static lv_obj_t *barra(lv_obj_t *padre)
{
    lv_obj_t *b = ui_pannello(padre, C_CARD2);
    lv_obj_set_size(b, LV_PCT(100), PRF->geo.navbar_h);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    ui_bordo(b, LV_BORDER_SIDE_TOP, C_LINE);

    /* HOME e la prima voce a sinistra, sempre presente; le sezioni attive
       occupano il resto della larghezza in parti uguali. La larghezza di
       una voce e quindi una misura derivata, non un dato del profilo. */
    lv_obj_t *casa = voce(b, SEZ_HOME, IC_M);
    lv_obj_set_height(casa, LV_PCT(100));
    lv_obj_set_flex_grow(casa, 1);
    voce_sfondo(casa, C_CARD);

    /* Quante sezioni entrano: tutte se ci stanno, altrimenti si lascia un
       posto ad «Altro». HOME occupa il primo. */
    const int posti = PRF->geo.barra_max - 1;
    const int attive = sezioni_attive();
    const bool serve_altro = attive > posti;
    in_barra = serve_altro ? posti - 1 : attive;

    for (int n = 0; n < in_barra; n++) {
        lv_obj_t *v = voce(b, sezione_attiva(n), IC_M);
        lv_obj_set_height(v, LV_PCT(100));
        lv_obj_set_flex_grow(v, 1);
        ui_bordo(v, LV_BORDER_SIDE_LEFT, C_LINE);
    }

    if (serve_altro) {
        lv_obj_t *v = voce_altro(b);
        lv_obj_set_height(v, LV_PCT(100));
        lv_obj_set_flex_grow(v, 1);
        ui_bordo(v, LV_BORDER_SIDE_LEFT, C_LINE);
    }
    return b;
}

lv_obj_t *nav_costruisci(lv_obj_t *padre)
{
    /* Il foglio vive su lv_layer_top() e sopravvive alla ricostruzione
       della schermata: se non lo si chiude qui resta appeso sopra la
       sezione appena aperta. */
    chiudi_foglio();
    n_voci = 0;
    return PRF->orientamento == VERTICALE ? barra(padre) : rail(padre);
}

/* --- evidenziazione ----------------------------------------------------- */

/* La voce scelta si riconosce dall'icona accesa e dallo sfondo, **mai da un
   bordo**. Il bordo qui era messo da un `if` a senso unico: si aggiungeva
   alla voce scelta e non si toglieva a quella che smetteva di esserlo, cosi
   che ogni sezione visitata se lo teneva addosso per sempre. Sul vetro si
   vedeva come un rail che a schermo appena acceso e pulito e si riempie di
   riquadri man mano che lo si usa.

   La regola generale che ne esce: una funzione che dipinge uno stato deve
   dipingere **tutti** i casi, non solo quello acceso. Un `if` senza `else`
   dentro un evidenziatore e sempre un difetto, perche lo stato precedente
   non si cancella da solo. */
static void colora(int n, bool scelta)
{
    lv_obj_t *v = voci[n];
    lv_obj_set_style_bg_color(v, scelta ? C_SEL_BG : voci_sfondo[n], 0);

    const lv_color_t c = scelta ? C_SEL_TXT : C_DIM;
    for (uint32_t i = 0; i < lv_obj_get_child_count(v); i++)
        lv_obj_set_style_text_color(lv_obj_get_child(v, i), c, 0);
}

void nav_evidenzia(sezione_t s)
{
    /* La diagnostica si apre dalle impostazioni: nel rail resta evidenziato
       l'ingranaggio, perche e da li che ci si e arrivati. */
    if (s == SEZ_DIAGNOSTICA) s = SEZ_IMPOSTAZIONI;

    /* Se la sezione corrente non ha una voce sua — sta nel foglio «Altro» —
       si accende «Altro». Senza, la barra resta tutta spenta e chi guarda
       non sa piu dov'e: peggio di un'evidenziazione approssimata. */
    bool ha_voce = false;
    for (int n = 0; n < n_voci; n++) if (voci_sez[n] == s) ha_voce = true;

    for (int n = 0; n < n_voci; n++)
        colora(n, voci_sez[n] == s || (!ha_voce && voci_sez[n] == ALTRO));
}

bool nav_inoltra_tocco(lv_point_t punto)
{
    for (int n = 0; n < n_voci; n++) {
        lv_area_t a;
        lv_obj_get_coords(voci[n], &a);
        if (punto.x >= a.x1 && punto.x <= a.x2 &&
            punto.y >= a.y1 && punto.y <= a.y2) {
            ui_vai(voci_sez[n]);
            return true;
        }
    }
    return false;
}


/* --- dock della home ---------------------------------------------------- */

lv_obj_t *nav_dock(lv_obj_t *padre)
{
    if (!PRF->home.dock) return NULL;   /* in verticale c'e la barra */

    lv_obj_t *d = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, 0);
    lv_obj_set_width(d, LV_PCT(100));
    lv_obj_set_flex_flow(d, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(d, PRF->geo.gap, 0);

    /* Le sezioni attive possono essere piu dei pulsanti che ci stanno: il
       dock mostra le prime nell'ordine di configurazione, e il rail continua
       a mostrarle tutte, quindi nessuna diventa irraggiungibile
       (09-profili.md §6). */
    int quanti = sezioni_attive();
    if (quanti > PRF->home.dock_max) quanti = PRF->home.dock_max;

    for (int n = 0; n < quanti; n++) {
        const sezione_t s = sezione_attiva(n);
        const sezione_info_t *info = sezione(s);

        lv_obj_t *b = ui_pannello(d, C_CARD);
        lv_obj_set_style_radius(b, PRF->geo.radius, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_height(b, LV_PCT(100));
        lv_obj_set_flex_grow(b, 1);
        /* min-width a zero e testo non a capo: se un'etichetta cresce il
           pulsante si stringe invece di spingere gli altri fuori schermo. */
        lv_obj_set_style_min_width(b, 0, 0);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(b, PRF->geo.gap, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_tocco, LV_EVENT_CLICKED, (void *)(intptr_t)s);

        ui_icona(b, info->icona, C_DIM, IC_L);

        lv_obj_t *l = ui_testo(b, tr(info->nome), C_TXT, FT_XS);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        const int32_t sp = (int32_t)PRF->font.f_xs
                         * (int32_t)COM.spaziatura_dock_millesimi / 1000;
        lv_obj_set_style_text_letter_space(l, sp, 0);

        /* Lo stato sotto il nome arriva coi dati veri (Fase 3): oggi la riga
           c'e ma e vuota, cosi l'altezza del pulsante e gia quella giusta. */
        ui_testo(b, " ", C_DIM, FT_S);
    }
    return d;
}
