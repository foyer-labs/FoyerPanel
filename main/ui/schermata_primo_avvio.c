/* ------------------------------------------------------------------------
 * Primo avvio — 01-specifica-ui.md §4.9.
 *
 * Quattro passi con i pallini:
 *   1  le reti che la radio vede davvero, piu forte per prima
 *   2  la password di quella scelta
 *   3  indirizzo e token di Home Assistant
 *   4  la prova, con i quattro controlli che si accendono da soli
 *
 * Ci si arriva quando la configurazione non c'e o non si legge: alla prima
 * accensione e dopo un ripristino di fabbrica. Non e una sezione — non ha
 * rail, non ha barra, non ha HOME — perche finche non e finito non c'e
 * nessuna home a cui tornare.
 *
 * --- cos'era, e perche non bastava ---------------------------------------
 *
 * Fino a poco fa questa schermata era un disegno: l'elenco delle reti era
 * scritto qui dentro, i quattro controlli del passo finale erano booleani
 * fissi, la password del Wi-Fi non si poteva battere **da nessuna parte**, e
 * quello che si scriveva non veniva salvato — si passava al passo dopo e
 * bastava.
 *
 * Serviva a guardare il disegno, e per quello andava bene. Ma e la prima
 * cosa che incontra un pannello appena acceso, e un pannello appena acceso
 * non ha nessun'altra strada: NVS e vuota, LittleFS e vuoto, e la pagina web
 * sta dietro a una rete che non c'e ancora. Chi lo installa avrebbe battuto
 * indirizzo e token per poi scoprire che non erano stati scritti.
 *
 * Adesso ogni passo scrive dove va scritto: l'SSID in config.json, la
 * password e il token in NVS attraverso segreti_scrivi(), l'indirizzo in
 * config.json. Il token non torna indietro nemmeno qui — si vede come
 * asterischi mentre lo si batte, e da quel momento in poi non lo mostra piu
 * nessuno.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "ha.h"
#include "sezioni.h"
#include "sistema.h"
#include "segreti.h"
#include "config.h"
#include "dati.h"
#include "schermate.h"
#include "stati.h"
#include "ui.h"
#include "widgets/tastiera.h"

#define PASSI 4
enum { P_RETE = 0, P_CHIAVE, P_HA, P_PROVA };

static lv_obj_t *radice;
static int   passo;
static int   campo;              /* nel passo 3: 0 indirizzo, 1 token */
static char  rete[40];
static char  chiave[SEGRETO_MAX];
static char  indirizzo[40];
static char  token[SEGRETO_MAX];
static int   rete_scelta = -1;
static bool  rete_protetta = true;

/* Only the network steps — list, password, test — opened from the "the
   Wi-Fi does not connect" screen. Home Assistant has nothing to do with it:
   the configuration is there, only the road is missing. */
static bool  solo_rete;

/* The test step redraws itself once a second: the four checks must light
   up while one looks at them. They were built once and stayed as at the
   first instant — the network "in progress" forever, even once connected. */
static lv_timer_t *rinfresco_prova;

static void costruisci(void);

/* --- le reti ------------------------------------------------------------
 *
 * Vengono dalla radio, non da qui. La scansione non e istantanea — mentre
 * gira i canali la radio non e connessa — quindi si chiede una volta e si
 * torna a guardare con un tempo: bloccare qui vorrebbe dire un'interfaccia
 * ferma per due secondi buoni sulla prima schermata che il pannello mostra
 * in vita sua. */
static lv_timer_t *attesa_scansione;

/* Vero da quando un giro di ricerca e partito davvero. Finche e falso il
   tempo qui sotto continua a chiederlo, invece di stare a guardare una
   radio che non ha ancora cominciato. */
static bool scansione_partita;

static void guarda_scansione(lv_timer_t *tm)
{
    LV_UNUSED(tm);

    /* --- prima si prova a farla partire ------------------------------
     *
     * Questa schermata si costruisce **prima** che la radio esista: sul
     * pannello l'interfaccia e in piedi al terzo secondo e il Wi-Fi si
     * accende al quinto, perche' il vetro non deve aspettare la rete per
     * mostrarsi. Chiedere una ricerca a quel punto fallisce, ed e giusto
     * che fallisca.
     *
     * Quello che non andava e cosa succedeva dopo: niente. Si chiedeva una
     * volta sola, e se quella volta era troppo presto l'elenco restava
     * vuoto **per sempre** — sulla schermata il cui unico scopo e far
     * scegliere una rete. Adesso si richiede finche non parte. */
    if (!scansione_partita) {
        scansione_partita = sistema_reti_cerca();
        return;
    }

    if (!sistema_reti_pronte()) return;
    lv_timer_delete(attesa_scansione);
    attesa_scansione = NULL;
    scansione_partita = false;
    costruisci();
}

