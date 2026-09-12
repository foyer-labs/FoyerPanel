/* ------------------------------------------------------------------------
 * Diagnostica — 10-diagnostica.md §3.
 *
 * "E l'unico modo per capire cosa non va su un pannello incassato a muro."
 * Niente tastiera, niente porta seriale raggiungibile: se un giorno smette
 * di aggiornare i valori o si riavvia da solo, deve poterlo
 * raccontare da solo.
 *
 * In alto i contatori, con i valori fuori soglia in ambra: heap sotto
 * 40 KB, fps sotto 10, ultimo dato oltre 60 secondi. In basso il registro
 * scorrevole con i quattro filtri.
 *
 * "Copia negli appunti" su un pannello non ha senso, quindi diventa
 * "Mostra come QR": un codice che porta all'URL di /api/log, da inquadrare
 * col telefono.
 * --------------------------------------------------------------------- */
#include "aggiornamento.h"
#include "registro.h"
#include "sistema.h"
#include "stati.h"
#include "comuni.h"
#include "dati.h"
#include "ha.h"
#include "versione.h"
#include "schermate.h"
#include "ui.h"

/* Le soglie di 10-diagnostica.md §3. Non sono misure di layout: sono i
   valori oltre i quali un contatore va guardato. */
#define SOGLIA_HEAP        40960
/* Quante righe di registro si costruiscono. Dodici riempiono lo schermo
   con un po' di scorrimento; il resto si legge da /api/log. Il numero
   non e estetico: e quanto ci sta nel blocco di memoria di LVGL
   insieme al resto della schermata. */
#define RIGHE_A_SCHERMO 12

#define SOGLIA_ULTIMO_DATO    60

/* Otto contatori in quattro colonne: e la forma della griglia, non una
   misura. */
#define CONT_COLONNE 4
#define CONT_RIGHE   2

static livello_t filtro = LOG_DEBUG;   /* LOG_DEBUG = tutti */

static void su_filtro(lv_event_t *e)
{
    filtro = (livello_t)(intptr_t)lv_event_get_user_data(e);
    ui_vai_a(SEZ_IMPOSTAZIONI, ui_vista());
}

static lv_color_t colore_livello(livello_t l)
{
    switch (l) {
    case LOG_ERRORE: return C_WARN;
    case LOG_AVVISO: return C_ACC;
    case LOG_INFO:   return C_DIM;
    default:         return C_OFF;
    }
}

static const char *nome_livello(livello_t l)
{
    switch (l) {
    case LOG_ERRORE: return tr(TX_DIAG_LEVEL_ERROR);
    case LOG_AVVISO: return tr(TX_DIAG_LEVEL_WARNING);
    case LOG_INFO:   return tr(TX_DIAG_LEVEL_INFO);
    default:         return tr(TX_DIAG_LEVEL_DEBUG);
    }
}

/* --- i due comandi in fondo ---------------------------------------------
 *
 * Il registro di un pannello incassato si legge dal telefono: sul vetro ce
 * ne stanno dodici righe, e quella che serve e quasi sempre piu vecchia.
 * Il QR porta a /api/log, che le da tutte.
 *
 * Senza rete non c'e nessun indirizzo a cui mandare qualcuno, e un QR che
 * porta a "http:///api/log" e un QR che fa perdere tempo: si dice invece
 * cosa manca. */
static void su_qr(lv_event_t *e)
{
    LV_UNUSED(e);
    const rete_info_t r = sistema_rete();
    if (!r.connessa || !r.indirizzo[0]) {
        avviso(tr(TX_DIAG_FULL_LOG), tr(TX_DIAG_LOG_NEEDS_NETWORK));
        return;
    }

    static char url[64], sotto[64];
    lv_snprintf(url,   sizeof url,   "http://%s/api/log", r.indirizzo);
    lv_snprintf(sotto, sizeof sotto, "%s", url);
    mostra_qr(tr(TX_DIAG_FULL_LOG), url, sotto);
}

/* Svuotare il registro non si conferma: non si perde niente che non sia
   gia successo, e chi lo preme lo preme apposta — di solito un attimo
   prima di rifare il guasto che sta cercando. Un modale in mezzo
   costringerebbe a due tocchi ogni volta per proteggere delle righe. */
