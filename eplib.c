#ifndef EPLIB_C_INCLUDED
#define EPLIB_C_INCLUDED

#include <stdarg.h>

#include "eplib.h"
#include "exos.h"

unsigned char rgb(int r, int g, int b)
{
  unsigned char ri, gi, bi, c;
  r = (r > 0 ? (r < 255 ? r : 255) : 0);
  g = (g > 0 ? (g < 255 ? g : 255) : 0);
  b = (b > 0 ? (b < 255 ? b : 255) : 0);
  ri = (unsigned char) ((r * 7 + 128) >> 8);
  gi = (unsigned char) ((g * 7 + 128) >> 8);
  bi = (unsigned char) ((b * 3 + 128) >> 8);
  c = ((ri & 1) << 6) | ((ri & 2) << 2) | ((ri & 4) >> 2)
      | ((gi & 1) << 7) | ((gi & 2) << 3) | ((gi & 4) >> 1)
      | ((bi & 1) << 5) | ((bi & 2) << 1);
  return c;
}

/* RND() & m, 0 to l */

unsigned char rnd8(unsigned char l, unsigned char m) __z88dk_callee __naked
{
  (void) l;
  (void) m;
  __asm__ (
      "pop   hl\n"
      "pop   bc\n"
      "push  hl\n"
      "00001$:\n"
      "jr    00004$\n"
      ".byte 0\n"
      "00002$:\n"
      "ld    a, r\n"
      "xor   a, l\n"
      "add   hl, hl\n"
      "xor   a, h\n"
      "ld    l, a\n"
      "xor   a, h\n"
      "ld    h, a\n"
      "and   a, b\n"
      "cp    a, c\n"
      "jr    c, 00003$\n"
      "jr    nz, 00002$\n"
      "00003$:\n"
      "ld    (00001$ + 1), hl\n"
      "ld    l, a\n"
      "ret\n"
      "00004$:\n"
      "ld    a, #0x21\n"
      "ld    (00001$), a\n"
      "di\n"
      "ld    a, r\n"
      "add   a, #4\n"
      "and   #0x7f\n"
      "ld    r, a\n"
      "ei\n"
      "ld    h, a\n"
      "push  bc\n"
      "ld    bc, #0x0027\n"
      "rst   0x30\n"
      ".byte 16\n"
      "pop   bc\n"
      "ld    l, d\n"
      "jr    00002$\n"
  );
}

void spoke(unsigned char s, unsigned int a, unsigned char b) __naked
{
  (void) s;
  (void) a;
  (void) b;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    b, (hl)\n"
      "inc   hl\n"
      "ld    e, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "inc   hl\n"
      "ld    a, (hl)\n"
      "set   7, d\n"
      "set   6, d\n"
      "ld    c, #0xb3\n"
      "in    h, (c)\n"
      "di\n"
      "out   (c), b\n"
      "ld    (de), a\n"
      "out   (c), h\n"
      "ei\n"
      "ret\n"
  );
}

unsigned char speek(unsigned char s, unsigned int a) __naked
{
  (void) s;
  (void) a;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    b, (hl)\n"
      "inc   hl\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    h, (hl)\n"
      "ld    l, a\n"
      "set   7, h\n"
      "set   6, h\n"
      "ld    c, #0xb3\n"
      "in    a, (c)\n"
      "di\n"
      "out   (c), b\n"
      "ld    l, (hl)\n"
      "out   (c), a\n"
      "ei\n"
      "ret\n"
  );
}

void spoke16(unsigned char s, unsigned int a, unsigned short w) __naked
{
  (void) s;
  (void) a;
  (void) w;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    b, (hl)\n"
      "inc   hl\n"
      "ld    e, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "inc   hl\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    h, (hl)\n"
      "ld    l, a\n"
      "ex    de, hl\n"
      "set   7, h\n"
      "set   6, h\n"
      "ld    c, #0xb3\n"
      "in    a, (c)\n"
      "di\n"
      "out   (c), b\n"
      "ld    (hl), e\n"
      "inc   hl\n"
      "ld    (hl), d\n"
      "out   (c), a\n"
      "ei\n"
      "ret\n"
  );
}

unsigned short speek16(unsigned char s, unsigned int a) __naked
{
  (void) s;
  (void) a;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    b, (hl)\n"
      "inc   hl\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    h, (hl)\n"
      "ld    l, a\n"
      "set   7, h\n"
      "set   6, h\n"
      "ld    c, #0xb3\n"
      "in    a, (c)\n"
      "di\n"
      "out   (c), b\n"
      "ld    e, (hl)\n"
      "inc   hl\n"
      "ld    d, (hl)\n"
      "out   (c), a\n"
      "ei\n"
      "ex    de, hl\n"
      "ret\n"
  );
}

void memset_fast(void *p,
                 unsigned char c, unsigned int nbytes) __z88dk_callee __naked
{
  (void) p;
  (void) c;
  (void) nbytes;
  __asm__ (
      "pop   de\n"
      "pop   hl\n"
      "dec   sp\n"
      "pop   af\n"
      "pop   bc\n"
      "push  de\n"
      "ld    d, a\n"
      "ld    a, c\n"
      "or    b\n"
      "ret   z\n"
      "ld    (hl), d\n"
      "dec   bc\n"
      "ld    a, c\n"
      "or    b\n"
      "ret   z\n"
      "ld    e, l\n"
      "ld    d, h\n"
      "inc   de\n"
      "ldir\n"
      "ret\n"
  );
}

