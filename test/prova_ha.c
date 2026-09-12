/* ------------------------------------------------------------------------
 * Prova del client Home Assistant — 11-collaudo.md §1, Fase 3.
 *
 * Parla davvero WebSocket con `tools/finto_ha.py`, che non e un'emulazione
 * ma un attrezzo: serve a far **succedere** le situazioni che il collaudo
 * elenca e che con Home Assistant vero si possono solo aspettare. Un token
 * sbagliato, con Home Assistant vero, si prova andando a rovinarne uno; una
 * caduta a meta di un comando e una questione di tempismo. Qui sono due
 * opzioni sulla riga di comando.
 *
 *     ./prova_ha PORTA SCENARIO
 *
 * Lo scenario dice cosa deve succedere, e chi avvia il finto server con le
 * opzioni giuste e tools/prova_ha.py.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "archivio.h"
#include "cJSON.h"
#include "config.h"
#include "entita.h"
#include "dati.h"
#include "ha.h"
#include "orologio.h"
#include "segreti.h"
#include "validazione.h"
#include "ws.h"

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

/* --- orologio della prova ------------------------------------------------
 * Il tempo scorre a comando, come nelle prove delle temporizzazioni: cosi
 * l'attesa di riconnessione si verifica senza aspettarla davvero. Ma il
 * traffico di rete e vero, quindi ogni passo dorme un millesimo: senza, si
 * girerebbe a vuoto consumando un core.
 */
static uint32_t orologio;

static void passo(uint32_t ms)
{
    orologio += ms;
    ha_gira(orologio);
    struct timespec t = { .tv_sec = 0, .tv_nsec = 1000000 };
    nanosleep(&t, NULL);
}

/* Fa girare finche `voluto` non e vero o finche non scade la pazienza. */
static bool aspetta(bool (*voluto)(void), uint32_t limite_ms)
{
    for (uint32_t n = 0; n < limite_ms; n += 5) {
        if (voluto()) return true;
        passo(5);
    }
    return voluto();
}

static bool pronto(void)   { return ha_stato() == HA_PRONTO; }
static bool rifiutato(void){ return ha_stato() == HA_TOKEN_RIFIUTATO; }
static bool caduto(void)   { return ha_stato() == HA_CADUTO; }

/* --- preparazione -------------------------------------------------------- */

static void prepara(int porta)
{
    archivio_radice(PROVE_DIR "/prova_dati_ha");
    segreti_avvia();

    FILE *f = fopen("docs/04-config.example.json", "rb");
    if (!f) { fprintf(stderr, "manca la configurazione d'esempio\n"); exit(2); }
    static char buf[ARCHIVIO_MAX];
    const size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);

    /* Il finto server sta su localhost, sulla porta che ci hanno dato. */
    cJSON *c = cJSON_ParseWithLength(buf, n);
    cJSON *ha = cJSON_GetObjectItem(c, "home_assistant");
    cJSON_ReplaceItemInObject(ha, "host", cJSON_CreateString("127.0.0.1"));
    cJSON_ReplaceItemInObject(ha, "port", cJSON_CreateNumber(porta));
    /* E **in chiaro**, qualunque cosa dica la configurazione. Il finto
       server parla TCP e basta; ereditare `tls` da un documento che descrive
       una casa vera vorrebbe dire far dipendere l'esito di questa prova da
       come qualcuno ha configurato il proprio Home Assistant — cioe da una
       cosa che con il protocollo qui provato non c'entra niente.

       Ci e successo: bastato che l'esempio passasse a tls vero perche tutti
       e sei gli scenari fallissero insieme, con un messaggio che parlava di
       entita mai viste e non di cifratura. Chi sovrascrive indirizzo e porta
       per puntare a un finto server deve sovrascrivere anche tutto il resto
       di come ci si arriva. */
    cJSON_ReplaceItemInObject(ha, "tls", cJSON_CreateBool(false));
    /* Riprova corta: la prova non deve aspettare quindici secondi veri. */
    cJSON_ReplaceItemInObject(ha, "retry_s", cJSON_CreateNumber(5));
    char *testo = cJSON_PrintUnformatted(c);
    cJSON_Delete(c);

    archivio_scrivi(CFG_FILE, testo, strlen(testo));
    cJSON_free(testo);

    if (cfg_carica() != CFG_LETTA) {
        fprintf(stderr, "la configurazione di prova non si legge\n");
        exit(2);
    }
    segreti_scrivi(SEG_HA_TOKEN, "finto-token-a-lunga-durata");
}

/* --- gli scenari --------------------------------------------------------- */

