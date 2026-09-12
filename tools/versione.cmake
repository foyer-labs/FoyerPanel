# ---------------------------------------------------------------------------
# Scrive versione.h leggendo git.
#
# La versione mostrata all'avvio e in /api/status deve corrispondere al tag
# piu recente, con "+dev" quando ci sono commit successivi. Ricavarla dalla
# build e l'unico modo perche resti vera: un #define scritto a mano lo si
# dimentica di aggiornare esattamente la volta che conta.
#
#     cmake -DRADICE=... -DUSCITA=... -P tools/versione.cmake
# ---------------------------------------------------------------------------

find_package(Git QUIET)

set(VERSIONE "0.0-sconosciuta")
set(REVISIONE "")

if(GIT_FOUND AND EXISTS ${RADICE}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${RADICE}
        OUTPUT_VARIABLE TAG OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _e)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${RADICE}
        OUTPUT_VARIABLE REVISIONE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${RADICE}
        OUTPUT_VARIABLE SPORCO ERROR_QUIET)

    if(_e EQUAL 0 AND TAG)
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-list ${TAG}..HEAD --count
            WORKING_DIRECTORY ${RADICE}
            OUTPUT_VARIABLE DOPO OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        # Quanti commit dopo il tag, e non un generico "+dev": con un tag
        # fermo da mesi "+dev" resta identico a ogni compilazione, e chi la
        # legge conclude che il pannello non si aggiorna. Il numero cresce da
        # solo, ed e la differenza fra una versione e un'etichetta.
        if(DOPO GREATER 0)
            set(VERSIONE "${TAG}+${DOPO}")
        else()
            set(VERSIONE "${TAG}")
        endif()
    else()
        # nessun tag ancora: siamo prima di v0.1
        set(VERSIONE "0.0+dev")
    endif()

    if(NOT SPORCO STREQUAL "")
        set(VERSIONE "${VERSIONE}-modificata")
    endif()
endif()

# Ora locale, non UTC: la legge chi ha compilato, sul suo orologio, e
# «19:14» risponde alla domanda «e questa la build che ho appena fatto?»
# meglio di un orario spostato di due ore. La sola data non bastava: fra due
# compilazioni dello stesso pomeriggio non distingueva niente, che e
# precisamente quando uno se lo chiede.
#
# Con i minuti dentro, versione.h cambia a ogni compilazione e i pochi file
# che lo includono si ricompilano. Sono quattro, e il prezzo e questo.
string(TIMESTAMP COMPILATO "%Y-%m-%d %H:%M")
string(TIMESTAMP COMPILATO_BREVE "%d/%m %H:%M")

set(NUOVO "/* generato da tools/versione.cmake — non versionato */
#ifndef VERSIONE_H
#define VERSIONE_H
#define PANNELLO_VERSIONE   \"${VERSIONE}\"
#define PANNELLO_REVISIONE  \"${REVISIONE}\"
#define PANNELLO_COMPILATO  \"${COMPILATO}\"
#define PANNELLO_COMPILATO_BREVE \"${COMPILATO_BREVE}\"
#endif
")

set(VECCHIO "")
if(EXISTS ${USCITA})
    file(READ ${USCITA} VECCHIO)
endif()
if(NOT VECCHIO STREQUAL NUOVO)
    file(WRITE ${USCITA} "${NUOVO}")
    message(STATUS "versione: ${VERSIONE} (${REVISIONE})")
endif()
