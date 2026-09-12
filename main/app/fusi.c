/* ------------------------------------------------------------------------
 * Fusi orari — generato da tools/genera_fusi.py. Non si modifica a mano.
 *
 * ESP-IDF non ha un database dei fusi: setenv("TZ", ...) vuole il formato
 * POSIX, e un nome come "Europe/Rome" non lo capisce. Non da errore: da UTC
 * in silenzio, cioe un orologio sbagliato di un'ora per sei mesi all'anno.
 *
 * Il contratto tiene il nome IANA — leggibile, riconoscibile, lo stesso che
 * usa Home Assistant — e questa tabella lo traduce. I nomi sono esattamente
 * quelli che lo schema propone: proporne uno che il pannello non sa
 * tradurre sarebbe la stessa trappola con un passaggio in piu, e una prova
 * lo impedisce.
 * --------------------------------------------------------------------- */
#include "fusi.h"

#include <string.h>

static const struct { const char *iana; const char *posix; } FUSI[] = {
    { "Europe/Rome",         "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Europe/Paris",        "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Madrid",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Lisbon",       "WET0WEST,M3.5.0/1,M10.5.0" },
    { "Europe/Amsterdam",    "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Brussels",     "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Zurich",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Vienna",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Prague",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Warsaw",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Stockholm",    "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Oslo",         "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Copenhagen",   "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Helsinki",     "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Athens",       "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Bucharest",    "EET-2EEST,M3.5.0/3,M10.5.0/4" },
    { "Europe/Dublin",       "GMT0IST,M3.5.0/1,M10.5.0" },
    { "Europe/Malta",        "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Europe/Moscow",       "MSK-3" },
    { "Atlantic/Canary",     "WET0WEST,M3.5.0/1,M10.5.0" },
    { "UTC",                 "UTC0" },
    { "America/New_York",    "EST5EDT,M3.2.0,M11.1.0" },
    { "America/Chicago",     "CST6CDT,M3.2.0,M11.1.0" },
    { "America/Denver",      "MST7MDT,M3.2.0,M11.1.0" },
    { "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0" },
    { "America/Sao_Paulo",   "<-03>3" },
    { "Asia/Dubai",          "<+04>-4" },
    { "Asia/Jerusalem",      "IST-2IDT,M3.4.4/26,M10.5.0" },
    { "Asia/Kolkata",        "IST-5:30" },
    { "Asia/Shanghai",       "CST-8" },
    { "Asia/Tokyo",          "JST-9" },
    { "Australia/Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3" },
};

const char *fuso_posix(const char *iana)
{
    if (!iana || !*iana) return NULL;

    for (unsigned n = 0; n < sizeof FUSI / sizeof FUSI[0]; n++)
        if (strcmp(FUSI[n].iana, iana) == 0) return FUSI[n].posix;

    /* Chi ha scritto direttamente una stringa POSIX se la tiene: si
       riconosce perche non contiene la barra dei nomi IANA, e passarla
       cosi com'e e piu utile che rifiutarla. Un fuso che questa tabella non
       conosce, invece, torna NULL — e chi chiama deve dirlo, non fingere
       che vada bene. */
    return strchr(iana, '/') ? NULL : iana;
}

int fusi_quanti(void) { return (int)(sizeof FUSI / sizeof FUSI[0]); }

const char *fuso_nome(int n)
{
    return (n >= 0 && n < fusi_quanti()) ? FUSI[n].iana : NULL;
}
