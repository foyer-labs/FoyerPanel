/* Mostra uno degli stati di 01-specifica-ui.md §4 nel simulatore. */
#include "sim.h"

#include <string.h>

#include "i18n.h"
#include "nav.h"
#include "stati.h"
#include "tempi.h"

void sim_applica_stato(const char *nome, int vista)
{
    if (!nome) return;

    if (!strcmp(nome, "avvio")) {
        stato_avvio(true);
    } else if (!strcmp(nome, "primo-avvio")) {
        stato_primo_avvio(true, vista);
    } else if (!strcmp(nome, "riconnessione")) {
        stato_riconnessione(3, "18:39");
    } else if (!strcmp(nome, "ha-giu")) {
        stato_ha_giu(true, "18:31");
    } else if (!strcmp(nome, "senza-rete")) {
        /* The reason is what the radio says when it refuses the password:
           the case the screen exists for. */
        stato_rete_giu(true, tr(TX_NET_REFUSED));
    } else if (!strcmp(nome, "cambia-rete")) {
        stato_cambia_rete();
    } else if (!strcmp(nome, "standby")) {
        /* Lo standby arriva dopo due minuti di immobilita: aspettarli per
           guardarlo non e provarlo, e catturarlo sarebbe impossibile. Qui
           si apre subito, che e la stessa schermata vista dallo stesso
           punto. */
        tempi_forza_standby();
    } else if (!strcmp(nome, "altro")) {
        /* Il foglio delle sezioni che non stanno in barra. Si apre con un
           tocco, e un tocco una cattura non lo sa dare: qui si apre da se,
           che e la stessa cosa vista dallo stesso punto. Senza, l'unico
           pezzo di navigazione nuovo resterebbe l'unico che nessuna
           immagine mostra. */
        nav_apri_altro();
    } else if (!strcmp(nome, "ripristino")) {
        /* L'azione e NULL di proposito: qui si guarda com'e fatta la
           conferma, non si cancella la configurazione del simulatore. */
        conferma_azione(
            "Ripristinare il pannello?",
            "Si cancellano la configurazione, la password del Wi-Fi e il "
            "token di Home Assistant. Al riavvio il pannello riparte dal "
            "primo avvio, e per rimetterlo in rete bisogna essere qui "
            "davanti.",
            "Ripristina", true, NULL);
    }
}
