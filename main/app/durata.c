/* ------------------------------------------------------------------------
 * Quanto manca, detto in ore — vedi durata.h.
 * --------------------------------------------------------------------- */
#include "durata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Quante ore vale un'unità, o zero se non è un tempo.
 *
 * I nomi sono quelli che Home Assistant usa davvero in
 * `unit_of_measurement`, brevi e per esteso, più le due forme italiane che
 * capita di trovare su entità scritte a mano in un `template`. Un elenco e
 * non un prefisso: `min` comincia per `m` come `mese`, e un confronto sul
 * primo carattere trasformerebbe i minuti in qualcos'altro. */
static double ore_per(const char *unita)
{
    static const struct { const char *nome; double ore; } U[] = {
        { "s", 1.0 / 3600.0 }, { "sec", 1.0 / 3600.0 },
        { "second", 1.0 / 3600.0 }, { "seconds", 1.0 / 3600.0 },
        { "secondi", 1.0 / 3600.0 },
        { "min", 1.0 / 60.0 }, { "minute", 1.0 / 60.0 },
        { "minutes", 1.0 / 60.0 }, { "minuti", 1.0 / 60.0 },
        { "h", 1.0 }, { "hr", 1.0 }, { "hour", 1.0 }, { "hours", 1.0 },
        { "ora", 1.0 }, { "ore", 1.0 },
        { "d", 24.0 }, { "day", 24.0 }, { "days", 24.0 },
        { "giorno", 24.0 }, { "giorni", 24.0 },
    };
    if (!unita) return 0.0;
    for (unsigned n = 0; n < sizeof U / sizeof U[0]; n++)
        if (strcmp(unita, U[n].nome) == 0) return U[n].ore;
    return 0.0;
}

bool durata_in_ore(const char *stato, const char *unita, double *ore)
{
    if (!stato || !*stato) return false;

    const double fattore = ore_per(unita);
    if (fattore <= 0.0) return false;

    char *fine = NULL;
    const double v = strtod(stato, &fine);
    /* `fine == stato` vuol dire che non ha letto niente: uno stato come
       «unknown» darebbe zero, e zero è proprio il valore che fa scattare
       l'allarme di una soglia. */
    if (fine == stato) return false;

    if (ore) *ore = v * fattore;
    return true;
}

void durata_testo(const char *stato, const char *unita, char *fuori, size_t max)
{
    if (!fuori || !max) return;
    if (!stato) stato = "";

    double ore = 0.0;
    if (durata_in_ore(stato, unita, &ore)) {
        if (ore < 0.0) ore = 0.0;
        if (ore >= 1.0) snprintf(fuori, max, "%.0f h", ore);
        else            snprintf(fuori, max, "<1 h");
        return;
    }

    /* Non è un tempo: si mostra com'è, con la sua unità. Inventare le ore
       da una percentuale sarebbe la cosa peggiore — un numero plausibile e
       falso, che nessuno andrebbe a controllare. */
    snprintf(fuori, max, "%s%s%s", stato,
             (unita && *unita) ? " " : "", (unita && *unita) ? unita : "");
}
