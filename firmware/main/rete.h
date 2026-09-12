/* ------------------------------------------------------------------------
 * Wi-Fi — solo pannello.
 *
 * Il nome della rete sta in `config.json` sotto `sistema/rete/ssid`: un SSID
 * l'access point lo grida a chiunque passi, non e un segreto, e serve poterlo
 * mostrare — un pannello che non sa dire a quale rete sta provando ad
 * attaccarsi non aiuta chi lo deve riparare.
 *
 * La password no. Sta in NVS e ci arriva da segreti_scrivi(); di qui esce
 * solo dentro segreti_usa(), che la passa a chi si connette e poi cancella
 * la copia. Nessuna funzione di questa intestazione la restituisce, e
 * nessuna riga di registro la nomina — insieme all'SSID, che 10-diagnostica.md
 * mette nello stesso elenco.
 * --------------------------------------------------------------------- */
#ifndef RETE_H
#define RETE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RETE_SPENTA = 0,   /* nessuna rete configurata: non e un guasto     */
    RETE_SENZA_CHIAVE, /* c'e il nome, manca la password                */
    RETE_IN_CORSO,     /* sta provando                                  */
    RETE_CONNESSA,
    RETE_RIFIUTATA,    /* l'access point ha detto di no: password?      */
    RETE_ASSENTE,      /* quella rete non si vede                       */
} rete_stato_t;

/* Accende la radio e prova. Torna falso solo se la radio non parte: una
   rete non configurata **non e un errore**, e il primo avvio di un pannello
   appena montato al muro. */
bool rete_avvia(void);

rete_stato_t rete_stato(void);

/* rete_avvia() has finished with the radio running. */
bool rete_radio_accesa(void);

/* --- from the graphics task ----------------------------------------------- */

/* Once per loop. True when it has just changed the configuration — a new
   network that did not answer was put back as it was — and whoever draws
   must read it again. */
bool rete_gira(uint32_t adesso_ms);

/* After a save of configuration or secrets: if name, password or hidden
   network changed, prepares the trial with its safety net. See rete.c.
   `subito` skips the thirty seconds of waiting: from the glass, name and
   password arrive together, and there is nothing to wait for. */
void rete_ricontrolla(bool subito);

/* True while a new network waits for its trial or is being tried. */
bool rete_in_prova(void);

/* After a save: if the NTP servers changed, restarts the time service with
   the new ones. */
void rete_ntp_rileggi(void);

/* The new network's trial, for the page: "waiting" (with the seconds left
   in *fra_s), "trying", "reverted" when the last one failed and the old
   network is back, "" when there is nothing to say. Never NULL, never the
   network's name. */
const char *rete_prova_stato(uint32_t *fra_s);

/* Perche uno stato da solo non basta a capire: "in corso" al terzo tentativo
   e "in corso" al trentesimo sono due situazioni diverse. */
uint32_t rete_tentativi(void);

/* dBm. Zero se non connessa. */
int8_t rete_potenza(void);

/* "192.0.2.42", oppure "" se non connessa. Mai NULL. */
const char *rete_indirizzo(void);

/* In una riga, per la diagnostica e per la console. Mai l'SSID, mai la
   password. */
const char *rete_descrizione(void);

/* Rilegge il nome dalla configurazione e la password dal magazzino, e
   riparte. Serve dopo averli cambiati: senza, bisognerebbe riavviare per
   provare una password, e provare una password e proprio la cosa che si fa
   piu volte di seguito. */
void rete_riprova(void);

#endif /* RETE_H */
