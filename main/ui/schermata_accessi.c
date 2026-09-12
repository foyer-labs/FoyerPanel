/* ------------------------------------------------------------------------
 * Accessi — 01-specifica-ui.md §3.5.
 *
 * Qui sta la regola piu importante di tutto il progetto: **il pannello non
 * mostra uno stato che non conosce**. Gli accessi sono `switch`, il pannello
 * manda un impulso e non sa se il cancello si e aperto. Nessuna riga
 * dichiara "chiuso" senza un sensore che lo affermi.
 *
 * L'asimmetria e voluta e deve vedersi:
 *   - la porta del garage ha un sensore vero e dice "aperta" o "chiusa" davvero
 *   - gli altri due accessi mostrano solo quando sono stati usati l'ultima
 *     volta, che e tutto cio che il pannello sa
 *   - le luci del giardino non sono un impulso ma un interruttore, quindi hanno
 *     un interruttore e non un pulsante "Apri", separate dalle altre da uno
 *     spazio maggiore
 *
 * **Qui c'era una telecamera**, a sinistra in orizzontale e in alto in
 * verticale, con quattro miniature sotto: mostrava chi c'era mentre si
 * apriva il cancello. E' uscita con tutto il resto del video, e i comandi
 * si prendono la larghezza intera in tutti e due gli orientamenti — come
 * nella sezione Interruttori, che fa la stessa cosa: righe piene, un
 * bersaglio grande, niente da mirare.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "ui.h"
#include "widgets/interruttore.h"
#include "widgets/pulsante_comando.h"

/* --- colonna dei comandi ------------------------------------------------ */

/* --- dal dito al cancello -----------------------------------------------
 *
 * Il riscontro sta **nel pulsante toccato** (01-specifica-ui.md §4.4), non
 * in una fascia altrove: chi ha appena premuto guarda li, e li deve trovare
 * la risposta.
 *
 * "Inviato" dichiara che Home Assistant ha accettato il comando, non che il
 * cancello si sia aperto. Sono due cose diverse e il pannello sa solo la
 * prima: un impulso non ha riscontro, e inventarne uno sarebbe la bugia che
 * fa tornare indietro a controllare.
 *
 * L'indice dell'accesso viaggia nel dato dell'evento invece che in un
 * puntatore alla scheda: le schede si distruggono a ogni ricostruzione, un
 * intero no. */
static void su_comando(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    const int n = (int)(intptr_t)lv_event_get_user_data(e);

    pulsante_comando_stato(b, CMD_IN_CORSO);
    pulsante_comando_stato(b, dati_accesso_aziona(n) ? CMD_RIUSCITO
                                                     : CMD_FALLITO);
}

/* L'interruttore commuta e basta: lo stato vero lo riporta Home Assistant
   col prossimo evento, e la schermata si rifa da sola. Fingere qui il
   cambio vorrebbe dire mostrarlo acceso anche quando il comando non e
   partito. */
static void su_interruttore(lv_event_t *e)
{
    dati_accesso_aziona((int)(intptr_t)lv_event_get_user_data(e));
}

static void riga_accesso(lv_obj_t *padre, const accesso_t *a)
{
    lv_obj_t *r = ui_scheda(padre);
    lv_obj_set_width(r, LV_PCT(100));
    /* Alta quanto il suo contenuto, non quanto lo spazio disponibile.
       Cresceva — flex_grow(1) — e andava bene finche la colonna era stretta
       e divideva l'altezza con la telecamera: quattro righe si spartivano
       quel che restava. Da sole in una schermata intera diventavano alte un
       quarto di vetro ciascuna, con il nome perso in mezzo a un rettangolo
       vuoto. */
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

    const char *ico = a->tipo == ACC_INTERRUTTORE ? ICO_LIGHTBULB
                    : a->ha_sensore               ? ICO_GARAGE
                                                  : ICO_FENCE;
    ui_icona(r, ico, C_DIM, IC_M);

    lv_obj_t *col = ui_pannello(r, C_CARD);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *nome = ui_testo(col, a->nome, C_TXT, FT_M);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nome, LV_PCT(100));

    /* Cosa il pannello puo davvero dire di questa riga. */
    const char *stato;
    static char buf[8][40];
    static uint8_t giro;
    if (!a->disponibile) {
        stato = tr(TX_COMMON_UNAVAILABLE);
    } else if (a->tipo == ACC_INTERRUTTORE) {
        stato = a->acceso ? tr(TX_ACCESS_LIGHTS_ON) : tr(TX_ACCESS_LIGHTS_OFF);
    } else if (a->ha_sensore) {
        /* Il sensore c'e, ma se non risponde non si dice "chiusa": e la
           parola che qualcuno leggerebbe prima di andare a dormire. */
        stato = !a->stato_noto ? tr(TX_ACCESS_STATE_UNKNOWN)
              : a->aperto      ? tr(TX_ACCESS_OPEN) : tr(TX_ACCESS_CLOSED);
    } else if (a->ultimo_uso) {
        char *b = buf[giro++ % 8];
        lv_snprintf(b, sizeof buf[0], tr(TX_ACCESS_LAST_PULSE), a->ultimo_uso);
        stato = b;
    } else {
        /* Non si scrive "chiuso": non lo sappiamo. */
        stato = tr(TX_ACCESS_NO_FEEDBACK);
    }
    ui_testo(col, stato, C_DIM, FT_S);

    if (a->tipo == ACC_INTERRUTTORE) {
        lv_obj_t *i = interruttore(r, a->acceso && a->disponibile,
                                   a->disponibile);
        if (a->disponibile)
            lv_obj_add_event_cb(i, su_interruttore, LV_EVENT_CLICKED,
                                (void *)(intptr_t)a->indice);
    } else {
        lv_obj_t *b = pulsante_comando(r, tr(TX_ACCESS_OPEN_BUTTON), a->conferma,
                                       a->disponibile);
        if (a->disponibile)
            lv_obj_add_event_cb(b, su_comando, EV_COMANDO_SCATTATO,
                                (void *)(intptr_t)a->indice);
    }
}