static void cerca_reti(void)
{
    scansione_partita = sistema_reti_cerca();
    if (attesa_scansione) return;
    attesa_scansione = lv_timer_create(guarda_scansione, 300, NULL);
}

static void su_rete(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    const rete_trovata_t r = sistema_rete_trovata(n);
    rete_scelta = n;
    rete_protetta = r.protetta;
    lv_snprintf(rete, sizeof rete, "%s", r.ssid);
    /* Cambiando rete la password di prima non vale piu, e lasciarla
       nel campo farebbe partire un tentativo con la chiave sbagliata. */
    lv_memzero(chiave, sizeof chiave);
    costruisci();
}

/* --- navigazione fra i passi -------------------------------------------- */

/* --- quello che si scrive, si scrive ------------------------------------
 *
 * Ogni passo salva la sua parte **appena e completa**, invece di tenere
 * tutto in memoria fino alla fine. La differenza si vede il giorno che
 * qualcuno stacca la corrente a meta configurazione: cosi si riprende da
 * dove si era, con l'ultimo passo gia fatto, invece di ricominciare.
 *
 * Ogni cosa nel posto suo: SSID e indirizzo in config.json, che si
 * esporta e si legge; password e token in NVS attraverso
 * segreti_scrivi(), che non tornano indietro da nessuna strada. */
static bool salva_rete(void)
{
    if (!cfg_imposta_testo("system/network/ssid", rete)) return false;
    if (!cfg_salva()) return false;
    /* Una rete aperta non ha password, e scriverne una vuota non e la
       stessa cosa di non scriverne: segreti_scrivi() con valore vuoto
       **cancella**, ed e esattamente cio che serve qui. */
    if (!segreti_scrivi(SEG_WIFI_PASSWORD, rete_protetta ? chiave : ""))
        return false;

    /* And now the radio uses them. This was missing: name and password were
       written and nobody told the radio, and the panel connected only at
       the next restart — while the test step waited for a network that
       could not arrive. */
    sistema_rete_applica();
    return true;
}

static bool salva_ha(void)
{
    /* Un campo lasciato vuoto **non cancella** quello che c'era. E la
       differenza fra "lo faccio dopo" e "toglilo": segreti_scrivi() con
       valore vuoto cancella, e qui sarebbe la cosa sbagliata — chi rimanda
       il token non sta chiedendo di buttare quello di prima.

       Sul primo avvio vero non c'e niente da conservare e i due
       comportamenti coincidono. Si vedono diversi dopo un ripristino a
       meta, o rientrando in questa schermata da un pannello gia in parte
       configurato. */
    if (indirizzo[0]) {
        if (!cfg_imposta_testo("home_assistant/host", indirizzo)) return false;
        if (!cfg_salva()) return false;
    }
    if (token[0]) return segreti_scrivi(SEG_HA_TOKEN, token);
    return true;
}

/* Il passo appena finito ha qualcosa da salvare? Se il salvataggio non
   riesce non si va avanti: proseguire vorrebbe dire arrivare in fondo e
   trovarsi un pannello che non sa niente di quello che gli e stato
   detto. */
static bool salva_il_passo(void)
{
    switch (passo) {
    case P_CHIAVE: return salva_rete();
    case P_HA:     return salva_ha();
    default:       return true;
    }
}

static bool salvataggio_fallito;

static void su_avanti(lv_event_t *e)
{
    LV_UNUSED(e);

    if (!salva_il_passo()) {
        salvataggio_fallito = true;
        costruisci();
        return;
    }
    salvataggio_fallito = false;

    /* The network only: from the password straight to the test, and the
       test closes and leaves the panel where it was. */
    if (solo_rete && passo == P_CHIAVE) passo = P_PROVA;
    else if (passo < PASSI - 1) passo++;
    else {
        /* Finito: si esce dal primo avvio e comincia il pannello. Da qui in
           poi la configurazione c'e, quindi il prossimo avvio non ripassera
           di qua. */
        const bool era_solo_rete = solo_rete;
        stato_primo_avvio(false, 0);
        if (!era_solo_rete) ui_vai(SEZ_HOME);
        return;
    }

    if (passo == P_RETE) cerca_reti();
    costruisci();
}

