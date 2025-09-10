/*
   IntelliSense shim for Open Watcom headers

   Purpose: Help VS Code's IntelliSense parse code that includes Watcom
   headers using non-standard extensions (e.g., :> in MK_FP, __near/__far).

   Usage: Add this file to c_cpp_properties.json "forcedInclude" so it is
   included only for IntelliSense. Real builds should NOT see this file.
*/

#ifdef __INTELLISENSE__

/* Neutralize common Watcom-specific keywords/attributes so IntelliSense parses */
#ifndef __near
#define __near
#endif
#ifndef __far
#define __far
#endif
#ifndef __interrupt
#define __interrupt
#endif
#ifndef __loadds
#define __loadds
#endif
#ifndef __saveregs
#define __saveregs
#endif
#ifndef __pascal
#define __pascal
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __based
#define __based(x)
#endif
#ifndef _WCRTLINK
#define _WCRTLINK
#endif
#ifndef _WCIRTLINK
#define _WCIRTLINK
#endif

/* Prevent parsing of c:\WATCOM\h\i86.h which contains nonstandard tokens */
#ifndef _I86_H_INCLUDED
#define _I86_H_INCLUDED

/* Provide minimal replacements typically needed from <i86.h> */
#include <stdint.h>

/* --- Minimal register structures and union --- */
struct BYTEREGS {
    unsigned char al, ah;
    unsigned char bl, bh;
    unsigned char cl, ch;
    unsigned char dl, dh;
};

struct WORDREGS {
    unsigned short ax; unsigned short _f1;
    unsigned short bx; unsigned short _f2;
    unsigned short cx; unsigned short _f3;
    unsigned short dx; unsigned short _f4;
    unsigned short si; unsigned short _f5;
    unsigned short di; unsigned short _f6;
    unsigned int   cflag;
};

union REGS {
    struct WORDREGS  w;
    struct BYTEREGS  h;
};
#define _REGS REGS

struct SREGS {
    unsigned short es, cs, ss, ds;
    unsigned short fs, gs;
};
#define _SREGS SREGS

#ifndef FP_OFF
#define FP_OFF(__p) ((unsigned)(uintptr_t)(__p))
#endif
#ifndef _FP_OFF
#define _FP_OFF(__p) FP_OFF(__p)
#endif

#ifndef FP_SEG
#define FP_SEG(__p) ((unsigned)((uintptr_t)(__p) >> 16))
#endif
#ifndef _FP_SEG
#define _FP_SEG FP_SEG
#endif

#ifndef MK_FP
#define MK_FP(__s,__o) \
    ((void *)( (uintptr_t)( ((unsigned long)(unsigned short)(__s) << 16) \
                           | (unsigned short)(__o) ) ))
#endif

/* --- Minimal function prototypes --- */
int int86(int, union REGS *, union REGS *);
int int86x(int, union REGS *, union REGS *, struct SREGS *);

/* --- Minimal REGPACK and intr() for INT 10h/16h peeks --- */
struct REGPACKW {
    unsigned short ax; unsigned short _f1;
    unsigned short bx; unsigned short _f2;
    unsigned short cx; unsigned short _f3;
    unsigned short dx; unsigned short _f4;
    unsigned short bp; unsigned short _f5;
    unsigned short si; unsigned short _f6;
    unsigned short di; unsigned short _f7;
    unsigned short ds;
    unsigned short es;
    unsigned int   flags;
};
union REGPACK {
    struct REGPACKW w;
};

/* INTR_* flag bits (matches Watcom i86.h) */
#ifndef INTR_ZF
#define INTR_CF     0x0001
#define INTR_PF     0x0004
#define INTR_AF     0x0010
#define INTR_ZF     0x0040
#define INTR_SF     0x0080
#define INTR_TF     0x0100
#define INTR_IF     0x0200
#define INTR_DF     0x0400
#define INTR_OF     0x0800
#endif

void intr(int, union REGPACK *);

#endif /* _I86_H_INCLUDED */

#endif /* __INTELLISENSE__ */