static void colonna_comandi(lv_obj_t *padre)
{
    lv_obj_t *col = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, PRF->geo.gap, 0);
    /* Larghezza intera in tutti e due gli orientamenti. In orizzontale
       questa colonna era stretta — dettaglio_col_w — perche accanto c'era
       la telecamera; da sola, tenerla stretta lascerebbe due terzi di vetro
       vuoti alla sua destra senza che niente spieghi perche. */
    lv_obj_set_width(col, LV_PCT(100));
    lv_obj_set_flex_grow(col, 1);

    /* Prima gli accessi a impulso, poi — staccate da uno spazio maggiore —
       le righe a interruttore: sono un'altra cosa e devono sembrarlo. */
    for (int passo = 0; passo < 2; passo++) {
        const tipo_accesso_t voluto = passo == 0 ? ACC_IMPULSO
                                                 : ACC_INTERRUTTORE;
        bool primo = true;
        for (int n = 0; n < dati_accessi(); n++) {
            const accesso_t *a = dati_accesso(n);
            if (!a || a->tipo != voluto) continue;
            if (passo == 1 && primo) {
                lv_obj_t *stacco = ui_pannello(col, C_BG);
                lv_obj_set_style_bg_opa(stacco, LV_OPA_TRANSP, 0);
                lv_obj_set_size(stacco, LV_PCT(100), PRF->geo.gap);
                primo = false;
            }
            riga_accesso(col, a);
        }
    }

    /* --- qui c'era il registro degli eventi, e non c'e piu --------------
     *
     * Un elenco di orari e di frasi — "07:12 cancello pedonale aperto" — che
     * nessuna entita di Home Assistant alimentava: erano i dati finti, che
     * servono a guardare una schermata prima di appenderla al muro. Sul
     * pannello vero raccontava una giornata che non era successa.
     *
     * Toglierlo e meglio che lasciarlo vuoto: un riquadro vuoto in fondo a
     * una schermata sembra un guasto, e questo invece e un pezzo che non e
     * ancora nato. Quando ci sara da cui prenderli davvero — il registro
     * dei logbook di Home Assistant e la strada — si rifara qui. */
}

/* --- costruzione -------------------------------------------------------- */

void schermata_accessi(lv_obj_t *c)
{
    /* --- what the panel can say about all of them together -------------
     *
     * Only what the sensors say. The subtitle named "the garage door": the
     * house this panel was written for had one access with a sensor, and
     * it was that one. Now the accesses with a sensor are counted, named
     * when there is one, and "all closed" is said only when every access
     * has a sensor — an access that only sends pulses may be open, and a
     * summary cannot promise what a row does not know. */
    const accesso_t *unico = NULL;
    int con = 0, aperti = 0, muti = 0, senza = 0;
    for (int n = 0; n < dati_accessi(); n++) {
        const accesso_t *a = dati_accesso(n);
        if (!a || a->tipo == ACC_INTERRUTTORE) continue;
        if (!a->ha_sensore) { senza++; continue; }
        con++;
        unico = a;
        if (!a->stato_noto) muti++;
        else if (a->aperto) aperti++;
    }

    static char sotto[96];
    char stato[64] = "";
    if (con == 1 && !unico->stato_noto)
        lv_snprintf(stato, sizeof stato, tr(TX_ACCESS_NAMED_SENSOR_DOWN), unico->nome);
    else if (con == 1)
        lv_snprintf(stato, sizeof stato, unico->aperto ? tr(TX_ACCESS_NAMED_OPEN)
                                                       : tr(TX_ACCESS_NAMED_CLOSED),
                    unico->nome);
    else if (aperti)
        lv_snprintf(stato, sizeof stato, trn(TXN_ACCESS_COUNT_OPEN, aperti), aperti);
    else if (muti)
        lv_snprintf(stato, sizeof stato, trn(TXN_ACCESS_SENSORS_DOWN, muti), muti);
    else if (con && !senza)
        lv_snprintf(stato, sizeof stato, "%s", tr(TX_ACCESS_ALL_CLOSED));

    if (!con && senza)
        lv_snprintf(sotto, sizeof sotto, "%s", tr(TX_ACCESS_PULSES_ONLY));
    else if (stato[0] && senza)
        lv_snprintf(sotto, sizeof sotto, tr(TX_ACCESS_OTHERS_NO_FEEDBACK), stato);
    else
        lv_snprintf(sotto, sizeof sotto, "%s", stato);
    ui_testata(tr(TX_SECTION_ACCESS), sotto[0] ? sotto : NULL);

    colonna_comandi(c);
}
