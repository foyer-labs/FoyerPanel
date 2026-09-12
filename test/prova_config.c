/* ------------------------------------------------------------------------
 * Prova della configurazione — 11-collaudo.md §1, Fase 2.
 *
 *     ./prova_config                 esegue i controlli
 *     ./prova_config file.json       valida un file solo, come
 *                                    verifica_config.py, ed esce 0 o 1
 *
 * La seconda forma serve a tools/confronta_validazione.py, che passa le
 * stesse configurazioni a questo validatore e a quello Python e pretende lo
 * stesso verdetto. E l'unica cosa che tiene onesto un controllo scritto a
 * mano: senza, si allontana dallo schema un campo per volta e nessuno se ne
 * accorge finche una configurazione buona non viene rifiutata sul muro.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivio.h"
#include "cJSON.h"
#include "config.h"
#include "validazione.h"

/* Le cartelle di appoggio stanno nella cartella di compilazione, che
   CMake passa in PROVE_DIR: sono prodotti come i binari, e nella radice
   del progetto facevano solo disordine. */
#ifndef PROVE_DIR
#define PROVE_DIR "."
#endif

static int errori;

#define ESIGE(cond, ...)                                                      \
    do {                                                                      \
        printf("  %-4s ", (cond) ? "ok" : "NO");                              \
        printf(__VA_ARGS__);                                                  \
        printf("\n");                                                         \
        if (!(cond)) errori++;                                                \
    } while (0)

/* --- lettura del file di esempio ---------------------------------------- */

static char esempio[ARCHIVIO_MAX + 1];

/* The same example as it was at schema 10, in Italian: the file a panel of
   the original project carries. The migration tests start from it — an old
   schema number on an English document would be a file no panel ever had —
   and so does the test that the whole migration lands on today's example. */
static char esempio_it[ARCHIVIO_MAX + 1];
#define ESEMPIO_IT "test/config-schema10-it.json"

static bool leggi_in(const char *percorso, char *buf, size_t max)
{
    FILE *f = fopen(percorso, "rb");
    if (!f) return false;
    const size_t n = fread(buf, 1, max - 1, f);
    fclose(f);
    buf[n] = 0;
    return n > 0;
}

static bool leggi_esempio(const char *percorso)
{
    return leggi_in(percorso, esempio, sizeof esempio);
}

/* Scrive un file nell'archivio senza passare dalla configurazione: serve a
   preparare le condizioni di partenza, comprese quelle rotte. */
static void metti(const char *nome, const char *testo)
{
    archivio_scrivi(nome, testo, strlen(testo));
}

/* --- una configurazione con un campo cambiato --------------------------- */

/* Applica una modifica al documento di esempio e restituisce il testo.
   `percorso` usa la barra, come gli errori: "display/brightness_active". */
static char *muta(const char *percorso, cJSON *valore)
{
    cJSON *c = cJSON_Parse(esempio);
    if (!c) return NULL;

    char copia[128];
    snprintf(copia, sizeof copia, "%s", percorso);
    char *ultimo = strrchr(copia, '/');
    cJSON *nodo = c;

    if (ultimo) {
        *ultimo = 0;
        char *p = copia;
        while (*p && nodo) {
            char *barra = strchr(p, '/');
            if (barra) *barra = 0;
            nodo = (p[0] >= '0' && p[0] <= '9' && cJSON_IsArray(nodo))
                 ? cJSON_GetArrayItem(nodo, atoi(p))
                 : cJSON_GetObjectItem(nodo, p);
            if (!barra) break;
            p = barra + 1;
        }
        ultimo++;
    } else {
        ultimo = copia;
    }

    if (nodo) {
        cJSON_DeleteItemFromObject(nodo, ultimo);
        if (valore) cJSON_AddItemToObject(nodo, ultimo, valore);
    } else if (valore) {
        cJSON_Delete(valore);
    }

    char *testo = cJSON_PrintUnformatted(c);
    cJSON_Delete(c);
    return testo;
}