static void su_indietro(lv_event_t *e)
{
    LV_UNUSED(e);
    if (solo_rete) {
        /* From the first step one leaves: before it is the panel. */
        if (passo == P_RETE) { stato_primo_avvio(false, 0); return; }
        passo = passo == P_PROVA ? P_CHIAVE : P_RETE;
        if (passo == P_RETE) cerca_reti();
        costruisci();
        return;
    }
    if (passo > 0) passo--;
    costruisci();
}

static void su_campo(lv_event_t *e)
{
    campo = (int)(intptr_t)lv_event_get_user_data(e);
    costruisci();
}

/* --- tastiera ----------------------------------------------------------- */

/* Dove finisce quello che si batte. Nel passo della chiave c'e un campo
   solo; in quello di Home Assistant sono due e si sceglie toccandoli. */
static char *campo_corrente(size_t *max)
{
    if (passo == P_CHIAVE) { *max = sizeof chiave; return chiave; }
    if (campo == 0)        { *max = sizeof indirizzo; return indirizzo; }
    *max = sizeof token;
    return token;
}

static void su_tasto(tasto_t tipo, const char *testo)
{
    size_t max = 0;
    char *b = campo_corrente(&max);

    switch (tipo) {
    case TASTO_CARATTERE: {
        const size_t l = lv_strlen(b);
        const size_t n = lv_strlen(testo);
        if (l + n < max - 1) {
            for (size_t k = 0; k < n; k++) b[l + k] = testo[k];
            b[l + n] = 0;
        }
        break;
    }
    case TASTO_CANCELLA: {
        size_t l = lv_strlen(b);
        /* Un carattere, non un byte: cancellare mezzo euro lascerebbe una
           stringa che non si puo disegnare. */
        while (l > 0 && ((unsigned char)b[l - 1] & 0xC0) == 0x80) l--;
        if (l > 0) l--;
        b[l] = 0;
        break;
    }
    case TASTO_AZIONE:
        /* Il tasto ambra chiude il campo, non il passo: sul passo di Home
           Assistant porta dall'indirizzo al token, e solo dal token porta
           avanti. Prima portava dritto all'ultimo passo saltando tutto
           quello che c'era in mezzo, compreso il salvataggio. */
        if (passo == P_HA && campo == 0) { campo = 1; break; }
        su_avanti(NULL);
        return;
    }
    costruisci();
}

/* --- pezzi comuni ------------------------------------------------------- */

static void pallini(lv_obj_t *padre)
{
    lv_obj_t *f = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_size(f, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, COM.gap_pallini, 0);

    for (int n = 0; n < PASSI; n++) {
        lv_obj_t *p = ui_pannello(f, n == passo ? C_ACC : C_OFF);
        lv_obj_set_size(p, n == passo ? COM.pallino_paginatore_attivo
                                      : COM.pallino_paginatore,
                        COM.pallino_paginatore);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    }
}

static void testata(lv_obj_t *padre, const char *titolo, const char *sotto)
{
    lv_obj_t *t = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_set_size(t, LV_PCT(100), PRF->geo.head_h);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(t, PRF->geo.gap, 0);

    if (passo > 0 || solo_rete) {
        lv_obj_t *b = ui_pannello(t, C_CARD2);
        lv_obj_set_size(b, PRF->tocco.pager.w, PRF->tocco.pager.h);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_indietro, LV_EVENT_CLICKED, NULL);
        ui_icona(b, ICO_ARROW_BACK, C_TXT, IC_S);
    }

    ui_testo(t, titolo, C_TXT, FT_L);
    ui_testo(t, solo_rete ? tr(TX_SETUP_CHANGE_NETWORK) : sotto, C_DIM, FT_S);
    ui_spazio(t);
    /* The dots count four steps, and changing only the network is not four
       steps. */
    if (!solo_rete) pallini(t);
}

static lv_obj_t *pulsante_avanti(lv_obj_t *padre, const char *testo, bool vivo)
{
    lv_obj_t *b = ui_pannello(padre, vivo ? C_ACC : C_CARD2);
    lv_obj_set_size(b, LV_SIZE_CONTENT, PRF->tocco.apertura.h);
    lv_obj_set_style_pad_hor(b, PRF->geo.pad, 0);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, vivo ? C_ACC : C_LINE);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, COM.pastiglia_gap, 0);
    ui_testo(b, testo, vivo ? C_INK : C_OFF, FT_M);
    ui_icona(b, ICO_ARROW_FORWARD, vivo ? C_INK : C_OFF, IC_S);
    if (vivo) {
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_avanti, LV_EVENT_CLICKED, NULL);
    }
    return b;
}

