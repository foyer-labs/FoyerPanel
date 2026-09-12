/* ------------------------------------------------------------------------
 * Segreti — la politica, non il magazzino.
 *
 * Qui c'e l'elenco chiuso dei nomi e la regola che li tiene al loro posto:
 * si scrivono, si chiede se ci sono, si usano per la durata di una
 * chiamata. Non si leggono.
 * --------------------------------------------------------------------- */
#include "segreti.h"

#include <string.h>

#include "magazzino.h"

/* I nomi con cui arrivano da POST /api/secrets. Corti perche NVS limita le
   chiavi a quindici caratteri, e conviene che le due cose coincidano invece
   di avere una tabella di traduzione da tenere allineata. */
static const char *const NOMI[SEG_QUANTI] = {
    [SEG_WIFI_PASSWORD]        = "wifi_pw",
    [SEG_HA_TOKEN]             = "ha_token",
    /* The five shared networks, numbered as in the list. They used to be
       wifi_osp_pw and wifi_pv1..4_pw — from when the first was «ospiti»
       (guests) and the others «private». See TRASLOCHI below: renaming a
       key in NVS is moving a password, and it is done at boot so that no
       password is lost for a nicer name. */
    [SEG_WIFI_RETE_1_PASSWORD] = "wifi_share1_pw",
    [SEG_WIFI_RETE_2_PASSWORD] = "wifi_share2_pw",
    [SEG_WIFI_RETE_3_PASSWORD] = "wifi_share3_pw",
    [SEG_WIFI_RETE_4_PASSWORD] = "wifi_share4_pw",
    [SEG_WIFI_RETE_5_PASSWORD] = "wifi_share5_pw",
};

/* --- the old names, moved once ------------------------------------------
 *
 * A panel coming from the Italian project, or from an earlier version of
 * this one, has its shared-network passwords under the old keys. At boot
 * each one is copied to its new key and the old key is erased. A new key
 * that already exists wins: it was written by this firmware, so it is the
 * newer of the two.
 *
 * The password passes through a buffer on the stack for the time of a copy
 * and the buffer is wiped: the same rule segreti_usa() follows. */
static const struct { const char *vecchio, *nuovo; } TRASLOCHI[] = {
    { "wifi_osp_pw", "wifi_share1_pw" },
    { "wifi_pv1_pw", "wifi_share2_pw" },
    { "wifi_pv2_pw", "wifi_share3_pw" },
    { "wifi_pv3_pw", "wifi_share4_pw" },
    { "wifi_pv4_pw", "wifi_share5_pw" },
};

static void trasloca(void)
{
    for (unsigned n = 0; n < sizeof TRASLOCHI / sizeof TRASLOCHI[0]; n++) {
        if (!magazzino_esiste(TRASLOCHI[n].vecchio)) continue;
        if (!magazzino_esiste(TRASLOCHI[n].nuovo)) {
            char buf[SEGRETO_MAX + 1];
            if (magazzino_leggi(TRASLOCHI[n].vecchio, buf, sizeof buf)
                    && !magazzino_scrivi(TRASLOCHI[n].nuovo, buf)) {
                /* Not written: the old key stays, and the next boot tries
                   again. Erasing it now would lose the only copy. */
                volatile char *p = buf;
                for (size_t i = 0; i < sizeof buf; i++) p[i] = 0;
                continue;
            }
            volatile char *p = buf;
            for (size_t i = 0; i < sizeof buf; i++) p[i] = 0;
        }
        magazzino_cancella(TRASLOCHI[n].vecchio);
    }
}

const char *segreto_nome(segreto_t s)
{
    return (s >= 0 && s < SEG_QUANTI && NOMI[s]) ? NOMI[s] : "";
}

segreto_t segreto_da_nome(const char *nome)
{
    if (!nome) return SEG_QUANTI;
    for (int n = 0; n < SEG_QUANTI; n++)
        if (NOMI[n] && strcmp(NOMI[n], nome) == 0) return (segreto_t)n;
    return SEG_QUANTI;
}

