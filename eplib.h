#ifndef EPLIB_H_INCLUDED
#define EPLIB_H_INCLUDED

#include <stdarg.h>

__sfr __at 0xB5 dave_keyboard_port;

unsigned char rgb(int r, int g, int b);

/* RND() & m, 0 to l */
unsigned char rnd8(unsigned char l, unsigned char m) __z88dk_callee;

void spoke(unsigned char s, unsigned int a, unsigned char b);
unsigned char speek(unsigned char s, unsigned int a);
void spoke16(unsigned char s, unsigned int a, unsigned short w);
unsigned short speek16(unsigned char s, unsigned int a);
void memset_fast(void *p, unsigned char c, unsigned int nbytes) __z88dk_callee;

int vsprintf_simple(char *buf, const char *fmt, va_list ap);
int sprintf_simple(char *buf, const char *fmt, ...);
int fprintf_simple(unsigned char chn, const char *fmt, ...);
void status_message(const char *msg);

void error_exit(const char *msg);

void vsync_wait(void);
void exos_irq_handler(int enabled);
void set_irq_callback(void (*func)(void));
void set_exit_callback(void (*func)(void));

unsigned char get_file_name(char *buf) __z88dk_fastcall;

#endif
