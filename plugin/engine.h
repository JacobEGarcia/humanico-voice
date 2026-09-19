/* engine.h - Humanico Voice engine interface. MIT. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Render text to mono PCM at 10000 Hz. Returns sample count (<= max_out),
 * or 0 on error. output must hold max_out floats. */
int hv_say(const char *text, double rate, double pitch, float *output, int max_out);
#ifdef __cplusplus
}
#endif
