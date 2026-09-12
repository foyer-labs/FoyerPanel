/* ------------------------------------------------------------------------
 * icons.h — le icone in uso, come stringhe UTF-8.
 *
 * GENERATO da tools/genera_icone.py. Non modificare a mano: l'icona nuova si
 * aggiunge all'elenco dello script, che la ritrova nel font e ricompila.
 *
 * Sono normalissime stringhe, quindi si usano come tali:
 *
 *     lv_label_set_text(l, ICO_LUCE);
 *     lv_obj_set_style_text_font(l, icona(IC_M), 0);
 *
 * Font-icona, non SVG: 02-design-tokens.md lo vieta a runtime.
 * --------------------------------------------------------------------- */
#ifndef ICONS_H
#define ICONS_H

#include "lvgl.h"

/* Quattro corpi, che scalano col profilo. Il quarto — IC_XL — esiste per
   una cosa sola: la lampadina al centro della scheda di una luce, che e
   insieme lo stato e il bersaglio del dito. Il suo font contiene **solo**
   le icone elencate in ICONE_XL dentro tools/genera_icone.py, perche a
   quella dimensione un font completo costerebbe mezzo megabyte di flash.
   Chiedere IC_XL per un'icona che non e in quell'elenco da un rettangolo
   vuoto: si aggiunge li, dove il costo si vede. */
typedef enum { IC_S, IC_M, IC_L, IC_XL, IC_QUANTI } icona_corpo_t;

/* Il font-icona del corpo, per il profilo attivo. Mai NULL. */
const lv_font_t *icona(icona_corpo_t corpo);


/* --- navigazione --- */
#define ICO_HOME                           "\xEE\xA2\x8A"  /* U+E88A home */
#define ICO_LIGHTBULB                      "\xEE\x83\xB0"  /* U+E0F0 lightbulb */
#define ICO_DEVICE_THERMOSTAT              "\xEE\x87\xBF"  /* U+E1FF device_thermostat */
#define ICO_SOLAR_POWER                    "\xEE\xB0\x8F"  /* U+EC0F solar_power */
#define ICO_VIDEOCAM                       "\xEE\x81\x8B"  /* U+E04B videocam */
#define ICO_GARAGE                         "\xEF\x80\x91"  /* U+F011 garage */
#define ICO_CALENDAR_MONTH                 "\xEE\xAF\x8C"  /* U+EBCC calendar_month */
#define ICO_WIFI                           "\xEE\x98\xBE"  /* U+E63E wifi */
#define ICO_SETTINGS                       "\xEE\xA2\xB8"  /* U+E8B8 settings */
#define ICO_MOP                            "\xEE\x8A\x8D"  /* U+E28D mop */

/* --- home --- */
#define ICO_PERSON                         "\xEE\x9F\xBD"  /* U+E7FD person */
#define ICO_BOLT                           "\xEE\xA8\x8B"  /* U+EA0B bolt */
#define ICO_BATTERY_FULL                   "\xEE\x86\xA4"  /* U+E1A4 battery_full */
#define ICO_SENSOR_WINDOW                  "\xEF\x86\xB4"  /* U+F1B4 sensor_window */
#define ICO_LOCK                           "\xEE\xA2\x8D"  /* U+E88D lock */
#define ICO_LOCK_OPEN                      "\xEE\xA2\x98"  /* U+E898 lock_open */
#define ICO_NOTIFICATIONS                  "\xEE\x9F\xB4"  /* U+E7F4 notifications */
#define ICO_NOTIFICATIONS_OFF              "\xEE\x9F\xB6"  /* U+E7F6 notifications_off */
#define ICO_SCHEDULE                       "\xEE\x86\x92"  /* U+E192 schedule */
#define ICO_LOCAL_LAUNDRY_SERVICE          "\xEE\x95\x8A"  /* U+E54A local_laundry_service */
#define ICO_DRY                            "\xEF\x86\xB3"  /* U+F1B3 dry */

/* --- meteo --- */
#define ICO_SUNNY                          "\xEE\xA0\x9A"  /* U+E81A sunny */
#define ICO_CLOUD                          "\xEE\x8A\xBD"  /* U+E2BD cloud */
#define ICO_RAINY                          "\xEF\x85\xB6"  /* U+F176 rainy */
#define ICO_BEDTIME                        "\xEE\x87\xB9"  /* U+E1F9 bedtime */

/* --- riscaldamento --- */
#define ICO_HEAT                           "\xEF\x94\xB7"  /* U+F537 heat */

/* --- primo_avvio --- */
#define ICO_SIGNAL_WIFI_4_BAR              "\xEE\x87\x98"  /* U+E1D8 signal_wifi_4_bar */
#define ICO_NETWORK_CHECK                  "\xEE\x99\x80"  /* U+E640 network_check */
#define ICO_DONE                           "\xEE\xA1\xB6"  /* U+E876 done */

