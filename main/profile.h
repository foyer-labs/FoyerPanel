/* ------------------------------------------------------------------------
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
typedef enum { FAM_P4 = 0, FAM_QUANTE = 1 } famiglia_t;

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

/* Il pannello montato per primo: e il profilo su cui parte il
   simulatore senza argomenti, e quello che la compilazione per il
   pannello usa se non le si dice altro. Sta in profili.json. */
#define PANNELLO_PROFILO_PREDEFINITO "p4-800x1280"

/* --- identificativi dei 2 profili --- */
#define PRF_P4_1280X800 0
#define PRF_P4_800X1280 1
#define PRF_QUANTI 2

#ifdef PROFILO_TUTTI
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


/* ===================== definizione della tabella ===================== */
#ifdef PROFILE_IMPLEMENTATION

const comuni_t COM = {
    .tocco_min = 44,
    .bordo = 1,
    .barra_scorrimento_w = 6,
    .tratto = 2,
    .tratteggio_segno = 4,
    .tratteggio_vuoto = 4,
    .pastiglia_pad_v = 6,
    .pastiglia_pad_h = 9,
    .pastiglia_gap = 7,
    .gap_stretto = 4,
    .righe_titolo_evento = 2,
    .righe_nome_luce = 2,
    .barretta_w = 4,
    .barretta_gap = 3,
    .barretta_h = 20,
    .barra_percentuale_h = 9,
    .pila_w = 56, .pila_h = 6,
    .pallino_paginatore = 8,
    .pallino_paginatore_attivo = 22,
    .gap_pallini = 8,
    .velo_conferma_pct = 76,
    .opacita_dato_vecchio_pct = 45,
    .opacita_evento_passato_pct = 45,
    .opacita_standby_pct = 62,
    .opacita_condizionatore_spento_pct = 68,
    .spaziatura_maiuscoletto_millesimi = 160,
    .spaziatura_dock_millesimi = 80,
    .dissolvenza_modale_ms = 150,
};

