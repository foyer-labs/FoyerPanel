/* ------------------------------------------------------------------------
 * Sorveglianza del collegamento — quale delle tre situazioni mostrare.
 * --------------------------------------------------------------------- */
#include "sorveglianza.h"

#include <inttypes.h>
#include <stdio.h>

#include "dati.h"
#include "ha.h"
#include "i18n.h"
#include "sistema.h"
#include "sezioni.h"
#include "stati.h"
#include "tempi.h"
#include "ui.h"

static situazione_t mostrata = SORV_OK;
static int          tentativo;
static char         ultimo_dato[16] = "—";

/* Dove si era prima che cadesse. Al rientro si torna li: chi stava
   guardando le telecamere quando e caduta la rete sta ancora guardando le
   telecamere. */
static sezione_t dove_ero;
static int       vista_ero;
static bool      so_dove_ero;

bool sorveglianza_dati_vecchi(void)
{
    return mostrata == SORV_RICONNETTO || mostrata == SORV_GIU
        || mostrata == SORV_SENZA_RETE;
}

bool sorveglianza_comandi_sospesi(void)
{
    return mostrata != SORV_OK;
}

/* L'ora dell'ultimo dato valido, che e quello che la fascia mostra. Finche
   l'orologio di rete non c'e, si dice da quanto invece che quando: "3 min
   fa" e un'informazione, "--:--" no. */
static void aggiorna_ultimo_dato(uint32_t eta_ms)
{
    if (eta_ms == UINT32_MAX) {
        snprintf(ultimo_dato, sizeof ultimo_dato, "%s", tr(TX_TIME_NEVER));
        return;
    }
    const uint32_t s = eta_ms / 1000;
    /* %u and an unsigned cast rather than PRIu32: the format now lives in
       a translation, and a translator can type %u but not a macro. */
    if (s < 60) snprintf(ultimo_dato, sizeof ultimo_dato,
                         tr(TX_TIME_SECONDS_AGO), (unsigned)s);
    else        snprintf(ultimo_dato, sizeof ultimo_dato,
                         tr(TX_TIME_MINUTES_AGO), (unsigned)(s / 60));
}

static void ricorda_dove(void)
{
    if (so_dove_ero) return;
    dove_ero = ui_dove();
    vista_ero = ui_vista();
    so_dove_ero = true;
}

static void torna_dove_ero(void)
{
    if (!so_dove_ero) return;
    so_dove_ero = false;
    /* Ridisegnare la stessa destinazione ricostruisce anche i valori, che
       nel frattempo sono arrivati. */
    ui_vai_a(dove_ero, vista_ero);
}

situazione_t sorveglianza_situazione(void) { return mostrata; }

/* Le tre soglie di 01-specifica-ui.md §4, e nient'altro. Separata da chi la
   mette a schermo perche si possa provarla dando i due numeri a mano:
   aspettare sessanta secondi veri per vedere comparire la schermata piena
   non e provarla, e aspettarne quindici per la fascia nemmeno. */
situazione_t sorveglianza_valuta(int ha_stato_corrente, uint32_t eta_ms)
{
    const ha_stato_t s = (ha_stato_t)ha_stato_corrente;

    /* Un token rifiutato non e una caduta: non passa da solo, e va detto
       in un modo diverso. Viene prima di tutto perche l'eta dell'ultimo
       dato, in quel caso, non racconta niente di utile. */
    if (s == HA_TOKEN_RIFIUTATO) return SORV_SENZA_TOKEN;

    /* Oltre il minuto si copre la schermata, qualunque cosa dica lo stato
       del collegamento: un socket aperto che non porta dati per un minuto
       e un collegamento perso, anche se nessuno lo ha dichiarato. */
    if (eta_ms >= T_HA_GIU) return SORV_GIU;

    /* Collegati e con dati recenti: tutto normale. Sotto i quindici
       secondi non si dice niente — una connessione che ascolta puo stare
       zitta, e una fascia ambra a ogni respiro insegna a ignorarla. */
    if (s == HA_PRONTO && eta_ms < T_RICONNESSIONE) return SORV_OK;

    /* Collegati ma silenziosi da un po': si sbiadisce, non si copre. */
    if (s == HA_PRONTO) return SORV_RICONNETTO;

    return SORV_RICONNETTO;
}

