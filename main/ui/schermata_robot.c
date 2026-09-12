/* ------------------------------------------------------------------------
 * Il robot lavapavimenti — 01-specifica-ui.md §3.9, proposta «la piantina».
 *
 * Le stanze disposte come stanno in casa, non in colonna. Si tocca la cucina
 * perche' e in alto a destra come la cucina vera, e la mano ci arriva senza
 * leggere l'etichetta: su un pannello che si usa passando, leggere e il
 * costo piu alto che si possa chiedere.
 *
 * Sopra, una fascia sola con lo stato e la batteria. Sotto, due comandi: il
 * primo cambia testo con quello che hai scelto — «Pulisci 2 stanze» oppure
 * «Pulisci tutto» — perche' un pulsante deve dire cosa fa **adesso**, non a
 * cosa serve in generale.
 *
 * Quello che qui non si fa: inventare. Senza entita configurata la
 * schermata lo dice e non finge; una stanza senza posizione non sparisce, va
 * in coda; e la batteria compare solo se il robot la manda davvero.
 * --------------------------------------------------------------------- */
#include "schermate.h"

#include <string.h>

#include "comuni.h"
#include "i18n.h"
#include "config.h"
#include "dati.h"
#include "theme.h"
#include "ui.h"
#include "widgets/pulsante_comando.h"

/* La griglia della piantina, come in configurazione. Le due misure stanno
   qui e non in profile.h perche' non sono misure di layout: sono la forma
   del sistema di coordinate che chi configura usa per dire «la cucina sta in
   alto a destra». Cambiarle cambierebbe il significato di ogni `col` gia
   scritta in config.json, non l'aspetto di una schermata. */
#define COLONNE 6
#define RIGHE   4

/* Le stanze scelte, per numero. Si azzera ricostruendo la schermata, che e
   quello che succede uscendo e rientrando: una scelta dimenticata li dentro
   manderebbe il robot dove non ci si aspetta. */
#define SCELTE_MAX 16
static int  scelte[SCELTE_MAX];
static int  quante_scelte;

static lv_obj_t *bottone_pulisci;
static lv_obj_t *riquadri[SCELTE_MAX];
static lv_obj_t *segni[SCELTE_MAX];
static int       numeri[SCELTE_MAX];
static int       quanti_riquadri;

/* --- la scelta ---------------------------------------------------------- */

static bool e_scelta(int numero)
{
    for (int n = 0; n < quante_scelte; n++)
        if (scelte[n] == numero) return true;
    return false;
}

static void togli(int numero)
{
    for (int n = 0; n < quante_scelte; n++) {
        if (scelte[n] != numero) continue;
        for (int k = n; k + 1 < quante_scelte; k++) scelte[k] = scelte[k + 1];
        quante_scelte--;
        return;
    }
}

/* Il testo del comando principale. Dice cosa succede toccandolo adesso:
   con una stanza sola si nomina la stanza, perche' «Pulisci la Cucina» e
   una frase che si verifica prima di premere. */
static void aggiorna_bottone(void)
{
    static char testo[48];

    if (quante_scelte == 0) {
        const char *senza = cfg_testo("robot/no_selection", "whole_house");
        if (strcmp(senza, "nothing") == 0) {
            pulsante_comando_testo(bottone_pulisci, tr(TX_ROBOT_PICK_A_ROOM));
            pulsante_comando_attivo(bottone_pulisci, false);
            return;
        }
        pulsante_comando_testo(bottone_pulisci, tr(TX_ROBOT_CLEAN_ALL));
        pulsante_comando_attivo(bottone_pulisci, true);
        return;
    }

    pulsante_comando_attivo(bottone_pulisci, true);
    if (quante_scelte == 1) {
        const char *nome = tr(TX_ROBOT_THE_ROOM);
        for (int n = 0; n < quanti_riquadri; n++)
            if (numeri[n] == scelte[0]) {
                const robot_stanza_t *s = dati_robot_stanza(n);
                if (s && s->nome) nome = s->nome;
                break;
            }
        lv_snprintf(testo, sizeof testo, tr(TX_ROBOT_CLEAN_ONE), nome);
    } else {
        lv_snprintf(testo, sizeof testo, trn(TXN_ROBOT_CLEAN_ROOMS, quante_scelte),
                    quante_scelte);
    }
    pulsante_comando_testo(bottone_pulisci, testo);
}

static void tinta(lv_obj_t *r, bool scelta)
{
    lv_obj_set_style_bg_color(r, scelta ? C_SEL_BG : C_CARD, 0);
    ui_bordo(r, LV_BORDER_SIDE_FULL, scelta ? C_SEL_LINE : C_LINE);
}

