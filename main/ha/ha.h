/* ------------------------------------------------------------------------
 * Home Assistant — WebSocket API con token a lunga durata.
 *
 * **Nessun polling.** Ci si sottoscrive a `state_changed` e si aspetta: il
 * pannello non chiede mai "com'e adesso", glielo dicono. Un pannello che
 * interroga a intervalli su una casa con qualche migliaio di entita e un
 * pannello che scalda dietro la placca e fa lavorare un Raspberry per
 * niente.
 *
 * Il giro e sempre lo stesso, ed e definito da Home Assistant:
 *
 *   ← auth_required        appena aperto il WebSocket
 *   → auth                 col token, che e l'unica volta che passa di qui
 *   ← auth_ok / auth_invalid
 *   → subscribe_events     state_changed
 *   → subscribe_entities   **solo** le entita nominate in config.json
 *   ← event {a:...}        quelle entita, una volta sola
 *   → unsubscribe_events   la fotografia e presa, basta cosi
 *   ← event {data:...}     da qui in poi, solo quello che cambia
 *
 * La fotografia non si chiede piu con `get_states`, che risponde con tutta
 * la casa: sull'impianto di casa sono 712 kB in un frame solo, e quel testo
 * diventa un albero cJSON da qualche megabyte piu una pausa di analisi che
 * si sente sotto il dito. Il pannello usa le entita che ha in
 * configurazione, e sono quelle che chiede.
 *
 * **auth_invalid non si riprova.** Un token sbagliato sbagliato resta, e
 * riprovare ogni quindici secondi vuol dire riempire il registro di Home
 * Assistant di tentativi falliti e non dire mai all'utente cosa c'e che non
 * va. Si ferma, si dice perche, e si riparte solo quando qualcuno cambia il
 * token. E il criterio di 11-collaudo.md: "messaggio chiaro, non un riavvio
 * in ciclo".
 * --------------------------------------------------------------------- */
#ifndef HA_H
#define HA_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HA_SPENTO = 0,        /* non configurato, o fermato apposta          */
    HA_CONNETTO,          /* socket e stretta di mano                    */
    HA_AUTENTICO,         /* token mandato, si aspetta la risposta       */
    HA_ALLINEO,           /* autenticati: si chiede la fotografia        */
    HA_PRONTO,            /* fotografia arrivata, eventi in ascolto      */
    HA_CADUTO,            /* collegamento perso: si riprovera            */
    HA_TOKEN_RIFIUTATO,   /* **non** si riprova: serve un token nuovo    */
} ha_stato_t;

/* Legge la configurazione e comincia. Senza host o senza token non prova
   nemmeno: resta HA_SPENTO, che e diverso da "caduto" e va detto in modo
   diverso. */
void ha_avvia(void);

/* Ferma e chiude. Dopo, ha_stato() e HA_SPENTO. */
void ha_ferma(void);

/* Riprova subito, azzerando il rifiuto del token. La chiama chi salva una
   configurazione o un segreto nuovo: e il solo modo per uscire da
   HA_TOKEN_RIFIUTATO, e deve essere un gesto di qualcuno. */
void ha_riprova(void);

/* Fa avanzare tutto: trasporto, protocollo, riconnessione, comandi
   scaduti. `adesso_ms` e l'orologio del sistema. */
void ha_gira(uint32_t adesso_ms);

ha_stato_t  ha_stato(void);
const char *ha_stato_nome(ha_stato_t s);

/* Perche non funziona, in italiano e senza segreti dentro. Mai NULL. */
const char *ha_motivo(void);

/* Da quanto non arriva niente da Home Assistant. Serve alle fasce di
   riconnessione e ai dati sbiaditi: sotto i 15 s non si dice niente, fra
   15 e 60 s si sbiadisce, oltre si copre la schermata. */
uint32_t ha_eta_ultimo_dato_ms(uint32_t adesso_ms);

/* --- comandi ------------------------------------------------------------ */

/* Chiama un servizio. `dati_json` sono i parametri aggiuntivi come oggetto
   JSON senza le graffe esterne — "\"brightness_pct\": 60" — oppure NULL.
   Falso se non si e collegati: e la risposta immediata che evita l'attesa
   infinita di 11-collaudo.md.

   Non aspetta la risposta: il pannello mostra il riscontro quando lo stato
   dell'entita cambia davvero, non quando il comando parte. E la differenza
   fra "l'ho chiesto" e "e successo", e su un pannello a muro conta. */
