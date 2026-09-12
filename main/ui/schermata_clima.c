/* ------------------------------------------------------------------------
 * Clima — 01-specifica-ui.md §3.2.
 *
 * L'impianto e fatto di **due cose diverse** e la sezione le tiene separate:
 * dodici zone di riscaldamento, che hanno solo il setpoint, e quattro
 * condizionatori Gree, che hanno modalita, ventilazione, deflettore e timer.
 * Non condividono nemmeno la scala delle temperature — riscaldamento 15-26
 * a mezzo grado, condizionatori 16-30 a grado intero, come accettano le
 * unita.
 *
 * La fascia superiore non contiene modalita globali: nell'impianto non
 * esistono, e inventarle sarebbe peggio che non averle.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "ui.h"
#include "widgets/paginatore.h"
#include "widgets/interruttore.h"
#include "widgets/posto_vuoto.h"

/* Le due viste della sezione, che sono i due impianti. */
typedef enum { GRUPPO_RISCALDAMENTO = 0, GRUPPO_CONDIZIONATORI } gruppo_t;

static int pagina;

static gruppo_t gruppo(void) { return (gruppo_t)ui_vista(); }

static void su_dettaglio(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    ui_vai_a(SEZ_CLIMA, schermata_clima_vista_dettaglio(n));
}

static void su_gruppo(lv_event_t *e)
{
    pagina = 0;
    ui_vai_a(SEZ_CLIMA, (int)(intptr_t)lv_event_get_user_data(e));
}

static void su_pagina(int p) { pagina = p; ui_vai_a(SEZ_CLIMA, ui_vista()); }

/* La pagina dell'elenco, per le catture. `--pagina` la sapeva impostare solo
   sul dettaglio del condizionatore: la seconda pagina del riscaldamento —
   quella dove sta il piano del box, spento, con le sue zone attenuate — non
   si poteva fotografare, ed e proprio quella che si vuole guardare. */
void schermata_clima_pagina(int p) { pagina = p < 0 ? 0 : p; }

/* Il colore segue la modalita: azzurro in raffrescamento, ambra in
   riscaldamento, niente accento da spento.

   Tutti e due fissi, e da qui in avanti anche l'ambra. Prima era l'accento,
   e la riga qui sopra diceva gia «ambra» — cioe descriveva una coincidenza:
   l'accento era ambra. Con un accento verde il riscaldamento diventava
   verde, che e il contrario di quello che questa riga vuole dire. */
static lv_color_t colore_modo(modo_clima_t m)
{
    switch (m) {
    case MODO_FREDDO:      return C_COOL;
    case MODO_DEUMIDIFICA: return C_COOL;
    case MODO_CALDO:       return C_CALDO;
    case MODO_SPENTO:      return C_DIM;
    default:               return C_TXT;
    }
}

static const char *icona_modo(modo_clima_t m)
{
    switch (m) {
    case MODO_CALDO:        return ICO_LOCAL_FIRE_DEPARTMENT;
    case MODO_FREDDO:       return ICO_AC_UNIT;
    case MODO_AUTO:         return ICO_AUTORENEW;
    case MODO_DEUMIDIFICA:  return ICO_WATER_DROP;
    case MODO_VENTILATORE:  return ICO_MODE_FAN;
    default:                return ICO_POWER_SETTINGS_NEW;
    }
}

/* --- tasti meno e piu --------------------------------------------------- */

/* Un tasto oltre il limite non risponde e si vede che non risponde: non
   avvolge al capo opposto (11-collaudo.md §2). */