/* --- accessi --- */
#define ICO_FENCE                          "\xEF\x87\xB6"  /* U+F1F6 fence */
#define ICO_GATE                           "\xEE\x89\xB7"  /* U+E277 gate */
#define ICO_DOOR_FRONT                     "\xEE\xBF\xBD"  /* U+EFFD door_front */
#define ICO_DOOR_OPEN                      "\xEE\x9D\xBC"  /* U+E77C door_open */

/* --- clima --- */
#define ICO_LOCAL_FIRE_DEPARTMENT          "\xEE\xA8\x85"  /* U+EA05 local_fire_department */
#define ICO_AC_UNIT                        "\xEE\xAC\xBB"  /* U+EB3B ac_unit */
#define ICO_AUTORENEW                      "\xEE\x80\xA8"  /* U+E028 autorenew */
#define ICO_WATER_DROP                     "\xEE\x9E\x98"  /* U+E798 water_drop */
#define ICO_MODE_FAN                       "\xEF\x85\xA8"  /* U+F168 mode_fan */
#define ICO_POWER_SETTINGS_NEW             "\xEE\xA2\xAC"  /* U+E8AC power_settings_new */
#define ICO_TIMER                          "\xEE\x90\xA5"  /* U+E425 timer */
#define ICO_AIR                            "\xEE\xBF\x98"  /* U+EFD8 air */
#define ICO_VOLUME_OFF                     "\xEE\x81\x8F"  /* U+E04F volume_off */
#define ICO_LIGHT_MODE                     "\xEE\x94\x98"  /* U+E518 light_mode */
#define ICO_SWAP_VERT                      "\xEE\x83\x83"  /* U+E0C3 swap_vert */
#define ICO_HEIGHT                         "\xEE\xA8\x96"  /* U+EA16 height */
#define ICO_WIND_POWER                     "\xEE\xB0\x8C"  /* U+EC0C wind_power */

/* --- comandi --- */
#define ICO_ADD                            "\xEE\x85\x85"  /* U+E145 add */
#define ICO_REMOVE                         "\xEE\x85\x9B"  /* U+E15B remove */
#define ICO_CHECK                          "\xEE\x97\x8A"  /* U+E5CA check */
#define ICO_CLOSE                          "\xEE\x85\x8C"  /* U+E14C close */
#define ICO_ARROW_BACK                     "\xEE\x97\x84"  /* U+E5C4 arrow_back */
#define ICO_ARROW_FORWARD                  "\xEE\x97\x88"  /* U+E5C8 arrow_forward */
#define ICO_CHEVRON_LEFT                   "\xEE\x90\x88"  /* U+E408 chevron_left */
#define ICO_CHEVRON_RIGHT                  "\xEE\x90\x89"  /* U+E409 chevron_right */
#define ICO_EXPAND_MORE                    "\xEE\x97\x8F"  /* U+E5CF expand_more */
#define ICO_KEYBOARD_ARROW_UP              "\xEE\x8C\x96"  /* U+E316 keyboard_arrow_up */
#define ICO_MORE_HORIZ                     "\xEE\x97\x93"  /* U+E5D3 more_horiz */
#define ICO_DONE                           "\xEE\xA1\xB6"  /* U+E876 done */
#define ICO_VISIBILITY                     "\xEE\x90\x97"  /* U+E417 visibility */
#define ICO_VISIBILITY_OFF                 "\xEE\xA3\xB5"  /* U+E8F5 visibility_off */
#define ICO_CONTENT_COPY                   "\xEE\x85\x8D"  /* U+E14D content_copy */
#define ICO_BACKSPACE                      "\xEE\x85\x8A"  /* U+E14A backspace */
#define ICO_KEYBOARD_CAPSLOCK              "\xEE\x8C\x98"  /* U+E318 keyboard_capslock */
#define ICO_SPACE_BAR                      "\xEE\x89\x96"  /* U+E256 space_bar */
#define ICO_KEYBOARD                       "\xEE\x8C\x92"  /* U+E312 keyboard */

/* --- stati --- */
#define ICO_WARNING                        "\xEE\x80\x82"  /* U+E002 warning */
#define ICO_ERROR                          "\xEE\x80\x80"  /* U+E000 error */
#define ICO_INFO                           "\xEE\xA2\x8E"  /* U+E88E info */
#define ICO_CLOUD_OFF                      "\xEE\x8B\x81"  /* U+E2C1 cloud_off */
#define ICO_REFRESH                        "\xEE\x97\x95"  /* U+E5D5 refresh */
#define ICO_SYNC                           "\xEE\x98\xA7"  /* U+E627 sync */
#define ICO_SYNC_PROBLEM                   "\xEE\x98\xA9"  /* U+E629 sync_problem */
#define ICO_WIFI_OFF                       "\xEE\x99\x88"  /* U+E648 wifi_off */
#define ICO_HOURGLASS_EMPTY                "\xEE\xA2\x8B"  /* U+E88B hourglass_empty */
#define ICO_UPDATE                         "\xEE\xA4\xA3"  /* U+E923 update */