bool sorveglianza_rete_persa(bool radio_pronta, bool connessa, bool in_prova,
                             uint32_t senza_rete_ms)
{
    /* Without a radio — the simulator, or a co-processor that did not
       answer — there is no Wi-Fi to announce lost. During a trial the radio
       disconnects on purpose, and if the new network fails the safety net
       brings the old one back by itself. */
    if (!radio_pronta || connessa || in_prova) return false;
    return senza_rete_ms >= T_HA_GIU;
}

void sorveglianza_gira(uint32_t adesso_ms)
{
    /* --- the network first ------------------------------------------------
     *
     * Since when it has been missing: the last time it was there is noted.
     * At power-up it counts from the first loop, so that a panel that never
     * finds the network says so after a minute instead of never. */
    static bool     rete_vista;
    static uint32_t rete_ultima_ms;
    const rete_info_t ri = sistema_rete();
    const bool radio = sistema_radio_pronta();
    if (!rete_vista || ri.connessa || !radio) {
        rete_vista = true;
        rete_ultima_ms = adesso_ms;
    }
    const bool senza_rete = sorveglianza_rete_persa(radio, ri.connessa,
                                                    sistema_rete_in_prova(),
                                                    adesso_ms - rete_ultima_ms);

    /* Senza Home Assistant configurato non c'e niente da sorvegliare: e il
       simulatore che guarda l'interfaccia, non un pannello a muro. The
       Wi-Fi is watched anyway — without a network not even the page that
       configures Home Assistant can be reached. */
    if (!senza_rete && !dati_dal_vero() && mostrata == SORV_OK) return;

    const ha_stato_t s = ha_stato();
    const uint32_t eta = ha_eta_ultimo_dato_ms(adesso_ms);
    const situazione_t voluta = senza_rete         ? SORV_SENZA_RETE
                              : !dati_dal_vero()   ? SORV_OK
                              : sorveglianza_valuta((int)s, eta);

    aggiorna_ultimo_dato(eta);

    /* La fascia di riconnessione conta i tentativi: sapere che ci sta
       provando, e da quanto, e la sola cosa utile mentre si aspetta. */
    if (voluta == SORV_RICONNETTO && mostrata != SORV_RICONNETTO) tentativo = 1;
    else if (voluta == SORV_RICONNETTO && s == HA_CONNETTO
             && mostrata == SORV_RICONNETTO) tentativo++;

    if (voluta == mostrata) {
        /* Anche restando nella stessa situazione, la fascia va rinfrescata:
           il tempo dell'ultimo dato scorre. And the Wi-Fi's reason changes —
           "attempt 3", "network not found" — and is said as it is now. */
        if (voluta == SORV_RICONNETTO) stato_riconnessione(tentativo, ultimo_dato);
        if (voluta == SORV_SENZA_RETE) stato_rete_giu(true, ri.descrizione);
        return;
    }

    /* Leaving the Wi-Fi screen removes it, whatever situation comes next. */
    if (voluta != SORV_SENZA_RETE) stato_rete_giu(false, NULL);

    switch (voluta) {
    case SORV_OK:
        stato_riconnessione(0, NULL);
        stato_ha_giu(false, NULL);
        torna_dove_ero();
        break;

    case SORV_RICONNETTO:
        ricorda_dove();
        stato_ha_giu(false, NULL);
        stato_riconnessione(tentativo, ultimo_dato);
        break;

    case SORV_GIU:
        ricorda_dove();
        stato_riconnessione(0, NULL);
        stato_ha_giu(true, ultimo_dato);
        break;

    case SORV_SENZA_TOKEN:
        /* Non "sto riprovando": non ci sta riprovando nessuno, e non
           servirebbe. Serve un token nuovo, e va detto cosi. */
        ricorda_dove();
        stato_riconnessione(0, NULL);
        stato_ha_giu(true, ha_motivo());
        break;

    case SORV_SENZA_RETE:
        ricorda_dove();
        stato_riconnessione(0, NULL);
        stato_ha_giu(false, NULL);
        stato_rete_giu(true, ri.descrizione);
        break;
    }

    mostrata = voluta;
}
