#include "sezioni.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "dati.h"
#include "icons/icons.h"

static const sezione_info_t INFO[] = {
    { SEZ_LUCI,         "lights",     TX_SECTION_LIGHTS,       ICO_LIGHTBULB         },
    { SEZ_INTERRUTTORI, "switches", TX_SECTION_SWITCHES,  ICO_OUTLET            },
    { SEZ_CLIMA,        "climate",    TX_SECTION_CLIMATE,      ICO_DEVICE_THERMOSTAT },
    { SEZ_ENERGIA,      "energy",     TX_SECTION_ENERGY,    ICO_SOLAR_POWER       },
    { SEZ_ACCESSI,      "access",     TX_SECTION_ACCESS,    ICO_GARAGE            },
    { SEZ_PROGRAMMAZIONI, "schedules", TX_SECTION_SCHEDULES,  ICO_SCHEDULE          },
    { SEZ_AGENDA,       "calendar",   TX_SECTION_AGENDA,     ICO_CALENDAR_MONTH    },
    { SEZ_WIFI,         "wifi",       TX_SECTION_WIFI,      ICO_WIFI              },
    { SEZ_ROBOT,        "robot",      TX_SECTION_ROBOT,      ICO_MOP               },
    { SEZ_HOME,         "home",       TX_SECTION_HOME,       ICO_HOME              },
    { SEZ_IMPOSTAZIONI, "settings",   TX_SECTION_SETTINGS,    ICO_SETTINGS          },
    { SEZ_DIAGNOSTICA,  "system",     TX_SECTION_SYSTEM,    ICO_MEMORY            },
};

/* Se la configurazione non si e letta si mostra tutto: e meglio di una
   navigazione vuota, e comunque si finisce al primo avvio. */
static const sezione_t TUTTE[] = {
    SEZ_LUCI, SEZ_INTERRUTTORI, SEZ_CLIMA, SEZ_ENERGIA,
    SEZ_ACCESSI, SEZ_PROGRAMMAZIONI, SEZ_AGENDA, SEZ_WIFI, SEZ_ROBOT,
};

static sezione_t elenco[SEZ_QUANTE];
static int       quante;

const sezione_info_t *sezione(sezione_t s)
{
    for (unsigned n = 0; n < sizeof INFO / sizeof INFO[0]; n++)
        if (INFO[n].id == s) return &INFO[n];
    return &INFO[0];
}

sezione_t sezione_da_chiave(const char *chiave)
{
    if (!chiave) return SEZ_HOME;
    for (unsigned n = 0; n < sizeof INFO / sizeof INFO[0]; n++) {
        const char *a = INFO[n].chiave, *b = chiave;
        while (*a && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) return INFO[n].id;
    }
    return SEZ_HOME;
}

/* Una sezione senza la propria sorgente dati non compare, e vale anche se
   qualcuno la elenca in `sezioni`: e la regola generale di
   03-config-contratto.md §4.4. Oggi tocca all'agenda, che non ha nessuna
   entita calendar, e alla condivisione Wi-Fi, che senza una rete ospiti da
   mostrare non avrebbe niente da far vedere (01-specifica-ui.md §3.7).
   Quando la sorgente compare, la sezione entra da sola. */
static bool qualche_rete(void)
{
    for (int n = 0; n < dati_reti(); n++)
        if (dati_rete(n).attiva) return true;
    return false;
}

static bool ha_la_sua_sorgente(sezione_t s)
{
    switch (s) {
    case SEZ_AGENDA: return dati_agenda_attiva();
    case SEZ_WIFI:   return qualche_rete();
    default:         return true;
    }
}

/* Quali sezioni, e in che ordine, lo dice `sezioni` di config.json. Una
   sezione assente non compare nel rail ne nel dock (03-config-contratto.md
   §4.2), ed e cosi che oggi si esclude l'agenda: non c'e una riga di codice
   che la nasconda, non e nell'array.

   L'ordine conta anche per il dock, che quando le sezioni sono piu dei
   pulsanti mostra le prime (09-profili.md §6). */
static void assicura(void)
{
    if (quante) return;

    sezione_t elenco_iniziale[SEZ_QUANTE];
    int n = 0;

    const int quante_cfg = cfg_quanti("sections");
    for (int k = 0; k < quante_cfg && n < SEZ_QUANTE; k++) {
        char percorso[24];
        snprintf(percorso, sizeof percorso, "sections/%d", k);
        const char *chiave = cfg_testo(percorso, "");
        const sezione_t s = sezione_da_chiave(chiave);
        /* sezione_da_chiave torna SEZ_HOME per cio che non riconosce, e la
           home non e una sezione: si scarta. */
        if (s == SEZ_HOME) continue;
        if (!ha_la_sua_sorgente(s)) continue;
        elenco_iniziale[n++] = s;
    }

    if (n == 0)
        for (unsigned k = 0; k < sizeof TUTTE / sizeof TUTTE[0]; k++)
            if (ha_la_sua_sorgente(TUTTE[k]))
                elenco_iniziale[n++] = TUTTE[k];

    sezioni_imposta(elenco_iniziale, n);
}

void sezioni_azzera(void) { quante = 0; }

void sezioni_imposta(const sezione_t *nuove, int n)
{
    if (n <= 0) {
        quante = 0;
        assicura();
        return;
    }
    if (n > SEZ_QUANTE) n = SEZ_QUANTE;
    for (int i = 0; i < n; i++) elenco[i] = nuove[i];
    quante = n;
}

int sezioni_attive(void)
{
    assicura();
    return quante;
}

sezione_t sezione_attiva(int n)
{
    assicura();
    return (n >= 0 && n < quante) ? elenco[n] : SEZ_HOME;
}

bool sezione_e_attiva(sezione_t s)
{
    assicura();
    for (int n = 0; n < quante; n++)
        if (elenco[n] == s) return true;
    return false;
}
