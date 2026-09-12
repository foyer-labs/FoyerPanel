/* ------------------------------------------------------------------------
 * Prova della palette — 02-design-tokens.md.
 *
 * Due cose, e la prima e' la piu importante.
 *
 * **Senza configurazione, la tabella deve contenere la palette scelta,
 * byte per byte.** I valori qui sotto sono trascritti a mano da com'era
 * `theme.h` prima che i colori diventassero configurabili: sono una
 * seconda copia, scritta apposta. Confrontare la tabella con i propri
 * predefiniti sarebbe girare in tondo — direbbe soltanto che una variabile
 * e' uguale a se stessa.
 *
 * **E con un accento diverso gli stati composti devono seguirlo.** Non
 * basta che cambino: devono cambiare *bene*, cioe conservare saturazione e
 * luminosita e girare solo la tinta. Se qualcuno un giorno sostituisse la
 * rotazione con una mescolanza — che e' la cosa che viene in mente per
 * prima — questa prova se ne accorgerebbe.
 * --------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archivio.h"
#include "cJSON.h"
#include "config.h"
#include "tema.h"

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

static uint32_t valore(tema_voce_t v) { return TEMA[v]; }

static void uguale(const char *nome, tema_voce_t v, uint32_t atteso)
{
    const uint32_t vero = valore(v);
    if (vero == atteso) { printf("%-24s #%06X  ok\n", nome, vero); return; }
    printf("%-24s #%06X  atteso #%06X   DIVERSO\n", nome, vero, atteso);
    falliti++;
}

/* --- la palette com'era, trascritta a mano ------------------------------ */

static const struct { const char *nome; tema_voce_t v; uint32_t hex; } COM_ERA[] = {
    { "bg",          TEMA_BG,         0x0D1014 },
    { "nero",        TEMA_NERO,       0x000000 },
    { "card",        TEMA_CARD,       0x151A21 },
    { "card2",       TEMA_CARD2,      0x11161C },
    { "line",        TEMA_LINE,       0x1E242D },
    { "txt",         TEMA_TXT,        0xE8ECF2 },
    { "dim",         TEMA_DIM,        0x8A95A6 },
    { "acc",         TEMA_ACC,        0xF0A835 },
    { "ok",          TEMA_OK,         0x4FC39A },
    { "warn",        TEMA_WARN,       0xE2664F },
    { "luce",        TEMA_LUCE,       0xF0A835 },
    { "meteo sole",    TEMA_METEO_SOLE,    0xF0A835 },
    { "meteo nuvola",  TEMA_METEO_NUVOLA,  0x9AA7B5 },
    { "meteo pioggia", TEMA_METEO_PIOGGIA, 0x57B6E0 },
    { "meteo neve",    TEMA_METEO_NEVE,    0xBBD9E8 },
    { "cool",        TEMA_COOL,       0x57B6E0 },
    { "caldo",       TEMA_CALDO,      0xF0A835 },
    { "off",         TEMA_OFF,        0x232B35 },
    { "ink",         TEMA_INK,        0x0D1014 },
    { "sel bg",      TEMA_SEL_BG,     0x221C0E },
    { "sel line",    TEMA_SEL_LINE,   0x4A3C1C },
    { "invio bg",    TEMA_INVIO_BG,   0x2A2313 },
    { "invio txt",   TEMA_INVIO_TXT,  0xF0C987 },
    { "fatto bg",    TEMA_FATTO_BG,   0x16301F },
    { "fatto line",  TEMA_FATTO_LINE, 0x2C5A3D },
    { "fatto txt",   TEMA_FATTO_TXT,  0x7DDBA6 },
    { "err bg",      TEMA_ERR_BG,     0x2E1A17 },
    { "err line",    TEMA_ERR_LINE,   0x5A2F28 },
    { "err txt",     TEMA_ERR_TXT,    0xF09287 },
    { "ricon bg",    TEMA_RICON_BG,   0x251D0E },
    { "avviso ok bg",  TEMA_AVV_OK_BG,  0x13251C },
    { "avviso ok txt", TEMA_AVV_OK_TXT, 0xA9E3C3 },
    { "avviso ko bg",  TEMA_AVV_KO_BG,  0x271815 },
    { "avviso ko txt", TEMA_AVV_KO_TXT, 0xF3B5AC },
    { "calendario 1", TEMA_CAL_1,     0xF0A835 },
    { "calendario 2", TEMA_CAL_2,     0x4FC39A },
    { "calendario 3", TEMA_CAL_3,     0x6F9FE0 },
    { "calendario 4", TEMA_CAL_4,     0xB98BD9 },
};

/* --- la configurazione della prova -------------------------------------- */

static void metti(const char *json)
{
    archivio_scrivi(CFG_FILE, json, strlen(json));
    cfg_carica();
    tema_applica();
}

/* L'esempio del progetto, col blocco `aspetto` dato da fuori. */
static const char *esempio;

static char *con_aspetto(const char *aspetto)
{
    cJSON *c = cJSON_Parse(esempio);
    if (!c) return NULL;

    cJSON_DeleteItemFromObject(c, "appearance");
    if (aspetto) {
        cJSON *a = cJSON_Parse(aspetto);
        if (a) cJSON_AddItemToObject(c, "appearance", a);
    }
    char *fuori = cJSON_PrintUnformatted(c);
    cJSON_Delete(c);
    return fuori;
}

