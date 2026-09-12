/* ------------------------------------------------------------------------
 * Having the graphics task do something — panel only.
 *
 * Configuration, Home Assistant, secrets and LVGL have a single owner: the
 * graphics task, which in the same loop draws, serves the web page and
 * talks to Home Assistant. None of the four has a lock, on purpose — one
 * reader at a time is the rule that keeps them simple.
 *
 * The console, though, runs in a task of its own, and its commands touched
 * exactly those things: `token` closed the WebSocket while the graphics
 * task could be inside ws_gira() reading it, `rete` replaced a text of the
 * configuration while someone else held its pointer. Rare faults, of the
 * worst kind: an unexplained restart, right while someone is trying to put
 * the panel right.
 *
 * Now the console asks, and the graphics task runs it between one loop and
 * the next — where nobody else is touching anything.
 * --------------------------------------------------------------------- */
#ifndef NEL_GRAFICO_H
#define NEL_GRAFICO_H

#include <stdbool.h>
#include <stdint.h>

/* Once, before the graphics task starts. */
void nel_grafico_avvia(void);

/* Has the graphics task run fai(dato) and waits for it to finish.
 *
 * False if within `attesa_ms` the graphics task has not even started it:
 * then it **will never run it**, and the caller decides what to do. The
 * case is real — the console is also the way back when the graphics task
 * is stuck — and waiting forever would take away exactly that way back.
 * If it did start it, the wait lasts until it finishes. */
bool nel_grafico(void (*fai)(void *), void *dato, uint32_t attesa_ms);

/* From the graphics task, once per loop: runs the pending request, if
   there is one. */
void nel_grafico_gira(void);

#endif /* NEL_GRAFICO_H */
