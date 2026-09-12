/* ------------------------------------------------------------------------
 * Ripristino di fabbrica.
 *
 * Cancella tutto quello che il pannello ha imparato da quando e stato
 * acceso la prima volta: la configurazione, la sua copia di sicurezza, e i
 * segreti in NVS — password del Wi-Fi, token di Home Assistant, credenziali
 * di go2rtc, password delle reti condivisibili. Al riavvio successivo non
 * c'e piu niente da leggere, quindi si riparte dal primo avvio.
 *
 * **Non e il comando `azzera` della console**, ed e bene che non lo sia.
 * Quello toglie la configurazione e lascia rete e token: serve a rimettere
 * in piedi un pannello con un config.json che non va, **senza perdere il
 * modo di raggiungerlo**. Questo toglie anche quelli, e un pannello dopo
 * questo comando e un pannello a cui bisogna tornare davanti.
 *
 * Per questo la conferma non e una formalita: chi lo preme dal divano, con
 * la pagina web aperta sul telefono, dopo si ritrova a doversi alzare.
 * --------------------------------------------------------------------- */
#ifndef RIPRISTINO_H
#define RIPRISTINO_H

#include <stdbool.h>

/* Cancella configurazione, copia e segreti. Vero se **tutto** e andato:
   un ripristino a meta e la cosa peggiore, perche il pannello riparte
   credendo di sapere qualcosa che non sa piu.

   Non riavvia: la memoria in RAM e ancora quella di prima, e chi chiama
   deve poterlo dire all'utente prima di spegnere la luce. Il riavvio lo fa
   il chiamante, subito dopo. */
bool ripristino_esegui(void);

#endif /* RIPRISTINO_H */
