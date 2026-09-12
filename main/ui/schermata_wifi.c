/* ------------------------------------------------------------------------
 * Condivisione Wi-Fi — 01-specifica-ui.md §3.7.
 *
 * Rete ospiti: a sinistra il codice QR su **fondo bianco**, che non e una
 * scelta estetica — le fotocamere dei telefoni faticano su fondo scuro, e
 * un QR che non si legge non serve a niente. A destra nome, password
 * monospaziata e il riquadro che spiega l'isolamento della rete.
 *
 * Reti private: fino a PRIVATE_MAX, tutte configurate dalla pagina web. Se
 * ce n'e piu di una si sceglie da un elenco, e si tocca quella che si vuole
 * mostrare.
 *
 * **Niente PIN.** C'e stato, con tentativi e blocco a tempo, perche la
 * specifica lo chiedeva. Ma questo e un pannello a muro in casa: chi gli sta
 * davanti e gia dentro, ha aperto la porta, e se volesse la rete di casa la
 * chiederebbe a voce. Il PIN non proteggeva da nessuno che non fosse gia
 * oltre la protezione vera, e in cambio chiedeva tre schermate, un
 * tastierino e quattro cifre da ricordare per fare una cosa che si fa due
 * volte l'anno. Il conto era in perdita, ed e stato chiuso.
 *
 * Quello che resta e la sola distinzione che serve davvero, ed e nel testo:
 * la rete ospiti da Internet e basta, la rete di casa da accesso a tutto.
 * --------------------------------------------------------------------- */
#include <string.h>

#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "segreti.h"
#include "tempi.h"
#include "ui.h"

/* Le viste della sezione. */
enum { VISTA_ELENCO = 0, VISTA_QR };

/* Quale rete si sta mostrando. Parte dalla prima e non da "nessuna": la
   vista del codice senza una rete scelta non e uno stato che il pannello
   raggiunge toccando — su_rete() la assegna sempre prima di navigare — ma e
   quello in cui si trova la cattura del simulatore, che salta dritta alla
   vista. */
static int scelta;

static void su_vista(lv_event_t *e)
{
    ui_vai_a(SEZ_WIFI, (int)(intptr_t)lv_event_get_user_data(e));
}

static void su_rete(lv_event_t *e)
{
    scelta = (int)(intptr_t)lv_event_get_user_data(e);
    ui_vai_a(SEZ_WIFI, VISTA_QR);
}

/* --- codice QR ---------------------------------------------------------- */

/* Formato di 01-specifica-ui.md §3.7. Va composto cosi, non inventato: un
   telefono che non riconosce la stringa non si collega e nessuno capisce
   perche. */
/* Su questa schermata la password **e** il contenuto: sta sul vetro, nel QR
   e, per la rete ospiti, anche in chiaro. Quello che il contratto vieta e
   che esca dal pannello — dalla pagina web, dal registro, dalla
   configurazione esportata — non che si veda qui, che e tutto il motivo per
   cui la schermata esiste. Si prende con segreti_usa(), per il tempo di
   comporre una stringa, e non viene tenuta da nessun'altra parte. */
static void componi_qr(const char *pw, void *dato)
{
    const rete_t *r = ((void **)dato)[0];
    char *s = ((void **)dato)[1];
    /* "none" in the configuration, "nopass" in the QR: that is what the
       standard says and what phones read. The configuration used to say
       "nessuna" and the QR carried it as it was — a string no phone knows,
       so an open network shared by QR never connected anyone. */
    const char *tipo = strcmp(r->sicurezza, "none") == 0 ? "nopass" : r->sicurezza;
    lv_snprintf(s, 160, "WIFI:T:%s;S:%s;P:%s;H:%s;;",
                tipo, r->ssid, pw, r->nascosta ? "true" : "false");
}

static const char *stringa_wifi(const rete_t *r)
{
    static char s[160];
    void *dato[2] = { (void *)r, s };

    /* Senza password impostata non si inventa un QR che non funziona: si
       compone quello per una rete aperta, e la riga accanto dira che la
       password manca. Mostrare uno stato che il pannello non conosce e
       proprio quello che questo progetto non fa. */
    if (!segreti_usa(r->segreto, componi_qr, dato))
        lv_snprintf(s, sizeof s, "WIFI:T:nopass;S:%s;P:;H:%s;;",
                    r->ssid, r->nascosta ? "true" : "false");
    return s;
}

static void copia_password(const char *pw, void *dato)
{
    lv_snprintf((char *)dato, SEGRETO_MAX + 1, "%s", pw);
}

