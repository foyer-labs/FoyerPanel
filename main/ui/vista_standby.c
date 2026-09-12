/* ------------------------------------------------------------------------
 * La vista di standby — 01-specifica-ui.md §4.7.
 *
 * Due numeri della stessa taglia, l'ora e la temperatura della stanza,
 * separati da un filo; sotto, una fascia con quattro dati.
 *
 * Perche' due numeri e non uno: il pannello sta al posto di un termostato,
 * e la temperatura della stanza e la ragione per cui qualcuno alza gli
 * occhi verso quel punto del muro. Dargliela piu piccola dell'ora
 * significherebbe averlo montato li per abitudine.
 *
 * **Qui dentro non si inventa niente.** Ogni pezzo compare se il suo dato
 * c'e e sparisce se non c'e, invece di mostrare un trattino: un trattino in
 * mezzo ad altri numeri sembra un valore, e su una schermata che si guarda
 * di sfuggita passando in corridoio nessuno andrebbe a verificare. Le
 * aperture seguono la stessa regola per una ragione in piu: a casa chiusa
 * quel posto deve stare zitto, cosi quando parla si nota.
 *
 * Le etichette si tengono e si riscrivono una volta al secondo invece di
 * ricostruire la vista. Un ridisegno pieno su questo schermo si vede — e
 * qui si vedrebbe piu che altrove, perche' e l'unica schermata che nessuno
 * sta toccando mentre cambia.
 * --------------------------------------------------------------------- */
#include "vista_standby.h"

#include <string.h>

#include "comuni.h"
#include "config.h"
#include "dati.h"

#include "orologio.h"
#include "theme.h"
#include "ui.h"

/* Una voce della fascia in fondo. Si tengono i tre pezzi che cambiano:
   l'icona — il meteo la cambia col tempo — il valore e l'occhiello. */
typedef struct {
    lv_obj_t *blocco, *icona, *valore, *nome;
} voce_t;

/* Tutto quello che si riscrive o si nasconde. Nasce e muore in questo file,
   e nessun altro puo portarselo via: il velo che lo contiene lo distrugge
   tempi.c, che subito dopo chiama vista_standby_dimentica(). */
/* Quante pastiglie di presenza si costruiscono. Il numero delle persone lo
   dice la configurazione e non cambia mentre lo standby e aperto, ma un
   documento scritto a mano potrebbe elencarne venti: oltre questo tetto la
   riga non si legge piu e il resto si scarta. */
#define PERSONE_MAX 6

static struct {
    lv_obj_t *ora, *data;
    lv_obj_t *persone;
    lv_obj_t *persona[PERSONE_MAX], *persona_testo[PERSONE_MAX];
    int       persone_n;
    lv_obj_t *temp, *stanza, *blocco_temp, *filo, *fuoco;
    lv_obj_t *scalda;
    lv_obj_t *chiesta, *chiesta_testo;
    lv_obj_t *apertura, *apertura_testo;
    voce_t    fuori, sole, casa, batteria;
} v;

/* Mostra o nasconde. Piu corto della condizione scritta a mano, e
   soprattutto dice a chi legge che quello e un pezzo che puo non esserci. */
