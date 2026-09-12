/* ------------------------------------------------------------------------
 * Finto fornitore — agenda e condivisione Wi-Fi. Solo Fase 1.
 *
 * L'agenda e spenta perche in Home Assistant non esiste nessuna entita
 * calendar: e lo stato vero dell'impianto. Con --casi agenda si accende, ed
 * e cosi che si verifica una sezione che oggi non si puo vedere.
 * --------------------------------------------------------------------- */
#include "dati_finti.h"

#include <stddef.h>

#include "config.h"
#include "i18n.h"

static const evento_t EVENTI[] = {
    { 0, "15:00", "Parcel delivery",      "Home",               0, true  },
    { 0, "19:30", "Dinner with friends",  "Shared calendar",    1, false },
    { 0, "21:00", "Guitar lesson",        "Calendar 3",         2, false },
    { 1, "08:15", "Team meeting",         "Work",               3, false },
    { 1, "18:00", "Weekly shopping",      "Home",               0, false },
    { 2, NULL,    "Boiler service",       "Home",               0, false },
    { 3, "10:30", "Doctor's appointment", "Home",               0, false },
    { 4, "20:45", "Concert",              "Shared calendar",    1, false },
};
#define N_EVENTI ((int)(sizeof EVENTI / sizeof EVENTI[0]))

static const char *const CALENDARI[] = {
    "Home", "Shared calendar", "Calendar 3", "Work",
};
#define N_CALENDARI ((int)(sizeof CALENDARI / sizeof CALENDARI[0]))

/* The day headings are interface, not data: they speak the panel's
   language. They were Italian words here, and the calendar screen showed
   them in every language. */
static const tx_t GIORNI[] = {
    TX_AGENDA_DAY_TODAY, TX_AGENDA_DAY_TOMORROW, TX_AGENDA_DAY_AFTER_TOMORROW,
    TX_AGENDA_DAY_IN_THREE_DAYS, TX_AGENDA_DAY_REST_OF_WEEK,
};

static const char *TITOLO_LUNGO =
    "Residents' meeting about replacing the boiler and the building's "
    "central heating system";

static evento_t eventi[N_EVENTI];
static caso_t   caso;
static bool     pronto;

void finto_agenda_applica(caso_t c)
{
    caso = c;
    pronto = true;
    for (int n = 0; n < N_EVENTI; n++) {
        eventi[n] = EVENTI[n];
        if (caso == CASO_TITOLI_LUNGHI) eventi[n].titolo = TITOLO_LUNGO;
    }
}

static void assicura(void)
{
    if (!pronto) finto_agenda_applica(CASO_NORMALE);
}

/* Oggi in Home Assistant non c'e nessuna entita calendar: la sezione resta
   nascosta, e non e un difetto ma il comportamento previsto per una sezione
   priva della propria sorgente dati. */
/* Lo dice la configurazione, non il codice: oggi `agenda.attiva` e falso
   perche in Home Assistant non esiste nessuna entita calendar. Nel
   simulatore --casi agenda la accende per poterla guardare. */
bool dati_agenda_attiva(void)
{
    assicura();
    return caso == CASO_AGENDA || cfg_vero("calendar/enabled", false);
}

int dati_eventi_agenda(void) { assicura(); return N_EVENTI; }

const evento_t *dati_evento_agenda(int n)
{
    assicura();
    return (n >= 0 && n < N_EVENTI) ? &eventi[n] : NULL;
}

int dati_calendari(void) { return N_CALENDARI; }

const char *dati_calendario(int n)
{
    return (n >= 0 && n < N_CALENDARI) ? CALENDARI[n] : "";
}

const char *dati_giorno_nome(uint8_t g)
{
    const int quanti = (int)(sizeof GIORNI / sizeof GIORNI[0]);
    return tr(g < quanti ? GIORNI[g] : GIORNI[quanti - 1]);
}

/* --- condivisione Wi-Fi ------------------------------------------------- */

/* --- le reti condivisibili -----------------------------------------------
 *
 * Il segreto di ognuna e una voce distinta dell'elenco chiuso, presa per
 * indice da questa tabella. Comporre il nome della chiave NVS da un numero
 * sarebbe piu corto, e vorrebbe dire che un indice fuori posto scrive dove
 * non deve: e esattamente cio che l'elenco chiuso di segreti.h esiste per
 * impedire.
 *
 * The five keys are wifi_share1_pw … wifi_share5_pw. Until schema 11 the
 * first was wifi_osp_pw, from when that network was «ospiti» (guests):
 * segreti.c moves the old keys to the new ones at boot, so no stored
 * password is lost. */
static const segreto_t SEGRETO_RETE[RETI_MAX] = {
    SEG_WIFI_RETE_1_PASSWORD, SEG_WIFI_RETE_2_PASSWORD,
    SEG_WIFI_RETE_3_PASSWORD, SEG_WIFI_RETE_4_PASSWORD,
    SEG_WIFI_RETE_5_PASSWORD,
};

int dati_reti(void)
{
    assicura();
    const int n = cfg_quanti("wifi_sharing/networks");
    if (n < 0) return 0;
    /* Lo schema si ferma a cinque, ma un file scritto a mano puo arrivare
       qui con dieci reti: senza questo taglio la tabella dei segreti si
       leggerebbe oltre la fine. */
    return n > RETI_MAX ? RETI_MAX : n;
}

rete_t dati_rete(int n)
{
    assicura();
    if (n < 0 || n >= dati_reti()) return (rete_t){ 0 };

    rete_t r = {
        cfg_testo_in("wifi_sharing/networks", n, "display_name", tr(TX_SETTINGS_NETWORK)),
        cfg_testo_in("wifi_sharing/networks", n, "ssid", ""),
        SEGRETO_RETE[n],
        cfg_testo_in("wifi_sharing/networks", n, "security", "WPA"),
        cfg_vero_in("wifi_sharing/networks", n, "hidden", false),
        cfg_vero_in("wifi_sharing/networks", n, "show_password", true),
        cfg_vero_in("wifi_sharing/networks", n, "enabled", true) };

    /* Un nome con spazi ed emoji: il QR deve restare valido, e un carattere
       che il font non copre deve diventare un segnaposto, non un rettangolo
       vuoto (11-collaudo.md §2). */
    if (caso == CASO_SSID_EMOJI && n == 0)
        r.ssid = "Lake house 🏠 guests";
    return r;
}
