#include "fmt.h"

#include <string.h>

const char *skip_spaces(const char *text)
{
  while (*text == ' ' || *text == '\t')
  {
    text++;
  }
  return text;
}

int fmt_u32(char *out, uint32_t value)
{
  char tmp[10];
  int n = 0;
  int i;

  if (value == 0U)
  {
    out[0] = '0';
    out[1] = '\0';
    return 1;
  }

  while (value > 0U && n < (int)sizeof(tmp))
  {
    tmp[n++] = (char)('0' + (value % 10U));
    value /= 10U;
  }

  for (i = 0; i < n; i++)
  {
    out[i] = tmp[n - 1 - i];
  }
  out[n] = '\0';
  return n;
}

int fmt_i32(char *out, int32_t value)
{
  if (value < 0)
  {
    out[0] = '-';
    /* Avoid UB on INT32_MIN. */
    return 1 + fmt_u32(out + 1, (uint32_t)(-(value + 1)) + 1U);
  }
  return fmt_u32(out, (uint32_t)value);
}

int fmt_f32(char *out, float value, unsigned decimals)
{
  uint32_t scale = 1U;
  uint32_t scaled;
  uint32_t whole;
  uint32_t frac;
  char *p = out;
  unsigned i;

  if (value != value) /* NaN */
  {
    memcpy(out, "nan", 4);
    return 3;
  }

  if (value < 0.0f)
  {
    *p++ = '-';
    value = -value;
  }

  for (i = 0; i < decimals; i++)
  {
    scale *= 10U;
  }

  /* Keep the conversion inside uint32 range. */
  if (value > 400000.0f)
  {
    value = 400000.0f;
  }

  scaled = (uint32_t)(value * (float)scale + 0.5f);
  whole  = scaled / scale;
  frac   = scaled % scale;

  p += fmt_u32(p, whole);

  if (decimals > 0U)
  {
    uint32_t divisor = scale / 10U;

    *p++ = '.';
    for (i = 0; i < decimals; i++)
    {
      *p++ = (char)('0' + ((frac / divisor) % 10U));
      divisor /= 10U;
    }
  }

  *p = '\0';
  return (int)(p - out);
}

int parse_i32(const char *text, int32_t *out)
{
  int negative = 0;
  int seen = 0;
  uint32_t magnitude = 0U;
  uint32_t limit;

  text = skip_spaces(text);
  if (*text == '+' || *text == '-')
  {
    negative = (*text == '-');
    text++;
  }

  /* INT32_MIN has one more representable magnitude than INT32_MAX. */
  limit = negative ? 0x80000000UL : 0x7FFFFFFFUL;

  while (*text >= '0' && *text <= '9')
  {
    uint32_t digit = (uint32_t)(*text - '0');

    if (magnitude > (limit - digit) / 10U)
    {
      return -1;   /* would overflow int32_t */
    }

    magnitude = magnitude * 10U + digit;
    seen = 1;
    text++;
  }

  if (seen == 0)
  {
    return -1;
  }

  *out = negative ? (int32_t)(0U - magnitude) : (int32_t)magnitude;
  return 0;
}

int parse_f32(const char *text, float *out)
{
  int negative = 0;
  int seen = 0;
  float value = 0.0f;
  float fraction = 0.1f;

  text = skip_spaces(text);
  if (*text == '+' || *text == '-')
  {
    negative = (*text == '-');
    text++;
  }

  while (*text >= '0' && *text <= '9')
  {
    /* Cap the magnitude instead of letting a long digit string saturate to
     * infinity before the range check in the command handler. */
    if (value < 1.0e8f)
    {
      value = value * 10.0f + (float)(*text - '0');
    }
    seen = 1;
    text++;
  }

  if (*text == '.')
  {
    text++;
    while (*text >= '0' && *text <= '9')
    {
      if (fraction >= 1.0e-7f)
      {
        value += (float)(*text - '0') * fraction;
        fraction *= 0.1f;
      }
      seen = 1;
      text++;
    }
  }

  if (seen == 0)
  {
    return -1;
  }

  *out = negative ? -value : value;
  return 0;
}
