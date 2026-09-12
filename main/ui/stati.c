#include "stati.h"
#include <string.h>
#include <stdio.h>

#include "comuni.h"
#include "dati.h"
#include "nav.h"
#include "tempi.h"
#include "ui.h"
#include "versione.h"
#include "ha.h"
#include "widgets/logo.h"
#include "config.h"
#include "orologio.h"
#include "sistema.h"

static lv_obj_t *fascia;
static lv_obj_t *pieno;
static lv_obj_t *avvio;
static lv_obj_t *modale;
static lv_timer_t *annullo;

/* --- fascia di riconnessione — §4.2 ------------------------------------- */

/* I valori restano leggibili ma sbiaditi: sono l'ultima cosa che il
   pannello sa, non quello che sta succedendo adesso. */
static void sbiadisci_contenuto(bool si)
{
    lv_obj_t *c = ui_contenuto();
    if (!c) return;
    const lv_opa_t o = si ? ui_opa(COM.opacita_dato_vecchio_pct) : LV_OPA_COVER;
    for (uint32_t n = 0; n < lv_obj_get_child_count(c); n++) {
        lv_obj_t *f = lv_obj_get_child(c, n);
        if (f != fascia) lv_obj_set_style_opa(f, o, 0);
    }
}

/* --- perche questa fascia puo morire senza dirlo ------------------------
 *
 * `pieno`, `modale` e `avvio` stanno su lv_layer_top() e sopravvivono a una
 * ricostruzione della schermata. La fascia no: e figlia di ui_contenuto(),
 * che ui_vai_a() **distrugge** ogni volta che si cambia sezione o si rifa
 * la schermata dopo un comando. Da quel momento il puntatore qui sotto
 * punta a memoria che LVGL ha gia riusato, e la prima lv_obj_delete() su di
 * lui e un panico.
 *
 * E la stessa lezione dell'orologio nella home, in un altro file: **un
 * puntatore a un oggetto che qualcun altro puo distruggere va azzerato da
 * chi lo distrugge**, non custodito sperando che regga. Qui glielo dice la
 * fascia stessa, morendo.
 *
 * Il ricordo di cosa c'e scritto sopra si azzera insieme: senza, dopo una
 * ricostruzione la guardia direbbe "non e cambiato niente" e la fascia non
 * tornerebbe mai piu. */
static int  ultimo_tentativo = -1;
static char ultimo_testo[32] = "";

/* --- cancellare un oggetto tenuto in cache ------------------------------
 *
 * Questi puntatori sopravvivono al ciclo di vita degli oggetti che indicano:
 * li teniamo noi in variabili statiche, e chi distrugge l'oggetto puo essere
 * qualcun altro — un genitore che se ne va, una schermata rifatta. Quando
 * succede, la cancellazione successiva e una **doppia cancellazione**, e su
 * questo pannello finiva cosi:
 *
 *     Guru Meditation Error: Load access fault
 *       lv_obj_has_class   (l'asserzione di lv_obj_delete)
 *       lv_obj_delete
 *
 * lv_obj_is_valid() e l'idioma di LVGL per questo caso: non e una toppa, e
 * il modo giusto di trattare un puntatore di cui non si possiede la vita.
 *
 * **La causa a monte resta aperta**: qualcosa distrugge questi oggetti senza
 * dirlo a chi li tiene, e finche non ha un nome questa guardia impedisce il
 * crash ma non spiega il difetto. */
static void cancella_se_c_e(lv_obj_t **o)
{
    if (!o || !*o) return;
    if (lv_obj_is_valid(*o)) lv_obj_delete(*o);
    *o = NULL;
}

static void scorda_fascia(lv_event_t *e)
{
    LV_UNUSED(e);
    fascia = NULL;
    ultimo_tentativo = -1;
    ultimo_testo[0] = 0;
}

/* Toccando la fascia si va dove si rimedia. Non si nasconde la fascia: il
   collegamento e ancora giu e continuera a dirlo, ma dalle impostazioni si
   arriva alla pagina di configurazione e all'indirizzo da correggere. */
static void su_fascia(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_vai(SEZ_IMPOSTAZIONI);
}