static const unsigned int sprintf_d_table[5] = { 10000, 1000, 100, 10, 1 };

int vsprintf_simple(char *buf, const char *fmt, va_list ap)
{
  char    *p = buf;

  for ( ; *fmt; fmt++) {
    char    c = *fmt;
    if (c != '%') {
      *(p++) = c;
    }
    else {
      c = *(++fmt);
      if (c == '%' || !c) {
        *(p++) = '%';
        if (!c)
          break;
      }
      else if (c == 'c') {
        unsigned int  tmp = va_arg(ap, unsigned int);
        *(p++) = (char) tmp;
      }
      else if (c == 'd' || c == 'u') {
        unsigned int  tmp = va_arg(ap, unsigned int);
        char    z = 0, i;
        if (c == 'd' && (int) tmp < 0) {
          *(p++) = '-';
          tmp = 0U - tmp;
        }
        for (i = 0; i < 5; i++) {
          char    d = '0';
          while (tmp >= sprintf_d_table[i]) {
            tmp -= sprintf_d_table[i];
            z = 1;
            d++;
          }
          if (z || i == 4)
            *(p++) = d;
        }
      }
      else if (c == 's') {
        const char  *tmp = va_arg(ap, const char *);
        if (tmp) {
          for ( ; *tmp; tmp++)
            *(p++) = *tmp;
        }
      }
      else {
        p[0] = '%';
        p[1] = c;
        p = p + 2;
      }
    }
  }
  *p = '\0';
  return (int) (p - buf);
}

int sprintf_simple(char *buf, const char *fmt, ...)
{
  va_list ap;
  int     n;
  va_start(ap, fmt);
  n = vsprintf_simple(buf, fmt, ap);
  va_end(ap);
  return n;
}

int fprintf_simple(unsigned char chn, const char *fmt, ...)
{
  char    buf[256];
  va_list ap;
  int     n;
  va_start(ap, fmt);
  n = vsprintf_simple(buf, fmt, ap);
  if (n > 255) {
    __asm__ (
        "xor   a, a\n"
        "rst   0x08\n"
    );
  }
  va_end(ap);
  return (int) exos_write_block(chn, buf, (unsigned int) n);
}

void status_message(const char *msg)
{
  unsigned int  st_addr = speek16(0xFF, 0xBFF6);
  unsigned char i;
  if (!msg)
    msg = "";
  for (i = 0; i < 40; i++) {
    char    c = *msg;
    if (c)
      msg++;
    else
      c = ' ';
    spoke(0xFF, st_addr + i, c);
  }
}

void error_exit(const char *msg)
{
  unsigned char i;
  exos_irq_handler(0);
  status_message(msg);
  for (i = 0; i < 100; i++)
    vsync_wait();
  __asm__ (
      "xor   a, a\n"
      "rst   0x08\n"
  );
}

void vsync_wait(void) __naked
{
  __asm__ (
      "rst   0x10\n"
      ".byte 3\n"
      "ret\n"
  );
}

void exos_irq_handler(int enabled) __naked
{
  (void) enabled;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "or    a, a\n"
      "jr    nz, 00001$\n"
      "rst   0x10\n"
      ".byte 6\n"
      "ret\n"
      "00001$:\n"
      "rst   0x10\n"
      ".byte 9\n"
      "ret\n"
  );
}

void set_irq_callback(void (*func)(void)) __naked
{
  (void) func;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    h, (hl)\n"
      "ld    l, a\n"
      "rst   0x10\n"
      ".byte 12\n"
      "ret\n"
  );
}

void set_exit_callback(void (*func)(void)) __naked
{
  (void) func;
  __asm__ (
      "ld    hl, #2\n"
      "add   hl, sp\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "ld    h, (hl)\n"
      "ld    l, a\n"
      "rst   0x10\n"
      ".byte 15\n"
      "ret\n"
  );
}

unsigned char get_file_name(char *buf) __z88dk_fastcall __naked
{
  (void) buf;
  __asm__ (
      "push  hl\n"
      "ld    hl, #0x00fd\n"
      "push  hl\n"
      "ld    de, #0x534f\n"
      "push  de\n"
      "ld    de, #0x4458\n"
      "push  de\n"
      "ld    de, #0x4506\n"
      "push  de\n"
      "ld    l, h\n"
      "add   hl, sp\n"
      "ex    de, hl\n"
      "rst   0x30\n"
      ".byte 26\n"
      "pop   de\n"
      "jr    nz, 00001$\n"
      "ld    l, a\n"
      "ld    h, a\n"
      "add   hl, sp\n"
      "ex    de, hl\n"
      "pop   hl\n"
      "pop   hl\n"
      "ld    hl, #0x2045\n"
      "ex    (sp), hl\n"
      "ld    hl, #0x4c49\n"
      "push  hl\n"
      "ld    hl, #0x4607\n"
      "push  hl\n"
      "rst   0x30\n"
      ".byte 26\n"
      "00001$:\n"
      "pop   hl\n"
      "pop   hl\n"
      "pop   hl\n"
      "pop   hl\n"
      "jr    nz, 00002$\n"
      "ld    c, (hl)\n"
      "cp    a, c\n"
      "jr    z, 00002$\n"
      "ld    b, a\n"
      "ld    e, l\n"
      "ld    d, h\n"
      "inc   hl\n"
      "ldir\n"
      "ld    (de), a\n"
      "00002$:\n"
      "ld    l, a\n"
      "ret\n"
  );
}

#endif