static void scenario_normale(void)
{
    prova("l'elenco delle entita si ricava dalla configurazione",
          (ha_avvia(), ent_seguite() > 50));
    /* Un identificatore e `dominio.oggetto`: gli indirizzi IP e gli URL che
       stanno nella stessa configurazione non devono entrarci. */
    prova("e non ci sono finiti dentro indirizzi o URL",
          !ent_seguita_e("192.0.2.10") && !ent_seguita_e("192.0.2.31"));

    prova("si arriva a connesso", aspetta(pronto, 4000));
    prova("senza motivi di errore", ha_motivo()[0] == 0);

    prova("la fotografia iniziale ha riempito le entita",
          ent_vista("light.soggiorno"));
    prova("con lo stato", ent_stato_e("light.soggiorno", "on"));
    prova("e con gli attributi",
          ent_attributo_numero("light.soggiorno", "brightness", 0) > 0);
    if (ent_mai_viste())
        for (int n = 0; n < ent_seguite(); n++)
            if (!ent_vista(ent_seguita(n)))
                printf("    mai vista: %s\n", ent_seguita(n));
    prova("le entita configurate ci sono tutte", ent_mai_viste() == 0);

    /* Un comando, e il riscontro che arriva **dallo stato**, non dalla
       risposta: e la differenza fra "l'ho chiesto" e "e successo". */
    prova("un comando parte",
          ha_chiama("light", "turn_off", "light.soggiorno", NULL));
    for (int n = 0; n < 400; n++) {
        passo(5);
        if (ent_stato_e("light.soggiorno", "off")) break;
    }
    prova("e lo stato dell'entita cambia davvero",
          ent_stato_e("light.soggiorno", "off"));
    prova("il comando risulta inviato e non fallito",
          ha_comandi_inviati() == 1 && ha_comandi_falliti() == 0);

    prova("un comando con dati aggiuntivi parte",
          ha_chiama("climate", "set_temperature", "climate.condizionatore_soggiorno",
                    "\"temperature\": 24"));
    for (int n = 0; n < 400; n++) {
        passo(5);
        if (ent_attributo_numero("climate.condizionatore_soggiorno", "temperature", 0) == 24)
            break;
    }
    prova("e l'attributo cambia",
          ent_attributo_numero("climate.condizionatore_soggiorno", "temperature", 0) == 24);

    prova("dal collegamento arrivano dati di recente",
          ha_eta_ultimo_dato_ms(orologio) < 5000);

    /* Il giro completo come lo fa un dito sull'interruttore: il fornitore
       traduce l'indice della scheda nell'entita, manda il servizio, e lo
       stato torna dall'evento. E la sola prova che il cammino
       configurazione -> entita -> widget funzioni in tutte e due le
       direzioni. */
    dati_ricarica();
    const luce_t *z = dati_luce(1);
    prova("la seconda zona ha la sua entita", z && z->entita && *z->entita);
    prova("ed e accesa come dice Home Assistant", z && z->acceso);
    prova("il comando dal fornitore parte", dati_luce_accendi(1, false));
    for (int n = 0; n < 400; n++) {
        passo(5);
        dati_ricarica();
        if (dati_luce(1) && !dati_luce(1)->acceso) break;
    }
    prova("e la scheda vede la luce spenta", dati_luce(1) && !dati_luce(1)->acceso);

    /* --- l'impulso, che e la cosa piu delicata che questo pannello fa ----
     *
     * Un accesso a impulso accende un relè e lo **rilascia** dopo
     * impulso_ms. Il rilascio non e una rifinitura: un relè che resta
     * chiuso e un cancello che non si richiude, ed e il genere di guasto
     * che si scopre tornando a casa.
     *
     * Si contano i comandi invece di guardare uno stato, perche un impulso
     * per definizione non ha riscontro: quello che si puo pretendere e che
     * ne partano **due**, uno per accendere e uno per rilasciare. */
    int impulso = -1;
    for (int n = 0; n < dati_accessi(); n++) {
        const accesso_t *a = dati_accesso(n);
        if (a && a->tipo == ACC_IMPULSO && a->disponibile) { impulso = n; break; }
    }
    prova("c'e un accesso a impulso in configurazione", impulso >= 0);

    if (impulso >= 0) {
        const uint32_t prima = ha_comandi_inviati();
        prova("l'impulso parte", dati_accesso_aziona(impulso));
        prova("ed e un comando solo, per ora",
              ha_comandi_inviati() == prima + 1);

        /* Il rilascio arriva col tempo, non subito: si fa scorrere
           l'orologio come lo farebbe il ciclo grafico. */
        for (int n = 0; n < 400 && ha_comandi_inviati() < prima + 2; n++) {
            passo(20);
            dati_gira(orologio);
        }
        prova("e dopo l'attesa arriva il rilascio",
              ha_comandi_inviati() == prima + 2);

        /* Due volte di fila non devono incastrarsi: e quello che succede
           quando qualcuno preme due volte perche il cancello e lento. */
        const uint32_t seconda = ha_comandi_inviati();
        prova("un secondo impulso parte lo stesso",
              dati_accesso_aziona(impulso));
        for (int n = 0; n < 400 && ha_comandi_inviati() < seconda + 2; n++) {
            passo(20);
            dati_gira(orologio);
        }
        prova("e anche lui viene rilasciato",
              ha_comandi_inviati() == seconda + 2);
    }

    /* --- il clima -------------------------------------------------------
     *
     * Qui la parte delicata non e mandare il comando: e **quale parola**
     * mandare. Modi, ventilazione e deflettore non sono uno standard —
     * sono quello che quel modello, con quella integrazione, dichiara di
     * accettare in hvac_modes, fan_modes, swing_modes. Il pannello sceglie
     * da quell'elenco invece di inventare, e queste prove pretendono
     * proprio quello: che un modo offerto parta e uno non offerto **no**.
     */
    if (dati_condizionatori() > 0) {
        const condizionatore_t *u = dati_condizionatore(0);
        prova("il condizionatore ha la sua entita",
              u && u->entita && *u->entita);

        const uint32_t prima = ha_comandi_inviati();
        prova("il setpoint parte", dati_condizionatore_imposta(0, 240));
        prova("ed e un comando solo", ha_comandi_inviati() == prima + 1);

        /* Fuori scala non deve essere rifiutato dal pannello ma **limitato**:
           il comando parte lo stesso, col valore massimo consentito. Se
           partisse com'e, a rifiutarlo sarebbe l'impianto, e chi guarda
           vedrebbe un numero che torna indietro da solo. */
        const limiti_t l = dati_limiti_condizionatori();
        prova("un setpoint assurdo viene limitato, non rifiutato",
              dati_condizionatore_imposta(0, (int16_t)(l.max + 500)));

        prova("un modo che l'apparecchio offre parte",
              dati_condizionatore_modo(0, MODO_FREDDO));
        dati_condizionatore_ventilazione(0, VENT_MEDIA);
        dati_condizionatore_deflettore(0, u->deflettore, u->oscillazione);

        /* --- e il collegamento e ancora vivo -------------------------
         *
         * Questa riga e la piu importante del blocco, e mancava. Le prove
         * di sopra chiedono che il comando **parta**; nessuna chiedeva che
         * dopo essere partito ci fosse ancora qualcuno dall'altra parte.
         *
         * Cosi e passata una versione che mandava JSON malformato: i dati
         * finivano dentro "service_data" gia avvolti nelle graffe, e ne
         * usciva un oggetto dentro un oggetto senza nome. ws_manda()
         * riusciva — i byte partivano davvero — e la prova diceva verde,
         * mentre Home Assistant a un messaggio cosi non risponde con un
         * errore: chiude il collegamento.
         *
         * "E partito" e "e stato capito" sono due domande diverse, e per la
         * seconda l'unica prova possibile e che il collegamento regga. */
        for (int n = 0; n < 200; n++) { passo(10); ha_gira(orologio); }
        prova("e dopo i comandi il collegamento e ancora vivo",
              ha_stato() == HA_PRONTO);
    }

    if (dati_zone_clima() > 0) {
        prova("una zona di riscaldamento accetta il setpoint",
              dati_zona_clima_imposta(0, 205));
    }

    /* --- lo standby: lo stato oppure un attributo ----------------------
     *
     * E la sola cosa che il fornitore dello standby aggiunge davvero, e
     * senza una prova sarebbe una promessa: la configurazione dice se il
     * valore sta nello stato dell'entita o dentro un attributo, e i due
     * casi si leggono in due modi diversi.
     *
     * Il finto server serve una `climate` con stato "cool" e attributo
     * `current_temperature` a 30,0: se il pannello leggesse lo stato non
     * otterrebbe 30, otterrebbe una parola. E per questo che questa prova
     * distingue davvero fra le due strade invece di passare per caso.
     *
     * Il sensore di fuori prova la strada opposta: nessun attributo in
     * configurazione, e il valore e lo stato — che qui e "21.4". */
    {
        const standby_t sb = dati_standby();

        prova("la temperatura della stanza viene dall'attributo",
              sb.interna_c_e && sb.interna == 300);
        prova("e non dallo stato, che qui e una parola",
              !ent_stato_e("climate.soggiorno", "30.0"));
        prova("l'etichetta della stanza viene dalla configurazione",
              strcmp(sb.stanza, "in soggiorno") == 0);

        prova("la temperatura di fuori viene dallo stato",
              sb.esterna_c_e && sb.esterna == 214);
        prova("e la condizione dal servizio meteo",
              strcmp(sb.condizione, "sunny") == 0);

        prova("i gradi chiesti vengono dall'entita del clima",
              sb.chiesta_c_e && sb.chiesta == 280);
        prova("e non sta scaldando, perche' nessuno lo ha detto",
              !sb.scalda);

        prova("l'energia c'e", sb.energia_c_e);
        /* Il conteggio viene dal sensore vero e non da qui: il finto
           server da "21.4" allo stato di sensor.panel_openings, e
           ventuno e quello che il pannello deve dire. Un numero inventato
           qui dentro passerebbe lo stesso una prova scritta come
           `aperture >= 0`, ed e per questo che si confronta col valore
           preciso. */
        /* Due, e non "piu di zero": confrontarlo con dati_aperture()
           sarebbe una tautologia, perche' e proprio quello che lo
           standby chiama. Il numero preciso lo lega alla casa finta, e
           se quella cambia questa riga lo dice. */
        prova("le aperture le conta il sensore, non lo standby",
              sb.aperture == 2);
        prova("e la prima ha un nome da scrivere nella pastiglia",
              sb.apertura_nome[0] != 0);
    }

    /* --- l'elenco delle entita per i menu della pagina -----------------
     *
     * Il punto e che l'elenco venga da **Home Assistant** e non da quello
     * che il pannello gia segue. La differenza si vede solo cosi: il finto
     * server ne aggiunge un paio che in configurazione non ci sono, e se
     * l'elenco non le contiene vuol dire che qualcuno lo sta ricavando
     * dalle entita seguite — che e la meta inutile della funzione, perche
     * non aiuta a sceglierne di nuove.
     *
     * E si chiede su domanda, non al collegamento: prima di chiederlo non
     * deve esserci niente, altrimenti sono kilobyte tenuti per un menu che
     * nessuno apre. */
    {
        prova("prima di chiederlo non c'e nessun elenco", ha_elenco() == NULL);

        ha_elenco_chiedi();
        for (int n = 0; n < 400 && !ha_elenco(); n++) { passo(10); }

        const char *e = ha_elenco();
        prova("chiesto, l'elenco arriva", e != NULL);
        prova("ed e gia impacchettato per la pagina",
              e && strncmp(e, "{\"entities\":[", 13) == 0);
        prova("contiene un'entita configurata",
              e && strstr(e, "light.soggiorno") != NULL);
        prova("e anche una che il pannello non segue",
              e && strstr(e, "sensor.umidita_cantina") != NULL);
        prova("chiederlo di nuovo non rifa il giro",
              (ha_elenco_chiedi(), ha_elenco() == e));
    }

    /* --- il timer si regola solo mentre corre --------------------------
     *
     * `timer.change` in Home Assistant vuole un timer attivo: su uno fermo
     * da errore. Il pannello deve fermarsi **prima** di mandarlo, non
     * scoprirlo dalla risposta — altrimenti ogni pressione lascia una riga
     * di errore nel registro di Home Assistant e niente sul vetro.
     *
     * Il finto server tiene i timer a "idle", che e anche lo stato normale
     * di casa: i condizionatori non hanno quasi mai un timer in corso. */
    if (dati_condizionatori() > 0) {
        const uint32_t prima = ha_comandi_inviati();
        const bool fermo = !dati_condizionatore(0)->timer_corre;
        prova("un timer fermo non si regola",
              fermo && !dati_condizionatore_timer_regola(0, 15));
        prova("e non parte nessun comando",
              ha_comandi_inviati() == prima);
    }

    /* --- i piani del riscaldamento -------------------------------------
     *
     * Le valvole delle zone aprono il circuito; l'accensione vera la comanda
     * un interruttore per piano. Sono due cose e il pannello deve tenerle
     * separate: una zona che chiede calore mentre il suo piano e spento
     * **non scalda**, e disegnarla come le altre farebbe credere il
     * contrario.
     *
     * Qui si verifica che il legame zona-piano regga davvero: ogni zona
     * finisce sotto il piano che la configurazione le da, e i conteggi
     * della fascia tornano. */
    if (dati_piani() > 0) {
        prova("i piani della configurazione ci sono", dati_piani() == 2);

        const piano_t *p0 = dati_piano(0);
        prova("il primo piano ha nome e interruttore",
              p0 && p0->nome[0] && p0->entita[0] && p0->disponibile);

        /* Il piano che questo pannello governa: da li discendono l'icona
           dello standby e quella della testata. */
        prova("il pannello sa quale piano governa",
              dati_piano_del_pannello() == 0);

        /* Le zone si contano da sole: se il legame per id non funzionasse,
           tutte finirebbero senza piano e questi due numeri sarebbero zero. */
        int somma = 0;
        for (int n = 0; n < dati_piani(); n++) somma += dati_piano(n)->zone;
        prova("ogni zona sta sotto un piano",
              somma == dati_zone_clima() && somma > 0);

        /* E il conteggio di chi chiede calore e quello vero, non una copia
           del totale. */
        int chiedono = 0;
        for (int n = 0; n < dati_zone_clima(); n++) {
            const zona_clima_t *z = dati_zona_clima(n);
            if (z && z->chiama) chiedono++;
        }
        int per_piano = 0;
        for (int n = 0; n < dati_piani(); n++) per_piano += dati_piano(n)->chiedono;
        prova("e chi chiede calore e contato una volta sola",
              per_piano == chiedono);

        /* Il comando: il servizio si ricava dal dominio dell'interruttore,
           come per gli interruttori. Un `switch.` comandato con un servizio
           di un altro dominio non da errore sul vetro — da una fascia che si
           muove e non succede niente. */
        const uint32_t prima = ha_comandi_inviati();
        const bool era = p0->acceso;
        prova("il piano si comanda", dati_piano_accendi(0, !era));
        prova("ed e partito un comando solo",
              ha_comandi_inviati() == prima + 1);

        for (int n = 0; n < 400; n++) {
            passo(5);
            if (ent_stato_e(p0->entita, era ? "off" : "on")) break;
        }
        prova("e l'interruttore del piano e cambiato davvero",
              ent_stato_e(p0->entita, era ? "off" : "on"));
    }

    /* --- gli interruttori, e chi decide il servizio --------------------
     *
     * Il comando lo ricava il pannello dal **dominio dell'entita**, non
     * dalla configurazione: `switch.` vuole `switch.turn_on`. Chiederlo
     * anche a chi configura vorrebbe dire poterlo sbagliare, e uno
     * `switch.` comandato con `light.turn_on` non da errore sul vetro —
     * da un interruttore che si muove e non succede niente.
     *
     * La prova guarda il **numero di comandi partiti** e lo stato
     * dell'entita dopo: il finto server applica il servizio solo se il
     * dominio e il suo. */
    if (dati_interruttori() > 0) {
        const interruttore_t *i = dati_interruttore(0);
        prova("il primo interruttore ha la sua entita",
              i && i->entita[0] && i->disponibile);

        const uint32_t prima = ha_comandi_inviati();
        const bool era = i->acceso;
        prova("si comanda", dati_interruttore_premi(0, !era));
        prova("ed e partito un comando solo",
              ha_comandi_inviati() == prima + 1);

        /* Il giro completo: il servizio e arrivato all'entita giusta, e lo
           stato e cambiato davvero. Se il dominio fosse stato dedotto male
           il finto server non avrebbe applicato niente. */
        const char *ent = i->entita;
        for (int n = 0; n < 400; n++) {
            passo(5);
            if (ent_stato_e(ent, era ? "off" : "on")) break;
        }
        prova("e lo stato dell'entita e cambiato",
              ent_stato_e(ent, era ? "off" : "on"));

        /* L'assorbimento e facoltativo, e la differenza fra «non ce l'ha» e
           «assorbe zero» non si perde: la prima non si scrive, la seconda
           si scrive «0 W». */
        int con = 0, senza = 0;
        for (int n = 0; n < dati_interruttori(); n++) {
            const interruttore_t *x = dati_interruttore(n);
            if (x && x->potenza_c_e) con++; else senza++;
        }
        prova("qualcuno ha il sensore dell'assorbimento e qualcuno no",
              con > 0 && senza > 0);
    }

    /* --- il conto alla rovescia si calcola, non si legge ---------------
     *
     * Il difetto che ha portato qui: il pannello leggeva un attributo
     * `remaining_s` che Home Assistant non ha, e mostrava "mancano 0h00" su
     * un timer che stava correndo. Un attributo assente non da errore, da
     * il ripiego — e il ripiego era zero.
     *
     * Il finto server tiene `timer.condizionatore_mansarda` a "active" con
     * `finishes_at` fra quarantadue minuti esatti, e `remaining` fermo a
     * "1:30:00" come fa Home Assistant. Se il conto alla rovescia dicesse
     * novanta, verrebbe da li e non dall'istante di fine. */
    {
        int mansarda = -1;
        for (int n = 0; n < dati_condizionatori(); n++)
            if (!strcmp(dati_condizionatore(n)->nome, "Mansarda")) mansarda = n;

        prova("la mansarda e fra i condizionatori", mansarda >= 0);
        if (mansarda >= 0) {
            const condizionatore_t *u = dati_condizionatore(mansarda);
            prova("il pannello vede il timer che corre", u->timer_corre);
            /* Un minuto di tolleranza: fra la risposta del server e questa
               riga passa del tempo vero, e pretendere il secondo esatto
               vorrebbe dire una prova che fallisce quando la macchina e
               carica. */
            prova("e quanto manca viene dall'istante di fine",
                  u->restano_min >= 41 && u->restano_min <= 42);
            prova("non dall'attributo che sta fermo", u->restano_min != 90);
            prova("e sa a che ora si spegne",
                  u->spegne_alle && strlen(u->spegne_alle) == 5
                  && u->spegne_alle[2] == ':');

            /* --- rispetto a cosa si riempie la barra --------------------
             *
             * Il finto server dichiara `duration` di **due ore**; la
             * configurazione dice `durata_predefinita: "1:30"`. Sono diversi
             * apposta: se la barra dividesse per la configurazione — che e
             * cio che il pannello userebbe avviando **lui** il timer —
             * quarantadue minuti darebbero il 46 per cento invece del 35, e
             * la barra racconterebbe di un timer che non e quello che corre.
             *
             * Un timer avviato dall'app con tre ore, diviso per un'ora e
             * mezza, si riempirebbe oltre il fondo. */
            prova("il totale viene dal timer, non dalla configurazione",
                  u->timer_totale_min == 120);
            prova("e la barra si riempie di conseguenza",
                  u->timer_quota_pct >= 34 && u->timer_quota_pct <= 35);
        }
    }

    /* --- leggere un istante di Home Assistant --------------------------
     *
     * Il pezzo che sta sotto al conto alla rovescia, provato da solo perche
     * dipende dall'ora vera e una prova che dipende dall'ora vera non
     * verifica niente. Il momento scelto e un mezzogiorno d'inverno, cosi
     * l'ora legale non c'entra e il numero e verificabile a mano:
     * 2026-01-01T12:00:00Z sono 1767268800 secondi dal 1970. */
    {
        prova("un istante in UTC si legge",
              orologio_da_iso("2026-01-01T12:00:00+00:00") == 1767268800LL);
        prova("«Z» vuol dire la stessa cosa",
              orologio_da_iso("2026-01-01T12:00:00Z") == 1767268800LL);
        prova("i decimi si saltano, non si leggono",
              orologio_da_iso("2026-01-01T12:00:00.123456+00:00")
              == 1767268800LL);
        /* Lo scarto si toglie: mezzogiorno a Roma d'inverno sono le undici
           in UTC, e chi lo sommasse invece di toglierlo sbaglierebbe di due
           ore — un errore che di sei mesi in sei mesi sembra giusto. */
        prova("lo scarto del fuso si toglie",
              orologio_da_iso("2026-01-01T13:00:00+01:00") == 1767268800LL);
        prova("una data che non si legge vale zero",
              orologio_da_iso("ieri sera") == 0
              && orologio_da_iso(NULL) == 0
              && orologio_da_iso("2026-13-99T99:99:99Z") == 0);
    }

    /* --- una casa tranquilla non e un collegamento caduto ---------------
     *
     * L'eta dell'ultimo dato deve misurare se il **collegamento** e vivo,
     * non se in casa e successo qualcosa. Senza un battito, una notte in cui
     * nessuno accende niente assomiglia a una caduta: dopo quindici secondi
     * di silenzio il pannello copriva la schermata con "riconnetto" su un
     * collegamento sano.
     *
     * Qui si sta zitti per venti secondi — piu della prima soglia — e si
     * pretende che il pannello continui a sapere di stare bene. */
    {
        const uint32_t partenza = orologio;
        while (orologio - partenza < 20000) {
            passo(200);
            ha_gira(orologio);
        }
        prova("dopo venti secondi di silenzio si e ancora pronti",
              ha_stato() == HA_PRONTO);
        prova("e l'ultimo dato non e invecchiato",
              ha_eta_ultimo_dato_ms(orologio) < 15000);
    }
}

