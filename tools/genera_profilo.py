#!/usr/bin/env python3
"""
Genera main/profile.h a partire da docs/profili.json.

    python tools/genera_profilo.py             riscrive main/profile.h
    python tools/genera_profilo.py --verifica  esce 1 se il file e disallineato

`profile.h` e l'unico sorgente che puo contenere misure di layout, e non
si scrive a mano: si rigenera. Se una misura serve e non c'e, la si
aggiunge a `profili.json` — che a sua volta rispecchia `09-profili.md` —
e si rilancia questo script.

Il file generato non dipende da LVGL: contiene i **corpi** tipografici,
non i puntatori ai font. La corrispondenza corpo -> font compilato sta in
ui/fonts/, che e l'unico altro posto dove un numero ha diritto di
esistere, e li e l'identita di un font, non una misura di layout.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

RADICE = Path(__file__).resolve().parent.parent
SORGENTE = RADICE / "docs" / "profili.json"
USCITA = RADICE / "main" / "profile.h"

# L'elenco lo detta profili.json, nel suo ordine. Un generatore che pretende
# un elenco scritto a mano e' un secondo posto dove sta la stessa verita, e
# il giorno che le due non sono d'accordo si ferma per la ragione sbagliata.
def ordine(d) -> list:
    return list(d["profili"])


# Le famiglie, nell'ordine in cui compaiono. Anche questa si legge: con una
# sola famiglia FAM_QUANTE vale 1 e le tabelle di font e icone hanno una
# riga sola, che e' esattamente quello che si vuole quando di famiglie ce
# n'e' una.
def famiglie(d) -> list:
    viste = []
    for k in d["profili"]:
        f = k.split("-", 1)[0]
        if f not in viste:
            viste.append(f)
    return viste


def simbolo(chiave: str) -> str:
    return "PRF_" + chiave.upper().replace("-", "_")


def i(v, ripiego: int = 0) -> int:
    """Intero, con 0 al posto di null: 0 vuol dire assente o lasciato al flex."""
    if v is None:
        return ripiego
    return int(round(float(v)))


def coppia(v) -> str:
    if not v:
        return "{ 0, 0 }"
    return "{ %d, %d }" % (i(v[0]), i(v[1]))


def stringa(v) -> str:
    return "NULL" if v is None else json.dumps(str(v))


def griglia(g: dict, nome: str, ripiego=(0, 0)) -> tuple[int, int]:
    v = g.get(nome) or list(ripiego)
    return i(v[0]), i(v[1])


INTESTAZIONE = '''/* ------------------------------------------------------------------------
 * profile.h — l'UNICO file sorgente con misure di layout.
 *
 * GENERATO da tools/genera_profilo.py leggendo docs/profili.json.
 * Non modificarlo a mano: la modifica va in profili.json, e prima ancora
 * in 09-profili.md, che e la fonte delle divergenze fra i pannelli.
 *
 * Nessun altro sorgente puo contenere un numero di layout. Un componente
 * che ha bisogno di sapere quanto e largo il rail lo chiede al profilo:
 *
 *     lv_obj_set_width(rail, PRF->geo.rail_w);
 *
 * Un 0 al posto di una misura vuol dire "assente" oppure "lasciato al
 * layout": non e un valore, e l'assenza di un vincolo.
 *
 * Scelta del profilo
 * ------------------
 * Sul pannello e un parametro di compilazione, uno solo dei quattro:
 *
 *     -DPANNELLO_PROFILO=PRF_P4_1280X800
 *
 * Nel simulatore si compila con -DPROFILO_TUTTI e si sceglie all'avvio
 * con profilo_scegli("p4-800x1280"): PRF diventa un puntatore, il codice
 * dell'interfaccia resta identico nei due casi.
 *
 * La tabella la definisce una sola unita di traduzione, main/profile_def.c,
 * che compila questo header con PROFILE_IMPLEMENTATION.
 * --------------------------------------------------------------------- */
#ifndef PROFILE_H
#define PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ORIZZONTALE = 0, VERTICALE = 1 } orientamento_t;

/* La famiglia hardware, non la risoluzione: e cio che decide la scala
   tipografica e la presenza del co-processore.
   Il profilo verticale appartiene alla famiglia del proprio orizzontale,
   non a una terza.

   L'elenco lo detta profili.json: oggi la famiglia e una sola, e una sola
   deve essere anche qui. Un FAM_ che nessun profilo usa e' una riga che
   racconta un bersaglio che non esiste piu. */
@FAMIGLIE@

/* larghezza e altezza di un elemento tocabile o di un riquadro */
typedef struct { uint16_t w, h; } misura_t;

typedef struct {
    const char     *chiave;        /* "p4-800x1280" e simili */
    const char     *nome;
    const char     *scheda;
    famiglia_t      famiglia;
    orientamento_t  orientamento;

    struct {
        uint16_t larghezza, altezza;
        uint16_t ppi;
        const char *interfaccia;
    } schermo;

    struct {
        uint16_t psram_mb, ram_interna_kb, flash_mb;
    } memoria;

    struct {
        uint16_t rail_w;        /* 0 in verticale */
        uint16_t navbar_h;      /* 0 in orizzontale */
        /* Quante voci tiene la barra in basso, HOME e «Altro» comprese.
           Oltre, le sezioni che avanzano vanno nel foglio «Altro»: le voci
           non si stringono mai, e a dieci destinazioni il dito ci starebbe
           ancora ma l'etichetta no. 0 in orizzontale, dove c'e il rail e le
           voci si impilano. */
        uint8_t  barra_max;
        uint16_t head_h;        /* testata di sezione */
        uint16_t home_head_h;   /* testata della home */
        misura_t rail_item, rail_home, rail_gear;
        uint8_t  pad, gap;
        /* Il padding interno delle schede: piu stretto di quello di schermo,
           e diverso sui due assi. Finora si riusava `pad` per entrambi, e
           sul dettaglio del condizionatore quei pixel in piu erano proprio
           quelli che mancavano. */
        misura_t pad_scheda;    /* .w ai lati, .h sopra e sotto */
        uint8_t  radius;        /* schede */
        uint8_t  radius_tile;   /* riquadri e voci del rail */
        uint8_t  radius_btn;    /* pulsanti */
    } geo;

    /* corpi tipografici, non puntatori: la corrispondenza con il font
       compilato la fa ui/fonts/. Sei di scala piu quattro fuori scala. */
    struct {
        uint8_t f_xxl, f_xl, f_l, f_m, f_s, f_xs;
        /* `standby` e il corpo dei **due** numeri affiancati: l'ora e la
           temperatura della stanza. Era il corpo dell'orologio quando
           l'orologio era solo, ed e sceso quando gli si e affiancata la
           temperatura — due numeri da centotrenta pixel non ci stanno. */
        uint8_t standby, standby_temp, energia_home, clima_dettaglio,
                password_mono;
    } font;

    /* Le icone scalano col profilo (02-design-tokens.md): tre corpi, che
       coprono le misure disegnate nei mockup da 15 a 31 px. */
    /* Il quarto corpo esiste per la sola lampadina delle luci: il suo
       font contiene un'icona sola, perche a quella misura un font completo
       costerebbe mezzo megabyte di flash. */
    struct { uint8_t s, m, l, xl; } icona;

    struct {
        uint8_t luci_col, luci_rig;
        uint8_t clima_col, clima_rig;
        uint8_t cond_col, cond_rig;
        uint8_t agenda_col;     /* 1 = elenco con intestazioni di giorno */
        uint8_t scene_max;
        /* La fascia in fondo allo standby: quattro dati. In orizzontale
           stanno in riga, in verticale la riga non ci sta e diventa due
           per due. */
        uint8_t standby_col, standby_rig;
    } griglia;

    struct {
        uint16_t fascia_h;      /* 0 = altezza lasciata al flex */
        uint16_t presenza_w, energia_w, aperture_w, agenda_w;
        bool     dock;
        uint8_t  dock_max;      /* 0 in verticale: il dock non c'e */
    } home;

    /* Dettaglio del condizionatore: e la schermata piu fitta del progetto e
       ha misure sue. 0 in dettaglio_col_w vuol dire "colonna unica", cioe
       verticale. */
    struct {
        uint16_t dettaglio_col_w;
        misura_t timer_passo;
        uint16_t riquadro_h;      /* un riquadro di modalita o ventilazione */
        uint16_t segmento_h;      /* fila di tasti fisso/oscillante/... */
        uint16_t extra_h;         /* fila dei quattro interruttori Gree  */
        /* Quante zone di riscaldamento stanno in una pagina **quando ci sono
           le fasce dei piani**: una fascia costa una sessantina di pixel, e
           due fasce piu quattro righe di schede non ci stanno. Senza piani
           valgono le righe della griglia, che sono di piu. */
        uint8_t  righe_con_fasce;
        misura_t deflettore;      /* disegno parametrico della lamella   */
    } clima;

    struct {
        uint16_t riga_h;          /* una riga dell'elenco interruttori   */
        uint8_t  per_pagina;      /* quante ce ne stanno, oltre si impagina */
    } interruttori;

    struct {
        uint16_t fascia_h;        /* i quattro riquadri in cima a Energia */
        uint16_t banda_h;         /* la fascia dell'impianto con la curva  */
        uint16_t stringa_h;       /* un riquadro V o A di una stringa      */
        uint16_t consumo_riga_h;  /* una riga dell'elenco dei consumi      */
    } energia;

    struct {
        uint16_t colonna_w;   /* riquadro del QR; 0 = larghezza piena */
        uint16_t qr;          /* lato del codice, fondo bianco compreso */
    } wifi;

    struct {
        misura_t dock, apertura, clima_pm, clima_pm_dettaglio;

        /* I due comandi della sezione robot: piu alti dei pulsanti ordinari,
           perche' sono i bersagli piu grandi della loro schermata e vanno
           colpiti passando, spesso con la stanza gia scelta. */
        uint8_t  comando_robot_h;
        misura_t pin, tastiera, interruttore, pager;
        uint16_t pin_box_w;     /* 0 = il modale prende la larghezza utile */
        uint16_t riga_rete_h;
    } tocco;

    struct {
        uint8_t  passi_avvio;
        uint16_t ota_rollback_s;
        bool     partizione_c6;
        bool     rotazione_software;
        uint8_t  standby_shift_x, standby_shift_y;
    } comp;
} profilo_t;

/* Misure uguali su tutti i profili. Stanno qui e non sparse nei sorgenti
   perche la regola non ammette numeri di layout altrove, nemmeno costanti. */
typedef struct {
    uint8_t tocco_min;
    uint8_t bordo;
    uint8_t barra_scorrimento_w; /* la barra di scorrimento di un elenco */
    uint8_t tratto;   /* spessore di una linea disegnata */
    /* Tratteggio dei posti liberi nelle griglie paginate. Il CSS dei mockup
       dice solo "dashed", che ogni browser disegna a modo suo: qui il passo
       e scelto, non copiato. */
    uint8_t tratteggio_segno, tratteggio_vuoto;
    /* Pastiglia: il riquadretto che porta la modalita di un condizionatore
       o un'etichetta breve. Misurata sul mockup del clima. */
    uint8_t pastiglia_pad_v, pastiglia_pad_h, pastiglia_gap;
    uint8_t gap_stretto;   /* fra un nome e il suo stato */
    uint8_t righe_titolo_evento;   /* poi tronca */
    uint8_t righe_nome_luce;       /* idem, nella griglia delle luci */
    /* Indicatore a barrette della ventilazione: quante ne sono piene dice
       la velocita. Misurato sul mockup del clima. */
    uint8_t barretta_w, barretta_gap, barretta_h;
    uint8_t barra_percentuale_h;

    /* La pila della batteria del robot: una barretta sotto la percentuale.
       Il numero dice quanto, la pila lo fa vedere — e da un metro si legge
       la seconda molto prima del primo. */
    uint8_t pila_w, pila_h;
    uint8_t pallino_paginatore;         /* pagina non corrente: un tondo    */
    uint8_t pallino_paginatore_attivo;  /* pagina corrente: piu lungo, non
                                           piu grande, cosi la fascia non
                                           cambia altezza                   */
    uint8_t gap_pallini;
    uint8_t velo_conferma_pct;
    uint8_t opacita_dato_vecchio_pct;
    uint8_t opacita_evento_passato_pct;
    uint8_t opacita_standby_pct;
    uint8_t opacita_condizionatore_spento_pct;
    uint16_t spaziatura_maiuscoletto_millesimi;  /* em x 1000 */
    uint16_t spaziatura_dock_millesimi;
    uint16_t dissolvenza_modale_ms;
} comuni_t;

extern const comuni_t COM;
'''

CODA = '''
#ifdef __cplusplus
}
#endif

#endif /* PROFILE_H */
'''


def tabella(chiave: str, p: dict) -> str:
    g = p.get("geometria", {})
    f = p.get("font", {})
    gr = p.get("griglie", {})
    h = p.get("home", {})
    t = p.get("tocco", {})
    b = p.get("comportamenti", {})
    s = p.get("schermo", {})
    m = p.get("memoria", {})

    luci = griglia(gr, "luci")
    clima = griglia(gr, "clima")
    sb = griglia(gr, "standby")
    cond = griglia(gr, "condizionatori", (4, 1))
    shift = b.get("standby_shift") or [0, 0]

    return f'''{{
    .chiave = {stringa(chiave)},
    .nome   = {stringa(p.get("nome"))},
    .scheda = {stringa(p.get("scheda"))},
    .famiglia = FAM_{chiave.split("-", 1)[0].upper()},
    .orientamento = {"VERTICALE" if p.get("orientamento") == "verticale" else "ORIZZONTALE"},

    .schermo = {{ .larghezza = {i(s.get("larghezza"))}, .altezza = {i(s.get("altezza"))},
                 .ppi = {i(s.get("ppi"))}, .interfaccia = {stringa(s.get("interfaccia"))} }},

    .memoria = {{ .psram_mb = {i(m.get("psram_mb"))},
                 .ram_interna_kb = {i(m.get("ram_interna_kb"))},
                 .flash_mb = {i(m.get("flash_mb"))} }},

    .geo = {{
        .rail_w      = {i(g.get("rail_w"))},
        .navbar_h    = {i(g.get("navbar_h"))},
        .barra_max   = {i(g.get("barra_max"))},
        .head_h      = {i(g.get("head_h"))},
        .home_head_h = {i(g.get("home_head_h"))},
        .rail_item   = {coppia(g.get("rail_item"))},
        .rail_home   = {coppia(g.get("rail_home"))},
        .rail_gear   = {coppia(g.get("rail_gear"))},
        .pad = {i(g.get("pad"))}, .gap = {i(g.get("gap"))},
        .pad_scheda = {coppia(g.get("pad_scheda"))},
        .radius = {i(g.get("radius"))}, .radius_tile = {i(g.get("radius_tile"))},
        .radius_btn = {i(g.get("radius_btn"))},
    }},

    .font = {{
        .f_xxl = {i(f.get("f_xxl"))}, .f_xl = {i(f.get("f_xl"))}, .f_l = {i(f.get("f_l"))},
        .f_m   = {i(f.get("f_m"))},   .f_s  = {i(f.get("f_s"))},  .f_xs = {i(f.get("f_xs"))},
        .standby = {i(f.get("standby"))}, .standby_temp = {i(f.get("standby_temp"))},
        .energia_home = {i(f.get("energia_home"))},
        .clima_dettaglio = {i(f.get("clima_dettaglio"))},
        .password_mono = {i(f.get("password_mono"))},
    }},

    .icona = {{ .s = {i(p.get("icone",{}).get("s"))}, .m = {i(p.get("icone",{}).get("m"))}, .l = {i(p.get("icone",{}).get("l"))}, .xl = {i(p.get("icone",{}).get("xl"))} }},

    .griglia = {{
        .luci_col = {luci[0]}, .luci_rig = {luci[1]},
        .clima_col = {clima[0]}, .clima_rig = {clima[1]},
        .cond_col = {cond[0]}, .cond_rig = {cond[1]},
        .agenda_col = {i(gr.get("agenda_colonne"))}, .scene_max = {i(gr.get("scene_max"))},
        .standby_col = {sb[0]}, .standby_rig = {sb[1]},
    }},

    .home = {{
        .fascia_h = {i(h.get("fascia_h"))},
        .presenza_w = {i(h.get("presenza_w"))}, .energia_w = {i(h.get("energia_w"))},
        .aperture_w = {i(h.get("aperture_w"))}, .agenda_w = {i(h.get("agenda_w"))},
        .dock = {"true" if h.get("dock") else "false"},
        .dock_max = {i(h.get("dock_max"))},
    }},

    .clima = {{
        .dettaglio_col_w = {i(p.get("clima",{}).get("dettaglio_col_w"))},
        .timer_passo = {coppia(p.get("clima",{}).get("timer_passo"))},
        .riquadro_h = {i(p.get("clima",{}).get("riquadro_h"))},
        .segmento_h = {i(p.get("clima",{}).get("segmento_h"))},
        .extra_h = {i(p.get("clima",{}).get("extra_h"))},
        .righe_con_fasce = {i(p.get("clima",{}).get("righe_con_fasce"))},
        .deflettore = {coppia(p.get("clima",{}).get("deflettore"))},
    }},

    .interruttori = {{
        .riga_h = {i(p.get("interruttori",{}).get("riga_h"))},
        .per_pagina = {i(p.get("interruttori",{}).get("per_pagina"))},
    }},

    .energia = {{ .fascia_h = {i(p.get("energia",{}).get("fascia_h"))},
                  .banda_h = {i(p.get("energia",{}).get("banda_h"))},
                  .stringa_h = {i(p.get("energia",{}).get("stringa_h"))},
                  .consumo_riga_h = {i(p.get("energia",{}).get("consumo_riga_h"))} }},

    .wifi = {{ .colonna_w = {i(p.get("wifi",{}).get("colonna_w"))},
               .qr = {i(p.get("wifi",{}).get("qr"))} }},

    .tocco = {{
        .dock = {coppia(t.get("dock"))},
        .apertura = {coppia(t.get("apertura"))},
        .comando_robot_h = {i(t.get("comando_robot_h"))},
        .clima_pm = {coppia(t.get("clima_pm"))},
        .clima_pm_dettaglio = {coppia(t.get("clima_pm_dettaglio"))},
        .pin = {coppia(t.get("pin"))},
        .tastiera = {coppia(t.get("tastiera"))},
        .interruttore = {coppia(t.get("interruttore"))},
        .pager = {coppia(t.get("pager"))},
        .pin_box_w = {i(t.get("pin_box_w"))},
        .riga_rete_h = {i(t.get("riga_rete_h"))},
    }},

    .comp = {{
        .passi_avvio = {i(b.get("passi_avvio"))},
        .ota_rollback_s = {i(b.get("ota_rollback_s"))},
        .partizione_c6 = {"true" if b.get("partizione_c6") else "false"},
        .rotazione_software = {"true" if b.get("rotazione_software") else "false"},
        .standby_shift_x = {i(shift[0])}, .standby_shift_y = {i(shift[1])},
    }},
}}'''


def comuni(c: dict) -> str:
    return f'''const comuni_t COM = {{
    .tocco_min = {i(c.get("tocco_min"))},
    .bordo = {i(c.get("bordo"))},
    .barra_scorrimento_w = {i(c.get("barra_scorrimento_w"))},
    .tratto = {i(c.get("tratto"))},
    .tratteggio_segno = {i(c.get("tratteggio_segno"))},
    .tratteggio_vuoto = {i(c.get("tratteggio_vuoto"))},
    .pastiglia_pad_v = {i(c.get("pastiglia_pad_v"))},
    .pastiglia_pad_h = {i(c.get("pastiglia_pad_h"))},
    .pastiglia_gap = {i(c.get("pastiglia_gap"))},
    .gap_stretto = {i(c.get("gap_stretto"))},
    .righe_titolo_evento = {i(c.get("righe_titolo_evento"))},
    .righe_nome_luce = {i(c.get("righe_nome_luce"))},
    .barretta_w = {i(c.get("barretta_w"))},
    .barretta_gap = {i(c.get("barretta_gap"))},
    .barretta_h = {i(c.get("barretta_h"))},
    .barra_percentuale_h = {i(c.get("barra_percentuale_h"))},
    .pila_w = {i(c.get("pila_w"))}, .pila_h = {i(c.get("pila_h"))},
    .pallino_paginatore = {i(c.get("pallino_paginatore"))},
    .pallino_paginatore_attivo = {i(c.get("pallino_paginatore_attivo"))},
    .gap_pallini = {i(c.get("gap_pallini"))},
    .velo_conferma_pct = {i(c.get("velo_conferma_pct"))},
    .opacita_dato_vecchio_pct = {i(c.get("opacita_dato_vecchio_pct"))},
    .opacita_evento_passato_pct = {i(c.get("opacita_evento_passato_pct"))},
    .opacita_standby_pct = {i(c.get("opacita_standby_pct"))},
    .opacita_condizionatore_spento_pct = {i(c.get("opacita_condizionatore_spento_pct"))},
    .spaziatura_maiuscoletto_millesimi = {i(float(c.get("spaziatura_maiuscoletto_em", 0)) * 1000)},
    .spaziatura_dock_millesimi = {i(float(c.get("spaziatura_dock_em", 0)) * 1000)},
    .dissolvenza_modale_ms = {i(c.get("dissolvenza_modale_ms"))},
}};'''


def quanti_profili() -> int:
    return len(ordine(json.loads(SORGENTE.read_text(encoding="utf-8"))))


def genera() -> str:
    d = json.loads(SORGENTE.read_text(encoding="utf-8"))
    prof = d["profili"]
    ORDINE = ordine(d)
    if not ORDINE:
        raise SystemExit("profili.json non ha nessun profilo")

    fam = famiglie(d)
    enum = ", ".join(f"FAM_{f.upper()} = {n}" for n, f in enumerate(fam))
    out = [INTESTAZIONE.replace(
        "@FAMIGLIE@",
        f"typedef enum {{ {enum}, FAM_QUANTE = {len(fam)} }} famiglia_t;")]

    predefinito = d.get("_predefinito", ORDINE[0])
    out.append("/* Il pannello montato per primo: e il profilo su cui parte il")
    out.append("   simulatore senza argomenti, e quello che la compilazione per il")
    out.append("   pannello usa se non le si dice altro. Sta in profili.json. */")
    out.append("#define PANNELLO_PROFILO_PREDEFINITO "
               + json.dumps(predefinito) + "\n")

    out.append(f"/* --- identificativi dei {len(ORDINE)} profili --- */")
    for n, k in enumerate(ORDINE):
        out.append(f"#define {simbolo(k)} {n}")
    out.append(f"#define PRF_QUANTI {len(ORDINE)}\n")

    out.append('''#ifdef PROFILO_TUTTI
/* Simulatore: tutte le tabelle, scelta all'avvio. */
extern const profilo_t PROFILI[PRF_QUANTI];
extern const profilo_t *PRF;

/* Sceglie il profilo per chiave. Falso se la chiave non esiste. */
bool profilo_scegli(const char *chiave);
/* La chiave dell'n-esimo profilo, per l'elenco a schermo. NULL fuori range. */
const char *profilo_chiave(int n);

#else
/* Pannello: un solo profilo, deciso alla compilazione. */
#ifndef PANNELLO_PROFILO
#error "definire PANNELLO_PROFILO, per esempio -DPANNELLO_PROFILO=PRF_P4_800X1280"
#endif
extern const profilo_t PROFILO_CORRENTE;
#define PRF (&PROFILO_CORRENTE)
#endif
''')

    out.append("\n/* ===================== definizione della tabella ===================== */")
    out.append("#ifdef PROFILE_IMPLEMENTATION\n")
    out.append(comuni(d.get("comuni", {})))
    out.append("")

    out.append("#ifdef PROFILO_TUTTI")
    out.append("const profilo_t PROFILI[PRF_QUANTI] = {")
    for k in ORDINE:
        out.append(f"  /* [{simbolo(k)}] */")
        out.append("  " + tabella(k, prof[k]).replace("\n", "\n  ") + ",")
    out.append("};")
    out.append('''const profilo_t *PRF = &PROFILI[0];

bool profilo_scegli(const char *chiave)
{
    for (int n = 0; n < PRF_QUANTI; n++) {
        const char *a = PROFILI[n].chiave, *b = chiave;
        while (*a && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) { PRF = &PROFILI[n]; return true; }
    }
    return false;
}

const char *profilo_chiave(int n)
{
    return (n >= 0 && n < PRF_QUANTI) ? PROFILI[n].chiave : NULL;
}
#else''')
    for n, k in enumerate(ORDINE):
        guardia = "#if" if n == 0 else "#elif"
        out.append(f"{guardia} PANNELLO_PROFILO == {simbolo(k)}")
        out.append("const profilo_t PROFILO_CORRENTE = " + tabella(k, prof[k]) + ";")
    out.append('#else\n#error "PANNELLO_PROFILO non riconosciuto"\n#endif')
    out.append("#endif /* PROFILO_TUTTI */\n")
    out.append("#endif /* PROFILE_IMPLEMENTATION */")
    out.append(CODA)
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifica", action="store_true",
                    help="non riscrive, esce 1 se profile.h e disallineato")
    arg = ap.parse_args()

    nuovo = genera()
    if arg.verifica:
        if not USCITA.exists():
            print("main/profile.h non esiste: lancia tools/genera_profilo.py")
            return 1
        if USCITA.read_text(encoding="utf-8") != nuovo:
            print("main/profile.h non corrisponde a docs/profili.json: "
                  "rilancia tools/genera_profilo.py")
            return 1
        print("main/profile.h allineato a docs/profili.json")
        return 0

    USCITA.parent.mkdir(parents=True, exist_ok=True)
    # LF on every system: on Windows write_text() would use CRLF, and git
    # would report the file changed and then normalise it back.
    USCITA.write_text(nuovo, encoding="utf-8", newline="\n")
    print(f"scritto {USCITA.relative_to(RADICE)} "
          f"({len(nuovo.splitlines())} righe, "
          f"{quanti_profili()} profili)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
