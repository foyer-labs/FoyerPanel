/* ------------------------------------------------------------------------
 * Prova delle temporizzazioni — 11-collaudo.md §1.
 *
 * Quattro criteri che a mano si verificano solo con un cronometro e molta
 * pazienza:
 *
 *   - il timer di inattivita si azzera a ogni tocco
 *   - dopo 60 s in una sezione si torna alla home, dalla Wi-Fi dopo 120
 *   - standby dopo 120 s, spegnimento dopo 600
 *   - il tocco di risveglio non attiva un comando
 *
 * Qui il tempo lo facciamo scorrere noi con lv_tick_inc, quindi dieci minuti
 * di attesa diventano qualche millisecondo e la prova entra in ctest. Non e
 * un trucco: la macchina a stati legge l'inattivita da LVGL e non sa da dove
 * arrivi il tempo, che e esattamente il motivo per cui e verificabile.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "lvgl.h"
#include "orologio.h"
#include "profile.h"
#include "sezioni.h"
#include "tempi.h"
#include "ui.h"

static int errori;

#define ESIGE(cond, ...)                                                      \
    do {                                                                      \
        printf("  %-4s ", (cond) ? "ok" : "NO");                              \
        printf(__VA_ARGS__);                                                  \
        printf("\n");                                                         \
        if (!(cond)) errori++;                                                \
    } while (0)

static void butta(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    LV_UNUSED(area);
    LV_UNUSED(px);
    lv_display_flush_ready(d);
}

/* Fa passare il tempo senza toccare niente, un secondo per volta come il
   battito della macchina a stati. */
static void aspetta(uint32_t ms)
{
    for (uint32_t passato = 0; passato < ms; passato += 1000) {
        lv_tick_inc(1000);
        lv_timer_handler();
    }
}

/* Un tocco, cioe quello che LVGL registra come attivita, seguito da un
   battito della macchina a stati: e la sequenza che avviene davvero. */
static void tocca(void)
{
    lv_display_trigger_activity(NULL);
    lv_tick_inc(1000);
    lv_timer_handler();
}

/* --- la fascia notturna -------------------------------------------------
 *
 * L'ora si impone con PANNELLO_ORA, la stessa maniglia che usano le catture,
 * e il fuso si fissa a UTC: senza fissarlo la prova direbbe cose diverse a
 * seconda di dove gira, che e' il contrario di una prova.
 *
 * Il caso che conta e' quello che scavalca la mezzanotte. Gli altri ci sono
 * perche' una regola che vale sempre e una che non vale mai passerebbero
 * entrambe un controllo fatto sul solo caso interessante. */
static void ora_finta(int h, int m, int anno)
{
    /* Giorni dal 1970 al 1° gennaio dell'anno chiesto, contando i bisestili
       come li conta il calendario e non come farebbe comodo. */
    long giorni = 0;
    for (int y = 1970; y < anno; y++)
        giorni += ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365;

    static char buf[24];
    snprintf(buf, sizeof buf, "%lld",
             (long long)giorni * 86400LL + (long long)h * 3600LL + m * 60LL);
    setenv("PANNELLO_ORA", buf, 1);
}

static void fascia(const char *da, const char *a)
{
    cfg_imposta_vero("display/night_off/enabled", true);
    cfg_imposta_testo("display/night_off/from", da);
    cfg_imposta_testo("display/night_off/to", a);
}

static void prova_notte(void)
{
    setenv("TZ", "UTC", 1);
    tzset();

    printf("\n--- spegnimento notturno ---\n");

    /* Spento nello schema: qualunque ora, non e' notte. */
    cfg_imposta_vero("display/night_off/enabled", false);
    ora_finta(3, 0, 2026);
    ESIGE(!tempi_e_notte(), "disattivato: le tre di notte non sono notte");

    /* La fascia che scavalca la mezzanotte — il caso normale. */
    fascia("23:00", "07:00");
    ora_finta(23, 30, 2026);
    ESIGE(tempi_e_notte(), "23:30 dentro 23:00-07:00");
    ora_finta(0, 1, 2026);
    ESIGE(tempi_e_notte(), "00:01 dentro 23:00-07:00 (dopo la mezzanotte)");
    ora_finta(6, 59, 2026);
    ESIGE(tempi_e_notte(), "06:59 dentro 23:00-07:00");
    ora_finta(7, 0, 2026);
    ESIGE(!tempi_e_notte(), "07:00 fuori: l'estremo alto non e compreso");
    ora_finta(12, 0, 2026);
    ESIGE(!tempi_e_notte(), "mezzogiorno fuori da 23:00-07:00");
    ora_finta(22, 59, 2026);
    ESIGE(!tempi_e_notte(), "22:59 fuori: l'estremo basso non e ancora");

    /* Una fascia che **non** scavalca, per non farsi ingannare da una
       condizione vera per il motivo sbagliato. */
    fascia("01:00", "05:00");
    ora_finta(3, 0, 2026);
    ESIGE(tempi_e_notte(), "03:00 dentro 01:00-05:00");
    ora_finta(0, 30, 2026);
    ESIGE(!tempi_e_notte(), "00:30 fuori da 01:00-05:00");
    ora_finta(23, 0, 2026);
    ESIGE(!tempi_e_notte(), "23:00 fuori da 01:00-05:00");

    /* Estremi uguali: fascia vuota, non fascia piena. Un pannello che
       leggesse "da 22:00 a 22:00" come "sempre" resterebbe nero per sempre,
       e chi l'ha scritto non capirebbe perche'. */
    fascia("22:00", "22:00");
    ora_finta(22, 0, 2026);
    ESIGE(!tempi_e_notte(), "da e a uguali: fascia vuota, non piena");

    /* L'orologio non ancora sincronizzato: nel 1970 non e mai notte. E la
       differenza fra un pannello che obbedisce e uno che si spegne a ogni
       riavvio diurno. */
    fascia("23:00", "07:00");
    ora_finta(23, 30, 1971);
    ESIGE(!orologio_valido() && !tempi_e_notte(),
          "orologio non sincronizzato: nessuna notte");

    unsetenv("PANNELLO_ORA");
    cfg_imposta_vero("display/night_off/enabled", false);
}