#ifdef PROFILO_TUTTI
const profilo_t PROFILI[PRF_QUANTI] = {
  /* [PRF_P4_1280X800] */
  {
      .chiave = "p4-1280x800",
      .nome   = "pannello da 10,1\"",
      .scheda = "ESP32-P4-WIFI6-Touch-LCD 10,1\"",
      .famiglia = FAM_P4,
      .orientamento = ORIZZONTALE,
  
      .schermo = { .larghezza = 1280, .altezza = 800,
                   .ppi = 149, .interfaccia = "mipi_dsi" },
  
      .memoria = { .psram_mb = 32,
                   .ram_interna_kb = 768,
                   .flash_mb = 16 },
  
      .geo = {
          .rail_w      = 106,
          .navbar_h    = 0,
          .barra_max   = 0,
          .head_h      = 66,
          .home_head_h = 102,
          .rail_item   = { 88, 88 },
          .rail_home   = { 88, 66 },
          .rail_gear   = { 88, 56 },
          .pad = 20, .gap = 14,
          .pad_scheda = { 18, 16 },
          .radius = 16, .radius_tile = 13,
          .radius_btn = 11,
      },
  
      .font = {
          .f_xxl = 56, .f_xl = 38, .f_l = 26,
          .f_m   = 17,   .f_s  = 14,  .f_xs = 12,
          .standby = 126, .standby_temp = 84,
          .energia_home = 52,
          .clima_dettaglio = 96,
          .password_mono = 34,
      },
  
      .icona = { .s = 19, .m = 24, .l = 30, .xl = 64 },
  
      .griglia = {
          .luci_col = 3, .luci_rig = 4,
          .clima_col = 3, .clima_rig = 4,
          .cond_col = 4, .cond_rig = 1,
          .agenda_col = 4, .scene_max = 5,
          .standby_col = 4, .standby_rig = 1,
      },
  
      .home = {
          .fascia_h = 300,
          .presenza_w = 250, .energia_w = 296,
          .aperture_w = 320, .agenda_w = 0,
          .dock = true,
          .dock_max = 7,
      },
  
      .clima = {
          .dettaglio_col_w = 420,
          .timer_passo = { 56, 50 },
          .riquadro_h = 96,
          .segmento_h = 52,
          .extra_h = 56,
          .righe_con_fasce = 5,
          .deflettore = { 34, 28 },
      },
  
      .interruttori = {
          .riga_h = 72,
          .per_pagina = 8,
      },
  
      .energia = { .fascia_h = 118,
                    .banda_h = 132,
                    .stringa_h = 116,
                    .consumo_riga_h = 62 },
  
      .wifi = { .colonna_w = 420,
                 .qr = 250 },
  
      .tocco = {
          .dock = { 171, 195 },
          .apertura = { 132, 52 },
          .comando_robot_h = 66,
          .clima_pm = { 48, 42 },
          .clima_pm_dettaglio = { 78, 78 },
          .pin = { 108, 72 },
          .tastiera = { 116, 105 },
          .interruttore = { 58, 32 },
          .pager = { 48, 44 },
          .pin_box_w = 740,
          .riga_rete_h = 110,
      },
  
      .comp = {
          .passi_avvio = 6,
          .ota_rollback_s = 120,
          .partizione_c6 = true,
          .rotazione_software = true,
          .standby_shift_x = 4, .standby_shift_y = 5,
      },
  },
  /* [PRF_P4_800X1280] */
  {
      .chiave = "p4-800x1280",
      .nome   = "pannello da 10,1\" in verticale",
      .scheda = "ESP32-P4-WIFI6-Touch-LCD 10,1\"",
      .famiglia = FAM_P4,
      .orientamento = VERTICALE,
  
      .schermo = { .larghezza = 800, .altezza = 1280,
                   .ppi = 149, .interfaccia = "mipi_dsi" },
  
      .memoria = { .psram_mb = 32,
                   .ram_interna_kb = 768,
                   .flash_mb = 16 },
  
      .geo = {
          .rail_w      = 0,
          .navbar_h    = 112,
          .barra_max   = 6,
          .head_h      = 72,
          .home_head_h = 116,
          .rail_item   = { 0, 0 },
          .rail_home   = { 0, 0 },
          .rail_gear   = { 0, 0 },
          .pad = 18, .gap = 14,
          .pad_scheda = { 17, 15 },
          .radius = 16, .radius_tile = 13,
          .radius_btn = 11,
      },
  
      .font = {
          .f_xxl = 56, .f_xl = 38, .f_l = 26,
          .f_m   = 17,   .f_s  = 14,  .f_xs = 12,
          .standby = 126, .standby_temp = 84,
          .energia_home = 52,
          .clima_dettaglio = 96,
          .password_mono = 34,
      },
  
      .icona = { .s = 19, .m = 24, .l = 30, .xl = 64 },
  
      .griglia = {
          .luci_col = 2, .luci_rig = 7,
          .clima_col = 2, .clima_rig = 6,
          .cond_col = 2, .cond_rig = 2,
          .agenda_col = 1, .scene_max = 5,
          .standby_col = 2, .standby_rig = 2,
      },
  
      .home = {
          .fascia_h = 0,
          .presenza_w = 0, .energia_w = 0,
          .aperture_w = 0, .agenda_w = 0,
          .dock = false,
          .dock_max = 0,
      },
  
      .clima = {
          .dettaglio_col_w = 0,
          .timer_passo = { 56, 50 },
          .riquadro_h = 96,
          .segmento_h = 60,
          .extra_h = 60,
          .righe_con_fasce = 8,
          .deflettore = { 34, 28 },
      },
  
      .interruttori = {
          .riga_h = 76,
          .per_pagina = 12,
      },
  
      .energia = { .fascia_h = 118,
                    .banda_h = 132,
                    .stringa_h = 116,
                    .consumo_riga_h = 62 },
  
      .wifi = { .colonna_w = 0,
                 .qr = 250 },
  
      .tocco = {
          .dock = { 171, 195 },
          .apertura = { 132, 52 },
          .comando_robot_h = 66,
          .clima_pm = { 46, 42 },
          .clima_pm_dettaglio = { 78, 78 },
          .pin = { 0, 76 },
          .tastiera = { 72, 88 },
          .interruttore = { 58, 32 },
          .pager = { 48, 44 },
          .pin_box_w = 0,
          .riga_rete_h = 110,
      },
  
      .comp = {
          .passi_avvio = 6,
          .ota_rollback_s = 120,
          .partizione_c6 = true,
          .rotazione_software = false,
          .standby_shift_x = 4, .standby_shift_y = 5,
      },
  },
};
const profilo_t *PRF = &PROFILI[0];

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
#else
#if PANNELLO_PROFILO == PRF_P4_1280X800
const profilo_t PROFILO_CORRENTE = {
    .chiave = "p4-1280x800",
    .nome   = "pannello da 10,1\"",
    .scheda = "ESP32-P4-WIFI6-Touch-LCD 10,1\"",
    .famiglia = FAM_P4,
    .orientamento = ORIZZONTALE,

    .schermo = { .larghezza = 1280, .altezza = 800,
                 .ppi = 149, .interfaccia = "mipi_dsi" },

    .memoria = { .psram_mb = 32,
                 .ram_interna_kb = 768,
                 .flash_mb = 16 },

    .geo = {
        .rail_w      = 106,
        .navbar_h    = 0,
        .barra_max   = 0,
        .head_h      = 66,
        .home_head_h = 102,
        .rail_item   = { 88, 88 },
        .rail_home   = { 88, 66 },
        .rail_gear   = { 88, 56 },
        .pad = 20, .gap = 14,
        .pad_scheda = { 18, 16 },
        .radius = 16, .radius_tile = 13,
        .radius_btn = 11,
    },

    .font = {
        .f_xxl = 56, .f_xl = 38, .f_l = 26,
        .f_m   = 17,   .f_s  = 14,  .f_xs = 12,
        .standby = 126, .standby_temp = 84,
        .energia_home = 52,
        .clima_dettaglio = 96,
        .password_mono = 34,
    },

    .icona = { .s = 19, .m = 24, .l = 30, .xl = 64 },

    .griglia = {
        .luci_col = 3, .luci_rig = 4,
        .clima_col = 3, .clima_rig = 4,
        .cond_col = 4, .cond_rig = 1,
        .agenda_col = 4, .scene_max = 5,
        .standby_col = 4, .standby_rig = 1,
    },

    .home = {
        .fascia_h = 300,
        .presenza_w = 250, .energia_w = 296,
        .aperture_w = 320, .agenda_w = 0,
        .dock = true,
        .dock_max = 7,
    },

    .clima = {
        .dettaglio_col_w = 420,
        .timer_passo = { 56, 50 },
        .riquadro_h = 96,
        .segmento_h = 52,
        .extra_h = 56,
        .righe_con_fasce = 5,
        .deflettore = { 34, 28 },
    },

    .interruttori = {
        .riga_h = 72,
        .per_pagina = 8,
    },

    .energia = { .fascia_h = 118,
                  .banda_h = 132,
                  .stringa_h = 116,
                  .consumo_riga_h = 62 },

    .wifi = { .colonna_w = 420,
               .qr = 250 },

    .tocco = {
        .dock = { 171, 195 },
        .apertura = { 132, 52 },
        .comando_robot_h = 66,
        .clima_pm = { 48, 42 },
        .clima_pm_dettaglio = { 78, 78 },
        .pin = { 108, 72 },
        .tastiera = { 116, 105 },
        .interruttore = { 58, 32 },
        .pager = { 48, 44 },
        .pin_box_w = 740,
        .riga_rete_h = 110,
    },

    .comp = {
        .passi_avvio = 6,
        .ota_rollback_s = 120,
        .partizione_c6 = true,
        .rotazione_software = true,
        .standby_shift_x = 4, .standby_shift_y = 5,
    },
};
#elif PANNELLO_PROFILO == PRF_P4_800X1280
const profilo_t PROFILO_CORRENTE = {
    .chiave = "p4-800x1280",
    .nome   = "pannello da 10,1\" in verticale",
    .scheda = "ESP32-P4-WIFI6-Touch-LCD 10,1\"",
    .famiglia = FAM_P4,
    .orientamento = VERTICALE,

    .schermo = { .larghezza = 800, .altezza = 1280,
                 .ppi = 149, .interfaccia = "mipi_dsi" },

    .memoria = { .psram_mb = 32,
                 .ram_interna_kb = 768,
                 .flash_mb = 16 },

    .geo = {
        .rail_w      = 0,
        .navbar_h    = 112,
        .barra_max   = 6,
        .head_h      = 72,
        .home_head_h = 116,
        .rail_item   = { 0, 0 },
        .rail_home   = { 0, 0 },
        .rail_gear   = { 0, 0 },
        .pad = 18, .gap = 14,
        .pad_scheda = { 17, 15 },
        .radius = 16, .radius_tile = 13,
        .radius_btn = 11,
    },

    .font = {
        .f_xxl = 56, .f_xl = 38, .f_l = 26,
        .f_m   = 17,   .f_s  = 14,  .f_xs = 12,
        .standby = 126, .standby_temp = 84,
        .energia_home = 52,
        .clima_dettaglio = 96,
        .password_mono = 34,
    },

    .icona = { .s = 19, .m = 24, .l = 30, .xl = 64 },

    .griglia = {
        .luci_col = 2, .luci_rig = 7,
        .clima_col = 2, .clima_rig = 6,
        .cond_col = 2, .cond_rig = 2,
        .agenda_col = 1, .scene_max = 5,
        .standby_col = 2, .standby_rig = 2,
    },

    .home = {
        .fascia_h = 0,
        .presenza_w = 0, .energia_w = 0,
        .aperture_w = 0, .agenda_w = 0,
        .dock = false,
        .dock_max = 0,
    },

    .clima = {
        .dettaglio_col_w = 0,
        .timer_passo = { 56, 50 },
        .riquadro_h = 96,
        .segmento_h = 60,
        .extra_h = 60,
        .righe_con_fasce = 8,
        .deflettore = { 34, 28 },
    },

    .interruttori = {
        .riga_h = 76,
        .per_pagina = 12,
    },

    .energia = { .fascia_h = 118,
                  .banda_h = 132,
                  .stringa_h = 116,
                  .consumo_riga_h = 62 },

    .wifi = { .colonna_w = 0,
               .qr = 250 },

    .tocco = {
        .dock = { 171, 195 },
        .apertura = { 132, 52 },
        .comando_robot_h = 66,
        .clima_pm = { 46, 42 },
        .clima_pm_dettaglio = { 78, 78 },
        .pin = { 0, 76 },
        .tastiera = { 72, 88 },
        .interruttore = { 58, 32 },
        .pager = { 48, 44 },
        .pin_box_w = 0,
        .riga_rete_h = 110,
    },

    .comp = {
        .passi_avvio = 6,
        .ota_rollback_s = 120,
        .partizione_c6 = true,
        .rotazione_software = false,
        .standby_shift_x = 4, .standby_shift_y = 5,
    },
};
#else
#error "PANNELLO_PROFILO non riconosciuto"
#endif
#endif /* PROFILO_TUTTI */

#endif /* PROFILE_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* PROFILE_H */
