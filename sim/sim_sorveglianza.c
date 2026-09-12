/* ------------------------------------------------------------------------
 * Prova delle tre soglie del collegamento — 11-collaudo.md §1, Fase 3.
 *
 * Le soglie sono 15 e 60 secondi. Provarle aspettandole vorrebbe dire un
 * minuto e un quarto per ogni caso, e nessuno le proverebbe piu di una
 * volta. Qui si danno i due numeri a mano — stato del collegamento ed eta
 * dell'ultimo dato — e si guarda cosa decide.
 *
 * Quello che si sta verificando non e un timer ma **una scelta fra quattro
 * modi di dire la stessa notizia**, e sbagliarla si vede solo sul muro:
 * una fascia ambra a ogni respiro insegna a ignorarla, una schermata piena
 * dopo tre secondi rende il pannello inservibile, e un "sto riprovando" su
 * un token rifiutato manda a cercare un guasto di rete che non c'e.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>

#include "ha.h"
#include "sorveglianza.h"

static int falliti;

static const char *nome(situazione_t s)
{
    switch (s) {
    case SORV_OK:          return "normale";
    case SORV_RICONNETTO:  return "riconnessione";
    case SORV_GIU:         return "irraggiungibile";
    case SORV_SENZA_TOKEN: return "serve un token";
    case SORV_SENZA_RETE:  return "no network";
    }
    return "?";
}

static void caso(const char *cosa, ha_stato_t stato, uint32_t eta_ms,
                 situazione_t voluta)
{
    const situazione_t avuta = sorveglianza_valuta((int)stato, eta_ms);
    const bool ok = avuta == voluta;
    printf("%-56s %s\n", cosa, ok ? "ok" : "FALLITA");
    if (!ok) {
        printf("    voluto %s, avuto %s\n", nome(voluta), nome(avuta));
        falliti++;
    }
}

static void rete(const char *cosa, bool radio, bool connessa, bool in_prova,
                 uint32_t senza_ms, bool voluta)
{
    const bool avuta = sorveglianza_rete_persa(radio, connessa, in_prova,
                                               senza_ms);
    const bool ok = avuta == voluta;
    printf("%-56s %s\n", cosa, ok ? "ok" : "FALLITA");
    if (!ok) falliti++;
}

int sim_prova_sorveglianza(void)
{
    printf("--- soglie del collegamento ---\n");

    /* Sotto i quindici secondi non si dice niente. Una connessione
       WebSocket che ascolta puo stare zitta: e un pannello che aspetta gli
       eventi, non uno che interroga. */
    caso("appena collegati, tutto normale", HA_PRONTO, 0, SORV_OK);
    caso("dopo dieci secondi di silenzio, ancora normale",
         HA_PRONTO, 10000, SORV_OK);
    caso("a quattordici secondi e novecento, ancora normale",
         HA_PRONTO, 14900, SORV_OK);

    /* Da quindici a sessanta: fascia, valori sbiaditi, comandi sospesi. */
    caso("a quindici secondi esatti compare la fascia",
         HA_PRONTO, 15000, SORV_RICONNETTO);
    caso("a trenta secondi la fascia resta", HA_PRONTO, 30000, SORV_RICONNETTO);
    caso("a cinquantanove secondi ancora fascia",
         HA_PRONTO, 59000, SORV_RICONNETTO);

    /* Oltre il minuto la schermata si copre, e vale anche se il socket
       risulta ancora aperto: un collegamento che non porta dati per un
       minuto e perso, che qualcuno lo abbia dichiarato o no. */
    caso("a sessanta secondi la schermata si copre",
         HA_PRONTO, 60000, SORV_GIU);
    caso("e a cinque minuti resta coperta", HA_PRONTO, 300000, SORV_GIU);

    /* Mentre si sta ancora collegando non si copre niente: e la situazione
       normale dei primi istanti dopo l'accensione. */
    caso("mentre collega, fascia e non schermata piena",
         HA_CONNETTO, 1000, SORV_RICONNETTO);
    caso("caduto da poco, fascia", HA_CADUTO, 20000, SORV_RICONNETTO);
    caso("caduto da un minuto, schermata piena",
         HA_CADUTO, 61000, SORV_GIU);

    /* Il token rifiutato viene prima di tutto: l'eta dell'ultimo dato, in
       quel caso, non racconta niente di utile. */
    caso("token rifiutato, appena successo",
         HA_TOKEN_RIFIUTATO, 0, SORV_SENZA_TOKEN);
    caso("token rifiutato da mezz'ora, sempre lo stesso messaggio",
         HA_TOKEN_RIFIUTATO, 1800000, SORV_SENZA_TOKEN);

    /* Mai arrivato niente: eta infinita, e non e "appena aggiornato". */
    caso("mai arrivato nessun dato", HA_CONNETTO, UINT32_MAX, SORV_GIU);

    printf("\n--- the Wi-Fi ---\n");
    /* radio, connected, on trial, how long without a network */
    rete("connected: nothing to say", true, true, false, 0, false);
    rete("off for half a minute: nothing yet", true, false, false,
         30000, false);
    rete("off for a minute: said, and the glass offered",
         true, false, false, 60000, true);
    /* While trying a new network the radio disconnects on purpose: the
       safety net handles it, and the screen would say something false. */
    rete("on trial for two minutes: not said", true, false, true,
         120000, false);
    /* Without a radio — the simulator, or a silent co-processor — there is
       no Wi-Fi to announce lost. */
    rete("no radio: not said", false, false, false, 600000, false);

    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutte le prove passano");
    return falliti ? 1 : 0;
}