/* Vero se la validazione rifiuta, e il primo errore riguarda `atteso`. */
static bool rifiutata(const char *percorso, cJSON *valore, const char *atteso)
{
    char *testo = muta(percorso, valore);
    if (!testo) return false;

    cJSON *c = cJSON_Parse(testo);
    const bool ok = validazione_esegui(c);
    cJSON_Delete(c);
    free(testo);

    if (ok) return false;
    if (!atteso) return true;

    for (int n = 0; n < cfg_errori(); n++) {
        const errore_cfg_t *e = cfg_errore(n);
        if (e->gravita == CFG_ERRORE && strstr(e->campo, atteso)) return true;
    }
    return false;
}

/* Il contrario di rifiutata(): il documento con quella modifica passa la
   validazione. Serve dove una regola si allarga — allargarla e facile,
   allargarla troppo pure, e senza una prova che dica "questo deve entrare"
   resterebbe solo quella che dice "questo deve restare fuori". */
static bool accettata(const char *percorso, cJSON *valore)
{
    char *testo = muta(percorso, valore);
    if (!testo) return false;

    cJSON *c = cJSON_Parse(testo);
    const bool ok = validazione_esegui(c);
    cJSON_Delete(c);
    free(testo);
    return ok;
}

/* --- la forma da riga di comando ---------------------------------------- */

static int valida_file(const char *percorso)
{
    if (!leggi_esempio(percorso)) {
        fprintf(stderr, "non riesco a leggere %s\n", percorso);
        return 2;
    }
    cJSON *c = cJSON_Parse(esempio);
    if (!c) {
        printf("(radice): JSON non valido\n");
        return 1;
    }
    const bool ok = validazione_esegui(c);
    cJSON_Delete(c);

    for (int n = 0; n < cfg_errori(); n++) {
        const errore_cfg_t *e = cfg_errore(n);
        printf("%s  %s: %s\n", e->gravita == CFG_ERRORE ? "errore" : "avviso",
               e->campo, e->motivo);
    }
    return ok ? 0 : 1;
}

/* --- i controlli -------------------------------------------------------- */

/* --- il pannello appena uscito di fabbrica ------------------------------
 *
 * Nessun file, nessuna copia: la condizione di un apparecchio appena montato
 * al muro. Che cfg_carica() dica CFG_PREDEFINITA lo si provava gia; quello
 * che non si provava e se in quello stato si possa **scrivere** — ed e la
 * prima cosa che chiunque fa, il nome della rete Wi-Fi, prima ancora di
 * avere una rete.
 *
 * Non si poteva: cfg_carica() tornava senza lasciare nessun documento in
 * memoria, e ogni accessore cadeva sul proprio ripiego cosi bene che a
 * schermo non si vedeva niente di strano. Il primo sintomo e arrivato dalla
 * console del pannello vero, con un "non sono riuscito a salvare" che non
 * spiegava niente.
 *
 * **Perche in un processo suo e non insieme alle altre prove.** Il documento
 * e una variabile statica che vive quanto il processo. Le prove che vengono
 * prima ne lasciano uno buono, e il vecchio ramo del primo avvio usciva
 * senza toccarlo: la scrittura riusciva, appoggiandosi al documento di
 * un'altra prova, e il difetto restava invisibile. Sul pannello cfg_carica()
 * viene chiamata **una volta sola**, con il documento ancora vuoto, e quella
 * condizione dentro un processo gia usato non si ricrea. Da qui la modalita
 * dedicata: un processo appena nato, come il pannello all'accensione.
 *
 * Vale come regola oltre questo caso: uno stato globale che sopravvive fra
 * una prova e l'altra non rende le prove piu comode, le rende meno capaci di
 * dire la verita. */