void stato_riconnessione(int tentativo, const char *ultimo_dato)
{
    /* --- rifare costa, e qui si rifaceva sempre -------------------------
     *
     * Questa funzione distrugge la fascia e la ricostruisce. Chi la chiama
     * lo fa a ogni giro del ciclo grafico, perche "il tempo dell'ultimo
     * dato scorre" — ma quel testo cambia al massimo una volta al secondo,
     * e nel frattempo si buttavano via e si ricreavano una dozzina di
     * oggetti centinaia di volte al secondo.
     *
     * Sul pannello vero non e stata una lentezza: e stato un watchdog. Il
     * compito grafico non lasciava piu girare il task inattivo, e con lui
     * se ne andavano il tocco e tutto il resto — toccare "freddo" su un
     * condizionatore non faceva niente perche il tocco non arrivava mai a
     * essere letto.
     *
     * La guardia sta **qui** e non in chi chiama: e questa funzione a
     * sapere cosa costa, e chi chiama non deve ricordarselo. */
    const char *testo = ultimo_dato ? ultimo_dato : "";

    if (fascia && tentativo == ultimo_tentativo
        && strcmp(ultimo_testo, testo) == 0) return;

    ultimo_tentativo = tentativo;
    snprintf(ultimo_testo, sizeof ultimo_testo, "%s", testo);

    if (fascia) { lv_obj_delete(fascia); fascia = NULL; }

    /* --- la barra **non** si attenua, e prima si attenuava ---------------
     *
     * Attenuare un comando vuol dire una cosa sola a chi guarda: non si puo
     * usare. E la navigazione durante la riconnessione si puo usare
     * benissimo — sospesi sono i comandi verso Home Assistant, non lo
     * spostarsi fra le sezioni. L'interfaccia diceva il falso, e chi la
     * guardava le credeva: restava fermo davanti a una fascia arancione
     * senza provare a toccare la voce che l'avrebbe portato a correggere
     * l'indirizzo.
     *
     * Il contenuto invece si sbiadisce, e li e vero: quei valori sono
     * vecchi e nessuno li sta aggiornando. */
    if (tentativo <= 0) { sbiadisci_contenuto(false); return; }

    /* Sta in cima al contenuto della sezione, non sopra tutto: quello che
       c'e sotto si continua a leggere. */
    fascia = ui_pannello(ui_contenuto(), C_RICON_BG);
    lv_obj_add_event_cb(fascia, scorda_fascia, LV_EVENT_DELETE, NULL);

    /* Toccandola si va alle impostazioni. E l'unico elemento a schermo che
       stia dicendo che qualcosa non va, e quando la causa sta nella
       configurazione — indirizzo cambiato, token revocato — le impostazioni
       sono esattamente dove bisogna andare. Farglielo dire e farglielo fare
       costa una riga; lasciarla muta costa a chi deve indovinare. */
    lv_obj_add_flag(fascia, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fascia, su_fascia, LV_EVENT_CLICKED, NULL);
    lv_obj_set_width(fascia, LV_PCT(100));
    lv_obj_set_height(fascia, PRF->tocco.apertura.h);
    lv_obj_set_style_radius(fascia, PRF->geo.radius, 0);
    ui_bordo(fascia, LV_BORDER_SIDE_FULL, C_RICON_LINE);
    lv_obj_set_style_pad_hor(fascia, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(fascia, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fascia, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fascia, PRF->geo.gap, 0);
    lv_obj_move_to_index(fascia, 0);

    ui_icona(fascia, ICO_SYNC, C_RICON_TXT, IC_S);

    static char t[48];
    lv_snprintf(t, sizeof t, tr(TX_STATE_RECONNECTING),
                tentativo);
    ui_testo(fascia, t, C_RICON_TXT, FT_M);

    ui_spazio(fascia);

    static char u[80];
    lv_snprintf(u, sizeof u, tr(TX_STATE_LAST_DATA_TAP),
                ultimo_dato ? ultimo_dato : tr(TX_STATE_UNKNOWN));
    ui_testo(fascia, u, C_RICON_TXT, FT_S);

    sbiadisci_contenuto(true);
}

/* --- irraggiungibile — §4.3 --------------------------------------------- */

/* Quando l'utente ha chiesto di andare alle impostazioni, questa schermata
   non si rimette davanti finche' Home Assistant non torna: se lo facesse,
   lo butterebbe fuori mentre sta correggendo proprio la cosa che manca.
   Si azzera quando la situazione cambia — stato_ha_giu(false) — perche' un
   collegamento che cade **dopo** essere tornato e una notizia nuova. */
static bool zitto;

static void su_riprova(lv_event_t *e)
{
    LV_UNUSED(e);
    /* Non si chiude niente: se la riconnessione riesce ci pensa la
       sorveglianza a togliere la schermata, e se fallisce e giusto che
       resti. Chiuderla qui direbbe "fatto" quando non si sa ancora. */
    ha_riprova();
}

static void su_impostazioni(lv_event_t *e)
{
    LV_UNUSED(e);

    /* --- l'uscita di sicurezza ------------------------------------------
     *
     * Questa e la sola strada per rimettere in piedi un pannello che ha
     * perso Home Assistant per un motivo che sta nella sua configurazione:
     * l'indirizzo cambiato, un token revocato, una porta diversa. La pagina
     * di configurazione si sblocca dalle impostazioni, e senza questo
     * pulsante alle impostazioni non ci si arriva — la barra sta sotto una
     * schermata che copre tutto e si prende il tocco.
     *
     * Percio la schermata si toglie **subito**, e non si rimette da sola
     * finche' Home Assistant non risponde di nuovo. */
    zitto = true;
    stato_ha_giu(false, NULL);
    ui_vai(SEZ_IMPOSTAZIONI);
}


/* The logo at the bottom of the screens that cover everything, dimmed: a
   signature, not a title. When the panel has nothing good to show it at
   least says whose it is — a fault screen recognisable at a glance is
   better than any other. */
static void firma(lv_obj_t *padre)
{
    logo_foyer(padre, lv_font_get_line_height(font(FT_L)), true, C_DIM);
}

void stato_ha_giu(bool si, const char *ultimo_dato)
{
    cancella_se_c_e(&pieno);
    if (!si) { zitto = false; return; }

    /* Vedi `zitto`: l'utente e andato a sistemare, non lo si interrompe. */
    if (zitto) return;

    pieno = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(pieno, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(pieno, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(pieno, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pieno, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(pieno, PRF->geo.gap, 0);
    lv_obj_set_style_pad_all(pieno, PRF->geo.pad, 0);

    /* In alto restano rete e orologio: quelli funzionano, e dirlo evita di
       far pensare che sia tutto rotto. */
    lv_obj_t *alto = ui_pannello(pieno, C_BG);
    lv_obj_set_style_bg_opa(alto, LV_OPA_TRANSP, 0);
    lv_obj_set_width(alto, LV_PCT(100));
    lv_obj_set_height(alto, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alto, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(alto, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(alto, COM.pastiglia_gap, 0);
    /* The network as it really is. It said "network connected" and that
       was all, even when the screen appeared precisely because there was
       no network. */
    const rete_info_t ri = sistema_rete();
    ui_icona(alto, ri.connessa ? ICO_WIFI : ICO_WIFI_OFF,
             ri.connessa ? C_DIM : C_WARN, IC_S);
    ui_testo(alto, ri.connessa ? tr(TX_STATE_NETWORK_CONNECTED) : ri.descrizione,
             C_DIM, FT_S);
    ui_spazio(alto);
    ui_orologio(alto, FT_M);

    ui_spazio(pieno);

    ui_icona(pieno, ICO_CLOUD_OFF, C_WARN, IC_L);
    ui_testo(pieno, tr(TX_STATE_HA_DOWN), C_TXT, FT_L);

    static char u[80];
    lv_snprintf(u, sizeof u, tr(TX_STATE_LAST_VALID_DATA),
                ultimo_dato ? ultimo_dato : tr(TX_STATE_UNKNOWN));
    ui_testo(pieno, u, C_DIM, FT_M);

    static char r[64];
    lv_snprintf(r, sizeof r, tr(TX_STATE_RETRY_EVERY), T_RICONNESSIONE / 1000);
    ui_testo(pieno, r, C_DIM, FT_S);

    lv_obj_t *bottoni = ui_pannello(pieno, C_BG);
    lv_obj_set_style_bg_opa(bottoni, LV_OPA_TRANSP, 0);
    lv_obj_set_size(bottoni, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottoni, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bottoni, PRF->geo.gap, 0);

    /* I due pulsanti hanno un gestore, e per un po' non l'hanno avuto: si
       accendevano sotto il dito e non portavano da nessuna parte, che su una
       schermata che copre tutto vuol dire un pannello da riflashare. */
    static const struct {
        const char *ico;
        tx_t nome;
        lv_event_cb_t che_fa;
    } B[] = {
        { ICO_SYNC,     TX_STATE_RETRY_NOW, su_riprova      },
        { ICO_SETTINGS, TX_COMMON_SETTINGS, su_impostazioni },
    };
    for (unsigned n = 0; n < sizeof B / sizeof B[0]; n++) {
        lv_obj_t *b = ui_pannello(bottoni, C_CARD2);
        lv_obj_set_size(b, LV_SIZE_CONTENT, PRF->tocco.apertura.h);
        lv_obj_set_style_pad_hor(b, PRF->geo.pad, 0);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(b, COM.pastiglia_gap, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, B[n].che_fa, LV_EVENT_CLICKED, NULL);
        ui_tocco_minimo(b, PRF->tocco.apertura.w, PRF->tocco.apertura.h);
        ui_icona(b, B[n].ico, C_DIM, IC_S);
        ui_testo(b, tr(B[n].nome), C_TXT, FT_M);
    }

    ui_spazio(pieno);
    firma(pieno);
}

/* --- the Wi-Fi does not connect -------------------------------------------
 *
 * Before this screen, a wall panel that lost its Wi-Fi — router password
 * changed, network renamed — showed "Home Assistant is not responding" and
 * that was all, and the only way to put it back on the network was the
 * serial cable: the configuration page comes over Wi-Fi, that is exactly
 * the way that is closed. Now it says so, says why, and lets the glass do
 * what used to need a computer. */
static lv_obj_t *senza_rete;
static lv_obj_t *senza_rete_motivo;

static void scorda_senza_rete(lv_event_t *e)
{
    LV_UNUSED(e);
    senza_rete = NULL;
    senza_rete_motivo = NULL;
}

static void su_cambia_rete(lv_event_t *e) { LV_UNUSED(e); stato_cambia_rete(); }
static void su_riprova_rete(lv_event_t *e) { LV_UNUSED(e); sistema_rete_riprova(); }

void stato_rete_giu(bool si, const char *motivo)
{
    if (!si) { cancella_se_c_e(&senza_rete); return; }

    /* Already open: only the reason is updated. Redrawing it every second
       would repaint the whole screen to change one word. */
    if (senza_rete && lv_obj_is_valid(senza_rete)) {
        if (senza_rete_motivo) ui_scrivi(senza_rete_motivo, motivo ? motivo : "");
        return;
    }

    senza_rete = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(senza_rete, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(senza_rete, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(senza_rete, scorda_senza_rete, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(senza_rete, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(senza_rete, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(senza_rete, PRF->geo.gap, 0);
    lv_obj_set_style_pad_all(senza_rete, PRF->geo.pad, 0);

    /* At the top the clock, which works without a network too. */
    lv_obj_t *alto = ui_pannello(senza_rete, C_BG);
    lv_obj_set_style_bg_opa(alto, LV_OPA_TRANSP, 0);
    lv_obj_set_width(alto, LV_PCT(100));
    lv_obj_set_height(alto, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alto, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(alto, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_spazio(alto);
    ui_orologio(alto, FT_M);

    ui_spazio(senza_rete);

    ui_icona(senza_rete, ICO_WIFI_OFF, C_WARN, IC_L);
    ui_testo(senza_rete, tr(TX_STATE_WIFI_DOWN), C_TXT, FT_L);
    senza_rete_motivo = ui_testo(senza_rete, motivo ? motivo : "", C_DIM, FT_M);

    lv_obj_t *nota = ui_testo(senza_rete, tr(TX_STATE_WIFI_DOWN_NOTE), C_DIM, FT_S);
    lv_label_set_long_mode(nota, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(nota, LV_PCT(80));
    lv_obj_set_style_text_align(nota, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *bottoni = ui_pannello(senza_rete, C_BG);
    lv_obj_set_style_bg_opa(bottoni, LV_OPA_TRANSP, 0);
    lv_obj_set_size(bottoni, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottoni, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bottoni, PRF->geo.gap, 0);

    static const struct {
        const char *ico;
        tx_t nome;
        lv_event_cb_t che_fa;
        bool primo;
    } B[] = {
        { ICO_WIFI, TX_STATE_CHOOSE_NETWORK, su_cambia_rete,  true  },
        { ICO_SYNC, TX_STATE_RETRY_NOW,      su_riprova_rete, false },
    };
    for (unsigned n = 0; n < sizeof B / sizeof B[0]; n++) {
        lv_obj_t *b = ui_pannello(bottoni, B[n].primo ? C_ACC : C_CARD2);
        lv_obj_set_size(b, LV_SIZE_CONTENT, PRF->tocco.apertura.h);
        lv_obj_set_style_pad_hor(b, PRF->geo.pad, 0);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, B[n].primo ? C_ACC : C_LINE);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(b, COM.pastiglia_gap, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, B[n].che_fa, LV_EVENT_CLICKED, NULL);
        ui_tocco_minimo(b, PRF->tocco.apertura.w, PRF->tocco.apertura.h);
        ui_icona(b, B[n].ico, B[n].primo ? C_INK : C_DIM, IC_S);
        ui_testo(b, tr(B[n].nome), B[n].primo ? C_INK : C_TXT, FT_M);
    }

    ui_spazio(senza_rete);
    firma(senza_rete);
}

/* --- avvio — §4.1 ------------------------------------------------------- *
 *
 * It used to be a picture of a boot: six steps with times written by hand,
 * one of them the cameras the project no longer has, and nothing on the
 * panel ever showed it. Now it is shown at every power-up that has a
 * configuration, and it looks for itself: a timer asks the radio, the
 * network, the clock and Home Assistant how they are, ticks each step
 * when it succeeds and writes how long it took.
 *
 * It goes away when everything is up, after T_AVVIO_MASSIMO whatever
 * happened, or at a touch. It never holds the panel hostage: the note at
 * the bottom says so, and the timeout makes it true. */

enum { PASSO_ATTESA, PASSO_FATTO, PASSO_GUASTO };
enum { P_SCHERMO, P_RADIO, P_RETE, P_ORA, P_HA, P_QUANTI };

static struct {
    lv_timer_t *timer;
    uint32_t    partito;              /* lv_tick_get() when it appeared */
    uint32_t    finito;               /* when the last step succeeded   */
    uint8_t     esito[P_QUANTI];
    lv_obj_t   *icona[P_QUANTI];
    lv_obj_t   *tempo[P_QUANTI];
} av;

/* How a step is doing right now, read from its own source. */
static int esito_di(int p)
{
    switch (p) {
    case P_SCHERMO: return PASSO_FATTO;   /* it is drawing this */
    case P_RADIO:   return sistema_radio_pronta() ? PASSO_FATTO : PASSO_ATTESA;
    case P_RETE:    return sistema_rete().connessa ? PASSO_FATTO : PASSO_ATTESA;
    case P_ORA:     return orologio_valido() ? PASSO_FATTO : PASSO_ATTESA;
    case P_HA:
        switch (ha_stato()) {
        case HA_PRONTO:          return PASSO_FATTO;
        case HA_TOKEN_RIFIUTATO: return PASSO_GUASTO;
        default:                 return PASSO_ATTESA;
        }
    }
    return PASSO_ATTESA;
}

static void mostra_esito(int p, int esito, uint32_t ms)
{
    if (!av.icona[p]) return;
    lv_label_set_text(av.icona[p], esito == PASSO_FATTO ? ICO_CHECK
                                 : esito == PASSO_GUASTO ? ICO_CLOSE
                                 : ICO_HOURGLASS_EMPTY);
    lv_obj_set_style_text_color(av.icona[p], esito == PASSO_FATTO ? C_OK
                                           : esito == PASSO_GUASTO ? C_WARN
                                           : C_DIM, 0);
    char t[24];
    if (esito == PASSO_GUASTO)
        lv_snprintf(t, sizeof t, "%s", ui_ha_stato(ha_stato()));
    else if (esito == PASSO_FATTO && p != P_SCHERMO && ms > 0) {
        /* Seconds with one decimal, from when the screen appeared: that
           is when anyone started waiting. The display itself has no time —
           it is done by definition — and neither has a step that was
           already done when the screen appeared: "0.0 s" would be a number
           that says nothing. */
        lv_snprintf(t, sizeof t, "%u.%u s", (unsigned)(ms / 1000),
                    (unsigned)(ms % 1000 / 100));
        i18n_decimals(t);
    } else if (esito == PASSO_ATTESA)
        lv_snprintf(t, sizeof t, "—");
    else
        t[0] = 0;
    ui_scrivi(av.tempo[p], t);
}

static void controlla_avvio(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!avvio) return;
    const uint32_t adesso = lv_tick_get();
    bool tutti = true;
    for (int p = 0; p < P_QUANTI; p++) {
        if (!av.icona[p]) continue;          /* a step this board has not */
        if (av.esito[p] != PASSO_FATTO) {
            const int e = esito_di(p);
            if (e != av.esito[p]) {
                av.esito[p] = (uint8_t)e;
                mostra_esito(p, e, adesso - av.partito);
            }
        }
        if (av.esito[p] != PASSO_FATTO) tutti = false;
    }
    if (tutti && !av.finito) av.finito = adesso ? adesso : 1;
    if ((av.finito && adesso - av.finito >= T_AVVIO_FINITO)
            || adesso - av.partito >= T_AVVIO_MASSIMO)
        stato_avvio(false);
}

static void su_avvio(lv_event_t *e)
{
    LV_UNUSED(e);
    stato_avvio(false);
}

static void scorda_avvio(lv_event_t *e)
{
    LV_UNUSED(e);
    if (av.timer) { lv_timer_delete(av.timer); av.timer = NULL; }
    memset(av.icona, 0, sizeof av.icona);
    memset(av.tempo, 0, sizeof av.tempo);
    avvio = NULL;
}

void stato_avvio(bool si)
{
    cancella_se_c_e(&avvio);
    if (!si) return;

    avvio = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(avvio, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(avvio, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(avvio, su_avvio, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(avvio, scorda_avvio, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(avvio, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(avvio, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(avvio, PRF->geo.pad, 0);

    lv_obj_t *k = ui_scheda(avvio);
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    if (PRF->wifi.colonna_w) lv_obj_set_width(k, PRF->tocco.pin_box_w);
    else                     lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
    /* Touches go to the whole screen, not to the card on top of it. */
    lv_obj_remove_flag(k, LV_OBJ_FLAG_CLICKABLE);

    /* The logo instead of the little house, as tall as a line of the
       home screen's energy number: a size of the profile. The panel's own
       name under it when it has one — in a house with two panels the boot
       screen is also where you find out which one you are looking at.
       Without one, nothing: the logo already says Foyer, and repeating it
       would be an echo. */
    const char *nome = cfg_testo("system/panel_name", "");
    logo_foyer(k, lv_font_get_line_height(font(FT_ENERGIA)), true, C_TXT);
    if (nome[0]) ui_testo(k, nome, C_TXT, FT_L);
    ui_testo(k, PANNELLO_VERSIONE, C_DIM, FT_S);

    static const tx_t PASSI[P_QUANTI] = {
        [P_SCHERMO] = TX_BOOT_DISPLAY,
        [P_RADIO]   = TX_BOOT_COPROCESSOR,
        [P_RETE]    = TX_BOOT_WIFI,
        [P_ORA]     = TX_BOOT_TIME,
        [P_HA]      = TX_BOOT_HA,
    };
    memset(&av, 0, sizeof av);
    av.partito = lv_tick_get();

    /* The co-processor step only where that co-processor exists. */
    const bool con_c6 = PRF->comp.partizione_c6;
    for (int p = 0; p < P_QUANTI; p++) {
        if (p == P_RADIO && !con_c6) continue;

        lv_obj_t *r = ui_pannello(k, C_CARD);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_height(r, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);
        lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);

        av.icona[p] = ui_icona(r, ICO_HOURGLASS_EMPTY, C_DIM, IC_S);
        lv_obj_t *l = ui_testo(r, tr(PASSI[p]), C_TXT, FT_M);
        lv_obj_set_flex_grow(l, 1);
        av.tempo[p] = ui_testo(r, "—", C_DIM, FT_S);

        av.esito[p] = (uint8_t)esito_di(p);
        mostra_esito(p, av.esito[p], 0);
    }

    lv_obj_t *nota = ui_testo(k, tr(TX_BOOT_READ_ONLY_NOTE), C_DIM, FT_S);
    lv_label_set_long_mode(nota, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(nota, LV_PCT(100));
    ui_testo(k, tr(TX_BOOT_TAP_TO_SKIP), C_DIM, FT_S);

    av.timer = lv_timer_create(controlla_avvio, T_AVVIO_CONTROLLO, NULL);
}

/* Chiude cio che sta sopra l'interfaccia. Lo chiama la navigazione: se si
   arriva da qualche parte, quello che copriva non ha piu motivo di esserci. */
void stati_chiudi(void)
{
    if (annullo) { lv_timer_delete(annullo); annullo = NULL; }
    cancella_se_c_e(&modale);
    cancella_se_c_e(&pieno);
    tempi_sospendi(false);
}

/* --- conferma di apertura — §4.5 ---------------------------------------- */

static void chiudi_conferma(lv_event_t *e)
{
    LV_UNUSED(e);
    if (annullo) { lv_timer_delete(annullo); annullo = NULL; }
    cancella_se_c_e(&modale);
    tempi_sospendi(false);
}

/* Tocco sul velo, cioe fuori dalla scheda. Il modale si annulla — che e il
   gesto che chiunque si aspetta — e se il dito era sulla navigazione, che
   sotto il velo si vede ancora, la navigazione risponde: da un modale si
   deve poter tornare a casa (11-collaudo.md §1). */
static void su_velo(lv_event_t *e)
{
    lv_indev_t *dito = lv_indev_active();
    lv_point_t p = { 0, 0 };
    if (dito) lv_indev_get_point(dito, &p);

    chiudi_conferma(e);
    nav_inoltra_tocco(p);
}

/* L'azione che un modale di conferma sta aspettando, se ce n'e una.
   Dichiarata **una volta sola**: c'era una seconda dichiarazione identica
   centosessanta righe piu in giu, e in C due definizioni provvisorie dello
   stesso nome nello stesso file sono lo stesso oggetto — cosi funzionava,
   per caso. Leggendole una per volta sembravano due variabili diverse,
   ognuna col suo giro di azzeramenti, e la prima persona che ne avesse
   rinominata una avrebbe scoperto che l'altra la azzerava. */
static void (*azione_confermata)(void);

static void scade_conferma(lv_timer_t *t)
{
    LV_UNUSED(t);
    /* Lasciar scadere e uno dei tre modi di dire di no: l'azione che
       aspettava va dimenticata, non rimandata. */
    azione_confermata = NULL;
    annullo = NULL;
    cancella_se_c_e(&modale);
    tempi_sospendi(false);
}


/* --- una cosa da dire, senza niente da scegliere -------------------------
 *
 * Diversa da conferma_azione() per una ragione sola, ed e importante:
 * quella mette due pulsanti perche c'e una decisione da prendere. Qui non
 * c'e. Mostrare "Annulla" e "Ho capito" davanti a un messaggio che non
 * chiede niente insegna che i due pulsanti sono intercambiabili, e quella
 * e un'abitudine che poi si porta davanti a un ripristino di fabbrica.
 *
 * Si chiude toccando fuori, come gli altri modali. */
void avviso(const char *titolo, const char *testo)
{
    if (modale) lv_obj_delete(modale);

    tempi_sospendi(true);

    modale = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(modale, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(modale, ui_opa(COM.velo_conferma_pct), 0);
    lv_obj_add_flag(modale, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modale, su_velo, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(modale, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modale, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modale, PRF->geo.pad, 0);

    lv_obj_t *k = ui_scheda(modale);
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    if (PRF->tocco.pin_box_w) lv_obj_set_width(k, PRF->tocco.pin_box_w);
    else                      lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    if (titolo) ui_testo(k, titolo, C_TXT, FT_L);
    if (testo) {
        lv_obj_t *m = ui_testo(k, testo, C_DIM, FT_S);
        lv_obj_set_width(m, LV_PCT(100));
        lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
    }
    ui_testo(k, tr(TX_STATE_TAP_OUTSIDE), C_DIM, FT_S);
}

/* --- un QR a schermo pieno ----------------------------------------------
 *
 * Il registro di un pannello a muro si legge dal telefono, non dal
 * pannello: sul vetro ci stanno dodici righe e quelle che servono sono
 * quasi sempre piu vecchie. Il QR porta all'indirizzo di /api/log, che le
 * da tutte.
 *
 * Il codice si costruisce con la stessa lv_qrcode della condivisione
 * Wi-Fi, e prende la misura da PRF->wifi.qr invece di aggiungerne una
 * propria al profilo: quel numero risponde alla domanda "quanto grande sta
 * comodo un QR su questo schermo", che e la stessa domanda tutte e due le
 * volte. Un secondo campo con lo stesso valore sarebbe solo un posto in
 * piu da tenere allineato.
 */
void mostra_qr(const char *titolo, const char *testo, const char *sotto)
{
    if (!testo || !*testo) return;
    if (modale) lv_obj_delete(modale);

    tempi_sospendi(true);

    modale = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(modale, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(modale, ui_opa(COM.velo_conferma_pct), 0);
    lv_obj_add_flag(modale, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modale, su_velo, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(modale, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modale, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modale, PRF->geo.pad, 0);

    lv_obj_t *k = ui_scheda(modale);
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(k, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    if (titolo) ui_testo(k, titolo, C_TXT, FT_L);

    lv_obj_t *qr = lv_qrcode_create(k);
    lv_qrcode_set_size(qr, PRF->wifi.qr);
    /* Nero su bianco e non i colori del pannello: un QR ambra su fondo
       scuro molti telefoni non lo leggono, ed e il genere di cosa che si
       scopre col telefono in mano davanti al muro. */
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_update(qr, testo, lv_strlen(testo));

    /* L'indirizzo scritto sotto, per chi il QR non lo puo inquadrare. */
    if (sotto) ui_testo(k, sotto, C_DIM, FT_S);
    ui_testo(k, tr(TX_STATE_TAP_OUTSIDE), C_DIM, FT_S);
}


/* --- conferma di un'azione che non si torna indietro --------------------
 *
 * Esegue davvero, ed e la differenza che conta:
 * questa chiama. Il chiamante passa cosa fare, e la funzione garantisce che
 * venga fatto **solo** premendo il pulsante di conferma — non chiudendo col
 * velo, non lasciando scadere il tempo, non premendo Annulla.
 *
 * L'azione parte **dopo** aver chiuso il modale. Su un ripristino di
 * fabbrica quello che segue e un riavvio, e riavviare con un oggetto LVGL
 * ancora aperto vuol dire lasciare a meta la struttura che il prossimo
 * avvio si ritrova; ma vale in generale: chi agisce non deve trovarsi
 * addosso la finestra che l'ha chiamato. */
static void su_conferma_si(lv_event_t *e)
{
    LV_UNUSED(e);
    void (*f)(void) = azione_confermata;

    azione_confermata = NULL;
    if (annullo) { lv_timer_delete(annullo); annullo = NULL; }
    cancella_se_c_e(&modale);
    tempi_sospendi(false);

    if (f) f();
}

static void su_conferma_no(lv_event_t *e)
{
    LV_UNUSED(e);
    azione_confermata = NULL;
    chiudi_conferma(e);
}

void conferma_azione(const char *titolo, const char *dettaglio,
                     const char *etichetta, bool pericolosa,
                     void (*azione)(void))
{
    if (modale) lv_obj_delete(modale);
    azione_confermata = azione;

    tempi_sospendi(true);

    modale = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(modale, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(modale, ui_opa(COM.velo_conferma_pct), 0);
    lv_obj_add_flag(modale, LV_OBJ_FLAG_CLICKABLE);
    /* Toccare il velo e uno dei tre modi di dire di no. */
    lv_obj_add_event_cb(modale, su_conferma_no, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(modale, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modale, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modale, PRF->geo.pad, 0);

    lv_obj_t *k = ui_scheda(modale);
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);   /* la scheda non annulla */
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    if (PRF->tocco.pin_box_w) lv_obj_set_width(k, PRF->tocco.pin_box_w);
    else                      lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    ui_icona(k, ICO_WARNING, pericolosa ? C_RICON_TXT : C_ACC, IC_L);
    ui_testo(k, titolo, C_TXT, FT_L);

    if (dettaglio && *dettaglio) {
        lv_obj_t *d = ui_testo(k, dettaglio, C_DIM, FT_S);
        lv_obj_set_width(d, LV_PCT(100));
        lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *bottoni = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(bottoni, LV_OPA_TRANSP, 0);
    lv_obj_set_width(bottoni, LV_PCT(100));
    lv_obj_set_height(bottoni, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottoni, LV_FLEX_FLOW_ROW);
    /* Larghi uguali e distanti: la distanza serve a non premere quello
       sbagliato con il pollice. Su un'azione che non si torna indietro
       conta il doppio. */
    lv_obj_set_style_pad_column(bottoni, PRF->geo.pad, 0);

    for (int n = 0; n < 2; n++) {
        const bool si = n == 1;
        lv_obj_t *b = ui_pannello(bottoni,
                                  si ? (pericolosa ? C_RICON_BG : C_ACC)
                                     : C_CARD2);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, PRF->tocco.apertura.h);
        lv_obj_set_style_min_width(b, 0, 0);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL,
                 si ? (pericolosa ? C_RICON_LINE : C_ACC) : C_LINE);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(b, si ? su_conferma_si : su_conferma_no,
                            LV_EVENT_CLICKED, NULL);
        ui_testo(b, si ? etichetta : tr(TX_COMMON_CANCEL),
                 si ? (pericolosa ? C_RICON_TXT : C_INK) : C_TXT, FT_M);
        /* Dopo i figli, non prima: altrimenti l'etichetta dentro il pulsante
           resta l'unica parte che prende il tocco. */
        ui_tocco_su_tutto(b);
    }

    static char scadenza[48];
    lv_snprintf(scadenza, sizeof scadenza,
                tr(TX_STATE_CANCELS_IN), T_CONFERMA_ANNULLO / 1000);
    ui_testo(k, scadenza, C_DIM, FT_S);

    annullo = lv_timer_create(scade_conferma, T_CONFERMA_ANNULLO, NULL);
    lv_timer_set_repeat_count(annullo, 1);
}
