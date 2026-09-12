/* ------------------------------------------------------------------------
 * Prova: la struttura dell'interfaccia viene dalla configurazione.
 *
 * Non e una prova sull'aspetto — quello lo guardano le catture — ma sul
 * confine che regge tutta la Fase 2: **quali** zone, quali unita, quali
 * sezioni li dice config.json, e il finto fornitore di dati inventa solo lo
 * **stato**. Finche i due sono mescolati un errore in mezzo non si vede, e
 * si scopre in Fase 3 quando arriva Home Assistant e sposta solo meta delle
 * cose.
 *
 * Qui si stampano i conteggi che l'interfaccia usa davvero, cioe quelli che
 * escono dallo strato dei dati. Chi li confronta con il documento e
 * tools/prova_struttura.py, che parte da una configurazione diversa da
 * quella d'esempio: se i numeri seguissero il codice invece del file, li
 * troverebbe fermi.
 * --------------------------------------------------------------------- */
#include "sim.h"

#include <stdio.h>

#include "dati.h"
#include "sezioni.h"

int sim_prova_struttura(void)
{
    printf("luci=%d\n", dati_luci());
    printf("scene=%d\n", dati_scene());
    printf("riscaldamento=%d\n", dati_zone_clima());
    printf("condizionatori=%d\n", dati_condizionatori());
    printf("accessi=%d\n", dati_accessi());

    /* I nomi, non solo i conteggi: un elenco della lunghezza giusta ma preso
       dai valori di esempio passerebbe un confronto sui soli numeri. */
    for (int n = 0; n < dati_luci(); n++)
        printf("luce.%d=%s\n", n, dati_luce(n)->nome);
    for (int n = 0; n < dati_zone_clima(); n++)
        printf("zona.%d=%s\n", n, dati_zona_clima(n)->nome);
    for (int n = 0; n < dati_condizionatori(); n++)
        printf("unita.%d=%s\n", n, dati_condizionatore(n)->nome);
    for (int n = 0; n < dati_accessi(); n++) {
        const accesso_t *a = dati_accesso(n);
        printf("accesso.%d=%s,%s,%s\n", n, a->id,
               a->tipo == ACC_INTERRUTTORE ? "switch" : "pulse",
               a->ha_sensore ? "sensore" : "cieco");
    }

    /* Le sezioni con il loro ordine: e quello che comanda il rail e il dock. */
    for (int n = 0; n < sezioni_attive(); n++)
        printf("sezione.%d=%s\n", n, sezione(sezione_attiva(n))->chiave);

    return 0;
}
