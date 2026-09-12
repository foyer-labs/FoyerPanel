/* ------------------------------------------------------------------------
 * Impostazioni a bordo — 01-specifica-ui.md §3.8.
 *
 * Contiene **solo cio che serve quando la rete non funziona**: rete e
 * cambio rete, indirizzo, stato di Home Assistant, luminosita,
 * l'interruttore "Consenti configurazione", riavvio. Tutto il resto sta
 * nella pagina web, che pero senza rete non si raggiunge — ed e la ragione
 * per cui questa schermata esiste ed e corta.
 *
 * Menu a sinistra in orizzontale, elenco unico scorrevole in verticale
 * (09-profili.md §1-ter). La voce Sistema apre la diagnostica.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "sistema.h"
#include "registro.h"
#include "ripristino.h"
#include "ha.h"
#include "config.h"
#include "schermate.h"
#include "stati.h"
#include "tempi.h"
#include "ui.h"
#include "versione.h"
#include "web.h"
#include "widgets/interruttore.h"
#include "widgets/logo.h"

static void su_sblocco(lv_event_t *e)
{
    LV_UNUSED(e);
    if (web_sbloccato()) web_richiudi();
    else                 web_sblocca();
    ui_vai_a(SEZ_IMPOSTAZIONI, ui_vista());   /* si ridisegna col conto */
}

/* Le voci del menu, che sono anche le viste della sezione. */
enum { V_RETE = 0, V_HA, V_SCHERMO, V_SISTEMA, V_INFO, V_QUANTE };

/* Home Assistant takes the house, its own symbol; the «i» goes to
   Information, which is where one looks for it. */
static const struct { tx_t nome; const char *icona; } VOCI[V_QUANTE] = {
    { TX_SETTINGS_NETWORK, ICO_ROUTER },
    { TX_SETTINGS_HA,      ICO_HOME },
    { TX_SETTINGS_DISPLAY, ICO_BRIGHTNESS_MEDIUM },
    { TX_SETTINGS_SYSTEM,  ICO_MEMORY },
    { TX_SETTINGS_INFO,    ICO_INFO },
};

static void su_voce(lv_event_t *e)
{
    ui_vai_a(SEZ_IMPOSTAZIONI, (int)(intptr_t)lv_event_get_user_data(e));
}

/* --- righe del pannello di destra --------------------------------------- */

static lv_obj_t *riga(lv_obj_t *padre, const char *nome, const char *sotto)
{
    lv_obj_t *r = ui_scheda(padre);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

    lv_obj_t *col = ui_pannello(r, C_CARD);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *l = ui_testo(col, nome, C_TXT, FT_M);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
    if (sotto) {
        lv_obj_t *s = ui_testo(col, sotto, C_DIM, FT_S);
        lv_label_set_long_mode(s, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(s, LV_PCT(100));
    }
    return r;
}

static void riga_valore(lv_obj_t *padre, const char *nome, const char *sotto,
                        const char *valore, lv_color_t colore)
{
    lv_obj_t *r = riga(padre, nome, sotto);
    ui_testo(r, valore, colore, FT_M);
}

static void riga_azione(lv_obj_t *padre, const char *nome, const char *sotto,
                        const char *ico, const char *azione,
                        lv_event_cb_t gestore)
{
    lv_obj_t *r = riga(padre, nome, sotto);
    lv_obj_t *b = ui_pannello(r, C_CARD2);
    lv_obj_set_size(b, LV_SIZE_CONTENT, PRF->tocco.apertura.h);
    lv_obj_set_style_pad_hor(b, PRF->geo.pad, 0);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, COM.pastiglia_gap, 0);
    ui_icona(b, ico, C_DIM, IC_S);
    ui_testo(b, azione, C_TXT, FT_S);

    /* Prima questi pulsanti erano cliccabili e basta: nessuno li ascoltava,
       quindi si premevano e non succedeva niente. Un pulsante che non fa
       niente e peggio di un pulsante che non c'e, perche chi lo preme pensa
       di aver sbagliato lui. Adesso il gestore e obbligatorio nella firma. */
    ui_tocco_su_tutto(b);
    lv_obj_add_event_cb(b, gestore, LV_EVENT_CLICKED, NULL);
}