static void su_stanza(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    if (n < 0 || n >= quanti_riquadri) return;

    const int numero = numeri[n];
    if (numero < 0) return;

    if (e_scelta(numero))              togli(numero);
    else if (quante_scelte < SCELTE_MAX) scelte[quante_scelte++] = numero;

    const bool scelta = e_scelta(numero);
    tinta(riquadri[n], scelta);
    if (segni[n]) {
        if (scelta) lv_obj_remove_flag(segni[n], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(segni[n], LV_OBJ_FLAG_HIDDEN);
    }
    aggiorna_bottone();
}

/* --- i comandi ---------------------------------------------------------- */

static void su_pulisci(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    pulsante_comando_stato(b, CMD_IN_CORSO);

    bool andato;
    if (quante_scelte > 0) {
        andato = dati_robot_pulisci(scelte, quante_scelte);
    } else {
        const char *senza = cfg_testo("robot/no_selection", "whole_house");
        andato = strcmp(senza, "nothing") != 0 && dati_robot_tutto();
    }
    pulsante_comando_stato(b, andato ? CMD_RIUSCITO : CMD_FALLITO);
}

static void su_base(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    pulsante_comando_stato(b, CMD_IN_CORSO);
    pulsante_comando_stato(b, dati_robot_alla_base() ? CMD_RIUSCITO
                                                     : CMD_FALLITO);
}

/* --- lo stato in cima --------------------------------------------------- */

static const char *parola(robot_stato_t s)
{
    switch (s) {
    case ROB_PULISCE:   return tr(TX_ROBOT_STATE_CLEANING);
    case ROB_RIENTRA:   return tr(TX_ROBOT_STATE_RETURNING);
    case ROB_ALLA_BASE: return tr(TX_ROBOT_STATE_DOCKED);
    case ROB_IN_PAUSA:  return tr(TX_ROBOT_STATE_PAUSED);
    case ROB_FERMO:     return tr(TX_ROBOT_STATE_IDLE);
    case ROB_ERRORE:    return tr(TX_ROBOT_STATE_ERROR);
    default:            return tr(TX_ROBOT_STATE_UNKNOWN);
    }
}

static lv_color_t tinta_stato(robot_stato_t s)
{
    switch (s) {
    case ROB_PULISCE:
    case ROB_RIENTRA:  return C_ACC;
    case ROB_ERRORE:   return C_WARN;
    case ROB_ALLA_BASE: return C_OK;
    default:           return C_DIM;
    }
}

static void fascia_stato(lv_obj_t *padre, const robot_t *r)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(k, PRF->geo.gap, 0);

    /* L'icona grande quanto quella di una sezione: e il ritratto del robot
       ed e l'unica immagine di questa schermata. Piccola faceva sembrare la
       fascia una riga di testo con un puntino davanti. */
    ui_icona(k, ICO_MOP, tinta_stato(r->stato), IC_L);

    lv_obj_t *col = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(col, 1);

    ui_testo(col, parola(r->stato), C_TXT, FT_L);

    /* La frase del robot solo se ce n'e una: un secondo rigo vuoto sotto lo
       stato sembra un dato che non e arrivato. */
    if (r->dettaglio && r->dettaglio[0])
        ui_testo(col, r->dettaglio, C_DIM, FT_S);

    ui_spazio(k);

    if (r->batteria_c_e) {
        lv_obj_t *b = ui_pannello(k, C_CARD);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_END);

        static char pct[8];
        lv_snprintf(pct, sizeof pct, "%d%%", r->batteria);
        const lv_color_t tinta_c = r->batteria <= 20 ? C_WARN
                                 : r->in_carica      ? C_ACC : C_OK;
        ui_testo(b, pct, tinta_c, FT_L);

        /* Una pila disegnata sotto il numero. Il numero dice quanto, la
           pila lo fa vedere: da un metro di distanza si legge la seconda
           molto prima del primo, ed e la distanza da cui si guarda un
           pannello a muro passando. */
        lv_obj_t *pila = ui_pannello(b, C_OFF);
        lv_obj_set_size(pila, COM.pila_w, COM.pila_h);
        lv_obj_set_style_radius(pila, COM.tratto, 0);
        lv_obj_set_style_pad_all(pila, 0, 0);

        lv_obj_t *carica = ui_pannello(pila, tinta_c);
        lv_obj_set_size(carica, LV_PCT(r->batteria < 4 ? 4 : r->batteria),
                        LV_PCT(100));
        lv_obj_set_style_radius(carica, COM.tratto, 0);

        ui_occhiello(b, r->in_carica ? tr(TX_ROBOT_CHARGING)
                             : tr(TX_ROBOT_BATTERY));
    }
}