static lv_obj_t *tasto_passo(lv_obj_t *padre, const char *ico, bool vivo,
                             misura_t m, icona_corpo_t corpo)
{
    lv_obj_t *b = ui_pannello(padre, C_CARD2);
    lv_obj_set_size(b, m.w, m.h);
    lv_obj_set_style_radius(b, PRF->geo.radius_btn, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_tocco_minimo(b, m.w, m.h);

    ui_icona(b, ico, vivo ? C_TXT : C_OFF, corpo);
    if (vivo) lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    return b;
}

/* --- dal tocco all'impianto ---------------------------------------------
 *
 * Il dato dell'evento porta chi e cosa in un intero solo: l'indice nei bit
 * bassi, il verso nel bit alto. Un puntatore a una struttura vorrebbe dire
 * una struttura che sopravvive alla scheda che l'ha creata, e le schede qui
 * si distruggono a ogni ricostruzione.
 *
 * Dopo il comando si ricostruisce: lo stato vero arrivera da Home Assistant
 * col prossimo evento, ma il numero deve muoversi subito o il tasto sembra
 * non aver fatto niente. */
#define PASSO_SU 0x1000

static void rifai_clima(void)
{
    dati_ricarica();
    ui_vai_a(ui_dove(), ui_vista());
}

static void su_passo_zona(lv_event_t *e)
{
    const int d = (int)(intptr_t)lv_event_get_user_data(e);
    const int n = d & 0xFFF;
    const zona_clima_t *z = dati_zona_clima(n);
    if (!z) return;

    const limiti_t l = dati_limiti_riscaldamento();
    dati_zona_clima_imposta(n, (int16_t)(z->richiesta +
                                         (d & PASSO_SU ? l.passo : -l.passo)));
    rifai_clima();
}

static void su_passo_unita(lv_event_t *e)
{
    const int d = (int)(intptr_t)lv_event_get_user_data(e);
    const int n = d & 0xFFF;
    const condizionatore_t *u = dati_condizionatore(n);
    if (!u) return;

    const limiti_t l = dati_limiti_condizionatori();
    dati_condizionatore_imposta(n, (int16_t)(u->richiesta +
                                             (d & PASSO_SU ? l.passo : -l.passo)));
    rifai_clima();
}

/* --- fascia superiore --------------------------------------------------- */

static void scheda_gruppo(lv_obj_t *padre, gruppo_t g, const char *ico,
                          const char *nome, const char *conteggio)
{
    const bool scelto = g == gruppo();

    lv_obj_t *b = ui_pannello(padre, scelto ? C_ACC : C_CARD2);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, LV_PCT(100));
    lv_obj_set_style_min_width(b, 0, 0);
    lv_obj_set_style_radius(b, PRF->geo.radius_tile, 0);
    ui_bordo(b, LV_BORDER_SIDE_FULL, scelto ? C_ACC : C_LINE);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, PRF->geo.gap, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, su_gruppo, LV_EVENT_CLICKED, (void *)(intptr_t)g);

    ui_icona(b, ico, scelto ? C_INK : C_DIM, IC_S);
    ui_testo(b, nome, scelto ? C_INK : C_TXT, FT_M);
    /* In verticale il conteggio non ci sta: su 600 px di larghezza due
       pulsanti e il riquadro dell'esterno si pestano i piedi, e il mockup
       verticale infatti li tiene su due righe e senza numeri. */
    if (conteggio) ui_testo(b, conteggio, scelto ? C_INK : C_DIM, FT_S);
}

