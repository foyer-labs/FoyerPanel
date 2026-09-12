/* ------------------------------------------------------------------------
 * Finto fornitore — contatori e registro. Solo Fase 1.
 *
 * I valori sono quelli dell'esempio di 10-diagnostica.md §4, con un paio di
 * righe di registro che coprono i casi che contano: un errore, un avviso,
 * un'informazione. Nessuna riga contiene un token o una password, che e
 * l'unica regola non negoziabile del registro.
 * --------------------------------------------------------------------- */
#include "dati_finti.h"
#include "registro.h"
#include "sistema.h"
#include "ha.h"

#include <stddef.h>

static const riga_log_t RIGHE[] = {
    { "18:41:07", LOG_INFO,   "ha",   "command switch.cancello_pedonale · ok · 118 ms" },
    { "18:40:52", LOG_INFO,   "web",  "configuration saved and applied" },
    { "18:39:14", LOG_AVVISO, "ha",   "sensor.robot_batteria unavailable" },
    { "18:32:00", LOG_INFO,   "ha",   "reconnected · 63 entities subscribed" },
    { "18:31:45", LOG_ERRORE, "ha",   "connection lost · retrying in 15 s" },
    { "18:12:03", LOG_INFO,   "sys",  "NTP synchronised · offset 0.4 s" },
    { "18:11:58", LOG_INFO,   "wifi", "connected · RSSI -52 dBm" },
    { "18:11:52", LOG_INFO,   "sys",  "boot completed in 4.2 s" },
};
#define N_RIGHE ((int)(sizeof RIGHE / sizeof RIGHE[0]))

/* --- i contatori, e da dove vengono davvero -----------------------------
 *
 * Erano tutti inventati, ed erano inventati **bene**: "connesso", ventiquattro
 * fotogrammi al secondo, -52 dBm. Numeri plausibili che non venivano da
 * nessuna parte, e che nella schermata Impostazioni e in /api/status
 * somigliavano abbastanza alla realta da non farsi notare per settimane.
 *
 * Un segnaposto che indovina e peggio di uno che sbaglia: uno che sbaglia lo
 * si corregge, uno che indovina resta.
 *
 * Adesso ogni campo ha una fonte, e sono tre: il **sistema** per memoria,
 * tempo e fotogrammi (sistema.h, con un'attuazione per bersaglio), la
 * **radio** per il segnale, **Home Assistant** per lo stato del collegamento
 * e i comandi. Dove una fonte non c'e — gli fps sul PC — il campo resta a
 * zero e chi disegna lo mostra come "non lo so", invece di riempirlo con
 * qualcosa di verosimile. */
contatori_t dati_contatori(void)
{
    const sistema_info_t s = sistema_stato();
    const rete_info_t    r = sistema_rete();

    return (contatori_t){
        .accensione_s = s.accensione_s,
        .riavvii = s.riavvii,
        .motivo_ultimo_riavvio = s.motivo,
        .heap_libero = s.heap_libero,
        .heap_minimo = s.heap_minimo,
        .psram_libera = s.psram_libera,
        .fps = s.fps,
        .wifi_rssi = r.potenza_dbm,
        .ha_stato = ha_stato_nome(ha_stato()),
        .ha_ultimo_dato_s =
            (uint16_t)(ha_eta_ultimo_dato() / 1000),
        .comandi_inviati = ha_comandi_inviati(),
        .comandi_falliti = ha_comandi_falliti(),
        .errori_recenti = (uint16_t)registro_errori(),
    };
}

/* Il registro vero. Le righe d'esempio qui sopra servono alle catture: sul
   PC nessuno scrive nel registro, e un elenco vuoto non mostra com'e fatto
   un elenco.

   La condizione pero non e piu "e vuoto" ma "non e **mai** stato scritto",
   e la differenza e nata insieme al pulsante Svuota registro: sul pannello
   il registro riceve righe fin dall'accensione, quindi dopo averlo
   svuotato e vuoto ma non vergine — e riempirlo di righe finte proprio
   nel momento in cui qualcuno lo ha appena pulito per cercare un guasto
   sarebbe il posto peggiore in cui inventare qualcosa. */
int dati_log(void)
{
    if (!registro_mai_scritto()) return registro_righe();
    return N_RIGHE;
}

const riga_log_t *dati_log_riga(int n)
{
    if (!registro_mai_scritto()) return registro_riga(n);
    return (n >= 0 && n < N_RIGHE) ? &RIGHE[n] : NULL;
}