static void scenario_token_cattivo(void)
{
    ha_avvia();
    prova("un token rifiutato porta a HA_TOKEN_RIFIUTATO",
          aspetta(rifiutato, 4000));
    prova("e lo dice in chiaro", strstr(ha_motivo(), "token") != NULL);
    prova("senza far uscire il token nel motivo",
          strstr(ha_motivo(), "finto-token") == NULL);

    /* Il punto: **non si riprova da soli**. Riprovare ogni quindici secondi
       riempirebbe il registro di Home Assistant e non direbbe mai a nessuno
       cosa c'e che non va. */
    const uint32_t prima = orologio;
    for (int n = 0; n < 200; n++) passo(200);   /* quaranta secondi */
    prova("e non si riprova da soli, nemmeno dopo quaranta secondi",
          ha_stato() == HA_TOKEN_RIFIUTATO && orologio > prima);
    prova("il collegamento resta chiuso", ws_stato() != WS_APERTO);
}

static void scenario_caduta(void)
{
    ha_avvia();
    prova("si arriva a connesso", aspetta(pronto, 4000));
    prova("e le entita si sono viste", ent_mai_viste() == 0);

    /* Il finto server chiude dopo N messaggi: il primo comando lo fa
       cadere a meta. */
    ha_chiama("light", "turn_off", "light.soggiorno", NULL);
    prova("la caduta si nota", aspetta(caduto, 4000));

    /* Quello che si sapeva non si tiene: mostrarlo come se fosse di adesso
       sarebbe peggio che non mostrarlo. */
    prova("i valori si dimenticano",
          !ent_vista("light.soggiorno"));
    prova("ma l'elenco di cosa chiedere resta", ent_seguite() > 50);
    prova("l'eta dell'ultimo dato cresce",
          ha_eta_ultimo_dato_ms(orologio + 20000) > 15000);

    /* Un comando mandato mentre il collegamento non c'e deve fallire
       subito: e il criterio contro l'attesa infinita. */
    const uint32_t falliti_prima = ha_comandi_falliti();
    prova("un comando a collegamento caduto fallisce subito",
          !ha_chiama("light", "turn_on", "light.soggiorno", NULL));
    /* E lo stesso dall'interfaccia: la scheda non e disponibile, quindi il
       fornitore non prova nemmeno. Nessuna attesa, nessun riscontro finto. */
    dati_ricarica();
    prova("e dall'interfaccia nemmeno parte", !dati_luce_accendi(0, true));
    prova("e viene contato come fallito",
          ha_comandi_falliti() == falliti_prima + 1);

    /* E poi si riprova da soli, che e la differenza col token rifiutato. */
    prova("dopo l'attesa si riprova e si torna connessi",
          aspetta(pronto, 20000));
    prova("e le entita si rivedono", ent_mai_viste() == 0);
}

