/* ------------------------------------------------------------------------
 * Sorveglianza del collegamento — 01-specifica-ui.md §4, 11-collaudo.md §1.
 *
 * Guarda com'e messo Home Assistant e decide cosa deve vedere chi sta
 * davanti al pannello. Tre soglie, tre trattamenti, e la differenza fra
 * loro e tutto il punto:
 *
 *   fino a 15 s     niente. Una connessione WebSocket che non manda nulla
 *                   per qualche secondo e normale: e un pannello che
 *                   ascolta, non uno che interroga. Mettere una fascia
 *                   ambra a ogni respiro insegnerebbe a ignorarla.
 *   da 15 a 60 s    fascia di riconnessione in testa alla sezione. I valori
 *                   restano ma sbiaditi, con l'ora dell'ultimo dato valido:
 *                   si continua a leggere, non si comanda.
 *   oltre 60 s      schermata piena di irraggiungibilita. In alto restano
 *                   rete e orologio, perche quelli funzionano.
 *
 * **Al rientro si torna dov'eri**, non alla home. Chi stava guardando le
 * telecamere quando e caduta la rete sta ancora guardando le telecamere, e
 * ritrovarsi in home vorrebbe dire rifare la strada per un guasto che non
 * lo riguardava.
 *
 * Un token rifiutato non e una caduta: e uno stato che non passa da solo, e
 * si dice in un modo diverso — non "sto riprovando" ma "serve un token
 * nuovo".
 *
 * Nor is a lost Wi-Fi, and it comes before everything else: without a
 * network Home Assistant is silent by force, and saying "Home Assistant is
 * not responding" to someone whose router is off or whose password changed
 * would send them looking in the wrong place. After a minute without a
 * network the screen says so, with the reason, and lets one choose another
 * network or type the password again from the glass — the only way that
 * needs no cable.
 * --------------------------------------------------------------------- */
#ifndef SORVEGLIANZA_H
#define SORVEGLIANZA_H

#include <stdbool.h>
#include <stdint.h>

/* Oltre questa eta il collegamento si considera perso del tutto e la
   schermata si copre. 01-specifica-ui.md §4 e 11-collaudo.md §1. */
#define T_HA_GIU 60000u

/* Quale delle quattro situazioni si sta mostrando. */
typedef enum {
    SORV_OK = 0,        /* si vedono i valori e si puo comandare      */
    SORV_RICONNETTO,    /* fascia in testa, valori sbiaditi           */
    SORV_GIU,           /* schermata piena di irraggiungibilita       */
    SORV_SENZA_TOKEN,   /* non passa da solo: serve un token nuovo    */
    SORV_SENZA_RETE,    /* the Wi-Fi does not connect: chosen from the glass */
} situazione_t;

/* Guarda e agisce. Da chiamare a ogni giro dal ciclo principale, dopo
   ha_gira(): e li che l'eta dell'ultimo dato si aggiorna. */
void sorveglianza_gira(uint32_t adesso_ms);

/* La sola decisione, separata da chi la mette a schermo. Sta qui perche si
   possa provarla dando i due numeri a mano: aspettare sessanta secondi veri
   per vedere se compare la schermata piena e' un modo di non provarla. */
situazione_t sorveglianza_valuta(int ha_stato_corrente, uint32_t eta_ms);

/* The other decision, the network's, apart for the same reason. True when
   the radio is there, has not been connected for at least T_HA_GIU, and is
   not trying a new network on purpose. */
bool sorveglianza_rete_persa(bool radio_pronta, bool connessa, bool in_prova,
                             uint32_t senza_rete_ms);

situazione_t sorveglianza_situazione(void);

/* Vero quando i valori a schermo sono vecchi e vanno mostrati sbiaditi.
   Lo leggono le schermate: e la stessa condizione che sospende i comandi. */
bool sorveglianza_dati_vecchi(void);

/* Vero quando non si deve poter comandare: collegamento perso, o entita
   non disponibile. Un comando che non puo arrivare non va nemmeno
   proposto. */
bool sorveglianza_comandi_sospesi(void);

#endif /* SORVEGLIANZA_H */
