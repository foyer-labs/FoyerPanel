/* ------------------------------------------------------------------------
 * Le informazioni di sistema, nel simulatore.
 *
 * Il PC una rete ce l'ha, ma non e **la** rete: il simulatore non si
 * collega a un access point e non ha un indirizzo suo da mostrare. Dirlo e
 * meglio che inventare un indirizzo che somiglia a un dato vero — e
 * inventarlo e esattamente quello che faceva la schermata Impostazioni
 * prima che questo porto esistesse.
 *
 * Vale per tutto il resto: heap e PSRAM di un PC non sono quelli del
 * pannello, e mostrarli darebbe numeri rassicuranti e privi di significato.
 * Zero vuol dire "non lo so", e chi disegna lo mostra come tale.
 * --------------------------------------------------------------------- */
#include "sistema.h"

#include <stdlib.h>
#include <time.h>

rete_info_t sistema_rete(void)
{
    return (rete_info_t){
        .connessa    = false,
        .indirizzo   = "",
        .potenza_dbm = 0,
        .descrizione = "simulatore: nessuna radio",
    };
}

static time_t acceso_da;

void sistema_avvia(void) { acceso_da = time(NULL); }

/* Gli fps del simulatore li decide SDL e la scheda video del PC: non
   dicono niente su come andra il pannello, e un numero che non insegna
   niente e meglio non mostrarlo. */
void sistema_fotogramma(void) { }

sistema_info_t sistema_stato(void)
{
    return (sistema_info_t){
        .accensione_s = acceso_da ? (uint32_t)(time(NULL) - acceso_da) : 0,
        .riavvii      = 0,
        .motivo       = "avvio del simulatore",
        .heap_libero  = 0,
        .heap_minimo  = 0,
        .psram_libera = 0,
        .fps          = 0,
    };
}

void sistema_riavvia(void) { exit(0); }

/* The PC has no radio of its own to bring up. */
bool sistema_radio_pronta(void) { return false; }

const char *sistema_rete_prova(uint32_t *fra_s)
{
    if (fra_s) *fra_s = 0;
    return "";
}

bool sistema_rete_in_prova(void) { return false; }
void sistema_rete_applica(void) { }
void sistema_rete_riprova(void) { }

/* --- reti finte, ma con una forma vera ----------------------------------
 *
 * Sul PC non c'e radio da girare. Questi nomi sono quelli che stavano
 * scritti dentro la schermata del primo avvio finche era un mockup: da qui
 * servono ancora, ma per un'altra ragione — provare che la schermata legge
 * un elenco invece di conoscerlo. Con l'elenco dentro la schermata le due
 * cose erano indistinguibili.
 *
 * La scansione risponde subito, ma **non alla prima domanda**: si finge un
 * giro che dura, se no il caso "sto cercando" non si vedrebbe mai e
 * nessuno si accorgerebbe che quella schermata non lo disegna. */
static const rete_trovata_t FINTE[] = {
    { "CasaEsempio",         -48, true  },
    { "CasaEsempio-Ospiti",  -51, true  },
    { "FRITZ!Box 7530 QF",   -67, true  },
    { "TIM-19384756",        -74, true  },
    { "Vodafone-Casa-2G",    -81, true  },
    { "Rete aperta bar",     -88, false },
};
#define FINTE_N ((int)(sizeof FINTE / sizeof FINTE[0]))

static int  giri_mancanti;
static bool pronte;

bool sistema_reti_cerca(void)
{
    giri_mancanti = 3;
    pronte = false;
    return true;
}

/* Il conto scende **solo qui**, e solo finche non e finito.
   sistema_reti_quante() legge e basta: una domanda che cambia lo stato di
   chi la riceve rende il risultato diverso a seconda di quante volte la si
   fa, e il primo a inciamparci e stato chi ha scritto la prova. */
bool sistema_reti_pronte(void)
{
    if (!pronte && giri_mancanti > 0 && --giri_mancanti == 0) pronte = true;
    return pronte;
}

int sistema_reti_quante(void) { return pronte ? FINTE_N : 0; }

rete_trovata_t sistema_rete_trovata(int n)
{
    if (!pronte || n < 0 || n >= FINTE_N) return (rete_trovata_t){ "", 0, false };
    return FINTE[n];
}
