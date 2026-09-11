/* Diagnostic-only boot tracer: sprinkled directly into App/main.c's real
 * Main() so the exact production boot sequence and scheduler loop run
 * unmodified, with brief on-screen checkpoints. Unlike memtest.c (which
 * reproduces the logic separately), this instruments the real code path -
 * same binary, same call depth, same timing - to rule those out as the
 * difference between a synthetic reproduction and the real failure.
 *
 * A no-op when ENABLE_BOOT_TRACE isn't defined, so call sites in main.c
 * don't need to be wrapped in #ifdef. */
#ifndef BOOT_TRACE_H
#define BOOT_TRACE_H

void BOOT_TRACE_Step(const char *step);
void BOOT_TRACE_Heartbeat(void);

#endif