static void scenario_manca(void)
{
    ha_avvia();
    prova("si arriva a connesso", aspetta(pronto, 4000));

    /* Un'entita configurata che Home Assistant non ha **non e un errore di
       configurazione**: puo essere un'integrazione giu. Il pannello la
       mostra "non disponibile" e disattiva i suoi comandi, e tutto il resto
       continua. */
    prova("l'entita che manca non e stata vista",
          !ent_vista("light.soggiorno"));
    prova("e non e comandabile",
          !ent_disponibile("light.soggiorno"));
    prova("ma il suo stato e vuoto, non inventato",
          ent_stato("light.soggiorno")[0] == 0);
    prova("le altre ci sono lo stesso",
          ent_vista("light.sala_da_pranzo"));
    prova("e il conto di quelle mancanti lo dice", ent_mai_viste() == 1);
}

static void scenario_comandi_falliscono(void)
{
    ha_avvia();
    prova("si arriva a connesso", aspetta(pronto, 4000));

    prova("il comando parte",
          ha_chiama("light", "turn_on", "light.soggiorno", NULL));
    for (int n = 0; n < 200; n++) passo(5);
    /* Home Assistant lo ha rifiutato: e diverso da un comando che non e
       nemmeno partito, e va contato lo stesso. */
    prova("ma Home Assistant lo rifiuta e si conta",
          ha_comandi_falliti() >= 1);
    prova("e lo stato dell'entita non e cambiato",
          ent_stato_e("light.soggiorno", "on"));
}

