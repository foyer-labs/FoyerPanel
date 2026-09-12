/* ------------------------------------------------------------------------
 * Aggiornamento del firmware — 05-architettura-firmware.md §7.
 *
 * Il pannello sta incassato a muro. Un aggiornamento che va male e un
 * pannello da staccare, portare a un PC e riflashare: e la situazione che
 * tutto questo esiste per evitare, ed e il motivo per cui qui ci sono tre
 * garanzie invece di una.
 *
 * **Si scrive sempre sull'altra partizione.** Quella che sta girando non si
 * tocca: se la scrittura si interrompe a meta — corrente, rete, un pacchetto
 * corrotto — al riavvio riparte quella di prima, intatta.
 *
 * **Si verifica la firma prima di avviare.** Non "prima di scrivere": la
 * scrittura e su una partizione che nessuno avviera finche non e completa, e
 * verificare durante la scrittura vorrebbe dire fidarsi di un'immagine
 * ancora a pezzi. Il controllo avviene alla chiusura, su tutto il blocco, e
 * di nuovo al riavvio dal bootloader.
 *
 * **Il firmware nuovo si dichiara valido da solo, e solo se funziona.**
 * Riavviare non basta a dire che va: un'immagine puo partire e non riuscire
 * a fare la cosa per cui esiste — niente rete, niente Home Assistant,
 * schermo nero. Percio parte **in prova**: se entro il tempo di grazia non
 * si e vista una casa collegata e un fotogramma disegnato, il pannello si
 * riavvia e il bootloader rimette quella di prima. Nessuno deve accorgersene
 * e nessuno deve intervenire.
 *
 * --- cosa NON c'e, e perche ----------------------------------------------
 *
 * **L'immagine del co-processore C6** (solo p4). 05-architettura-firmware.md
 * §7 dice che il pacchetto dovrebbe contenerla, e ha ragione: due firmware
 * che si aggiornano separatamente prima o poi si disallineano. Ma
 * riprogrammare il C6 attraverso SDIO e una cosa che non si puo provare
 * senza la scheda, e un aggiornamento non provato che gira su un pannello a
 * muro e peggio di un aggiornamento che non c'e. Sta scritto qui, e il
 * pannello all'avvio dice se `c6_fw` e vuota.
 * --------------------------------------------------------------------- */
#ifndef AGGIORNAMENTO_H
#define AGGIORNAMENTO_H

#include <stdbool.h>
#include <stddef.h>

/* --- scrivere un'immagine nuova ---------------------------------------- */

/* Prepara la partizione ferma. `totale` e quanto ci si aspetta di ricevere,
   e serve solo a rifiutare subito quello che non ci starebbe: zero vuol dire
   "non lo so", ed e accettato.

   Falso se non c'e una partizione dove scrivere o se e gia in corso: due
   aggiornamenti insieme sono un'immagine mescolata. */
bool agg_apri(size_t totale);

/* Un pezzo, nell'ordine in cui arriva. Falso al primo problema, e da quel
   momento l'aggiornamento e da buttare. */
bool agg_scrivi(const void *dati, size_t n);

/* Chiude. `completo` falso — la connessione e caduta a meta — butta via
   tutto senza toccare la partizione di avvio.
 *
 * Con `completo` vero verifica l'immagine intera, **firma compresa**, e solo
 * se convince la segna come quella da avviare al prossimo riavvio. Il
 * riavvio non lo fa: chi chiama deve poter rispondere prima di spegnersi. */
bool agg_chiudi(bool completo);

/* Perche non e riuscito. Mai NULL, vuoto se non e successo niente. */
const char *agg_motivo(void);

/* Quanti byte sono stati scritti finora: la pagina web ne fa una barra. */
size_t agg_scritti(void);

/* --- il giro di prova dopo il riavvio ----------------------------------- */

/* Questa immagine sta girando **in prova**: e stata appena installata e non
   si e ancora dimostrata buona. Falso su un avvio normale. */
bool agg_in_prova(void);

/* Ha funzionato: da adesso e quella buona e il rollback non scatta piu.
   Chiamarla solo dopo aver verificato che il pannello faccia il suo
   mestiere, non solo che sia partito. */
void agg_conferma(void);

/* Non ha funzionato: si torna alla precedente. Riavvia. */
void agg_rifiuta(void);

/* La versione che sta girando, per la pagina e per il registro. */
const char *agg_versione(void);

#endif /* AGGIORNAMENTO_H */
