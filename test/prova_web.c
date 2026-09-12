/* ------------------------------------------------------------------------
 * Prova del server di configurazione — 03-config-contratto.md §6.
 *
 * Si chiama web_servi() direttamente invece di parlare in HTTP con se
 * stessi. Non e una scorciatoia: quello che va verificato sono **le regole
 * di accesso**, e un socket di mezzo aggiunge solo modi di fallire che non
 * c'entrano — porte occupate, tempi di attesa, un ordine di esecuzione che
 * cambia da una macchina all'altra. Il trasporto e provato dal fatto che il
 * simulatore serve la pagina; le regole si provano qui, e sono queste che
 * lasciano un pannello aperto sulla rete se si sbagliano.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivio.h"
#include "cJSON.h"
#include "config.h"
#include "segreti.h"
#include "translations.h"
#include "validazione.h"   /* cfg_albero() */
#include "web.h"

/* Le cartelle di appoggio stanno nella cartella di compilazione, che
   CMake passa in PROVE_DIR: sono prodotti come i binari, e nella radice
   del progetto facevano solo disordine. */
#ifndef PROVE_DIR
#define PROVE_DIR "."
#endif

static int falliti;

static void prova(const char *cosa, bool esito)
{
    printf("%-62s %s\n", cosa, esito ? "ok" : "FALLITA");
    if (!esito) falliti++;
}

/* Una richiesta, e il codice che ne esce. Il corpo finisce in `fuori` se
   serve guardarlo. */
static int chiedi(metodo_t m, const char *percorso, const char *query,
                  const char *corpo, char *fuori, size_t max)
{
    richiesta_t r = {
        .metodo = m, .percorso = percorso, .query = query ? query : "",
        .corpo = corpo ? corpo : "", .corpo_n = corpo ? strlen(corpo) : 0,
        .chiamante = "192.0.2.50",
    };
    risposta_t s = {0};
    web_servi(&r, &s);
    if (fuori) snprintf(fuori, max, "%s", s.corpo ? s.corpo : "");
    const int codice = s.codice;
    if (s.libera) s.libera((void *)s.corpo);
    return codice;
}

/* §2: quello che si applica a caldo si applica a caldo. Qui si conta
   soltanto se l'avviso parte, perche cosa ne faccia l'interfaccia — rileggere
   i dati e ridisegnare — non e affare del server. */
static int avvisi_di_cambio;
static void ho_sentito(void) { avvisi_di_cambio++; }
static int avvisi_di_testi;
static void testi_sentiti(void) { avvisi_di_testi++; }

/* Il tempo scorre a comando: quindici minuti costano un microsecondo. */
static uint32_t orologio;
static void avanza(uint32_t ms) { orologio += ms; web_tempo(orologio); }

/* Carica un'immagine finta a pezzi, come farebbe un browser.
 *
 * `buona` mette in testa il byte con cui comincia un'immagine ESP-IDF;
 * `completa` falso interrompe a meta, cioe dichiara piu byte di quanti ne
 * manda — che e esattamente quello che succede quando cade la rete. */
static bool ota_finto(size_t byte, bool buona, bool completa)
{
    static char immagine[8192];
    if (byte > sizeof immagine) byte = sizeof immagine;
    for (size_t n = 0; n < byte; n++) immagine[n] = (char)(n & 0xFF);
    immagine[0] = buona ? (char)0xE9 : 'P';

    const size_t mandati = completa ? byte : byte / 2;
    return web_prova_ota(immagine, mandati, byte);
}

