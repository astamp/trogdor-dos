#ifndef DEBUG_F_H
#define DEBUG_F_H

#define MAX_DEBUG_LINE (80)

void DEBUG_init();
void DEBUG_fini();

/* Outputs a debugging message. */
void DEBUG_printf(const char* format, ...);

#define debugf DEBUG_printf

#endif // DEBUG_F_H
