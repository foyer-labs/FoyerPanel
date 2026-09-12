/* ------------------------------------------------------------------------
 * L'impronta di una sezione — quanto basta per sapere se ridisegnare.
 *
 * Il pannello ricostruiva la schermata a ogni cambiamento di **qualunque**
 * entita seguita. Con un contatore di energia che si aggiorna ogni secondo,
 * la pagina delle Luci si rifaceva di continuo senza che nessuna luce si
 * fosse accesa — e un ridisegno pieno, su questo schermo, non e gratis: e la
 * periferica RGB che resta a secco di pixel e perde i sincronismi. Sul vetro
 * si vede come un lampo nero e un'immagine che scivola di lato.
 *
 * Qui si calcola un numero che riassume **cio che quella sezione mostra**.
 * Se non cambia, non c'e niente di nuovo da far vedere e non si ridisegna.
 *
 * --- perche sui dati e non sulle entita ---------------------------------
 *
 * La strada ovvia sarebbe una tabella "sezione -> entita di configurazione
 * che le servono". Sarebbe una seconda descrizione di cosa mostra ogni
 * schermata, accanto a quella che le schermate hanno gia nel codice — e due
 * descrizioni della stessa cosa si allontanano al primo cambiamento, in
 * silenzio: la schermata smetterebbe di aggiornarsi e nessuno saprebbe
 * perche.
 *
 * L'impronta si prende invece sugli **stessi accessori** che la schermata
 * usa per disegnare. Non puo sfasarsi da cio che si vede, perche guarda
 * esattamente quello.
 *
 * --- cosa non entra nell'impronta ---------------------------------------
 *
 * I nomi e la struttura: cambiano solo salvando una configurazione nuova, e
 * quella strada passa gia da ui_avvia(), che rifa tutto. Qui interessano i
 * valori che si muovono da soli.
 * --------------------------------------------------------------------- */
#include "impronta.h"

#include <string.h>

#include "dati.h"
#include "orologio.h"
#include "sezioni.h"

/* FNV-1a. Non e una firma: se due stati diversi dessero lo stesso numero si
   perderebbe un ridisegno, non un dato — e la probabilita e quella di
   indovinare due volte di fila un numero da quattro miliardi. */
static uint32_t mescola(uint32_t h, uint32_t v)
{
    for (int n = 0; n < 4; n++) {
        h ^= (v >> (n * 8)) & 0xFF;
        h *= 16777619u;
    }
    return h;
}