static void su_svuota(lv_event_t *e)
{
    LV_UNUSED(e);
    registro_svuota();
    ui_vai_a(SEZ_IMPOSTAZIONI, ui_vista());
}

/* --- contatori ---------------------------------------------------------- */

static void contatore(lv_obj_t *g, int n, const char *nome, const char *valore,
                      bool fuori_soglia)
{
    lv_obj_t *k = ui_scheda(g);
    lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, n % CONT_COLONNE, 1,
                            LV_GRID_ALIGN_STRETCH, n / CONT_COLONNE, 1);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(k, COM.gap_stretto, 0);
    if (fuori_soglia) ui_bordo(k, LV_BORDER_SIDE_FULL, C_SEL_LINE);

    lv_obj_t *v = ui_testo(k, valore, fuori_soglia ? C_ACC : C_TXT, FT_M);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    lv_obj_set_width(v, LV_PCT(100));
    ui_occhiello(k, nome);
}

static void contatori(lv_obj_t *c)
{
    const contatori_t k = dati_contatori();

    lv_obj_t *g = ui_griglia(c, CONT_COLONNE, CONT_RIGHE);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_height(g, PRF->energia.fascia_h * CONT_RIGHE
                      + PRF->geo.gap * (CONT_RIGHE - 1));

    static char b[8][32];
    int n = 0;

    lv_snprintf(b[0], sizeof b[0], tr(TX_DIAG_UPTIME_VALUE), k.accensione_s / 86400,
                (k.accensione_s % 86400) / 3600);
    contatore(g, n++, tr(TX_DIAG_UPTIME), b[0], false);

    lv_snprintf(b[1], sizeof b[1], "%u · %s", k.riavvii, k.motivo_ultimo_riavvio);
    contatore(g, n++, tr(TX_DIAG_RESTARTS), b[1], false);

    lv_snprintf(b[2], sizeof b[2], "%u kB", k.heap_libero / 1024);
    contatore(g, n++, tr(TX_DIAG_HEAP_FREE), b[2], k.heap_libero < SOGLIA_HEAP);

    /* Il minimo storico e il numero che conta: e il primo sintomo di una
       perdita di memoria, e si vede giorni prima del riavvio. */
    lv_snprintf(b[3], sizeof b[3], "%u kB", k.heap_minimo / 1024);
    contatore(g, n++, tr(TX_DIAG_HEAP_MIN), b[3], k.heap_minimo < SOGLIA_HEAP);

    /* --- non sono "fps" nel senso di quanto e veloce -------------------
     *
     * Da quando la schermata si rifa solo se e cambiato qualcosa che mostra,
     * questo numero dice **quante volte al secondo l'immagine e cambiata**,
     * non quanti fotogrammi il pannello saprebbe disegnare. Su uno schermo
     * fermo e zero, ed e giusto che sia zero.
     *
     * Per questo non e piu rosso quando e basso: un pannello che non
     * ridisegna niente perche non e successo niente sta funzionando
     * benissimo, e segnarlo come guasto insegnerebbe a ignorare l'unico
     * posto dove si va a cercare quando qualcosa non va.
     *
     * Quanto in fretta sappia disegnare e un'altra domanda, e si misura
     * apposta: 11-collaudo.md la chiede mentre si scorre, che e la
     * condizione in cui la risposta conta. */
    lv_snprintf(b[4], sizeof b[4], "%u", k.fps);
    contatore(g, n++, tr(TX_DIAG_REDRAWS), b[4], false);

    lv_snprintf(b[5], sizeof b[5], "%s · %u s", ui_ha_stato(ha_stato()),
                k.ha_ultimo_dato_s);
    contatore(g, n++, tr(TX_DIAG_HA), b[5],
              k.ha_ultimo_dato_s > SOGLIA_ULTIMO_DATO);

    lv_snprintf(b[6], sizeof b[6], tr(TX_DIAG_COMMANDS_VALUE), k.comandi_inviati,
                k.comandi_falliti);
    contatore(g, n++, tr(TX_DIAG_COMMANDS), b[6], k.comandi_falliti > 0);

    lv_snprintf(b[7], sizeof b[7], "%d dBm", k.wifi_rssi);
    contatore(g, n++, tr(TX_DIAG_WIFI), b[7], false);
}