static void mostra(lv_obj_t *o, bool si)
{
    if (!o) return;
    if (si) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

/* --- quanto grandi, e perche' sono gradini e non un numero libero -------
 *
 * I caratteri di questo progetto sono **bitmap generate quando si compila**,
 * da profili.json: un corpo che nessuno ha generato non esiste sul pannello.
 * Si sceglie quindi fra cinque gradini che ci sono davvero — sul p4 in
 * verticale 60, 84, 104, 126 e 150 pixel — e ognuno ha l'insieme di
 * caratteri dell'orologio.
 *
 * La scala precedente era a tre, e prendeva in prestito ruoli fatti per
 * altro. Da li venivano due difetti in uno: i gradini non erano una scala —
 * erano le taglie che capitavano — e FT_XXL, tagliato per i numeri
 * dell'energia, **non ha lettere**: sotto la temperatura compariva un
 * rettangolo vuoto al posto della C.
 *
 * Il valore di riposo e il gradino 4 per l'ora e il 2 per la temperatura,
 * cioe esattamente le taglie di prima: una configurazione che non dice
 * niente non deve cambiare quello che si vede. */
static font_ruolo_t gradino(const char *percorso, int riposo)
{
    const int32_t v = cfg_intero(percorso, riposo);
    static const font_ruolo_t SCALA[5] = {
        FT_STANDBY_1, FT_STANDBY_2, FT_STANDBY_3, FT_STANDBY_4, FT_STANDBY_5,
    };
    const int n = v < 1 ? 1 : v > 5 ? 5 : (int)v;
    return SCALA[n - 1];
}

/* --- i pezzi ------------------------------------------------------------ */

static voce_t voce(lv_obj_t *padre, const char *ico, lv_color_t colore,
                   const char *nome)
{
    voce_t x = { 0 };

    x.blocco = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(x.blocco, LV_OPA_TRANSP, 0);
    lv_obj_set_size(x.blocco, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(x.blocco, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(x.blocco, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(x.blocco, COM.pastiglia_gap, 0);

    x.icona = ui_icona(x.blocco, ico, colore, IC_L);

    lv_obj_t *d = ui_pannello(x.blocco, C_BG);
    lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, 0);
    lv_obj_set_size(d, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(d, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(d, COM.gap_stretto, 0);

    x.valore = ui_testo(d, "—", C_TXT, FT_L);
    x.nome   = ui_occhiello(d, nome);
    return x;
}

/* Una pastiglia: bordo sottile e testo piccolo. `testo_fuori` riceve
   l'etichetta interna, per chi deve riscriverla.

   `grande` la porta alla taglia grande, icona compresa. Serve a chi c'e in
   casa: e l'unica riga di questa schermata che qualcuno legge davvero da
   lontano, e alla taglia dell'occhiello non si leggeva. Provata prima a
   FT_M, che era gia meglio ma ancora piccola per due metri di distanza su
   uno schermo attenuato: sotto l'orario, che qui e enorme, un nome a FT_M
   sembra una didascalia. */
static lv_obj_t *pastiglia(lv_obj_t *padre, const char *ico, lv_color_t colore,
                           const char *testo, lv_obj_t **testo_fuori,
                           bool grande)
{
    lv_obj_t *p = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_size(p, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(p, PRF->tocco.pager.h, 0);
    ui_bordo(p, LV_BORDER_SIDE_FULL, colore);
    lv_obj_set_style_pad_hor(p, COM.pastiglia_pad_h, 0);
    lv_obj_set_style_pad_ver(p, COM.pastiglia_pad_v, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(p, COM.pastiglia_gap, 0);

    if (ico) ui_icona(p, ico, colore, grande ? IC_L : IC_S);
    lv_obj_t *l = grande ? ui_testo(p, testo, colore, FT_L)
                         : ui_occhiello(p, testo);
    lv_obj_set_style_text_color(l, colore, 0);
    if (testo_fuori) *testo_fuori = l;
    return p;
}

/* --- la costruzione ----------------------------------------------------- */

void vista_standby_costruisci(lv_obj_t *padre)
{
    const bool verticale = PRF->orientamento == VERTICALE;

    /* --- chi c'e in casa, in cima ---------------------------------------
     *
     * Prima dell'ora, e non sotto: entrando in casa la domanda «chi c'e» si
     * fa prima di quella sull'ora, e questa riga stava in fondo, piccola e
     * grigia. Su uno schermo al dodici per cento di luminosita il grigio su
     * nero non e poco leggibile: e invisibile da due metri, che e la
     * distanza da cui si guarda un pannello in corridoio. Adesso porta il
     * colore d'accento del tema e la taglia del testo normale.
     *
     * Una pastiglia per persona, e si costruiscono tutte adesso: la
     * configurazione non cambia mentre lo standby e aperto, e crearle e
     * distruggerle a ogni battito vorrebbe dire un ridisegno al secondo
     * sull'unica schermata che nessuno sta toccando mentre cambia.
     *
     * Quali si vedono lo decide l'aggiornamento. */
    v.persone = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(v.persone, LV_OPA_TRANSP, 0);
    lv_obj_set_width(v.persone, LV_PCT(100));
    lv_obj_set_height(v.persone, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(v.persone, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(v.persone, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(v.persone, COM.pastiglia_gap, 0);
    lv_obj_set_style_pad_row(v.persone, COM.pastiglia_gap, 0);

    v.persone_n = dati_persone();
    if (v.persone_n > PERSONE_MAX) v.persone_n = PERSONE_MAX;
    for (int n = 0; n < v.persone_n; n++)
        v.persona[n] = pastiglia(v.persone, ICO_PERSON, C_ACC, "—",
                                 &v.persona_testo[n], true);

    /* La parte alta: i due numeri. In orizzontale affiancati con un filo in
       mezzo; in verticale impilati, perche' due numeri di questa taglia su
       seicento pixel non ci stanno in riga (09-profili.md §1-ter). */
    lv_obj_t *alto = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(alto, LV_OPA_TRANSP, 0);
    lv_obj_set_width(alto, LV_PCT(100));
    lv_obj_set_flex_grow(alto, 1);
    lv_obj_set_flex_flow(alto, verticale ? LV_FLEX_FLOW_COLUMN
                                         : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(alto, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(alto, PRF->geo.pad, 0);
    lv_obj_set_style_pad_row(alto, PRF->geo.pad, 0);

    /* --- l'ora ---------------------------------------------------------- */
    lv_obj_t *col_ora = ui_pannello(alto, C_BG);
    lv_obj_set_style_bg_opa(col_ora, LV_OPA_TRANSP, 0);
    lv_obj_set_height(col_ora, LV_SIZE_CONTENT);
    if (verticale) lv_obj_set_width(col_ora, LV_SIZE_CONTENT);
    else           lv_obj_set_flex_grow(col_ora, 1);
    lv_obj_set_flex_flow(col_ora, LV_FLEX_FLOW_COLUMN);
    /* Tre argomenti e tutti e tre contano. Il primo e l'asse principale,
       che in una colonna e il verticale. Il secondo e il trasversale: dove
       stanno i figli dentro la loro traccia. Il **terzo** e dove sta la
       traccia dentro il contenitore, ed e quello che serve qui: con il
       terzo al centro la traccia — larga quanto il figlio piu largo — si
       centrava nella colonna, e i due numeri restavano in mezzo alle loro
       meta invece di andare ai bordi. Si vedeva come un margine
       sbagliato. */
    const lv_flex_align_t lato_ora = verticale ? LV_FLEX_ALIGN_CENTER
                                               : LV_FLEX_ALIGN_START;
    lv_obj_set_flex_align(col_ora, LV_FLEX_ALIGN_CENTER, lato_ora, lato_ora);
    lv_obj_set_style_pad_row(col_ora, COM.pastiglia_gap, 0);

    v.ora  = ui_testo(col_ora, "--:--", C_TXT,
                      gradino("display/standby/clock", 4));
    v.data = ui_testo(col_ora, tr(TX_STANDBY_WAITING_TIME), C_DIM, FT_M);

    /* L'apertura sta sotto l'ora e non nella fascia: e l'unica cosa qui che
       dica qualcosa di urgente, e in mezzo ai numeri dell'energia si
       leggerebbe come un numero fra gli altri. */
    v.apertura = pastiglia(col_ora, ICO_SENSOR_WINDOW, C_ACC, "—",
                           &v.apertura_testo, false);
    lv_obj_set_style_margin_top(v.apertura, COM.pastiglia_gap, 0);

    /* --- il filo fra i due ---------------------------------------------- */
    v.filo = ui_pannello(alto, C_LINE);
    if (verticale) lv_obj_set_size(v.filo, LV_PCT(64), COM.bordo);
    else           lv_obj_set_size(v.filo, COM.bordo, LV_PCT(64));

    /* --- la temperatura della stanza ------------------------------------ */
    v.blocco_temp = ui_pannello(alto, C_BG);
    lv_obj_set_style_bg_opa(v.blocco_temp, LV_OPA_TRANSP, 0);
    lv_obj_set_height(v.blocco_temp, LV_SIZE_CONTENT);
    if (verticale) lv_obj_set_width(v.blocco_temp, LV_SIZE_CONTENT);
    else           lv_obj_set_flex_grow(v.blocco_temp, 1);
    lv_obj_set_flex_flow(v.blocco_temp, LV_FLEX_FLOW_COLUMN);
    const lv_flex_align_t lato_temp = verticale ? LV_FLEX_ALIGN_CENTER
                                                : LV_FLEX_ALIGN_END;
    lv_obj_set_flex_align(v.blocco_temp, LV_FLEX_ALIGN_CENTER,
                          lato_temp, lato_temp);
    lv_obj_set_style_pad_row(v.blocco_temp, COM.pastiglia_gap, 0);

    /* Un corpo suo, piu piccolo dell'ora di circa un quinto. I due numeri
       erano della stessa taglia, ed era una scelta motivata — il pannello
       sta al posto di un termostato, e dare alla temperatura un corpo minore
       avrebbe detto che si guarda l'orologio. Sul muro pero due numeri
       identici si contendono l'occhio e non vince nessuno: adesso c'e un
       ordine di lettura, e la temperatura resta grande abbastanza da non
       essere retrocessa a dato secondario. */
    /* L'icona del riscaldamento sta **accanto** al numero, non sotto fra le
       pastiglie: quelle si leggono da vicino, questa deve arrivare da tre
       metri. Percio il corpo e IC_XL — lo stesso della lampadina nelle
       Luci — e non IC_S come nelle pastiglie.
       Il numero e l'icona sono un gruppo centrato: quando il riscaldamento
       si accende, il numero si sposta di mezza icona. Succede due volte al
       giorno su una schermata che nessuno sta fissando, e costa meno di uno
       spazio vuoto tenuto sempre da parte per un'icona che quasi sempre non
       c'e. */
    lv_obj_t *riga_temp = ui_pannello(v.blocco_temp, C_BG);
    lv_obj_set_style_bg_opa(riga_temp, LV_OPA_TRANSP, 0);
    lv_obj_set_size(riga_temp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga_temp, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(riga_temp, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(riga_temp, PRF->geo.gap, 0);

    /* Onde di calore e non una fiamma, e non e un vezzo: due righe piu in
       giu c'e la pastiglia «sta scaldando» con la sua fiamma, che dice
       un'altra cosa. Due fiamme quasi uguali a venti pixel di distanza
       renderebbero illeggibili tutti e due i significati. */
    v.fuoco = ui_icona(riga_temp, ICO_HEAT, C_CALDO, IC_XL);

    v.temp = ui_testo(riga_temp, "—", C_TXT,
                      gradino("display/standby/temperature", 2));

    lv_obj_t *pastiglie = ui_pannello(v.blocco_temp, C_BG);
    lv_obj_set_style_bg_opa(pastiglie, LV_OPA_TRANSP, 0);
    lv_obj_set_size(pastiglie, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pastiglie, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pastiglie, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pastiglie, PRF->geo.gap, 0);

    /* «Sta scaldando»: e' l'impianto, non l'interfaccia. Stessa ambra
       dell'icona del riscaldamento e della zona che chiama. */
    v.scalda  = pastiglia(pastiglie, ICO_LOCAL_FIRE_DEPARTMENT, C_CALDO,
                          tr(TX_STANDBY_HEATING), NULL, false);
    v.chiesta = pastiglia(pastiglie, NULL, C_DIM, "—", &v.chiesta_testo,
                          false);

    v.stanza = ui_occhiello(v.blocco_temp, tr(TX_STANDBY_INDOORS));

    /* --- la fascia in fondo ---------------------------------------------
     *
     * Quattro dati. Le colonne le dice il profilo e non l'orientamento
     * letto qui: quattro in riga in orizzontale, due per due in verticale.
     * E una misura di layout come le altre, e sta dove stanno le altre. */
    lv_obj_t *fascia = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(fascia, LV_OPA_TRANSP, 0);
    lv_obj_set_width(fascia, LV_PCT(100));
    lv_obj_set_height(fascia, LV_SIZE_CONTENT);
    lv_obj_set_style_margin_top(fascia, PRF->geo.pad, 0);
    lv_obj_set_style_pad_row(fascia, PRF->geo.pad, 0);
    /* A capo e non a griglia, e la differenza si vede il giorno che un dato
       manca: una cella di griglia vuota resta li a occupare la sua colonna,
       e sul vetro si legge come un buco. Cosi invece i tre che restano si
       ridistribuiscono, e nessuno si accorge che erano quattro. */
    lv_obj_set_flex_flow(fascia, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(fascia, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    v.fuori    = voce(fascia, ICO_SUNNY,
                      C_METEO_SOLE, tr(TX_STANDBY_OUTSIDE));
    v.sole     = voce(fascia, ICO_SOLAR_POWER,   C_ACC, tr(TX_ENERGY_FROM_SUN));
    v.casa     = voce(fascia, ICO_HOME,          C_DIM, tr(TX_STANDBY_HOUSE));
    v.batteria = voce(fascia, ICO_BATTERY_FULL,  C_OK,  tr(TX_STANDBY_BATTERY));

    /* Quanti per riga lo dice il profilo: quattro in orizzontale, due in
       verticale. La larghezza percentuale e cio che manda a capo, e lascia
       spazio alle spaziature — con il cento per cento tondo l'ultimo
       scenderebbe da solo. */
    voce_t *F[4] = { &v.fuori, &v.sole, &v.casa, &v.batteria };
    const int32_t quota = 92 / PRF->griglia.standby_col;
    for (int n = 0; n < 4; n++)
        lv_obj_set_width(F[n]->blocco, LV_PCT(quota));

    /* Subito, non al prossimo battito: un secondo di trattini all'ingresso
       nello standby si nota, perche' e il momento in cui si guarda. */
    vista_standby_aggiorna();
}

/* --- l'aggiornamento ---------------------------------------------------- */

void vista_standby_aggiorna(void)
{
    if (!v.ora) return;

    char ora[8], data[40];
    orologio_ora(ora, sizeof ora);
    orologio_data(data, sizeof data);
    ui_scrivi(v.ora, ora);
    ui_scrivi(v.data, data[0] ? data : tr(TX_STANDBY_WAITING_TIME));

    const standby_t s = dati_standby();
    static char b[6][40];

    /* --- la temperatura della stanza ------------------------------------ */
    mostra(v.blocco_temp, s.interna_c_e);
    mostra(v.filo, s.interna_c_e);
    if (s.interna_c_e) {
        lv_snprintf(b[0], sizeof b[0], "%s°C", ui_temp(s.interna));
        ui_scrivi(v.temp, b[0]);
        ui_scrivi(v.stanza, s.stanza);
    }

    mostra(v.chiesta, s.chiesta_c_e);
    if (s.chiesta_c_e) {
        lv_snprintf(b[1], sizeof b[1], tr(TX_STANDBY_REQUESTED), ui_temp(s.chiesta));
        ui_scrivi(v.chiesta_testo, b[1]);
    }
    mostra(v.scalda, s.scalda);
    mostra(v.fuoco, s.riscaldamento_acceso);

    /* --- chi c'e in casa -------------------------------------------------
     *
     * Solo chi c'e. Chi e fuori non compare: una pastiglia spenta col nome
     * di qualcuno direbbe a chiunque passi che quella persona non e in casa,
     * e questo schermo sta in corridoio.
     *
     * Se la presenza non e nota — nessuna entita `person` configurata, o
     * Home Assistant non ancora collegato — la riga sparisce invece di
     * mostrarsi vuota: 11-collaudo.md §2, quello che non si sa non si
     * disegna. */
    int in_casa = 0;
    if (dati_presenza_nota()) {
        for (int n = 0; n < v.persone_n; n++) {
            const persona_t *p = dati_persona(n);
            const bool c_e = p && p->in_casa;
            mostra(v.persona[n], c_e);
            if (c_e) {
                ui_scrivi(v.persona_testo[n], p->nome ? p->nome : "—");
                in_casa++;
            }
        }
    } else {
        for (int n = 0; n < v.persone_n; n++) mostra(v.persona[n], false);
    }
    mostra(v.persone, in_casa > 0);

    /* --- l'apertura ------------------------------------------------------ */
    mostra(v.apertura, s.aperture > 0);
    if (s.aperture > 0) {
        if (s.aperture == 1 && s.apertura_nome[0])
            lv_snprintf(b[2], sizeof b[2], tr(TX_STANDBY_ONE_OPEN), s.apertura_nome);
        else
            lv_snprintf(b[2], sizeof b[2], trn(TXN_STANDBY_OPEN_COUNT, s.aperture),
                        s.aperture);
        ui_scrivi(v.apertura_testo, b[2]);
    }

    /* --- la fascia -------------------------------------------------------
     *
     * L'icona del meteo cambia col tempo, quindi si riscrive come i numeri.
     * La condizione va accanto al nome — "fuori · sereno" — e non al posto
     * del numero: quel numero viene dal sensore sul balcone, la parola dal
     * servizio meteo, e se il servizio tace il numero resta buono. */
    mostra(v.fuori.blocco, s.esterna_c_e);
    if (s.esterna_c_e) {
        lv_snprintf(b[3], sizeof b[3], "%s°", ui_temp(s.esterna));
        ui_scrivi(v.fuori.valore, b[3]);
        ui_scrivi(v.fuori.icona, ui_meteo_icona(s.condizione));
        /* Il colore va rimesso insieme al glifo: la voce nasce col sole, e
           senza questa riga una giornata di pioggia avrebbe la nuvola
           gialla — che e peggio della nuvola ambra di prima, perche' li
           almeno era ambra tutto. */
        lv_obj_set_style_text_color(v.fuori.icona,
                                    ui_meteo_colore(s.condizione), 0);

        const char *c = ui_meteo_nome(s.condizione);
        if (c && *c) {
            lv_snprintf(b[4], sizeof b[4], tr(TX_STANDBY_OUTSIDE_WITH), c);
            ui_scrivi(v.fuori.nome, b[4]);
        } else {
            ui_scrivi(v.fuori.nome, tr(TX_STANDBY_OUTSIDE));
        }
    }

    mostra(v.sole.blocco, s.energia_c_e);
    mostra(v.casa.blocco, s.energia_c_e);
    if (s.energia_c_e) {
        ui_scrivi(v.sole.valore, ui_potenza(s.sole_w));
        ui_scrivi(v.casa.valore, ui_potenza(s.casa_w));
    }

    mostra(v.batteria.blocco, s.batteria_c_e);
    if (s.batteria_c_e) {
        lv_snprintf(b[5], sizeof b[5], "%u%%", (unsigned)s.batteria_pct);
        ui_scrivi(v.batteria.valore, b[5]);
    }
}

void vista_standby_dimentica(void)
{
    memset(&v, 0, sizeof v);
}
