# ---------------------------------------------------------------------------
# Compilazione per il PC: simulatore SDL2 e prove.
#
# Il pannello si compila con ESP-IDF (idf.py build) e usa main/CMakeLists.txt.
# Questo file serve alla Fase 1, che gira interamente sul PC.
#
#     cmake -B build -G Ninja
#     cmake --build build
#     ./build/pannello --profilo p4-1280x800
#
# Preparazione dell'ambiente in docs/sviluppo.md. Su Windows si usa WSL2: SDL2 e la
# toolchain sono molto piu lisci li.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
project(pannello C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/ui_sources.cmake)

# texts_gen.h is included by the config validator too (for the language
# codes), and the validator is compiled into half the tests: one include
# directory for all of them instead of one line in each.
include_directories(${PANNELLO_MAIN_DIR}/i18n)

# --- il profilo e generato, non scritto a mano ------------------------------
# Rigenerarlo a ogni configurazione costa nulla ed evita la classe di errori
# piu noiosa: profili.json modificato e profile.h vecchio.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/genera_profilo.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        RESULT_VARIABLE _gen
        OUTPUT_VARIABLE _gen_out)
    if(NOT _gen EQUAL 0)
        message(FATAL_ERROR "generazione di profile.h fallita:\n${_gen_out}")
    endif()
    message(STATUS "profilo: ${_gen_out}")

    # Stessa ragione del profilo: pagina modificata e pagina.h vecchia e la
    # classe di errori piu noiosa che ci sia.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/genera_pagina.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        RESULT_VARIABLE _pag
        OUTPUT_VARIABLE _pag_out)
    if(NOT _pag EQUAL 0)
        message(FATAL_ERROR "generazione di pagina.h fallita:\n${_pag_out}")
    endif()
    message(STATUS "pagina: ${_pag_out}")

    # And the texts, for the same reason: a JSON edited and a table not
    # regenerated shows yesterday's word, and compiles without a warning.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/gen_texts.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        RESULT_VARIABLE _txt
        OUTPUT_VARIABLE _txt_out)
    if(NOT _txt EQUAL 0)
        message(FATAL_ERROR "generating texts_gen failed:\n${_txt_out}")
    endif()
    message(STATUS "texts: ${_txt_out}")
else()
    message(WARNING "Python non trovato: main/profile.h non viene rigenerato")
endif()

# --- versione dalla build, mai scritta a mano -------------------------------
add_custom_target(versione ALL
    COMMAND ${CMAKE_COMMAND}
            -DRADICE=${CMAKE_CURRENT_LIST_DIR}
            -DUSCITA=${CMAKE_BINARY_DIR}/generato/versione.h
            -P ${CMAKE_CURRENT_LIST_DIR}/tools/versione.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/generato/versione.h
    COMMENT "versione da git describe")

# --- LVGL 9 -----------------------------------------------------------------
set(LVGL_TAG v9.5.0 CACHE STRING "versione di LVGL")

include(FetchContent)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG ${LVGL_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)

# La configurazione di LVGL sta in sim/, non alla radice: alla radice ci
# starebbe scomoda e sembrerebbe roba nostra invece che della libreria.
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/sim/lv_conf.h CACHE FILEPATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)

# --- cJSON ------------------------------------------------------------------
# La stessa libreria che ESP-IDF porta come componente, cosi il codice della
# configurazione e uno solo per il PC e per il pannello.
set(CJSON_TAG v1.7.18 CACHE STRING "versione di cJSON")
FetchContent_Declare(cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG ${CJSON_TAG}
    GIT_SHALLOW TRUE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cjson)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# --- simulatore -------------------------------------------------------------
