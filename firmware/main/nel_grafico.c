/* ------------------------------------------------------------------------
 * Having the graphics task do something — see nel_grafico.h.
 *
 * One request at a time, in one place, and not a queue: the one asking is
 * the console, which writes a command and waits for the answer before
 * taking another. The turn is there anyway, so that a second caller one day
 * does not find someone else's request half done.
 *
 * The delicate point is giving up. Whoever tires of waiting must be able to
 * withdraw the request **only if nobody has started it yet**, and must know
 * it for certain: if the graphics task takes it at that same instant,
 * `dato` points into the caller's stack, and that stack is about to be
 * reused. So the state changes hands under a short lock, and the graphics
 * task starts only what it still finds waiting.
 * --------------------------------------------------------------------- */
#include "nel_grafico.h"

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum {
    LIBERA,       /* no request                                          */
    IN_ATTESA,    /* written, the graphics task has not seen it yet      */
    IN_CORSO,     /* the graphics task is running it                     */
} fase_t;

static struct {
    void (*fai)(void *);
    void  *dato;
    fase_t fase;
} richiesta;

static portMUX_TYPE      lucchetto = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t finita;   /* given by the graphics task when done */
static SemaphoreHandle_t turno;    /* one caller at a time                 */

void nel_grafico_avvia(void)
{
    if (!finita) finita = xSemaphoreCreateBinary();
    if (!turno)  turno  = xSemaphoreCreateMutex();
}

bool nel_grafico(void (*fai)(void *), void *dato, uint32_t attesa_ms)
{
    if (!fai || !finita || !turno) return false;

    xSemaphoreTake(turno, portMAX_DELAY);

    taskENTER_CRITICAL(&lucchetto);
    richiesta.fai  = fai;
    richiesta.dato = dato;
    richiesta.fase = IN_ATTESA;
    taskEXIT_CRITICAL(&lucchetto);

    bool fatta = xSemaphoreTake(finita, pdMS_TO_TICKS(attesa_ms)) == pdTRUE;

    if (!fatta) {
        /* Only what nobody started is withdrawn. */
        taskENTER_CRITICAL(&lucchetto);
        const bool ritirata = richiesta.fase == IN_ATTESA;
        if (ritirata) richiesta.fase = LIBERA;
        taskEXIT_CRITICAL(&lucchetto);

        /* Started — or just finished, between the timeout and the lock: in
           both cases the graphics task will give the semaphore, and it must
           be taken, or the next request would find it already given and
           return before being run. */
        if (!ritirata) {
            xSemaphoreTake(finita, portMAX_DELAY);
            fatta = true;
        }
    }

    xSemaphoreGive(turno);
    return fatta;
}

void nel_grafico_gira(void)
{
    taskENTER_CRITICAL(&lucchetto);
    const bool tocca = richiesta.fase == IN_ATTESA;
    if (tocca) richiesta.fase = IN_CORSO;
    taskEXIT_CRITICAL(&lucchetto);

    if (!tocca) return;

    richiesta.fai(richiesta.dato);

    taskENTER_CRITICAL(&lucchetto);
    richiesta.fase = LIBERA;
    taskEXIT_CRITICAL(&lucchetto);
    xSemaphoreGive(finita);
}