/* --- la piantina -------------------------------------------------------- */

static void piantina(lv_obj_t *padre)
{
    lv_obj_t *g = ui_griglia(padre, COLONNE, RIGHE);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_flex_grow(g, 1);

    quanti_riquadri = 0;

    /* Le stanze senza posizione vanno in coda, una cella per volta, dopo
       l'ultima riga occupata: chi ha configurato a meta deve poterle
       comandare lo stesso, anche se la piantina non racconta piu la casa. */
    uint8_t col_libera = 1, riga_libera = 1;

    const int quante = dati_robot_stanze();
    for (int n = 0; n < quante && quanti_riquadri < SCELTE_MAX; n++) {
        const robot_stanza_t *s = dati_robot_stanza(n);
        if (!s) continue;

        uint8_t c = s->col, r = s->riga, w = s->larghezza, h = s->altezza;
        if (c < 1 || r < 1 || c > COLONNE || r > RIGHE) {
            c = col_libera; r = riga_libera; w = 1; h = 1;
            if (++col_libera > COLONNE) { col_libera = 1; riga_libera++; }
            if (riga_libera > RIGHE) break;   /* non c'e piu posto */
        }
        if (c + w - 1 > COLONNE) w = (uint8_t)(COLONNE - c + 1);
        if (r + h - 1 > RIGHE)   h = (uint8_t)(RIGHE - r + 1);

        lv_obj_t *k = ui_pannello(g, C_CARD);
        lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, c - 1, w,
                                LV_GRID_ALIGN_STRETCH, r - 1, h);
        lv_obj_set_style_radius(k, PRF->geo.radius_tile, 0);
        lv_obj_set_style_pad_all(k, PRF->geo.pad, 0);
        lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
        /* Al centro, non in un angolo. Le stanze hanno forme diverse — il
           soggiorno e largo il doppio del bagno — e un nome appoggiato in
           basso a sinistra in riquadri di taglia diversa non allinea con
           niente: si legge come un errore di impaginazione. Al centro ogni
           nome sta dov'e il suo riquadro, e la piantina si guarda come una
           piantina. */
        lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(k, COM.gap_stretto, 0);
        lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(k, su_stanza, LV_EVENT_CLICKED,
                            (void *)(intptr_t)quanti_riquadri);
        tinta(k, false);

        /* Il segno di scelta sta sopra il nome e compare solo quando la
           stanza e scelta: a riposo la piantina resta pulita, e quando si
           tocca qualcosa succede in due modi — il fondo che si accende e un
           segno che appare — perche' su un vetro riflesso il solo cambio di
           tinta a volte non si vede. */
        lv_obj_t *segno = ui_icona(k, ICO_CHECK, C_ACC, IC_S);
        lv_obj_add_flag(segno, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *nome = ui_testo(k, s->nome ? s->nome : tr(TX_ROBOT_ROOM), C_TXT, FT_M);
        lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nome, LV_PCT(100));
        lv_obj_set_style_text_align(nome, LV_TEXT_ALIGN_CENTER, 0);
        segni[quanti_riquadri] = segno;

        riquadri[quanti_riquadri] = k;
        numeri[quanti_riquadri] = s->numero;
        quanti_riquadri++;
    }

    if (quanti_riquadri == 0) {
        lv_obj_t *a = ui_testo(g, tr(TX_ROBOT_NO_ROOMS), C_DIM, FT_S);
        lv_obj_set_grid_cell(a, LV_GRID_ALIGN_STRETCH, 0, COLONNE,
                                LV_GRID_ALIGN_CENTER, 0, RIGHE);
        lv_label_set_long_mode(a, LV_LABEL_LONG_WRAP);
    }
}

/* --- avvisi e manutenzione, la vista a parte ----------------------------
 *
 * Perche' a parte e non in fondo alla piantina: le stanze sono cio per cui
 * si apre questa sezione, e una spazzola con duecento ore davanti non deve
 * rubare loro spazio. Gli avvisi invece **si annunciano da soli**, col
 * triangolo accanto al nome — in home e qui in testata — e chi lo vede sa
 * gia dove porta.
 *
 * Quando non c'e niente da segnalare la vista si raggiunge lo stesso,
 * toccando la fascia dello stato: le ore che restano a un filtro si vanno a
 * cercare, e una porta che esiste solo quando la cosa e gia rotta e una
 * porta che nessuno impara. */
#define VISTA_AVVISI 1