static void su_riconnetti(lv_event_t *e)
{
    LV_UNUSED(e);
    ha_riprova();
    ui_vai_a(SEZ_IMPOSTAZIONI, ui_vista());
}

static void su_riavvia(lv_event_t *e)
{
    LV_UNUSED(e);
    sistema_riavvia();
}

/* --- ripristino di fabbrica ---------------------------------------------
 *
 * Due passaggi, e nessuno dei due e cerimonia. Il primo e la conferma, che
 * per un'azione senza ritorno e il minimo. Il secondo e che la conferma
 * dica **cosa** si perde: "cancella tutto" non aiuta nessuno a decidere,
 * mentre "la password del Wi-Fi e il token" fa capire in un attimo che dopo
 * bisogna tornare davanti al pannello con le credenziali in mano.
 *
 * Se il ripristino fallisce non si riavvia. Un pannello che riparte
 * credendo di aver dimenticato qualcosa che invece ha ancora e' peggio di
 * un pannello che dice di non esserci riuscito. */
static void ripristina_davvero(void)
{
    if (!ripristino_esegui()) {
        registro_aggiungi(LOG_ERRORE, "sistema",
                          "ripristino non riuscito: la configurazione o i "
                          "segreti sono ancora li");
        return;
    }
    sistema_riavvia();
}

static void su_ripristina(lv_event_t *e)
{
    LV_UNUSED(e);
    conferma_azione(
        tr(TX_SETTINGS_RESET_QUESTION), tr(TX_SETTINGS_RESET_EXPLAIN),
        tr(TX_SETTINGS_RESET_BUTTON), true, ripristina_davvero);
}

/* --- i cinque pannelli -------------------------------------------------- */

static void pannello_rete(lv_obj_t *c)
{
    const rete_info_t r = sistema_rete();

    /* Il nome della rete viene dalla configurazione, lo stato dalla radio:
       sono due cose diverse e possono contraddirsi — una rete configurata a
       cui non ci si riesce ad attaccare e proprio il caso in cui uno guarda
       questa schermata. */
    const char *ssid = cfg_testo("system/network/ssid", "");
    riga_valore(c, tr(TX_SETTINGS_WIFI_NETWORK), r.descrizione,
                ssid[0] ? ssid : tr(TX_SETTINGS_NO_NETWORK),
                r.connessa ? C_TXT : C_DIM);

    riga_valore(c, tr(TX_SETTINGS_IP_ADDRESS), tr(TX_SETTINGS_FROM_DHCP),
                r.indirizzo[0] ? r.indirizzo : tr(TX_SETTINGS_NONE),
                r.indirizzo[0] ? C_TXT : C_DIM);

    if (r.connessa) {
        static char segnale[24];
        lv_snprintf(segnale, sizeof segnale, "%d dBm", r.potenza_dbm);
        /* Sotto i -70 la connessione regge ma gli aggiornamenti arrivano a
           singhiozzo: e il numero che spiega un pannello lento quando tutto
           il resto va. */
        riga_valore(c, tr(TX_SETTINGS_SIGNAL), r.potenza_dbm > -70 ? tr(TX_SETTINGS_SIGNAL_GOOD)
                                                        : tr(TX_SETTINGS_SIGNAL_WEAK),
                    segnale, r.potenza_dbm > -70 ? C_TXT : C_WARN);
    }
}