static void riquadro_qr(lv_obj_t *padre, const rete_t *r)
{
    /* Fondo bianco obbligatorio. */
    lv_obj_t *k = ui_pannello(padre, lv_color_white());
    lv_obj_set_style_radius(k, PRF->geo.radius, 0);
    lv_obj_set_style_pad_all(k, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
    if (PRF->wifi.colonna_w) {
        lv_obj_set_size(k, PRF->wifi.colonna_w, LV_PCT(100));
    } else {
        lv_obj_set_width(k, LV_PCT(100));
        lv_obj_set_height(k, LV_SIZE_CONTENT);
    }

    lv_obj_t *qr = lv_qrcode_create(k);
    lv_qrcode_set_size(qr, PRF->wifi.qr);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    const char *s = stringa_wifi(r);
    lv_qrcode_update(qr, s, lv_strlen(s));

    lv_obj_t *l = ui_testo(k, tr(TX_WIFI_SCAN_WITH_PHONE),
                           lv_color_black(), FT_S);
    lv_obj_set_width(l, LV_PCT(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
}

/* --- rete ospiti -------------------------------------------------------- */

static void riga_dato(lv_obj_t *padre, const char *nome, const char *valore,
                      font_ruolo_t corpo)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(k, COM.gap_stretto, 0);
    ui_occhiello(k, nome);
    lv_obj_t *l = ui_testo(k, valore, C_TXT, corpo);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
}

/* --- l'avviso sull'isolamento, e perche non c'e piu -------------------
 *
 * C'era un riquadro che diceva «questa rete da accesso a tutti i
 * dispositivi di casa», e stava solo sulle reti private. Adesso che le reti
 * sono un elenco senza tipo, il pannello **non sa** quale sia la rete degli
 * ospiti e quale quella di casa: dirlo di tutte sarebbe falso per meta, non
 * dirlo di nessuna e almeno onesto.
 *
 * Se servira tornare a dirlo, la strada e una spunta per rete in
 * configurazione — non un indovinello sul nome. */

static void pulsante_cb(lv_obj_t *padre, const char *ico, const char *testo,
                        lv_event_cb_t cb, int dato)
{
    lv_obj_t *b = ui_pannello(padre, C_CARD2);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_height(b, PRF->tocco.apertura.h);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, COM.pastiglia_gap, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)dato);
    ui_icona(b, ico, C_DIM, IC_S);
    ui_testo(b, testo, C_TXT, FT_M);
}

static void pulsante(lv_obj_t *padre, const char *ico, const char *testo,
                     int vista)
{
    pulsante_cb(padre, ico, testo, su_vista, vista);
}

