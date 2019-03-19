#include "daveplay.h"
#include "midi_in.h"
#include "envelope.h"
#include "eplib.h"

#define ENABLE_CHN1_ALLOC       1

/* oct_table[n] =
 *     (unsigned int) (250000.0 / (440.0 * pow(2.0, (n / 64.0 - 71.0) / 12.0))
 *                     + 0.5)
 */
static unsigned int   oct_table[768];
/* sin_table[n] = (int) (sin(n * PI * 0.5 / 255.0) * 181.02 + 0.5) */
static unsigned char  sin_table[256];

DaveChannel     dave_chn[DAVE_VIRT_CHNS];

static unsigned char  midi_dave_chn[16];
static unsigned char  dave_chn0_index;  /* 0, 4, 6 */
static unsigned char  dave_chn1_index;  /* 1, 5, 7 */

typedef struct {
  unsigned int  chn_freq[4];
  unsigned char volume_l[4];
  unsigned char volume_r[4];
} DaveRegisters;

static DaveRegisters  dave_regs;

/* freq_mult_table[n] =
 *     (unsigned int) (65536.0 * pow(0.5, 1.0 / (3.0 * (1 << n))) + 0.5)
 */
static const unsigned int freq_mult_table[9] = {
  52016, 58386, 61858, 63670, 64596, 65065, 65300, 65418, 65477
};

void dave_init(void)
{
  unsigned int  j, k, f;
  unsigned char s;
  oct_table[0] = 34323U;
  memset_fast(&(oct_table[1]), 0x00, sizeof(unsigned int) * 767);
  for (s = 0; s < 8; s++) {
    k = 256 >> s;
    for (j = k; j < 768; j = j + k) {
      if (oct_table[j] == 0) {
        oct_table[j] = ((unsigned long) oct_table[j - k] * freq_mult_table[s]
                        + 0x8000U) >> 16;
      }
    }
  }
  for (j = 1; j < 767; j = j + 2) {
    k = (oct_table[j - 1] | oct_table[j + 1]) & 1;
    oct_table[j] = (oct_table[j - 1] >> 1) + (oct_table[j + 1] >> 1) + k;
  }
  oct_table[767] = ((unsigned long) oct_table[766] * freq_mult_table[8]
                    + 0x8000U) >> 16;
  j = 0;
  k = 284;
  for (f = 0; f < 256; f++) {
    sin_table[f] = (unsigned char) ((j + 128) >> 8);
    j = j + k;
    k = k - (((j >> 6) * 41 + 8258) >> 14);
  }
  dave_reset();
}

void dave_reset(void)
{
  unsigned char c;
  memset_fast(dave_chn, 0x00, sizeof(dave_chn));
  for (c = 0; c < DAVE_VIRT_CHNS; c++) {
    DaveChannel *chn = dave_channel_ptr(c);
    chn->env_state = 0xA0;
    chn->pitch = 0x0080;
    chn->veloc = 127;
    chn->pan = 64;
    chn->vol = 127;
    chn->vol_l = 128;
    chn->vol_r = 128;
  }
  memset_fast(&dave_regs, 0x00, sizeof(DaveRegisters));
  memset_fast(midi_dave_chn, 0, 16);
  dave_chn0_index = 0;
  dave_chn1_index = 1;
}

void dave_channel_release(DaveChannel *chn) __z88dk_fastcall
{
  if (chn->env_state & 0x10) {
    /* release */
    chn->env_state = 0x60;
    if (*(chn->env_ptr) >= 0xC0)
      chn->env_ptr = chn->env_ptr + 4;
    chn->env_timer = 0;
  }
  else if ((unsigned char) chn != (unsigned char) &(dave_chn[3])) {
    chn->env_state = 0xA0;
    chn->env_timer = 0;
  }
}

void dave_channel_off(unsigned char c) __z88dk_fastcall
{
  DaveChannel *chn = dave_channel_ptr(c);
  chn->env_state = 0xA0;
  chn->env_timer = 0;
}

