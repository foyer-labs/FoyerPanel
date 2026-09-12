/* ------------------------------------------------------------------------
 * Stati del collegamento e schermata di avvio — 01-specifica-ui.md §4.
 *
 * Tre situazioni, tre trattamenti diversi, e la differenza conta:
 *
 *   riconnessione (transitoria)   fascia ambra in testa alla sezione, i
 *                                 valori restano ma sbiaditi, con l'ora
 *                                 dell'ultimo dato valido. Si continua a
 *                                 leggere, non si comanda.
 *   irraggiungibile (persistente) schermata piena. In alto restano rete e
 *                                 orologio, perche quelli funzionano.
 *   conferma di apertura          velo e modale: TIENI PREMUTO per gli
 *                                 accessi che lo chiedono.
 * --------------------------------------------------------------------- */
#ifndef STATI_H
#define STATI_H

#include <stdbool.h>

#include "lvgl.h"

/* Chiude modali e schermate piene. La chiama la navigazione. */
void stati_chiudi(void);

/* Fascia di riconnessione in testa alla sezione. `tentativo` a zero la
   toglie. I valori sotto vanno sbiaditi e i comandi sospesi. */
void stato_riconnessione(int tentativo, const char *ultimo_dato);

/* Schermata piena di irraggiungibilita. */
void stato_ha_giu(bool si, const char *ultimo_dato);

/* Full screen: the Wi-Fi does not connect. It says the reason — password
   refused, network not found — and offers to choose another network or
   type the password again from the glass. Called with the screen already
   open, it only updates the reason. */
void stato_rete_giu(bool si, const char *motivo);

/* The network steps of the first boot — list, password, test — and nothing
   else: to change network or type the password again without going
   through Home Assistant. It closes by itself at the end, where one was. */
void stato_cambia_rete(void);

/* The boot screen, §4.1: the steps of a power-up, ticked as they succeed
   with how long each took. Shown by the device at every start that has a
   configuration; it closes itself when everything is up, after
   T_AVVIO_MASSIMO, or at a touch. */
void stato_avvio(bool si);

/* Primo avvio: tre passi con i pallini, rete, Home Assistant, prova. Non e
   una sezione — non ha rail ne HOME — perche finche non e finito non c'e
   nessuna home a cui tornare. */
void stato_primo_avvio(bool si, int passo_iniziale);

/* Qui c'era conferma_apertura(): un modale del mockup che apriva un velo,
   due pulsanti, e non faceva niente — tutti e due chiudevano e basta. Non
   e stato collegato perche nel frattempo la conferma di un'apertura l'ha
   presa pulsante_comando(), con la pressione prolungata: il campo
   `conferma` di ogni accesso in config.json accende quell'anello. Una
   seconda conferma sopra a quella sarebbe cerimonia. */

/* Conferma per un'azione che non si torna indietro.
 *
 * A differenza di quella sopra, questa **esegue**: `azione` viene chiamata
 * se e solo se qualcuno preme il pulsante di conferma. Chiudere col velo,
 * premere Annulla o lasciar scadere gli otto secondi sono tutti e tre modi
 * di dire di no, e nessuno dei tre chiama niente.
 *
 * `pericolosa` colora il pulsante di conferma come un avviso invece che
 * come l'accento: un ripristino di fabbrica e un riavvio non devono avere
 * lo stesso aspetto, perche uno dei due si puo rifare e l'altro no. */
void conferma_azione(const char *titolo, const char *dettaglio,
                     const char *etichetta, bool pericolosa,
                     void (*azione)(void));

/* Una cosa da dire, senza niente da scegliere: un pulsante solo non c'e
   nemmeno, si chiude toccando fuori. Non e conferma_azione() con
   l'etichetta cambiata — quella mette due pulsanti perche c'e una
   decisione, e metterli dove non c'e insegna che sono intercambiabili. */
void avviso(const char *titolo, const char *testo);

/* Un QR grande quanto ci sta, sopra il velo. `sotto` e l'indirizzo scritto
   in chiaro per chi il telefono non ce l'ha in mano. Si chiude toccando
   fuori, come gli altri modali. */
void mostra_qr(const char *titolo, const char *testo, const char *sotto);

#endif /* STATI_H */
