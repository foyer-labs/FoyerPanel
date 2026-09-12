#include "pulsante_comando.h"

#include "comuni.h"
#include "tempi.h"
#include "config.h"

typedef struct {
    lv_obj_t       *icona;
    lv_obj_t       *testo;
    lv_obj_t       *sotto;      /* "TIENI PREMUTO", solo con conferma */
    lv_obj_t       *anello;
    const char     *nome;       /* il testo a riposo */
    bool            conferma;
    stato_comando_t stato;
    lv_timer_t     *ritorno;
    lv_anim_t       avanzamento;
    bool click_attaccato;  /* vedi pulsante_comando_attivo */
} comando_t;

static void libera(lv_event_t *e)
{
    comando_t *k = lv_obj_get_user_data(lv_event_get_target(e));
    if (k && k->ritorno) lv_timer_delete(k->ritorno);
    lv_free(k);
}

/* --- aspetto ------------------------------------------------------------ */

static void colori(lv_obj_t *b, lv_color_t sfondo, lv_color_t bordo,
                   lv_color_t testo)
{
    comando_t *k = lv_obj_get_user_data(b);
    lv_obj_set_style_bg_color(b, sfondo, 0);
    lv_obj_set_style_border_color(b, bordo, 0);
    lv_obj_set_style_text_color(k->testo, testo, 0);
    lv_obj_set_style_text_color(k->icona, testo, 0);
    if (k->sotto) lv_obj_set_style_text_color(k->sotto, testo, 0);
}

static void torna_pronto(lv_timer_t *t)
{
    pulsante_comando_stato(lv_timer_get_user_data(t), CMD_PRONTO);
}

void pulsante_comando_stato(lv_obj_t *b, stato_comando_t s)
{
    comando_t *k = lv_obj_get_user_data(b);
    if (!k) return;

    if (k->ritorno) { lv_timer_delete(k->ritorno); k->ritorno = NULL; }
    k->stato = s;

    switch (s) {
    case CMD_PRONTO:
        colori(b, C_ACC, C_ACC, C_INK);
        lv_label_set_text(k->icona, "");
        lv_label_set_text(k->testo, k->nome);
        if (k->sotto) lv_label_set_text(k->sotto, tr(TX_COMMAND_HOLD));
        break;

    case CMD_IN_CORSO:
        colori(b, C_INVIO_BG, C_INVIO_LINE, C_INVIO_TXT);
        lv_label_set_text(k->icona, ICO_HOURGLASS_EMPTY);
        lv_label_set_text(k->testo, tr(TX_COMMAND_SENDING));
        if (k->sotto) lv_label_set_text(k->sotto, "");
        break;

    case CMD_RIUSCITO:
        colori(b, C_FATTO_BG, C_FATTO_LINE, C_FATTO_TXT);
        lv_label_set_text(k->icona, ICO_CHECK);
        lv_label_set_text(k->testo, tr(TX_COMMAND_SENT));
        if (k->sotto) lv_label_set_text(k->sotto, "");
        /* Torna da solo dopo tre secondi. */
        k->ritorno = lv_timer_create(torna_pronto, T_RISCONTRO_RIUSCITO, b);
        lv_timer_set_repeat_count(k->ritorno, 1);
        break;

    case CMD_FALLITO:
        /* Permanente: un comando fallito non sparisce da solo. */
        colori(b, C_ERR_BG, C_ERR_LINE, C_ERR_TXT);
        lv_label_set_text(k->icona, ICO_CLOSE);
        lv_label_set_text(k->testo, tr(TX_COMMAND_FAILED));
        if (k->sotto) lv_label_set_text(k->sotto, "");
        break;
    }
}

/* --- pressione prolungata ----------------------------------------------- */

/* L'anello e arrivato in fondo: il gesto e compiuto. */
static void anello_pieno(lv_anim_t *a)
{
    lv_obj_t *anello = a->var;
    lv_obj_t *b = lv_obj_get_parent(anello);
    if (b) lv_obj_send_event(b, EV_COMANDO_SCATTATO, NULL);
}

/* Senza conferma scatta al click, che e il gesto piu breve che si possa
   chiedere per una cosa che non ne merita uno piu lungo. */
static void su_click(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_send_event(b, EV_COMANDO_SCATTATO, NULL);
}