int sim_prova_tempi(void)
{
    lv_display_t *d = lv_display_create((int32_t)PRF->schermo.larghezza,
                                        (int32_t)PRF->schermo.altezza);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    static uint8_t *buf;
    const size_t byte = (size_t)PRF->schermo.larghezza * 64 * sizeof(uint16_t);
    buf = lv_malloc(byte);
    lv_display_set_buffers(d, buf, NULL, (uint32_t)byte,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, butta);
    lv_sysmon_hide_performance(d);
    lv_sysmon_hide_memory(d);

    ui_avvia();
    printf("profilo %s\n", PRF->chiave);

    /* --- le regole speciali spente, mentre si misurano le soglie -------
     *
     * La notte e la casa vuota portano allo schermo nero **prima** dello
     * standby, ed e precisamente quello che devono fare. Ma qui si sta
     * misurando lo standby, e con una configurazione che accende lo
     * spegnimento notturno questa prova passa o fallisce **a seconda
     * dell'ora a cui la si lancia**: e passata tutta la sera e ha
     * cominciato a fallire alle 23:30, quando si e aperta la fascia
     * notturna di casa. Un verde che dipende dall'orologio non vuol dire
     * niente.
     *
     * La notte ha le sue prove, in prova_notte(), a soglie sue. */
    cfg_imposta_vero("display/night_off/enabled", false);
    cfg_imposta_vero("display/off_when_nobody_home", false);

    /* --- ritorno automatico alla home ---------------------------------- */
    ui_vai(SEZ_LUCI);
    tocca();
    aspetta(T_RITORNO_HOME - 5000);
    ESIGE(ui_dove() == SEZ_LUCI,
          "a %d s dalla sezione ci si resta", (T_RITORNO_HOME - 5000) / 1000);
    aspetta(10000);
    ESIGE(ui_dove() == SEZ_HOME,
          "a %d s si torna alla home", T_RITORNO_HOME / 1000);

    /* --- il tocco azzera il conto -------------------------------------- */
    ui_vai(SEZ_CLIMA);
    tocca();
    aspetta(T_RITORNO_HOME - 5000);
    tocca();                       /* proprio prima che scada */
    aspetta(T_RITORNO_HOME - 5000);
    ESIGE(ui_dove() == SEZ_CLIMA,
          "un tocco azzera il conto e la sezione resta aperta");

    /* --- la schermata Wi-Fi aspetta il doppio -------------------------- */
    ui_vai(SEZ_WIFI);
    tocca();
    aspetta(T_RITORNO_HOME + 5000);
    ESIGE(ui_dove() == SEZ_WIFI,
          "dalla Wi-Fi a %d s ci si resta ancora", (T_RITORNO_HOME + 5) / 1000);
    aspetta(T_RITORNO_HOME_WIFI - T_RITORNO_HOME);
    ESIGE(ui_dove() == SEZ_HOME,
          "dalla Wi-Fi si torna a %d s", T_RITORNO_HOME_WIFI / 1000);

    /* --- standby e spegnimento ----------------------------------------- */
    tocca();
    aspetta(T_STANDBY - 5000);
    ESIGE(tempi_stato() == ST_ATTIVO, "prima di %d s niente standby",
          T_STANDBY / 1000);
    aspetta(10000);
    ESIGE(tempi_stato() == ST_STANDBY, "standby a %d s", T_STANDBY / 1000);

    /* Lo standby e scattato qualche secondo dopo la soglia, perche il
       battito e al secondo: si tiene un margine invece di contare sul
       millisecondo. */
    aspetta(T_SPEGNIMENTO - T_STANDBY - 30000);
    ESIGE(tempi_stato() == ST_STANDBY, "prima di %d s lo schermo resta acceso",
          T_SPEGNIMENTO / 1000);
    aspetta(40000);
    ESIGE(tempi_stato() == ST_SPENTO, "schermo spento a %d s",
          T_SPEGNIMENTO / 1000);

    /* --- risveglio ------------------------------------------------------ */
    tocca();
    ESIGE(tempi_stato() == ST_ATTIVO, "il tocco risveglia");
    /* La vista di standby copre tutto e si prende il tocco: e cosi che il
       tocco di risveglio non puo raggiungere un comando sottostante. */
    ESIGE(lv_obj_get_child_count(lv_layer_top()) == 0,
          "risvegliandosi non resta niente sopra l'interfaccia");

    /* --- sospensione ---------------------------------------------------- */
    tempi_sospendi(true);
    aspetta(T_STANDBY + 10000);
    ESIGE(tempi_stato() == ST_ATTIVO,
          "con un modale aperto lo standby e sospeso");
    tempi_sospendi(false);

    prova_notte();

    printf("\n%d criteri di tempo, %d falliti\n", 9 + 11, errori);
    return errori == 0 ? 0 : 1;
}
