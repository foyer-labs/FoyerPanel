/* ------------------------------------------------------------------------
 * Finto fornitore — accessi. Solo Fase 1.
 *
 * I quattro accessi di 04-config.example.json, con l'asimmetria che conta:
 * la porta del garage ha un sensore di stato vero, gli altri due accessi no,
 * e le luci del giardino non sono un impulso ma un interruttore. Quell'asimmetria e
 * voluta e deve vedersi.
 *
 * Questo file teneva anche le telecamere, e si chiamava dati_finti_video.c:
 * stavano insieme perche un accesso mostrava la propria telecamera mentre
 * si apriva. Uscite le telecamere, resta la meta che comanda cancelli.
 * --------------------------------------------------------------------- */
#include <stdio.h>

#include "dati_finti.h"

#include <stddef.h>
#include <string.h>

#include "config.h"
#include "entita.h"
#include "ha.h"

/* Campi per nome e non per posizione: aggiungere un campo alla struttura
   non deve far scivolare in silenzio tutta una colonna di valori. */
static const accesso_t VALORI_ACCESSI[] = {
    { .id = "pedonale", .nome = "Pedestrian gate", .tipo = ACC_IMPULSO,
      .ultimo_uso = "17:58", .disponibile = true },
    { .id = "cancello", .nome = "Driveway gate", .tipo = ACC_IMPULSO,
      .conferma = true, .disponibile = true },
    /* L'unico con un sensore vero: puo dire "aperta" o "chiusa" davvero. */
    { .id = "garage", .nome = "Garage door", .tipo = ACC_IMPULSO,
      .conferma = true, .ha_sensore = true, .stato_noto = true,
      .aperto = false, .ultimo_uso = "09:12", .disponibile = true },
    /* Non un impulso ma un interruttore: il pannello conosce lo stato. */
    { .id = "giardino", .nome = "Garden lights", .tipo = ACC_INTERRUTTORE,
      .disponibile = true },
};
#define N_VALORI_ACCESSI ((int)(sizeof VALORI_ACCESSI / sizeof VALORI_ACCESSI[0]))
#define ACCESSI_MAX 6

static const char *const EVENTI_ORA[] = { "17:58", "17:58", "16:41" };
static const char *const EVENTI_TESTO[] = {
    "motion on the driveway · automatic passage",
    "pedestrian gate opened from the doorbell",
    "garage door opened from the panel",
};
#define N_EVENTI ((int)(sizeof EVENTI_ORA / sizeof EVENTI_ORA[0]))

static accesso_t accessi[ACCESSI_MAX];
static int       n_accessi;
static caso_t    caso;
static bool      pronto;

void finto_accessi_applica(caso_t c)
{
    caso = c;
    pronto = true;

    n_accessi = cfg_quanti("access");
    if (n_accessi > ACCESSI_MAX) n_accessi = ACCESSI_MAX;

    for (int n = 0; n < n_accessi; n++) {
        accessi[n] = VALORI_ACCESSI[n % N_VALORI_ACCESSI];
        accessi[n].indice = n;
        accessi[n].id = cfg_testo_in("access", n, "id", "");
        accessi[n].nome = cfg_testo_in("access", n, "name", "accesso");
        accessi[n].conferma = cfg_vero_in("access", n, "confirm", false);
        /* Il tipo e il sensore di stato vengono dalla configurazione, ed e
           da li che nasce l'asimmetria fra gli accessi: uno solo sa dire
           come sta, e uno solo e un interruttore invece di un impulso. */
        const char *tipo = cfg_testo_in("access", n, "type", "pulse");
        accessi[n].tipo = strcmp(tipo, "switch") == 0 ? ACC_INTERRUTTORE
                                                            : ACC_IMPULSO;
        accessi[n].ha_sensore = cfg_ha_in("access", n, "state_sensor");

        const char *comando = cfg_testo_in("access", n, "entity", "");
        const char *sensore = cfg_testo_in("access", n, "state_sensor", "");

        /* Un accesso a impulso **non sa** se il cancello si e aperto: il
           pannello manda un impulso a uno switch e finisce li. Quello che
           si puo sapere lo dice il sensore di stato, e solo chi ce l'ha.
           Per gli altri il pannello non mostra uno stato: mostra che non
           c'e riscontro, ed e la differenza fra un pannello onesto e uno
           che indovina. */
        if (ent_vista(comando))
            accessi[n].disponibile = ent_disponibile(comando);
        else if (dati_dal_vero())
            accessi[n].disponibile = false;

        if (accessi[n].ha_sensore && ent_vista(sensore)) {
            /* I binary_sensor con device_class "opening" dicono `on` per
               aperto: al contrario di come suonerebbe. */
            accessi[n].aperto = ent_stato_e(sensore, "on");
            accessi[n].stato_noto = ent_disponibile(sensore);
        } else if (accessi[n].ha_sensore && dati_dal_vero()) {
            accessi[n].stato_noto = false;
        }

        /* Un interruttore, invece, il proprio stato ce l'ha: e acceso o
           spento, e si vede. */
        if (accessi[n].tipo == ACC_INTERRUTTORE && ent_vista(comando))
            accessi[n].acceso = ent_stato_e(comando, "on");
        if (caso == CASO_NOMI_LUNGHI) accessi[n].nome = FINTO_NOME_LUNGO;
        if (caso == CASO_NON_DISPONIBILE) accessi[n].disponibile = (n % 2) == 0;
    }
}

