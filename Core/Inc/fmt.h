/**
 * Small, allocation-free integer/float formatting and parsing helpers.
 * Used instead of printf() so the firmware does not depend on newlib's
 * floating point printf support.
 */

#ifndef FMT_H
#define FMT_H

#include <stdint.h>
#include <stddef.h>

/* Write a NUL-terminated decimal representation; return the length. */
int fmt_u32(char *out, uint32_t value);
int fmt_i32(char *out, int32_t value);
int fmt_f32(char *out, float value, unsigned decimals);

/* Parse a decimal integer / float.  Return 0 on success, -1 on error. */
int parse_i32(const char *text, int32_t *out);
int parse_f32(const char *text, float *out);

/* Skip leading spaces/tabs. */
const char *skip_spaces(const char *text);

#endif /* FMT_H */