# CONFIGURE_DEPENDS: un file nuovo entra nella build senza riconfigurare a mano
# --- il cifrato, se questo PC ce l'ha ---------------------------------------
#
# Sul pannello TLS c'e sempre: esp-tls fa parte di ESP-IDF. Sul PC dipende da
# cosa e installato, e OpenSSL non si pretende — il simulatore deve
# compilare anche su una macchina spoglia, perche le ventitre prove non ne
# hanno bisogno: parlano con tools/finto_ha.py, che e in chiaro.
#
# Dove OpenSSL non c'e, chiedere un canale cifrato **fallisce dicendolo**.
# Il ripiego silenzioso in chiaro sarebbe la cosa peggiore: chi prova il
# simulatore contro un Home Assistant vero vedrebbe "non si collega" e
# cercherebbe il guasto nella rete, mentre il pannello funziona. Due bersagli
# che si comportano diverso senza dirlo sono peggio di uno che manca.
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL trovato: il simulatore parla anche cifrato")
else()
    message(STATUS "OpenSSL assente: nel simulatore il cifrato non c'e")
endif()

function(pannello_cifrato bersaglio)
    if(OpenSSL_FOUND)
        target_compile_definitions(${bersaglio} PRIVATE PANNELLO_CON_OPENSSL=1)
        target_link_libraries(${bersaglio} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
endfunction()

file(GLOB SRC_SIM CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/sim/*.c)

add_executable(pannello ${SRC_SIM} ${PANNELLO_SRC_COMUNI})
add_dependencies(pannello versione)
target_include_directories(pannello PRIVATE
    ${PANNELLO_INCLUDE_COMUNI}
    ${CMAKE_CURRENT_LIST_DIR}/sim
    ${CMAKE_BINARY_DIR}/generato
    ${cjson_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS})
target_compile_definitions(pannello PRIVATE
    PROFILO_TUTTI          # nel simulatore ci sono tutti i profili
    SIMULATORE
    SIM_CONFIG_ESEMPIO="${CMAKE_CURRENT_LIST_DIR}/docs/04-config.example.json")
target_compile_options(pannello PRIVATE
    -Wall -Wextra -Wno-unused-parameter ${SDL2_CFLAGS_OTHER})
target_link_libraries(pannello PRIVATE lvgl cjson ${SDL2_LIBRARIES} m)

# --- prove ------------------------------------------------------------------
enable_testing()

add_executable(prova_profilo
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_profilo.c
    ${PANNELLO_SRC_PROFILO})
target_include_directories(prova_profilo PRIVATE ${PANNELLO_MAIN_DIR})
target_compile_definitions(prova_profilo PRIVATE PROFILO_TUTTI)
target_compile_options(prova_profilo PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME profilo COMMAND prova_profilo)

# 11-collaudo.md §1: l'heap dopo una lunga navigazione e quello iniziale.
# I due orientamenti coprono tutti i rami del layout, e ognuno costa meno di
# un secondo.
foreach(P p4-1280x800 p4-800x1280)
    add_test(NAME heap_${P} COMMAND pannello --profilo ${P} --prova-heap 20)
endforeach()

# 11-collaudo.md §1, Fase 3: le tre soglie del collegamento. Un profilo
# basta: la decisione non dipende dal vetro.
add_test(NAME sorveglianza_collegamento
         COMMAND pannello --profilo p4-800x1280 --prova-sorveglianza)

# 11-collaudo.md §1: le temporizzazioni. Il tempo lo fa scorrere la prova,
# quindi dieci minuti di attesa costano qualche millisecondo.
foreach(P p4-1280x800 p4-800x1280)
    add_test(NAME tempi_${P} COMMAND pannello --profilo ${P} --prova-tempi)
endforeach()

# La regola del progetto, verificata a ogni ctest: nessun numero di layout
# fuori da profile.h. Un criterio di 11-collaudo.md §1 che altrimenti si
# controllerebbe a occhio, cioe mai.
if(Python3_FOUND)
    add_test(NAME numeri_di_layout
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/verifica_numeri.py)
    add_test(NAME pagina_allineata
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/genera_pagina.py --verifica)
    # I fusi che la pagina propone devono essere quelli che il pannello sa
    # tradurre. Uno in piu nello schema, senza rigenerare la tabella, e un
    # fuso scegliibile che non funziona — e non funziona **in silenzio**.
    add_test(NAME fusi_allineati
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/genera_fusi.py --verifica)
    add_test(NAME profilo_allineato
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/genera_profilo.py --verifica)
    add_test(NAME misure_dai_mockup
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/estrai_misure.py)
    # I commenti rimandano ai documenti come fonte delle decisioni, e quel
    # rimando vale finche la sezione esiste. Una citazione a vuoto e gia un
    # problema; una che punta alla sezione **sbagliata** e peggio, perche
    # sembra giusta: il robot ha citato per un mese «Primo avvio», e niente
    # lo controllava.
    add_test(NAME citazioni_ai_documenti
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/verifica_citazioni.py)
    # Una chiave nuova nello schema non compare nella pagina finche qualcuno
    # non la aggiunge a una sezione, e non lo dice nessuno: si vede solo dal
    # muro, dove quella funzione non si accende mai. E successo con gli
    # elettrodomestici, con tutto il resto gia pronto.
    add_test(NAME pagina_copre_lo_schema
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/verifica_copertura_pagina.py)
    # Un campo facoltativo si scrive anyOf con un ramo null, e cosi il tipo
    # smette di stare al primo livello: la pagina non lo vedeva piu e metteva
    # un campo di testo davanti a un intero, salvando "1000" con le
    # virgolette. Il pannello ripiegava su zero e la barretta del consumo non
    # si disegnava — senza un errore da nessuna parte.
    # The page has no text of its own: it looks every one up by key in the
    # "web" section of i18n/. A key it asks for and English does not have
    # shows as the key itself, on a page that otherwise works.
    add_test(NAME page_texts
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/check_page_texts.py)
    add_test(NAME pagina_conosce_i_tipi
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/verifica_pagina_tipi.py)
    # Le icone che si possono scegliere per un interruttore vivono in tre
    # posti — il generatore, lo schema, il validatore — e se divergono la
    # pagina offre un nome che sul vetro diventa un rettangolo vuoto.
    # A configuration path the code reads and the schema does not have is
    # not an error anywhere: the accessor returns its fallback and the
    # setting changed on the page never arrives. Written when every key
    # went from Italian to English, to find the reads that were missed.
    add_test(NAME config_paths_in_schema
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/check_config_paths.py)
    add_test(NAME icone_scegliibili
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/verifica_icone_scelta.py)
endif()

# Fase 2: la configurazione. Non serve LVGL, quindi la prova e un eseguibile
# a se, veloce, che gira anche dove non c'e SDL.
add_executable(prova_config
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_config.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/archivio_posix.c
    ${PANNELLO_MAIN_DIR}/cfg/config.c
    ${PANNELLO_MAIN_DIR}/cfg/validazione.c
    # La validazione confronta il `profilo` scritto nel documento con quello
    # del pannello: senza un pannello non c'e confronto. Qui vale quello
    # dell'esempio, che e la casa vera.
    ${PANNELLO_MAIN_DIR}/profile_def.c)
target_compile_definitions(prova_config PRIVATE PANNELLO_PROFILO=PRF_P4_800X1280)
target_include_directories(prova_config PRIVATE
    ${PANNELLO_MAIN_DIR} ${PANNELLO_MAIN_DIR}/cfg ${cjson_SOURCE_DIR})
target_link_libraries(prova_config PRIVATE cjson)
target_compile_options(prova_config PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME configurazione COMMAND prova_config)
set_tests_properties(configurazione PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Il primo avvio in un processo **suo**: il documento di configurazione e una
# variabile statica, e le prove che vengono prima ne lasciano uno buono. Un
# pannello appena montato al muro parte invece col vuoto, e quella condizione
# dentro un processo gia usato non si ricrea.
add_test(NAME primo_avvio COMMAND prova_config --primo-avvio)
set_tests_properties(primo_avvio PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Fase 3: il client di Home Assistant. Parla WebSocket davvero, con il
# finto server di tools/finto_ha.py: un protocollo si prova parlandolo.
add_executable(prova_ha
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_ha.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/archivio_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/canale_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/sistema_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/magazzino_posix.c
    ${PANNELLO_MAIN_DIR}/cfg/config.c
    ${PANNELLO_MAIN_DIR}/cfg/segreti.c
    ${PANNELLO_MAIN_DIR}/cfg/validazione.c
    ${PANNELLO_SRC_DATI}
    ${PANNELLO_SRC_PROFILO}
    ${PANNELLO_SRC_HA}
    ${PANNELLO_SRC_RETE})
target_compile_definitions(prova_ha PRIVATE PROFILO_TUTTI)
target_include_directories(prova_ha PRIVATE
    ${PANNELLO_INCLUDE_COMUNI} ${cjson_SOURCE_DIR})
target_link_libraries(prova_ha PRIVATE cjson m)
target_compile_options(prova_ha PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(Python3_FOUND)
    add_test(NAME home_assistant
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/prova_ha.py
                     --binario $<TARGET_FILE:prova_ha>)
    set_tests_properties(home_assistant PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} TIMEOUT 300)
endif()

# La palette. Due cose: che senza configurazione la tabella contenga
# esattamente i colori scelti quando il pannello e stato disegnato, e che
# con un accento diverso gli stati composti lo seguano ruotando la tinta
# invece di mescolarsi. La prima e l'unica che tiene ferma la promessa
# scritta in theme.h.
add_executable(prova_tema
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_tema.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/archivio_posix.c
    ${PANNELLO_MAIN_DIR}/cfg/config.c
    ${PANNELLO_MAIN_DIR}/cfg/validazione.c
    ${PANNELLO_MAIN_DIR}/ui/tema.c
    ${PANNELLO_SRC_PROFILO})
target_include_directories(prova_tema PRIVATE
    ${PANNELLO_INCLUDE_COMUNI} ${cjson_SOURCE_DIR})
target_compile_definitions(prova_tema PRIVATE PROFILO_TUTTI)
# Di LVGL serve solo il tipo del colore, non la libreria: vedi tema.c.
target_link_libraries(prova_tema PRIVATE cjson m)
target_compile_options(prova_tema PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME palette COMMAND prova_tema)
set_tests_properties(palette PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Un colore predefinito e scritto in tre posti: la tabella del firmware, il
# `default` dello schema — che e cio che la pagina mostra — e la tabella del
# documento, che i mockup rendono. Se il secondo si allontanasse dal primo,
# la pagina mostrerebbe un colore e il pannello ne userebbe un altro, senza
# un errore da nessuna parte.
if(Python3_FOUND)
    add_test(NAME palette_nei_tre_posti
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/verifica_palette.py)
    set_tests_properties(palette_nei_tre_posti PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
endif()

# La conversione in ore. Due file e nessuna dipendenza: e' esattamente
# per poterla provare che sta in un pezzo suo invece che dentro
# dati_robot.c, dove sarebbe stata una funzione statica in mezzo a cose che
# vogliono la configurazione e Home Assistant.
add_executable(prova_durata
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_durata.c
    ${PANNELLO_MAIN_DIR}/app/durata.c)
target_include_directories(prova_durata PRIVATE ${PANNELLO_MAIN_DIR}/app)
target_compile_options(prova_durata PRIVATE -Wall -Wextra)
add_test(NAME durata_in_ore COMMAND prova_durata)

# Languages. Two tests, because they fail for different reasons.
#
# texts_complete reads the JSON: every language has every key, the same
# printf placeholders in the same order as English, and only characters
# the fonts can draw. A placeholder swapped in a translation is not a
# typo — it is a string passed where a number is expected, on one screen,
# in one language.
#
# i18n runs the C side: plural rules, the fallback to English, decimal
# separators. It is the part that decides which of the right words shows.
if(Python3_FOUND)
    add_test(NAME texts_complete
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/gen_texts.py --check)
endif()
add_executable(test_i18n
    ${CMAKE_CURRENT_LIST_DIR}/test/test_i18n.c
    ${PANNELLO_MAIN_DIR}/i18n/i18n.c
    ${PANNELLO_MAIN_DIR}/i18n/texts_gen.c)
target_include_directories(test_i18n PRIVATE ${PANNELLO_MAIN_DIR}/i18n)
target_compile_options(test_i18n PRIVATE -Wall -Wextra)
add_test(NAME i18n COMMAND test_i18n)

# The translations edited from the page: saved, checked, applied, exported.
# In the source tree, like the configuration tests: the export of English
# is compared with i18n/en.json itself.
add_executable(test_translations
    ${CMAKE_CURRENT_LIST_DIR}/test/test_translations.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/archivio_posix.c
    ${PANNELLO_MAIN_DIR}/cfg/translations.c
    ${PANNELLO_MAIN_DIR}/i18n/i18n.c
    ${PANNELLO_MAIN_DIR}/i18n/texts_gen.c)
target_include_directories(test_translations PRIVATE
    ${PANNELLO_MAIN_DIR}/i18n ${PANNELLO_MAIN_DIR}/cfg ${cjson_SOURCE_DIR})
target_link_libraries(test_translations PRIVATE cjson)
target_compile_options(test_translations PRIVATE -Wall -Wextra)
add_test(NAME translations COMMAND test_translations)
set_tests_properties(translations PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Fase 2: il server di configurazione. Le regole di accesso si provano
# chiamando web_servi(), non aprendo un socket: quello che va verificato e
# chi puo cosa, e un socket di mezzo aggiunge solo modi di fallire.
add_executable(prova_web
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_web.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/archivio_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/canale_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/sistema_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/magazzino_posix.c
    # Il trasporto c'e anche se la prova non apre nessun socket: web_avvia()
    # e web_ferma() lo chiamano, e il collegatore vuole trovarlo.
    ${PANNELLO_MAIN_DIR}/cfg/config.c
    ${PANNELLO_MAIN_DIR}/cfg/segreti.c
    ${PANNELLO_MAIN_DIR}/cfg/validazione.c
    ${PANNELLO_MAIN_DIR}/cfg/ripristino.c
    ${PANNELLO_MAIN_DIR}/cfg/translations.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/aggiornamento_posix.c
    ${PANNELLO_MAIN_DIR}/web/web.c
    # I fornitori di dati, non sezioni.c: quello disegna e vuole LVGL.
    ${PANNELLO_SRC_DATI}
    ${PANNELLO_SRC_RETE}
    # I fornitori leggono lo stato dalle entita: il collegatore vuole
    # trovare anche quelle, col loro trasporto.
    ${PANNELLO_SRC_HA}
    ${PANNELLO_SRC_PROFILO})
add_dependencies(prova_web versione)
target_include_directories(prova_web PRIVATE
    ${PANNELLO_INCLUDE_COMUNI} ${CMAKE_BINARY_DIR}/generato ${cjson_SOURCE_DIR})
target_compile_definitions(prova_web PRIVATE PROFILO_TUTTI)
target_link_libraries(prova_web PRIVATE cjson m)
target_compile_options(prova_web PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME server_web COMMAND prova_web)
set_tests_properties(server_web PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Fase 2: la struttura dell'interfaccia viene da config.json. Undici
# configurazioni diverse fra loro, e i conteggi devono seguire il file.
# Fase 2: i segreti. Non serve LVGL neanche a questa.
add_executable(prova_segreti
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_segreti.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/archivio_posix.c
    ${CMAKE_CURRENT_LIST_DIR}/sim/magazzino_posix.c
    ${PANNELLO_MAIN_DIR}/cfg/config.c
    ${PANNELLO_MAIN_DIR}/cfg/segreti.c
    ${PANNELLO_MAIN_DIR}/cfg/validazione.c
    ${PANNELLO_MAIN_DIR}/cfg/ripristino.c
    ${PANNELLO_MAIN_DIR}/app/dati_finti_agenda.c
    # The demo calendar speaks through tr(): its day headings and the
    # default name of a shared network are interface texts.
    ${PANNELLO_MAIN_DIR}/i18n/i18n.c
    ${PANNELLO_MAIN_DIR}/i18n/texts_gen.c
    # La validazione confronta il `profilo` scritto nel documento con quello
    # del pannello: senza un pannello non c'e confronto.
    ${PANNELLO_MAIN_DIR}/profile_def.c)
target_compile_definitions(prova_segreti PRIVATE PANNELLO_PROFILO=PRF_P4_800X1280)
target_include_directories(prova_segreti PRIVATE
    ${PANNELLO_MAIN_DIR} ${PANNELLO_MAIN_DIR}/cfg ${PANNELLO_MAIN_DIR}/app
    ${PANNELLO_MAIN_DIR}/i18n
    ${cjson_SOURCE_DIR})
target_link_libraries(prova_segreti PRIVATE cjson)
target_compile_options(prova_segreti PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME segreti COMMAND prova_segreti)

# La coda dei tocchi: pura logica, nessun hardware, e per questo si puo
# provare. Sul vetro un tocco perso si vede come "ha risposto al secondo" e
# da li non si risale a niente.
add_executable(prova_tocco_eventi
    ${CMAKE_CURRENT_LIST_DIR}/test/prova_tocco_eventi.c
    ${PANNELLO_MAIN_DIR}/app/tocco_eventi.c)
target_include_directories(prova_tocco_eventi PRIVATE ${PANNELLO_MAIN_DIR}/app)
target_compile_options(prova_tocco_eventi PRIVATE -Wall -Wextra)
add_test(NAME tocchi_in_coda COMMAND prova_tocco_eventi)
set_tests_properties(segreti PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

if(Python3_FOUND)
    # I comandi devono arrivare dove devono. Quattro volte un tocco si e
    # perso per strada e quattro volte se n'e accorto un dito sul vetro:
    # qui il tocco si inietta davvero e si guarda dove finisce.
    add_test(NAME tocchi_che_arrivano
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/prova_tocco.py
                     --binario $<TARGET_FILE:pannello>)
    set_tests_properties(tocchi_che_arrivano PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

    add_test(NAME struttura_dalla_configurazione
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/prova_struttura.py
                     --binario $<TARGET_FILE:pannello>)
    set_tests_properties(struttura_dalla_configurazione PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

    add_test(NAME validazione_uguale_a_python
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_LIST_DIR}/tools/confronta_validazione.py
                     --binario $<TARGET_FILE:prova_config>)
    set_tests_properties(validazione_uguale_a_python PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
endif()

# Il profilo deve compilare anche da solo, uno per volta, come sul pannello:
# e il modo per accorgersi che un campo esiste solo nel ramo del simulatore.
foreach(P PRF_P4_1280X800 PRF_P4_800X1280)
    string(TOLOWER ${P} _n)
    add_executable(prova_${_n}
        ${CMAKE_CURRENT_LIST_DIR}/test/prova_profilo.c
        ${PANNELLO_SRC_PROFILO})
    target_include_directories(prova_${_n} PRIVATE ${PANNELLO_MAIN_DIR})
    target_compile_definitions(prova_${_n} PRIVATE PANNELLO_PROFILO=${P})
    target_compile_options(prova_${_n} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    add_test(NAME ${_n} COMMAND prova_${_n})
endforeach()

# Il cifrato vale per ogni bersaglio che si porta dentro il canale POSIX.
foreach(_b pannello prova_ha prova_web)
    if(TARGET ${_b})
        pannello_cifrato(${_b})
    endif()
endforeach()

# Le cartelle di appoggio delle prove — prova_dati* — nella cartella di
# compilazione e non nella radice del progetto: le prove girano dalla
# radice perche leggono docs/ e i18n/, e senza questo le lasciavano li.
# In fondo al file, dopo che tutte le prove esistono.
foreach(_prova prova_config prova_ha prova_tema test_translations prova_web prova_segreti)
    if(TARGET ${_prova})
        target_compile_definitions(${_prova} PRIVATE PROVE_DIR="${CMAKE_BINARY_DIR}")
    endif()
endforeach()