static uint32_t mescola_testo(uint32_t h, const char *s)
{
    for (; s && *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

static uint32_t luci(uint32_t h)
{
    for (int n = 0; n < dati_luci(); n++) {
        const luce_t *z = dati_luce(n);
        if (!z) continue;
        h = mescola(h, (uint32_t)(z->acceso | (z->disponibile << 1)));
        h = mescola(h, z->percento);
    }
    return mescola(h, (uint32_t)dati_luci_accese());
}

static uint32_t clima(uint32_t h)
{
    for (int n = 0; n < dati_zone_clima(); n++) {
        const zona_clima_t *z = dati_zona_clima(n);
        if (!z) continue;
        h = mescola(h, (uint32_t)(uint16_t)z->misurata);
        h = mescola(h, (uint32_t)(uint16_t)z->richiesta);
        h = mescola(h, (uint32_t)(z->chiama | (z->accesa << 1)
                                  | (z->disponibile << 2)));
        h = mescola(h, z->umidita);
    }
    for (int n = 0; n < dati_condizionatori(); n++) {
        const condizionatore_t *u = dati_condizionatore(n);
        if (!u) continue;
        h = mescola(h, (uint32_t)u->modo);
        h = mescola(h, (uint32_t)(uint16_t)u->stanza);
        h = mescola(h, (uint32_t)(uint16_t)u->richiesta);
        h = mescola(h, (uint32_t)(u->ventilazione | (u->deflettore << 8)
                                  | (u->oscillazione << 16)));
        h = mescola(h, (uint32_t)(u->disponibile | (u->timer_corre << 1)
                                  | (u->automazione_attiva << 2)));
        h = mescola(h, (uint32_t)(u->restano_min | (u->durata_min << 16)));
        /* La barra si muove anche quando i minuti no: un timer prolungato
           cambia il totale e basta. */
        h = mescola(h, (uint32_t)(u->timer_quota_pct
                                  | (u->timer_totale_min << 8)));
        for (int k = 0; k < 4; k++) h = mescola(h, (uint32_t)u->extra[k]);
    }
    return h;
}

static uint32_t energia(uint32_t h)
{
    const energia_t e = dati_energia();
    h = mescola(h, (uint32_t)e.produzione_w);
    h = mescola(h, (uint32_t)e.rete_w);
    h = mescola(h, (uint32_t)e.batteria_w);
    h = mescola(h, e.batteria_pct);
    h = mescola(h, (uint32_t)e.oggi_wh);
    h = mescola(h, (uint32_t)e.disponibile);
    for (int n = 0; n < dati_stringhe(); n++) {
        const stringa_t *s = dati_stringa(n);
        if (!s) continue;
        h = mescola(h, s->tensione_decimi);
        h = mescola(h, s->corrente_centesimi);
        h = mescola(h, (uint32_t)s->potenza_w);
        h = mescola(h, (uint32_t)s->disponibile);
    }
    return h;
}

/* Le programmazioni cambiano da sole in due modi: gli orari, quando
   qualcuno li tocca da un'altra parte, e le finestre «in corso», che
   scattano a un minuto preciso senza che nessuna entita cambi. Il secondo e
   il motivo per cui qui entra anche l'ora: senza, l'ambra della finestra in
   corso comparirebbe solo al primo tocco. */
static uint32_t programmazioni(uint32_t h)
{
    h = mescola(h, (uint32_t)orologio_minuti());
    for (int g = 0; g < dati_programmazioni(); g++) {
        const programmazione_t *p = dati_programmazione(g);
        if (!p) continue;
        h = mescola(h, (uint32_t)(p->acceso | (p->disponibile << 1)));
        h = mescola(h, (uint32_t)p->watt);
        for (int n = 0; n < p->finestre; n++) {
            const finestra_t *f = dati_finestra(g, n);
            if (!f) continue;
            h = mescola(h, (uint32_t)(uint16_t)f->da_min);
            h = mescola(h, (uint32_t)(uint16_t)f->a_min);
            h = mescola(h, (uint32_t)f->in_corso);
        }
        for (int n = 0; n < p->automazioni; n++) {
            const automazione_t *a = dati_automazione(g, n);
            if (!a) continue;
            h = mescola(h, (uint32_t)(a->attiva | (a->disponibile << 1)));
            h = mescola_testo(h, a->ultimo_scatto);
        }
    }
    return h;
}

static uint32_t accessi(uint32_t h)
{
    for (int n = 0; n < dati_accessi(); n++) {
        const accesso_t *a = dati_accesso(n);
        if (!a) continue;
        h = mescola(h, (uint32_t)(a->stato_noto | (a->aperto << 1)
                                  | (a->acceso << 2) | (a->disponibile << 3)));
        h = mescola_testo(h, a->ultimo_uso);
    }
    return h;
}

static uint32_t casa(uint32_t h)
{
    h = mescola(h, (uint32_t)(dati_persone_in_casa() | (dati_persone() << 8)));
    for (int n = 0; n < dati_persone(); n++) {
        const persona_t *p = dati_persona(n);
        if (!p) continue;
        h = mescola(h, (uint32_t)p->in_casa);
        h = mescola_testo(h, p->quando);
    }
    h = mescola(h, (uint32_t)(dati_aperture() | (dati_aperture_note() << 8)));
    for (int n = 0; n < dati_aperture(); n++) {
        const apertura_t *a = dati_apertura(n);
        if (a) { h = mescola_testo(h, a->nome); h = mescola_testo(h, a->da); }
    }

    const meteo_t m = dati_meteo();
    h = mescola(h, (uint32_t)(uint16_t)m.temperatura);
    h = mescola(h, (uint32_t)m.disponibile);
    h = mescola_testo(h, m.stato);

    /* La home riassume tutto il resto, quindi eredita tutto il resto. */
    return energia(clima(luci(h)));
}

uint32_t impronta_sezione(int sezione)
{
    /* La sezione entra nell'impronta: cosi cambiando pagina il numero
       cambia da solo, senza doverlo azzerare a mano da qualche parte. */
    uint32_t h = mescola(2166136261u, (uint32_t)sezione);

    switch (sezione) {
    case SEZ_LUCI:       return luci(h);
    case SEZ_CLIMA:      return clima(h);
    case SEZ_ENERGIA:    return energia(h);
    case SEZ_ACCESSI:    return accessi(h);
    case SEZ_PROGRAMMAZIONI: return programmazioni(h);
    case SEZ_HOME:       return casa(h);
    default:
        /* Agenda, Wi-Fi, impostazioni e diagnostica non mostrano valori che
           si muovono da soli: un'impronta costante vuol dire che restano
           ferme finche non le si tocca. Se un giorno una di loro mostrasse
           qualcosa che cambia, va aggiunta qui — e il posto dove cercare
           quando una schermata "non si aggiorna". */
        return h;
    }
}