static void pannello_ha(lv_obj_t *c)
{
    /* Lo stato vero, non quello dei contatori: quelli sono ancora d'esempio,
       e "connesso" scritto li somigliava abbastanza alla realta da non farsi
       notare. Un segnaposto che indovina e peggio di uno che sbaglia. */
    const ha_stato_t s = ha_stato();
    const bool bene = s == HA_PRONTO;
    riga_valore(c, tr(TX_SETTINGS_STATE), ha_motivo(), ui_ha_stato(s),
                bene ? C_OK : C_WARN);

    static char dove[96];
    lv_snprintf(dove, sizeof dove, "%s:%d",
                cfg_testo("home_assistant/host", tr(TX_SETTINGS_NO_ADDRESS)),
                (int)cfg_intero("home_assistant/port", 8123));
    riga_valore(c, tr(TX_SETTINGS_ADDRESS),
                cfg_vero("home_assistant/tls", false)
                    ? tr(TX_SETTINGS_TLS)
                    : tr(TX_SETTINGS_PLAIN),
                dove, C_TXT);

    static char da[24];
    const uint32_t eta = ha_eta_ultimo_dato();
    if (eta == UINT32_MAX)
        lv_snprintf(da, sizeof da, "%s", tr(TX_TIME_NEVER));
    else
        lv_snprintf(da, sizeof da, tr(TX_TIME_SECONDS_AGO), (unsigned)(eta / 1000));
    riga_valore(c, tr(TX_SETTINGS_LAST_DATA), NULL, da, C_TXT);

    riga_azione(c, tr(TX_SETTINGS_RECONNECT_NOW), tr(TX_SETTINGS_RECONNECT_EXPLAIN),
                ICO_SYNC, tr(TX_SETTINGS_RECONNECT), su_riconnetti);
}

/* --- information -------------------------------------------------------------
 *
 * The panel's "who am I": the logo, which version it is, and why it has
 * that name. Numbers for whoever must tell someone else what runs on that
 * wall — the revision finds the exact code — and that otherwise live only
 * in the diagnostics. */
