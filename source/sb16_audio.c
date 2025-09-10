#include <dos.h>
#include <conio.h>
#include <i86.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include "sb16_audio.h"
#include "dbg_log.h"

/* ---- Configurable defaults ---- */
#define SB16_DEFAULT_SRATE     11025u
#define SB16_DEFAULT_MASTER    220u
#define SB16_DEFAULT_PCM       220u

/* ---- BLASTER and SB16 DSP ports ---- */
static unsigned g_sb_base = 0x220;
static int      g_sb_irq  = 7;       /* IRQ Channel */
static int      g_sb_dma8 = 1;       /* 8-bit DMA channel */

#define SB16_PORT_RESET(base)   ((base) + 0x06)
#define SB16_PORT_READ(base)    ((base) + 0x0A)
#define SB16_PORT_WRITE(base)   ((base) + 0x0C)
#define SB16_PORT_STATUS(base)  ((base) + 0x0E)
#define SB16_PORT_MIXER_ADDR(b) ((b) + 0x04)
#define SB16_PORT_MIXER_DATA(b) ((b) + 0x05)

/* ---- SB16 DSP status bits (read at STATUS) ---- */
/* bit7 (0x80): 1 = read buffer has data available; also means write port is busy */
#define DSP_STATUS_RX_READY     0x80

/* ---- DMA controller 0 (8-bit channels 0..3) ---- */
#define DMA0_MASK_PORT      0x0A
#define DMA0_MODE_PORT      0x0B
#define DMA0_CLEARFF_PORT   0x0C
#define DMA0_STATUS_PORT    0x08   /* read: TC bits for ch0..3; reading clears TC bits */

static const unsigned char dma0_addr_port[4]  = {0x00,0x02,0x04,0x06};
static const unsigned char dma0_count_port[4] = {0x01,0x03,0x05,0x07};
static const unsigned char dma0_page_port[4]  = {0x87,0x83,0x81,0x82};

/* 8237 mode presets used here (OR with channel number 0..3) */
/* 0x48 = single transfer, address increment, memory->device (playback) */
#define DMA0_MODE_SINGLE_INC_MEM2DEV  0x48
/* Mask/unmask: write 0x04|ch to mask, write ch to unmask */
#define DMA0_MASK_SET_BIT             0x04

/* ---- Mixer registers (SB16) ---- */
#define MIXER_REG_MASTER_L   0x30
#define MIXER_REG_MASTER_R   0x31
#define MIXER_REG_PCM_L      0x32
#define MIXER_REG_PCM_R      0x33

/* ---- PIC interrupt vector bases (real-mode DOS defaults) ---- */
#define PIC1_VEC_BASE         0x08   /* IRQ0..7 map to int 0x08..0x0F */
#define PIC2_VEC_BASE         0x70   /* IRQ8..15 map to int 0x70..0x77 */
#define PIC1_IRQ_COUNT        8

/* ---- PIC I/O ports and commands ---- */
#define PIC1_CMD_PORT         0x20   /* Master PIC command port */
#define PIC2_CMD_PORT         0xA0   /* Slave PIC command port */
#define PIC_EOI_COMMAND       0x20   /* Non-specific End Of Interrupt */

/* ---- Misc helpers ---- */
#define DSP_RESET_ACK         0xAA   /* DSP returns 0xAA after reset */
#define ALIGN_64K_MASK        0xFFFFUL
#define DMA_ALIGN_SLOP        16UL   /* spare bytes to reach next paragraph */

/* ---- DSP commands ---- */
#define DSP_CMD_SPKR_ON      0xD1
#define DSP_CMD_SPKR_OFF     0xD3
#define DSP_CMD_RATE         0x41  /* set output sample rate: hi, lo */
#define DSP_CMD_8BIT_SINGLE  0x14  /* 8-bit, single-cycle: length-1 lo,hi */
#define DSP_CMD_HALT_DMA     0xD0

/* ---- State ---- */
static void (__interrupt __far *g_old_isr)(void) = 0;
static volatile int     g_playing = 0;
static unsigned         g_engine_rate = SB16_DEFAULT_SRATE;
static unsigned char    g_prev_pcm_l = 0, g_prev_pcm_r = 0;
static volatile int     g_have_prev_pcm = 0;

/* DMA buffer: 64K-aligned, up to 64K length */
static unsigned char __far *g_dma_raw = 0;
static unsigned char __far *g_dma_buf = 0;
static unsigned              g_dma_len = 0;

