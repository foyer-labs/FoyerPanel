/* ------------------------------------------------------------------------
 * Ripristino di fabbrica — vedi ripristino.h per la differenza con `azzera`.
 * --------------------------------------------------------------------- */
#include "ripristino.h"

#include "archivio.h"
#include "config.h"
#include "segreti.h"

bool ripristino_esegui(void)
{
    /* I segreti per primi, e non e indifferente.
     *
     * Se saltasse la corrente a meta, il pannello ripartirebbe con una
     * configurazione ancora valida e senza le credenziali: si accorge subito
     * che non riesce a collegarsi, lo dice, e da li si riparte. Nell'ordine
     * opposto ripartirebbe **senza configurazione ma con le credenziali di
     * prima**, cioe al primo avvio con dentro segreti che l'utente crede
     * cancellati — che e esattamente la cosa che un ripristino di fabbrica
     * non deve poter lasciare dietro di se. */
    const bool s = segreti_azzera();

    /* archivio_cancella() torna falso anche quando il file non c'era, ed e
       giusto per lui: dice se ha cancellato qualcosa. Qui pero un file che
       non c'e e un ripristino riuscito — anzi, e lo stato d'arrivo — quindi
       si guarda com'e rimasto il disco, non cosa ha fatto la chiamata. */
    archivio_cancella(CFG_FILE);
    archivio_cancella(CFG_COPIA);
    const bool c = !archivio_esiste(CFG_FILE) && !archivio_esiste(CFG_COPIA);

    return s && c;
}
