/* ------------------------------------------------------------------------
 * Impianto di navigazione — 01-specifica-ui.md §1.
 *
 * La forma dipende dall'orientamento, e non e una rotazione del layout ma
 * una riorganizzazione (09-profili.md §1-ter):
 *
 *   orizzontale   rail verticale a sinistra, ingranaggio in fondo,
 *                 dock in home con un pulsante per sezione
 *   verticale     barra orizzontale in basso, ingranaggio nella testata
 *                 della home, nessun dock
 *
 * In entrambi i casi HOME e il ritorno unico: nessun tasto "indietro"
 * gerarchico, e la voce corrente e evidenziata.
 * --------------------------------------------------------------------- */
#ifndef NAV_H
#define NAV_H

#include "lvgl.h"
#include "sezioni.h"

/* Costruisce rail o barra dentro `padre`, secondo l'orientamento. */
lv_obj_t *nav_costruisci(lv_obj_t *padre);

/* Evidenzia la voce della sezione corrente e attenua le altre. */
void nav_evidenzia(sezione_t s);

/* Manda alla sezione la cui voce di navigazione sta sotto quel punto, e
   dice se ne ha trovata una.
   Serve ai modali: il velo li copre tutto lo schermo, rail compreso, come
   nel mockup — ma un rail che si vede, anche attenuato, deve rispondere,
   altrimenti da un modale non si torna a casa (11-collaudo.md §1). */
bool nav_inoltra_tocco(lv_point_t punto);

/* Apre il foglio con le sezioni che non stanno in barra, e lo chiude.
   Aprirlo serve al simulatore per fotografarlo; chiuderlo serve a chi
   cambia schermata, perche il foglio vive sopra tutto e sopravviverebbe. */
void nav_apri_altro(void);
void nav_chiudi_altro(void);

/* Il dock della home: un pulsante per sezione attiva, fino a dock_max.
   Restituisce NULL sui profili verticali, dove il dock non esiste e la
   barra in basso lo sostituisce. */
lv_obj_t *nav_dock(lv_obj_t *padre);

#endif /* NAV_H */
