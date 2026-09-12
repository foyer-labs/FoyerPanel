/* ------------------------------------------------------------------------
 * Home — 01-specifica-ui.md §2.
 *
 *   ORIZZONTALE   testata con orologio, data e meteo
 *                 fascia [Presenza] [Energia] [Aperture e luci] [Agenda]
 *                 dock, un pulsante per sezione attiva
 *
 *   VERTICALE     testata con orologio, data, meteo e ingranaggio
 *                 Energia a tutta larghezza
 *                 [Presenza] [Aperture e luci]
 *                 Agenda del giorno
 *                 nessun dock: c'e la barra in basso
 *
 * Le schede riassumono quello che le sezioni mostrano per intero, e i dati
 * vengono da sensori aggregati: uno per scheda, con tutto negli attributi.
 * Quando il sensore non c'e, la scheda **dice che non lo sa** invece di
 * restare vuota o di dedurre: dedurre chi e in casa dalla somma di quattro
 * `person` sarebbe una funzione in piu da tenere allineata a Home
 * Assistant, e sbagliata il giorno che qualcuno aggiunge un ospite.
 *
 * Una scheda la cui sezione non e attiva non compare, e le altre si
 * ridistribuiscono lo spazio: e la regola generale di
 * 03-config-contratto.md §4.4, ed e cosi che oggi sparisce l'agenda.
 * --------------------------------------------------------------------- */
#include <string.h>

#include "comuni.h"
#include "config.h"
#include "dati.h"
#include "orologio.h"
#include "widgets/pulsante_comando.h"
#include "widgets/interruttore.h"
#include "nav.h"
#include "schermate.h"
#include "ui.h"

/* --- testata della home ------------------------------------------------- */

/* Gli stati di weather.* di Home Assistant sono una decina; il pannello ne
   distingue quattro, che sono quelle che cambiano cosa ci si mette addosso.
   Distinguerne dieci con dieci icone diverse sarebbe una collezione, non
   un'informazione. */
/* --- la temperatura della stanza ---------------------------------------
 *
 * Questo pannello sta al posto di un termostato, e in testata c'era solo la
 * temperatura **esterna**: quella della stanza non compariva da nessuna
 * parte in home, e chi alzava gli occhi verso quel punto del muro doveva
 * aprire un'altra sezione per trovarla. Nello standby c'era gia, grande;
 * qui mancava del tutto.
 *
 * Il sensore e quello dello standby, letto da dati_standby(): un secondo
 * posto dove dire qual e il termometro di casa potrebbe dire due cose
 * diverse sullo stesso vetro.
 *
 * Elemento suo nella testata e non dentro il riquadro del meteo: li dentro
 * si spezzava a meta, perche' quel riquadro e dimensionato per un numero
 * solo e due ci stavano solo andando a capo. */
