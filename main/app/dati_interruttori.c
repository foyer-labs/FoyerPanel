/* ------------------------------------------------------------------------
 * Interruttori — quelli che non appartengono a nessun altro elenco.
 *
 * Prese, luci smart, scaldini, pompe: roba che si accende e si spegne e
 * basta. Le luci di casa hanno una sezione loro perche hanno la
 * luminosita e le scene; qui c'e cio che ha due stati e nient'altro, e
 * proprio per questo non meritava di essere modellato due volte.
 *
 * **Il comando lo decide il dominio dell'entita, non la configurazione.**
 * `switch.` vuole `switch.turn_on`, `light.` vuole `light.turn_on`,
 * `input_boolean.` il suo: chiedere anche il servizio a chi configura
 * sarebbe chiedere due volte la stessa cosa, e la seconda volta si puo
 * sbagliare. Si scrive l'entita e basta.
 *
 * L'assorbimento e facoltativo. Quando c'e, e un sensore a parte — la
 * presa che comanda e quella che misura sono lo stesso apparecchio ma due
 * entita — e quando non c'e la riga non lascia il posto vuoto: si stringe.
 * --------------------------------------------------------------------- */
#include "dati.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "dati_finti.h"
#include "entita.h"
#include "ha.h"

#define INTERRUTTORI_MAX 24

static interruttore_t elenco[INTERRUTTORI_MAX];
static int            quanti;

/* Il dominio, cioe cio che sta prima del punto. Torna la lunghezza, zero se
   quel punto non c'e: un identificatore senza dominio non e comandabile e
   la voce lo dira invece di mandare un servizio inventato. */
static size_t dominio(const char *entita, char *buf, size_t n)
{
    const char *punto = entita ? strchr(entita, '.') : NULL;
    if (!punto || punto == entita) return 0;

    const size_t l = (size_t)(punto - entita);
    if (l >= n) return 0;
    memcpy(buf, entita, l);
    buf[l] = 0;
    return l;
}

/* --- i valori del mockup, per quando non c'e nessuna casa che parli ------
 *
 * Tre accesi e il resto spento, e due assorbimenti su cinque: sono i casi
 * che la riga deve saper disegnare — con l'assorbimento e senza, acceso e
 * spento. Sul pannello vero non si vedono mai. */
static void interruttori_finti(void)
{
    static const struct { bool acceso; bool watt_c_e; int32_t watt; } F[] = {
        { true,  true,  118 }, { false, true,   0 }, { true,  true, 640 },
        { false, false,   0 }, { true,  false,   0 },
    };
    for (int n = 0; n < quanti; n++) {
        const int i = n % (int)(sizeof F / sizeof F[0]);
        elenco[n].disponibile = true;
        elenco[n].acceso      = F[i].acceso;
        elenco[n].potenza_c_e = F[i].watt_c_e;
        elenco[n].watt        = F[i].watt;
    }
}

static void leggi(void)
{
    quanti = cfg_quanti("switches");
    if (quanti > INTERRUTTORI_MAX) quanti = INTERRUTTORI_MAX;

    for (int n = 0; n < quanti; n++) {
        interruttore_t *i = &elenco[n];
        i->nome   = cfg_testo_in("switches", n, "name", "");
        i->entita = cfg_testo_in("switches", n, "entity", "");

        /* Il nome, non il glifo: qui non si sa cosa sia un glifo. Chi
           disegna lo traduce, e a chi non ha scelto un'icona da una presa. */
        i->icona = cfg_testo_in("switches", n, "icon", "");

        i->disponibile = i->entita[0] && ent_vista(i->entita)
                         && ent_disponibile(i->entita);
        i->acceso = i->disponibile && ent_stato_e(i->entita, "on");

        const char *pw = cfg_testo_in("switches", n, "power", "");
        i->potenza_c_e = pw[0] && ent_vista(pw) && ent_disponibile(pw);
        if (i->potenza_c_e) {
            /* L'unita la dichiara Home Assistant e non si assume mai: un
               sensore in kilowatt letto come watt fa sembrare una presa una
               fonderia. Stessa regola dell'energia e degli elettrodomestici. */
            const char *u = ent_attributo(pw, "unit_of_measurement", "W");
            const int fattore = (u[0] == 'k' || u[0] == 'K') ? 1000 : 1;
            i->watt = (int32_t)(ent_numero(pw, 0) * fattore);
        } else {
            i->watt = 0;
        }
    }

    if (!dati_dal_vero()) interruttori_finti();
}

int dati_interruttori(void)
{
    leggi();
    return quanti;
}

const interruttore_t *dati_interruttore(int n)
{
    if (n < 0 || n >= quanti) return NULL;
    return &elenco[n];
}

bool dati_interruttore_premi(int n, bool acceso)
{
    const interruttore_t *i = dati_interruttore(n);
    if (!i || !i->disponibile) return false;
    if (!dati_dal_vero()) return false;

    char dom[24];
    if (!dominio(i->entita, dom, sizeof dom)) return false;

    return ha_chiama(dom, acceso ? "turn_on" : "turn_off", i->entita, NULL);
}