static void fascia(lv_obj_t *c)
{
    const bool vert = PRF->orientamento == VERTICALE;

    lv_obj_t *f = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, PRF->tocco.apertura.h);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    static char zone[16], unita[16];
    lv_snprintf(zone, sizeof zone, trn(TXN_CLIMATE_ZONES, dati_zone_clima()),
                dati_zone_clima());
    lv_snprintf(unita, sizeof unita, "%d", dati_condizionatori());

    scheda_gruppo(f, GRUPPO_RISCALDAMENTO, ICO_LOCAL_FIRE_DEPARTMENT,
                  tr(TX_CLIMATE_HEATING), vert ? NULL : zone);
    scheda_gruppo(f, GRUPPO_CONDIZIONATORI, ICO_AC_UNIT,
                  tr(TX_CLIMATE_AIR_CONDITIONERS), vert ? NULL : unita);

    /* Temperatura esterna in un riquadro suo: non e un comando e non deve
       sembrarlo. In orizzontale sta a destra dei due gruppi, in verticale
       su una riga tutta sua. */
    lv_obj_t *e = ui_pannello(vert ? c : f, C_CARD2);
    if (vert) {
        lv_obj_set_width(e, LV_PCT(100));
        lv_obj_set_height(e, PRF->tocco.pager.h);
    } else {
        lv_obj_set_height(e, LV_PCT(100));
        lv_obj_set_width(e, LV_SIZE_CONTENT);
    }
    lv_obj_set_style_radius(e, PRF->geo.radius_tile, 0);
    ui_bordo(e, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_style_pad_hor(e, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(e, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(e, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(e, PRF->geo.gap, 0);

    static char fuori[24];
    lv_snprintf(fuori, sizeof fuori, tr(TX_CLIMATE_OUTDOOR),
                ui_temp(dati_temperatura_esterna()));
    ui_icona(e, ICO_SUNNY, C_DIM, IC_S);
    ui_testo(e, fuori, C_TXT, FT_M);
}

/* --- gruppo riscaldamento ------------------------------------------------
 *
 * La scheda di una zona sta in **due righe**: nome e umidita sopra, e sotto
 * la temperatura misurata accanto ai comandi del setpoint.
 *
 * Ne occupava tre, alte piu del doppio, e la terza riga di contenuto non
 * c'era: erano tre blocchi distribuiti con SPACE_BETWEEN sull'altezza della
 * cella, cioe due terzi di vuoto. Su un pannello verticale questo costava
 * **una pagina in piu** — sei zone per volta invece di dodici — e con le
 * zone divise per piano voleva dire che per sapere se il piano di sotto
 * stava scaldando bisognava toccare il vetro. Un dato che si guarda di
 * sfuggita e che chiede un tocco per comparire e un dato che non si guarda.
 *
 * Non e stato tolto niente di quello che si leggeva: nome, umidita,
 * temperatura misurata nel corpo piu grande, setpoint, il perche a parole e
 * i due tasti. E cambiato solo il posto della misurata, che da riga sua e
 * passata accanto ai comandi — dove c'era spazio vuoto. */
static void zona(lv_obj_t *g, const zona_clima_t *z, int col, int rig)
{
    lv_obj_t *k = ui_scheda(g);
    lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, col, 1,
                            LV_GRID_ALIGN_STRETCH, rig, 1);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    /* Il contenuto sta insieme al centro invece di spingersi ai due bordi:
       con schede basse SPACE_BETWEEN spalancherebbe di nuovo lo spazio che
       si e appena tolto, e la scheda tornerebbe a sembrare mezza vuota. */
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    if (!z) { posto_vuoto(k); return; }

    /* Il piano di questa zona, se ne ha uno e se e spento: una valvola che
       chiede calore mentre l'interruttore del suo piano e spento **non
       scalda niente**, e disegnarla come le altre farebbe credere il
       contrario. Si attenua tutta, e sotto il setpoint lo dice a parole. */
    const piano_t *pi = dati_piano(z->piano);
    const bool piano_spento = pi && pi->disponibile && !pi->acceso;
    if (piano_spento)
        lv_obj_set_style_opa(k, ui_opa(COM.opacita_condizionatore_spento_pct), 0);

    /* Bordo e valore ambra quando la zona chiama davvero calore: e l'unica
       informazione che dice se l'impianto sta lavorando adesso. Rafforzato
       con un fondo appena tinto — era il solo bordo, e da un metro e mezzo
       un bordo di un pixel non si vede. */
    if (z->chiama && !piano_spento) {
        ui_bordo(k, LV_BORDER_SIDE_FULL, C_SEL_LINE);
        lv_obj_set_style_bg_color(k, C_SEL_BG, 0);
    }

    const bool nota = z->disponibile && z->misurata != TEMP_IGNOTA;

    /* I contenitori qui sotto sono impaginazione e basta: sfondo
       trasparente, non del colore della scheda. Erano C_CARD, cioe lo stesso
       colore, e finche la scheda era di quel colore non si vedevano — ma
       tingendo la scheda quando la zona chiama calore sono comparsi dei
       rettangoli scuri dentro. Un contenitore che ripete il colore del padre
       aspetta solo che il padre cambi colore. */

    /* prima riga: nome e umidita */
    lv_obj_t *alto = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(alto, LV_OPA_TRANSP, 0);
    lv_obj_set_width(alto, LV_PCT(100));
    lv_obj_set_height(alto, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alto, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(alto, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(alto, PRF->geo.gap, 0);

    lv_obj_t *nome = ui_testo(alto, z->nome, z->accesa ? C_TXT : C_DIM, FT_M);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(nome, 1);

    if (z->umidita) {
        static char um[12];
        lv_snprintf(um, sizeof um, tr(TX_CLIMATE_HUMIDITY), z->umidita);
        ui_testo(alto, um, C_DIM, FT_S);
    }

    /* seconda riga: la misurata a sinistra, i comandi a destra */
    lv_obj_t *riga = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
    lv_obj_set_width(riga, LV_PCT(100));
    lv_obj_set_height(riga, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(riga, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(riga, PRF->geo.gap, 0);

    static char misurata[12];
    lv_snprintf(misurata, sizeof misurata, "%s°", ui_temp(z->misurata));
    ui_testo(riga, misurata,
             !nota ? C_DIM : (z->chiama ? C_CALDO : (z->accesa ? C_TXT : C_DIM)),
             FT_XL);

    if (!z->disponibile) {
        ui_testo(riga, tr(TX_COMMON_UNAVAILABLE), C_DIM, FT_S);
        return;
    }

    /* i comandi del setpoint: meno, valore richiesto, piu */
    const limiti_t lim = dati_limiti_riscaldamento();
    lv_obj_t *set = ui_pannello(riga, C_CARD);
    lv_obj_set_style_bg_opa(set, LV_OPA_TRANSP, 0);
    lv_obj_set_size(set, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(set, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(set, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(set, PRF->geo.gap, 0);

    lv_obj_add_event_cb(
        tasto_passo(set, ICO_REMOVE, nota && z->richiesta > lim.min,
                    PRF->tocco.clima_pm, IC_S),
        su_passo_zona, LV_EVENT_CLICKED, (void *)(intptr_t)z->indice);

    lv_obj_t *mezzo = ui_pannello(set, C_CARD);
    lv_obj_set_style_bg_opa(mezzo, LV_OPA_TRANSP, 0);
    lv_obj_set_size(mezzo, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mezzo, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mezzo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    static char richiesta[12];
    lv_snprintf(richiesta, sizeof richiesta, "%s°", ui_temp(z->richiesta));
    ui_testo(mezzo, richiesta, z->chiama ? C_CALDO : C_TXT, FT_M);
    /* Etichetta esplicita: chiedere calore e averlo raggiunto sono due cose
       diverse, e da spenta non c'e nessuna richiesta. */
    /* «Piano spento» viene prima di «nessuna richiesta», e l'ordine conta:
       sono tutte e due vere, ma la prima spiega la seconda e vale per tutte
       le zone del piano. Chi legge vuole sapere perche non scalda, e la
       risposta e l'interruttore, non il singolo setpoint. */
    ui_testo(mezzo, piano_spento ? tr(TX_CLIMATE_FLOOR_OFF)
                   : !z->accesa ? tr(TX_CLIMATE_NO_DEMAND)
                   : (z->chiama ? tr(TX_CLIMATE_CALLING) : tr(TX_CLIMATE_REACHED)),
             C_DIM, FT_S);

    lv_obj_add_event_cb(
        tasto_passo(set, ICO_ADD, nota && z->richiesta < lim.max,
                    PRF->tocco.clima_pm, IC_S),
        su_passo_zona, LV_EVENT_CLICKED,
        (void *)(intptr_t)(z->indice | PASSO_SU));
}

/* --- la fascia di un piano ----------------------------------------------
 *
 * Le valvole delle zone aprono il circuito; l'accensione vera la comanda
 * questo interruttore. Sono due cose e stanno separate: la fascia in cima al
 * gruppo, le zone sotto.
 *
 * Il bersaglio e tutta la fascia e non il solo interruttore, come nelle
 * righe della sezione Interruttori: 76 px contro 34, e l'interruttore resta
 * li a dire lo stato e la direzione senza prendersi il tocco. */
static void su_piano(lv_event_t *e)
{
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    const piano_t *p = dati_piano(n);
    if (!p) return;
    dati_piano_accendi(n, !p->acceso);
    rifai_clima();
}

static void fascia_piano(lv_obj_t *c, const piano_t *p, int indice)
{
    lv_obj_t *f = ui_scheda(c);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(f, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    if (!p->disponibile)
        lv_obj_set_style_opa(f, ui_opa(COM.opacita_dato_vecchio_pct), 0);

    ui_icona(f, ICO_HEAT, p->acceso ? C_CALDO : C_DIM, IC_M);

    lv_obj_t *col = ui_pannello(f, C_CARD);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *nome = ui_testo(col, p->nome && p->nome[0] ? p->nome : tr(TX_CLIMATE_FLOOR),
                              C_TXT, FT_M);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nome, LV_PCT(100));

    /* Lo stato in chiaro. «Non risponde» non e «spento»: un interruttore che
       Home Assistant non sa raggiungere non si finge spento, perche premerlo
       non farebbe niente e nessuno saprebbe perche. */
    static char stato[8][40];
    const int b = indice % 8;
    if (!p->disponibile)
        lv_snprintf(stato[b], sizeof stato[0], "%s", tr(TX_COMMON_NOT_RESPONDING));
    else if (!p->acceso)
        lv_snprintf(stato[b], sizeof stato[0], "%s", tr(TX_SWITCHES_OFF));
    else if (p->chiedono)
        lv_snprintf(stato[b], sizeof stato[0],
                    trn(TXN_CLIMATE_FLOOR_ON_CALLING, p->chiedono), p->chiedono);
    else
        lv_snprintf(stato[b], sizeof stato[0], "%s", tr(TX_CLIMATE_FLOOR_ON_NO_DEMAND));
    ui_testo(col, stato[b], p->acceso ? C_CALDO : C_DIM, FT_XS);

    lv_obj_t *sw = interruttore(f, p->acceso, p->disponibile);
    if (p->disponibile) {
        lv_obj_add_flag(f, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(f, su_piano, LV_EVENT_CLICKED,
                            (void *)(intptr_t)indice);
        lv_obj_remove_flag(sw, LV_OBJ_FLAG_CLICKABLE);
    }

    /* La contraddizione, e questa e l'unica cosa in tutto il riscaldamento
       che prima non si poteva sapere da nessuna parte del pannello: le zone
       chiedono calore e il piano e spento, quindi non scaldera niente. Si
       scopriva toccando i termosifoni. */
    if (p->disponibile && !p->acceso && p->chiedono)
        ui_testo(c, tr(TX_CLIMATE_FLOOR_OFF_CONFLICT),
                 C_WARN, FT_XS);
}

/* Quante zone di riscaldamento l'ordinamento per piano sa tenere. E lo
   stesso limite del fornitore (ZONE_MAX in dati_finti_clima.c): scriverlo
   qui vuol dire che i due possono divergere, ma l'alternativa era esporre un
   limite del fornitore nell'interfaccia. Se un giorno le zone superassero
   questo numero, le ultime resterebbero fuori dall'elenco — e la riga sotto
   e' quella da cambiare. */
#define ZONE_ORDINE_MAX 24

/* Dove finisce la pagina che comincia dalla zona `primo`.
 *
 * Il conto e' in **righe**, e prima era in zone: nel profilo c'era «quante
 * zone stanno in una pagina con le fasce», e una pagina di sei zone poteva
 * occupare tre righe o cinque secondo come i piani se le dividevano. Cinque
 * righe non ci stavano, e l'ultima scheda finiva mezza sotto il bordo — un
 * numero che si legge a meta' e' peggio di un numero che non c'e'.
 *
 * Una fascia costa una riga di bilancio anche se e' piu bassa di una riga di
 * schede: e un arrotondamento in eccesso, e in eccesso e' il verso giusto
 * per una misura che decide se qualcosa esce dal vetro.
 *
 * Un gruppo piu alto della pagina si spezza, e la pagina dopo ricomincia con
 * la sua fascia: e' la stessa fascia, e ripeterla e' l'unico modo perche' le
 * zone della seconda pagina dicano di che piano sono. */
static int fine_pagina(const int *ordine, int quante, int primo, int colonne,
                       int budget)
{
    int usate = 0, n = primo;

    while (n < quante) {
        const zona_clima_t *z = dati_zona_clima(ordine[n]);
        const int mio = z ? z->piano : -1;

        int fine = n;
        while (fine < quante) {
            const zona_clima_t *q = dati_zona_clima(ordine[fine]);
            if (!q || q->piano != mio) break;
            fine++;
        }

        const int resta = budget - usate - 1;   /* meno la fascia del gruppo */
        if (resta <= 0) break;

        int righe = (fine - n + colonne - 1) / colonne;
        if (righe > resta) {                    /* il gruppo si spezza */
            righe = resta;
            fine = n + righe * colonne;
            if (fine > quante) fine = quante;
        }

        usate += 1 + righe;
        n = fine;
    }

    /* Una pagina vuota non esiste: se non ci sta nemmeno una riga si mette
       lo stesso, se no l'impaginazione non avanza e il ciclo non finisce. */
    if (n == primo)
        n = primo + colonne < quante ? primo + colonne : quante;

    return n;
}

static void riscaldamento(lv_obj_t *c)
{
    const int quante = dati_zone_clima();
    const uint8_t colonne = PRF->griglia.clima_col;
    const int n_piani = dati_piani();

    /* Senza piani in configurazione niente cambia: un elenco solo, come
       prima. Il riscaldamento a piani e una cosa che si aggiunge, non una a
       cui bisogna adeguarsi. */
    if (!n_piani) {
        const int capienza = colonne * PRF->griglia.clima_rig;
        const int pagine = paginatore_pagine(quante, capienza);
        if (pagina >= pagine) pagina = 0;
        if (pagine > 1)
            paginatore_frecce(ui_testata_destra(), pagina, pagine, su_pagina);

        lv_obj_t *g = ui_griglia(c, colonne, PRF->griglia.clima_rig);
        lv_obj_set_width(g, LV_PCT(100));
        lv_obj_set_flex_grow(g, 1);
        for (int n = 0; n < capienza; n++)
            zona(g, dati_zona_clima(pagina * capienza + n),
                 n % colonne, n / colonne);
        paginatore_pallini(c, pagina, pagine);
        return;
    }

    /* --- con i piani: una fascia sopra le proprie zone -------------------
     *
     * Le zone si raggruppano **per piano**, e i piani vengono nell'ordine in
     * cui stanno in configurazione. Dentro un piano, le zone restano
     * nell'ordine del documento: spostare una zona la sposta, come ovunque
     * qui.
     *
     * Il primo tentativo raggruppava le zone **consecutive** dello stesso
     * piano, per non toccare l'ordine. Sembrava piu rispettoso e sul vetro
     * era peggio: in una configurazione vera le zone dei due piani si
     * alternano — l'elenco finisce con «Termostato piano terra» e
     * «Termostato primo piano» — e la pagina mostrava quattro fasce, con
     * «Piano terra» due volte. Una fascia ripetuta non e un ordine rispettato, e un elenco
     * che sembra rotto.
     *
     * Le zone senza piano vanno in fondo, in un gruppo loro: non spariscono,
     * perche una zona che si smette di vedere e peggio di una zona in un
     * gruppo sbagliato. */
    static int ordine[ZONE_ORDINE_MAX];
    int quante_ord = 0;
    for (int k = 0; k < n_piani && quante_ord < ZONE_ORDINE_MAX; k++)
        for (int n = 0; n < quante && quante_ord < ZONE_ORDINE_MAX; n++) {
            const zona_clima_t *z = dati_zona_clima(n);
            if (z && z->piano == k) ordine[quante_ord++] = n;
        }
    for (int n = 0; n < quante && quante_ord < ZONE_ORDINE_MAX; n++) {
        const zona_clima_t *z = dati_zona_clima(n);
        if (z && (z->piano < 0 || z->piano >= n_piani)) ordine[quante_ord++] = n;
    }

    /* Le pagine si scoprono percorrendo i gruppi: quante siano dipende da
       come i piani si dividono le zone, non da una divisione. */
    int inizi[ZONE_ORDINE_MAX + 1];
    int pagine = 0;
    for (int n = 0; n < quante_ord && pagine < ZONE_ORDINE_MAX; ) {
        inizi[pagine++] = n;
        n = fine_pagina(ordine, quante_ord, n, colonne,
                        PRF->clima.righe_con_fasce);
    }
    if (pagine == 0) pagine = 1, inizi[0] = 0;
    if (pagina >= pagine) pagina = 0;

    if (pagine > 1)
        paginatore_frecce(ui_testata_destra(), pagina, pagine, su_pagina);

    const int primo = inizi[pagina];
    const int ultimo = fine_pagina(ordine, quante_ord, primo, colonne,
                                   PRF->clima.righe_con_fasce);

    int n = primo;
    while (n < ultimo) {
        const zona_clima_t *z = dati_zona_clima(ordine[n]);
        const int mio = z ? z->piano : -1;

        const piano_t *pi = dati_piano(mio);
        if (pi) fascia_piano(c, pi, mio);
        else    ui_occhiello(c, tr(TX_CLIMATE_NO_FLOOR));

        int fine = n;
        while (fine < ultimo) {
            const zona_clima_t *q = dati_zona_clima(ordine[fine]);
            if (!q || q->piano != mio) break;
            fine++;
        }

        const int gruppo = fine - n;
        const int r = (gruppo + colonne - 1) / colonne;
        /* Cresce **in proporzione alle sue righe**, non a contenuto: le
           tracce di una griglia LVGL sono frazioni dell'altezza, e con
           SIZE_CONTENT non c'e niente da dividere — le schede collassavano
           una sull'altra. Cosi i gruppi si spartiscono lo spazio in parti
           proporzionali e le schede restano alte uguali da un gruppo
           all'altro. */
        lv_obj_t *g = ui_griglia(c, colonne, r);
        lv_obj_set_width(g, LV_PCT(100));
        lv_obj_set_flex_grow(g, r);

        for (int k = 0; k < gruppo; k++)
            zona(g, dati_zona_clima(ordine[n + k]), k % colonne, k / colonne);

        n = fine;
    }

    paginatore_pallini(c, pagina, pagine);
}

/* --- gruppo condizionatori ---------------------------------------------- */

static void riga_stato(lv_obj_t *padre, const char *ico, const char *nome,
                       const char *valore)
{
    lv_obj_t *r = ui_pannello(padre, C_CARD);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, PRF->geo.gap, 0);

    ui_icona(r, ico, C_DIM, IC_S);
    ui_testo(r, nome, C_DIM, FT_S);
    ui_spazio(r);
    lv_obj_t *v = ui_testo(r, valore, C_TXT, FT_S);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
}

/* --- la barra del conto alla rovescia ------------------------------------
 *
 * Sotto la riga del timer, nello spazio che la scheda aveva vuoto fra gli
 * stati e «Tutti i comandi». Dice la stessa cosa del numero accanto — quanto
 * manca — ma la dice in un colpo d'occhio, e da un metro e mezzo un colpo
 * d'occhio arriva prima di due cifre.
 *
 * **Cala**, non cresce: una barra che cresce racconterebbe quanto e passato,
 * che e un'altra domanda. Si muove al minuto, come il numero, perche il
 * pannello ricarica al cambio di minuto (01-specifica-ui.md §3.2) e
 * un'animazione al secondo costerebbe un ridisegno al secondo per un pixel.
 *
 * Senza `duration` del timer la quota e zero e **la barra non si disegna**:
 * resta il numero, che da solo non mente. Vale anche per l'ora di fine, che
 * si scrive solo se c'e. */
static void barra_timer(lv_obj_t *padre, const condizionatore_t *u)
{
    if (!u->timer_quota_pct && !u->spegne_alle) return;

    /* A timer che corre ma automazione spenta si mostra tutto attenuato:
       l'informazione resta leggibile — quel conteggio esiste davvero — ma
       smette di avere l'aria di una cosa che succedera. La riga sopra lo
       dice gia a parole con «senza effetto». */
    const bool vale = u->automazione_attiva;

    if (u->timer_quota_pct) {
        lv_obj_t *barra = ui_pannello(padre, C_BG);
        lv_obj_set_width(barra, LV_PCT(100));
        lv_obj_set_height(barra, COM.barretta_h);
        lv_obj_set_style_radius(barra, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(barra, 0, 0);
        if (!vale) lv_obj_set_style_opa(barra, ui_opa(COM.opacita_dato_vecchio_pct), 0);

        /* Del colore della modalita, non dell'accento fisso: la barra
           appartiene a questa scheda, e una striscia ambra dentro una scheda
           azzurra sembrerebbe arrivare da un'altra parte. */
        lv_obj_t *dentro = ui_pannello(barra, C_BG);
        lv_obj_set_style_bg_color(dentro, colore_modo(u->modo), 0);
        lv_obj_set_style_bg_opa(dentro, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dentro, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_height(dentro, LV_PCT(100));
        lv_obj_set_width(dentro, lv_pct(u->timer_quota_pct));
    }

    if (u->spegne_alle) {
        /* Buffer locale e non statico: lv_label_set_text() copia il testo
           dentro l'etichetta, quindi non deve sopravvivere a questa riga. */
        char fino[24];
        lv_snprintf(fino, sizeof fino, tr(TX_CLIMATE_UNTIL), u->spegne_alle);
        lv_obj_t *l = ui_testo(padre, fino, C_DIM, FT_XS);
        if (!vale) lv_obj_set_style_opa(l, ui_opa(COM.opacita_dato_vecchio_pct), 0);
    }
}

static void unita(lv_obj_t *padre, const condizionatore_t *u, int indice)
{
    const bool spento = u->modo == MODO_SPENTO;
    const lv_color_t accento = colore_modo(u->modo);

    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
    /* Scheda attenuata da spenta: c'e ma non chiede attenzione. */
    if (spento) lv_obj_set_style_opa(k, ui_opa(COM.opacita_condizionatore_spento_pct), 0);
    else        ui_bordo(k, LV_BORDER_SIDE_FULL, accento);

    /* nome e pastiglia della modalita */
    lv_obj_t *alto = ui_pannello(k, C_CARD);
    lv_obj_set_width(alto, LV_PCT(100));
    lv_obj_set_height(alto, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alto, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(alto, COM.bordo, 0);

    lv_obj_t *nome = ui_testo(alto, u->nome, C_TXT, FT_M);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nome, LV_PCT(100));

    lv_obj_t *past = ui_pannello(alto, C_CARD2);
    lv_obj_set_size(past, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(past, PRF->geo.radius_btn, 0);
    lv_obj_set_style_pad_hor(past, COM.pastiglia_pad_h, 0);
    lv_obj_set_style_pad_ver(past, COM.pastiglia_pad_v, 0);
    lv_obj_set_flex_flow(past, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(past, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(past, COM.pastiglia_gap, 0);
    ui_icona(past, icona_modo(u->modo), accento, IC_S);
    ui_testo(past, dati_modo_nome(u->modo), accento, FT_S);

    /* temperatura in stanza */
    static char stanza[8][12];
    lv_snprintf(stanza[indice % 8], sizeof stanza[0], "%s°", ui_temp(u->stanza));
    ui_testo(k, stanza[indice % 8], spento ? C_DIM : accento, FT_XL);

    if (!u->disponibile) {
        ui_testo(k, tr(TX_COMMON_UNAVAILABLE), C_DIM, FT_S);
        return;
    }

    /* setpoint */
    const limiti_t lim = dati_limiti_condizionatori();
    lv_obj_t *set = ui_pannello(k, C_CARD);
    lv_obj_set_width(set, LV_PCT(100));
    lv_obj_set_height(set, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(set, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(set, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_add_event_cb(
        tasto_passo(set, ICO_REMOVE, !spento && u->richiesta > lim.min,
                    PRF->tocco.clima_pm, IC_S),
        su_passo_unita, LV_EVENT_CLICKED, (void *)(intptr_t)indice);
    ui_temperatura(set, u->richiesta, spento ? C_DIM : C_TXT, FT_M);
    lv_obj_add_event_cb(
        tasto_passo(set, ICO_ADD, !spento && u->richiesta < lim.max,
                    PRF->tocco.clima_pm, IC_S),
        su_passo_unita, LV_EVENT_CLICKED,
        (void *)(intptr_t)(indice | PASSO_SU));

    /* tre righe di stato */
    riga_stato(k, ICO_MODE_FAN, tr(TX_CLIMATE_FAN),
               dati_ventilazione_nome(u->ventilazione));
    riga_stato(k, ICO_SWAP_VERT, tr(TX_CLIMATE_VANE),
               u->oscillazione == OSC_OSCILLANTE ? tr(TX_CLIMATE_SWINGING)
                                                 : dati_deflettore_nome(u->deflettore));

    /* La riga del timer compare solo se la configurazione ha sia il timer
       sia l'automazione: mezza funzione e peggio di nessuna. */
    /* E solo quando **corre**: un timer fermo non e uno stato da riportare,
       e la riga «Spegni al timer: spento» occupava spazio per dire che non
       succedera niente — cosa che si sa gia da sola. */
    if (u->ha_timer && u->timer_corre) {
        const char *valore;
        if (!u->automazione_attiva) {
            /* Vedere i minuti scorrere e credere che l'unita si spegnera
               sarebbe un inganno: si dice cosa succedera davvero. */
            valore = tr(TX_CLIMATE_NO_EFFECT);
        } else {
            /* Qui basta quanto manca; l'ora esatta di spegnimento sta nel
               dettaglio, dove c'e spazio per leggerla. */
            static char resta[8][12];
            lv_snprintf(resta[indice % 8], sizeof resta[0], "%uh%02u",
                        u->restano_min / 60, u->restano_min % 60);
            valore = resta[indice % 8];
        }
        riga_stato(k, ICO_TIMER, tr(TX_CLIMATE_OFF_TIMER), valore);
        barra_timer(k, u);
    }

    ui_spazio(k);

    /* pulsante che apre il dettaglio */
    lv_obj_t *piu = ui_pannello(k, C_CARD2);
    lv_obj_set_width(piu, LV_PCT(100));
    lv_obj_set_height(piu, PRF->tocco.apertura.h);
    lv_obj_set_style_radius(piu, PRF->geo.radius_btn, 0);
    ui_bordo(piu, LV_BORDER_SIDE_FULL, C_LINE);
    lv_obj_set_flex_flow(piu, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(piu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(piu, PRF->geo.gap, 0);
    lv_obj_add_flag(piu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(piu, su_dettaglio, LV_EVENT_CLICKED,
                        (void *)(intptr_t)indice);
    ui_icona(piu, ICO_TUNE, C_DIM, IC_S);
    ui_testo(piu, tr(TX_CLIMATE_ALL_CONTROLS), C_TXT, FT_S);
}

static void condizionatori(lv_obj_t *c)
{
    const int quanti = dati_condizionatori();
    const uint8_t colonne = PRF->griglia.cond_col;
    const uint8_t righe = PRF->griglia.cond_rig;

    /* In orizzontale non e una griglia ma quattro colonne alte; in verticale
       diventa 2x2 (09-profili.md §1-ter). Le tracce lo dicono da sole. */
    lv_obj_t *g = ui_griglia(c, colonne, righe);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_flex_grow(g, 1);

    for (int n = 0; n < colonne * righe; n++) {
        lv_obj_t *cella;
        if (n < quanti) {
            unita(g, dati_condizionatore(n), n);
            cella = lv_obj_get_child(g, -1);
        } else {
            cella = ui_scheda(g);
            posto_vuoto(cella);
        }
        lv_obj_set_grid_cell(cella, LV_GRID_ALIGN_STRETCH, n % colonne, 1,
                                    LV_GRID_ALIGN_STRETCH, n / colonne, 1);
    }
}

/* --- costruzione -------------------------------------------------------- */

void schermata_clima(lv_obj_t *c)
{
    /* Le viste oltre le due dei gruppi sono i dettagli delle unita. */
    const int dettaglio = ui_vista() - schermata_clima_vista_dettaglio(0);
    if (dettaglio >= 0) { schermata_clima_dettaglio(c, dettaglio); return; }

    static char sotto[48];
    const int chiedono = dati_zone_in_richiesta();
    const int accesi = dati_condizionatori_accesi();

    if (gruppo() == GRUPPO_RISCALDAMENTO)
        lv_snprintf(sotto, sizeof sotto, trn(TXN_CLIMATE_ZONES_CALLING, chiedono),
                    chiedono);
    else
        lv_snprintf(sotto, sizeof sotto, trn(TXN_CLIMATE_UNITS_ON, accesi), accesi);
    ui_testata(tr(TX_SECTION_CLIMATE), sotto);

    fascia(c);
    if (gruppo() == GRUPPO_RISCALDAMENTO) riscaldamento(c);
    else                                condizionatori(c);
}