/* --- i segreti che non hanno piu un uso ---------------------------------
 *
 * Le credenziali delle telecamere sono uscite dall'elenco insieme al video.
 * Due volte, in due forme diverse. Uscite dall'elenco, pero, non escono da NVS: quelle
 * chiavi restano scritte nel flash del pannello, e nessuna riga di questo
 * programma le nominerebbe piu — cioe nessuno le cancellerebbe mai.
 *
 * Una password che sopravvive alla funzione che la usava e' il genere di
 * cosa che si scopre anni dopo leggendo una partizione. Si cancellano
 * all'avvio, per nome, una volta sola: dalla seconda accensione in poi
 * `magazzino_cancella` non trova piu niente e non fa niente.
 *
 * L'elenco e' scritto qui e non ricavato da segreto_nome(): quei nomi non
 * esistono piu nel programma, ed e' esattamente il motivo per cui bisogna
 * ripeterli. Quando anche il piu vecchio dei pannelli sara' passato di qui,
 * questa tabella potra' sparire. */
static const char *const NOMI_SEPOLTI[] = {
    /* La prima tornata: go2rtc e le otto telecamere in MJPEG. */
    "go2rtc_user", "go2rtc_pw",
    "cam1_usr", "cam1_pw", "cam2_usr", "cam2_pw",
    "cam3_usr", "cam3_pw", "cam4_usr", "cam4_pw",
    "cam5_usr", "cam5_pw", "cam6_usr", "cam6_pw",
    "cam7_usr", "cam7_pw", "cam8_usr", "cam8_pw",

    /* La seconda: le quattro coppie della sezione in RTSP, vissuta il
       09/09/2026 dalla mattina al pomeriggio. Sono nomi diversi dai
       precedenti di tre lettere — `cam1_utente` e non `cam1_usr` — e
       vanno cancellati per conto loro.

       Questa riga vale piu della prima: quelle chiavi hanno contenuto una
       password vera, scritta a mano da chi usa il pannello, per una
       telecamera che sta in casa. Non e un residuo di prova. */
    "cam1_utente", "cam1_password", "cam2_utente", "cam2_password",
    "cam3_utente", "cam3_password", "cam4_utente", "cam4_password",
};

bool segreti_avvia(void)
{
    if (!magazzino_avvia()) return false;

    for (unsigned n = 0; n < sizeof NOMI_SEPOLTI / sizeof NOMI_SEPOLTI[0]; n++)
        if (magazzino_esiste(NOMI_SEPOLTI[n]))
            magazzino_cancella(NOMI_SEPOLTI[n]);

    trasloca();
    return true;
}

bool segreti_scrivi(segreto_t s, const char *valore)
{
    if (s < 0 || s >= SEG_QUANTI) return false;

    /* Valore vuoto vuol dire "togli": senza, per cancellare un token
       bisognerebbe scriverne uno finto, che resterebbe li a fingere. */
    if (!valore || !*valore) return magazzino_cancella(NOMI[s]);

    if (strlen(valore) > SEGRETO_MAX) return false;
    return magazzino_scrivi(NOMI[s], valore);
}

bool segreti_impostato(segreto_t s)
{
    if (s < 0 || s >= SEG_QUANTI) return false;
    return magazzino_esiste(NOMI[s]);
}

bool segreti_usa(segreto_t s, void (*uso)(const char *valore, void *dato),
                 void *dato)
{
    if (s < 0 || s >= SEG_QUANTI || !uso) return false;

    /* In pila e non statico: quando la funzione ritorna il buffer non
       esiste piu, e nel frattempo lo si azzera a mano perche la memoria
       liberata non e memoria cancellata. */
    char valore[SEGRETO_MAX + 1];
    if (!magazzino_leggi(NOMI[s], valore, sizeof valore)) return false;

    uso(valore, dato);

    /* memset semplice: il compilatore potrebbe toglierlo perche `valore`
       non si rilegge, quindi si passa da un puntatore volatile. */
    volatile char *p = valore;
    for (size_t n = 0; n < sizeof valore; n++) p[n] = 0;
    return true;
}

bool segreti_azzera(void) { return magazzino_azzera(); }
