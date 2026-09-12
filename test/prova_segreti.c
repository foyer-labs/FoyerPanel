/* ------------------------------------------------------------------------
 * Prova dei segreti — 03-config-contratto.md §1.
 *
 * Quello che si vuole dimostrare non e che scrivere e rileggere funzioni:
 * e che **un segreto non esca da nessuna delle strade che il pannello
 * offre**. Sono tre: la configurazione esportata, la risposta della pagina
 * web, il registro di diagnostica. Qui si coprono le prime due; il registro
 * arriva col server.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#include "archivio.h"
#include "config.h"
#include "magazzino.h"
#include "segreti.h"
#include "validazione.h"   /* cfg_albero() */

/* Le cartelle di appoggio stanno nella cartella di compilazione, che
   CMake passa in PROVE_DIR: sono prodotti come i binari, e nella radice
   del progetto facevano solo disordine. */
#ifndef PROVE_DIR
#define PROVE_DIR "."
#endif

static int falliti;

static void prova(const char *cosa, bool esito)
{
    printf("%-58s %s\n", cosa, esito ? "ok" : "FALLITA");
    if (!esito) falliti++;
}

/* Il token vero non compare mai in questo file: se ne inventa uno con una
   forma riconoscibile, cosi cercarlo dentro un'esportazione e semplice. */
static const char *const FINTO_TOKEN =
    "eyJhbGciOiJIUzI1NiJ9.NONDEVEUSCIREMAI.xxxxxxxxxxxxxxxxxxxxxxxxx";

static const char *visto;
static void guarda(const char *valore, void *dato)
{
    (void)dato;
    visto = valore;
    prova("segreti_usa passa il valore giusto",
          strcmp(valore, FINTO_TOKEN) == 0);
}

