/* ------------------------------------------------------------------------
 * Finto fornitore di dati — Fase 1 e 2.
 *
 * La divisione che conta: **la struttura viene dalla configurazione, lo
 * stato da qui**. Quali zone esistono, come si chiamano, in che ordine
 * stanno, quali sono dimmerabili lo dice `config.json`, ed e gia il
 * comportamento definitivo. Se una luce e accesa e a che percentuale lo
 * inventa questo file, finche in Fase 3 non arriva Home Assistant a dirlo
 * davvero.
 *
 * Tenerle separate fin da subito serve a una cosa: quando il client
 * WebSocket arrivera, questo file sparisce e non si porta dietro nient'altro.
 *
 * Nell'impianto vero le scene non ci sono (`"scene": []`), e infatti la
 * fascia non compare — ma perche lo dice la configurazione, non perche sia
 * scritto qui dentro.
 * --------------------------------------------------------------------- */
#include "dati_finti.h"

#include <stddef.h>
#include <string.h>

#include "config.h"
#include "entita.h"
#include "ha.h"
#include "i18n.h"

/* Forty characters with accents and apostrophes, for the case in
   11-collaudo.md §2: it must be cut with an ellipsis without the card
   changing size. */
const char *const FINTO_NOME_LUNGO = "Entrée corridor's far north-east lamps";

#define LUCI_MAX  32
#define SCENE_MAX 5

static luce_t      luci[LUCI_MAX];
static int         n_luci;
static const char *scene[SCENE_MAX];
static int         n_scene;
static caso_t      caso;
static bool        pronto;

static const char *const NOMI[CASO_QUANTI] = {
    [CASO_NORMALE]            = "normale",
    [CASO_ZERO_LUCI]          = "zero-luci",
    [CASO_UNA_LUCE]           = "una-luce",
    [CASO_TUTTE_SPENTE]       = "tutte-spente",
    [CASO_ZERO_SCENE]         = "zero-scene",
    [CASO_NOMI_LUNGHI]        = "nomi-lunghi",
    [CASO_NON_DISPONIBILE]    = "non-disponibile",
    [CASO_TEMP_IGNOTA]        = "temp-ignota",
    [CASO_TIMER_SENZA_AUTO]   = "timer-senza-automazione",
    [CASO_SETPOINT_AL_LIMITE] = "setpoint-al-limite",
    [CASO_NOTTE]              = "notte",
    [CASO_BATTERIA_FERMA]     = "batteria-ferma",
    [CASO_CORRENTE_ZERO]      = "corrente-zero",
    [CASO_AGENDA]             = "agenda",
    [CASO_APERTURA]           = "apertura",
    [CASO_TITOLI_LUNGHI]      = "titoli-lunghi",
    [CASO_SSID_EMOJI]         = "ssid-emoji",
};

/* Vero appena c'e un Home Assistant configurato. Da quel momento lo stato
   arriva da li o non arriva: non si inventa. */
bool dati_dal_vero(void) { return ha_stato() != HA_SPENTO; }

/* Uno stato verosimile e sempre uguale per la stessa zona: le catture non
   devono cambiare a ogni esecuzione, altrimenti confrontarle coi mockup
   diventa impossibile. Serve solo quando Home Assistant non c'e. */
static bool accesa_di_norma(int n)
{
    return n == 0 || n == 1 || n == 2 || n == 4;
}

void dati_caso(caso_t c)
{
    caso = (c >= 0 && c < CASO_QUANTI) ? c : CASO_NORMALE;
    pronto = true;

    /* --- struttura: dalla configurazione ------------------------------- */
    n_luci = cfg_quanti("lights/zones");
    if (n_luci > LUCI_MAX) n_luci = LUCI_MAX;

    for (int n = 0; n < n_luci; n++) {
        luce_t *z = &luci[n];
        z->nome = cfg_testo_in("lights/zones", n, "name", tr(TX_LIGHTS_UNNAMED));
        z->entita = cfg_testo_in("lights/zones", n, "entity", "");
        z->dimmerabile = cfg_vero_in("lights/zones", n, "dimmable", false);

        /* Un interruttore non si regola, qualunque cosa dica la
           configurazione: `switch.` non ha `brightness` e non risponde a
           una richiesta di luminosita. Meglio togliere il cursore che
           lasciarlo li a non fare niente — e il terzo comando muto di
           questo progetto ha insegnato quanto costa. */
        if (z->entita && strncmp(z->entita, "light.", 6) != 0)
            z->dimmerabile = false;

        /* --- stato: da Home Assistant, o inventato se non c'e --------- */
        if (ent_vista(z->entita)) {
            z->acceso = ent_stato_e(z->entita, "on");
            z->disponibile = ent_disponibile(z->entita);
            /* Home Assistant da la luminosita in 0..255; il pannello la
               mostra in percento perche e come la si dice a voce. */
            const double b = ent_attributo_numero(z->entita, "brightness", -1);
            z->percento = b >= 0 ? (uint8_t)((b * 100 + 127) / 255)
                                 : (z->acceso ? 100 : 0);
        } else if (dati_dal_vero()) {
            /* Configurata ma mai vista: non disponibile, e nessun valore.
               Non e un errore di configurazione — puo essere
               un'integrazione giu — e il resto della schermata continua. */
            z->acceso = false;
            z->percento = 0;
            z->disponibile = false;
        } else {
            z->acceso = accesa_di_norma(n);
            z->percento = z->dimmerabile ? 65 : (z->acceso ? 100 : 0);
            z->disponibile = true;
        }

        switch (caso) {
        case CASO_TUTTE_SPENTE:
            z->acceso = false;
            z->percento = 0;
            break;
        case CASO_NOMI_LUNGHI:
            z->nome = FINTO_NOME_LUNGO;
            break;
        case CASO_NON_DISPONIBILE:
            /* Una si, una no: la sezione deve restare usabile mentre una
               parte delle entita non risponde. */
            z->disponibile = (n % 2) == 0;
            break;
        default:
            break;
        }
    }

    n_scene = cfg_quanti("lights/scenes");
    if (n_scene > SCENE_MAX) n_scene = SCENE_MAX;
    for (int n = 0; n < n_scene; n++)
        scene[n] = cfg_testo_in("lights/scenes", n, "name", "scena");

    finto_clima_applica(caso);
    finto_energia_applica(caso);
    finto_accessi_applica(caso);
    finto_agenda_applica(caso);
    finto_riassunto_applica(caso);
}

