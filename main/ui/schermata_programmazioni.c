/* ------------------------------------------------------------------------
 * Programmazioni — 01-specifica-ui.md §3.10.
 *
 * Un elenco solo che scorre, e i gruppi che si aprono sul posto. E la
 * proposta C dei mockup, scelta perche e la piu vicina alla view di Home
 * Assistant da cui questa sezione viene: li e una colonna di schede, qui
 * anche.
 *
 * **Chiuso, un gruppo mostra le sue finestre.** Non e un dettaglio: e la
 * ragione per cui uno entra in questa sezione. Un'intestazione che dicesse
 * solo il nome costringerebbe ad aprire tutti e sei i gruppi per sapere a
 * che ora si accende la pompa, cioe a fare sei tocchi per una domanda sola.
 * L'apertura serve alle **automazioni**, che si toccano due volte l'anno.
 *
 * **La schermata scorre.** E l'unica sezione del pannello che lo fa oltre
 * al registro e all'agenda, e qui serve davvero: sei gruppi chiusi ci stanno,
 * uno aperto con nove automazioni no, e impaginare un elenco che si apre e
 * si chiude vorrebbe dire far saltare le pagine sotto il dito.
 *
 * **Gli orari si toccano.** Ognuno e una pastiglia che apre il tastierino a
 * pagina piena (vedi tastiera_ora.c). Il tastierino e uno, e vale per tutte
 * le finestre: cambia solo cosa scrive quando si conferma.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "sorveglianza.h"
#include "tempi.h"
#include "ui.h"
#include "widgets/interruttore.h"
#include "widgets/tastiera_ora.h"

/* Quale gruppo e aperto sta nella **vista**, come il dettaglio del
   condizionatore: zero vuol dire tutti chiusi, `1 + n` il gruppo n.
   Tenerlo in una variabile di questo file funzionerebbe uguale sul vetro,
   ma la vista e cio che il pannello sa riaprire — e cio che il simulatore
   sa mostrare, quindi la schermata con un gruppo aperto si cattura come
   tutte le altre invece di doverla guardare a mano. */
static int aperto_ora(void)
{
    return ui_vista() - 1;
}

static void su_gruppo(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    ui_vai_a(SEZ_PROGRAMMAZIONI, aperto_ora() == n ? 0 : n + 1);
}

static void su_interruttore_gruppo(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    const programmazione_t *p = dati_programmazione(n);
    if (!p) return;
    dati_programmazione_premi(n, !p->acceso);
    tempi_risveglia();
    ui_vai_a(SEZ_PROGRAMMAZIONI, ui_vista());
}

static void su_automazione(lv_event_t *e)
{
    /* Gruppo e automazione in un intero solo: il gruppo nei bit alti.
       Un puntatore alla riga sarebbe vecchio al primo aggiornamento che
       arriva da Home Assistant, perche la schermata si ricostruisce. */
    const int d = (int)(intptr_t)lv_event_get_user_data(e);
    const int g = d >> 8, n = d & 0xff;
    const automazione_t *a = dati_automazione(g, n);
    if (!a) return;
    dati_automazione_abilita(g, n, !a->attiva);
    tempi_risveglia();
    ui_vai_a(SEZ_PROGRAMMAZIONI, ui_vista());
}

/* --- una pastiglia con un orario dentro ---------------------------------
 *
 * Il bersaglio e la pastiglia e non il numero: un numero e alto quanto il
 * suo corpo, e mirare a un'altezza di venti pixel su un vetro verticale
 * non riesce. La pastiglia ha il minimo di tocco garantito da ui_tocco_minimo. */
static void su_orario(lv_event_t *e)
{
    const int d = (int)(intptr_t)lv_event_get_user_data(e);
    tastiera_ora_apri(d >> 9, (d >> 1) & 0xff, (d & 1) != 0);
}