int main(void)
{
    archivio_radice(PROVE_DIR "/prova_dati_web");
    web_quando_cambia(ho_sentito);
    web_when_texts_change(testi_sentiti);
    segreti_avvia();
    segreti_azzera();

    FILE *f = fopen("docs/04-config.example.json", "rb");
    prova("la configurazione d'esempio si apre", f != NULL);
    if (f) {
        static char buf[ARCHIVIO_MAX];
        const size_t n = fread(buf, 1, sizeof buf, f);
        fclose(f);
        archivio_scrivi(CFG_FILE, buf, n);
    }
    prova("si legge", cfg_carica() == CFG_LETTA);

    static char corpo[ARCHIVIO_MAX * 2];

    /* --- a chiave chiusa ------------------------------------------------
     * La regola che tiene in piedi tutto il resto: niente esiste finche
     * qualcuno non tocca l'interruttore sul muro. */
    web_tempo(0);
    prova("all'inizio e chiuso", !web_sbloccato());

    prova("la pagina non c'e",
          chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 404);
    prova("la configurazione non c'e",
          chiedi(HTTP_GET, "/api/config", NULL, NULL, NULL, 0) == 404);
    prova("l'esportazione non c'e",
          chiedi(HTTP_GET, "/api/config/export", NULL, NULL, NULL, 0) == 404);
    prova("lo stato dei segreti non c'e",
          chiedi(HTTP_GET, "/api/secrets", NULL, NULL, NULL, 0) == 404);
    prova("le entita non ci sono",
          chiedi(HTTP_GET, "/api/entities", NULL, NULL, NULL, 0) == 404);
    prova("non si puo scrivere la configurazione",
          chiedi(HTTP_POST, "/api/config", NULL, "{}", NULL, 0) == 404);
    prova("non si puo scrivere un segreto",
          chiedi(HTTP_POST, "/api/secrets", NULL,
                 "{\"ha_token\":\"entrato\"}", NULL, 0) == 404);
    prova("e infatti il segreto non e stato scritto",
          !segreti_impostato(SEG_HA_TOKEN));
    prova("non si puo far riavviare il pannello",
          chiedi(HTTP_POST, "/api/reboot", NULL, NULL, NULL, 0) == 404);
    /* La piu distruttiva delle azioni sta dietro alla stessa porta di tutte
       le altre, e non a una piu robusta: la porta e gia "essere stati in
       casa negli ultimi quindici minuti", e non c'e niente di piu forte da
       chiedere a chi e in casa. */
    prova("ne ripristinarlo di fabbrica",
          chiedi(HTTP_POST, "/api/reset", NULL, NULL, NULL, 0) == 404);
    /* L'aggiornamento non passa dal dispatcher — e un percorso a flusso, e
       il flusso lo scavalca — quindi il controllo dello sblocco e scritto
       una seconda volta dentro ota_apri(). Questa riga esiste per accorgersi
       il giorno che qualcuno lo togliesse credendolo un doppione: sarebbe
       una porta aperta sul firmware, la piu grossa di tutte. */
    prova("ne caricargli un firmware", !ota_finto(64, true, true));
    prova("non si leggono le traduzioni",
          chiedi(HTTP_GET, "/api/texts", NULL, NULL, NULL, 0) == 404
          && chiedi(HTTP_GET, "/api/texts/export", "lang=it", NULL, NULL, 0) == 404);
    prova("ne si scrivono",
          chiedi(HTTP_POST, "/api/texts", "lang=it",
                 "{\"panel\":{\"robot.clean_all\":\"x\"}}", NULL, 0) == 404
          && chiedi(HTTP_POST, "/api/texts/reset", "lang=it", NULL, NULL, 0) == 404);
    prova("un percorso inventato non dice niente di diverso",
          chiedi(HTTP_GET, "/admin", NULL, NULL, NULL, 0) == 404);

    /* --- la deroga: i due percorsi in sola lettura ---------------------- */
    avanza(2000);
    prova("lo stato risponde anche a chiave chiusa",
          chiedi(HTTP_GET, "/api/status", NULL, NULL, corpo, sizeof corpo) == 200);
    prova("e non contiene l'SSID",
          strstr(corpo, "ssid") == NULL && strstr(corpo, "SSID") == NULL);
    prova("ne alcun token", strstr(corpo, "token") == NULL);

    prova("una seconda richiesta nello stesso secondo viene fermata",
          chiedi(HTTP_GET, "/api/status", NULL, NULL, NULL, 0) == 429);
    avanza(WEB_INTERVALLO_MIN_MS);
    prova("dopo un secondo passa di nuovo",
          chiedi(HTTP_GET, "/api/status", NULL, NULL, NULL, 0) == 200);

    avanza(2000);
    prova("il registro risponde",
          chiedi(HTTP_GET, "/api/log", "n=5", NULL, corpo, sizeof corpo) == 200);
    prova("e restituisce righe", strstr(corpo, "\"lines\"") != NULL);

    /* --- sblocco --------------------------------------------------------- */
    web_sblocca();
    prova("sbloccato", web_sbloccato());
    prova("ora la pagina c'e",
          chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 200);
    prova("e lo schema anche",
          chiedi(HTTP_GET, "/api/schema", NULL, NULL, NULL, 0) == 200);
    /* L'elenco delle entita senza Home Assistant collegato: non e un
       errore ed e importante che non lo sembri. La pagina deve poter
       distinguere "non ce n'e" da "non ha risposto", perche nel primo caso
       resta ai campi di testo e nel secondo riprova. Un 404 le farebbe
       credere di essere su un firmware che questo percorso non ce l'ha,
       che e una terza cosa ancora. */
    /* --- le traduzioni ----------------------------------------------------
     *
     * Il dettaglio sta in test_translations.c; qui le strade e chi le puo
     * percorrere. */
    translations_reset("it");
    prova("la pagina chiede i suoi testi, nella lingua del pannello",
          chiedi(HTTP_GET, "/api/texts", NULL, NULL, corpo, sizeof corpo) == 200
          && strstr(corpo, "\"lang\":\"it\"") && strstr(corpo, "\"languages\""));
    prova("o in quella che sceglie",
          chiedi(HTTP_GET, "/api/texts", "lang=en", NULL, corpo, sizeof corpo) == 200
          && strstr(corpo, "\"lang\":\"en\""));
    prova("una lingua che non c'e e un 404, non l'inglese di nascosto",
          chiedi(HTTP_GET, "/api/texts", "lang=xx", NULL, NULL, 0) == 404);
    prova("l'editor ha il riferimento inglese e il testo di base",
          chiedi(HTTP_GET, "/api/texts", "lang=it&edit=1", NULL, corpo,
                 sizeof corpo) == 200
          && strstr(corpo, "\"en\":") && strstr(corpo, "\"base\":"));
    const int cambi_prima = avvisi_di_cambio;
    prova("una traduzione si salva",
          chiedi(HTTP_POST, "/api/texts", "lang=it",
                 "{\"panel\":{\"robot.clean_all\":\"Pulisci ogni stanza\"}}",
                 corpo, sizeof corpo) == 200);
    prova("e ridisegna le schermate senza ricollegare Home Assistant",
          avvisi_di_testi == 1 && avvisi_di_cambio == cambi_prima);
    prova("un segnaposto sbagliato e un 422 col campo e il motivo",
          chiedi(HTTP_POST, "/api/texts", "lang=it",
                 "{\"panel\":{\"access.named_open\":\"%d aperto\"}}",
                 corpo, sizeof corpo) == 422
          && strstr(corpo, "\"field\":\"panel.access.named_open\"")
          && strstr(corpo, "\"file_changed\":false"));
    prova("e non ridisegna niente", avvisi_di_testi == 1);
    {
        static char grande[256 * 1024];
        prova("l'esportazione e il file del progetto con la modifica",
              chiedi(HTTP_GET, "/api/texts/export", "lang=it", NULL, grande,
                     sizeof grande) == 200
              && strncmp(grande, "{\n  \"_meta\":", 12) == 0
              && strstr(grande, "\"robot.clean_all\": \"Pulisci ogni stanza\""));
    }
    prova("si torna ai testi inclusi",
          chiedi(HTTP_POST, "/api/texts/reset", "lang=it", NULL, NULL, 0) == 200
          && avvisi_di_testi == 2);

    prova("l'elenco entita risponde anche senza Home Assistant",
          chiedi(HTTP_GET, "/api/entities", NULL, NULL, corpo,
                 sizeof corpo) == 200);
    prova("e dice perche e vuoto invece di tacere",
          strstr(corpo, "\"entities\":[]") != NULL &&
          strstr(corpo, "connected") != NULL);

    prova("la configurazione si legge",
          chiedi(HTTP_GET, "/api/config", NULL, NULL, corpo, sizeof corpo) == 200);
    prova("ed e la configurazione vera",
          strstr(corpo, "Striscia LED divano") != NULL);

    /* --- 422: campi rifiutati, file intatto ------------------------------ */
    cJSON *doc = cJSON_Parse(corpo);
    prova("la si puo rileggere come JSON", doc != NULL);
    cJSON *schermo = cJSON_GetObjectItem(doc, "display");
    cJSON_SetNumberValue(cJSON_GetObjectItem(schermo, "brightness_active"), 900);
    char *rotta = cJSON_PrintUnformatted(doc);

    const int codice = chiedi(HTTP_POST, "/api/config", NULL, rotta,
                              corpo, sizeof corpo);
    prova("una configurazione fuori intervallo prende 422", codice == 422);
    prova("e dice quale campo",
          strstr(corpo, "display/brightness_active") != NULL);
    prova("e dice che il file non e stato toccato",
          strstr(corpo, "\"file_changed\":false") != NULL);
    /* Un avviso non e un rifiuto: se finisse fra i campi rifiutati, la
       pagina segnerebbe in rosso un campo che va benissimo. */
    prova("gli avvisi stanno in un elenco a parte",
          strstr(corpo, "\"warnings\"") != NULL);
    {
        cJSON *r = cJSON_Parse(corpo);
        const cJSON *campi = cJSON_GetObjectItem(r, "fields");
        bool solo_errori = true;
        for (const cJSON *c = campi ? campi->child : NULL; c; c = c->next)
            if (strstr(cJSON_GetStringValue(cJSON_GetObjectItem(c, "reason")),
                       "conservata"))
                solo_errori = false;
        prova("fra i campi rifiutati non c'e nessun avviso", solo_errori);
        cJSON_Delete(r);
    }

    /* Il file: quello di prima, non quello rifiutato. */
    prova("il file sul pannello e ancora quello valido",
          cfg_intero("display/brightness_active", 0) != 900);
    prova("e una configurazione rifiutata non avvisa nessuno",
          avvisi_di_cambio == 0);
    cJSON_free(rotta);

    /* --- salvataggio buono ---------------------------------------------- */
    cJSON_SetNumberValue(cJSON_GetObjectItem(schermo, "brightness_active"), 77);
    char *buona = cJSON_PrintUnformatted(doc);
    prova("una configurazione valida prende 200",
          chiedi(HTTP_POST, "/api/config", NULL, buona, NULL, 0) == 200);
    prova("e il valore nuovo e a bordo",
          cfg_intero("display/brightness_active", 0) == 77);
    /* Senza questo avviso l'interfaccia resterebbe con i dati di prima
       finche qualcuno non riavvia, che e proprio quello che §2 vieta. */
    prova("e chi disegna e stato avvisato di rileggere",
          avvisi_di_cambio == 1);
    cJSON_free(buona);
    cJSON_Delete(doc);

    /* --- segreti: si scrivono, non tornano indietro ---------------------- */
    prova("un segreto si scrive",
          chiedi(HTTP_POST, "/api/secrets", NULL,
                 "{\"ha_token\":\"NONDEVEUSCIREMAI\"}",
                 corpo, sizeof corpo) == 200);
    prova("la risposta dice solo che c'e, non cosa c'e",
          strstr(corpo, "NONDEVEUSCIREMAI") == NULL
          && strstr(corpo, "true") != NULL);
    prova("ed e davvero a bordo", segreti_impostato(SEG_HA_TOKEN));
    /* A new token is to be used, not only kept: without this notice the
       connection to Home Assistant stayed on the old token — or on "token
       refused" — until someone saved the configuration for another reason. */
    prova("and whoever draws was told, as for the configuration",
          avvisi_di_cambio == 2);

    chiedi(HTTP_GET, "/api/secrets", NULL, NULL, corpo, sizeof corpo);
    prova("nemmeno lo stato dei segreti lo restituisce",
          strstr(corpo, "NONDEVEUSCIREMAI") == NULL);
    chiedi(HTTP_GET, "/api/config", NULL, NULL, corpo, sizeof corpo);
    prova("ne l'esportazione della configurazione",
          strstr(corpo, "NONDEVEUSCIREMAI") == NULL);

    prova("un nome di segreto che non esiste non diventa una chiave",
          chiedi(HTTP_POST, "/api/secrets", NULL,
                 "{\"../../etc/passwd\":\"x\"}", corpo, sizeof corpo) == 200
          && strstr(corpo, "false") != NULL);
    prova("and writing nothing tells nobody", avvisi_di_cambio == 2);

    /* --- l'aggiornamento del firmware ------------------------------------
     *
     * Quello che si prova non e esp_ota_write, che e di Espressif: e la
     * **catena** nostra. Un'immagine arriva a pezzi da un socket, e in fondo
     * a quella catena c'e una partizione di avvio che cambia. Sbagliarla si
     * paga con un pannello a muro che non riparte, ed e per questo che il
     * porto esiste anche sul PC.
     *
     * I pezzi sono piccoli di proposito: e cosi che arrivano davvero, e un
     * difetto nel rimettere insieme i pezzi non si vede mai su un blocco
     * solo. */
    prova("un'immagine buona si installa", ota_finto(4096, true, true));
    prova("  e il pannello si riavvia da solo", web_riavvio_chiesto());

    /* Il criterio di 11-collaudo.md: un binario che non e un firmware viene
       rifiutato **prima** che la partizione di avvio cambi. Sul pannello a
       fermarlo e la firma; qui la magia iniziale, che e la stessa cosa nel
       punto in cui conta — alla chiusura, su tutto il blocco. */
    prova("un file che non e un firmware viene rifiutato",
          !ota_finto(4096, false, true));

    /* Una connessione che cade a meta non deve lasciare niente di
       installato: e il caso normale di un Wi-Fi domestico, non un caso
       limite. */
    prova("un trasferimento interrotto non installa niente",
          !ota_finto(4096, true, false));

    /* E dopo un'interruzione si deve poter riprovare: se la partizione
       restasse occupata servirebbe un riavvio per rimettersi in pari, e
       nessuno lo indovinerebbe. */
    prova("e dopo si puo riprovare", ota_finto(2048, true, true));

    /* --- il ripristino di fabbrica --------------------------------------
     *
     * Sta **in fondo** perche cancella tutto quello che le prove sopra
     * hanno scritto: metterlo prima vorrebbe dire riscrivere ogni volta lo
     * stato per le prove successive, e finire per provarlo su un pannello
     * gia vuoto — cioe non provare niente.
     *
     * Quello che si verifica non e il codice di risposta: e che dopo non ci
     * sia piu niente. Un ripristino che risponde 200 e lascia il token in
     * NVS e il difetto peggiore che questa funzione possa avere, perche
     * l'utente crede di aver cancellato. */
    prova("prima del ripristino il token c'e", segreti_impostato(SEG_HA_TOKEN));
    prova("e la configurazione anche", archivio_esiste(CFG_FILE));

    prova("il ripristino risponde che ha ripristinato",
          chiedi(HTTP_POST, "/api/reset", NULL, NULL,
                 corpo, sizeof corpo) == 200);
    prova("e chiede il riavvio", strstr(corpo, "restart") != NULL);
    prova("il riavvio e stato chiesto davvero", web_riavvio_chiesto());

    prova("dopo, il token non c'e piu", !segreti_impostato(SEG_HA_TOKEN));
    prova("ne nessun altro segreto", !segreti_impostato(SEG_WIFI_PASSWORD));
    prova("la configurazione e sparita", !archivio_esiste(CFG_FILE));
    prova("e anche la sua copia", !archivio_esiste(CFG_COPIA));

    /* --- la finestra si richiude da sola --------------------------------- */
    avanza(WEB_SBLOCCO_MS - 1000);
    prova("un minuto prima della scadenza e ancora aperta", web_sbloccato());
    prova("e la pagina si vede", chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 200);

    avanza(2000);
    prova("scaduta si richiude da sola", !web_sbloccato());
    prova("e la pagina torna a non esistere",
          chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 404);
    avanza(2000);
    prova("mentre lo stato risponde ancora",
          chiedi(HTTP_GET, "/api/status", NULL, NULL, NULL, 0) == 200);

    /* --- endpoint_stato: si puo spegnere anche quello -------------------- */
    cJSON *diag = cJSON_GetObjectItem(cfg_albero(), "diagnostics");
    cJSON_ReplaceItemInObject(diag, "status_endpoint", cJSON_CreateFalse());
    avanza(2000);
    prova("con endpoint_stato a falso non resta nessuna superficie esposta",
          chiedi(HTTP_GET, "/api/status", NULL, NULL, NULL, 0) == 404
          && chiedi(HTTP_GET, "/api/log", NULL, NULL, NULL, 0) == 404);

    /* --- la porta tenuta aperta per il montaggio -------------------------
     *
     * E una barriera spenta di proposito, quindi va provata proprio come si
     * prova una barriera accesa: che apra davvero, che apra **senza** che
     * nessuno abbia toccato il vetro, e soprattutto che spegnerla richiuda
     * subito — se restasse aperta fino al riavvio, rimetterla sarebbe una
     * promessa che il pannello non mantiene. */
    cJSON_ReplaceItemInObject(diag, "status_endpoint", cJSON_CreateTrue());
    web_richiudi();
    avanza(2000);
    prova("chiusa, la pagina non esiste",
          !web_sbloccato() && chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 404);

    cJSON_AddItemToObject(diag, "config_always_open",
                          cJSON_CreateTrue());
    avanza(2000);
    prova("con configurazione_sempre_aperta la pagina si apre da sola",
          web_sempre_aperta() && web_sbloccato()
          && chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 200);
    prova("e senza che nessuno abbia toccato il vetro",
          web_sblocco_resta_ms() == 0);

    /* Il tempo non la richiude: e la differenza fra questa e la finestra. */
    avanza(WEB_SBLOCCO_MS * 3);
    prova("tre finestre dopo e ancora aperta",
          web_sbloccato() && chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 200);

    cJSON_ReplaceItemInObject(diag, "config_always_open",
                              cJSON_CreateFalse());
    avanza(2000);
    prova("spegnendola si richiude subito, senza aspettare un riavvio",
          !web_sbloccato() && chiedi(HTTP_GET, "/", NULL, NULL, NULL, 0) == 404);

    segreti_azzera();
    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutte le prove passano");
    return falliti ? 1 : 0;
}