/* --- indisponibile: uno stato che arriva, ma non si puo usare ------------ */

static void scenario_indisponibile(void)
{
    ha_avvia();
    prova("si arriva a connesso", aspetta(pronto, 4000));

    /* Questo non lo produce il finto server: si inietta a mano, perche
       `unavailable` e uno stato che arriva **davvero** da Home Assistant
       quando un'integrazione cade, e va distinto da "mai vista". */
    ha_messaggio(
        "{\"id\":1,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\","
        "\"data\":{\"entity_id\":\"light.soggiorno\","
        "\"new_state\":{\"entity_id\":\"light.soggiorno\","
        "\"state\":\"unavailable\",\"attributes\":{}}}}}", orologio);

    prova("un'entita unavailable risulta vista",
          ent_vista("light.soggiorno"));
    prova("ma non disponibile",
          !ent_disponibile("light.soggiorno"));
    prova("e il suo stato si legge com'e",
          ent_stato_e("light.soggiorno", "unavailable"));
    prova("un numero da uno stato non numerico torna il ripiego",
          ent_numero("light.soggiorno", -1) == -1);
}

/* --- collegato, e piantato ---------------------------------------------
 *
 * Il socket e aperto, i byte arrivano a destinazione, e dall'altra parte
 * non risponde piu nessuno. E il guasto che un Home Assistant vero fa
 * quando si impianta senza morire, ed e diverso da una caduta: una
 * caduta il pannello la vede subito, questa no.
 *
 * Sorveglianza ha due soglie apposta — quindici secondi per la fascia,
 * sessanta per coprire la schermata — e tutte e due leggono l'eta
 * dell'ultimo dato. Per un periodo quell'eta non poteva superare i dieci
 * secondi, perche il battito la rimetteva a zero mandando **il proprio**
 * ping: le due soglie non potevano scattare, e questo caso finiva con un
 * pannello che mostrava valori vecchi di ore, in verde.
 *
 * L'orologio qui e finto, quindi settanta secondi costano meno di un
 * decimo di secondo veri. */
