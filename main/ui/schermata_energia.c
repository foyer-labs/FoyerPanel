/* ------------------------------------------------------------------------
 * Energia — 01-specifica-ui.md §3.3.
 *
 *   fascia superiore con quattro riquadri
 *   grafico della giornata e colonna delle stringhe
 *
 * REGOLA OBBLIGATORIA DEI SEGNI. Il pannello non mostra mai un numero
 * negativo: mostra il valore assoluto e cambia la parola. "dalla rete" o
 * "in rete", "in carica" o "in scarica", e "ferma" sotto i 50 W — cosi la
 * batteria non alterna a ogni aggiornamento.
 *
 * Il consumo di casa non e un sensore: e produzione + rete - batteria.
 *
 * La potenza per stringa e stimata, perche l'inverter espone tensione e
 * corrente ma non la potenza. La somma delle due stringhe risulta un po'
 * superiore alla produzione dichiarata: quella e in alternata, queste in
 * continua, prima del rendimento di conversione. Non e un errore, e non va
 * "corretto" facendo quadrare i numeri.
 * --------------------------------------------------------------------- */
#include "comuni.h"
#include "dati.h"
#include "schermate.h"
#include "ui.h"

/* --- fascia superiore --------------------------------------------------- */

static void riquadro(lv_obj_t *padre, const char *ico, lv_color_t colore,
                     const char *valore, const char *sotto)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_height(k, LV_PCT(100));
    lv_obj_set_style_min_width(k, 0, 0);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    lv_obj_t *alto = ui_pannello(k, C_CARD);
    lv_obj_set_width(alto, LV_PCT(100));
    lv_obj_set_height(alto, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alto, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(alto, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(alto, COM.pastiglia_gap, 0);
    /* FT_L e non FT_XL: quattro riquadri su ottocento pixel lasciano
       centottanta di larghezza ciascuno, e «3,42 kW» a FT_XL ne vuole di
       piu — la «W» finiva tagliata dal bordo della scheda. Si vedeva solo
       guardando una cattura: a ragionarci sopra la fascia sembrava a
       posto, ed era sbagliata da sempre. */
    ui_testo(alto, valore, colore, FT_L);

    lv_obj_t *basso = ui_pannello(k, C_CARD);
    lv_obj_set_width(basso, LV_PCT(100));
    lv_obj_set_height(basso, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(basso, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(basso, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(basso, COM.pastiglia_gap, 0);
    ui_icona(basso, ico, C_DIM, IC_S);
    lv_obj_t *l = ui_testo(basso, sotto, C_DIM, FT_S);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(l, 1);
}

static void fascia(lv_obj_t *c)
{
    const energia_t e = dati_energia();

    lv_obj_t *f = ui_pannello(c, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, PRF->energia.fascia_h);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    /* Produzione. A zero non si scrive "0,00 kW": si dice che non c'e. */
    const bool produce = e.produzione_w > ENERGIA_SOGLIA_W;
    riquadro(f, ICO_SOLAR_POWER, produce ? C_OK : C_DIM,
             produce ? ui_potenza(e.produzione_w) : "—",
             produce ? tr(TX_ENERGY_FROM_SUN) : tr(TX_ENERGY_NO_PRODUCTION));

    riquadro(f, ICO_HOME, C_TXT, ui_potenza(dati_energia_casa_w()), tr(TX_ENERGY_IN_HOUSE));

    /* Rete: la parola cambia col verso, il numero resta positivo. */
    const verso_t vr = dati_verso(e.rete_w);
    riquadro(f, ICO_BOLT, vr == VERSO_NEGATIVO ? C_OK : C_TXT,
             vr == VERSO_NULLO ? "—" : ui_potenza(e.rete_w),
             vr == VERSO_POSITIVO ? tr(TX_ENERGY_FROM_GRID)
             : vr == VERSO_NEGATIVO ? tr(TX_ENERGY_TO_GRID) : tr(TX_ENERGY_GRID_IDLE));

    /* La batteria, e non piu l'energia di oggi: «oggi» adesso sta in fondo
       alla schermata, insieme agli altri tre totali della giornata. Qui in
       alto ci sono solo grandezze **istantanee**, e mescolarci un totale
       faceva leggere quattro numeri come se fossero la stessa cosa. */
    static char pila[16];
    lv_snprintf(pila, sizeof pila, "%u%%", e.batteria_pct);
    const verso_t vb = dati_verso(e.batteria_w);
    riquadro(f, ICO_BATTERY_FULL, e.batteria_pct <= 20 ? C_WARN
                           : vb == VERSO_POSITIVO ? C_ACC : C_TXT,
             e.disponibile ? pila : "—",
             vb == VERSO_POSITIVO ? tr(TX_ENERGY_CHARGING)
             : vb == VERSO_NEGATIVO ? tr(TX_ENERGY_DISCHARGING) : tr(TX_ENERGY_BATTERY_IDLE));
}

/* --- grafico della giornata --------------------------------------------- */

/* --- la banda dell'impianto --------------------------------------------
 *
 * Il valore di adesso a sinistra, la curva della giornata accanto. Prima il
 * grafico si prendeva tutto lo spazio disponibile e mostrava un solo dato;
 * cosi occupa un'altezza dichiarata e ne lascia agli altri quattro blocchi.
 *
 * **La curva e ancora finta.** `dati_storico()` legge un array costante nel
 * codice: la forma e quella di una giornata di sole con le nuvole del
 * pomeriggio, non quella di oggi. Sta scritto qui perche chi guarda questo
 * codice sappia che quel disegno non e una misura — e chi lo guarda sul
 * vetro non ha modo di accorgersene. */
static void banda(lv_obj_t *padre)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_height(k, PRF->energia.banda_h);
    lv_obj_set_flex_flow(k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(k, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(k, PRF->geo.gap, 0);

    lv_obj_t *sx = ui_pannello(k, C_CARD);
    lv_obj_set_size(sx, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sx, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sx, COM.gap_stretto, 0);
    ui_occhiello(sx, tr(TX_ENERGY_PLANT));

    const energia_t e = dati_energia();
    ui_testo(sx, ui_potenza(e.produzione_w),
             e.produzione_w > ENERGIA_SOGLIA_W ? C_OK : C_DIM, FT_XL);

    const int punti = dati_storico_punti();

    /* Senza curva vera non si disegna una riga piatta: una riga a zero e
       indistinguibile da una giornata di pioggia, e mentirebbe con l'aria
       di informare. Si dice cos'e successo — sta aspettando, oppure quel
       sensore non ha statistiche — e si lascia il posto vuoto. */
    if (dati_dal_vero() && !dati_storico_vero()) {
        ui_testo(k, dati_storico_negato()
                    ? tr(TX_ENERGY_NO_STATISTICS)
                    : tr(TX_ENERGY_CURVE_COMING),
                 C_DIM, FT_S);
        return;
    }

    int32_t massimo = 1;
    for (int n = 0; n < punti; n++)
        if (dati_storico(n) > massimo) massimo = dati_storico(n);

    lv_obj_t *g = lv_chart_create(k);
    lv_obj_remove_style_all(g);
    lv_obj_set_flex_grow(g, 1);
    lv_obj_set_height(g, LV_PCT(100));
    lv_obj_set_style_min_width(g, 0, 0);
    lv_chart_set_type(g, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g, (uint32_t)punti);
    lv_chart_set_range(g, LV_CHART_AXIS_PRIMARY_Y, 0, massimo);
    lv_chart_set_div_line_count(g, 0, 0);
    lv_obj_set_style_line_width(g, COM.tratto, LV_PART_ITEMS);
    lv_obj_set_style_size(g, 0, 0, LV_PART_INDICATOR);

    lv_chart_series_t *s = lv_chart_add_series(g, C_OK, LV_CHART_AXIS_PRIMARY_Y);
    for (int n = 0; n < punti; n++) {
        const int32_t v = dati_storico(n);
        /* Le ore che non sono ancora arrivate — e quelle di cui non si sa
           niente — restano **buchi**, non zeri: LV_CHART_POINT_NONE spezza
           la linea invece di farla cadere sull'asse. Alle nove del mattino
           un grafico che scende a zero per le quindici ore che mancano
           racconta una giornata finita e disastrosa. */
        lv_chart_set_next_value(g, s, v < 0 ? LV_CHART_POINT_NONE : v);
    }
}

/* --- i quattro riquadri delle stringhe ----------------------------------
 *
 * Tensione e corrente separate, come nella scheda che questa casa gia
 * guarda su Home Assistant: due riquadri per stringa, ciascuno con il suo
 * numero grande. Sono quattro riquadri per due stringhe e costano spazio,
 * ma la lettura e quella a cui uno e abituato — e un pannello che chiede di
 * imparare una lettura nuova per gli stessi dati chiede troppo. */
static lv_obj_t *riquadro_stringa(lv_obj_t *padre, const stringa_t *s,
                                  bool volt)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_style_min_width(k, 0, 0);
    lv_obj_set_style_pad_row(k, COM.gap_stretto, 0);

    static char eti[4][24];
    static int  quale;
    quale = (quale + 1) % 4;
    lv_snprintf(eti[quale], sizeof eti[0], "%s %s",
                volt ? "V." : "A.", s->nome ? s->nome : "");
    ui_occhiello(k, eti[quale]);

    if (!s->disponibile) {
        ui_testo(k, tr(TX_COMMON_UNAVAILABLE), C_DIM, FT_S);
        return k;
    }

    static char val[4][20];
    if (volt)
        lv_snprintf(val[quale], sizeof val[0], "%u%c%u V",
                    s->tensione_decimi / 10, i18n_decimal(),
                    s->tensione_decimi % 10);
    else
        lv_snprintf(val[quale], sizeof val[0], "%u%c%02u A",
                    s->corrente_centesimi / 100, i18n_decimal(),
                    s->corrente_centesimi % 100);
    ui_testo(k, val[quale], C_TXT, FT_L);
    return k;
}

static lv_obj_t *stringhe_griglia(lv_obj_t *padre)
{
    const int quante = dati_stringhe();
    if (!quante) return NULL;

    /* Due colonne per stringa — la tensione e la corrente — dette con una
       griglia e non con una percentuale: le colonne si dividono lo spazio da
       sole, e non c'e nessun numero da tenere allineato al gap. */
    lv_obj_t *g = ui_griglia(padre, 2, quante);
    lv_obj_set_width(g, LV_PCT(100));
    lv_obj_set_height(g, PRF->energia.stringa_h * quante
                      + PRF->geo.gap * (quante - 1));

    for (int n = 0; n < quante; n++) {
        const stringa_t *s = dati_stringa(n);
        if (!s) continue;
        for (int v = 0; v < 2; v++) {
            lv_obj_t *k = riquadro_stringa(g, s, v == 0);
            lv_obj_set_grid_cell(k, LV_GRID_ALIGN_STRETCH, v, 1,
                                 LV_GRID_ALIGN_STRETCH, n, 1);
        }
    }
    return g;
}

/* --- chi sta consumando, e scorre ---------------------------------------
 *
 * Dodici dispositivi non stanno in nessuna altezza ragionevole, e la scelta
 * era fra paginarli e farli scorrere. Scorre: davanti a un pannello a muro
 * e lo stesso gesto con cui si scorre qualunque altra cosa, mentre delle
 * frecce vanno trovate, capite e centrate col dito.
 *
 * La barra di scorrimento resta visibile invece che comparire al tocco: su
 * un elenco che a volte sta tutto e a volte no, e l'unica cosa che dice
 * **che c'e dell'altro sotto**. */
static lv_obj_t *consumi(lv_obj_t *padre)
{
    lv_obj_t *k = ui_scheda(padre);
    lv_obj_set_width(k, LV_PCT(100));
    lv_obj_set_flex_grow(k, 1);
    lv_obj_set_style_min_height(k, 0, 0);
    lv_obj_set_style_pad_row(k, PRF->geo.gap, 0);

    lv_obj_t *hd = ui_pannello(k, C_CARD);
    lv_obj_set_width(hd, LV_PCT(100));
    lv_obj_set_height(hd, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hd, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hd, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_occhiello(hd, tr(TX_ENERGY_WHO_IS_USING));
    ui_spazio(hd);
    ui_testo(hd, ui_potenza(dati_dispositivi_totale_w()), C_DIM, FT_S);

    const int quanti = dati_dispositivi();
    if (!quanti) {
        ui_testo(k, tr(TX_ENERGY_NO_DEVICES), C_DIM, FT_S);
        return k;
    }

    lv_obj_t *elenco = ui_pannello(k, C_CARD);
    lv_obj_set_width(elenco, LV_PCT(100));
    lv_obj_set_flex_grow(elenco, 1);
    lv_obj_set_style_min_height(elenco, 0, 0);
    lv_obj_set_style_pad_all(elenco, 0, 0);
    /* Un margine a destra per la barra di scorrimento: senza, le righe sono
       larghe quanto l'elenco e le passano sopra — la barra c'e, si disegna,
       e non si vede. */
    lv_obj_set_style_pad_right(elenco, PRF->geo.gap, 0);
    lv_obj_set_flex_flow(elenco, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(elenco, PRF->geo.gap, 0);
    /* ui_pannello() **toglie** LV_OBJ_FLAG_SCROLLABLE a ogni pannello, ed e
       la scelta giusta: in una schermata a misura fissa un pannello che
       scorre di nascosto nasconde contenuto senza dirlo. Qui lo scorrimento
       lo vogliamo, quindi si rimette — e va rimesso a mano, se no questo
       elenco non scorre affatto e non c'e nessun segno che lo dica. */
    lv_obj_add_flag(elenco, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(elenco, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(elenco, LV_SCROLLBAR_MODE_ON);
    /* La barra va vista, se no non dice niente: senza un colore esplicito
       resta trasparente sul fondo della scheda, ed e l'unica cosa che
       annuncia che sotto c'e dell'altro. */
    lv_obj_set_style_bg_color(elenco, C_DIM, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(elenco, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(elenco, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(elenco, COM.barra_scorrimento_w, LV_PART_SCROLLBAR);
    lv_obj_add_flag(elenco, LV_OBJ_FLAG_SCROLL_MOMENTUM);

    const int32_t massimo = dati_dispositivo(0) ? dati_dispositivo(0)->watt : 0;

    for (int n = 0; n < quanti; n++) {
        const dispositivo_t *d = dati_dispositivo(n);
        if (!d) continue;

        lv_obj_t *r = ui_pannello(elenco, C_CARD);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_height(r, PRF->energia.consumo_riga_h);
        lv_obj_set_style_pad_all(r, 0, 0);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(r, COM.gap_stretto, 0);

        lv_obj_t *riga = ui_pannello(r, C_CARD);
        lv_obj_set_width(riga, LV_PCT(100));
        lv_obj_set_height(riga, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(riga, 0, 0);
        lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(riga, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t *nome = ui_testo(riga, d->nome ? d->nome : "",
                                  d->disponibile ? C_TXT : C_DIM, FT_M);
        lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(nome, 1);

        /* Il colore della **barra** dice la posizione, non il valore: il
           primo e caldo, gli altri si spengono. Un dispositivo che consuma
           ottocento watt in una casa che ne consuma mille e un altro
           discorso rispetto a ottocento su diecimila, e una scala assoluta
           non lo direbbe.
        
           Dal terzo in giu la barra e **attenuata** e non bianca: bianca
           pesa piu dell'ambra che le sta sopra, e il colore finiva per
           contraddire l'ordine — si vedeva solo guardando una cattura. */
        const lv_color_t colore = !d->disponibile ? C_DIM
                                : n == 0          ? C_WARN
                                : n == 1          ? C_ACC
                                                  : C_DIM;
        /* Il numero resta leggibile: e il testo, non un indicatore. */
        if (d->disponibile)
            ui_testo(riga, ui_potenza(d->watt),
                     n < 2 ? colore : C_TXT, FT_M);
        else
            ui_testo(riga, "—", C_DIM, FT_M);

        lv_obj_t *barra = ui_pannello(r, C_BG);
        lv_obj_set_width(barra, LV_PCT(100));
        lv_obj_set_height(barra, COM.barretta_h);
        lv_obj_set_style_radius(barra, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(barra, 0, 0);

        if (d->disponibile && massimo > 0 && d->watt > 0) {
            lv_obj_t *dentro = ui_pannello(barra, C_BG);
            lv_obj_set_style_bg_color(dentro, colore, 0);
            lv_obj_set_style_bg_opa(dentro, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(dentro, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_height(dentro, LV_PCT(100));
            int32_t pct = (int32_t)((int64_t)d->watt * 100 / massimo);
            if (pct < 3) pct = 3;
            lv_obj_set_width(dentro, lv_pct(pct));
        }
    }
    return k;
}

/* --- i quattro totali della giornata ------------------------------------
 *
 * Energia, non potenza. Chi manca non si disegna: meglio tre riquadri che
 * quattro di cui uno mostra zero fingendo di sapere. */
static void totali(lv_obj_t *padre)
{
    const giornata_t g = dati_giornata();
    if (!g.prodotto_c_e && !g.consumato_c_e && !g.prelevato_c_e && !g.immesso_c_e)
        return;

    lv_obj_t *f = ui_pannello(padre, C_BG);
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, 0);
    lv_obj_set_width(f, LV_PCT(100));
    lv_obj_set_height(f, PRF->energia.fascia_h);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PRF->geo.gap, 0);

    const struct { bool c_e; int32_t wh; const char *eti; lv_color_t col; } Q[4] = {
        { g.prodotto_c_e,  g.prodotto_wh,  tr(TX_ENERGY_PRODUCED),  C_OK   },
        { g.consumato_c_e, g.consumato_wh, tr(TX_ENERGY_CONSUMED), C_TXT  },
        { g.prelevato_c_e, g.prelevato_wh, tr(TX_ENERGY_IMPORTED), C_WARN },
        { g.immesso_c_e,   g.immesso_wh,   tr(TX_ENERGY_EXPORTED), C_ACC  },
    };

    for (int n = 0; n < 4; n++) {
        if (!Q[n].c_e) continue;
        lv_obj_t *k = ui_scheda(f);
        lv_obj_set_flex_grow(k, 1);
        lv_obj_set_height(k, LV_PCT(100));
        lv_obj_set_style_min_width(k, 0, 0);
        lv_obj_set_flex_align(k, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        ui_occhiello(k, Q[n].eti);
        ui_testo(k, ui_energia(Q[n].wh), Q[n].col, FT_L);
    }
}

/* --- costruzione -------------------------------------------------------- */

void schermata_energia(lv_obj_t *c)
{
    const energia_t e = dati_energia();

    static char sotto[48];
    if (!e.disponibile)
        lv_snprintf(sotto, sizeof sotto, "%s", tr(TX_ENERGY_NO_DATA));
    else if (e.produzione_w > ENERGIA_SOGLIA_W)
        lv_snprintf(sotto, sizeof sotto, tr(TX_ENERGY_POWER_FROM_SUN),
                    ui_potenza(e.produzione_w));
    else
        lv_snprintf(sotto, sizeof sotto, "%s", tr(TX_ENERGY_NO_PRODUCTION));
    ui_testata(tr(TX_SECTION_ENERGY), sotto);

    /* L'ordine e il messaggio: in alto quello che sta succedendo **adesso**,
       in fondo quello che e successo **oggi**. Due domande diverse, due
       posti diversi, e nessun numero che significhi due cose. */
    fascia(c);          /* i quattro valori istantanei          */
    banda(c);           /* l'impianto, con la curva della giornata */
    if (PRF->orientamento == VERTICALE) {
        stringhe_griglia(c);/* V e A delle due stringhe             */
        consumi(c);         /* chi sta consumando — e l'unico che scorre */
    } else {
        /* Side by side when the screen is wide. Five stacked blocks do not
           fit in 800 pixels: the strings keep their declared height, and
           the one block meant to scroll — who is using power — was the one
           squeezed, down to a strip where not even its title fitted. Next
           to the strings it gets their full height, and scrolls as the
           specification wants. */
        lv_obj_t *riga = ui_pannello(c, C_BG);
        lv_obj_set_style_bg_opa(riga, LV_OPA_TRANSP, 0);
        lv_obj_set_width(riga, LV_PCT(100));
        lv_obj_set_flex_grow(riga, 1);
        lv_obj_set_style_min_height(riga, 0, 0);
        lv_obj_set_flex_flow(riga, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(riga, PRF->geo.gap, 0);

        lv_obj_t *s = stringhe_griglia(riga);
        if (s) {
            lv_obj_set_width(s, 0);
            lv_obj_set_flex_grow(s, 1);
        }
        lv_obj_t *k = consumi(riga);
        lv_obj_set_width(k, 0);
        lv_obj_set_height(k, LV_PCT(100));
    }
    totali(c);          /* i quattro totali della giornata      */
}
