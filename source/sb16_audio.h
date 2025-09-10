/* Minimal SB16 audio library (single-voice, async) */
#ifndef SB16_AUDIO_H
#define SB16_AUDIO_H

/* Return codes */
#define SB16_OK              0
#define SB16_ERR            -1
#define SB16_MAX_WAVES      32
#define SB16_MAX_LEN        65535UL     /* single-cycle 8-bit DMA limit */
/* Volume range for SB16 mixer (0..255) */
#define SB16_MAX_VOLUME     255u

/* Initialize SB16 audio at the desired sample rate (Hz).
 * Parses BLASTER for base/IRQ/DMA, resets DSP, installs ISR, and sets default mixer. */
int  sb16_init(unsigned srate_hz);

/* Shutdown: stop speaker, restore ISR, free all sample memory. */
void sb16_shutdown(void);

/* Register an 8-bit unsigned PCM, mono waveform in memory. Copies into far RAM.
 * Returns index [0..SB16_MAX_WAVES-1] on success, -1 on error. */
/* length is in bytes; accepts >64K, but will clamp to SB16_MAX_LEN internally */
int  sb16_register_wave(const unsigned char *pcm_near, unsigned long length, unsigned srate_hz);

/* Unload all registered waveforms and free memory. */
void sb16_unload_all(void);

/* Play a previously registered waveform by index with per-play volume 0..255.
 * Non-blocking; returns 0 on success, -1 on busy/error. */
int  sb16_play(unsigned index, unsigned volume);

/* Non-zero while playback is active. */
int  sb16_is_playing(void);

/* Optional: set mixer master and PCM volumes (0..255). */
void sb16_set_mixer(unsigned char master, unsigned char pcm);

/* Load an 8-bit mono PCM WAV file from disk and register it.
 * Returns index [0..SB16_MAX_WAVES-1] on success, -1 on error.
 * Supports PCM format (audioFormat=1), mono, 8-bit; captures the WAV's sample rate. */
int  sb16_load_wav(const char *path);

/* Query DSP version and capability */
int  sb16_get_dsp_version(unsigned char *major, unsigned char *minor);
int  sb16_is_sb16(void); /* returns non-zero if DSP major >= 4 */

#endif /* SB16_AUDIO_H */