static lv_obj_t *pastiglia_ora(lv_obj_t *padre, int16_t minuti, bool acceso,
                               int gruppo, int finestra, bool capo)
{
    static char testo[8][8];
    static uint8_t giro;
    char *b = testo[giro++ % 8];

    if (minuti == ORARIO_IGNOTO) lv_snprintf(b, sizeof testo[0], "--:--");
    else lv_snprintf(b, sizeof testo[0], "%02d:%02d", minuti / 60, minuti % 60);

    lv_obj_t *p = ui_pannello(padre, C_CARD2);
    lv_obj_set_size(p, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(p, PRF->geo.radius_btn, 0);
    lv_obj_set_style_pad_hor(p, PRF->geo.pad, 0);
    lv_obj_set_style_pad_ver(p, COM.pastiglia_pad_v, 0);
    ui_bordo(p, LV_BORDER_SIDE_FULL, acceso ? C_ACC : C_LINE);
    ui_testo(p, b, acceso ? C_ACC : C_TXT, FT_L);

    if (minuti != ORARIO_IGNOTO) {
        lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
        ui_tocco_su_tutto(p);
        lv_obj_add_event_cb(p, su_orario, LV_EVENT_CLICKED,
                            (void *)(intptr_t)((gruppo << 9) | (finestra << 1)
                                               | (capo ? 1 : 0)));
    }
    return p;
}

/* --- la riga di una finestra -------------------------------------------- */

static void riga_finestra(lv_obj_t *padre, const finestra_t *f,
                          int gruppo, int n)
{
    lv_obj_t *r = ui_pannello(padre, C_CARD);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

    /* Il numero della finestra — o «dalle» per una di validita, che non e
       la prima di una serie ma un intervallo solo.
       Il numero e nudo e non «1ª»: l'indicatore ordinale femminile non e
       fra i caratteri compilati nel font, e sul vetro veniva un rettangolo
       vuoto. Aggiungerlo al font per due lettere costerebbe piu di quanto
       valgano. */
    static char et[4][12];
    static uint8_t giro;
    char *e = et[giro++ % 4];
    if (f->validita) lv_snprintf(e, sizeof et[0], "%s", tr(TX_SCHEDULES_FROM));
    else             lv_snprintf(e, sizeof et[0], "%d", n + 1);
    ui_testo(r, e, C_DIM, FT_S);

    pastiglia_ora(r, f->da_min, f->in_corso, gruppo, n, true);
    ui_icona(r, ICO_ARROW_FORWARD, C_DIM, IC_S);
    pastiglia_ora(r, f->a_min, f->in_corso, gruppo, n, false);

    /* Quanto dura, in fondo. Serve a leggere una finestra senza sottrarre
       a mente, e a vedere a colpo d'occhio quella che dura troppo. */
    if (f->da_min != ORARIO_IGNOTO && f->a_min != ORARIO_IGNOTO) {
        int d = f->a_min - f->da_min;
        if (d <= 0) d += 24 * 60;       /* scavalca la mezzanotte */
        static char q[4][16];
        char *b = q[giro % 4];
        if (d % 60) lv_snprintf(b, sizeof q[0], "%d h %02d", d / 60, d % 60);
        else        lv_snprintf(b, sizeof q[0], "%d h", d / 60);
        lv_obj_t *l = ui_testo(r, b, C_DIM, FT_S);
        lv_obj_set_flex_grow(l, 1);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    }
}

/* --- la riga di un'automazione ------------------------------------------ */

static void riga_automazione(lv_obj_t *padre, const automazione_t *a,
                             int gruppo, int n)
{
    lv_obj_t *r = ui_pannello(padre, C_CARD);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

    /* Un'automazione disattivata si attenua **e lo dice a parole**: un
       interruttore grigio, da un metro e mezzo, si distingue male da uno
       acceso, e la differenza fra le due cose qui e tutto. */
    if (!a->attiva) lv_obj_set_style_opa(r, ui_opa(COM.opacita_dato_vecchio_pct), 0);

    ui_icona(r, ICO_TIMER, a->attiva ? C_ACC : C_DIM, IC_S);

    lv_obj_t *col = ui_pannello(r, C_CARD);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *nm = ui_testo(col, a->nome && a->nome[0] ? a->nome : "—",
                            C_TXT, FT_M);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, LV_PCT(100));

    ui_testo(col, !a->disponibile ? tr(TX_COMMON_NOT_RESPONDING)
                 : !a->attiva     ? tr(TX_SCHEDULES_DISABLED)
                 : a->ultimo_scatto ? a->ultimo_scatto
                                    : tr(TX_SCHEDULES_NEVER_TRIGGERED),
             C_DIM, FT_XS);

    lv_obj_t *sw = interruttore(r, a->attiva, a->disponibile);
    if (a->disponibile && !sorveglianza_comandi_sospesi())
        lv_obj_add_event_cb(sw, su_automazione, LV_EVENT_CLICKED,
                            (void *)(intptr_t)((gruppo << 8) | n));
}