static void dentro_casa(lv_obj_t *padre)
{
    if (!cfg_vero("home/indoor_temperature", true)) return;

    const standby_t sb = dati_standby();
    if (!sb.interna_c_e) return;

    lv_obj_t *d = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, 0);
    lv_obj_set_size(d, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(d, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(d, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(d, 0, 0);

    lv_obj_t *riga = ui_pannello(d, C_BG);
    lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
    lv_obj_set_size(riga, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(riga, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(riga, COM.pastiglia_gap, 0);

    /* L'icona dice che **il riscaldamento di questo piano e abilitato**, non
       che la stanza sta chiedendo calore adesso. Erano la stessa icona e
       volevano dire la seconda cosa; adesso e questa, ed e quella che serve
       a un pannello che sta al posto di un termostato. Chi vuole sapere se
       sta chiedendo adesso lo legge nella sezione Clima, dove le zone che
       chiamano sono in ambra.
       Onde di calore e non fiamma: la fiamma resta a dire «sta chiedendo»
       nello standby, e due simboli uguali per due fatti diversi non li
       distingue nessuno.
       Il corpo resta IC_S: qui l'icona sta accanto a un numero da 34 px in
       una testata gia piena, e ingrandirla vorrebbe dire stringere il
       resto. Quella grande e nello standby, dove c'e spazio e distanza. */
    if (sb.riscaldamento_acceso) ui_icona(riga, ICO_HEAT, C_CALDO, IC_S);

    /* Like the weather next to it: two numbers side by side, one with
       small tenths and one with big ones, would look like two panels. */
    ui_temperatura(riga, sb.interna, C_TXT, FT_XL);

    /* Sotto, i gradi chiesti quando l'entita del clima c'e: sono il termine
       di paragone, non il dato, e stanno piccoli. Senza quella, il nome
       della stanza — che dice **di cosa** e la temperatura, e su un pannello
       in corridoio non e scontato. */
    static char sotto[24];
    if (sb.chiesta_c_e)
        lv_snprintf(sotto, sizeof sotto, tr(TX_STANDBY_REQUESTED), ui_temp(sb.chiesta));
    else
        lv_snprintf(sotto, sizeof sotto, "%s", sb.stanza ? sb.stanza : tr(TX_STANDBY_INDOORS));
    ui_testo(d, sotto, C_DIM, FT_XS);
}

static void meteo(lv_obj_t *padre)
{
    lv_obj_t *m = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);
    lv_obj_set_height(m, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(m, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(m, PRF->geo.gap, 0);

    const meteo_t meteo = dati_meteo();
    ui_icona(m, ui_meteo_icona(meteo.stato),
             ui_meteo_colore(meteo.stato), IC_L);
    /* With the decimal when there is one: "18.7°" and not "18°", which was
       the old truncation and said a degree less than the phone did. */
    ui_temperatura(m, meteo.disponibile ? meteo.temperatura : TEMP_IGNOTA,
                   C_TXT, FT_XL);
}

/* --- l'orologio che cammina ---------------------------------------------
 *
 * La schermata si ricostruisce quando cambiano le entita, e i minuti non
 * sono un'entita: senza qualcuno che glielo dica, l'ora resterebbe quella
 * dell'ultima luce accesa. Rifare la schermata ogni minuto sarebbe pero la
 * stessa lezione gia imparata a caro prezzo — un ridisegno pieno costa il
 * bus della PSRAM da cui lo schermo legge — quindi qui si toccano **due
 * etichette** e nient'altro, e solo quando il testo cambia davvero.
 *
 * Le etichette possono essere state distrutte da una navigazione: si
 * chiede a LVGL se esistono ancora invece di fidarsi del puntatore. */
static lv_obj_t *et_ora, *et_data;

/* --- perche non basta chiedere se il puntatore e valido -----------------
 *
 * La prima versione si guardava le spalle con lv_obj_is_valid(), e sembrava
 * abbastanza. Non lo era, e il pannello lo ha detto con un panico:
 * quella funzione risponde "questo e un oggetto vivo?", non "e **il mio**
 * oggetto?". Uscendo dalla home le etichette vengono distrutte, LVGL riusa
 * quella memoria per gli oggetti della schermata nuova, e il puntatore
 * torna a essere validissimo — puntando pero a un pannello. Chiedere il
 * testo a un pannello restituisce spazzatura, e la strcmp che segue muore
 * su un indirizzo vicino a zero.
 *
 * La regola generale: **un puntatore valido non e lo stesso puntatore**, e
 * un controllo di validita non distingue le due cose. Chi conserva un
 * riferimento a un oggetto che qualcun altro puo distruggere deve farsi
 * dire quando succede, non dedurlo.
 *
 * Qui glielo dice l'etichetta stessa, morendo. */
static void scorda_orologio(lv_event_t *e)
{
    LV_UNUSED(e);
    et_ora = et_data = NULL;
}

void home_orologio_aggiorna(void)
{
    if (!et_ora) return;

    char ora[8];
    orologio_ora(ora, sizeof ora);
    ui_scrivi(et_ora, ora);

    if (!et_data) return;
    char data[40];
    orologio_data(data, sizeof data);
    if (data[0]) ui_scrivi(et_data, data);
}

static void su_ingranaggio(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_vai(SEZ_IMPOSTAZIONI);
}

static void testata_home(lv_obj_t *padre)
{
    lv_obj_t *t = ui_pannello(padre, C_BG);
    lv_obj_set_size(t, LV_PCT(100), PRF->geo.home_head_h);
    lv_obj_set_style_pad_hor(t, PRF->geo.pad, 0);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(t, PRF->geo.gap, 0);
    ui_bordo(t, LV_BORDER_SIDE_BOTTOM, C_LINE);

    /* L'ora vera, e sotto la data. Finche la rete non l'ha corretta restano
       i trattini e la frase che spiega perche: un pannello che mostra
       "01:00" con la faccia di chi sa che ore sono e peggio di uno che
       ammette di non saperlo. */
    char ora[8], data[40];
    orologio_ora(ora, sizeof ora);
    orologio_data(data, sizeof data);

    lv_obj_t *col = ui_pannello(t, C_BG);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 0, 0);

    et_ora  = ui_testo(col, ora, C_TXT, FT_XXL);
    et_data = ui_testo(col, data[0] ? data : tr(TX_STANDBY_WAITING_TIME),
                       C_DIM, FT_S);
    /* Basta ascoltare il contenitore: distruggendosi porta via tutti e due,
       e una sola iscrizione non puo restare disallineata rispetto a se
       stessa. */
    lv_obj_add_event_cb(col, scorda_orologio, LV_EVENT_DELETE, NULL);
    ui_spazio(t);
    dentro_casa(t);
    meteo(t);

    /* In verticale l'ingranaggio si sposta qui, perche la barra in basso e
       occupata dalle sezioni (09-profili.md §1-ter).

       In orizzontale sta nel rail, e li lo costruisce nav.c con la stessa
       funzione di tutte le altre voci — che gli attacca anche la
       navigazione. Qui e disegnato a mano, e per un po' e stato un pulsante
       che si accendeva sotto il dito e non portava da nessuna parte: senza
       impostazioni non c'era modo di configurare il pannello dal pannello.
       Sul simulatore non si vedeva, perche' li si guarda la disposizione. */
    if (PRF->orientamento == VERTICALE) {
        lv_obj_t *g = ui_pannello(t, C_CARD2);
        lv_obj_set_size(g, PRF->geo.head_h, PRF->geo.head_h);
        lv_obj_set_style_radius(g, PRF->geo.radius_tile, 0);
        ui_bordo(g, LV_BORDER_SIDE_FULL, C_LINE);
        lv_obj_set_flex_align(g, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(g, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g, su_ingranaggio, LV_EVENT_CLICKED, NULL);
        ui_icona(g, ICO_SETTINGS, C_DIM, IC_M);
    }
}

/* --- schede ------------------------------------------------------------- */

static lv_obj_t *scheda(lv_obj_t *padre, const char *occhiello)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);
    ui_occhiello(k, occhiello);
    return k;
}

/* Quello che si scrive quando il pannello non sa. Non e un errore e non va
   in rosso: e la differenza fra "non lo so" e "non c'e niente", che a
   schermo si vedono uguali se non si dice. */
static void non_lo_so(lv_obj_t *k, const char *cosa)
{
    /* Due motivi diversi per la stessa scheda vuota, e vanno detti diversi:
       senza Home Assistant collegato non manca un sensore, manca tutto. */
    lv_obj_t *l = ui_testo(k, dati_dal_vero() ? cosa
                              : tr(TX_HOME_HA_NOT_CONNECTED), C_DIM, FT_S);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
}

/* Una riga "cosa ....... valore", che e la forma di mezza home. */
static void riga_valore(lv_obj_t *padre, const char *icona, const char *nome,
                        const char *valore)
{
    lv_obj_t *r = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, COM.pastiglia_gap, 0);

    if (icona) ui_icona(r, icona, C_DIM, IC_S);
    lv_obj_t *n = ui_testo(r, nome, C_DIM, FT_S);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(n, 1);
    ui_testo(r, valore, C_TXT, FT_S);
}

/* --- in casa ------------------------------------------------------------ */

static void contenuto_presenza(lv_obj_t *k)
{
    if (!dati_presenza_nota()) {
        non_lo_so(k, tr(TX_HOME_PRESENCE_DOWN));
        return;
    }

    /* Senza l'elenco resta il conteggio, che e comunque un dato vero: e
       meglio di tre nomi dedotti. */
    if (!dati_persone()) {
        static char riga[32];
        const int quante = dati_persone_in_casa();
        lv_snprintf(riga, sizeof riga, trn(TXN_HOME_PEOPLE, quante), quante);
        ui_testo(k, riga, C_TXT, FT_XL);
        ui_testo(k, tr(TX_HOME_AT_HOME), C_DIM, FT_S);
        return;
    }

    for (int n = 0; n < dati_persone(); n++) {
        const persona_t *p = dati_persona(n);
        if (!p) continue;
        lv_obj_t *r = ui_pannello(k, C_BG);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_height(r, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, COM.pastiglia_gap, 0);

        /* L'iniziale in un cerchio: un ritratto non c'e, e le iniziali si
           distinguono da lontano meglio di un nome intero piccolo. */
        lv_obj_t *tondo = ui_pannello(r, p->in_casa ? C_CARD2 : C_BG);
        lv_obj_set_size(tondo, PRF->icona.l, PRF->icona.l);
        lv_obj_set_style_radius(tondo, LV_RADIUS_CIRCLE, 0);
        ui_bordo(tondo, LV_BORDER_SIDE_FULL, p->in_casa ? C_ACC : C_LINE);
        lv_obj_set_flex_align(tondo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        static char iniziale[8][2];
        static uint8_t giro;
        char *i = iniziale[giro++ % 8];
        i[0] = p->nome && *p->nome ? p->nome[0] : '?';
        i[1] = 0;
        ui_testo(tondo, i, p->in_casa ? C_ACC : C_DIM, FT_S);

        lv_obj_t *col = ui_pannello(r, C_BG);
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_height(col, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *nome = ui_testo(col, p->nome ? p->nome : "—",
                                  p->in_casa ? C_TXT : C_DIM, FT_S);
        lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nome, LV_PCT(100));
        if (p->quando && *p->quando) {
            lv_obj_t *q = ui_testo(col, p->quando, C_DIM, FT_XS);
            lv_label_set_long_mode(q, LV_LABEL_LONG_DOT);
            lv_obj_set_width(q, LV_PCT(100));
        }
        if (!p->in_casa)
            lv_obj_set_style_opa(r, ui_opa(COM.opacita_dato_vecchio_pct), 0);
    }
}

/* --- energia adesso ------------------------------------------------------ */

static void contenuto_energia(lv_obj_t *k)
{
    const energia_t e = dati_energia();
    if (!e.disponibile) {
        non_lo_so(k, tr(TX_HOME_INVERTER_DOWN));
        return;
    }

    lv_obj_t *grande = ui_pannello(k, C_BG);
    lv_obj_set_style_bg_opa(grande, LV_OPA_TRANSP, 0);
    lv_obj_set_width(grande, LV_PCT(100));
    lv_obj_set_height(grande, LV_SIZE_CONTENT);
    /* **In colonna e non in riga.** Prima l'unita stava a destra del numero,
       e su una scheda stretta finiva tagliata a meta: "kW dal sole"
       diventava "kW dal", e con un numero di quattro cifre solo "kW". Il
       numero e l'unica cosa che non si puo accorciare, quindi l'unita gli
       va sotto — dove ha tutta la larghezza della scheda e nessun numero con
       cui contendersela. */
    lv_obj_set_flex_flow(grande, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(grande, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grande, 0, 0);
    /* Il numero grande e l'unita piccola sono due etichette: il font delle
       cifre grandi non ha le lettere in tabella, e "kW" dentro quel testo
       diventerebbe due rettangoli vuoti. */
    ui_testo(grande, ui_potenza_numero(e.produzione_w), C_OK, FT_ENERGIA);

    static char unita[32];
    lv_snprintf(unita, sizeof unita, tr(TX_HOME_UNIT_FROM_PV),
                ui_potenza_unita(e.produzione_w));
    lv_obj_t *u = ui_testo(grande, unita, C_DIM, FT_S);
    /* Se anche sotto non ci stesse, si stringe lei con i puntini invece di
       spingere fuori la scheda: 01-specifica-ui.md §2 lo chiede per il dock
       e vale ovunque. */
    lv_label_set_long_mode(u, LV_LABEL_LONG_DOT);
    lv_obj_set_width(u, LV_PCT(100));

    riga_valore(k, ICO_BOLT, tr(TX_HOME_HOUSE), ui_potenza(dati_energia_casa_w()));

    /* La parola cambia col verso, il numero resta positivo: il pannello non
       mostra mai un numero negativo (01-specifica-ui.md §3.3). */
    const verso_t vr = dati_verso(e.rete_w);
    riga_valore(k, ICO_BOLT,
                vr == VERSO_NULLO     ? tr(TX_HOME_GRID)
                : vr == VERSO_POSITIVO ? tr(TX_HOME_FROM_GRID) : tr(TX_HOME_TO_GRID),
                ui_potenza(e.rete_w));
    /* La batteria solo se c'e. Zero per cento e un impianto senza accumulo,
       non un accumulo scarico: mostrare "0%" a chi non ha una batteria
       sarebbe un guasto inventato. Chi ce l'ha davvero e scarica sul serio
       ha comunque una potenza che scorre, e allora la riga compare. */
    if (e.batteria_pct > 0 || e.batteria_w != 0) {
        static char batteria[24];
        lv_snprintf(batteria, sizeof batteria, "%u%%", e.batteria_pct);
        riga_valore(k, ICO_BATTERY_FULL, tr(TX_HOME_BATTERY), batteria);
    }

    riga_valore(k, ICO_SUNNY, tr(TX_HOME_TODAY), ui_energia(e.oggi_wh));
}

/* --- aperture e luci ----------------------------------------------------- */

static void contenuto_aperture(lv_obj_t *k)
{
    static char riga[48];

    if (dati_aperture_note()) {
        const int n = dati_aperture();
        lv_obj_t *grande = ui_pannello(k, C_BG);
        lv_obj_set_style_bg_opa(grande, LV_OPA_TRANSP, 0);
        lv_obj_set_width(grande, LV_PCT(100));
        lv_obj_set_height(grande, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(grande, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(grande, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(grande, COM.pastiglia_gap, 0);

        lv_snprintf(riga, sizeof riga, "%d", n);
        ui_testo(grande, riga, n ? C_ACC : C_TXT, FT_XL);
        ui_testo(grande, trn(TXN_HOME_OPENINGS, n), C_DIM, FT_S);

        for (int i = 0; i < n && i < 3; i++) {
            const apertura_t *a = dati_apertura(i);
            if (!a) continue;
            riga_valore(k, NULL, a->nome, a->da ? a->da : "");
        }
    } else {
        non_lo_so(k, tr(TX_HOME_OPENINGS_DOWN));
    }

    ui_spazio(k);
    lv_snprintf(riga, sizeof riga, trn(TXN_LIGHTS_ON_COUNT, dati_luci_accese()),
                dati_luci_accese());
    riga_valore(k, ICO_LIGHTBULB, tr(TX_SECTION_LIGHTS), riga);
}

/* Le quattro schede nello stesso ordine nei due orientamenti: cosi
   riempirle e una cosa sola e non due che si allontanano. */
/* --- i comandi rapidi ---------------------------------------------------
 *
 * Gli stessi accessi della loro sezione, qui in una riga sola: i cancelli e
 * l'interruttore delle luci del giardino. Sono le cose che si toccano passando —
 * si entra, si esce, si accende fuori — e chiedere un salto di sezione per
 * ognuna vuol dire due tocchi dove ne basta uno.
 *
 * Non e una copia della sezione: li c'e lo stato di ogni accesso, l'ultimo
 * impulso, il sensore della porta del garage. Qui c'e solo il comando, perche' una
 * riga di stato sotto ogni pulsante rifarebbe la sezione dentro la home e si
 * riprenderebbe lo spazio che questa striscia era venuta a occupare.
 *
 * `dati_accesso_aziona()` e la stessa di la: un comando solo, un posto solo
 * da cui parte. */
static void su_comando_home(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    pulsante_comando_stato(b, CMD_IN_CORSO);
    const int n = (int)(intptr_t)lv_event_get_user_data(e);
    pulsante_comando_stato(b, dati_accesso_aziona(n) ? CMD_RIUSCITO
                                                     : CMD_FALLITO);
}

static void su_interruttore_home(lv_event_t *e)
{
    dati_accesso_aziona((int)(intptr_t)lv_event_get_user_data(e));
}

/* --- il robot, una riga ------------------------------------------------
 *
 * Cosa sta facendo e quanta batteria ha. Per saperlo bisognava entrare nella
 * sua sezione, e la risposta e lunga due parole: sta pulendo, oppure e alla
 * base. Toccandola si apre la sezione, dove ci sono le stanze e i comandi.
 *
 * Sparisce quando il robot non e configurato o non risponde: una riga che
 * dicesse «stato ignoto» occuperebbe lo spazio di una che non serve. */
static void su_riga_robot(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_vai(SEZ_ROBOT);
}

static void riga_robot(lv_obj_t *c)
{
    if (!cfg_vero("home/robot", true)) return;
    if (!sezione_e_attiva(SEZ_ROBOT)) return;

    const robot_t r = dati_robot();
    if (!r.disponibile) return;

    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(k, PRF->geo.gap, 0);
    lv_obj_add_flag(k, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(k, su_riga_robot, LV_EVENT_CLICKED, NULL);

    const bool lavora = r.stato == ROB_PULISCE || r.stato == ROB_RIENTRA;
    ui_icona(k, ICO_MOP, lavora ? C_ACC : C_DIM, IC_M);

    /* Il nome prima dello stato: in una casa il robot ha un nome, e
       «Dusty sta pulendo» e la stessa informazione detta come la direbbe
       qualcuno. Chi non gliene ha dato uno legge «Robot», che e come si
       chiamava prima. */
    const char *cosa = tr(r.stato == ROB_PULISCE   ? TX_HOME_ROBOT_CLEANING
                        : r.stato == ROB_RIENTRA   ? TX_HOME_ROBOT_RETURNING
                        : r.stato == ROB_ALLA_BASE ? TX_HOME_ROBOT_DOCKED
                        : r.stato == ROB_IN_PAUSA  ? TX_HOME_ROBOT_PAUSED
                        : r.stato == ROB_ERRORE    ? TX_HOME_ROBOT_ERROR
                        : TX_HOME_ROBOT_STOPPED);

    lv_obj_t *col = ui_pannello(k, C_CARD);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 0, 0);

    lv_obj_t *n = ui_testo(col, cfg_testo("robot/name", tr(TX_ROBOT_DEFAULT_NAME)), C_TXT, FT_M);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_width(n, LV_PCT(100));

    /* Il triangolo prima dello stato, in coda alla riga: e' un bersaglio
       suo dentro una riga che e gia un bersaglio — toccando il triangolo si
       va agli avvisi, toccando il resto alla sezione. */
    lv_obj_t *st = ui_testo(col, cosa,
                            r.stato == ROB_ERRORE ? C_WARN : C_DIM, FT_XS);
    lv_label_set_long_mode(st, LV_LABEL_LONG_DOT);
    lv_obj_set_width(st, LV_PCT(100));

    /* Il triangolo, quando c'e qualcosa. Sta prima della batteria perche'
       e' la cosa che chiede di essere fatta, e la batteria e' solo un
       numero da sapere. */
    robot_triangolo(k);

    if (r.batteria_c_e) {
        static char pct[8];
        lv_snprintf(pct, sizeof pct, "%d%%", r.batteria);
        ui_testo(k, pct,
                 r.batteria <= 20 ? C_WARN : r.in_carica ? C_ACC : C_OK, FT_M);
    }
}

/* --- lavatrice e asciugatrice ------------------------------------------
 *
 * Una scheda sola divisa in due, e non due righe: sono una coppia e si
 * leggono come una coppia. Chi lavora sta a piena luce, chi e fermo e
 * attenuato — e l'attenuazione fa il lavoro che due righe di testo
 * farebbero in piu spazio.
 *
 * Costa una riga sola tutto il giorno, ed e il punto: questi due apparecchi
 * sono fermi quasi sempre, e una forma che ne costasse due sarebbe pagata
 * ventiquattro ore per essere utile venti minuti.
 *
 * Sparisce quando non sono configurati o quando nessuna delle due entita
 * risponde, come fa la riga del robot: una scheda che dicesse «stato
 * ignoto» occuperebbe lo spazio di una che non serve. */
static void meta_elettrodomestico(lv_obj_t *padre, const elettrodomestico_t *e)
{
    lv_obj_t *m = ui_pannello(padre, C_CARD);
    lv_obj_set_flex_grow(m, 1);
    lv_obj_set_height(m, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(m, 0, 0);
    lv_obj_set_flex_flow(m, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(m, PRF->geo.gap, 0);

    /* Fermo o non disponibile: tutta la meta si attenua in un colpo solo,
       invece di scegliere un colore diverso per ogni pezzo. */
    if (!e->in_ciclo)
        lv_obj_set_style_opa(m, ui_opa(COM.opacita_dato_vecchio_pct), 0);

    const lv_color_t colore = e->in_ciclo ? C_ACC : C_DIM;
    /* Un'icona per apparecchio, non una ripetuta: in una scheda divisa
       in due meta uguali l'icona e la sola cosa che dice quale meta
       stai guardando prima ancora di leggere il nome. */
    ui_icona(m, e->asciuga ? ICO_DRY : ICO_LOCAL_LAUNDRY_SERVICE,
             colore, IC_M);

    lv_obj_t *col = ui_pannello(m, C_CARD);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, COM.gap_stretto, 0);

    lv_obj_t *nm = ui_testo(col, e->nome && e->nome[0] ? e->nome : "—",
                            C_TXT, FT_M);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, LV_PCT(100));

    ui_testo(col, !e->disponibile ? tr(TX_COMMON_NOT_RESPONDING)
                 : e->in_ciclo    ? tr(TX_HOME_CYCLE_RUNNING)
                                  : tr(TX_HOME_APPLIANCE_IDLE),
             e->in_ciclo ? C_ACC : C_DIM, FT_XS);

    /* La barretta solo con il pieno carico dichiarato: senza, non si sa
       rispetto a cosa riempirla e una barra che finge di misurare e peggio
       di nessuna barra. */
    if (e->in_ciclo && e->quota_pct) {
        lv_obj_t *barra = ui_pannello(col, C_BG);
        lv_obj_set_width(barra, LV_PCT(100));
        lv_obj_set_height(barra, COM.barretta_h);
        lv_obj_set_style_radius(barra, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(barra, 0, 0);

        lv_obj_t *dentro = ui_pannello(barra, C_BG);
        lv_obj_set_style_bg_color(dentro, C_ACC, 0);
        lv_obj_set_style_bg_opa(dentro, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dentro, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_height(dentro, LV_PCT(100));
        lv_obj_set_width(dentro, lv_pct(e->quota_pct));
    }

    if (e->potenza_c_e)
        ui_testo(m, ui_potenza(e->watt), colore, FT_M);
}

static void elettrodomestici(lv_obj_t *c)
{
    if (!cfg_vero("home/appliances", true)) return;

    const int quanti = dati_elettrodomestici();
    if (!quanti) return;

    /* Nessuna delle due risponde: non si disegna niente. Una sola che
       risponde basta a giustificare la scheda — l'altra meta dira «non
       risponde», che e un'informazione. */
    bool qualcuna = false;
    for (int n = 0; n < quanti; n++) {
        const elettrodomestico_t *e = dati_elettrodomestico(n);
        if (e && e->disponibile) { qualcuna = true; break; }
    }
    if (!qualcuna) return;

    lv_obj_t *k = ui_scheda(c);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    /* Le due meta allineate **in cima** e non al centro: quella che lavora e
       piu alta di una barretta, e centrandole i due nomi finivano a quote
       diverse. Da fermi si legge come una svista, ed e proprio il caso
       normale. */
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(k, PRF->geo.gap, 0);

    for (int n = 0; n < quanti; n++) {
        const elettrodomestico_t *e = dati_elettrodomestico(n);
        if (!e) continue;

        /* Il filo fra le due meta, e solo fra: con un apparecchio solo non
           c'e niente da separare. */
        if (n) {
            lv_obj_t *filo = ui_pannello(k, C_LINE);
            lv_obj_set_width(filo, COM.bordo);
            lv_obj_set_height(filo, LV_PCT(100));
        }
        meta_elettrodomestico(k, e);
    }
}

static void comandi_rapidi(lv_obj_t *c)
{
    const int quanti = dati_accessi();
    if (quanti <= 0) return;

    lv_obj_t *riga = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
    lv_obj_set_width(riga, LV_PCT(100));
    lv_obj_set_height(riga, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(riga, PRF->geo.gap, 0);

    for (int n = 0; n < quanti; n++) {
        const accesso_t *a = dati_accesso(n);
        if (!a) continue;

        lv_obj_t *k = ui_scheda(riga);
        lv_obj_set_flex_grow(k, 1);
        lv_obj_set_height(k, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(k, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(k, COM.pastiglia_gap, 0);

        /* Il nome si accorcia invece di allargare la scheda: quattro schede
           di larghezza diversa su una riga si leggono come un errore di
           impaginazione, e i nomi degli accessi sono lunghi quanto capita. */
        lv_obj_t *nome = ui_testo(k, a->nome, C_DIM, FT_XS);
        lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nome, LV_PCT(100));
        lv_obj_set_style_text_align(nome, LV_TEXT_ALIGN_CENTER, 0);

        /* --- chi ha un sensore dice com'e, e chi non ce l'ha tace --------
         *
         * La porta del garage ha un sensore di stato, i cancelli no. Mostrare
         * "chiuso" sotto un cancello che non ha modo di saperlo sarebbe la
         * bugia piu comoda di questo pannello — quella che si legge prima
         * di andare a dormire.
         *
         * Quindi la riga compare **solo** dove c'e un riscontro, e dice tre
         * cose: aperto in ambra, chiuso in verde, e "stato non noto" in
         * grigio quando il sensore c'e ma non risponde. Il colore fa il
         * lavoro da lontano, la parola lo fa da vicino. */
        /* La riga c'e sempre, anche vuota. Senza, la scheda con il sensore
           diventava piu alta delle altre tre e la fila si sfalsava — lo
           stesso difetto di allineamento che l'interruttore aveva appena
           smesso di avere. Una riga vuota non si vede e tiene il posto. */
        const bool noto = a->ha_sensore && a->stato_noto;
        ui_testo(k, !a->ha_sensore ? ""
                    : !noto        ? tr(TX_ACCESS_STATE_UNKNOWN)
                    : a->aperto    ? tr(TX_HOME_OPEN_M) : tr(TX_HOME_CLOSED_M),
                 !noto ? C_DIM : a->aperto ? C_ACC : C_OK, FT_XS);

        if (a->tipo == ACC_INTERRUTTORE) {
            /* --- l'interruttore dentro un vano alto quanto un pulsante ---
             *
             * L'interruttore e piu basso di un pulsante di comando — ha la
             * sua misura, che vale ovunque nel pannello — e la sua scheda
             * finiva percio piu corta delle altre tre. Quattro schede
             * affiancate di altezza diversa si leggono come un errore di
             * impaginazione, non come una differenza voluta.
             *
             * Non si allunga l'interruttore: un interruttore alto il doppio
             * del suo tratto non sembra piu un interruttore. Si mette in
             * mezzo a un vano che ha l'altezza del pulsante, e la riga torna
             * dritta senza che nessuno dei due componenti cambi identita. */
            lv_obj_t *vano = ui_pannello(k, C_CARD);
            lv_obj_set_style_bg_opa(vano, LV_OPA_TRANSP, 0);
            lv_obj_set_size(vano, LV_PCT(100), PRF->tocco.apertura.h);
            lv_obj_set_flex_flow(vano, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(vano, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            lv_obj_t *i = interruttore(vano, a->acceso && a->disponibile,
                                       a->disponibile);
            if (a->disponibile)
                lv_obj_add_event_cb(i, su_interruttore_home, LV_EVENT_CLICKED,
                                    (void *)(intptr_t)a->indice);
        } else {
            lv_obj_t *b = pulsante_comando(k, tr(TX_ACCESS_OPEN_BUTTON), a->conferma,
                                           a->disponibile);
            lv_obj_set_width(b, LV_PCT(100));
            if (a->disponibile)
                lv_obj_add_event_cb(b, su_comando_home, EV_COMANDO_SCATTATO,
                                    (void *)(intptr_t)a->indice);
        }
    }
}

static void riempi(lv_obj_t *k, int quale)
{
    switch (quale) {
    case 0: contenuto_presenza(k); break;
    case 1: contenuto_energia(k);  break;
    case 2: contenuto_aperture(k); break;
    default: break;                /* l'agenda ha gia il suo contenuto */
    }
}

/* --- disposizione orizzontale ------------------------------------------- */

static void fascia_orizzontale(lv_obj_t *c)
{
    lv_obj_t *f = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);
    if (PRF->home.fascia_h) lv_obj_set_height(f, PRF->home.fascia_h);
    else                    lv_obj_set_flex_grow(f, 1);

    /* Le larghezze fisse vengono dal profilo; a 0 vuol dire "prendi lo
       spazio che resta", ed e cosi che la scheda flessibile cambia da un
       pannello all'altro senza che qui si scriva un numero. */
    struct { const char *occhiello; uint16_t larghezza; sezione_t serve; } schede[] = {
        { tr(TX_HOME_CARD_PRESENCE), PRF->home.presenza_w, SEZ_QUANTE   },
        { tr(TX_HOME_CARD_ENERGY),   PRF->home.energia_w,  SEZ_ENERGIA  },
        { tr(TX_HOME_CARD_OPENINGS), PRF->home.aperture_w, SEZ_QUANTE   },
        { tr(TX_HOME_CARD_TODAY),    PRF->home.agenda_w,   SEZ_AGENDA   },
    };

    for (unsigned n = 0; n < sizeof schede / sizeof schede[0]; n++) {
        /* Sezione senza sorgente dati: la scheda non compare e le altre si
           ridistribuiscono lo spazio. Vale oggi per l'agenda. */
        if (schede[n].serve != SEZ_QUANTE && !sezione_e_attiva(schede[n].serve))
            continue;

        lv_obj_t *k = scheda(f, schede[n].occhiello);
        lv_obj_set_height(k, LV_PCT(100));
        if (schede[n].larghezza) lv_obj_set_width(k, schede[n].larghezza);
        else                     lv_obj_set_flex_grow(k, 1);
        riempi(k, n);
    }

    /* --- e in orizzontale la striscia dei comandi non c'e ---------------
     *
     * Provata e tolta quando sotto le schede c'erano la striscia delle
     * telecamere e il dock: il dock era l'unico dei tre che poteva cedere
     * altezza, si stringeva sotto il suo minimo e le icone uscivano dal
     * vetro tagliate a meta.
     *
     * Adesso la striscia non c'e piu e lo spazio si e liberato, quindi la
     * ragione di allora e caduta. Non si rimette lo stesso senza guardare:
     * la misura da toccare sarebbe home.fascia_h, e una scelta di
     * proporzioni si fa su un vetro, non a memoria. */
}

/* --- disposizione verticale --------------------------------------------- */

static void fascia_verticale(lv_obj_t *c)
{
    /* Energia a tutta larghezza in cima: e il dato che si guarda da lontano. */
    lv_obj_t *e = scheda(c, tr(TX_HOME_CARD_ENERGY));
    lv_obj_set_width(e, LV_PCT(100));
    lv_obj_set_height(e, LV_SIZE_CONTENT);
    riempi(e, 1);

    /* Presenza e aperture affiancate, a meta ciascuna. */
    lv_obj_t *coppia = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(coppia, LV_OPA_TRANSP, 0);
    lv_obj_set_width(coppia, LV_PCT(100));
    lv_obj_set_flex_grow(coppia, 1);
    lv_obj_set_flex_flow(coppia, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(coppia, PRF->geo.gap, 0);

    lv_obj_t *p = scheda(coppia, tr(TX_HOME_CARD_PRESENCE));
    lv_obj_set_flex_grow(p, 1);
    lv_obj_set_height(p, LV_PCT(100));
    riempi(p, 0);

    lv_obj_t *a = scheda(coppia, tr(TX_HOME_CARD_OPENINGS));
    lv_obj_set_flex_grow(a, 1);
    lv_obj_set_height(a, LV_PCT(100));
    riempi(a, 2);

    /* Sotto le due schede, non dentro: il loro contenuto e corto e la
       coppia cresceva a riempire l'altezza, lasciando due rettangoli quasi
       vuoti. Queste righe hanno altezza propria e la coppia cresce in cio
       che resta, quindi lo spazio si recupera senza toccare misure. */
    riga_robot(c);
    elettrodomestici(c);
    comandi_rapidi(c);

    if (sezione_e_attiva(SEZ_AGENDA)) {
        lv_obj_t *ag = scheda(c, tr(TX_HOME_CARD_TODAY));
        lv_obj_set_width(ag, LV_PCT(100));
        lv_obj_set_flex_grow(ag, 1);
    }
}

/* --- costruzione -------------------------------------------------------- */

void schermata_home(lv_obj_t *c)
{
    ui_testata(NULL, NULL);

    testata_home(c);

    lv_obj_t *corpo = ui_pannello(c, C_BG);
    lv_obj_set_width(corpo, LV_PCT(100));
    lv_obj_set_flex_grow(corpo, 1);
    lv_obj_set_flex_flow(corpo, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(corpo, PRF->geo.pad, 0);
    lv_obj_set_style_pad_row(corpo, PRF->geo.gap, 0);

    if (PRF->orientamento == VERTICALE) fascia_verticale(corpo);
    else                                fascia_orizzontale(corpo);

    /* Il dock c'e solo in orizzontale; in verticale lo sostituisce la barra
       e lo spazio recuperato torna al contenuto. */
    lv_obj_t *d = nav_dock(corpo);
    if (d) lv_obj_set_flex_grow(d, 1);
}