/* --- una riga dell'elenco ----------------------------------------------- */
static void riga_rete(lv_obj_t *padre, int n, const rete_t *r)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(k, PRF->geo.gap, 0);
    lv_obj_add_event_cb(k, su_rete, LV_EVENT_CLICKED, (void *)(intptr_t)n);

    ui_icona(k, ICO_WIFI, C_ACC, IC_M);

    lv_obj_t *testi = ui_pannello(k, C_CARD);
    lv_obj_set_flex_grow(testi, 1);
    lv_obj_set_height(testi, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(testi, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(testi, COM.gap_stretto, 0);
    lv_obj_t *l = ui_testo(testi, r->nome_mostrato, C_TXT, FT_L);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
    ui_testo(testi, r->ssid, C_DIM, FT_S);

    ui_icona(k, ICO_CHEVRON_RIGHT, C_DIM, IC_S);

    /* **Dopo** aver creato i figli, non prima. ui_tocco_su_tutto toglie il
       tocco ai figli che esistono in quel momento: chiamandola su una scheda
       vuota non toglie niente a nessuno, e il riquadro dei testi — che e un
       lv_obj, quindi cliccabile per difetto — si mangia il tocco proprio
       dove e naturale toccare, sul nome della rete. Restavano sensibili
       solo le due icone, che sono etichette e il tocco non lo prendono.

       L'intestazione della funzione lo dice gia a parole, e non e bastato:
       ora se ne accorge da sola e lo scrive nel registro. */
    ui_tocco_su_tutto(k);
}

/* --- l'elenco, che e cio che si vede entrando ---------------------------
 *
 * Prima entrando si vedeva subito il codice della rete ospiti, e le altre
 * stavano in fondo come un ripensamento. Era l'ordine in cui la funzione era
 * cresciuta, non l'ordine in cui la si usa: chi apre questa sezione ha in
 * mente **a chi** sta dando la rete, e quella e la prima domanda a cui
 * rispondere. */
static void vista_elenco(lv_obj_t *c)
{
    ui_testata(tr(TX_SECTION_WIFI), tr(TX_WIFI_CHOOSE_NETWORK));

    lv_obj_t *elenco = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(elenco, LV_OPA_TRANSP, 0);
    lv_obj_set_size(elenco, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(elenco, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(elenco, PRF->geo.gap, 0);

    int quante = 0;
    for (int n = 0; n < dati_reti(); n++) {
        const rete_t r = dati_rete(n);
        if (!r.attiva) continue;
        riga_rete(elenco, n, &r);
        quante++;
    }

    /* Zero reti attive non e un errore: e una sezione che qualcuno ha
       lasciato accesa senza configurarci niente. Si dice, invece di
       mostrare una pagina vuota che sembra rotta. */
    if (!quante)
        ui_testo(elenco, tr(TX_WIFI_NO_NETWORKS), C_DIM, FT_S);
}

/* --- il codice di una rete ----------------------------------------------
 *
 * Niente conto alla rovescia e niente chiusura automatica: servivano a
 * richiudere una serratura che non c'e piu. Il ritorno alla home dopo due
 * minuti senza tocchi c'era gia e basta a non lasciare il codice sul vetro
 * per sempre.
 *
 * E niente elenco delle altre reti qui sotto: da questa schermata si torna
 * indietro, e l'indietro porta dove si era. Tenere le altre in fondo
 * significava due modi di fare la stessa cosa, e il secondo era quello che
 * non si trovava. */
static void vista_qr(lv_obj_t *c)
{
    const rete_t r = dati_rete(scelta);
    ui_testata(r.nome_mostrato && r.nome_mostrato[0] ? r.nome_mostrato
                                                     : tr(TX_WIFI_NETWORK),
               tr(TX_WIFI_SCAN_TO_CONNECT));

    lv_obj_t *fuori = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(fuori, LV_OPA_TRANSP, 0);
    lv_obj_set_size(fuori, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(fuori, PRF->orientamento == VERTICALE
                                ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(fuori, PRF->geo.gap, 0);
    lv_obj_set_style_pad_row(fuori, PRF->geo.gap, 0);

    riquadro_qr(fuori, &r);

    lv_obj_t *destra = ui_pannello(fuori, C_BG);
    lv_obj_set_style_bg_opa(destra, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(destra, 1);
    lv_obj_set_flex_flow(destra, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(destra, PRF->geo.gap, 0);
    if (PRF->orientamento == VERTICALE) lv_obj_set_width(destra, LV_PCT(100));
    else                                lv_obj_set_height(destra, LV_PCT(100));

    riga_dato(destra, tr(TX_WIFI_NETWORK_NAME), r.ssid, FT_L);

    /* --- la password, se chi configura ha deciso di mostrarla -----------
     *
     * Il valore di riposo e mostrarla: il QR basta a collegarsi, ma chi
     * scrive la password a mano su un portatile ha bisogno di leggerla. Chi
     * non vuole che si possa trascrivere da sopra la spalla toglie la
     * spunta, e allora resta solo il codice.
     *
     * Il buffer vive quanto la schermata: e la stessa vita del pixel acceso
     * che lo mostra. */
    static char pw[SEGRETO_MAX + 1];
    pw[0] = 0;
    const bool pw_c_e = segreti_usa(r.segreto, copia_password, pw);
    riga_dato(destra, tr(TX_WIFI_PASSWORD),
              !pw_c_e             ? tr(TX_WIFI_NOT_SET)
              : r.mostra_password ? pw
                                  : tr(TX_WIFI_HIDDEN_USE_QR),
              r.mostra_password ? FT_MONO : FT_M);

    ui_spazio(destra);
    pulsante(destra, ICO_ARROW_BACK, tr(TX_WIFI_ALL_NETWORKS), VISTA_ELENCO);
}

/* --- costruzione -------------------------------------------------------- */

void schermata_wifi(lv_obj_t *c)
{
    /* Senza una rete scelta non c'e niente da mostrare: si torna all'elenco
       invece di aprire un riquadro vuoto. Succede se la configurazione
       cambia mentre la vista e aperta. */
    if (ui_vista() == VISTA_QR && scelta >= 0 && scelta < dati_reti())
        vista_qr(c);
    else
        vista_elenco(c);
}
