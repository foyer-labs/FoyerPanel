/* ------------------------------------------------------------------------
 * Configurazione di LVGL per il simulatore.
 *
 * Contiene solo le voci che ci discostano dai valori predefiniti: in LVGL 9
 * lv_conf_internal.h riempie tutto il resto, quindi un file corto e valido e
 * si legge.
 *
 * Attenzione: qui non ci vanno misure di layout. Le uniche misure ammesse
 * sono quelle della libreria grafica — dimensione dei buffer, profondita di
 * colore — che riguardano come si disegna, non che aspetto ha l'interfaccia.
 * --------------------------------------------------------------------- */
#ifndef LV_CONF_H
#define LV_CONF_H

/* lv_conf_internal.h viene incluso anche da lv_blend_helium.S: se stdint.h
   arrivasse fino all'assemblatore, questo si troverebbe davanti i typedef
   della libc e si fermerebbe. */
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/* 16 bit come sul pannello (CONFIG_LV_COLOR_DEPTH_16): disegnare sul PC a 32
   bit nasconderebbe le bande di colore che sul vetro si vedono. */
#define LV_COLOR_DEPTH 16

/* --- perche qui la memoria viene dalla libc e non da un blocco fisso ----
 *
 * Sul pannello LVGL ha 64 kB fissi in RAM interna, e ci si sbatte contro:
 * la schermata di diagnostica costruiva piu oggetti di quanti ce ne
 * stessero, lv_obj_create tornava NULL e l'apparecchio si riavviava. Verrebbe
 * voglia di dare al simulatore lo stesso blocco, cosi da scoprirlo sul PC.
 *
 * **Non si puo**, ed e stato provato. Sul pannello quel blocco contiene solo
 * gli oggetti: i framebuffer sono del DSI e stanno in PSRAM. Nel
 * simulatore il buffer di disegno lo alloca LVGL, dallo stesso blocco — e
 * 1280x800 a sedici bit sono 2 MB. Con 64 kB il simulatore non riesce
 * nemmeno ad aprire lo schermo: "lv_display_set_buffers: buf1 != NULL".
 *
 * Dargli un blocco piu grande per far stare il buffer significherebbe
 * lasciare agli oggetti tutto lo spazio avanzato, cioe non misurare piu
 * niente. Quindi qui la memoria resta quella della libc, e si accetta che
 * **questa** classe di guasti — troppi oggetti in una schermata — il
 * simulatore non la veda. Si vede sul pannello, e da li si legge con
 * `stato`, che dice quanto blocco e usato e quanto e grande il pezzo libero
 * piu grande.
 *
 * Il giorno in cui il simulatore prendesse il proprio buffer di disegno da
 * fuori — malloc suo, passato a lv_display_set_buffers — questa nota
 * andrebbe riscritta, perche allora il blocco conterrebbe solo oggetti come
 * sul pannello e il confronto tornerebbe onesto. */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_OS LV_OS_NONE

/* Il tempo lo fornisce il simulatore, cosi le temporizzazioni di
   01-specifica-ui.md §5 si possono accelerare durante le prove. */
#define LV_TICK_CUSTOM 0

/* --- driver di uscita --- */
#define LV_USE_SDL              1
#define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT        1
#define LV_SDL_FULLSCREEN       0
#define LV_SDL_DIRECT_EXIT      1

/* --- diagnostica: severa nel simulatore, e giusto cosi --- */
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF           1
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_OBJ       1


/* Serve al criterio di 11-collaudo.md §1 sull'heap che non cresce. */
#define LV_USE_MEM_MONITOR      1
#define LV_USE_SYSMON           1
#define LV_USE_PERF_MONITOR     1   /* l'attrezzo c'e; si mostra con --fps */

/* --- widget e componenti che il progetto usa davvero --- */
#define LV_USE_QRCODE           1   /* condivisione Wi-Fi, 01 §3.7 */
#define LV_USE_CANVAS           1   /* deflettore parametrico, superfici video */
#define LV_USE_CHART            1   /* grafico della giornata, 01 §3.3 */
#define LV_USE_ARC              1   /* anello della pressione prolungata */

/* Decodifica JPEG **solo nel simulatore**. Sul pannello la fa l'hardware:
   il P4 ha il decoder JPEG. TJpgDec sta dentro
   LVGL, e' un file solo e non porta dipendenze, e serve a una cosa sola —
   che il simulatore mostri i fotogrammi veri invece di rettangoli. Senza,
   meta della Fase 4 si guarderebbe a occhio. */
#define LV_USE_TJPGD            1
/* TJpgDec decodifica da un buffer in memoria solo attraverso il filesystem
   in memoria: senza, rifiuta e scrive un avviso nel registro. Serve a
   questo e a nient'altro. */
#define LV_USE_FS_MEMFS         1
#define LV_FS_MEMFS_LETTER      'M'

/* Niente SVG a runtime: e una regola del progetto, non un'ottimizzazione.
   Le icone sono font-icona o immagini precompilate. Senza queste tre righe
   LVGL si porta dietro ThorVG, che e un motore vettoriale in C++. */
#define LV_USE_VECTOR_GRAPHIC   0
#define LV_USE_THORVG_INTERNAL  0
#define LV_USE_SVG              0

/* Non li usiamo e occupano spazio. */
#define LV_USE_ANIMIMG          0
#define LV_USE_CALENDAR         0
#define LV_USE_IMGFONT          0
#define LV_USE_SPAN             0
#define LV_USE_TABVIEW          0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0
#define LV_USE_MENU             0
#define LV_USE_SPINBOX          0

/* --- font ---
   I corpi sono Inter compilata da tools/genera_font.py secondo
   09-profili.md §4, piu JetBrains Mono per la password del Wi-Fi. Nessun
   font di libreria: occuperebbero spazio senza essere mai disegnati.

   Il predefinito serve solo come rete: il codice chiede sempre un ruolo con
   font(FT_M) e non nomina mai un corpo.

   **Deve essere un corpo che i profili di oggi generano davvero.** Una
   volta era un corpo che i profili avevano smesso di chiedere: quel file
   non si compilava piu e il collegamento si e fermato su un simbolo
   mancante dentro LVGL — non nel nostro codice, che quel corpo non lo
   nomina mai. Il giorno che questa riga nomina un corpo
   che nessun profilo chiede, l'errore riparte da li.

   La dichiarazione passa da LV_FONT_CUSTOM_DECLARE e non da un
   LV_FONT_DECLARE scritto qui: lv_conf.h viene incluso prima che lv_font_t
   esista, e un extern anticipato fa cadere tutta la compilazione. */
#define LV_FONT_CUSTOM_DECLARE  LV_FONT_DECLARE(font_md_17)
#define LV_FONT_DEFAULT         &font_md_17

#endif /* LV_CONF_H */