static void su_avvisi(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_vai_a(SEZ_ROBOT, VISTA_AVVISI);
}

static void su_indietro(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_vai(SEZ_ROBOT);
}

/* Il triangolo: compare solo se c'e qualcosa, e porta dove serve. Sta in
   una funzione sola perche' lo mettono in due — la testata di questa
   sezione e la riga del robot in home — e due disegni dello stesso segno si
   allontanano al primo che ne cambia uno.

   Il bersaglio e la pastiglia e non l'icona: ui_tocco_minimo() la porta ai
   quarantaquattro pixel di 09-profili.md §8 anche quando il disegno e piu
   piccolo. */
lv_obj_t *robot_triangolo(lv_obj_t *padre)
{
    const int accesi = dati_robot_avvisi_accesi()
                     + dati_robot_manutenzioni_agli_sgoccioli();
    if (accesi <= 0) return NULL;

    lv_obj_t *p = ui_pannello(padre, C_AVV_KO_BG);
    lv_obj_set_size(p, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(p, PRF->geo.radius_btn, 0);
    lv_obj_set_style_pad_hor(p, COM.pastiglia_pad_h, 0);
    lv_obj_set_style_pad_ver(p, COM.pastiglia_pad_v, 0);
    ui_bordo(p, LV_BORDER_SIDE_FULL, C_AVV_KO_LINE);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(p, COM.pastiglia_gap, 0);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(p, su_avvisi, LV_EVENT_CLICKED, NULL);

    ui_icona(p, ICO_WARNING, C_AVV_KO_TXT, IC_S);

    /* Il numero **e** la parola. Un «2» da solo dentro un triangolo si
       legge, ma non dice di cosa: accanto al nome di un robot potrebbe
       essere qualunque conto. E una pastiglia che si tocca deve dire dove
       porta. */
    static char quanti[16];
    lv_snprintf(quanti, sizeof quanti, trn(TXN_ROBOT_ALERTS, accesi), accesi);
    ui_testo(p, quanti, C_AVV_KO_TXT, FT_S);

    ui_tocco_su_tutto(p);
    ui_tocco_minimo(p, COM.tocco_min, COM.tocco_min);
    return p;
}

/* Una riga della vista: un'icona, una frase, e a destra il valore quando
   c'e. La stessa forma per gli avvisi e per le manutenzioni, perche' sono
   la stessa cosa a due gradi di urgenza. */
static void riga_avviso(lv_obj_t *c, const char *icona, lv_color_t colore,
                        const char *testo, const char *valore, bool attenua)
{
    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(k, PRF->geo.gap, 0);
    if (attenua)
        lv_obj_set_style_opa(k, ui_opa(COM.opacita_dato_vecchio_pct), 0);

    ui_icona(k, icona, colore, IC_M);

    lv_obj_t *l = ui_testo(k, testo, attenua ? C_DIM : C_TXT, FT_M);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(l, 1);
    lv_obj_set_width(l, 0);

    if (valore && *valore) ui_testo(k, valore, colore, FT_M);
}

static void vista_avvisi(lv_obj_t *c)
{
    const int quanti = dati_robot_avvisi();
    const int accesi = dati_robot_avvisi_accesi();
    const int manut = dati_robot_manutenzioni();

    static char sotto[64];
    if (accesi) lv_snprintf(sotto, sizeof sotto, tr(TX_ROBOT_ALERTS_TO_CHECK), accesi);
    else        lv_snprintf(sotto, sizeof sotto, "%s", tr(TX_ROBOT_NOTHING_TO_REPORT));
    ui_testata(cfg_testo("robot/name", tr(TX_ROBOT_DEFAULT_NAME)), sotto);

    /* Il ritorno sta in testata come nel dettaglio del condizionatore: e la
       stessa situazione — una vista dentro una sezione — e si torna
       indietro con lo stesso gesto. */
    lv_obj_t *dietro = ui_pannello(ui_testata_destra(), C_CARD2);
    lv_obj_set_size(dietro, LV_SIZE_CONTENT, PRF->tocco.pager.h);
    lv_obj_set_style_pad_hor(dietro, PRF->geo.gap, 0);
    lv_obj_set_style_radius(dietro, PRF->geo.radius_btn, 0);
    ui_bordo(dietro, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_flow(dietro, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dietro, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dietro, COM.pastiglia_gap, 0);
    lv_obj_add_flag(dietro, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dietro, su_indietro, LV_EVENT_CLICKED, NULL);
    ui_icona(dietro, ICO_ARROW_BACK, C_TXT, IC_S);
    ui_testo(dietro, tr(TX_ROBOT_BACK_TO_ROOMS), C_TXT, FT_M);
    ui_tocco_su_tutto(dietro);

    lv_obj_t *col = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, PRF->geo.gap, 0);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_AUTO);

    /* Prima quelli accesi, poi gli spenti attenuati: chi apre per un
       triangolo trova in cima la ragione per cui l'ha aperto. Gli spenti
       non si nascondono — «l'acqua sporca sta bene» e un'informazione, e
       serve a fidarsi di quelli accesi. */
    for (int passata = 0; passata < 2; passata++) {
        for (int n = 0; n < quanti; n++) {
            const robot_avviso_t *a = dati_robot_avviso(n);
            if (!a || !a->testo || !*a->testo) continue;
            if (a->attivo != (passata == 0)) continue;
            riga_avviso(col, a->attivo ? ICO_WARNING : ICO_CHECK,
                        a->attivo ? C_WARN : C_OK,
                        a->testo, NULL, !a->attivo);
        }
    }

    if (quanti == 0 && manut == 0) {
        ui_testo(col, tr(TX_ROBOT_NO_ALERTS_CONFIGURED), C_DIM, FT_S);
        return;
    }

    if (manut) {
        ui_occhiello(col, tr(TX_ROBOT_MAINTENANCE));
        for (int n = 0; n < manut; n++) {
            const robot_manutenzione_t *m = dati_robot_manutenzione(n);
            if (!m) continue;
            riga_avviso(col, m->agli_sgoccioli ? ICO_WARNING : ICO_TIMER,
                        m->agli_sgoccioli ? C_WARN : C_DIM,
                        m->nome, m->disponibile ? m->valore : tr(TX_COMMON_NOT_RESPONDING),
                        !m->disponibile);
        }
    }
}