/* Registered waves */
typedef struct {
    unsigned char __far *data;  /* PCM u8 */
    unsigned              len;   /* <= 65535 */
    unsigned              rate;  /* Hz */
    int                   used;
} Wave;

static Wave g_waves[SB16_MAX_WAVES];

/* BIOS tick at 0x40:0x6C (~18.2 Hz). Useful for coarse timestamps. */
static unsigned long bios_ticks(void)
{
    return *(volatile unsigned long __far*)MK_FP(0x40, 0x6C);
}

/* ---- Helpers ---- */
static void mixer_write(unsigned char reg, unsigned char val) {
    outp(SB16_PORT_MIXER_ADDR(g_sb_base), reg);
    outp(SB16_PORT_MIXER_DATA(g_sb_base), val);
}

static unsigned char mixer_read(unsigned char reg) {
    outp(SB16_PORT_MIXER_ADDR(g_sb_base), reg);
    return (unsigned char)inp(SB16_PORT_MIXER_DATA(g_sb_base));
}

static void set_pcm_volume(unsigned char v) {
    mixer_write(MIXER_REG_PCM_L, v);
    mixer_write(MIXER_REG_PCM_R, v);
}

static void set_master_volume(unsigned char v) {
    mixer_write(MIXER_REG_MASTER_L, v);
    mixer_write(MIXER_REG_MASTER_R, v);
}

/* Configure SB16 mixer IRQ/DMA to match BLASTER values */
static void sb16_config_irq_dma(void) {
    unsigned char irq_bits = 0;
    unsigned char dma_bits = 0;
    switch (g_sb_irq) {
        case 2:  irq_bits = 0x01; break;
        case 5:  irq_bits = 0x02; break;
        case 7:  irq_bits = 0x04; break;
        case 10: irq_bits = 0x08; break;
        default: irq_bits = 0x04;  break; /* default to IRQ7 */
    }
    switch (g_sb_dma8) {
        case 0: dma_bits |= 0x01; break;
        case 1: dma_bits |= 0x02; break;
        case 3: dma_bits |= 0x08; break;
        default: dma_bits |= 0x02; break; /* default to DMA1 */
    }
    /* Mixer registers: 0x80 = IRQ select, 0x81 = DMA select */
    mixer_write(0x80, irq_bits);
    mixer_write(0x81, dma_bits);
    dbg_log_printf("SB16", "mixer: IRQsel=%02X DMA sel=%02X", irq_bits, dma_bits);
}

/* Unmask the PIC line for our IRQ */
static void pic_unmask_irq(int irq) {
    if (irq < PIC1_IRQ_COUNT) {
        unsigned char imr = (unsigned char)inp(0x21);
        imr &= (unsigned char)~(1u << irq);
        outp(0x21, imr);
    } else {
        unsigned char imr2 = (unsigned char)inp(0xA1);
        imr2 &= (unsigned char)~(1u << (irq - PIC1_IRQ_COUNT));
        outp(0xA1, imr2);
    }
}

static void parse_blaster(void) {
    const char *env = getenv("BLASTER");
    if (!env) return;
    while (*env) {
        if ((*env=='A'||*env=='a') && env[1]) g_sb_base = (unsigned)strtoul(env+1,0,16);
        if ((*env=='I'||*env=='i') && env[1]) g_sb_irq  = atoi(env+1);
        if ((*env=='D'||*env=='d') && env[1]) g_sb_dma8 = atoi(env+1);
        while (*env && *env!=' ') ++env;
        while (*env==' ') ++env;
    }
    /* Sanitize parsed values to safe defaults/ranges */
    {
        unsigned old_base = g_sb_base; int old_irq = g_sb_irq; int old_dma = g_sb_dma8;
        /* base should be 16-byte aligned and in a typical SB range */
        if ((g_sb_base & 0xF) != 0 || g_sb_base < 0x200 || g_sb_base > 0x280) g_sb_base = 0x220;
        /* typical SB16 IRQs */
        if (!(g_sb_irq == 5 || g_sb_irq == 7 || g_sb_irq == 10)) g_sb_irq = 7;
        /* 8-bit DMA channels usable for PCM: 0,1,3 (avoid 2/floppy) */
        if (!(g_sb_dma8 == 0 || g_sb_dma8 == 1 || g_sb_dma8 == 3)) g_sb_dma8 = 1;
        if (old_base != g_sb_base || old_irq != g_sb_irq || old_dma != g_sb_dma8) {
            dbg_log_printf("SB16", "BLASTER sanitized: base=%03X irq=%d dma8=%d", g_sb_base, g_sb_irq, g_sb_dma8);
        }
    }
}

