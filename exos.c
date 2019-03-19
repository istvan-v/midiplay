#ifndef EXOS_C_INCLUDED
#define EXOS_C_INCLUDED

#include <stdio.h>
#include "exos.h"

static unsigned char exos_open_(unsigned char n, const char *fname,
                                unsigned char exosFN) __naked
{
  (void) n;
  (void) fname;
  (void) exosFN;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "ld    (00002$ + 1), a\n"
      "inc   hl\n"
      "ld    e, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "inc   hl\n"
      "ld    a, (hl)\n"
      "ld    (00003$), a\n"
      "ld    l, e\n"
      "ld    h, d\n"
      "xor   a\n"
      "ld    c, a\n"
      "ld    b, a\n"
      "cpir\n"
      "ld    l, c\n"
      "ld    h, b\n"
      "add   hl, sp\n"
      "ld    sp, hl\n"
      "push  hl\n"
      "ld    a, c\n"
      "cpl\n"
      "ld    c, a\n"
      "ld    (hl), a\n"
      "inc   hl\n"
      "ld    a, b\n"
      "cpl\n"
      "ld    b, a\n"
      "or    c\n"
      "jr    z, 00001$\n"
      "ex    de, hl\n"
      "ldir\n"
      "ex    de, hl\n"
      "00001$:\n"
      "pop   de\n"
      "00002$:\n"
      "ld     a, #0\n"
      "rst    0x30\n"
      "00003$:\n"
      ".byte  1\n"
      "ld     sp, hl\n"
      "ld     l, a\n"
      "ret\n"
  );
}

unsigned char exos_open_channel(unsigned char n, const char *fname)
{
  return exos_open_(n, fname, 1);
}

unsigned char exos_create_channel(unsigned char n, const char *fname)
{
  return exos_open_(n, fname, 2);
}


unsigned char exos_close_channel(unsigned char n) __naked
{
  (void) n;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "rst   0x30\n"
      ".byte 3\n"
      "ld    l, a\n"
      "ret\n"
  );
}

int exos_read_byte(unsigned char chn) __naked
{
  (void) chn;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "rst   0x30\n"
      ".byte 5\n"
      "ld    l, b\n"
      "ld    h, a\n"
      "ret\n"
  );
}

unsigned char exos_write_byte(unsigned char chn, unsigned char b) __naked
{
  (void) chn;
  (void) b;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    b, (hl)\n"
      "rst   0x30\n"
      ".byte 7\n"
      "ld    l, a\n"
      "ret\n"
  );
}

unsigned int exos_read_block(unsigned char chn,
                             void *buf, unsigned int nbytes) __naked
{
  (void) chn;
  (void) buf;
  (void) nbytes;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    e, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "inc   hl\n"
      "ld    c, (hl)\n"
      "inc   hl\n"
      "ld    b, (hl)\n"
      "push  bc\n"
      "rst   0x30\n"
      ".byte 6\n"
      "pop   hl\n"
      "or    a\n"
      "sbc   hl, bc\n"
      "ret\n"
  );
}

unsigned int exos_write_block(unsigned char chn,
                              const void *buf, unsigned int nbytes) __naked
{
  (void) chn;
  (void) buf;
  (void) nbytes;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    e, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "inc   hl\n"
      "ld    c, (hl)\n"
      "inc   hl\n"
      "ld    b, (hl)\n"
      "push  bc\n"
      "rst   0x30\n"
      ".byte 8\n"
      "pop   hl\n"
      "or    a\n"
      "sbc   hl, bc\n"
      "ret\n"
  );
}

unsigned char exos_special_func(unsigned char chn, unsigned char func,
                                unsigned char param1, unsigned char param2,
                                unsigned char param3) __naked
{
  (void) chn;
  (void) func;
  (void) param1;
  (void) param2;
  (void) param3;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    b, (hl)\n"
      "inc   hl\n"
      "ld    c, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "inc   hl\n"
      "ld    e, (hl)\n"
      "rst   0x30\n"
      ".byte 11\n"
      "ld    l, a\n"
      "ret\n"
  );
}

int exos_channel_read_status(unsigned char chn) __naked
{
  (void) chn;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "rst   0x30\n"
      ".byte 9\n"
      "ld    l, c\n"
      "ld    h, a\n"
      "ret\n"
  );
}

unsigned char exos_set_variable(unsigned char n, unsigned char value) __naked
{
  (void) n;
  (void) value;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    c, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "ld    b, #1\n"
      "rst   0x30\n"
      ".byte 16\n"
      "ld    l, a\n"
      "ret\n"
  );
}

int exos_get_variable(unsigned char n) __naked
{
  (void) n;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    c, (hl)\n"
      "ld    b, #0\n"
      "rst   0x30\n"
      ".byte 16\n"
      "ld    l, d\n"
      "ld    h, a\n"
      "ret\n"
  );
}

int exos_alloc(void) __naked
{
  __asm__ (
      "rst   0x30\n"
      ".byte 24\n"
      "ld    l, c\n"
      "ld    h, a\n"
      "ret\n"
  );
}

unsigned char exos_free(unsigned char segment) __naked
{
  (void) segment;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    c, (hl)\n"
      "rst   0x30\n"
      ".byte 25\n"
      "ld    l, a\n"
      "ret\n"
  );
}

int getchar(void) __naked
{
  __asm__ (
      "ld    a, #0xff\n"
      "rst   0x30\n"
      ".byte 5\n"
      "ld    l, b\n"
      "ld    h, a\n"
      "ret\n"
  );
}

void print_chn(unsigned char chn, const char *s) __naked
{
  (void) chn;
  (void) s;
  __asm__ (
    "ld    hl, #2\n"
    "add   hl, sp\n"
    "ld    a, (hl)\n"
    "inc   hl\n"
    "ld    e, (hl)\n"
    "inc   hl\n"
    "ld    d, (hl)\n"
    "ld    h, a\n"
    "xor   a\n"
    "ld    c, a\n"
    "ld    b, a\n"
    "ex    de, hl\n"
    "cpir\n"
    "add   hl, bc\n"
    "ex    de, hl\n"
    "ld    a, c\n"
    "cpl\n"
    "ld    c, a\n"
    "ld    a, b\n"
    "cpl\n"
    "ld    b, a\n"
    "or    c\n"
    "ret   z\n"
    "ld    a, h\n"
    "rst   0x30\n"
    ".byte 8\n"
    "ret\n"
  );
}

void print_string(const char *s)
{
  print_chn(255, s);
}

#endif