static void scenario_muto(void)
{
    ha_avvia();
    prova("si arriva a connesso", aspetta(pronto, 4000));

    const uint32_t partenza = orologio;
    while (orologio - partenza < 20000) passo(200);

    prova("dopo venti secondi di silenzio l'eta supera la prima soglia",
          ha_eta_ultimo_dato_ms(orologio) >= 15000);
    prova("e il collegamento risulta ancora aperto",
          ha_stato() == HA_PRONTO);

    while (orologio - partenza < 70000) passo(200);

    prova("dopo settanta l'eta supera anche la seconda",
          ha_eta_ultimo_dato_ms(orologio) >= 60000);

    /* Che poi cada e giusto — il ping su un socket morto prima o poi da
       errore — ma non e questo che si sta provando: si sta provando che
       il **numero** cresce, perche e l'unico su cui sorveglianza.c possa
       decidere. */
}

/* --- the heartbeat, after a command -------------------------------------
 *
 * The ping always went with id 4, and Home Assistant wants increasing ids:
 * after the first command — id 10 and up — every heartbeat came back as
 * "id_reuse". The connection looked alive anyway, since an error is an
 * answer too, and no test saw it: the fake server answered everything. It
 * now closes on an id that does not increase, and this checks that the
 * panel stays up for three heartbeats in a row after a command. */