static void pannello_info(lv_obj_t *c)
{
    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    /* As tall as a line of the climate's big number: a size of the
       profile, and on the glass the right presence for a logo. */
    logo_foyer(k, lv_font_get_line_height(font(FT_CLIMA)), true, C_TXT);
    ui_testo(k, tr(TX_SETTINGS_TAGLINE), C_DIM, FT_M);

    riga_valore(c, tr(TX_SETTINGS_VERSION), NULL, PANNELLO_VERSIONE, C_TXT);
    riga_valore(c, tr(TX_SETTINGS_REVISION), tr(TX_SETTINGS_REVISION_HINT),
                PANNELLO_REVISIONE, C_TXT);
    riga_valore(c, tr(TX_SETTINGS_BUILT), NULL, PANNELLO_COMPILATO, C_TXT);
    riga_valore(c, tr(TX_SETTINGS_PROFILE), NULL, PRF->chiave, C_TXT);

    lv_obj_t *nota = ui_testo(c, tr(TX_SETTINGS_NAME_STORY), C_DIM, FT_S);
    lv_label_set_long_mode(nota, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(nota, LV_PCT(100));
}

static void pannello_schermo(lv_obj_t *c)
{
    /* I valori **in vigore**, non i predefiniti. Questa schermata mostrava
       le costanti di compilazione: a chi aveva scritto trenta secondi nella
       pagina web diceva comunque centoventi, e il posto dove si va a
       controllare un'impostazione e' l'ultimo che dovrebbe mentire. */
    static char la[16], ls[16];
    lv_snprintf(la, sizeof la, "%d%%", (int)cfg_intero("display/brightness_active", 85));
    lv_snprintf(ls, sizeof ls, "%d%%", (int)cfg_intero("display/brightness_standby", 12));
    riga_valore(c, tr(TX_SETTINGS_BRIGHTNESS_ON), NULL, la, C_TXT);
    riga_valore(c, tr(TX_SETTINGS_BRIGHTNESS_STANDBY), NULL, ls, C_TXT);

    static char s[64];
    const uint32_t spegn = tempi_spegnimento_ms();
    if (spegn)
        lv_snprintf(s, sizeof s, tr(TX_SETTINGS_STANDBY_OFF_AFTER),
                    (unsigned)(tempi_standby_ms() / 1000),
                    (unsigned)(spegn / 1000));
    else
        lv_snprintf(s, sizeof s, tr(TX_SETTINGS_STANDBY_NEVER_OFF),
                    (unsigned)(tempi_standby_ms() / 1000));
    riga_valore(c, tr(TX_SETTINGS_STANDBY), NULL, s, C_TXT);

    /* La regola notturna si dice solo se e accesa, e si dice **anche**
       quando e in vigore adesso: uno schermo che si spegne da solo alle
       undici di sera deve poter rispondere alla domanda "perche'". */
    if (cfg_vero("display/night_off/enabled", false)) {
        static char n[64];
        lv_snprintf(n, sizeof n, "%s-%s%s",
                    cfg_testo("display/night_off/from", "23:00"),
                    cfg_testo("display/night_off/to", "07:00"),
                    tempi_e_notte() ? tr(TX_SETTINGS_NOW_SUFFIX) : "");
        riga_valore(c, tr(TX_SETTINGS_NIGHT_OFF), NULL, n,
                    tempi_e_notte() ? C_ACC : C_TXT);
    }
}

/* --- costruzione -------------------------------------------------------- */

static void menu(lv_obj_t *padre, int scelta)
{
    lv_obj_t *m = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(m, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m, PRF->geo.gap, 0);
    if (PRF->orientamento == VERTICALE) {
        /* In verticale il menu diventa una riga di pastiglie sopra il
           pannello: un elenco a due colonne su 600 px non ci sta. */
        lv_obj_set_width(m, LV_PCT(100));
        lv_obj_set_height(m, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(m, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(m, PRF->geo.gap, 0);
    } else {
        lv_obj_set_size(m, PRF->clima.dettaglio_col_w, LV_PCT(100));
    }

    for (int n = 0; n < V_QUANTE; n++) {
        const bool sel = n == scelta;
        lv_obj_t *v = ui_pannello(m, sel ? C_SEL_BG : C_CARD2);
        lv_obj_set_style_radius(v, PRF->geo.radius_tile, 0);
        ui_bordo(v, LV_BORDER_SIDE_FULL, sel ? C_SEL_LINE : C_LINE);
        lv_obj_set_flex_flow(v, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(v, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(v, PRF->geo.pad, 0);
        lv_obj_set_style_pad_column(v, PRF->geo.gap, 0);
        lv_obj_add_flag(v, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(v, su_voce, LV_EVENT_CLICKED, (void *)(intptr_t)n);

        if (PRF->orientamento == VERTICALE) {
            lv_obj_set_flex_grow(v, 1);
            lv_obj_set_height(v, PRF->tocco.apertura.h);
            lv_obj_set_style_min_width(v, 0, 0);
            lv_obj_set_flex_align(v, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            ui_icona(v, VOCI[n].icona, sel ? C_ACC : C_DIM, IC_S);
        } else {
            lv_obj_set_width(v, LV_PCT(100));
            lv_obj_set_height(v, PRF->tocco.apertura.h);
            ui_icona(v, VOCI[n].icona, sel ? C_ACC : C_DIM, IC_S);
            lv_obj_t *l = ui_testo(v, tr(VOCI[n].nome), sel ? C_ACC : C_TXT, FT_M);
            lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
            lv_obj_set_flex_grow(l, 1);
        }
    }
}

void schermata_impostazioni(lv_obj_t *c)
{
    int scelta = ui_vista();
    if (scelta < 0 || scelta >= V_QUANTE) scelta = V_RETE;

    /* La voce Sistema e la diagnostica: schermata sua, non un pannello. */
    if (scelta == V_SISTEMA) { schermata_diagnostica(c); return; }

    ui_testata(tr(TX_COMMON_SETTINGS), tr(VOCI[scelta].nome));

    lv_obj_t *fuori = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(fuori, LV_OPA_TRANSP, 0);
    lv_obj_set_size(fuori, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(fuori, PRF->orientamento == VERTICALE
                                ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(fuori, PRF->geo.gap, 0);
    lv_obj_set_style_pad_row(fuori, PRF->geo.gap, 0);

    menu(fuori, scelta);

    lv_obj_t *destra = ui_pannello(fuori, C_BG);
    lv_obj_set_style_bg_opa(destra, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(destra, 1);
    lv_obj_set_flex_flow(destra, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(destra, PRF->geo.gap, 0);
    if (PRF->orientamento == VERTICALE) lv_obj_set_width(destra, LV_PCT(100));
    else                                lv_obj_set_height(destra, LV_PCT(100));
    lv_obj_add_flag(destra, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(destra, LV_DIR_VER);

    switch (scelta) {
    case V_HA:         pannello_ha(destra);         break;
    case V_SCHERMO:    pannello_schermo(destra);    break;
    case V_INFO:       pannello_info(destra);       break;
    default:           pannello_rete(destra);       break;
    }

    /* Sempre in fondo, su ogni pannello: lo sblocco della pagina web e il
       riavvio. Sono le due cose che si cercano quando qualcosa non va. */
    /* Lo sblocco fisico: bisogna essere in casa, davanti al muro. Finche
       questo interruttore e spento la pagina web risponde 404 a tutto,
       tranne ai due percorsi in sola lettura. */
    static char quanto[80];
    if (web_sempre_aperta()) {
        /* Detto qui perche e qui che si guarda: una porta tenuta aperta
           dalla configurazione non ha un conto alla rovescia, e mostrarne
           uno a zero sarebbe peggio che non mostrare niente. */
        lv_snprintf(quanto, sizeof quanto, "%s", tr(TX_SETTINGS_ALWAYS_OPEN));
    } else if (web_sbloccato()) {
        const uint32_t s = web_sblocco_resta_ms() / 1000;
        lv_snprintf(quanto, sizeof quanto,
                    tr(TX_SETTINGS_OPEN_CLOSES_IN), s / 60, s % 60);
    } else {
        lv_snprintf(quanto, sizeof quanto, "%s", tr(TX_SETTINGS_OPENS_FOR_15));
    }
    lv_obj_t *sblocco = riga(destra, tr(TX_SETTINGS_ALLOW_CONFIG), quanto);
    lv_obj_t *sw = interruttore(sblocco, web_sbloccato(), true);
    lv_obj_add_event_cb(sw, su_sblocco, LV_EVENT_CLICKED, NULL);

    riga_azione(destra, tr(TX_SETTINGS_RESTART_PANEL), tr(TX_SETTINGS_RESTART_EXPLAIN),
                ICO_RESTART_ALT, tr(TX_SETTINGS_RESTART), su_riavvia);

    /* Sotto il riavvio e non accanto: si somigliano nel gesto — si preme un
       pulsante e il pannello si spegne un attimo — e si somigliano nel
       nome. Quello che li separa e scritto nella riga sotto ognuno, ed e
       l'unica cosa che conta: uno tiene tutto, l'altro non tiene niente. */
    riga_azione(destra, tr(TX_SETTINGS_FACTORY_RESET), tr(TX_SETTINGS_FACTORY_RESET_EXPLAIN),
                ICO_DELETE, tr(TX_SETTINGS_RESET_BUTTON), su_ripristina);

    /* L'indirizzo e quello di **questo** pannello, non quello predefinito:
       qui c'era scritto `pannello.local` a prescindere, e chi aveva dato un
       nome al proprio leggeva un indirizzo che non risponde. */
    char host[32];
    static char dove[96];
    lv_snprintf(dove, sizeof dove,
                tr(TX_SETTINGS_REST_OF_CONFIG), cfg_nome_host(host, sizeof host));
    lv_obj_t *nota = ui_testo(destra, dove, C_DIM, FT_S);
    lv_label_set_long_mode(nota, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(nota, LV_PCT(100));
}