static DaveChannel *find_best_channel(unsigned char c0,
                                      unsigned char c1,
                                      unsigned char c2)
{
  DaveChannel *chn0 = dave_channel_ptr(c0);
  DaveChannel *chn1 = dave_channel_ptr(c1);
  unsigned char env0 = chn0->env_state & 0xE0;
  unsigned char env1 = chn1->env_state & 0xE0;
  if (env0 < env1) {
    env0 = env1;
    chn0 = chn1;
  }
  else if (env0 == env1 && chn0->env_timer < chn1->env_timer) {
    chn0 = chn1;
  }
  chn1 = dave_channel_ptr(c2);
  env1 = chn1->env_state & 0xE0;
  if (env0 < env1)
    return chn1;
  if (env0 == env1 && chn0->env_timer < chn1->env_timer)
    return chn1;
  return chn0;
}

static void pan_note(DaveChannel *chn, unsigned int pitch)
{
  unsigned char *pan = &(chn->pan);
#ifndef PANNED_NOTE_NEW
  unsigned int  p = ((pitch >> 8) << 2) + *pan;
  if (p < 256)
    *pan = 1;
  else if (p >= 380)
    *pan = 125;
  else
    *pan = (unsigned char) (p - 255);
#else
  unsigned char p = (unsigned char) (pitch >> 8);
  p = (unsigned char) (((p + p + *pan) << 1) + 193);
  if (p >= 0x80)
    p = ~p;
  *pan = p;
#endif
}

static void set_channel_params(DaveChannel *chn, const unsigned char *env_ptr,
                               unsigned int pitch, unsigned char veloc,
                               const unsigned char *ctrls)
    __z88dk_callee __preserves_regs(iyl, iyh) __naked
{
  (void) chn;
  (void) env_ptr;
  (void) pitch;
  (void) veloc;
  (void) ctrls;
  __asm__ (
      "pop   de\n"
      "pop   hl\n"
      "pop   bc\n"
      "inc   hl\n"
      "ld    (hl), c\n"
      "inc   hl\n"
      "ld    (hl), b\n"
      "inc   hl\n"
      "inc   hl\n"
      "inc   hl\n"
      "xor   a, a\n"
      "ld    (hl), a\n"
      "inc   hl\n"
      "ld    (hl), a\n"
      "inc   hl\n"
      "pop   bc\n"
      "ld    (hl), c\n"
      "inc   hl\n"
      "ld    (hl), b\n"
      "inc   hl\n"
      "dec   sp\n"
      "pop   bc\n"
      "ld    (hl), b\n"
      "inc   hl\n"
      "ex    de, hl\n"
      "ex    (sp), hl\n"
      "ld    a, (hl)\n"
      "inc   hl\n"
      "inc   hl\n"
      "add   a, a\n"
      "add   a, a\n"
      "ld    (de), a\n"
      "inc   de\n"
      "ldi\n"
      "ldi\n"
      "ex    de, hl\n"
      "ld    (hl), #0xff\n"
      "inc   hl\n"
      "inc   hl\n"
      "ld    (hl), #0\n"
      "ret\n"
  );
}

