/**
 * Persistent parameters, stored in one flash page at 0x08024000.
 * Layout and magic number match the original firmware.
 */

#ifndef PARAMS_H
#define PARAMS_H

#include <stdint.h>

typedef struct
{
  uint16_t mtype;       /* 1..4, 4 = open loop                        */
  uint16_t deadzone;    /* 0..3600                                    */
  uint16_t mline;       /* encoder lines per motor revolution         */
  uint16_t mphase;      /* gear ratio                                 */
  float    wdiameter;   /* wheel diameter, mm                         */
  float    kp;
  float    ki;
  float    kd;
} params_t;

extern params_t g_params;

/* Load parameters from flash, writing factory defaults if the page is blank. */
void params_init(void);

/* Overwrite the parameter page with g_params (erases + reprograms the page). */
int params_write_all(void);

/* Restore factory defaults and write them. */
void params_factory_reset(void);

#endif /* PARAMS_H */