/* --- registro ----------------------------------------------------------- */

static void filtri(lv_obj_t *padre)
{
    static const struct { tx_t nome; livello_t l; } F[] = {
        { TX_DIAG_FILTER_ALL,      LOG_DEBUG  },
        { TX_DIAG_FILTER_ERRORS,   LOG_ERRORE },
        { TX_DIAG_FILTER_WARNINGS, LOG_AVVISO },
        { TX_DIAG_FILTER_INFO,     LOG_INFO   },
    };

    lv_obj_t *f = ui_pannello(padre, C_CARD);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_size(f, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    for (unsigned n = 0; n < sizeof F / sizeof F[0]; n++) {
        const bool sel = F[n].l == filtro;
        lv_obj_t *b = ui_pannello(f, sel ? C_ACC : C_CARD2);
        lv_obj_set_size(b, PRF->tocco.apertura.w, PRF->tocco.pager.h);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, sel ? C_ACC : C_LINE);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, su_filtro, LV_EVENT_CLICKED,
                            (void *)(intptr_t)F[n].l);
        ui_testo(b, tr(F[n].nome), sel ? C_INK : C_DIM, FT_S);
    }
}

static void registro(lv_obj_t *c)
{
    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    lv_obj_t *hd = ui_pannello(k, C_CARD);
    lv_obj_set_width(hd, LV_PCT(100));
    lv_obj_set_height(hd, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hd, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hd, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_occhiello(hd, tr(TX_DIAG_LOG_HEADER));
    ui_spazio(hd);
    filtri(hd);

    lv_obj_t *elenco = ui_pannello(k, C_CARD);
    lv_obj_set_width(elenco, LV_PCT(100));
    lv_obj_set_flex_grow(elenco, 1);
    lv_obj_set_flex_flow(elenco, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(elenco, COM.gap_stretto, 0);
    lv_obj_add_flag(elenco, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(elenco, LV_DIR_VER);

    /* --- quante righe si costruiscono davvero -------------------------
     *
     * L'elenco si scorre, ma LVGL costruisce **tutti** i figli, anche
     * quelli fuori dall'area visibile: trentadue righe da quattro oggetti
     * l'una sono centoventotto oggetti piu il testo, e il blocco di memoria
     * di LVGL — 64 kB fissi in RAM interna, che su questo chip e la risorsa
     * scarsa — non li regge. Sul pannello si vedeva cosi: entrando in
     * Impostazioni > Sistema, lv_obj_create restituiva NULL, nessuno lo
     * controllava, e la prima scrittura dentro quel puntatore riavviava
     * l'apparecchio.
     *
     * La cura non e allargare il blocco: quella memoria e la stessa che
     * serve a Wi-Fi, TLS e mDNS, e ne restano gia poche decine di kilobyte.
     * La cura e smettere di costruire quello che non si guarda.
     *
     * Il registro intero resta leggibile da /api/log, dove non c'e nessun
     * oggetto grafico da costruire e le righe costano quello che pesano. */
    int mostrate = 0;

    /* Le piu recenti in alto. */
    for (int n = 0; n < dati_log() && mostrate < RIGHE_A_SCHERMO; n++) {
        const riga_log_t *r = dati_log_riga(n);
        if (!r) continue;
        if (filtro != LOG_DEBUG && r->livello != filtro) continue;
        mostrate++;

        lv_obj_t *riga = ui_pannello(elenco, C_CARD);
        lv_obj_set_width(riga, LV_PCT(100));
        lv_obj_set_height(riga, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(riga, PRF->geo.gap, 0);

        /* 10-diagnostica.md §3 chiede i numeri monospaziati, cioe
           incolonnati, per scorrerli con l'occhio. Non serve un font
           monospaziato: Inter e compilata con le cifre tabulari, quindi gli
           orari si incolonnano gia e restano leggibili al corpo del testo,
           che un monospaziato da 30 px non sarebbe. */
        ui_testo(riga, r->ora, C_DIM, FT_S);

        lv_obj_t *liv = ui_testo(riga, nome_livello(r->livello),
                                 colore_livello(r->livello), FT_XS);
        lv_obj_set_width(liv, PRF->clima.timer_passo.w);

        ui_testo(riga, r->sorgente, C_OFF, FT_XS);

        lv_obj_t *m = ui_testo(riga, r->messaggio, C_TXT, FT_S);
        lv_label_set_long_mode(m, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(m, 1);
    }

    /* Un elenco vuoto senza una riga che lo dica sembra una schermata a
       meta, e chi ha appena premuto Svuota registro non ha modo di sapere
       se ha funzionato o se si e rotto qualcosa. Vale anche per il filtro:
       "nessun errore" e una notizia buona, ma va detta. */
    if (mostrate == 0) {
        lv_obj_t *v = ui_testo(elenco,
            filtro == LOG_DEBUG ? tr(TX_DIAG_LOG_EMPTY)
                                : tr(TX_DIAG_NO_ROWS),
            C_DIM, FT_S);
        lv_obj_set_width(v, LV_PCT(100));
    }

    /* In fondo i due comandi. */
    lv_obj_t *piede = ui_pannello(k, C_CARD);
    lv_obj_set_width(piede, LV_PCT(100));
    lv_obj_set_height(piede, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(piede, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(piede, PRF->geo.gap, 0);

    /* I due comandi erano cliccabili e non chiamavano niente: avevano
       l'aspetto di pulsanti che funzionano, la pastiglia si illuminava
       sotto il dito, e non succedeva niente. Un comando che non fa niente
       e peggio di un comando che manca, perche chi lo preme conclude che
       il pannello si e bloccato. */
    static const struct { const char *ico; tx_t nome; void (*fa)(lv_event_t *); }
    CMD[] = {
        { ICO_QR_CODE_2, TX_DIAG_SHOW_QR,   su_qr     },
        { ICO_DELETE,    TX_DIAG_CLEAR_LOG, su_svuota },
    };
    for (unsigned n = 0; n < sizeof CMD / sizeof CMD[0]; n++) {
        lv_obj_t *b = ui_pannello(piede, C_CARD2);
        lv_obj_set_height(b, PRF->tocco.apertura.h);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_style_min_width(b, 0, 0);
        lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
        ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(b, COM.pastiglia_gap, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, CMD[n].fa, LV_EVENT_CLICKED, NULL);
        ui_icona(b, CMD[n].ico, C_DIM, IC_S);
        ui_testo(b, tr(CMD[n].nome), C_TXT, FT_S);
    }
}

void schermata_diagnostica(lv_obj_t *c)
{
    const contatori_t k = dati_contatori();

    /* La versione qui non e un vezzo: dopo un aggiornamento e l'unico modo
       per sapere se e andata. Un pannello che ha rifiutato la versione
       nuova e tornato alla precedente si riaccende identico a com'era, e
       senza questa riga "e andata" e "e tornata indietro" sono la stessa
       schermata.

       "in prova" e la finestra in cui il firmware nuovo deve dimostrarsi
       buono: se resta li piu di due minuti qualcosa non gli fa chiudere il
       giro, ed e il momento di guardare il registro qui sotto. */
    /* Con la data di compilazione accanto alla versione: due versioni
       possono chiamarsi allo stesso modo — una compilata con l'albero
       sporco si chiama come quella di prima — e allora l'orario e l'unica
       cosa che distingue «ho flashato» da «credo di aver flashato». */
    static char sotto[128];
    lv_snprintf(sotto, sizeof sotto, trn(TXN_DIAG_SUBTITLE, k.errori_recenti),
                agg_versione(), agg_in_prova() ? tr(TX_DIAG_ON_TRIAL) : "",
                PANNELLO_COMPILATO_BREVE, k.errori_recenti);
    ui_testata(tr(TX_COMMON_SETTINGS), sotto);

    contatori(c);
    registro(c);
}