static int dsp_wait_write(void) {
    /* Wait for write-ready: STATUS bit7 must be 0 to accept a byte */
    unsigned long t=100000UL; /* iteration guard; not time-accurate */
    while (t-- && (inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) { }
    return (inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY) ? -1 : 0;
}

static int dsp_write(unsigned char v) {
    if (dsp_wait_write()!=0) { dbg_log_printf("SB16", "dsp_write timeout: v=%02X status=%02X", v, inp(SB16_PORT_STATUS(g_sb_base))); return -1; }
    dbg_log_printf("SB16", "dsp_write: v=%02X", v);
    outp(SB16_PORT_WRITE(g_sb_base), v);
    return 0;
}

static int dsp_reset(void) {
    unsigned long t=100000UL; /* guard; 0xAA expected within a few ms */
    outp(SB16_PORT_RESET(g_sb_base), 1);
    delay(3); /* ~3 ms reset pulse */
    outp(SB16_PORT_RESET(g_sb_base), 0);
    /* Wait for data-ready (bit7=1), then expect 0xAA on READ */
    while (t-- && !(inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) { }
    if (!(inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) { dbg_log_printf("SB16", "dsp_reset: RX not ready"); return -1; }
    {
        int ok = (inp(SB16_PORT_READ(g_sb_base)) == DSP_RESET_ACK);
        if (!ok) dbg_log_printf("SB16", "dsp_reset: bad ack");
        return ok ? 0 : -1;
    }
}

static void dsp_speaker_on(void)  { dsp_write(DSP_CMD_SPKR_ON); }
static void dsp_speaker_off(void) { dsp_write(DSP_CMD_SPKR_OFF); }

static void dsp_set_rate(unsigned hz) {
    dsp_write(DSP_CMD_RATE);
    dsp_write((hz >> 8) & 0xFF);
    dsp_write(hz & 0xFF);
}

/* Map IRQ line to IVT vector number using default PIC bases. */
static int irq_to_vector(int irq) {
    return (irq < PIC1_IRQ_COUNT)
        ? (irq + PIC1_VEC_BASE)
        : (PIC2_VEC_BASE + (irq - PIC1_IRQ_COUNT));
}

/* IRQ handler: ACK + clear playing flag */
static void __interrupt __far sb16_irq_isr(void) {
    /* Minimal ACK for 8-bit SB16 DMA IRQ */
    (void)inp(SB16_PORT_READ(g_sb_base)); /* read once to ACK */
    /* Clear SB16 mixer IRQ status latch (0x82) */
    outp(SB16_PORT_MIXER_ADDR(g_sb_base), 0x82);
    (void)inp(SB16_PORT_MIXER_DATA(g_sb_base));
    /* Stop DAC output to avoid tail noise */
    dsp_speaker_off();
    /* Mark done; defer volume restore outside ISR */
    g_playing = 0;
    if (g_sb_irq >= PIC1_IRQ_COUNT) outp(PIC2_CMD_PORT, PIC_EOI_COMMAND);
    outp(PIC1_CMD_PORT, PIC_EOI_COMMAND);
}

/*
 * Program DMA0 (Intel 8237A) for 8‑bit single‑cycle playback.
 *
 * Background (very short intro to ISA DMA for 8‑bit audio):
 * - The original PC DMA controller (DMA0 / 8237A) services 8‑bit channels 0..3.
 * - A physical address is 20‑bit. DMA0 splits this into:
 *     - 16‑bit base address (low 16 bits)
 *     - 8‑bit page register (high 8 bits)
 *   So we compute a 20‑bit linear address and write it as 16‑bit addr + 8‑bit page.
 * - The controller uses a byte‑wide I/O port and a flip‑flop: each 16‑bit value
 *   (address or count) is written as low byte then high byte. Writing to the
 *   CLEARFF port resets that flip‑flop so the next write is treated as the low byte.
 * - The transfer count is programmed as (length‑1). For example, to transfer N bytes,
 *   you write N‑1 into the count registers.
 * - We program the mode register for: single transfer, address increment,
 *   memory‑to‑device (playback), and the given channel.
 * - Finally, we unmask the channel so the DMA engine can run when the device requests it.
 */
static void dma_program(const void __far *data, unsigned len) {
    /* Compute the 20‑bit linear (physical) address from segment:offset */
    unsigned long lin = ((unsigned long)FP_SEG(data) << 4) + FP_OFF(data);
    /* Split into the 8237A's 16‑bit base and 8‑bit page */
    unsigned addr  = (unsigned)(lin & 0xFFFF);
    unsigned page  = (unsigned)((lin >> 16) & 0xFF);
    /* DMA count is programmed as (length‑1) */
    unsigned cnt   = len - 1;
    /* 8‑bit DMA channel number (0..3), selected from BLASTER env during init */
    int ch = g_sb_dma8;

    dbg_log_printf("SB16", "dma: ch=%d addr=%04X page=%02X len=%u cnt=%u", ch, addr, page, len, cnt);
    /* 1) Mask the channel so we can safely program registers */
    outp(DMA0_MASK_PORT, DMA0_MASK_SET_BIT | ch);
    dbg_log_printf("SB16", "dma: masked ch=%d", ch);
    /* 2) Reset the flip‑flop so the next 16‑bit write starts with low byte */
    outp(DMA0_CLEARFF_PORT, 0x00); /* value ignored; write triggers reset */
    dbg_log_printf("SB16", "dma: clearff #1");
    /* 3) Set mode: single transfer, increment address, memory→device, for this channel */
    outp(DMA0_MODE_PORT, DMA0_MODE_SINGLE_INC_MEM2DEV | ch);
    dbg_log_printf("SB16", "dma: mode set");
    /* 4) Program base address (low then high) */
    outp(dma0_addr_port[ch], addr & 0xFF);
    outp(dma0_addr_port[ch], (addr >> 8) & 0xFF);
    dbg_log_printf("SB16", "dma: addr set");
    /* 5) Program page register (upper 8 bits of the 20‑bit address) */
    outp(dma0_page_port[ch], page & 0xFF);
    dbg_log_printf("SB16", "dma: page set");
    /* 6) Reset flip‑flop again before writing the 16‑bit count */
    outp(DMA0_CLEARFF_PORT, 0x00);
    dbg_log_printf("SB16", "dma: clearff #2");
    /* 7) Program count (low then high). Note: cnt == len‑1 */
    outp(dma0_count_port[ch], cnt & 0xFF);
    outp(dma0_count_port[ch], (cnt >> 8) & 0xFF);
    dbg_log_printf("SB16", "dma: count set");
    /* 8) Unmask the channel to allow DMA requests */
    outp(DMA0_MASK_PORT, ch);
    dbg_log_printf("SB16", "dma: unmasked ch=%d", ch);
}

static void sb_start_playback(unsigned len, unsigned hz) {
    dbg_log_printf("SB16", "start: set_rate=%u", hz);
    dsp_set_rate(hz);
    dbg_log_printf("SB16", "start: speaker on");
    dsp_speaker_on();
    dbg_log_printf("SB16", "start: cmd 8bit single");
    dsp_write(DSP_CMD_8BIT_SINGLE);
    /* SB DSP expects (length-1) low, then high byte */
    dbg_log_printf("SB16", "start: len-1 lo=%u hi=%u", ((len - 1) & 0xFF), (((len - 1) >> 8) & 0xFF));
    dsp_write((len - 1) & 0xFF);
    dsp_write(((len - 1) >> 8) & 0xFF);
    dbg_log_printf("SB16", "start: len=%u hz=%u ticks=%lu", len, hz, bios_ticks());
}

/* ---- Public API ---- */
int sb16_init(unsigned srate_hz) {
    int vec;
    unsigned long alloc;

    if (srate_hz==0) srate_hz = SB16_DEFAULT_SRATE;
    g_engine_rate = srate_hz;

    dbg_log_open(0);
    parse_blaster();
    dbg_log_printf("SB16", "BLASTER: base=%03X irq=%d dma8=%d rate=%u", g_sb_base, g_sb_irq, g_sb_dma8, g_engine_rate);
    if (dsp_reset()!=0) return SB16_ERR;
    dbg_log_printf("SB16", "dsp_reset: OK");
    /* Ensure mixer IRQ/DMA match the BLASTER settings */
    sb16_config_irq_dma();

    /* default mixer levels */
    set_master_volume(SB16_DEFAULT_MASTER);
    set_pcm_volume(SB16_DEFAULT_PCM);

    /* Allocate a 64K-aligned DMA buffer with room up to 64K */
    /* Allocate enough to round up to next 64K boundary */
    alloc = SB16_MAX_LEN + ALIGN_64K_MASK + DMA_ALIGN_SLOP;
    g_dma_raw = (unsigned char __far *)_fmalloc(alloc);
     dbg_log_printf("SB16", "dma_alloc: size=%08lX", alloc);
    if (!g_dma_raw) return SB16_ERR;
    {
        unsigned long lin  = ((unsigned long)FP_SEG(g_dma_raw) << 4) + FP_OFF(g_dma_raw);
        unsigned long lin2 = (lin + ALIGN_64K_MASK) & ~ALIGN_64K_MASK;  /* ceil to 64K */
        g_dma_buf = (unsigned char __far *)MK_FP((unsigned)(lin2 >> 4), (unsigned)(lin2 & 0x0F));
        dbg_log_printf("SB16", "dma_alloc: raw=%04X:%04X lin=%08lX buf=%04X:%04X", FP_SEG(g_dma_raw), FP_OFF(g_dma_raw), lin, FP_SEG(g_dma_buf), FP_OFF(g_dma_buf));
    }
    g_dma_len = 0;

    /* Install IRQ handler */
    vec = irq_to_vector(g_sb_irq);
    _disable();
    g_old_isr = _dos_getvect(vec);
    _dos_setvect(vec, sb16_irq_isr);
    _enable();
    dbg_log_printf("SB16", "isr installed: vec=%02X", vec);
    /* Unmask PIC for our IRQ so we receive it */
    pic_unmask_irq(g_sb_irq);

    /* init waves */
    {
        int i; for (i=0;i<SB16_MAX_WAVES;i++){ g_waves[i].data=0; g_waves[i].len=0; g_waves[i].rate=g_engine_rate; g_waves[i].used=0; }
    }
    return SB16_OK;
}

void sb16_shutdown(void) {
    int vec;
    /* Stop any ongoing playback cleanly */
    if (g_playing) {
        (void)dsp_write(DSP_CMD_HALT_DMA);
        dsp_speaker_off();
        /* Mask DMA channel to be safe */
        outp(DMA0_MASK_PORT, DMA0_MASK_SET_BIT | g_sb_dma8);
        /* Poll DMA0 status for terminal count to avoid freeing while active */
        {
            unsigned long t = 100000UL;
            while (t--) {
                unsigned char s = (unsigned char)inp(DMA0_STATUS_PORT);
                if (s & (1u << (unsigned)g_sb_dma8)) break;
            }
        }
        g_playing = 0;
    } else {
        dsp_speaker_off();
    }
    /* Restore PCM mixer volume if it was changed during play */
    if (g_have_prev_pcm) {
        mixer_write(MIXER_REG_PCM_L, g_prev_pcm_l);
        mixer_write(MIXER_REG_PCM_R, g_prev_pcm_r);
        g_have_prev_pcm = 0;
    }
    vec = irq_to_vector(g_sb_irq);
    if (g_old_isr) { _dos_setvect(vec, g_old_isr); g_old_isr = 0; }
    sb16_unload_all();
    if (g_dma_raw) { _ffree(g_dma_raw); g_dma_raw = g_dma_buf = 0; }
    dbg_log_printf("SB16", "shutdown: ticks=%lu", bios_ticks());
}

void sb16_unload_all(void) {
    int i;
    for (i=0;i<SB16_MAX_WAVES;i++) {
        if (g_waves[i].used && g_waves[i].data) { _ffree(g_waves[i].data); }
        g_waves[i].data=0; g_waves[i].len=0; g_waves[i].rate=g_engine_rate; g_waves[i].used=0;
    }
}

int sb16_register_wave(const unsigned char *pcm_near, unsigned long length, unsigned srate_hz) {
    int i;
    /* Accept large length but clamp to 16-bit DMA limit */
    if (!pcm_near || length==0UL) return SB16_ERR;
    if (srate_hz==0) srate_hz = g_engine_rate;
    for (i=0;i<SB16_MAX_WAVES;i++) if (!g_waves[i].used) break;
    if (i>=SB16_MAX_WAVES) return SB16_ERR;

    {
        unsigned len16 = (length > (unsigned long)SB16_MAX_LEN) ? SB16_MAX_LEN : (unsigned)length;
        g_waves[i].data = (unsigned char __far *)_fmalloc(len16);
        if (!g_waves[i].data) return SB16_ERR;
        /* copy near -> far efficiently */
        movedata(FP_SEG(pcm_near), FP_OFF(pcm_near), FP_SEG(g_waves[i].data), FP_OFF(g_waves[i].data), len16);
        g_waves[i].len  = len16;
    }
    g_waves[i].rate = srate_hz;
    g_waves[i].used = 1;
    dbg_log_printf("SB16", "register: idx=%d len=%u rate=%u", i, g_waves[i].len, g_waves[i].rate);
    return i;
}

/* Copy PCM bytes using pointer iteration (no movedata). Note: use __huge so
 * pointer arithmetic crosses 64K boundaries correctly on 16:16 pointers. */
static void copy_u8_far_to_dma(const unsigned char __far *src, unsigned len) {
    const unsigned char __huge *s = (const unsigned char __huge *)src;
    unsigned char __huge *d = (unsigned char __huge *)g_dma_buf;
    while (len--) {
        *d++ = *s++;
    }
}

int sb16_play(unsigned index, unsigned volume) {
    unsigned len;
    unsigned rate;
    if (index >= SB16_MAX_WAVES) return SB16_ERR;
    if (!g_waves[index].used) return SB16_ERR;
    /* If already playing, preempt the current sound cleanly */
    if (g_playing) {
        dbg_log_printf("SB16", "play preempt: idx=%u", index);
        dsp_write(DSP_CMD_HALT_DMA);   /* stop playback */
        g_playing = 0;
        if (g_have_prev_pcm) {
            mixer_write(MIXER_REG_PCM_L, g_prev_pcm_l);
            mixer_write(MIXER_REG_PCM_R, g_prev_pcm_r);
            g_have_prev_pcm = 0;
        }
    }

    if (volume > SB16_MAX_VOLUME) volume = SB16_MAX_VOLUME;
    len  = g_waves[index].len;
    rate = g_waves[index].rate;
    if (len == 0) return SB16_ERR;

    /* Save current PCM mixer volume and set requested hard ware volume */
    g_prev_pcm_l = mixer_read(MIXER_REG_PCM_L);
    g_prev_pcm_r = mixer_read(MIXER_REG_PCM_R);
    g_have_prev_pcm = 1;
    set_pcm_volume((unsigned char)volume);
    dbg_log_printf("SB16", "play: idx=%u vol=%u len=%u rate=%u", index, volume, len, rate);
    dbg_log_printf("SB16", "src: %04X:%04X len=%u", FP_SEG(g_waves[index].data), FP_OFF(g_waves[index].data), len);

    /* Always copy into the 64K-aligned DMA buffer to avoid boundary issues */
    {
        const unsigned char __far *src = g_waves[index].data;
        unsigned off = FP_OFF(src);
        dbg_log_printf("SB16", "copy->dma_buf: off=%04X len=%u dst=%04X:%04X", off, len, FP_SEG(g_dma_buf), FP_OFF(g_dma_buf));
        copy_u8_far_to_dma(src, 2048);
        g_dma_len = len;
        /* Program DMA and start playback from DMA buffer */
        dbg_log_printf("SB16", "pre-dma: src=%04X:%04X len=%u", FP_SEG(g_dma_buf), FP_OFF(g_dma_buf), g_dma_len);
        dma_program(g_dma_buf, g_dma_len);
    }
    g_playing = 1;
    sb_start_playback(g_dma_len, rate);
    return SB16_OK;
}

int sb16_is_playing(void) {
    return g_playing;
}

void sb16_set_mixer(unsigned char master, unsigned char pcm) {
    set_master_volume(master);
    set_pcm_volume(pcm);
}

/* Public: query DSP version. Returns 0 on success, -1 on failure. */
int sb16_get_dsp_version(unsigned char *major, unsigned char *minor) {
    unsigned long t;
    if (dsp_write(0xE1) != 0) return SB16_ERR; /* Get DSP Version */
    /* wait for major */
    t = 100000UL;
    while (t-- && !(inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) { }
    if (!(inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) return SB16_ERR;
    if (major) *major = (unsigned char)inp(SB16_PORT_READ(g_sb_base));
    /* wait for minor */
    t = 100000UL;
    while (t-- && !(inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) { }
    if (!(inp(SB16_PORT_STATUS(g_sb_base)) & DSP_STATUS_RX_READY)) return SB16_ERR;
    if (minor) *minor = (unsigned char)inp(SB16_PORT_READ(g_sb_base));
    return SB16_OK;
}

int sb16_is_sb16(void) {
    unsigned char maj=0, min=0;
    if (sb16_get_dsp_version(&maj, &min) == SB16_OK) return (maj >= 4) ? 1 : 0;
    return 0;
}

/* ---- WAV loader (8-bit, mono, PCM) ---- */
/* FourCC helpers */
#define FCC(a,b,c,d)  ((unsigned long)(unsigned char)(a) | ((unsigned long)(unsigned char)(b)<<8) | ((unsigned long)(unsigned char)(c)<<16) | ((unsigned long)(unsigned char)(d)<<24))
#define FCC_RIFF FCC('R','I','F','F')
#define FCC_WAVE FCC('W','A','V','E')
#define FCC_fmt_ FCC('f','m','t',' ')
#define FCC_data FCC('d','a','t','a')

typedef struct {
    unsigned long id;
    unsigned long size;
} RiffChunk;

int sb16_load_wav(const char *path) {
    FILE *fp = 0;
    unsigned long riff_id=0, riff_size=0, wave_id=0;
    unsigned long fmt_rate=0;
    unsigned short fmt_fmt=0, fmt_ch=0, fmt_bits=0;
    unsigned short fmt_blockalign=0;
    int idx = SB16_ERR;
    unsigned to_read = 0;
    unsigned char *near_buf = 0;

    if (!path) return SB16_ERR;
    dbg_log_printf("SB16", "wav: open '%s'", path);
    fp = fopen(path, "rb");
    if (!fp) return SB16_ERR;

    /* RIFF header: 'RIFF' <size> 'WAVE' */
    if (fread(&riff_id, 1, 4, fp) != 4) { dbg_log_printf("SB16", "wav: short read RIFF id"); goto fail; }
    if (fread(&riff_size, 1, 4, fp) != 4) { dbg_log_printf("SB16", "wav: short read RIFF size"); goto fail; }
    if (fread(&wave_id, 1, 4, fp) != 4) { dbg_log_printf("SB16", "wav: short read WAVE id"); goto fail; }
    if (riff_id != FCC_RIFF || wave_id != FCC_WAVE) goto fail;

    /* Iterate chunks until we find fmt and data */
    {
        int have_fmt = 0, have_data = 0;
        unsigned long data_size = 0;
        long data_pos = 0;
        while (!have_data) {
            RiffChunk ch;
            if (fread(&ch, 1, sizeof(ch), fp) != sizeof(ch)) { dbg_log_printf("SB16", "wav: EOF while scanning chunks"); goto fail; }
            if (ch.id == FCC_fmt_) {
                unsigned char fmtbuf[32];
                unsigned long n = ch.size;
                if (n > sizeof(fmtbuf)) n = sizeof(fmtbuf);
                if (fread(fmtbuf, 1, n, fp) != n) { dbg_log_printf("SB16", "wav: short fmt chunk"); goto fail; }
                /* Parse PCM fmt (at least 16 bytes) */
                if (ch.size >= 16) {
                    fmt_fmt  = *(unsigned short*)(fmtbuf + 0);
                    fmt_ch   = *(unsigned short*)(fmtbuf + 2);
                    fmt_rate = *(unsigned long *)(fmtbuf + 4);
                    /* skip byteRate at +8 */
                    fmt_blockalign = *(unsigned short*)(fmtbuf + 12);
                    fmt_bits = *(unsigned short*)(fmtbuf + 14);
                    dbg_log_printf("SB16", "wav: fmt fmt=%u ch=%u bits=%u rate=%lu blockalign=%u", (unsigned)fmt_fmt, (unsigned)fmt_ch, (unsigned)fmt_bits, fmt_rate, (unsigned)fmt_blockalign);
                }
                else goto fail;
                /* Skip any remaining fmt bytes */
                if (ch.size > n) fseek(fp, (long)(ch.size - n), SEEK_CUR);
                have_fmt = 1;
            } else if (ch.id == FCC_data) {
                data_size = ch.size;
                data_pos = ftell(fp);
                have_data = 1;
                dbg_log_printf("SB16", "wav: data size=%lu at=%ld", data_size, data_pos);
                /* do not read yet; break after validation */
            } else {
                /* skip unknown chunk */
                fseek(fp, (long)ch.size, SEEK_CUR);
            }
            /* Chunks are word-aligned */
            if (ch.size & 1) fseek(fp, 1, SEEK_CUR);
        }

        /* Validate fmt */
        if (!have_fmt) { dbg_log_printf("SB16", "wav: missing fmt chunk"); goto fail; }
        if (fmt_fmt != 1 /*PCM*/ || fmt_ch != 1 || fmt_bits != 8) { dbg_log_printf("SB16", "wav: unsupported fmt (fmt=%u ch=%u bits=%u)", (unsigned)fmt_fmt, (unsigned)fmt_ch, (unsigned)fmt_bits); goto fail; }
        if (fmt_blockalign == 0) { dbg_log_printf("SB16", "wav: invalid blockalign"); goto fail; }
        if (fmt_rate == 0) fmt_rate = g_engine_rate;

        /* Clamp to DMA single-cycle limit */
        if (data_size > SB16_MAX_LEN) data_size = SB16_MAX_LEN;
        to_read = (unsigned)data_size;

        /* Flat load into near buffer, then register (copies near->far) */
        if (fseek(fp, data_pos, SEEK_SET) != 0) { dbg_log_printf("SB16", "wav: fseek to data failed"); goto fail; }
        near_buf = (unsigned char*)malloc(to_read);
        if (!near_buf) {
            /* Fallback: allocate far buffer and stream into it with a small near chunk */
            int i;
            unsigned char __far *far_buf;
            dbg_log_printf("SB16", "wav: malloc %u failed; using far streaming fallback", to_read);
            for (i=0;i<SB16_MAX_WAVES;i++) if (!g_waves[i].used) break;
            if (i>=SB16_MAX_WAVES) { dbg_log_printf("SB16", "wav: no free wave slots"); goto fail; }
            far_buf = (unsigned char __far *)_fmalloc(to_read);
            if (!far_buf) { dbg_log_printf("SB16", "wav: _fmalloc %u failed", to_read); goto fail; }
            {
                unsigned written = 0;
                static unsigned char chunk[2048]; /* near static to avoid stack blowups */
                unsigned long base_lin = ((unsigned long)FP_SEG(far_buf) << 4) + FP_OFF(far_buf);
                while (written < to_read) {
                    unsigned want = to_read - written;
                    if (want > sizeof(chunk)) want = (unsigned)sizeof(chunk);
                    {
                        size_t n = fread(chunk, 1, want, fp);
                        if (n == 0) { dbg_log_printf("SB16", "wav: short read while streaming (off=%u)", written); _ffree(far_buf); goto fail; }
                        /* Copy chunk to far_buf, splitting at 64K boundaries */
                        {
                            unsigned long dlin = base_lin + written;
                            unsigned src_seg = FP_SEG(chunk);
                            unsigned src_off = FP_OFF(chunk);
                            unsigned remaining = (unsigned)n;
                            while (remaining) {
                                unsigned dseg = (unsigned)(dlin >> 4);
                                unsigned doff = (unsigned)(dlin & 0x0F);
                                unsigned room = 0x10000u - doff;
                                unsigned chunk_len = remaining < room ? remaining : room;
                                movedata(src_seg, src_off, dseg, doff, chunk_len);
                                dlin      += chunk_len;
                                src_off   += chunk_len;
                                remaining -= chunk_len;
                            }
                            written += (unsigned)n;
                        }
                    }
                }
                g_waves[i].data = far_buf;
                g_waves[i].len  = to_read;
                g_waves[i].rate = (unsigned)fmt_rate;
                g_waves[i].used = 1;
                idx = i;
                dbg_log_printf("SB16", "wav: registered idx=%d len=%u rate=%lu (far)", idx, to_read, fmt_rate);
            }
        } else {
            if (fread(near_buf, 1, to_read, fp) != to_read) { dbg_log_printf("SB16", "wav: short read data"); goto fail; }
            idx = sb16_register_wave(near_buf, to_read, (unsigned)fmt_rate);
            dbg_log_printf("SB16", "wav: registered idx=%d len=%u rate=%lu", idx, to_read, fmt_rate);
        }
    }

fail:
    if (near_buf) free(near_buf);
    if (fp) fclose(fp);
    return idx;
}