/* --- passo 1: la rete --------------------------------------------------- */

static void passo_rete(lv_obj_t *c)
{
    testata(c, tr(TX_SETUP_TITLE), tr(TX_SETUP_STEP1));

    lv_obj_t *fuori = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(fuori, LV_OPA_TRANSP, 0);
    lv_obj_set_width(fuori, LV_PCT(100));
    lv_obj_set_flex_grow(fuori, 1);
    lv_obj_set_flex_flow(fuori, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(fuori, PRF->geo.gap, 0);

    /* Elenco delle reti, con l'altezza di riga che dice il profilo. */
    lv_obj_t *elenco = ui_scheda(fuori);
    lv_obj_set_width(elenco, LV_PCT(100));
    lv_obj_set_flex_grow(elenco, 1);
    lv_obj_set_style_pad_row(elenco, COM.gap_stretto, 0);
    lv_obj_add_flag(elenco, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(elenco, LV_DIR_VER);

    const int quante = sistema_reti_quante();
    ui_occhiello(elenco, quante ? tr(TX_SETUP_NETWORKS_FOUND) : tr(TX_SETUP_SEARCHING));

    if (!quante) {
        /* La radio sta girando i canali. Dirlo e meglio di un riquadro
           vuoto: un elenco vuoto e indistinguibile da un elenco che non
           arrivera mai, e chi installa non ha modo di sapere quale dei due
           sta guardando. */
        lv_obj_t *a = ui_testo(elenco,
            tr(TX_SETUP_SEARCHING_EXPLAIN), C_DIM, FT_S);
        lv_obj_set_width(a, LV_PCT(100));
        lv_label_set_long_mode(a, LV_LABEL_LONG_WRAP);
    }

    for (int n = 0; n < quante; n++) {
        const rete_trovata_t rt = sistema_rete_trovata(n);
        const bool sel = n == rete_scelta;
        lv_obj_t *r = ui_pannello(elenco, sel ? C_SEL_BG : C_CARD);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_height(r, PRF->tocco.riga_rete_h);
        lv_obj_set_style_radius(r, PRF->geo.radius_btn, 0);
        if (sel) ui_bordo(r, LV_BORDER_SIDE_FULL, C_SEL_LINE);
        lv_obj_set_style_pad_hor(r, PRF->geo.gap, 0);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(r, su_rete, LV_EVENT_CLICKED, (void *)(intptr_t)n);

        /* La qualita si dice con l'icona, non con i dBm: -67 non vuol dire
           niente a nessuno. */
        ui_icona(r, rt.dbm > -60 ? ICO_SIGNAL_WIFI_4_BAR : ICO_WIFI,
                 sel ? C_ACC : C_DIM, IC_M);

        lv_obj_t *nome = ui_testo(r, rt.ssid, sel ? C_ACC : C_TXT, FT_M);
        lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(nome, 1);

        if (rt.protetta) ui_icona(r, ICO_LOCK, C_DIM, IC_S);
        else             ui_testo(r, tr(TX_SETUP_OPEN), C_WARN, FT_S);

        /* Dopo i figli: la riga intera deve prendere il tocco, non solo il
           nome. E l'errore che questo progetto ha gia fatto tre volte. */
        ui_tocco_su_tutto(r);
    }

    /* --- qui c'era un codice QR ------------------------------------------
     *
     * Rimandava all'indirizzo predefinito dell'access point dell'ESP32,
     * con scritto "prosegui dal telefono", e
     * quell'indirizzo **non esiste**: il punto di accesso temporaneo che
     * avrebbe dovuto aprirlo non e mai stato scritto — nel progetto non c'e
     * una riga che accenda la modalita AP. Chi lo inquadrava trovava il
     * telefono che non si collegava a niente, e non aveva modo di capire se
     * il guasto fosse suo o del pannello.
     *
     * **E non si fara.** Serviva a rompere un cerchio: il pannello senza rete
     * non ha una pagina raggiungibile, quindi si configurava dal telefono
     * attraverso una rete che il pannello stesso apriva. Quel cerchio ora si
     * rompe prima e senza radio in piu: la password del Wi-Fi si batte al
     * passo 2 — e corta, otto o venti caratteri — e da quel momento
     * http://pannello.local risponde. La cosa lunga davvero, il token di
     * Home Assistant, si incolla di la; al passo 3 c'e "Lo faccio dopo"
     * apposta.
     *
     * Il conto: SoftAP, un secondo server, una pagina ridotta e il passaggio
     * fra le due reti, per un caso che resta — una rete nascosta, che la
     * scansione non mostra — e che si risolve dalla console. Non vale.
     *
     * L'elenco intanto si prende tutta la larghezza: due reti in piu
     * visibili senza scorrere.
     */
    /* Il piede con il pulsante avanti. */
    lv_obj_t *piede = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(piede, LV_OPA_TRANSP, 0);
    lv_obj_set_width(piede, LV_PCT(100));
    lv_obj_set_height(piede, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(piede, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(piede, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    pulsante_avanti(piede, tr(TX_COMMON_NEXT), rete_scelta >= 0);
}

/* --- passo 2: la password della rete ------------------------------------
 *
 * Mancava del tutto. Si sceglieva la rete e si passava a Home Assistant,
 * come se una rete protetta si aprisse da sola: il pannello non poteva
 * collegarsi a niente, e chi installava non aveva nessun posto dove
 * scrivere la chiave se non la console seriale.
 *
 * La password si vede mentre si batte, e non e una svista. Su una tastiera
 * a schermo, con le dita, senza poter rileggere quello che si e scritto, una
 * password lunga si sbaglia quasi sempre — e l'errore che ne segue e "non si
 * connette", che non dice mai che il problema e una lettera. Chi sta davanti
 * al pannello e in casa propria; il rischio che qualcuno legga da sopra la
 * spalla e minore del rischio di sbagliare e non capire perche.
 *
 * In NVS ci va lo stesso: si vede qui, per il tempo di scriverla, e da li in
 * poi non la mostra piu nessuno. */
static void passo_chiave(lv_obj_t *c)
{
    static char sotto[80];
    lv_snprintf(sotto, sizeof sotto, tr(TX_SETUP_STEP2),
                rete[0] ? rete : tr(TX_SETUP_THIS_NETWORK));
    testata(c, tr(TX_SETUP_WIFI_PASSWORD), sotto);

    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    ui_bordo(k, LV_BORDER_SIDE_FULL, C_ACC);
    lv_obj_set_style_pad_row(k, COM.gap_stretto, 0);

    ui_occhiello(k, rete_protetta ? tr(TX_WIFI_PASSWORD) : tr(TX_SETUP_OPEN_NETWORK));

    const char *mostrato = rete_protetta
        ? (chiave[0] ? chiave : "-")
        : tr(TX_SETUP_NO_PASSWORD);
    lv_obj_t *l = ui_testo(k, mostrato,
                           chiave[0] || !rete_protetta ? C_TXT : C_DIM,
                           rete_protetta ? FT_MONO : FT_M);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));

    if (salvataggio_fallito) {
        lv_obj_t *g = ui_testo(c,
            tr(TX_SETUP_SAVE_FAILED),
            C_RICON_TXT, FT_S);
        lv_obj_set_width(g, LV_PCT(100));
        lv_label_set_long_mode(g, LV_LABEL_LONG_WRAP);
    }

    ui_spazio(c);

    /* Su una rete aperta non c'e niente da battere: si va avanti e basta. */
    if (!rete_protetta) {
        lv_obj_t *piede = ui_pannello(c, C_BG);
        lv_obj_set_style_bg_opa(piede, LV_OPA_TRANSP, 0);
        lv_obj_set_width(piede, LV_PCT(100));
        lv_obj_set_height(piede, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(piede, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(piede, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        pulsante_avanti(piede, tr(TX_COMMON_NEXT), true);
        return;
    }

    tastiera(c, tr(TX_SETUP_CONNECT), su_tasto);
}

/* --- passo 3: Home Assistant -------------------------------------------- */

static void riga_campo(lv_obj_t *padre, int quale, const char *nome,
                       const char *valore, bool segreto)
{
    const bool sel = quale == campo;

    lv_obj_t *k = ui_pannello(padre, C_CARD);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(k, PRF->geo.radius, 0);
    ui_bordo(k, LV_BORDER_SIDE_FULL, sel ? C_ACC : C_LINE);
    lv_obj_set_style_pad_all(k, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(k, COM.gap_stretto, 0);
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(k, su_campo, LV_EVENT_CLICKED, (void *)(intptr_t)quale);

    ui_occhiello(k, nome);

    /* Il token si scrive ma non si rilegge: e un segreto, e mostrarlo in
       chiaro su un pannello a muro non ha nessun vantaggio.

       Il segnaposto e una parola e non un trattino lungo, e non e una
       questione di gusto: il campo del token si disegna col font
       monospaziato, che tools/genera_font.py genera sul solo intervallo
       0x20-0x7E. Un carattere fuori da li non e un carattere brutto, e un
       **rettangolo vuoto** — che e quello che si vedeva. Regola generale:
       in FT_MONO ci va solo ASCII. */
    const char *mostrato = valore[0] ? valore : tr(TX_SETUP_TO_ENTER);
    static char nascosto[8][40];
    static uint8_t giro;
    if (segreto && valore[0]) {
        char *b = nascosto[giro++ % 8];
        size_t n = lv_strlen(valore);
        if (n > 12) n = 12;
        for (size_t i = 0; i < n; i++) b[i] = '*';
        b[n] = 0;
        mostrato = b;
    }

    /* A spaziatura fissa solo quando c'e un valore: il segnaposto e prosa,
       e la prosa in monospaziato si legge peggio e non guadagna niente. */
    lv_obj_t *l = ui_testo(k, mostrato, valore[0] ? C_TXT : C_DIM,
                           segreto && valore[0] ? FT_MONO : FT_M);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
}

static void passo_ha(lv_obj_t *c)
{
    testata(c, tr(TX_SETTINGS_HA), tr(TX_SETUP_STEP3));

    lv_obj_t *campi = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(campi, LV_OPA_TRANSP, 0);
    lv_obj_set_width(campi, LV_PCT(100));
    lv_obj_set_height(campi, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(campi, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(campi, PRF->geo.gap, 0);

    lv_obj_t *a = ui_pannello(campi, C_BG);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(a, 1);
    lv_obj_set_height(a, LV_SIZE_CONTENT);
    riga_campo(a, 0, tr(TX_SETUP_ADDRESS), indirizzo, false);

    lv_obj_t *b = ui_pannello(campi, C_BG);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, LV_SIZE_CONTENT);
    riga_campo(b, 1, tr(TX_SETUP_TOKEN), token, true);

    /* --- lo faccio dopo -------------------------------------------------
     *
     * Il token di Home Assistant e un JWT: circa duecento caratteri di
     * base64 casuale, maiuscole minuscole cifre punti e trattini. Batterlo
     * su una tastiera a schermo, con le dita, senza poterlo rileggere, e
     * mezz'ora e tre tentativi — e il tentativo sbagliato non si vede: si
     * vede "token rifiutato" due schermate dopo.
     *
     * Ma a questo punto la rete c'e gia: e stata configurata due passi fa.
     * Quindi `http://pannello.local` risponde, e da li il token si
     * **incolla**. Rimandare non e una scorciatoia, e la strada giusta.
     *
     * E la ragione per cui il punto di accesso temporaneo non serve piu:
     * esisteva per far arrivare il telefono al pannello prima che la rete
     * ci fosse, e adesso la rete c'e prima. */
    lv_obj_t *dopo = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(dopo, LV_OPA_TRANSP, 0);
    lv_obj_set_width(dopo, LV_PCT(100));
    lv_obj_set_height(dopo, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dopo, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dopo, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dopo, PRF->geo.gap, 0);

    char host[32];
    static char consiglio[128];
    lv_snprintf(consiglio, sizeof consiglio,
                tr(TX_SETUP_TOKEN_PASTE_TIP), cfg_nome_host(host, sizeof host));
    lv_obj_t *nota = ui_testo(dopo, consiglio, C_DIM, FT_S);
    lv_label_set_long_mode(nota, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(nota, 1);

    lv_obj_t *salta = ui_pannello(dopo, C_CARD2);
    lv_obj_set_size(salta, LV_SIZE_CONTENT, PRF->tocco.apertura.h);
    lv_obj_set_style_pad_hor(salta, PRF->geo.pad, 0);
    lv_obj_set_style_radius(salta, PRF->geo.radius_btn, 0);
    ui_bordo(salta, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_align(salta, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(salta, su_avanti, LV_EVENT_CLICKED, NULL);
    ui_testo(salta, tr(TX_SETUP_LATER), C_TXT, FT_M);
    ui_tocco_su_tutto(salta);

    ui_spazio(c);

    tastiera(c, campo == 0 ? tr(TX_COMMON_NEXT) : tr(TX_SETUP_CONNECT), su_tasto);
}

/* --- passo 3: la prova -------------------------------------------------- */

/* L'indirizzo di **questo** pannello. Era scritto `pannello.local` fisso, e
   al primo avvio e' proprio la riga che si legge per sapere dove andare. */
static const char *dove_incollare(void)
{
    char host[32];
    static char dove[64];
    lv_snprintf(dove, sizeof dove, tr(TX_SETUP_PASTE_AT),
                cfg_nome_host(host, sizeof host));
    return dove;
}

static void passo_prova(lv_obj_t *c)
{
    testata(c, tr(TX_SETUP_TEST_TITLE), tr(TX_SETUP_STEP4));

    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    /* --- i quattro controlli, guardati e non dichiarati ------------------
     *
     * Erano quattro booleani scritti a mano: tre spuntati e uno no, sempre,
     * qualunque cosa stesse facendo il pannello. Su una schermata che si
     * chiama "prova di collegamento" e il difetto peggiore possibile —
     * dichiara riuscito un collegamento che nessuno ha provato, e chi
     * installa se ne va convinto che funzioni.
     *
     * Adesso vengono da rete_info_t e ha_stato(), cioe dalle stesse due
     * fonti che alimentano la diagnostica. Un controllo puo essere in tre
     * stati e non due: fatto, in corso, fallito — e il terzo esiste perche
     * "token rifiutato" non e "sto ancora provando", e aspettare un
     * clessidra che non finira mai e il modo peggiore di scoprirlo. */
    enum { ATTESA, FATTO, GUASTO };
    const rete_info_t ri = sistema_rete();
    const ha_stato_t hs = ha_stato();

    struct { const char *nome; int come; const char *nota; } P[4];

    P[0] = (typeof(P[0])){ tr(TX_SETTINGS_WIFI_NETWORK), ri.connessa ? FATTO : ATTESA,
                           ri.descrizione };
    P[1] = (typeof(P[1])){ tr(TX_SETUP_ADDRESS_REACHABLE),
                           hs >= HA_AUTENTICO ? FATTO
                           : hs == HA_CADUTO ? GUASTO : ATTESA,
                           hs == HA_CADUTO ? ha_motivo() : "" };
    /* Senza token non c'e niente da aspettare, e mostrare una clessidra
       vorrebbe dire far guardare un'attesa che non finira mai. Chi ha
       premuto "lo faccio dopo" lo ha deciso: qui gli si ricorda dove
       finire, invece di lasciarlo davanti a tre righe ferme. */
    const bool c_e_token = segreti_impostato(SEG_HA_TOKEN);

    P[2] = (typeof(P[2])){ tr(TX_SETUP_TOKEN_ACCEPTED),
                           hs >= HA_ALLINEO ? FATTO
                           : hs == HA_TOKEN_RIFIUTATO ? GUASTO : ATTESA,
                           hs == HA_TOKEN_RIFIUTATO
                           ? tr(TX_SETUP_TOKEN_REFUSED)
                           : !c_e_token
                           ? dove_incollare()
                           : "" };
    P[3] = (typeof(P[3])){ tr(TX_SETUP_ENTITIES_SUBSCRIBED),
                           hs == HA_PRONTO ? FATTO : ATTESA,
                           !c_e_token ? tr(TX_SETUP_WHEN_TOKEN) : "" };

    for (unsigned n = 0; n < 4; n++) {
        lv_obj_t *r = ui_pannello(k, C_CARD);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_height(r, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

        ui_icona(r, P[n].come == FATTO  ? ICO_CHECK
                  : P[n].come == GUASTO ? ICO_WARNING
                                        : ICO_HOURGLASS_EMPTY,
                 P[n].come == FATTO  ? C_OK
                 : P[n].come == GUASTO ? C_RICON_TXT : C_DIM, IC_S);

        /* Nome e motivo in colonna, non affiancati: con il nome che cresce
           per riempire la riga, il motivo finiva spinto all'estremita
           opposta e sembrava riferito a un'altra cosa. Sotto, e chiaro a
           chi appartiene. */
        lv_obj_t *testi = ui_pannello(r, C_CARD);
        lv_obj_set_flex_grow(testi, 1);
        lv_obj_set_height(testi, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(testi, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(testi, COM.gap_stretto, 0);

        ui_testo(testi, P[n].nome, C_TXT, FT_M);

        if (P[n].nota && P[n].nota[0]) {
            lv_obj_t *nt = ui_testo(testi, P[n].nota,
                                    P[n].come == GUASTO ? C_RICON_TXT : C_DIM,
                                    FT_S);
            lv_label_set_long_mode(nt, LV_LABEL_LONG_DOT);
            lv_obj_set_width(nt, LV_PCT(100));
        }
    }

    static char riepilogo[128];
    lv_snprintf(riepilogo, sizeof riepilogo, tr(TX_SETUP_SUMMARY),
                rete[0] ? rete : tr(TX_SETUP_NONE),
                indirizzo[0] ? indirizzo : tr(TX_SETTINGS_NO_ADDRESS));
    ui_testo(k, riepilogo, C_DIM, FT_S);

    lv_obj_t *piede = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(piede, LV_OPA_TRANSP, 0);
    lv_obj_set_width(piede, LV_PCT(100));
    lv_obj_set_height(piede, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(piede, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(piede, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    /* Si puo cominciare comunque. Un pannello che non lascia uscire finche
       Home Assistant non risponde e un pannello che si puo bloccare per un
       server spento: da dentro non si aggiusta niente, mentre dalla home si
       arriva alle Impostazioni e alla pagina web. Il pulsante pero dice
       quale delle due cose sta facendo. */
    pulsante_avanti(piede,
                    solo_rete         ? tr(ri.connessa ? TX_COMMON_DONE : TX_COMMON_CLOSE)
                    : hs == HA_PRONTO ? tr(TX_SETUP_START)
                    : !c_e_token      ? tr(TX_SETUP_START_THEN_TOKEN)
                                      : tr(TX_SETUP_START_ANYWAY), true);
}

/* --- costruzione -------------------------------------------------------- */

static void rinfresca_prova(lv_timer_t *t)
{
    LV_UNUSED(t);
    costruisci();
}

static void costruisci(void)
{
    if (!radice) return;
    lv_obj_clean(radice);

    if (passo == P_PROVA && !rinfresco_prova)
        rinfresco_prova = lv_timer_create(rinfresca_prova, 1000, NULL);
    else if (passo != P_PROVA && rinfresco_prova) {
        lv_timer_delete(rinfresco_prova);
        rinfresco_prova = NULL;
    }

    switch (passo) {
    case P_CHIAVE: passo_chiave(radice); break;
    case P_HA:     passo_ha(radice);     break;
    case P_PROVA:  passo_prova(radice);  break;
    default:       passo_rete(radice);   break;
    }
}

void stato_primo_avvio(bool si, int passo_iniziale)
{
    if (radice) { lv_obj_delete(radice); radice = NULL; }
    if (attesa_scansione) { lv_timer_delete(attesa_scansione); attesa_scansione = NULL; }
    if (rinfresco_prova) { lv_timer_delete(rinfresco_prova); rinfresco_prova = NULL; }
    scansione_partita = false;
    solo_rete = false;
    if (!si) return;

    passo = (passo_iniziale >= 0 && passo_iniziale < PASSI) ? passo_iniziale : 0;
    campo = 0;
    salvataggio_fallito = false;

    /* Quello che il pannello sa gia. Al primo avvio vero non sa niente e
       questi restano vuoti; dopo un ripristino a meta — corrente staccata
       fra un passo e l'altro — si riprende da dove si era invece di
       ricominciare. */
    lv_snprintf(rete, sizeof rete, "%s", cfg_testo("system/network/ssid", ""));
    lv_snprintf(indirizzo, sizeof indirizzo, "%s",
                cfg_testo("home_assistant/host", ""));

    /* Arrivando direttamente a un passo avanzato — succede solo dal
       simulatore, per guardare una schermata — i passi prima si danno per
       fatti, altrimenti si vedrebbero campi vuoti che nessuno ha avuto modo
       di riempire. */
    if (passo > 0 && !rete[0]) {
        rete_scelta = 0;
        rete_protetta = true;
        lv_snprintf(rete, sizeof rete, "%s", "CasaEsempio");
        lv_snprintf(chiave, sizeof chiave, "%s", "unapassword");
        if (!indirizzo[0])
            lv_snprintf(indirizzo, sizeof indirizzo, "%s", "192.0.2.10");
        lv_snprintf(token, sizeof token, "%s", "eyJhbGciOiJIUzI1NiIsInR5");
    }

    if (passo == P_RETE) cerca_reti();

    radice = ui_pannello(lv_layer_top(), C_BG);
    lv_obj_set_size(radice, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(radice, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(radice, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(radice, PRF->geo.pad, 0);
    lv_obj_set_style_pad_row(radice, PRF->geo.gap, 0);

    costruisci();
}

/* See stati.h. It starts from the network list with nothing chosen: the old
   network is the one that does not connect, and offering it already
   selected would invite the same mistake again. */
void stato_cambia_rete(void)
{
    stato_primo_avvio(true, P_RETE);
    solo_rete = true;
    rete_scelta = -1;
    lv_memzero(chiave, sizeof chiave);
    costruisci();
}