/* --- l'intestazione di un gruppo ---------------------------------------- */

/* --- l'intestazione, che e anche il bersaglio ---------------------------
 *
 * Apre e chiude il gruppo. **Non la scheda intera**: dentro la scheda ci
 * sono le pastiglie degli orari, che sono bersagli loro, e un dito che
 * cadesse fra due pastiglie aprirebbe il gruppo invece di non fare niente —
 * o peggio, chi mira a un orario e sbaglia di poco si vede chiudere il
 * gruppo sotto le dita. L'intestazione e alta abbastanza da bastare.
 *
 * L'interruttore del dispositivo resta un bersaglio suo: e sopra
 * l'intestazione, e LVGL manda il tocco al piu in alto dei due. Le due
 * azioni sono diverse — accendere e aprire — e meritano due bersagli. */
static void intestazione(lv_obj_t *padre, const programmazione_t *p, int n)
{
    lv_obj_t *t = ui_pannello(padre, C_CARD);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_set_width(t, LV_PCT(100));
    lv_obj_set_height(t, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(t, PRF->geo.gap, 0);

    ui_icona(t, icona_da_nome(p->icona, ICO_TIMER),
             p->acceso ? C_ACC : C_DIM, IC_M);

    lv_obj_t *col = ui_pannello(t, C_CARD);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *nm = ui_testo(col, p->nome && p->nome[0] ? p->nome : "—",
                            C_TXT, FT_L);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, LV_PCT(100));

    /* Sotto il nome, cosa sta facendo adesso. Con un dispositivo: acceso o
       spento, e i watt se li misura. Senza — un gruppo di sola validita —
       si dice quello che si sa, cioe quante automazioni governa. */
    static char sotto[12][40];
    static uint8_t giro;
    char *s = sotto[giro++ % 12];
    if (!p->c_e_entita)
        lv_snprintf(s, sizeof sotto[0], trn(TXN_SCHEDULES_AUTOMATIONS, p->automazioni),
                    p->automazioni);
    else if (!p->disponibile)
        lv_snprintf(s, sizeof sotto[0], "%s", tr(TX_COMMON_NOT_RESPONDING));
    else if (p->potenza_c_e)
        lv_snprintf(s, sizeof sotto[0], "%s · %d W",
                    p->acceso ? tr(TX_SWITCHES_ON) : tr(TX_SWITCHES_OFF), (int)p->watt);
    else
        lv_snprintf(s, sizeof sotto[0], "%s", p->acceso ? tr(TX_SWITCHES_ON)
                                                      : tr(TX_SWITCHES_OFF));
    ui_testo(col, s, p->acceso ? C_ACC : C_DIM, FT_S);

    lv_obj_t *sw = NULL;
    if (p->c_e_entita) {
        sw = interruttore(t, p->acceso, p->disponibile);
        if (p->disponibile && !sorveglianza_comandi_sospesi())
            lv_obj_add_event_cb(sw, su_interruttore_gruppo, LV_EVENT_CLICKED,
                                (void *)(intptr_t)n);
        else
            sw = NULL;   /* si vede, non si preme */
    }

    /* La freccia dice cosa succede toccando, e non e un pulsante a parte:
       il bersaglio e tutta l'intestazione. */
    ui_icona(t, aperto_ora() == n ? ICO_EXPAND_MORE : ICO_CHEVRON_RIGHT,
             C_DIM, IC_S);

    lv_obj_add_event_cb(t, su_gruppo, LV_EVENT_CLICKED, (void *)(intptr_t)n);
    /* **Dopo** aver costruito il contenuto, e non prima: i contenitori
       nascono premibili e si mangiano il tocco: la colonna del nome sta in
       mezzo all'intestazione e prendeva tutto. E successo tre volte in
       questo progetto, e la prova del dito lo ha visto anche stavolta.

       L'interruttore se lo riprende subito dopo: e sopra l'intestazione,
       e LVGL manda il tocco al piu in alto dei due. */
    ui_tocco_su_tutto(t);
    if (sw) ui_tocco_su_tutto(sw);
}