unsigned char dave_channel_on(unsigned char midi_chn, unsigned char pgm,
                              unsigned int pitch, unsigned char veloc,
                              const unsigned char *ctrls)
{
  DaveChannel   *chn;
  unsigned int  env_pos;
  unsigned char c, env_flags;
#if 0
  veloc = 127;
#endif
  if (midi_chn == 9) {
    chn = &(dave_chn[3]);
    env_pos = drum_env_offsets[(pitch >> 8) & 0x7F];
    dave_chn[3].env_state = (unsigned char) (env_pos >> 8) & 0xB0;
    set_channel_params(chn, envelope_data + ((env_pos & 0x0FFF) << 1),
                       pitch, veloc, ctrls);
    return 3;
  }
  c = midi_dave_chn[midi_chn];
  if (c) {
    if (c < 4)
      chn = dave_channel_ptr(c - 1);
    else
      chn = find_best_channel(c & 1, c & 0xFD, c | 2);
  }
  else {
    c = (midi_chn & 1) << 1;
#if ENABLE_CHN1_ALLOC
    chn = find_best_channel(c, c ^ 2, 1);
#else
    chn = find_best_channel(c, c ^ 2, c);
#endif
  }
  env_pos = pgm_env_offsets[pgm & 0x7F];
  env_flags = (unsigned char) (env_pos >> 8);
  chn->env_state = env_flags & 0xB0;
  set_channel_params(chn, envelope_data + ((env_pos & 0x0FFF) << 1),
                     pitch, veloc, ctrls);
  if (env_flags & 0x40)
    pan_note(chn, pitch);
  c = (unsigned char) ((unsigned int) chn - (unsigned int) dave_chn)
      / sizeof(DaveChannel);
  return c;
}

void dave_chn_distortion(unsigned char c, unsigned char value)
{
  DaveChannel *chn = dave_channel_ptr(c);
  chn->dist = (unsigned char) (value << 2);
}

void dave_assign_channel(unsigned char midi_chn, unsigned char dave_chn)
{
  midi_dave_chn[midi_chn] = (dave_chn >> 2) & 7;
}

void dave_chn_set_pan(unsigned char c, unsigned char value)
{
  DaveChannel *chn = dave_channel_ptr(c);
  chn->pan = value;
  chn->vol_l = 0xFF;
}

void dave_chn_set_volume(unsigned char c, unsigned char value)
{
  DaveChannel *chn = dave_channel_ptr(c);
  chn->vol = value;
  chn->vol_l = 0xFF;
}

void dave_chn_aftertouch(unsigned char c, unsigned char value)
{
  DaveChannel *chn = dave_channel_ptr(c);
  chn->veloc += (value - chn->aftertouch);
  chn->aftertouch = value;
  chn->vol_l = 0xFF;
}

/* v = v1 * 256 + v2 */

static unsigned char volume_mult(unsigned int v)
    __z88dk_fastcall __preserves_regs(c, b, iyl, iyh) __naked
{
  (void) v;
  __asm__ (
      "ld    e, l\n"
      "ld    d, #0\n"
      "sla   h\n"
      "jr    c, 00001$\n"
      "ld    l, d\n"
      "00001$:\n"
      "add   hl, hl\n"
      "inc   l\n"
      "jr    nc, 00002$\n"
      "add   hl, de\n"
      "00002$:\n"
      "add   hl, hl\n"
      "jr    nc, 00003$\n"
      "add   hl, de\n"
      "00003$:\n"
      "add   hl, hl\n"
      "jr    nc, 00004$\n"
      "add   hl, de\n"
      "00004$:\n"
      "add   hl, hl\n"
      "jr    nc, 00005$\n"
      "add   hl, de\n"
      "00005$:\n"
      "add   hl, hl\n"
      "jr    nc, 00006$\n"
      "add   hl, de\n"
      "00006$:\n"
      "add   hl, hl\n"
      "jr    nc, 00007$\n"
      "add   hl, de\n"
      "00007$:\n"
      "add   hl, hl\n"
      "jr    nc, 00008$\n"
      "add   hl, de\n"
      "00008$:\n"
      "add   hl, hl\n"
      "ld    l, h\n"
      "rra\n"
      "or    a, l\n"
      "ret   p\n"
      "ld    l, #0x80\n"
      "ret\n"
  );
}