/* --- l'elenco delle entita, per i menu della pagina ---------------------
 *
 * ha_elenco_chiedi() lo domanda a Home Assistant se non e gia in arrivo o
 * gia arrivato; non blocca e non torna niente. ha_elenco() da il documento
 * gia pronto per la pagina — `{"entita":["light.x","sensor.y"]}` — o NULL
 * finche non c'e. Gia impacchettato di proposito: il server web manda un
 * corpo solo, e comporlo li vorrebbe dire una seconda copia. Il testo resta
 * di qui e vive quanto il collegamento: chi lo riceve non lo libera.
 * ha_elenco_no() e vero se Home Assistant ha rifiutato la domanda: allora
 * non arrivera, e chi disegna deve dirlo invece di aspettare.
 *
 * Si chiede su domanda e non al collegamento: un pannello a muro sta
 * acceso mesi senza che nessuno apra la pagina di configurazione. */
/* --- le stanze che il robot conosce -------------------------------------
 *
 * Chiede a Home Assistant la mappa del robot e ne ricava le stanze come
 * `{"stanze":[{"numero":20,"nome":"Cucina"},...]}`. I numeri sono quelli dei
 * **segmenti**, cioe i soli che il robot accetta: quelli che la sua
 * applicazione mostra sulla mappa sono un altro elenco, e copiarli fa uscire
 * il robot dalla base per farlo rientrare subito senza pulire.
 *
 * Su domanda, come l'elenco delle entita. NULL finche' non e arrivata. */
void        ha_stanze_chiedi(const char *entita);
const char *ha_stanze(void);

/* --- la curva della giornata --------------------------------------------
 *
 * Ventiquattro medie orarie di un sensore, dalla mezzanotte locale a
 * adesso, chieste con `recorder/statistics_during_period`. Le statistiche e
 * non lo storico grezzo: un sensore di potenza che cambia ogni pochi
 * secondi produce migliaia di righe in ventiquattro ore, e queste sono
 * ventiquattro.
 *
 * Richiede che il sensore abbia `state_class` in Home Assistant — senza,
 * il recorder non ne tiene statistiche e la risposta torna vuota. E il caso
 * che ha_storico_no() distingue da «non l'ho ancora chiesto». */
void    ha_storico_chiedi(const char *entita);
bool    ha_storico_c_e(void);
bool    ha_storico_no(void);        /* chiesto e tornato vuoto */
int32_t ha_storico_ora(int ora);    /* 0..23, in unita del sensore */

/* Da chiamare quando e ora di rinfrescare: la prossima ha_storico_chiedi()
   tornera a chiedere invece di ricordarsi di averlo gia fatto. */
void    ha_storico_scade(void);
bool        ha_stanze_no(void);

void        ha_elenco_chiedi(void);
const char *ha_elenco(void);
bool        ha_elenco_no(void);

bool ha_chiama(const char *dominio, const char *servizio, const char *entita,
               const char *dati_json);

/* --- solo con il banco di prova del robot -------------------------------
 *
 * Esistono soltanto compilando con -D PANNELLO_BANCO_ROBOT=1: servono a
 * /api/prova/robot, che e un comando fisico raggiungibile da chiunque sia
 * sulla rete di casa e sta spento di riposo. Vedi firmware/main/CMakeLists.txt.
 */
#ifdef PANNELLO_BANCO_ROBOT
/* L'ultimo messaggio di risposta a un comando, come e arrivato. Mai NULL,
   vuoto finche' non ne e arrivata una. */
const char *ha_ultima_risposta(void);

/* Come ha_chiama() ma con `"return_response": true`: la risposta del
   servizio torna indietro e si legge con ha_ultima_risposta(). */
bool ha_chiama_con_risposta(const char *dominio, const char *servizio,
                            const char *entita, const char *dati_json);
#endif

/* Quanti comandi sono partiti e quanti non sono riusciti, per la
   diagnostica di 10-diagnostica.md §2. */
uint32_t ha_comandi_inviati(void);

/* Da quanto non arriva niente, senza dover sapere che ora e: il client se
   lo ricorda dall'ultimo ha_gira(). Serve a chi mostra i contatori e non
   ha un orologio a portata di mano — le prove, per esempio, che girano
   senza LVGL. */
uint32_t ha_eta_ultimo_dato(void);
uint32_t ha_comandi_falliti(void);

/* --- per le prove ------------------------------------------------------- */

/* Fa entrare un messaggio come se fosse arrivato dal WebSocket. Serve alle
   prove per costruire situazioni che un server vero non produrrebbe a
   comando — un evento con un'entita sconosciuta, un result senza campi. */
void ha_messaggio(const char *json, uint32_t adesso_ms);

#endif /* HA_H */