/* --- interruttori --- */
#define ICO_OUTLET                         "\xEF\x87\x94"  /* U+F1D4 outlet */
#define ICO_POWER_SETTINGS_NEW             "\xEE\xA2\xAC"  /* U+E8AC power_settings_new */
#define ICO_LIGHTBULB                      "\xEE\x83\xB0"  /* U+E0F0 lightbulb */
#define ICO_BOLT                           "\xEE\xA8\x8B"  /* U+EA0B bolt */
#define ICO_CABLE                          "\xEE\xBF\xA6"  /* U+EFE6 cable */
#define ICO_MODE_FAN                       "\xEF\x85\xA8"  /* U+F168 mode_fan */
#define ICO_WIND_POWER                     "\xEE\xB0\x8C"  /* U+EC0C wind_power */
#define ICO_AC_UNIT                        "\xEE\xAC\xBB"  /* U+EB3B ac_unit */
#define ICO_LOCAL_FIRE_DEPARTMENT          "\xEE\xA8\x85"  /* U+EA05 local_fire_department */
#define ICO_HEAT_PUMP                      "\xEE\xB0\x98"  /* U+EC18 heat_pump */
#define ICO_WATER_DROP                     "\xEE\x9E\x98"  /* U+E798 water_drop */
#define ICO_VALVE                          "\xEE\x88\xA4"  /* U+E224 valve */
#define ICO_POOL                           "\xEE\xAD\x88"  /* U+EB48 pool */
#define ICO_SHOWER                         "\xEF\x81\xA1"  /* U+F061 shower */
#define ICO_GRASS                          "\xEF\x88\x85"  /* U+F205 grass */
#define ICO_SUNNY                          "\xEE\xA0\x9A"  /* U+E81A sunny */
#define ICO_TV                             "\xEE\x8C\xB3"  /* U+E333 tv */
#define ICO_SPEAKER                        "\xEE\x8C\xAD"  /* U+E32D speaker */
#define ICO_COFFEE                         "\xEE\xBF\xAF"  /* U+EFEF coffee */
#define ICO_KITCHEN                        "\xEE\xAD\x87"  /* U+EB47 kitchen */
#define ICO_VIDEOCAM                       "\xEE\x81\x8B"  /* U+E04B videocam */
#define ICO_ROUTER                         "\xEE\x8C\xA8"  /* U+E328 router */
#define ICO_STORAGE                        "\xEE\x87\x9B"  /* U+E1DB storage */
#define ICO_GARAGE                         "\xEF\x80\x91"  /* U+F011 garage */
#define ICO_DOOR_FRONT                     "\xEE\xBF\xBD"  /* U+EFFD door_front */
#define ICO_SPEED                          "\xEE\xA7\xA4"  /* U+E9E4 speed */
#define ICO_TIMER                          "\xEE\x90\xA5"  /* U+E425 timer */
#define ICO_SENSORS                        "\xEE\x94\x9E"  /* U+E51E sensors */

/* --- diagnostica --- */
#define ICO_QR_CODE_2                      "\xEE\x80\x8A"  /* U+E00A qr_code_2 */
#define ICO_DELETE                         "\xEE\xA1\xB2"  /* U+E872 delete */
#define ICO_MEMORY                         "\xEE\x8C\xA2"  /* U+E322 memory */
#define ICO_STORAGE                        "\xEE\x87\x9B"  /* U+E1DB storage */
#define ICO_SPEED                          "\xEE\xA7\xA4"  /* U+E9E4 speed */
#define ICO_BUG_REPORT                     "\xEE\xA1\xA8"  /* U+E868 bug_report */
#define ICO_FILTER_ALT                     "\xEE\xBD\x8F"  /* U+EF4F filter_alt */
#define ICO_RESTART_ALT                    "\xEF\x81\x93"  /* U+F053 restart_alt */
#define ICO_BRIGHTNESS_MEDIUM              "\xEE\x86\xAE"  /* U+E1AE brightness_medium */
#define ICO_NETWORK_CHECK                  "\xEE\x99\x80"  /* U+E640 network_check */
#define ICO_ROUTER                         "\xEE\x8C\xA8"  /* U+E328 router */
#define ICO_SENSORS                        "\xEE\x94\x9E"  /* U+E51E sensors */
#define ICO_TUNE                           "\xEE\x90\xA9"  /* U+E429 tune */

/* --- l'icona scelta in configurazione -----------------------------------
 *
 * La sezione Interruttori lascia scegliere l'icona di ogni voce dalla
 * pagina web, e in config.json quella scelta e una **stringa**: "outlet",
 * "mode_fan". Qui si traduce nel glifo.
 *
 * L'elenco e chiuso — il gruppo `interruttori` di tools/genera_icone.py — e
 * lo schema propone gli stessi nomi. Un nome che non c'e torna il ripiego
 * invece di NULL: chi disegna non deve avere un ramo in piu, e un
 * interruttore senza icona valida e comunque un interruttore da mostrare. */
const char *icona_da_nome(const char *nome, const char *ripiego);

/* Quante ne puo scegliere chi configura, e la n-esima: servono alla prova
   che confronta questo elenco con l'enum dello schema. */
int         icone_scegliibili(void);
const char *icona_scegliibile(int n);

#endif /* ICONS_H */
