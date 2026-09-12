/* ------------------------------------------------------------------------
 * Le poche cose che l'interfaccia deve sapere del sistema sotto.
 *
 * Indirizzo IP, potenza del segnale, memoria libera, da quanto e acceso,
 * riavvio: sul pannello vengono dalla radio e da ESP-IDF, sul PC in parte
 * non esistono. L'interfaccia pero deve poterli mostrare senza sapere su
 * cosa gira — e finora non poteva, ed e la ragione per cui la schermata
 * Impostazioni mostrava un indirizzo scritto a mano e la diagnostica dei
 * contatori inventati.
 *
 * Stessa forma di archivio.h e magazzino.h: un'intestazione comune, due
 * attuazioni.
 * --------------------------------------------------------------------- */
#ifndef SISTEMA_H
#define SISTEMA_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool        connessa;
    const char *indirizzo;    /* "192.0.2.131", o "" — mai NULL */
    int8_t      potenza_dbm;  /* 0 se non connessa                */
    const char *descrizione;  /* "connessa", "in corso", ... mai NULL */
} rete_info_t;

rete_info_t sistema_rete(void);

/* The radio is up: on the P4 the ESP32-C6 answered and esp_wifi started.
   Not the same as a connection — a radio can be up with no network in
   range — and it is the boot screen's second step. False where there is
   no radio at all, as in the simulator. */
bool sistema_radio_pronta(void);

/* The trial of a Wi-Fi network just changed from the page: "waiting" (and
   the seconds before it starts), "trying", "reverted", or "" when there is
   nothing to say. Never NULL. On the PC there is no network to try. */
const char *sistema_rete_prova(uint32_t *fra_s);

/* True while a new network waits for its trial or is being tried: the
   radio disconnects on purpose, and announcing a lost Wi-Fi would be
   wrong. */
bool sistema_rete_in_prova(void);

/* Network name and password were just written from the glass: let the
   radio use them. If the previous network worked they are tried with the
   safety net, otherwise simply applied. Does nothing on the PC. */
void sistema_rete_applica(void);

/* Retry the connection now, without waiting for the next attempt. */
void sistema_rete_riprova(void);

/* --- le reti che si vedono da qui ---------------------------------------
 *
 * Serve al primo avvio, dove non si puo chiedere a chi installa di battere
 * un SSID a mano: sbagliare una lettera da una tastiera a schermo e
 * facilissimo, e il guasto che ne segue — "non si connette" — non dice mai
 * che il nome era sbagliato.
 *
 * La scansione **non e istantanea**: la radio deve girare i canali, e
 * mentre lo fa non e connessa. Percio si chiede e si torna a guardare, non
 * si aspetta: sistema_reti_cerca() avvia il giro, sistema_reti_pronte()
 * dice se e finito, e nel frattempo la schermata resta viva.
 */
typedef struct {
    const char *ssid;      /* mai NULL, mai vuoto: le reti senza nome si scartano */
    int8_t      dbm;       /* meno vicino a zero, piu debole */
    bool        protetta;  /* falso solo su una rete aperta */
} rete_trovata_t;

/* Comincia un giro di scansione. Falso se la radio non c'e o e occupata. */
bool sistema_reti_cerca(void);

/* Il giro e finito e l'elenco si puo leggere. */
bool sistema_reti_pronte(void);

/* Quante ne sono state trovate, e la n-esima. Ordinate dalla piu forte:
   quella di casa e quasi sempre la prima, ed e la sola cosa che si possa
   fare per indovinare al posto di chi guarda. */
int sistema_reti_quante(void);
rete_trovata_t sistema_rete_trovata(int n);

typedef struct {
    uint32_t accensione_s;    /* da quanto e acceso                     */
    uint16_t riavvii;         /* quante volte, da sempre                */
    const char *motivo;       /* perche l'ultima volta. Mai NULL        */
    uint32_t heap_libero;     /* RAM interna, in byte                   */
    uint32_t heap_minimo;     /* il minimo storico: il numero che conta */
    uint32_t psram_libera;
    uint8_t  fps;             /* 0 = non misurati                       */
} sistema_info_t;

sistema_info_t sistema_stato(void);

/* Da chiamare una volta all'accensione: e li che si conta il riavvio e si
   legge perche il chip si e riacceso. Chiedendolo piu tardi si conterebbe
   una volta per ogni domanda. */
void sistema_avvia(void);

/* Un fotogramma e stato consegnato allo schermo. Da chiamare dal travaso:
   e l'unico posto che sa davvero quando un'immagine e uscita, e contare
   altrove — giri di ciclo, chiamate a lv_timer_handler — darebbe un numero
   che somiglia agli fps senza esserlo. */
void sistema_fotogramma(void);

/* Riavvia. Non torna. Sul PC chiude il simulatore, che e la cosa piu
   vicina al riavvio che un simulatore possa fare. */
void sistema_riavvia(void);

#endif /* SISTEMA_H */