/* L'ultimo istante noto, per programmare i rilasci senza chiedere
   l'ora a nessuno: la porta dati_gira(). */
static uint32_t ultimo_adesso_ms;

static void assicura(void)
{
    if (!pronto) finto_accessi_applica(CASO_NORMALE);
}

/* --- azionare un accesso ------------------------------------------------
 *
 * 03-config-contratto.md §5: un impulso "chiama switch.turn_on e rilascia
 * dopo impulso_ms". Il rilascio e parte del comando, non un ripensamento:
 * un relè che resta chiuso e un cancello che non si richiude.
 *
 * Il dominio si ricava dall'entita invece di scriverlo fisso. Il contratto
 * parla di switch perche e quello che c'e in questa casa, ma un `button` si
 * preme e non si accende, e uno `script` non si rilascia: dare a tutti
 * switch.turn_on funzionerebbe per caso finche qualcuno non cambia il
 * proprio impianto.
 */
#define RILASCI_MAX ACCESSI_MAX

static struct {
    char     entita[96];
    uint32_t quando_ms;
} rilasci[RILASCI_MAX];

static void programma_rilascio(const char *entita, uint32_t fra_ms,
                               uint32_t adesso_ms)
{
    for (int n = 0; n < RILASCI_MAX; n++) {
        if (rilasci[n].entita[0]) continue;
        snprintf(rilasci[n].entita, sizeof rilasci[n].entita, "%s", entita);
        rilasci[n].quando_ms = adesso_ms + fra_ms;
        return;
    }
    /* Nessuno slot libero vuol dire piu impulsi in volo che accessi
       configurati, cioe qualcosa che non dovrebbe succedere. Si lascia
       perdere il rilascio invece di sovrascriverne un altro: perdere un
       rilascio lascia un relè chiuso, sovrascriverlo ne lascia chiusi due. */
}

void dati_gira(uint32_t adesso_ms)
{
    ultimo_adesso_ms = adesso_ms;

    for (int n = 0; n < RILASCI_MAX; n++) {
        if (!rilasci[n].entita[0]) continue;
        /* La sottrazione fra tempi senza segno regge il giro dell'orologio,
           che su uint32 arriva dopo quarantanove giorni — cioe capitera. */
        if ((int32_t)(adesso_ms - rilasci[n].quando_ms) < 0) continue;

        const char *e = rilasci[n].entita;
        const char *punto = strchr(e, '.');
        char dominio[24] = "switch";
        if (punto && (size_t)(punto - e) < sizeof dominio) {
            memcpy(dominio, e, (size_t)(punto - e));
            dominio[punto - e] = 0;
        }
        ha_chiama(dominio, "turn_off", e, NULL);
        rilasci[n].entita[0] = 0;
    }
}

bool dati_accesso_aziona(int n)
{
    const accesso_t *a = dati_accesso(n);
    if (!a || !a->disponibile) return false;

    const char *e = cfg_testo_in("access", n, "entity", "");
    if (!e || !*e) return false;

    char dominio[24] = "switch";
    const char *punto = strchr(e, '.');
    if (punto && (size_t)(punto - e) < sizeof dominio) {
        memcpy(dominio, e, (size_t)(punto - e));
        dominio[punto - e] = 0;
    }

    /* Senza Home Assistant il comando non va da nessuna parte, e fingere che
       sia andato sarebbe la bugia piu facile da scrivere: si cambia lo stato
       inventato, cosi il simulatore resta usabile, e si dice che non e
       partito. */
    if (!dati_dal_vero()) {
        if (a->tipo == ACC_INTERRUTTORE) accessi[n].acceso = !a->acceso;
        return false;
    }

    if (a->tipo == ACC_INTERRUTTORE)
        return ha_chiama(dominio, a->acceso ? "turn_off" : "turn_on", e, NULL);

    /* Un pulsante si preme; uno script si lancia. Nessuno dei due si
       rilascia, perche non restano premuti. */
    if (strcmp(dominio, "button") == 0)
        return ha_chiama(dominio, "press", e, NULL);
    if (strcmp(dominio, "script") == 0 || strcmp(dominio, "scene") == 0)
        return ha_chiama(dominio, "turn_on", e, NULL);
    if (strcmp(dominio, "cover") == 0)
        return ha_chiama(dominio, "open_cover", e, NULL);

    if (!ha_chiama(dominio, "turn_on", e, NULL)) return false;

    const int32_t ms = cfg_intero_in("access", n, "pulse_ms", 800);
    programma_rilascio(e, (uint32_t)(ms > 0 ? ms : 800), ultimo_adesso_ms);
    return true;
}

int dati_accessi(void) { assicura(); return n_accessi; }

const accesso_t *dati_accesso(int n)
{
    assicura();
    return (n >= 0 && n < n_accessi) ? &accessi[n] : NULL;
}

int dati_eventi(void) { assicura(); return N_EVENTI; }

const char *dati_evento_ora(int n)
{
    return (n >= 0 && n < N_EVENTI) ? EVENTI_ORA[n] : "";
}

const char *dati_evento_testo(int n)
{
    return (n >= 0 && n < N_EVENTI) ? EVENTI_TESTO[n] : "";
}