#if 0
static void dave_ctrl_update(DaveChannel *chn)
{
  unsigned char v, vol_l, vol_r;
  v = volume_mult(((unsigned int) (chn->vol + 1) << 8) | (chn->veloc + 1));
  vol_l = sin_table[(unsigned char) (255 - (chn->pan << 1))];
  vol_r = sin_table[(unsigned char) chn->pan << 1];
  chn->vol_l = volume_mult(((unsigned int) v << 8) | vol_l);
  chn->vol_r = volume_mult(((unsigned int) v << 8) | vol_r);
}
#else
static void dave_ctrl_update(DaveChannel *chn)
    __z88dk_fastcall __preserves_regs(iyl, iyh) __naked
{
  (void) chn;
  __asm__ (
      "push  hl\n"
      "ex    (sp), ix\n"
      "ld    l, 9 (ix)\n"
      "ld    h, 12 (ix)\n"
      "inc   l\n"
      "inc   h\n"
      "call  _volume_mult\n"
      "ld    b, l\n"
      "ld    a, 11 (ix)\n"
      "add   a, a\n"
      "ld    l, a\n"
      "ld    h, #0\n"
      "ld    de, #_sin_table\n"
      "add   hl, de\n"
      "ld    c, (hl)\n"
      "cpl\n"
      "ld    l, a\n"
      "ld    h, #0\n"
      "add   hl, de\n"
      "ld    l, (hl)\n"
      "ld    h, b\n"
      "call  _volume_mult\n"
      "ld    13 (ix), l\n"
      "ld    l, c\n"
      "ld    h, b\n"
      "call  _volume_mult\n"
      "ld    14 (ix), l\n"
      "pop   ix\n"
      "ret\n"
  );
}
#endif

static const signed char poly4_offs_table_5[16] = {
   2,  1,  0, -1,  1,  0, -1,  1,  0, -1,  1,  0, -1, -2,  3,  2
};

static const signed char poly4_offs_table_15[16] = {
   0,  0, -1,  0, -1,  1,  0,  0, -1,  1,  0,  1,  0,  0,  1,  0
};

void dave_channel_pitch(unsigned char c, unsigned char pb)
{
  DaveChannel *chn = dave_channel_ptr(c);
  chn->pitch = (chn->pitch & 0xFF00U) | pb;
}

static unsigned int pitch_to_dave_freq(unsigned int p) __z88dk_fastcall
{
  unsigned char s = 0;
  if (p >= 0x1800) {
    if (p >= 0x8000U)
      return 0x0FFF;
    p = p - 0x1800;
    s = 8;
  }
  else if (p < 1588) {
    return 0x0FFF;
  }
  if (p >= 0x0C00) {
    p = p - 0x0C00;
    s = s | 4;
  }
  if (p >= 0x0600) {
    p = p - 0x0600;
    s = s | 2;
  }
  if (p >= 0x0300) {
    p = p - 0x0300;
    s++;
  }
  p = oct_table[p];
  if (s & 8)
    p = p >> 8;
  if (s & 4)
    p = p >> 4;
  if (s & 2)
    p = p >> 2;
  if (s & 1)
    p = p >> 1;
  p = (p - 1) >> 1;
  return p;
}

/* pb_dist = pitch_bend | (distortion << 8) */

static unsigned int dave_chn_calc_freq(DaveChannel *chn, unsigned int pb_dist)
{
  unsigned int  pitch;
  unsigned char d = (unsigned char) (pb_dist >> 8);
  if (chn == &(dave_chn[3]))
    return (d ^ dave_chn[3].dist);
  pitch = chn->pitch;
  pitch = ((pitch & 0x7F00) >> 2) + (pitch & 0x00FF);
  pitch = pitch + ((int) ((pb_dist + 2048) & 0x0FFF) - 2048);
  pitch = pitch_to_dave_freq(pitch);
  d = (d ^ chn->dist) & 0xF0;
  if ((d & 0x30) == 0x10) {
    unsigned int  n = pitch;
    unsigned char m;
    while (n >= 256)
      n = (n & 255) + (n >> 8);
    m = (unsigned char) n;
    while (m > 15)
      m = (m & 15) + (m >> 4);
    if (chn->dist & 0x08)
      pitch = pitch + poly4_offs_table_5[m];
    else
      pitch = pitch + poly4_offs_table_15[m];
  }
  else if ((d & 0x30) == 0x20) {
    unsigned int  m = pitch;
    while (m > 31)
      m = (m & 31) + ((m << 3) >> 8);   /* m >> 5 */
    if (m == 30)
      pitch++;
  }
  return (pitch | ((unsigned int) d << 8));
}

