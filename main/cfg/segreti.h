/* ------------------------------------------------------------------------
 * Segreti — 03-config-contratto.md §1.
 *
 * Password del Wi-Fi e token di Home Assistant. Sul
 * pannello stanno in NVS, spazio dei nomi `secrets`, e non in config.json:
 * il file si esporta, si copia su una chiavetta, si manda per posta a chi
 * aiuta a configurare l'altro pannello, e un token dentro un file cosi e un
 * token perso.
 *
 * **Questa intestazione non ha una funzione che legge un segreto e lo
 * restituisce.** Non e una dimenticanza. Chi deve usare un segreto lo usa
 * dove serve — segreti_usa() lo passa a una funzione e non lo lascia in
 * giro — e chi deve solo sapere se c'e chiama segreti_impostato(). La
 * pagina web puo scrivere e chiedere `impostato: true/false`, punto: se
 * esistesse un accessore che torna la stringa, prima o poi finirebbe in una
 * risposta HTTP o in una riga di registro, e nessuno se ne accorgerebbe
 * fino al giorno sbagliato.
 *
 * Per la stessa ragione i nomi dei segreti sono un elenco chiuso: un nome
 * arbitrario dalla rete diventerebbe una chiave NVS arbitraria.
 * --------------------------------------------------------------------- */
#ifndef SEGRETI_H
#define SEGRETI_H

#include <stdbool.h>
#include <stddef.h>

/* Lunghezza massima di un segreto, terminatore escluso. Le password WPA2
   arrivano a 63 caratteri, i token di Home Assistant sono JWT lunghi. */
#define SEGRETO_MAX 512

/* Quante reti private si possono condividere. Il limite e la
   schermata: quattro schede ci stanno senza impaginare. */
#define RETI_MAX 5

typedef enum {
    SEG_WIFI_PASSWORD = 0,   /* rete principale                            */
    SEG_HA_TOKEN,            /* token a lunga durata                       */
    SEG_WIFI_RETE_1_PASSWORD,  /* la prima dell'elenco condiviso           */

    /* Le reti private condivisibili, fino a quattro. Sono voci **distinte**
       e non un nome costruito a runtime: se il nome della chiave si
       componesse da un indice o da un testo di configurazione, basterebbe un
       numero fuori posto per scrivere in NVS dove non si deve. L'elenco
       chiuso e proprio questo. */
    SEG_WIFI_RETE_2_PASSWORD,
    SEG_WIFI_RETE_3_PASSWORD,
    SEG_WIFI_RETE_4_PASSWORD,
    SEG_WIFI_RETE_5_PASSWORD,


    SEG_QUANTI,
} segreto_t;

/* Il nome con cui il segreto arriva da POST /api/secrets e con cui sta in
   NVS. Mai NULL. */
const char *segreto_nome(segreto_t s);

/* Il segreto con quel nome, o SEG_QUANTI se il nome non e nell'elenco. */
segreto_t segreto_da_nome(const char *nome);

/* Apre il magazzino. Falso solo se NVS non e utilizzabile. */
bool segreti_avvia(void);

/* Scrive. Un valore vuoto **cancella** il segreto: e il modo per togliere
   un token senza doverne inventare uno finto. */
bool segreti_scrivi(segreto_t s, const char *valore);

/* C'e o non c'e. E tutto quello che la pagina web puo sapere. */
bool segreti_impostato(segreto_t s);

/* Usa un segreto senza copiarlo altrove: `uso` lo riceve, ne fa quello che
   deve — un'intestazione HTTP, una connessione Wi-Fi — e al ritorno la
   copia in chiaro viene cancellata dalla memoria. Falso se il segreto non
   c'e, e in quel caso `uso` non viene chiamata.

   Il valore passato a `uso` **non va conservato**: vale solo per la durata
   della chiamata. */
bool segreti_usa(segreto_t s, void (*uso)(const char *valore, void *dato),
                 void *dato);

/* Cancella tutto: e il ripristino di §7, e va insieme alla cancellazione
   di LittleFS. */
bool segreti_azzera(void);

#endif /* SEGRETI_H */