static void scenario_battito(void)
{
    ha_avvia();
    prova("connected", aspetta(pronto, 4000));
    prova("a command goes",
          ha_chiama("light", "turn_off", "light.soggiorno", NULL));

    const uint32_t partenza = orologio;
    while (orologio - partenza < 35000) passo(200);

    prova("three heartbeats later the connection is still up",
          ha_stato() == HA_PRONTO);
    prova("and the last data is recent: the pongs arrive",
          ha_eta_ultimo_dato_ms(orologio) < 15000);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "uso: %s PORTA SCENARIO\n", argv[0]);
        return 2;
    }
    const int porta = atoi(argv[1]);
    const char *scenario = argv[2];

    prepara(porta);
    printf("--- scenario: %s (porta %d) ---\n", scenario, porta);

    if (!strcmp(scenario, "normale"))                  scenario_normale();
    else if (!strcmp(scenario, "token-cattivo"))       scenario_token_cattivo();
    else if (!strcmp(scenario, "caduta"))              scenario_caduta();
    else if (!strcmp(scenario, "manca"))               scenario_manca();
    else if (!strcmp(scenario, "comandi-falliscono"))  scenario_comandi_falliscono();
    else if (!strcmp(scenario, "indisponibile"))       scenario_indisponibile();
    else if (!strcmp(scenario, "muto"))                scenario_muto();
    else if (!strcmp(scenario, "battito"))             scenario_battito();
    else { fprintf(stderr, "scenario sconosciuto: %s\n", scenario); return 2; }

    ha_ferma();
    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutte le prove passano");
    return falliti ? 1 : 0;
}
