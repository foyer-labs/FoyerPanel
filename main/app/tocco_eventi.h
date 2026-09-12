/* ------------------------------------------------------------------------
 * Il dito, fra chi lo legge e chi lo consuma.
 *
 * Due parti che vanno a ritmi diversi. Da un lato il chip di tocco, che va
 * letto spesso e regolarmente — quindici millisecondi — perche un tocco
 * dura poco piu di cento e chi lo legge di rado se lo perde. Dall'altro
 * LVGL, che chiede quando gli pare: fra una richiesta e l'altra passa il
 * tempo di disegnare un fotogramma intero e di far girare Home Assistant,
 * e su questo pannello sono anche duecento millisecondi.
 *
 * In mezzo ci vuole qualcosa che **ricordi**. Non uno stato — "adesso il
 * dito e giu" — perche uno stato letto in ritardo e uno stato sbagliato:
 * un tocco cominciato e finito nel frattempo non lascia traccia. Ci vuole
 * la **successione**: giu qui, su qui. Quella si conserva, e chi la
 * consuma la ritrova intera anche se arriva tardi.
 *
 * Questo modulo e la coda di quelle transizioni. Non parla con nessun
 * hardware — riceve punti e li mette in fila — ed e per questo che si puo
 * provare sul PC, che e l'unico modo di sapere se e giusto: sul vetro un
 * tocco perso si vede come "non ha funzionato", e non dice mai perche.
 *
 * --- perche non uno stato con qualche bandiera ---------------------------
 *
 * C'era, e faceva quasi la stessa cosa: una bandiera "pressione in attesa",
 * una "rilascio dovuto", e lo stato di adesso come ripiego. Funzionava nei
 * casi che si riesce a immaginare, e i casi che non si riesce a immaginare
 * sono quelli in cui due tocchi cadono dentro lo stesso buco. Nessuno e
 * mai riuscito a dimostrarla giusta guardandola, compreso chi l'ha scritta.
 * Una coda si dimostra guardandola: entra una transizione, esce quella
 * transizione, nell'ordine.
 * --------------------------------------------------------------------- */
#ifndef TOCCO_EVENTI_H
#define TOCCO_EVENTI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct { int16_t x, y; bool giu; } tocco_evento_t;

/* Dimentica tutto. Da chiamare all'avvio. */
void tocco_azzera(void);

/* Cosa dice il chip, adesso. `giu` falso quando non c'e nessun dito; le
   coordinate contano solo quando e vero.
 *
 * Le chiamate ripetute con lo stesso stato **non** aggiungono niente alla
 * coda: quello che si conserva sono i cambi, non le letture. Un dito tenuto
 * giu per un secondo sono due eventi, non sessantasei. */
void tocco_letto(int16_t x, int16_t y, bool giu);

/* Cosa dire a chi disegna, adesso.
 *
 * Se c'e una transizione in coda esce quella, la piu vecchia. Se non ce n'e,
 * esce lo stato in cui si e rimasti — cosi un dito tenuto giu continua a
 * risultare giu, che e cio che serve al trascinamento e alla pressione
 * prolungata.
 *
 * Le coordinate ci sono sempre, anche da rilasciato: chi disegna le usa per
 * sapere **dove** e finito il tocco, e senza non saprebbe su cosa. */
tocco_evento_t tocco_prossimo(void);

/* Quante transizioni aspettano. Solo per la diagnostica: se questo numero
   non torna a zero, chi consuma non sta consumando. */
int tocco_in_attesa(void);

/* Quante ne sono state buttate perche la coda era piena. Diverso da zero
   vuol dire che fra due letture di chi disegna sono successe piu cose di
   quante se ne possano ricordare — e allora il difetto non e qui, e in
   quanto tempo passa fra una lettura e l'altra. */
uint16_t tocco_perse(void);

#endif /* TOCCO_EVENTI_H */