static int primo_avvio(void)
{
    archivio_radice(PROVE_DIR "/prova_dati_primo");
    archivio_cancella(CFG_FILE);
    archivio_cancella(CFG_COPIA);

    ESIGE(cfg_carica() == CFG_PREDEFINITA,
          "senza file si parte dal primo avvio");
    ESIGE(cfg_imposta_testo("system/network/ssid", "PortaDiCasa"),
          "e al primo avvio si puo comunque scrivere");
    ESIGE(cfg_salva(), "e salvare");

    ESIGE(cfg_carica() == CFG_LETTA,
          "al giro dopo quel file c'e e si legge");
    ESIGE(strcmp(cfg_testo("system/network/ssid", ""), "PortaDiCasa") == 0,
          "col valore scritto prima, sopravvissuto al riavvio");

    /* Gli oggetti intermedi non c'erano e li ha creati chi scriveva: senza,
       il valore avrebbe avuto un percorso senza strada sotto. */
    ESIGE(cfg_imposta_testo("system/panel_name", "Ingresso") &&
          strcmp(cfg_testo("system/panel_name", ""), "Ingresso") == 0,
          "un secondo campo nello stesso oggetto si aggiunge");
    ESIGE(strcmp(cfg_testo("system/network/ssid", ""), "PortaDiCasa") == 0,
          "senza cancellare il primo");

    printf("\n%d falliti\n", errori);
    return errori == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *sorgente = "docs/04-config.example.json";

    if (argc > 1 && strcmp(argv[1], "--primo-avvio") == 0)
        return primo_avvio();
    if (argc > 1 && argv[1][0] != '-') return valida_file(argv[1]);
    if (argc > 2) sorgente = argv[2];

    if (!leggi_esempio(sorgente)) {
        fprintf(stderr, "non riesco a leggere %s\n", sorgente);
        return 2;
    }
    if (!leggi_in(ESEMPIO_IT, esempio_it, sizeof esempio_it)) {
        fprintf(stderr, "non riesco a leggere %s\n", ESEMPIO_IT);
        return 2;
    }
    archivio_radice(PROVE_DIR "/prova_dati");

    /* --- la configurazione vera passa ------------------------------------ */
    {
        cJSON *c = cJSON_Parse(esempio);
        const bool ok = validazione_esegui(c);
        int avvisi = 0;
        for (int n = 0; n < cfg_errori(); n++)
            if (cfg_errore(n)->gravita == CFG_AVVISO) avvisi++;
        cJSON_Delete(c);
        ESIGE(ok, "la configurazione della casa passa la validazione");
        /* Zero avvisi, non uno. Se ne ricompare uno, e una novita da
           guardare. */
        ESIGE(avvisi == 0, "e senza avvisi (ne ho contati %d)", avvisi);
    }

    /* --- il livello 1, campo per campo ----------------------------------- */
    ESIGE(rifiutata("display/brightness_active", cJSON_CreateNumber(101),
                    "brightness_active"), "luminosita oltre 100 rifiutata");
    ESIGE(rifiutata("display/standby_after_s", cJSON_CreateNumber(5),
                    "standby_after_s"), "standby sotto il minimo rifiutato");
    ESIGE(rifiutata("system/profile", cJSON_CreateString("p4-9999x9999"),
                    "profile"), "un profilo inventato viene rifiutato");
    ESIGE(rifiutata("home_assistant/port", cJSON_CreateNumber(0), "port"),
          "porta zero rifiutata");
    ESIGE(rifiutata("home_assistant/host",
                    cJSON_CreateString("http://192.0.2.10"), "host"),
          "un host con lo schema davanti viene rifiutato");
    /* Una luce puo essere `light.` o `switch.` — succede coi rele e con
       certe integrazioni — ma non un dominio qualsiasi. */
    ESIGE(rifiutata("lights/zones/0/entity", cJSON_CreateString("sensor.salotto"),
                    "entity"), "una zona luci che non e ne light ne switch viene rifiutata");
    ESIGE(accettata("lights/zones/0/entity", cJSON_CreateString("switch.salotto")),
          "una zona luci puo essere un interruttore");
    ESIGE(rifiutata("climate/heating/0/climate",
                    cJSON_CreateString("sensor.salotto"), "climate"),
          "una zona clima che non e un climate viene rifiutata");
    ESIGE(rifiutata("presence/sensor", NULL, "sensor"),
          "il sensore di presenza e obbligatorio");
    ESIGE(rifiutata("sections", cJSON_CreateArray(), "sections"),
          "nessuna sezione viene rifiutata");

    /* --- il livello 2, le regole fra campi ------------------------------- */
    ESIGE(rifiutata("display/brightness_standby", cJSON_CreateNumber(99),
                    "brightness_standby"),
          "standby piu luminoso dell'attivo viene rifiutato");
    ESIGE(rifiutata("display/off_after_s", cJSON_CreateNumber(30),
                    "off_after_s"),
          "spegnimento prima dello standby viene rifiutato");
    ESIGE(rifiutata("access/0/pulse_ms", NULL, "pulse_ms"),
          "un accesso a impulso senza impulso_ms viene rifiutato");
    ESIGE(rifiutata("climate/heating_limits/max", cJSON_CreateNumber(10),
                    "heating_limits"),
          "limiti con min oltre max vengono rifiutati");

    /* --- l'archivio ------------------------------------------------------ */
    archivio_cancella(CFG_FILE);
    archivio_cancella(CFG_COPIA);
    archivio_cancella("config.json.bak");

    metti(CFG_FILE, esempio);
    ESIGE(cfg_carica() == CFG_LETTA, "una configurazione buona si legge");
    ESIGE(strcmp(cfg_testo("system/panel_name", ""), "Ingresso") == 0,
          "e i valori si leggono per percorso");
    ESIGE(cfg_quanti("lights/zones") == 13, "tredici zone luci");
    ESIGE(cfg_ha_in("access", 2, "state_sensor"),
          "la porta del garage ha il sensore di stato");
    ESIGE(!cfg_ha_in("access", 0, "state_sensor"),
          "il cancello pedonale no, ed e la differenza che conta");

    /* Scrittura atomica: dopo un salvataggio il precedente e ancora li. */
    ESIGE(cfg_salva(), "la configurazione si salva");
    ESIGE(archivio_esiste("config.json.bak"),
          "e il precedente diventa la copia di sicurezza");

    /* Un file principale illeggibile fa ripartire dalla copia. */
    metti(CFG_COPIA, esempio);
    metti(CFG_FILE, "{ questo non e JSON");
    ESIGE(cfg_carica() == CFG_DA_COPIA,
          "un config.json corrotto fa ripartire dalla copia");

    /* Corrotti entrambi, si va al primo avvio senza restare bloccati. */
    metti(CFG_COPIA, "neanche questo");
    ESIGE(cfg_carica() == CFG_PREDEFINITA,
          "corrotti entrambi si va al primo avvio");

    /* Uno schema piu nuovo del nostro si rifiuta. */
    {
        char *futura = muta("schema", cJSON_CreateNumber(CFG_SCHEMA + 1));
        metti(CFG_FILE, futura);
        free(futura);
        ESIGE(cfg_carica() == CFG_TROPPO_NUOVA,
              "uno schema piu nuovo del firmware viene rifiutato");
    }

    /* Uno schema vecchio si migra e si riscrive. */
    {
        char *vecchia = NULL;
        char *senza_home = NULL;
        {
            cJSON *c = cJSON_Parse(esempio_it);
            cJSON_SetNumberValue(cJSON_GetObjectItem(c, "schema"), 1);
            cJSON_DeleteItemFromObject(c, "home");
            senza_home = cJSON_PrintUnformatted(c);
            cJSON_Delete(c);
        }
        metti(CFG_FILE, senza_home);
        free(vecchia);
        free(senza_home);

        ESIGE(cfg_carica() == CFG_LETTA, "uno schema vecchio si migra");
        ESIGE(cfg_intero("schema", 0) == CFG_SCHEMA,
              "e il file riscritto porta la versione nuova");
        ESIGE(cJSON_GetObjectItem(cfg_albero(), "home") != NULL,
              "la migrazione crea il blocco che agli schemi dopo serve");
    }

    /* Lo schema 4 aveva le telecamere. Non basta smettere di leggerle: lo
       schema nuovo non ammette campi che non conosce, quindi una
       configurazione che se le portasse dietro verrebbe rifiutata al primo
       salvataggio — con un errore su un campo che chi configura non ha mai
       scritto e che la pagina non gli mostra piu. */
    {
        cJSON *c = cJSON_Parse(esempio_it);
        cJSON_SetNumberValue(cJSON_GetObjectItem(c, "schema"), 4);

        /* --- lo stesso nome, due cose diverse -------------------------
         *
         * L'esempio di oggi ha gia un blocco `telecamere`, ed e' quello
         * dello schema 6: un **elenco** di indirizzi RTSP. Quello che
         * questa prova vuole mettere e' quello dello schema 4: un
         * **oggetto** con dentro l'indirizzo di go2rtc.
         *
         * Senza toglierlo prima, nel documento finiscono due chiavi con lo
         * stesso nome, la migrazione ne cancella una e resta l'altra — e la
         * prova fallisce dicendo che la migrazione non funziona, quando
         * invece funziona benissimo su una configurazione che nessuno
         * scriverebbe mai. */
        cJSON_DeleteItemFromObject(c, "telecamere");

        cJSON *cam = cJSON_AddObjectToObject(c, "telecamere");
        cJSON_AddStringToObject(cam, "base_url", "http://192.0.2.10:1984");
        cJSON *el = cJSON_AddArrayToObject(cam, "elenco");
        cJSON *uno = cJSON_CreateObject();
        cJSON_AddStringToObject(uno, "id", "ingresso");
        cJSON_AddStringToObject(uno, "nome", "Ingresso");
        cJSON_AddStringToObject(uno, "sorgente", "ingresso");
        cJSON_AddItemToArray(el, uno);

        cJSON *home = cJSON_GetObjectItem(c, "home");
        if (!home) home = cJSON_AddObjectToObject(c, "home");
        cJSON_AddBoolToObject(home, "striscia_telecamere", true);

        cJSON_AddItemToArray(cJSON_GetObjectItem(c, "sezioni"),
                             cJSON_CreateString("telecamere"));

        cJSON_AddStringToObject(
            cJSON_GetArrayItem(cJSON_GetObjectItem(c, "accessi"), 0),
            "telecamera", "ingresso");

        char *testo = cJSON_PrintUnformatted(c);
        cJSON_Delete(c);
        metti(CFG_FILE, testo);
        free(testo);

        ESIGE(cfg_carica() == CFG_LETTA,
              "una configurazione con le telecamere si migra");

        const cJSON *d = cfg_albero();
        ESIGE(!cJSON_GetObjectItem(d, "telecamere"),
              "e il blocco telecamere non c'e piu");
        ESIGE(!cJSON_GetObjectItem(cJSON_GetObjectItem(d, "home"),
                                   "striscia_telecamere"),
              "ne la striscia in home");
        ESIGE(!cJSON_GetObjectItem(
                  cJSON_GetArrayItem(cJSON_GetObjectItem(d, "access"), 0),
                  "telecamera"),
              "ne la telecamera di un accesso");

        bool c_e_ancora = false;
        const cJSON *s = NULL;
        cJSON_ArrayForEach(s, cJSON_GetObjectItem(d, "sections"))
            if (cJSON_IsString(s) && strcmp(s->valuestring, "telecamere") == 0)
                c_e_ancora = true;
        ESIGE(!c_e_ancora, "e la sezione esce dall'elenco");

        /* E soprattutto: quello che resta deve passare la validazione, che
           e' la cosa che sarebbe rotta se la migrazione non ci fosse. */
        ESIGE(validazione_esegui(d), "e la configurazione migrata e valida");
    }

    /* --- le telecamere sono tornate per un'ora, e poi via ---------------
     *
     * Lo schema 6 le riammetteva, in RTSP: un elenco di indirizzi. Lo
     * schema 7 le toglie. Non e' una configurazione di laboratorio — e'
     * esistita davvero sul pannello di casa, la mattina del 09/09/2026, e
     * quindi la migrazione che la smonta va percorsa da una prova come
     * tutte le altre.
     *
     * Il perche' della rinuncia e' in 05-architettura-firmware.md §5: il
     * decodificatore H.264 di questo silicio e' software e accetta il solo
     * profilo baseline, e i flussi di casa sono sopra. */
    {
        cJSON *c = cJSON_Parse(esempio_it);
        cJSON_SetNumberValue(cJSON_GetObjectItem(c, "schema"), 6);

        cJSON *el = cJSON_AddArrayToObject(c, "telecamere");
        cJSON *uno = cJSON_CreateObject();
        cJSON_AddStringToObject(uno, "nome", "Ingresso");
        cJSON_AddStringToObject(uno, "url", "rtsp://192.0.2.30:554/stream2");
        cJSON_AddItemToArray(el, uno);

        cJSON_AddItemToArray(cJSON_GetObjectItem(c, "sezioni"),
                             cJSON_CreateString("telecamere"));

        char *testo = cJSON_PrintUnformatted(c);
        cJSON_Delete(c);
        metti(CFG_FILE, testo);
        free(testo);

        ESIGE(cfg_carica() == CFG_LETTA,
              "una configurazione con le telecamere in RTSP si migra");
        ESIGE(cfg_intero("schema", 0) == CFG_SCHEMA, "e arriva allo schema 7");

        const cJSON *d = cfg_albero();
        ESIGE(!cJSON_GetObjectItem(d, "telecamere"),
              "e l'elenco degli indirizzi non c'e piu");

        bool c_e_ancora = false;
        const cJSON *s = NULL;
        cJSON_ArrayForEach(s, cJSON_GetObjectItem(d, "sections"))
            if (cJSON_IsString(s) && strcmp(s->valuestring, "telecamere") == 0)
                c_e_ancora = true;
        ESIGE(!c_e_ancora, "ne la sezione nell'elenco");

        /* E quello che resta deve passare la validazione: e' la cosa che
           sarebbe rotta senza la migrazione, perche' lo schema non ammette
           campi che non conosce. */
        ESIGE(validazione_esegui(d), "e la configurazione migrata e valida");
    }

    /* Lo schema 2 aveva una sola rete privata, e i tempi del PIN dentro le
       reti. Una migrazione che non venisse mai eseguita da una prova
       sarebbe una migrazione scritta a occhi chiusi: qui si costruisce una
       configurazione com'era e si guarda dov'e finita ogni cosa. */
    {
        cJSON *c = cJSON_Parse(esempio_it);
        cJSON_SetNumberValue(cJSON_GetObjectItem(c, "schema"), 2);

        cJSON *cw = cJSON_GetObjectItem(c, "condivisione_wifi");
        cJSON_DeleteItemFromObject(cw, "private");
        cJSON_DeleteItemFromObject(cw, "tentativi_pin");
        cJSON_DeleteItemFromObject(cw, "minuti_blocco");
        cJSON_DeleteItemFromObject(cw, "secondi_visibilita");

        cJSON *p = cJSON_AddObjectToObject(cw, "privata");
        cJSON_AddStringToObject(p, "nome_mostrato", "Rete di casa");
        cJSON_AddStringToObject(p, "ssid", "CasaEsempio");
        cJSON_AddBoolToObject(p, "attiva", true);
        cJSON_AddNumberToObject(p, "tentativi_pin", 5);
        cJSON_AddNumberToObject(p, "minuti_blocco", 7);
        cJSON_AddBoolToObject(p, "richiedi_pin", true);

        char *vecchia2 = cJSON_PrintUnformatted(c);
        cJSON_Delete(c);
        metti(CFG_FILE, vecchia2);
        free(vecchia2);

        ESIGE(cfg_carica() == CFG_LETTA, "anche lo schema 2 si migra");
        ESIGE(cfg_quanti("wifi_sharing/private") == 1,
              "la rete privata diventa il primo elemento dell'elenco");
        ESIGE(strcmp(cfg_testo_in("wifi_sharing/private", 0, "ssid", ""),
                     "CasaEsempio") == 0,
              "con dentro quello che c'era");
        /* Nello stesso caricamento passa anche la 3->4, che il PIN lo
           toglie: i campi che lo governavano non sono piu ammessi dallo
           schema, e se restassero il primo salvataggio verrebbe rifiutato
           su campi che nessuno ha mai scritto. Vanno cercati dove stavano
           allo schema 2 — **dentro** le reti — e non solo dove stavano
           allo schema 3. */
        ESIGE(cfg_intero("wifi_sharing/tentativi_pin", -1) == -1,
              "i tempi del PIN se ne vanno con il PIN");
        ESIGE(!cfg_ha_in("wifi_sharing/private", 0, "tentativi_pin"),
              "anche quelli rimasti dentro la rete dallo schema 2");
        ESIGE(!cfg_ha_in("wifi_sharing/private", 0, "richiedi_pin"),
              "e cosi la richiesta del PIN");
    }

    /* --- 10 -> 11: the Italian file lands exactly on the English one -----
     *
     * The strongest thing a key-renaming migration can be asked: take the
     * example as it was at schema 10, in Italian, and load it. What comes
     * out must be **the same document** as today's English example — not
     * similar, equal, key by key and value by value. A key the table
     * forgot, a value renamed in the schema and not in the migration, and
     * this fails; it is also the proof that a panel from the original
     * project can move to this one without touching its configuration. */
    {
        metti(CFG_FILE, esempio_it);
        ESIGE(cfg_carica() == CFG_LETTA, "the schema-10 Italian example loads");
        cJSON *atteso = cJSON_Parse(esempio);
        ESIGE(atteso && cJSON_Compare(cfg_albero(), atteso, true),
              "and becomes exactly the English example, key by key");
        if (atteso && !cJSON_Compare(cfg_albero(), atteso, true)) {
            /* Where they differ, for whoever has to fix it. */
            const cJSON *x = NULL;
            cJSON_ArrayForEach(x, atteso) {
                const cJSON *y = cJSON_GetObjectItem(cfg_albero(), x->string);
                if (!y || !cJSON_Compare(x, y, true))
                    printf("       differs under \"%s\"\n", x->string);
            }
            cJSON_ArrayForEach(x, cfg_albero())
                if (!cJSON_GetObjectItem(atteso, x->string))
                    printf("       left over: \"%s\"\n", x->string);
        }
        cJSON_Delete(atteso);
        ESIGE(validazione_esegui(cfg_albero()), "and it is valid");

        /* The same file through the configuration page: imported, it is
           migrated before it is judged, not refused over its old names. */
        metti(CFG_FILE, esempio);
        cfg_carica();
        ESIGE(cfg_sostituisci(esempio_it, strlen(esempio_it)),
              "an Italian file imported from the page is migrated and saved");
        ESIGE(strcmp(cfg_testo("system/panel_name", ""), "Ingresso") == 0,
              "and reads with the new names");
    }

    /* Un salvataggio piu grande del massimo viene rifiutato. */
    {
        static char grosso[ARCHIVIO_MAX + 100];
        memset(grosso, 'x', sizeof grosso - 1);
        ESIGE(!cfg_sostituisci(grosso, sizeof grosso - 1),
              "una configurazione oltre i 24 kB viene rifiutata");
    }

    /* Una configurazione non valida non tocca il file. */
    {
        metti(CFG_FILE, esempio);
        cfg_carica();
        char *rotta = muta("presence/sensor", NULL);
        const bool salvata = cfg_sostituisci(rotta, strlen(rotta));
        free(rotta);
        ESIGE(!salvata, "una configurazione non valida non viene salvata");
        ESIGE(cfg_errori() > 0, "e i campi rifiutati sono leggibili");

        cfg_carica();
        ESIGE(strcmp(cfg_testo("presence/sensor", ""),
                     "sensor.panel_presence") == 0,
              "il file su disco e rimasto quello di prima");
    }

    /* --- salvataggio interrotto a meta -------------------------------------
     *
     * Su un pannello a muro "manca la corrente durante il salvataggio" non
     * e un caso di scuola. La scrittura e atomica proprio per questo, ma
     * "e atomica" era finora un'affermazione: qui si prova.
     *
     * I due momenti in cui l'interruzione fa danno sono uno prima e uno
     * dopo la rinomina. Prima: c'e un temporaneo a meta e il file buono e
     * intatto — e non deve succedere niente. Dopo: il file principale e
     * quello nuovo e completo. Il caso peggiore, un principale troncato,
     * lo copre gia il ripiego sulla copia, ma vale la pena vederlo
     * accadere con un file mezzo scritto invece che con "{ non e JSON".
     */
    {
        metti(CFG_FILE, esempio);
        ESIGE(cfg_carica() == CFG_LETTA, "si riparte da una buona");
        ESIGE(cfg_salva(), "si salva, cosi la copia esiste");

        /* Interruzione prima della rinomina: resta un temporaneo a meta. */
        metti("config.json.tmp", "{\"schema\": 2, \"sistema\": {\"nome_pan");
        ESIGE(cfg_carica() == CFG_LETTA,
              "un temporaneo a meta non viene raccolto da nessuno");
        ESIGE(strcmp(cfg_testo("system/panel_name", ""), "Ingresso") == 0,
              "e la configurazione buona e ancora quella");
        archivio_cancella("config.json.tmp");

        /* Interruzione a meta del principale: si riparte dalla copia, che
           e completa perche e stata rinominata tutta insieme. */
        {
            static char meta[2048];
            snprintf(meta, sizeof meta, "%.900s", esempio);
            metti(CFG_FILE, meta);
        }
        ESIGE(cfg_carica() == CFG_DA_COPIA,
              "un principale troncato fa ripartire dalla copia");
        ESIGE(strcmp(cfg_testo("system/panel_name", ""), "Ingresso") == 0,
              "e la copia e intera, non a meta");
    }

    /* --- §8: quello che non conosciamo si conserva -------------------------
     *
     * Un config.json esportato da un pannello e importato in un altro deve
     * tornare indietro intatto. E la ragione per cui il documento
     * resta un albero JSON invece di diventare una struttura C: una struct
     * perderebbe tutto quello che non prevede, e l'esportazione smetterebbe
     * di essere una copia di sicurezza per diventare una copia parziale.
     *
     * Finora era vero per costruzione. Adesso e vero e si vede.
     */
    {
        char *con_ignoti = NULL;
        {
            cJSON *c = cJSON_Parse(esempio);
            /* Una sezione intera che questo firmware non conosce... */
            cJSON *futura = cJSON_AddObjectToObject(c, "aspirapolvere");
            cJSON_AddStringToObject(futura, "entity", "vacuum.roborock");
            cJSON_AddNumberToObject(futura, "rooms", 7);
            /* ...e un campo sconosciuto dentro un oggetto conosciuto. */
            cJSON_AddStringToObject(cJSON_GetObjectItem(c, "system"),
                                    "colore_del_muro", "grigio chiaro");
            con_ignoti = cJSON_PrintUnformatted(c);
            cJSON_Delete(c);
        }

        metti(CFG_FILE, con_ignoti);
        ESIGE(cfg_carica() == CFG_LETTA,
              "campi sconosciuti non fanno rifiutare il documento");

        /* Il giro completo: salvato dal pannello, riletto dal pannello. */
        ESIGE(cfg_salva(), "si risalva");
        ESIGE(cfg_carica() == CFG_LETTA, "e si rilegge");

        char *fuori = cfg_esporta(false);
        ESIGE(fuori && strstr(fuori, "aspirapolvere"),
              "una sezione sconosciuta sopravvive al giro");
        ESIGE(fuori && strstr(fuori, "vacuum.roborock"),
              "coi suoi valori, non svuotata");
        ESIGE(fuori && strstr(fuori, "colore_del_muro"),
              "e cosi un campo sconosciuto dentro un oggetto conosciuto");
        ESIGE(fuori && strstr(fuori, "Striscia LED divano"),
              "senza che il resto si sia perso per strada");
        cfg_libera_testo(fuori);
        free(con_ignoti);
    }

    /* --- un intero scritto fra virgolette ------------------------------
     *
     * Non e' un caso di scuola: la pagina di configurazione ha salvato per
     * settimane `"potenza_massima": "1000"` con le virgolette, perche' non
     * riconosceva come numerico un campo dichiarato
     * `anyOf: [{integer}, {null}]`. cfg_intero_in() ripiegava su zero, e con
     * zero la barretta del consumo di lavatrice e asciugatrice non si
     * disegnava. Nessun errore da nessuna parte.
     *
     * La pagina adesso salva numeri; questo lato resta tollerante perche'
     * una configurazione salvata da una versione precedente deve continuare
     * a valere. Tollerante, non credulone: quello che numero non e', non
     * diventa numero. */
    {
        char *doc;
        {
            cJSON *c = cJSON_Parse(esempio);
            cJSON *e = cJSON_GetObjectItem(c, "appliances");
            cJSON *primo = cJSON_GetArrayItem(e, 0);
            cJSON_DeleteItemFromObject(primo, "max_power");
            cJSON_AddStringToObject(primo, "max_power", "1000");
            cJSON *secondo = cJSON_GetArrayItem(e, 1);
            cJSON_DeleteItemFromObject(secondo, "max_power");
            cJSON_AddStringToObject(secondo, "max_power", "2000 W");
            doc = cJSON_PrintUnformatted(c);
            cJSON_Delete(c);
        }
        metti(CFG_FILE, doc);
        ESIGE(cfg_carica() == CFG_LETTA, "un intero fra virgolette si legge");
        ESIGE(cfg_intero_in("appliances", 0, "max_power", 0) == 1000,
              "e vale come il numero che e'");
        ESIGE(cfg_intero_in("appliances", 1, "max_power", -1) == -1,
              "ma «2000 W» non e' un numero e vale il ripiego");
        free(doc);
    }

    printf("\n%d falliti\n", errori);
    return errori == 0 ? 0 : 1;
}
