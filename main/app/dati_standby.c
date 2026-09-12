/* ------------------------------------------------------------------------
 * La schermata di standby — 01-specifica-ui.md §4.7.
 *
 * Mette insieme quello che le altre sezioni sanno gia: la temperatura di
 * una stanza, quella di fuori, il clima, l'energia, le aperture. Non ha
 * dati suoi, e non deve averne — un secondo posto dove dire quali sensori
 * sono avrebbe potuto dire due cose diverse sullo stesso vetro.
 *
 * L'unica cosa che aggiunge davvero e il modo di leggere un valore: **lo
 * stato oppure un attributo**, a scelta. Non e un vezzo di
 * configurazione. Un sensore Zigbee tiene la temperatura nello stato; un
 * termostato la tiene nell'attributo `current_temperature`; una sonda
 * dentro un dispositivo multiplo la tiene in un attributo col nome che le
 * ha dato chi ha scritto l'integrazione. Sono tutti sensori di
 * temperatura, e nessuno dei tre si legge come gli altri due.
 *
 * Indovinare — provare lo stato e, se non e un numero, provare
 * l'attributo — funzionerebbe quasi sempre, e quel "quasi" e il problema:
 * il giorno che uno stato dice "22.5" per motivi suoi si mostrerebbe il
 * numero sbagliato senza che niente protesti.
 * --------------------------------------------------------------------- */
#include "dati.h"

#include <stddef.h>
#include <string.h>

#include "config.h"
#include "dati_finti.h"
#include "entita.h"
#include "ha.h"
#include "i18n.h"

/* Un valore fuori scala per qualunque grandezza di casa: gradi, watt,
   percentuali. Serve a distinguere "non c'e" da "vale zero", che sono due
   cose diverse e si mostrano in due modi diversi. */
#define ASSENTE (-999999.0)

/* Legge il valore di `base`, che in configurazione e un oggetto con
   `entita` e — se serve — `attributo`.

   Torna ASSENTE se l'entita non e configurata, se Home Assistant non l'ha
   mai mandata, se e indisponibile, o se il valore non e un numero. Chi
   chiama non deve distinguere fra questi casi: sul vetro sono la stessa
   cosa, cioe niente da mostrare. */
static double leggi(const char *base_entita, const char *base_attributo)
{
    const char *e = cfg_testo(base_entita, "");
    if (!e || !*e || !ent_vista(e) || !ent_disponibile(e)) return ASSENTE;

    const char *a = cfg_testo(base_attributo, "");
    if (a && *a) return ent_attributo_numero(e, a, ASSENTE);
    return ent_numero(e, ASSENTE);
}

/* Da gradi a decimi, arrotondando dalla parte giusta anche sotto zero:
   -0,25 deve diventare -0,3 e non -0,2. */
static int16_t decimi(double gradi)
{
    return (int16_t)(gradi * 10 + (gradi < 0 ? -0.5 : 0.5));
}

/* --- quando Home Assistant non c'e -------------------------------------
 *
 * Il simulatore senza collegamento mostra valori inventati, come fanno gia
 * le altre sezioni: e l'unico modo di guardare una schermata prima di
 * appenderla al muro, e la convenzione di tutto questo progetto.
 *
 * Sul pannello questo ramo non si prende mai: li Home Assistant e
 * configurato, `dati_dal_vero()` e vero, e quello che non arriva **non si
 * vede** invece di essere inventato. La differenza fra le due cose e tutta
 * qui, in questa riga: `if (!dati_dal_vero())`.
 *
 * I numeri sono quelli del mockup approvato, cosi le catture del
 * simulatore mostrano la schermata che si e scelta. */
static standby_t inventata(standby_t s)
{
    s.interna_c_e = true;  s.interna = 208;
    s.esterna_c_e = true;  s.esterna = 112;
    s.condizione  = "sunny";
    s.chiesta_c_e = true;  s.chiesta = 210;
    s.scalda      = true;
    /* Il riscaldamento del proprio piano si legge **davvero** anche qui: i
       piani vengono dalla configurazione e il loro stato dal fornitore del
       clima, che senza casa collegata ne accende uno. Inventarlo una seconda
       volta vorrebbe dire che la cattura e la schermata vera possono dire
       due cose diverse. */
    {
        const piano_t *mio = dati_piano(dati_piano_del_pannello());
        s.riscaldamento_acceso = mio && mio->disponibile && mio->acceso;
    }
    s.energia_c_e = true;  s.sole_w = 1860; s.casa_w = 740;
    s.batteria_c_e = true; s.batteria_pct = 64;
    return s;
}