static void set_dave_registers(const DaveRegisters *r) __z88dk_fastcall __naked
{
  (void) r;
  __asm__ (
      "ld    bc, #0x109f\n"
      "00001$:\n"
      "inc   c\n"
      "outi\n"
      "jp    nz, 00001$\n"
      "ret\n"
  );
}

static const unsigned char  chn_index_table[8] = {
  4, 5, 4, 5, 6, 7, 0, 1
};

static void update_chn_01_index(void)
{
  if (dave_chn[0].env_state & dave_chn[4].env_state & dave_chn[6].env_state
      & 0x80) {
    dave_chn0_index = 0;
  }
  else {
    do {
      dave_chn0_index = chn_index_table[dave_chn0_index];
    } while (dave_channel_ptr(dave_chn0_index)->env_state >= 0x80);
  }
  if (dave_chn[1].env_state & dave_chn[5].env_state & dave_chn[7].env_state
      & 0x80) {
    dave_chn1_index = 1;
  }
  else {
    do {
      dave_chn1_index = chn_index_table[dave_chn1_index];
    } while (dave_channel_ptr(dave_chn1_index)->env_state >= 0x80);
  }
}

void dave_play(void)
{
  DaveChannel   *chn = dave_chn;
  unsigned char c;
  memset_fast(&dave_regs, 0x00, sizeof(DaveRegisters));
  midi_port_read();
  for (c = 0; c < DAVE_VIRT_CHNS; c++, chn++) {
    if (chn->env_state < 0x80) {
      const unsigned char *p = chn->env_ptr;
      unsigned char vol_l = *p;
      unsigned char *vol;
      unsigned int  *freq;
      if (vol_l & 0xC0) {
        if (p[1] == 0xFF) {
          dave_channel_off(c);
          continue;
        }
        if (!(chn->env_state & 0x40)) { /* check loop flags if not releasing */
          switch (vol_l & 0xC0) {
          case 0x40:                    /* begin loop (L) */
            chn->env_ptr = chn->env_ptr + 4;
            chn->env_loop_ptr = p;
            break;
          case 0x80:                    /* end of loop (R) */
            p = chn->env_loop_ptr;
            chn->env_ptr = p + 4;
            vol_l = *p;
            break;
          case 0xC0:                    /* hold single frame (S) */
            chn->env_state = 0x10;
            break;
          }
        }
        else {
          chn->env_ptr = chn->env_ptr + 4;
        }
        vol_l = vol_l & 0x3F;
      }
      else {
        chn->env_ptr = chn->env_ptr + 4;
      }
      chn->env_timer++;
      if (c == 2) {
        freq = &(dave_regs.chn_freq[2]);
        vol = &(dave_regs.volume_l[2]);
      }
      else if (c == 3) {
        freq = &(dave_regs.chn_freq[3]);
        vol = &(dave_regs.volume_l[3]);
      }
      else if (c == dave_chn0_index) {
        freq = &(dave_regs.chn_freq[0]);
        vol = &(dave_regs.volume_l[0]);
      }
      else if (c == dave_chn1_index) {
        freq = &(dave_regs.chn_freq[1]);
        vol = &(dave_regs.volume_l[1]);
      }
      else {
        continue;
      }
      if (((unsigned char *) chn)[13] == 0xFF)  /* chn->vol_l */
        dave_ctrl_update(chn);
      *vol = volume_mult(((unsigned int) chn->vol_l << 8) | vol_l);
      vol[4] = volume_mult(((unsigned int) chn->vol_r << 8) | *(++p));
      p++;
      *freq = dave_chn_calc_freq(chn, *((const unsigned int *) p));
    }
  }
  set_dave_registers(&dave_regs);
  update_chn_01_index();
}