static void su_premuto(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    comando_t *k = lv_obj_get_user_data(b);
    if (!k || !k->conferma || !k->anello) return;
    /* L'anello si riempie nel tempo scritto in configurazione — 1500 ms se
       non c'e scritto niente: se il dito si alza prima, rientra. Il numero
       non e cosmetico, e chi ha le mani ferme male lo alza. */
    lv_arc_set_value(k->anello, 0);
    lv_obj_clear_flag(k->anello, LV_OBJ_FLAG_HIDDEN);
    lv_anim_init(&k->avanzamento);
    lv_anim_set_var(&k->avanzamento, k->anello);
    lv_anim_set_exec_cb(&k->avanzamento,
                        (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_values(&k->avanzamento, 0, 100);
    lv_anim_set_duration(&k->avanzamento,
                         (uint32_t)cfg_intero("display/long_press_ms",
                                              T_PRESSIONE_PROLUNGATA));
    lv_anim_set_completed_cb(&k->avanzamento, anello_pieno);
    lv_anim_start(&k->avanzamento);
}

static void su_rilasciato(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    comando_t *k = lv_obj_get_user_data(b);
    if (!k || !k->conferma || !k->anello) return;
    lv_anim_delete(k->anello, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_arc_set_value(k->anello, 0);
    lv_obj_add_flag(k->anello, LV_OBJ_FLAG_HIDDEN);
}

/* --- costruzione -------------------------------------------------------- */

lv_obj_t *pulsante_comando(lv_obj_t *padre, const char *testo, bool conferma,
                           bool attivo)
{
    comando_t *k = lv_malloc_zeroed(sizeof *k);
    if (!k) return ui_pannello(padre, C_CARD2);

    k->nome = testo;
    k->conferma = conferma;

    lv_obj_t *b = ui_pannello(padre, C_ACC);
    lv_obj_set_size(b, PRF->tocco.apertura.w, PRF->tocco.apertura.h);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_ACC);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_user_data(b, k);
    lv_obj_add_event_cb(b, libera, LV_EVENT_DELETE, NULL);
    ui_tocco_minimo(b, PRF->tocco.apertura.w, PRF->tocco.apertura.h);

    lv_obj_t *riga = ui_pannello(b, C_ACC);
    lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
    lv_obj_set_size(riga, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(riga, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(riga, COM.pastiglia_gap, 0);

    k->icona = ui_icona(riga, "", C_INK, IC_S);
    k->testo = ui_testo(riga, testo, C_INK, FT_M);

    if (conferma) {
        k->sotto = ui_testo(b, tr(TX_COMMAND_HOLD), C_INK, FT_XS);
        const int32_t sp = (int32_t)PRF->font.f_xs
                         * (int32_t)COM.spaziatura_dock_millesimi / 1000;
        lv_obj_set_style_text_letter_space(k->sotto, sp, 0);

        /* Anello di avanzamento sopra il pulsante, nascosto a riposo. */
        k->anello = lv_arc_create(b);
        lv_obj_remove_style_all(k->anello);
        lv_obj_set_size(k->anello, PRF->tocco.apertura.h, PRF->tocco.apertura.h);
        lv_obj_center(k->anello);
        lv_arc_set_range(k->anello, 0, 100);
        lv_arc_set_value(k->anello, 0);
        lv_arc_set_bg_angles(k->anello, 270, 270 + 359);
        lv_obj_set_style_arc_width(k->anello, COM.tratto, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(k->anello, C_INK, LV_PART_INDICATOR);
        lv_obj_add_flag(k->anello, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(k->anello, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_add_event_cb(b, su_premuto, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(b, su_rilasciato, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(b, su_rilasciato, LV_EVENT_PRESS_LOST, NULL);
    } else if (attivo) {
        k->click_attaccato = true;
        lv_obj_add_event_cb(b, su_click, LV_EVENT_CLICKED, NULL);
    }

    if (attivo) lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    else        lv_obj_set_style_opa(b, ui_opa(COM.opacita_dato_vecchio_pct), 0);

    pulsante_comando_stato(b, CMD_PRONTO);
    return b;
}

/* --- cambiare un pulsante gia costruito --------------------------------- */

void pulsante_comando_testo(lv_obj_t *b, const char *testo)
{
    if (!b || !testo) return;
    comando_t *k = lv_obj_get_user_data(b);
    if (!k) return;

    /* Il nome di riposo cambia insieme all'etichetta: senza, il ritorno da
       "Inviato" a fine dei tre secondi rimetterebbe il testo di prima. */
    k->nome = testo;
    if (k->testo) ui_scrivi(k->testo, testo);
}

void pulsante_comando_attivo(lv_obj_t *b, bool attivo)
{
    if (!b) return;
    comando_t *k = lv_obj_get_user_data(b);
    if (!k) return;

    if (attivo) {
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(b, LV_OPA_COVER, 0);
        /* Il gestore del click si attacca alla nascita solo se il pulsante
           nasceva attivo e senza conferma: qui si rimedia una volta sola,
           senza rischiare di attaccarlo due volte. */
        if (!k->conferma && !k->click_attaccato) {
            k->click_attaccato = true;
            lv_obj_add_event_cb(b, su_click, LV_EVENT_CLICKED, NULL);
        }
    } else {
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(b, ui_opa(COM.opacita_dato_vecchio_pct), 0);
    }
}
