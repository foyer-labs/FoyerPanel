# ---------------------------------------------------------------------------
# Il componente principale del firmware.
#
# I sorgenti dell'interfaccia sono gli stessi del simulatore e stanno in
# ../../main: si leggono da ui_sources.cmake, che e l'elenco condiviso. Un
# file aggiunto li compare in tutti e due i bersagli, ed e il modo per
# accorgersi subito se qualcosa in ui/ ha smesso di compilare per uno dei
# due.
#
# Ci sono **tutti**, compresi web e Home Assistant, e non e strafare: i loro
# trasporti sono a socket, e lwIP da i socket BSD anche qui. Quello che cambia fra PC e apparecchio sono i due porti veri —
# archivio su LittleFS, magazzino su NVS — e sono quelli che questa cartella
# aggiunge.
#
# Qui dentro stanno anche i due pezzi che esistono solo sull'apparecchio e
# non hanno un corrispondente sul PC: la radio Wi-Fi e la console sulla
# seriale. La console e il modo in cui password e token entrano senza passare
# da nessun file del progetto — e il rientro per quando la rete non c'e e la
# pagina di configurazione e irraggiungibile.
# ---------------------------------------------------------------------------
get_filename_component(RADICE ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
include(${RADICE}/ui_sources.cmake)

# --- la scheda -------------------------------------------------------------
#
# Lo schermo e i piedini della scheda stanno in due file suoi, dietro due
# intestazioni — hw_lcd.h, hw_scheda.h — che il resto del firmware conosce
# senza sapere quale silicio ci sia sotto. Il bersaglio e uno, l'ESP32-P4:
# un altro si ferma qui invece di compilare contro i piedini sbagliati.
if(NOT IDF_TARGET STREQUAL "esp32p4")
    message(FATAL_ERROR "il pannello si compila per esp32p4, non per ${IDF_TARGET}")
endif()
set(PANNELLO_SRC_SCHEDA hw_lcd_dsi.c hw_scheda.c)
# La radio sta su un altro chip. esp_wifi_remote presenta la stessa
# intestazione — esp_wifi.h — con le stesse funzioni, ed e per questo che
# rete.c non ha un solo ramo condizionale: da questa parte del confine non
# si sa di stare parlando via SDIO con un ESP32-C6.
#
# `esp_wifi` sta nell'elenco anche qui, e non e una svista: in questa
# generazione del componente l'intestazione finta non viene esposta da
# esp_wifi_remote, viene **iniettata** in quella di esp_wifi. Chi include
# esp_wifi.h la trova solo se ha esp_wifi fra i propri requisiti, e senza
# l'errore e «esp_wifi.h: No such file or directory» su un bersaglio che
# il Wi-Fi ce l'ha, che manda a cercare dalla parte sbagliata.
set(PANNELLO_REQ_RADIO esp_wifi esp_wifi_remote esp_hosted)

idf_component_register(
    SRCS
        main.c
        ${PANNELLO_SRC_SCHEDA}
        hw_tocco.c
        archivio_littlefs.c
        magazzino_nvs.c
        rete.c
        console.c
        nel_grafico.c
        canale_esp.c
        sistema_esp.c
        registro_esp.c
        aggiornamento_esp.c
        ${PANNELLO_SRC_PROFILO}
        ${PANNELLO_SRC_UI}
        ${PANNELLO_SRC_APP}
        ${PANNELLO_SRC_I18N}
        ${PANNELLO_SRC_CFG}
        ${PANNELLO_SRC_WEB}
        ${PANNELLO_SRC_HA}
        ${PANNELLO_SRC_RETE}
    INCLUDE_DIRS
        .
        ${PANNELLO_INCLUDE_COMUNI}
        ${PANNELLO_MAIN_DIR}/hw
    REQUIRES
        esp_lcd
        # Il vetro capacitivo, un GSL3680: sta in firmware/components
        # perche' il suo firmware e legato a questo modello di vetro e viene
        # dal pacchetto del costruttore. esp_lcd_touch e l'interfaccia
        # comune, e viene dal registro.
        esp_lcd_touch
        esp_lcd_touch_gsl3680
        # Anche il vetro e nostro, e un componente locale non si aggancia da
        # solo come uno del registro: va nominato.
        esp_lcd_jd9365
        driver
        esp_timer
        esp_psram        # heap_caps_get_total_size sulla PSRAM
        spi_flash        # esp_flash_get_size: la verifica all'avvio
        app_update       # l'OTA: partizioni, firma, rollback
        nvs_flash
        ${PANNELLO_REQ_RADIO}   # la radio, che sta su un altro chip
        esp_netif         # e chi le da un indirizzo
        lwip              # SNTP: l'ora viene da li
        esp_event
        esp-tls           # il canale cifrato verso Home Assistant
        mdns              # pannello.local
        console           # i due segreti entrano da qui
        json
        littlefs
        lvgl
)

# Il profilo: una sola tabella nel binario, scelta qui.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    PANNELLO_PROFILO=${PANNELLO_PROFILO_SCELTA}
    # I font e le icone generati includono LVGL in due modi a seconda di
    # questa definizione: "lvgl.h" se c'e, "lvgl/lvgl.h" se manca. Nel
    # simulatore LVGL e una cartella scaricata e vale la seconda; qui e un
    # componente gestito e l'intestazione sta in cima, quindi vale la
    # prima. La guardia ce l'avevano gia — mancava solo di accenderla.
    LV_LVGL_H_INCLUDE_SIMPLE=1)

