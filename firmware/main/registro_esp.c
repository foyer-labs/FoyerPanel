/* ------------------------------------------------------------------------
 * Riempire il registro, sul pannello.
 *
 * --- perche si aggancia l'uscita di ESP-IDF -----------------------------
 *
 * L'alternativa sarebbe chiamare registro_aggiungi() a mano nei posti che
 * contano. Sarebbe una seconda voce accanto a ogni ESP_LOG gia scritto, e
 * col tempo le due si allontanerebbero: qualcuno aggiunge un errore e
 * dimentica il registro, e la schermata di diagnostica smette di raccontare
 * proprio l'errore per cui la si e aperta.
 *
 * Ma soprattutto: le righe piu utili non sono le nostre. "esp-aes: Failed
 * to allocate memory", "Task watchdog got triggered", "certificato non
 * verificato" vengono da ESP-IDF e da mbedTLS, e sono esattamente quelle che
 * si va a cercare. Agganciando l'uscita si prendono tutte, comprese quelle
 * di codice che non abbiamo scritto e che nessuno pensera mai a strumentare.
 *
 * --- il formato ---------------------------------------------------------
 *
 * ESP-IDF stampa "I (1234) tag: messaggio". Livello, tempo e sorgente si
 * leggono da li senza chiedere niente a nessuno. Quello che non ha quella
 * forma si conserva com'e, sotto la sorgente "sistema": e meglio una riga
 * senza etichetta che una riga persa.
 * --------------------------------------------------------------------- */
#include "porti.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "registro.h"

static vprintf_like_t precedente;

/* La sorgente di una riga arrivata a meta, in attesa del suo testo. */
static char      sospesa[16];
static livello_t sospeso_livello = LOG_INFO;

static livello_t livello_da(char c)
{
    switch (c) {
    case 'E': return LOG_ERRORE;
    case 'W': return LOG_AVVISO;
    case 'D':
    case 'V': return LOG_DEBUG;
    default:  return LOG_INFO;
    }
}

static int cattura(const char *formato, va_list ap)
{
    /* Si compone una volta sola e si usa due volte: una per il registro,
       una per la seriale. Comporlo due volte con lo stesso va_list non si
       puo — un va_list si consuma — e copiarlo per rifare il lavoro
       costerebbe il doppio a ogni riga di log. */
    static char riga[224];
    va_list copia;
    va_copy(copia, ap);
    vsnprintf(riga, sizeof riga, formato, copia);
    va_end(copia);

    /* I colori ANSI che ESP-IDF antepone renderebbero illeggibile la riga
       sullo schermo: si salta la sequenza iniziale se c'e. */
    char *p = riga;
    if (p[0] == 0x1B) {
        char *m = strchr(p, 'm');
        if (m) p = m + 1;
    }

    /* "I (1234) tag: messaggio" */
    if ((p[0] == 'I' || p[0] == 'W' || p[0] == 'E' || p[0] == 'D' ||
         p[0] == 'V') && p[1] == ' ' && p[2] == '(') {
        char *chiusa = strchr(p, ')');
        char *duepunti = chiusa ? strchr(chiusa, ':') : NULL;
        if (duepunti && duepunti > chiusa + 2) {
            char sorgente[16];
            const size_t len = (size_t)(duepunti - chiusa - 2);
            snprintf(sorgente, sizeof sorgente, "%.*s",
                     (int)(len < sizeof sorgente - 1 ? len : sizeof sorgente - 1),
                     chiusa + 2);

            char *testo = duepunti + 1;
            while (*testo == ' ') testo++;
            /* Via l'a capo finale: nel registro le righe sono gia righe. */
            char *fine = testo + strlen(testo);
            while (fine > testo && (fine[-1] == '\n' || fine[-1] == '\r')) *--fine = 0;

            /* --- una riga puo arrivare in due pezzi ------------------
             *
             * La libreria Wi-Fi scrive il prefisso e il messaggio con due
             * chiamate distinte: prima "I (1234) wifi:" e poi il testo, da
             * solo. Trattandoli come due righe se ne ottengono una vuota
             * con la sorgente e una senza sorgente col contenuto — che e
             * esattamente quello che si vedeva nel registro del pannello.
             *
             * Se il testo e vuoto, quindi, la riga non e finita: si tiene
             * da parte la sorgente e la si appiccica al pezzo successivo. */
            if (!*testo) {
                snprintf(sospesa, sizeof sospesa, "%s", sorgente);
                sospeso_livello = livello_da(p[0]);
                goto avanti;
            }
            sospesa[0] = 0;
            registro_aggiungi(livello_da(p[0]), sorgente, testo);
            goto avanti;
        }
    }

    /* Senza prefisso: o e il seguito di una riga spezzata, o e una riga
       che non ha la forma attesa. Nel primo caso si ricongiunge con la
       sorgente tenuta da parte; nel secondo si conserva com'e, che e
       meglio di perderla. */
    {
        char *fine = p + strlen(p);
        while (fine > p && (fine[-1] == '\n' || fine[-1] == '\r')) *--fine = 0;
        if (*p) {
            registro_aggiungi(sospesa[0] ? sospeso_livello : LOG_INFO,
                              sospesa[0] ? sospesa : "sistema", p);
            sospesa[0] = 0;
        }
    }

avanti:
    /* E poi sulla seriale, come sempre: agganciarsi non vuol dire rubare. */
    return precedente ? precedente(formato, ap) : vprintf(formato, ap);
}

void registro_aggancia(void)
{
    if (!precedente) precedente = esp_log_set_vprintf(cattura);
}