standby_t dati_standby(void)
{
    standby_t s = {
        .stanza = tr(TX_STANDBY_INDOORS),
        .condizione = "",
        .apertura_nome = "",
    };

    /* L'etichetta si legge comunque: e configurazione, non un dato che
       arriva dalla rete, e vale anche mentre si guarda il simulatore. */
    const char *etichetta = cfg_testo("standby/indoor_temperature/label", "");
    if (etichetta && *etichetta) s.stanza = etichetta;

    if (!dati_dal_vero()) {
        s = inventata(s);
        /* Le aperture il finto fornitore le sa gia inventare da se: si
           chiedono a lui invece di rifarle qui. */
        s.aperture = dati_aperture();
        if (s.aperture > 0) {
            const apertura_t *a = dati_apertura(0);
            if (a && a->nome) s.apertura_nome = a->nome;
        }
        return s;
    }

    /* --- la temperatura della stanza ---------------------------------- */
    const double t = leggi("standby/indoor_temperature/entity",
                           "standby/indoor_temperature/attribute");
    if (t > ASSENTE) {
        s.interna_c_e = true;
        s.interna = decimi(t);
    }
    /* --- la temperatura di fuori --------------------------------------
     *
     * Il sensore locale sta in `meteo` e non qui, perche' non e un dato
     * dello standby: e lo stesso che la home mostra in alto e che la
     * sezione Clima scrive nella pastiglia «Esterno».
     *
     * La risoluzione — sensore locale, poi servizio meteo, poi niente — sta
     * tutta in dati_riassunto.c. Qui c'era una seconda copia, e una seconda
     * copia di una regola e' una regola che prima o poi diverge. */
    /* --- il riscaldamento del proprio piano ----------------------------
     *
     * Non «di casa»: di **questo** piano. Un pannello sta al posto del
     * termostato del suo piano e dice lo stato di cio che comanda; il
     * pannello del box, con lo stesso firmware e una riga di configurazione
     * diversa, dira il suo. Senza `clima.piano_pannello` non si dice
     * niente, che e meglio di dire quello di qualcun altro. */
    {
        const piano_t *mio = dati_piano(dati_piano_del_pannello());
        s.riscaldamento_acceso = mio && mio->disponibile && mio->acceso;
    }

    const meteo_t m = dati_meteo();
    if (m.temperatura != TEMP_IGNOTA) {
        s.esterna_c_e = true;
        s.esterna = m.temperatura;
    }
    if (m.disponibile && m.stato) s.condizione = m.stato;

    /* --- quanti gradi sono stati chiesti, e se sta scaldando -----------
     *
     * Facoltativo di proposito: senza l'entita del clima restano i gradi
     * misurati, che e una schermata onesta e completa. Con, si sa anche
     * se l'impianto sta facendo qualcosa — che e la domanda vera davanti
     * a un termostato quando la stanza e piu fredda di come la vuoi. */
    const char *cl = cfg_testo("standby/climate", "");
    if (cl && *cl && ent_vista(cl) && ent_disponibile(cl)) {
        const double r = ent_attributo_numero(cl, "temperature", ASSENTE);
        if (r > ASSENTE) {
            s.chiesta_c_e = true;
            s.chiesta = decimi(r);
        }
        /* `heating` e la sola azione che ci interessa: `idle` vuol dire
           acceso e fermo, e mostrarlo come "sta scaldando" sarebbe una
           bugia comoda. */
        s.scalda = strcmp(ent_attributo(cl, "hvac_action", ""), "heating") == 0;
    }

    /* --- l'energia ----------------------------------------------------
     *
     * Dalla sezione `energia`, la stessa della home: il sensore aggregato
     * e i nomi dei suoi attributi si scelgono li, una volta sola.
     *
     * Il consumo di casa e calcolato — produzione meno quello che va in
     * rete e in batteria — a meno che la configurazione non nomini un
     * attributo che lo misura davvero. Chi ha quel sensore ha un numero
     * migliore del nostro conto, e va usato il suo. */
    const energia_t en = dati_energia();
    if (en.disponibile) {
        s.energia_c_e = true;
        s.sole_w = en.produzione_w;

        const double casa = leggi("energy/aggregate_sensor",
                                  "energy/attributes/house_w");
        s.casa_w = casa > ASSENTE ? (int32_t)casa : dati_energia_casa_w();

        /* La batteria a parte: una casa senza accumulo ha l'impianto
           fotovoltaico e non ha la percentuale, e uno zero al suo posto
           direbbe "scarica" invece di "non ce n'e". */
        const char *ba = cfg_testo("energy/attributes/battery_pct", "");
        if (ba && *ba) {
            s.batteria_c_e = true;
            s.batteria_pct = en.batteria_pct;
        }
    }

    /* --- le aperture --------------------------------------------------
     *
     * Zero non e un dato da mostrare: e il motivo per cui il riquadro non
     * c'e. A casa chiusa quello spazio non deve dire "0 aperte", deve
     * stare zitto — cosi quando parla si nota. */
    s.aperture = dati_aperture();
    if (s.aperture > 0) {
        const apertura_t *a = dati_apertura(0);
        if (a && a->nome) s.apertura_nome = a->nome;
    }

    return s;
}
