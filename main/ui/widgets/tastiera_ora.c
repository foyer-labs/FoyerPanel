/* ------------------------------------------------------------------------
 * Il tastierino di un orario. Vedi tastiera_ora.h per il perche.
 * --------------------------------------------------------------------- */
#include "widgets/tastiera_ora.h"

#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "tempi.h"
#include "ui.h"

/* Cosa si sta cambiando, e a che ora si e arrivati girando i tasti. */
static lv_obj_t *modale;
static lv_obj_t *cifra_ore, *cifra_minuti;
static int gruppo_ap, finestra_ap;
static bool capo_ap;
static int minuti_ap;

/* Il passo dei minuti. Cinque e il piu grosso che non impedisce niente: gli
   orari di casa sono ai quarti e alle mezze, e chi ne volesse uno ai
   ventitre minuti puo tenere premuto. */
#define PASSO_MINUTI 5

static void scrivi_cifre(void)
{
    static char o[4], m[4];
    lv_snprintf(o, sizeof o, "%02d", minuti_ap / 60);
    lv_snprintf(m, sizeof m, "%02d", minuti_ap % 60);
    ui_scrivi(cifra_ore, o);
    ui_scrivi(cifra_minuti, m);
}

static void chiudi(void)
{
    if (modale) lv_obj_delete(modale);
    modale = NULL;
    cifra_ore = cifra_minuti = NULL;
    tempi_sospendi(false);
}

static void su_fuori(lv_event_t *e)
{
    LV_UNUSED(e);
    chiudi();
}

static void su_annulla(lv_event_t *e)
{
    LV_UNUSED(e);
    chiudi();
}

static void su_salva(lv_event_t *e)
{
    LV_UNUSED(e);
    const int g = gruppo_ap, f = finestra_ap, m = minuti_ap;
    const bool capo = capo_ap;
    chiudi();
    dati_finestra_imposta(g, f, capo, m);
    /* Si ridisegna: l'orario nuovo arrivera da Home Assistant come stato
       dell'input_datetime, ma la schermata dietro sta ancora mostrando
       quello vecchio e chi ha appena salvato guarda li. */
    ui_vai(SEZ_PROGRAMMAZIONI);
}

/* I quattro tasti. Il dato dice cosa muovere e di quanto: ore o minuti,
   avanti o indietro. */
static void su_passo(lv_event_t *e)
{
    const int d = (int)(intptr_t)lv_event_get_user_data(e);
    minuti_ap += d;
    /* Gira invece di fermarsi: da 23 a 00 e un passo solo, e chi arriva in
       fondo alla scala vuole quasi sempre l'altro capo. */
    while (minuti_ap < 0)        minuti_ap += 24 * 60;
    while (minuti_ap >= 24 * 60) minuti_ap -= 24 * 60;
    scrivi_cifre();
}

static lv_obj_t *tasto(lv_obj_t *padre, const char *ico, int passo)
{
    lv_obj_t *b = ui_pannello(padre, C_CARD2);
    lv_obj_set_size(b, PRF->tocco.clima_pm_dettaglio.w,
                       PRF->tocco.clima_pm_dettaglio.h);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_icona(b, ico, C_TXT, IC_M);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    ui_tocco_su_tutto(b);
    lv_obj_add_event_cb(b, su_passo, LV_EVENT_CLICKED, (void *)(intptr_t)passo);
    return b;
}

/* Una colonna: piu, la cifra, meno. */
static lv_obj_t *colonna(lv_obj_t *padre, int passo)
{
    lv_obj_t *c = ui_pannello(padre, C_CARD);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_size(c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(c, PRF->geo.gap, 0);

    tasto(c, ICO_ADD, passo);
    lv_obj_t *cifra = ui_testo(c, "00", C_TXT, FT_XXL);
    tasto(c, ICO_REMOVE, -passo);
    return cifra;
}

static lv_obj_t *pulsante(lv_obj_t *padre, const char *testo, bool primario,
                          lv_event_cb_t gestore)
{
    lv_obj_t *b = ui_pannello(padre, primario ? C_ACC : C_CARD2);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, PRF->tocco.apertura.h);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    if (!primario) ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_testo(b, testo, primario ? C_INK : C_TXT, FT_M);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    ui_tocco_su_tutto(b);
    lv_obj_add_event_cb(b, gestore, LV_EVENT_CLICKED, NULL);
    return b;
}

void tastiera_ora_apri(int gruppo, int finestra, bool capo)
{
    const programmazione_t *p = dati_programmazione(gruppo);
    const finestra_t *f = dati_finestra(gruppo, finestra);
    if (!p || !f) return;

    const int16_t adesso = capo ? f->da_min : f->a_min;
    if (adesso == ORARIO_IGNOTO) return;

    if (modale) lv_obj_delete(modale);
    gruppo_ap = gruppo;
    finestra_ap = finestra;
    capo_ap = capo;
    minuti_ap = adesso;

    /* Lo standby non deve scattare mentre qualcuno sta scegliendo un'ora:
       il tocco sui tasti lo rimanda, ma chi si ferma a pensare no. */
    tempi_sospendi(true);

    modale = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(modale, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(modale, ui_opa(COM.velo_conferma_pct), 0);
    lv_obj_add_flag(modale, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modale, su_fuori, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(modale, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modale, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modale, PRF->geo.pad, 0);

    lv_obj_t *k = ui_scheda(modale);
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);   /* la scheda non chiude */
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    if (PRF->tocco.pin_box_w) lv_obj_set_width(k, PRF->tocco.pin_box_w);
    else                      lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    /* Cosa si sta cambiando: senza, un tastierino aperto sopra un velo e un
       orario senza soggetto — e con quattro finestre in casa non si sa piu
       quale si stava toccando. */
    static char che[48];
    if (f->validita)
        lv_snprintf(che, sizeof che, capo ? tr(TX_SCHEDULE_WINDOW_START)
                                          : tr(TX_SCHEDULE_WINDOW_END), p->nome);
    else
        lv_snprintf(che, sizeof che, capo ? tr(TX_SCHEDULE_SWITCH_ON)
                                          : tr(TX_SCHEDULE_SWITCH_OFF), p->nome);
    ui_occhiello(k, che);

    lv_obj_t *riga = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
    lv_obj_set_size(riga, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(riga, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(riga, PRF->geo.gap, 0);

    cifra_ore = colonna(riga, 60);
    ui_testo(riga, ":", C_DIM, FT_XXL);
    cifra_minuti = colonna(riga, PASSO_MINUTI);
    scrivi_cifre();

    lv_obj_t *bottoni = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(bottoni, LV_OPA_TRANSP, 0);
    lv_obj_set_width(bottoni, LV_PCT(100));
    lv_obj_set_height(bottoni, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottoni, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bottoni, PRF->geo.gap, 0);

    pulsante(bottoni, tr(TX_COMMON_CANCEL), false, su_annulla);
    pulsante(bottoni, tr(TX_COMMON_SAVE), true, su_salva);
}
