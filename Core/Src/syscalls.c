/**
 * Minimal newlib system call stubs for a bare-metal target.
 *
 * Without these, the linker pulls the default stubs from libnosys and emits:
 *   warning: _close / _lseek / _read / _write is not implemented and will
 *   always fail
 * CubeIDE's error parser reports those four warnings as build errors even
 * though the link succeeds, so we provide real implementations here.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "board.h"

/* -------------------------------------------------------------------------- */
/* Console                                                                    */
/* -------------------------------------------------------------------------- */

int _write(int file, char *ptr, int len)
{
  int remaining = len;

  (void)file;

  if (huart1.Instance == NULL)
  {
    return len;   /* called before board_init(): discard */
  }

  /* One blocking transfer instead of one per byte.  HAL takes a 16-bit
   * length, so very large writes are split into chunks. */
  while (remaining > 0)
  {
    uint16_t chunk = (remaining > 0xFFFF) ? 0xFFFFU : (uint16_t)remaining;

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)ptr, chunk, 1000U);
    ptr       += chunk;
    remaining -= (int)chunk;
  }

  return len;
}

int _read(int file, char *ptr, int len)
{
  (void)file;
  (void)ptr;
  (void)len;
  return 0;
}

/* -------------------------------------------------------------------------- */
/* File system stubs                                                          */
/* -------------------------------------------------------------------------- */

int _close(int file)
{
  (void)file;
  return -1;
}

int _fstat(int file, struct stat *st)
{
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file)
{
  (void)file;
  return 1;
}

int _lseek(int file, int ptr, int dir)
{
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

/* -------------------------------------------------------------------------- */
/* Process stubs                                                              */
/* -------------------------------------------------------------------------- */

int _kill(int pid, int sig)
{
  (void)pid;
  (void)sig;
  errno = EINVAL;
  return -1;
}

int _getpid(void)
{
  return 1;
}

void _exit(int status)
{
  (void)status;
  for (;;)
  {
  }
}

/* -------------------------------------------------------------------------- */
/* Heap                                                                       */
/* -------------------------------------------------------------------------- */

extern uint8_t _end;      /* end of .bss, provided by the linker script */
extern uint8_t _estack;   /* top of RAM, provided by the linker script */

static uint8_t *heap_ptr;

void *_sbrk(ptrdiff_t incr)
{
  uint8_t *previous;
  uint8_t *next;
  const uintptr_t limit = (uintptr_t)&_estack - 0x800U;  /* keep 2 KB for the stack */

  if (heap_ptr == NULL)
  {
    heap_ptr = &_end;
  }

  previous = heap_ptr;
  next     = previous + incr;

  if ((uintptr_t)next > limit)
  {
    errno = ENOMEM;
    return (void *)-1;
  }

  heap_ptr = next;
  return (void *)previous;
}