int main(void)
{
    archivio_radice(PROVE_DIR "/prova_dati_tema");

    /* L'esempio del progetto fa da configurazione di partenza. */
    FILE *f = fopen("docs/04-config.example.json", "rb");
    if (!f) { fprintf(stderr, "non trovo l'esempio\n"); return 2; }
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *testo = malloc((size_t)n + 1);
    if (!testo || fread(testo, 1, (size_t)n, f) != (size_t)n) return 2;
    testo[n] = 0;
    fclose(f);
    esempio = testo;

    /* --- 1. senza `aspetto`, la palette e quella scelta ------------------ */

    printf("--- senza aspetto in configurazione ---\n");
    char *senza = con_aspetto(NULL);
    metti(senza);
    free(senza);

    for (unsigned i = 0; i < sizeof COM_ERA / sizeof COM_ERA[0]; i++)
        uguale(COM_ERA[i].nome, COM_ERA[i].v, COM_ERA[i].hex);

    /* --- 2. con un accento diverso, gli stati lo seguono ----------------- */

    printf("\n--- accento blu #3580F0 ---\n");
    char *blu = con_aspetto("{\"accent\":\"#3580F0\"}");
    metti(blu);
    free(blu);

    prova("l'accento e quello scelto", valore(TEMA_ACC) == 0x3580F0);
    /* I valori attesi vengono dal modello, calcolato a parte: l'ambra
       scurissima della selezione diventa un blu scurissimo con la stessa
       saturazione e la stessa luminosita. */
    uguale("sel bg ruotato",   TEMA_SEL_BG,    0x0E1422);
    uguale("sel line ruotato", TEMA_SEL_LINE,  0x1C2B4A);
    uguale("invio txt ruotato", TEMA_INVIO_TXT, 0x87B0F0);

    prova("il verde non si muove: e un'altra famiglia",
          valore(TEMA_FATTO_BG) == 0x16301F);
    prova("ne il rosso", valore(TEMA_ERR_BG) == 0x2E1A17);
    prova("ne l'azzurro del raffrescamento, che vuol dire freddo",
          valore(TEMA_COOL) == 0x57B6E0);
    /* Le due voci che chi usa il pannello ha chiesto di staccare
       dall'accento, dopo aver visto un sole verde e le luci verdi. */
    prova("ne il sole, che deve dire che tempo fa",
          valore(TEMA_METEO_SOLE) == 0xF0A835);
    prova("ne la lampadina, che si sceglie a parte",
          valore(TEMA_LUCE) == 0xF0A835);
    prova("ne l'ambra del riscaldamento, che fa coppia con l'azzurro",
          valore(TEMA_CALDO) == 0xF0A835);

    /* --- 3. i neutri restano in mezzo ------------------------------------ */

    printf("\n--- sfondo e schede piu chiari ---\n");
    char *chiaro = con_aspetto("{\"background\":\"#202830\",\"cards\":\"#2C3642\"}");
    metti(chiaro);
    free(chiaro);

    const uint32_t bg = valore(TEMA_BG), card = valore(TEMA_CARD);
    const uint32_t c2 = valore(TEMA_CARD2);
    bool in_mezzo = true;
    for (int s = 16; s >= 0; s -= 8) {
        const int a = (bg >> s) & 0xff, b = (card >> s) & 0xff;
        const int x = (c2 >> s) & 0xff;
        if (x < (a < b ? a : b) || x > (a > b ? a : b)) in_mezzo = false;
    }
    printf("sfondo #%06X  card2 #%06X  schede #%06X\n", bg, c2, card);
    prova("card2 resta fra lo sfondo e le schede", in_mezzo);

    /* --- 4. il testo sopra l'accento segue l'accento --------------------- */

    printf("\n--- accento scuro ---\n");
    char *scuro = con_aspetto("{\"accent\":\"#243A6B\"}");
    metti(scuro);
    free(scuro);
    /* Con l'ambra il testo sopra l'accento era lo sfondo, cioe scuro. Con
       un accento scuro deve diventare chiaro, se no e nero su nero — ed e
       l'unico modo per cui una scelta legittima renderebbe illeggibile un
       pulsante. */
    prova("sopra un accento scuro il testo diventa chiaro",
          valore(TEMA_INK) == valore(TEMA_TXT));

    /* --- e il caso di mezzo, che e' quello che ha corretto la regola ----
     *
     * #5D875B e' il primo accento scelto davvero su questo pannello. Con
     * una soglia sulla luminanza finiva sotto, quindi testo chiaro: 3,5:1.
     * Ma lo sfondo, su quel verde, da 4,6:1 — cioe la soglia sceglieva la
     * peggiore delle due. Adesso si confrontano, e questa riga tiene ferma
     * la differenza: e' un caso di mezzo, ed e' li che una soglia sbaglia. */
    printf("\n--- accento di mezzo tono ---\n");
    char *mezzo = con_aspetto("{\"accent\":\"#5D875B\"}");
    metti(mezzo);
    free(mezzo);
    prova("sopra un verde di mezzo tono vince lo sfondo, che stacca di piu",
          valore(TEMA_INK) == valore(TEMA_BG));

    free(testo);
    printf("\n%s\n", falliti ? "PROVE FALLITE" : "tutto a posto");
    return falliti ? 1 : 0;
}