void dati_ricarica(void) { dati_caso(caso); }

caso_t dati_caso_corrente(void) { return caso; }

const char *dati_caso_nome(caso_t c)
{
    return (c >= 0 && c < CASO_QUANTI && NOMI[c]) ? NOMI[c] : "normale";
}

caso_t dati_caso_da_nome(const char *nome)
{
    if (!nome) return CASO_NORMALE;
    for (int c = 0; c < CASO_QUANTI; c++)
        if (NOMI[c] && strcmp(NOMI[c], nome) == 0) return (caso_t)c;
    return CASO_NORMALE;
}

/* Il caso puo essere stato scelto dalla riga di comando prima del primo
   accesso ai dati: qui si riempie solo se non l'ha gia fatto nessuno. */
static void assicura(void)
{
    if (!pronto) dati_caso(CASO_NORMALE);
}

/* --- luci --------------------------------------------------------------- */

int dati_luci(void)
{
    assicura();
    switch (caso) {
    case CASO_ZERO_LUCI: return 0;
    case CASO_UNA_LUCE:  return n_luci ? 1 : 0;
    default:             return n_luci;
    }
}

const luce_t *dati_luce(int n)
{
    assicura();
    return (n >= 0 && n < dati_luci()) ? &luci[n] : NULL;
}

int dati_luci_accese(void)
{
    int quante = 0;
    for (int n = 0; n < dati_luci(); n++)
        if (luci[n].acceso) quante++;
    return quante;
}

/* --- scene -------------------------------------------------------------- */

int dati_scene(void)
{
    assicura();
    return caso == CASO_ZERO_SCENE ? 0 : n_scene;
}

const char *dati_scena(int n)
{
    return (n >= 0 && n < dati_scene()) ? scene[n] : NULL;
}

/* --- comandi ------------------------------------------------------------ */

bool dati_luce_accendi(int n, bool accesa)
{
    const luce_t *z = dati_luce(n);
    if (!z || !z->disponibile || !z->entita || !*z->entita) return false;

    /* Senza Home Assistant il comando non va da nessuna parte, e fingere
       che sia andato sarebbe la bugia piu facile da scrivere: si cambia lo
       stato inventato, cosi il simulatore resta usabile, e si dice che il
       comando non e partito. */
    if (!dati_dal_vero()) {
        luci[n].acceso = accesa;
        luci[n].percento = luci[n].dimmerabile ? (accesa ? 65 : 0)
                                               : (accesa ? 100 : 0);
        return false;
    }

    /* --- il dominio si legge dall'entita, non si da per scontato --------
     *
     * Qui c'era "light" scritto a mano, e per quasi tutte le luci va bene.
     * Ma in una casa vera qualche luce finisce su un rele, e Home Assistant
     * la espone come `switch.` — succede coi moduli da incasso, con certe
     * integrazioni Zigbee, e ogni volta che il produttore non ha dichiarato
     * il dispositivo per quello che e.
     *
     * Il servizio da chiamare pero e lo stesso — turn_on e turn_off
     * esistono per tutti e due — e cambia solo il dominio, che sta scritto
     * nell'identificativo prima del punto. Leggerlo da li vuol dire che non
     * serve nessuna configurazione in piu: si scrive `switch.tettoia` dove
     * si scriveva `light.tettoia` e funziona.
     *
     * Se il punto non c'e, l'identificativo non e valido e "light" e il
     * ripiego meno sorprendente. */
    char dominio[24] = "light";
    const char *punto = strchr(z->entita, '.');
    if (punto && punto > z->entita) {
        const size_t l = (size_t)(punto - z->entita);
        if (l < sizeof dominio) { memcpy(dominio, z->entita, l); dominio[l] = 0; }
    }

    return ha_chiama(dominio, accesa ? "turn_on" : "turn_off", z->entita, NULL);
}

int dati_scena_attiva(void) { return dati_scene() ? 1 : -1; }
