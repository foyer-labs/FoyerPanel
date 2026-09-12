#include "tempi.h"

#include <stdio.h>

#include "comuni.h"
#include "orologio.h"
#include "config.h"
#include "dati.h"
#include "vista_standby.h"
#include "ui.h"

static stato_t   stato = ST_AVVIO;
static bool      sospeso;
static lv_obj_t *velo;        /* la vista di standby, sopra tutto */
static lv_obj_t *contenuto;   /* cio che trasla per lo spostamento pixel */
static lv_timer_t *battito;
static uint8_t   passo_pixel;
static uint32_t  prossimo_spostamento;   /* ms di inattivita al passo dopo */

/* --- vista di standby — 01-specifica-ui.md §4.7 ------------------------- */

/* Sette posizioni entro l'escursione del profilo, lungo un ciclo chiuso.
   Il ciclo si chiude perche altrimenti, dopo abbastanza ore, il contenuto
   si troverebbe sempre dalla stessa parte e la ritenzione tornerebbe. */
static const int8_t CICLO[7][2] = {
    { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { 0, -1 },
};

static void sposta_pixel(void)
{
    if (!contenuto) return;
    passo_pixel = (uint8_t)((passo_pixel + 1) % (sizeof CICLO / sizeof CICLO[0]));
    lv_obj_set_style_translate_x(
        contenuto, CICLO[passo_pixel][0] * PRF->comp.standby_shift_x, 0);
    lv_obj_set_style_translate_y(
        contenuto, CICLO[passo_pixel][1] * PRF->comp.standby_shift_y, 0);
}

static void su_tocco_velo(lv_event_t *e)
{
    LV_UNUSED(e);
    tempi_risveglia();
}

static void apri_standby(void)
{
    if (velo) return;

    /* Nero vero e non C_BG: il fondo del tema e un grigio molto scuro, e di
       notte in un corridoio buio si legge come un rettangolo debolmente
       illuminato. Qui non c'e niente da separare da niente — e l'unica
       schermata senza schede sopra — quindi il nero non toglie struttura,
       toglie solo luce. */
    velo = ui_pannello(lv_layer_top(), C_NERO);
    lv_obj_set_size(velo, LV_PCT(100), LV_PCT(100));
    /* Copre tutto e si prende il tocco: il tocco di risveglio non deve mai
       arrivare a un comando sotto (11-collaudo.md §1). */
    lv_obj_add_flag(velo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(velo, su_tocco_velo, LV_EVENT_PRESSED, NULL);

    contenuto = ui_pannello(velo, C_NERO);
    lv_obj_set_size(contenuto, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_opa(contenuto, ui_opa(COM.opacita_standby_pct), 0);
    lv_obj_set_flex_flow(contenuto, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(contenuto, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(contenuto, PRF->geo.gap, 0);
    /* Margine interno oltre l'escursione massima, per lato: senza, la
       traslazione taglierebbe il bordo del contenuto. */
    lv_obj_set_style_pad_all(contenuto,
                             PRF->geo.pad + PRF->comp.standby_shift_x + COM.bordo, 0);

    vista_standby_costruisci(contenuto);

    passo_pixel = 0;
    prossimo_spostamento = tempi_standby_ms() + T_SPOSTAMENTO_PIXEL;
    stato = ST_STANDBY;
}

/* Come in stati.c: `velo` e un puntatore che teniamo noi e la cui vita puo
   finire altrove. lv_obj_is_valid() e l'idioma di LVGL per non cancellare
   due volte lo stesso oggetto. */
static void cancella_velo(void)
{
    if (velo && lv_obj_is_valid(velo)) lv_obj_delete(velo);
    velo = NULL;
    contenuto = NULL;
    vista_standby_dimentica();
}

static void spegni_schermo(void)
{
    if (!velo) apri_standby();
    /* Sul pannello qui si spegne la retroilluminazione; nel simulatore
       resta solo il nero, che e cio che si vedrebbe. */
    lv_obj_set_style_opa(contenuto, LV_OPA_TRANSP, 0);
    stato = ST_SPENTO;
}

void tempi_forza_standby(void)
{
    apri_standby();
}

void tempi_rifai_standby(void)
{
    /* Non aperta: si costruira gia nuova la prossima volta. */
    if (!velo) return;

    /* Lo schermo spento e uno standby con il contenuto trasparente: si
       ricostruisce e si rimette com'era, se no salvare la configurazione di
       notte accenderebbe il muro. */
    const bool era_spento = stato == ST_SPENTO;
    cancella_velo();
    apri_standby();
    if (era_spento) {
        lv_obj_set_style_opa(contenuto, LV_OPA_TRANSP, 0);
        stato = ST_SPENTO;
    }
}

void tempi_risveglia(void)
{
    cancella_velo();
    stato = ST_ATTIVO;
    lv_display_trigger_activity(NULL);
}

/* --- battito ------------------------------------------------------------ */

/* --- le soglie, lette e non compilate ----------------------------------
 *
 * Si rileggono a ogni battito invece di essere prese una volta all'avvio.
 * Costa quattro discese in un albero JSON al secondo — niente, in confronto
 * al ridisegno che sta accanto — e in cambio una modifica dalla pagina vale
 * subito, che e' l'unico modo per cui uno possa regolare lo standby stando
 * davanti al pannello e vedendo cosa succede. */
static uint32_t secondi_cfg(const char *percorso, uint32_t ripiego_ms)
{
    const int32_t s = cfg_intero(percorso, (int32_t)(ripiego_ms / 1000));
    return s > 0 ? (uint32_t)s * 1000 : 0;
}

uint32_t tempi_standby_ms(void)
{
    /* Zero qui non ha senso — vorrebbe dire standby immediato, e nessuno lo
       vuole — quindi il ripiego copre anche quel caso. */
    const uint32_t v = secondi_cfg("display/standby_after_s", T_STANDBY);
    return v ? v : T_STANDBY;
}

/* Zero vuol dire **mai**: chi non vuole che lo schermo si spenga scrive
   zero, e lo schema lo permette apposta (minimo 0). Chi legge deve
   ricordarsene, e per questo il controllo sta scritto dove si usa. */
uint32_t tempi_spegnimento_ms(void)
{
    return secondi_cfg("display/off_after_s", T_SPEGNIMENTO);
}

static uint32_t soglia_ritorno(void)
{
    /* Sulla schermata Wi-Fi il ritorno e piu lento: chi sta inquadrando il
       QR col telefono non tocca lo schermo (01-specifica-ui.md §4.8). */
    if (ui_dove() == SEZ_WIFI)
        return secondi_cfg("wifi_sharing/return_home_after_s",
                           T_RITORNO_HOME_WIFI);
    return secondi_cfg("display/return_home_after_s", T_RITORNO_HOME);
}

/* --- la notte -----------------------------------------------------------
 *
 * Una fascia oraria in cui lo schermo sta spento come se la casa fosse
 * vuota. Nello schema c'era da sempre — `attivo`, `da`, `a` — e nel
 * firmware non esisteva: si impostava e non succedeva niente.
 *
 * Vale la stessa regola della casa vuota, e per la stessa ragione: **un
 * tocco riaccende**, e si torna al nero solo quando scade l'inattivita
 * normale. Uno schermo che si rispegnesse in faccia a chi l'ha appena
 * toccato alle due di notte sarebbe peggio di uno sempre acceso.
 *
 * La fascia puo scavalcare la mezzanotte, che e' anzi il caso normale:
 * 23:00-07:00 e' due intervalli, non uno, e trattarla come un confronto
 * semplice la renderebbe vera per sedici ore invece che per otto. */
static int minuti_da_testo(const char *hhmm, int ripiego)
{
    if (!hhmm || !hhmm[0]) return ripiego;
    int h = 0, m = 0;
    if (sscanf(hhmm, "%d:%d", &h, &m) != 2) return ripiego;
    if (h < 0 || h > 23 || m < 0 || m > 59) return ripiego;
    return h * 60 + m;
}

bool tempi_e_notte(void)
{
    if (!cfg_vero("display/night_off/enabled", false)) return false;

    /* Senza ora credibile non si spegne niente: vedi orologio_minuti(). */
    const int ora = orologio_minuti();
    if (ora < 0) return false;

    const int da = minuti_da_testo(cfg_testo("display/night_off/from",
                                             "23:00"), 23 * 60);
    const int a  = minuti_da_testo(cfg_testo("display/night_off/to",
                                             "07:00"), 7 * 60);
    if (da == a) return false;             /* fascia vuota, non fascia piena */
    if (da < a)  return ora >= da && ora < a;
    return ora >= da || ora < a;           /* scavalca la mezzanotte */
}

/* --- la casa vuota ------------------------------------------------------
 *
 * Uno schermo acceso in una casa senza nessuno non serve a niente, ed e
 * l'unica occasione in cui si puo spegnere davvero senza togliere niente a
 * nessuno: le altre soglie sono compromessi fra risparmio e comodita, questa
 * no.
 *
 * Si spegne solo se la presenza e **nota**. Senza entita `person`
 * configurate, o con Home Assistant non ancora collegato, dati_presenza_nota()
 * e falsa e qui non succede niente: un pannello che si spegne perche' non sa
 * chi c'e in casa sembrerebbe rotto, e chi lo guarda non avrebbe modo di
 * capire che sta obbedendo a una spunta.
 *
 * Al ritorno si riaccende sullo standby e non sulla home: chi entra dalla
 * porta guarda l'ora e la temperatura, non la sezione dove qualcuno era
 * rimasto tre giorni prima. Da li in poi valgono le soglie di sempre. */
static bool era_vuota;

static bool casa_vuota(void)
{
    if (!cfg_vero("display/off_when_nobody_home", false)) return false;
    if (!dati_presenza_nota()) return false;
    return dati_persone_in_casa() == 0;
}

static void batti(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (sospeso) return;

    const uint32_t fermo = lv_display_get_inactive_time(NULL);

    /* --- la casa vuota, e il tocco che vale quanto vale sempre ----------
     *
     * Chi arriva riaccende: e una transizione, si guarda una volta sola.
     *
     * Chi non c'e fa spegnere, ma **non all'istante**: solo quando scade
     * l'inattivita normale, la stessa che porterebbe allo standby. La casa
     * vuota non cambia la regola del tocco, accorcia la strada verso il nero
     * — al posto della soglia lunga dello spegnimento.
     *
     * Prima si spegneva subito, e un tocco riaccendeva per **un secondo**:
     * al battito dopo la casa era ancora vuota e lo schermo tornava nero in
     * faccia a chi lo stava guardando. Era il contrario di quello che la
     * specifica chiede — «puo essere riacceso col tocco» — e si vedeva solo
     * provando con la casa davvero vuota. */
    /* Due ragioni diverse per lo stesso nero, e la stessa regola per
       uscirne: nessuno in casa, oppure la fascia notturna. */
    const bool vuota = casa_vuota() || tempi_e_notte();

    if (!vuota && era_vuota) {
        /* apri_standby() rifa la vista e rimette l'opacita che lo
           spegnimento aveva tolto; il conto dell'inattivita riparte da
           adesso, se no la soglia sarebbe gia scaduta e si tornerebbe
           subito al nero. */
        cancella_velo();
        lv_display_trigger_activity(NULL);
        apri_standby();
    }
    era_vuota = vuota;

    if (vuota && stato != ST_SPENTO && fermo >= tempi_standby_ms())
        spegni_schermo();

    switch (stato) {
    case ST_AVVIO:
        break;

    case ST_ATTIVO:
        /* Prima il ritorno alla home, poi lo standby. L'ordine conta sulla
           schermata Wi-Fi, dove le due soglie coincidono a 120 s: li si
           torna prima a casa e lo standby arriva subito dopo, invece di
           lasciare il codice QR sotto la vista di standby. */
        if (ui_dove() != SEZ_HOME && fermo >= soglia_ritorno()) {
            ui_vai(SEZ_HOME);
        } else if (fermo >= tempi_standby_ms()) {
            apri_standby();
        }
        break;

    case ST_STANDBY:
        vista_standby_aggiorna();
        /* Qualunque attivita risveglia, non solo il tocco sul velo: se
           qualcosa registra un'interazione mentre siamo in standby, restarci
           sarebbe uno stato che non corrisponde a quello che sta succedendo. */
        if (fermo < tempi_standby_ms()) {
            tempi_risveglia();
        } else if (tempi_spegnimento_ms() &&
                   fermo >= tempi_spegnimento_ms()) {
            spegni_schermo();
        } else if (fermo >= prossimo_spostamento) {
            sposta_pixel();
            prossimo_spostamento += T_SPOSTAMENTO_PIXEL;
        }
        break;

    case ST_SPENTO:
        /* Anche da schermo spento: il primo tocco riaccende, ed e l'unica
           cosa che deve fare. */
        if (fermo < T_STANDBY) tempi_risveglia();
        break;
    }
}

void tempi_avvia(void)
{
    stato = ST_ATTIVO;
    lv_display_trigger_activity(NULL);
    if (!battito) battito = lv_timer_create(batti, 1000, NULL);
}

stato_t tempi_stato(void) { return stato; }

uint32_t tempi_fermo_ms(void) { return lv_display_get_inactive_time(NULL); }

void tempi_sospendi(bool si)
{
    /* --- solo sul passaggio vero, non a ogni chiamata -------------------
     *
     * Riprendere da una sospensione **e** un gesto di qualcuno: chi ha appena
     * chiuso un modale ha passato del tempo a leggerlo, e mandarlo in standby
     * un istante dopo sarebbe assurdo. Per questo qui si dichiara attivita.
     *
     * Ma questa funzione la chiama anche stati_chiudi(), che a sua volta e
     * chiamata da ui_vai_a() a **ogni** ricostruzione della schermata — e da
     * quando le schermate si rifanno da sole quando cambiano i dati, quella
     * era una dichiarazione di attivita ogni volta che una luce si accendeva
     * in un'altra stanza. L'orologio dell'inattivita non arrivava mai ai due
     * minuti e lo standby non compariva piu.
     *
     * Distinguere e semplice: si sveglia solo chi era davvero sospeso. Una
     * chiamata a "riprendi" quando non c'era niente da riprendere non e un
     * gesto di nessuno. */
    if (sospeso == si) return;
    sospeso = si;
    if (!si) lv_display_trigger_activity(NULL);
}