/* --- costruzione -------------------------------------------------------- */

void schermata_robot(lv_obj_t *c)
{
    if (ui_vista() == VISTA_AVVISI) { vista_avvisi(c); return; }

    const robot_t r = dati_robot();

    quante_scelte = 0;
    quanti_riquadri = 0;
    bottone_pulisci = NULL;

    /* Il nome che gli avete dato, se ce n'e uno: la testata di una sezione
       che si chiama come una categoria — «Robot» — dice meno di una che si
       chiama come la cosa che comanda. */
    ui_testata(cfg_testo("robot/name", tr(TX_ROBOT_DEFAULT_NAME)),
               r.disponibile ? tr(TX_ROBOT_SUBTITLE)
                             : tr(TX_ROBOT_UNAVAILABLE));
    robot_triangolo(ui_testata_destra());

    lv_obj_t *col = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, PRF->geo.gap, 0);

    fascia_stato(col, &r);
    piantina(col);

    /* --- i comandi ------------------------------------------------------
     *
     * **Nessuna pressione prolungata**, e c'era. Il gesto lungo si mette
     * davanti a un comando che non si puo ritirare — un cancello che si apre
     * su strada — e mandare a pulire non lo e: il robot si ferma con un
     * tocco, e la stanza sbagliata costa dieci minuti di rumore. Un ostacolo
     * davanti a un gesto senza conseguenze e solo un gesto in piu, ogni
     * volta.
     *
     * Piu alti dei pulsanti ordinari, e la misura sta nel profilo: sono i
     * bersagli piu grandi della loro schermata e si colpiscono passando,
     * spesso con la stanza gia scelta. */

    lv_obj_t *azioni = ui_pannello(col, C_BG);
    lv_obj_set_style_bg_opa(azioni, LV_OPA_TRANSP, 0);
    lv_obj_set_width(azioni, LV_PCT(100));
    lv_obj_set_height(azioni, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(azioni, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(azioni, PRF->geo.gap, 0);

    bottone_pulisci = pulsante_comando(azioni, tr(TX_ROBOT_CLEAN_ALL), false,
                                       r.disponibile);
    lv_obj_set_flex_grow(bottone_pulisci, 2);
    lv_obj_set_height(bottone_pulisci, PRF->tocco.comando_robot_h);
    lv_obj_add_event_cb(bottone_pulisci, su_pulisci, EV_COMANDO_SCATTATO, NULL);

    lv_obj_t *base = pulsante_comando(azioni, tr(TX_ROBOT_TO_DOCK), false,
                                      r.disponibile);
    lv_obj_set_flex_grow(base, 1);
    lv_obj_set_height(base, PRF->tocco.comando_robot_h);
    lv_obj_add_event_cb(base, su_base, EV_COMANDO_SCATTATO, NULL);

    aggiorna_bottone();
}