int main(void)
{
    archivio_radice(PROVE_DIR "/prova_dati_segreti");
    prova("il magazzino si apre", segreti_avvia());
    segreti_azzera();

    /* --- elenco chiuso di nomi ------------------------------------------ */
    prova("i nomi si traducono nei due versi",
          segreto_da_nome(segreto_nome(SEG_HA_TOKEN)) == SEG_HA_TOKEN);
    prova("un nome che non esiste non diventa una chiave",
          segreto_da_nome("../../etc/passwd") == SEG_QUANTI);
    prova("nemmeno un nome vuoto",
          segreto_da_nome("") == SEG_QUANTI && segreto_da_nome(NULL) == SEG_QUANTI);

    /* --- scrittura e presenza ------------------------------------------- */
    prova("all'inizio non c'e niente", !segreti_impostato(SEG_HA_TOKEN));
    prova("si scrive", segreti_scrivi(SEG_HA_TOKEN, FINTO_TOKEN));
    prova("ora c'e", segreti_impostato(SEG_HA_TOKEN));
    prova("gli altri restano vuoti",
          !segreti_impostato(SEG_WIFI_PASSWORD)
          && !segreti_impostato(SEG_WIFI_RETE_1_PASSWORD));

    /* --- uso a prestito ------------------------------------------------- */
    prova("segreti_usa riesce", segreti_usa(SEG_HA_TOKEN, guarda, NULL));
    prova("segreti_usa fallisce se il segreto non c'e",
          !segreti_usa(SEG_WIFI_PASSWORD, guarda, NULL));

    /* Il buffer sta nella pila di segreti_usa: al ritorno non e piu suo.
       Non si puo dereferenziare `visto` — sarebbe proprio l'errore che il
       contratto vieta — ma si puo controllare che qualcuno l'abbia visto. */
    prova("il valore e arrivato alla funzione", visto != NULL);

    /* --- cancellazione col valore vuoto --------------------------------- */
    prova("il valore vuoto cancella", segreti_scrivi(SEG_HA_TOKEN, ""));
    prova("e infatti non c'e piu", !segreti_impostato(SEG_HA_TOKEN));

    /* --- niente segreti nella configurazione ---------------------------- */
    segreti_scrivi(SEG_HA_TOKEN, FINTO_TOKEN);
    segreti_scrivi(SEG_WIFI_PASSWORD, "password-del-wifi-di-casa");

    /* Una configurazione vera da esportare: quella d'esempio, la stessa
       della casa. E il posto piu probabile in cui un segreto finirebbe per
       sbaglio, perche e il file che si copia da un pannello all'altro. */
    FILE *f = fopen("docs/04-config.example.json", "rb");
    prova("la configurazione d'esempio si apre", f != NULL);
    if (f) {
        static char buf[ARCHIVIO_MAX];
        const size_t n = fread(buf, 1, sizeof buf, f);
        fclose(f);
        archivio_scrivi(CFG_FILE, buf, n);
    }
    prova("la configurazione si legge", cfg_carica() == CFG_LETTA);

    /* Il caso che conta davvero: qualcuno scrive un token a mano dentro
       config.json. Lo schema non lo prevede, ma §8 impone di conservare i
       campi sconosciuti, quindi il token sopravvive al giro e finisce nel
       file che si copia da un pannello all'altro. */
    cJSON_AddStringToObject(cfg_albero(), "ha_token", FINTO_TOKEN);
    cJSON_AddStringToObject(
        cJSON_GetObjectItem(cfg_albero(), "home_assistant"),
        "password", "password-del-wifi-di-casa");

    char *esportata = cfg_esporta(true);
    prova("la configurazione si esporta", esportata != NULL);
    if (esportata) {
        prova("il token scritto a mano viene oscurato",
              strstr(esportata, "NONDEVEUSCIREMAI") == NULL);
        prova("e cosi la password",
              strstr(esportata, "password-del-wifi-di-casa") == NULL);
        prova("ma il resto della configurazione c'e ancora",
              strstr(esportata, "Striscia LED divano") != NULL);
        cfg_libera_testo(esportata);
    }

    /* Senza oscuramento il campo torna com'era: e la prova che il
       controllo di sopra guarda davvero qualcosa. */
    char *intera = cfg_esporta(false);
    prova("senza oscuramento il token c'e",
          intera && strstr(intera, "NONDEVEUSCIREMAI") != NULL);
    cfg_libera_testo(intera);

    /* E l'albero in memoria non e stato toccato: al prossimo salvataggio
       non finiscono asterischi dentro config.json. */
    prova("l'albero in memoria e intatto",
          strcmp(cfg_testo("ha_token", ""), FINTO_TOKEN) == 0);

    /* --- azzeramento ---------------------------------------------------- */
    prova("l'azzeramento toglie tutto", segreti_azzera());
    bool nessuno = true;
    for (int n = 0; n < SEG_QUANTI; n++)
        if (segreti_impostato((segreto_t)n)) nessuno = false;
    prova("e non ne resta nemmeno uno", nessuno);

    /* --- the old names move to the new ones ------------------------------
     *
     * A panel from the Italian project keeps its shared-network passwords
     * under wifi_osp_pw and wifi_pv1..4_pw. At boot they must reappear
     * under the new names, and the old keys must be gone — a password left
     * under a name nobody reads any more is one nobody will ever erase. */
    magazzino_scrivi("wifi_osp_pw", "ospiti-di-prima");
    magazzino_scrivi("wifi_pv2_pw", "privata-di-prima");
    /* A new key that exists already wins: it was written by this firmware. */
    magazzino_scrivi("wifi_pv3_pw", "vecchia");
    segreti_scrivi(SEG_WIFI_RETE_4_PASSWORD, "nuova");
    segreti_avvia();
    prova("the old guest password is under its new name",
          segreti_impostato(SEG_WIFI_RETE_1_PASSWORD));
    prova("...and so is the old second private one",
          segreti_impostato(SEG_WIFI_RETE_3_PASSWORD));
    prova("the old keys are gone",
          !magazzino_esiste("wifi_osp_pw") && !magazzino_esiste("wifi_pv2_pw")
          && !magazzino_esiste("wifi_pv3_pw"));
    char letto[32] = "";
    magazzino_leggi("wifi_share4_pw", letto, sizeof letto);
    prova("an existing new key is not overwritten by the old one",
          strcmp(letto, "nuova") == 0);
    magazzino_leggi("wifi_share1_pw", letto, sizeof letto);
    prova("and the moved password is the same password",
          strcmp(letto, "ospiti-di-prima") == 0);
    segreti_azzera();

    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutte le prove passano");
    return falliti ? 1 : 0;
}