# --- la pagina di configurazione viaggia col firmware ----------------------
#
# main/web/pagina.h e generato da web/index.html e docs/config.schema.json,
# ed e compilato dentro il binario. Finche la generazione era un comando da
# lanciare a mano, il firmware poteva partire con una pagina piu vecchia
# dello schema che lo stesso firmware usa per validare: la pagina disegna i
# campi guardando lo schema, e uno schema che parla di campi che quella
# pagina non sa disegnare da una sezione vuota. Non un errore, una sezione
# vuota — che e il modo peggiore di sbagliare, perche sembra una scelta.
#
# Ora la generazione e un passo della compilazione. Non scrive niente se il
# file e gia allineato, quindi non sporca l'albero senza motivo.
idf_build_get_property(python PYTHON)
add_custom_target(pagina_fw ALL
    COMMAND ${python} ${RADICE}/tools/genera_pagina.py
    BYPRODUCTS ${RADICE}/main/web/pagina.h
    COMMENT "pagina di configurazione da web/index.html e config.schema.json")
add_dependencies(${COMPONENT_LIB} pagina_fw)

# The language tables, generated the same way and for the same reason.
add_custom_target(texts_fw ALL
    COMMAND ${python} ${RADICE}/tools/gen_texts.py
    BYPRODUCTS ${RADICE}/main/i18n/texts_gen.h ${RADICE}/main/i18n/texts_gen.c
    COMMENT "panel texts from i18n/*.json")
add_dependencies(${COMPONENT_LIB} texts_fw)

# La versione viene da git come nel simulatore: scritta a mano si sbaglia,
# e su un pannello a muro la versione e la prima cosa che si chiede quando
# qualcosa non va.
add_custom_target(versione_fw ALL
    COMMAND ${CMAKE_COMMAND}
            -DRADICE=${RADICE}
            -DUSCITA=${CMAKE_BINARY_DIR}/generato/versione.h
            -P ${RADICE}/tools/versione.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/generato/versione.h
    COMMENT "versione da git describe")
add_dependencies(${COMPONENT_LIB} versione_fw)
target_include_directories(${COMPONENT_LIB} PUBLIC ${CMAKE_BINARY_DIR}/generato)

# La maniglia per provare il ritorno indietro (main.c, giro di prova). Senza
# questa riga `idf.py -DPANNELLO_ROLLBACK_FINTO=1 build` passerebbe una
# variabile a CMake e nessuna macro al compilatore: il firmware si
# costruirebbe identico e il collaudo direbbe che il rollback non funziona.
if(PANNELLO_ROLLBACK_FINTO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PANNELLO_ROLLBACK_FINTO=1)
    message(WARNING "immagine di collaudo: non confermera mai se stessa")
endif()

# Il banco di prova del robot: /api/prova/robot, e la cattura dell'ultima
# risposta di Home Assistant che quel percorso legge. Spento di riposo perche
# e un comando **fisico** raggiungibile da chiunque sia sulla rete di casa —
# far partire l'aspirapolvere — e va bene per una prova concordata, non per
# sempre.
#
# Non e cancellato: riscriverlo la prossima volta costerebbe piu di questa
# riga, e con esso si perderebbe tutto quello che aveva imparato a
# interrogare — la mappa delle stanze, gli attributi dell'entita, il JSON
# esatto che parte. Come il banco del rollback, si accende quando serve e in
# una cartella sua.
if(PANNELLO_BANCO_ROBOT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PANNELLO_BANCO_ROBOT=1)
    message(WARNING "banco del robot acceso: /api/prova/robot e aperto a chiunque sia in rete")
endif()