/* --- costruzione -------------------------------------------------------- */

void schermata_programmazioni(lv_obj_t *c)
{
    const int quanti = dati_programmazioni();

    int attive = 0;
    for (int n = 0; n < quanti; n++) {
        const programmazione_t *p = dati_programmazione(n);
        if (p && p->acceso) attive++;
    }

    static char sotto[48];
    if (quanti == 0)
        lv_snprintf(sotto, sizeof sotto, "%s", tr(TX_SCHEDULES_NO_GROUPS));
    else {
        char g[24], a[32];
        lv_snprintf(g, sizeof g, trn(TXN_SCHEDULES_GROUPS, quanti), quanti);
        lv_snprintf(a, sizeof a, trn(TXN_SCHEDULES_ACTIVE_NOW, attive), attive);
        lv_snprintf(sotto, sizeof sotto, "%s · %s", g, a);
    }
    ui_testata(tr(TX_SCHEDULES_TITLE), sotto);

    const int aperto = aperto_ora() < quanti ? aperto_ora() : -1;

    /* Il contenitore che scorre. ui_pannello() toglie la scorrevolezza a
       ogni pannello — e giusto, perche una schermata che scorre per sbaglio
       e peggio di una che non ci sta — e qui si rimette apposta. */
    lv_obj_t *elenco = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(elenco, LV_OPA_TRANSP, 0);
    lv_obj_set_width(elenco, LV_PCT(100));
    lv_obj_set_flex_grow(elenco, 1);
    lv_obj_set_flex_flow(elenco, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(elenco, PRF->geo.gap, 0);
    lv_obj_add_flag(elenco, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(elenco, LV_DIR_VER);

    for (int n = 0; n < quanti; n++) {
        const programmazione_t *p = dati_programmazione(n);
        if (!p) continue;

        lv_obj_t *k = ui_scheda(elenco);
        lv_obj_set_width(k, LV_PCT(100));
        lv_obj_set_height(k, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

        intestazione(k, p, n);

        /* Le finestre si vedono **anche da chiuso**: sono la ragione per
           cui uno entra qui. Se il gruppo non ne ha, non si lascia il posto
           vuoto. */
        for (int f = 0; f < p->finestre; f++) {
            const finestra_t *w = dati_finestra(n, f);
            if (w) riga_finestra(k, w, n, f);
        }

        if (aperto == n) {
            if (p->automazioni) ui_occhiello(k, tr(TX_SCHEDULES_AUTOMATIONS_HEADER));
            for (int a = 0; a < p->automazioni; a++) {
                const automazione_t *u = dati_automazione(n, a);
                if (u) riga_automazione(k, u, n, a);
            }
            /* Una finestra di validita non accende niente, e senza una riga
               che lo dica «22:00 → 06:00» si legge come un'accensione. Si
               scrive una volta sola per gruppo, sotto le automazioni che
               quel permesso lo usano. */
            for (int f = 0; f < p->finestre; f++) {
                const finestra_t *w = dati_finestra(n, f);
                if (!w || !w->validita) continue;
                ui_testo(k, tr(TX_SCHEDULES_OUTSIDE_HOURS),
                         C_DIM, FT_XS);
                break;
            }
        }

    }

    if (quanti == 0)
        ui_testo(elenco, tr(TX_SCHEDULES_NONE), C_DIM, FT_M);
}
