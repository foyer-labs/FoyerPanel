/* ------------------------------------------------------------------------
 * Server di configurazione — 03-config-contratto.md §6.
 *
 * La regola che regge tutto: **il server e spento**. Non nel senso che il
 * socket non ascolta — ascolta, perche /api/status e /api/log devono
 * rispondere sempre — ma nel senso che ogni altro percorso risponde `404`
 * finche qualcuno non tocca l'interruttore "Consenti configurazione" sul
 * pannello. Sblocco fisico: bisogna essere in casa, davanti al muro.
 *
 * Dura quindici minuti e poi si richiude da sola. Non c'e un modo per
 * lasciarla aperta, e non e una dimenticanza: un pannello a muro con una
 * pagina di configurazione sempre raggiungibile e un pannello che qualcuno
 * riconfigurera prima o poi.
 *
 * `404` e non `403`: un `403` dice "c'e qualcosa qui, ma non puoi". Un
 * `404` non dice niente, che e quello che si vuole dire a chi scandaglia
 * la rete.
 *
 * I due percorsi sempre attivi sono una deroga voluta, motivata in
 * `10-diagnostica.md` §4, e regge solo perche non accettano scritture, non
 * restituiscono segreti ne configurazione, e sono limitati a una richiesta
 * al secondo per chiamante.
 * --------------------------------------------------------------------- */
#ifndef WEB_H
#define WEB_H

#include <stdbool.h>
#include <stdint.h>

#include "http.h"

/* Quanto dura lo sblocco. 03-config-contratto.md §6 e 01-specifica-ui.md
   §3.8 dicono quindici minuti, ed e l'unica durata prevista. */
#define WEB_SBLOCCO_MS (15u * 60u * 1000u)

/* Una richiesta al secondo per chiamante sui due percorsi sempre attivi. */
#define WEB_INTERVALLO_MIN_MS 1000u

bool web_avvia(int porta);
void web_ferma(void);

/* Il gestore unico. Pubblico perche lo provano le prove senza aprire un
   socket: le regole di accesso si verificano meglio chiamando una funzione
   che parlando in HTTP con se stessi. */
void web_servi(const richiesta_t *r, risposta_t *s);

/* Chi sente quando la configurazione e cambiata. Serve perche §2 dice che
   entita, nomi, zone e ordine delle sezioni si applicano **subito**, senza
   riavviare: dopo un salvataggio riuscito l'interfaccia va ricostruita.

   E una callback e non una chiamata diretta perche web.c non conosce
   l'interfaccia — la prova lo compila senza LVGL — e non deve conoscerla:
   il giorno che oltre alle schermate ci sara anche il client di Home
   Assistant da riagganciare, si aggiunge un ascoltatore, non un ramo. */
void web_quando_cambia(void (*cb)(void));

/* A translation was saved. Only the screens are redrawn: the texts are the
   only thing that changed, and the full callback above also reconnects
   Home Assistant — a second of "reconnecting" at every batch of edits for
   a word. NULL, or never set, falls back to the full one. */
void web_when_texts_change(void (*cb)(void));

/* --- sblocco ------------------------------------------------------------ */

/* L'interruttore della schermata Impostazioni. */
void web_sblocca(void);
void web_richiudi(void);

/* Vero finche la finestra e aperta. Da chiamare anche solo per sapere se
   mostrare il conto alla rovescia. */
bool     web_sbloccato(void);

/* Vero quando `diagnostica.configurazione_sempre_aperta` tiene la porta
   aperta: niente finestra, niente tocco sul vetro. Chi mostra il conto alla
   rovescia deve chiedere prima questo, se no annuncia una scadenza che non
   arrivera mai. */
bool     web_sempre_aperta(void);

/* Il pannello deve riavviarsi appena possibile.
 *
 * Una bandiera e non una richiamata, e la ragione e nell'ordine: la
 * risposta HTTP esce **dopo** che web_servi() e tornata, e riavviare dentro
 * la richiesta vorrebbe dire spegnersi con la risposta ancora nel buffer.
 * Chi ha chiesto il riavvio vedrebbe un errore di rete e non saprebbe se il
 * comando e arrivato.
 *
 * Cosi invece la risposta parte, il ciclo principale legge questa bandiera
 * al giro dopo, e riavvia. Chi guarda il browser vede la conferma e poi il
 * pannello che riparte, che e l'ordine in cui le due cose sono successe. */
bool     web_riavvio_chiesto(void);

/* Solo per le prove: fa passare un'immagine dal percorso a flusso senza
   aprire un socket, a pezzi come arriverebbe davvero.
 *
 * Il trasporto non si prova cosi, ed e voluto: e la stessa scelta gia fatta
 * per le regole di accesso — quello che conta e **chi puo cosa** e cosa
 * succede ai byte, e un socket di mezzo aggiunge solo modi di fallire che
 * non c'entrano. Che il trasporto funzioni lo dimostra il simulatore, che
 * serve la pagina davvero.
 *
 * `dichiarati` e quanto direbbe Content-Length: passarne piu di `n` e il
 * modo di simulare una connessione caduta a meta. */
bool     web_prova_ota(const void *dati, size_t n, size_t dichiarati);
uint32_t web_sblocco_resta_ms(void);

/* Il tempo che scorre. Nel simulatore lo da il ciclo principale, nelle
   prove lo da la prova: cosi quindici minuti si verificano in un
   millisecondo invece che in quindici minuti. */
void web_tempo(uint32_t ms);

#endif /* WEB_H */
