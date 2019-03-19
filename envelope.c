#include "envelope.h"
#include "midi_in.h"
#include "eplib.h"
#include "exos.h"

unsigned char       envelope_data[ENV_BUF_SIZE];
unsigned int        pgm_env_offsets[128];
unsigned int        drum_env_offsets[128];

static const char   *file_buf_ptr;

static void strip_space(char *file_buf, unsigned int fsize)
{
  char    *s = file_buf;
  char    *t = s;
  unsigned char comment_flag = 0;
  for ( ; fsize != 0; fsize--, s++) {
    char    c = *s;
    if (comment_flag && c != '\r' && c != '\n' && c != '\0')
      continue;
    comment_flag = 0;
    if ((unsigned char) c <= (unsigned char) ' ')
      continue;
    if (c == '#') {
      comment_flag = 1;
      continue;
    }
    *(t++) = c;
  }
  *t = '\0';
}

static char read_char(void)
{
  char    c = *file_buf_ptr;
  if ((c >= '0' && c <= '9') || c == '+' || c == '-')
    return '0';
  if (c)
    file_buf_ptr++;
  return c;
}

static int read_number(void)
{
  int     n;
  char    is_negative = 0;
  char    c;
  c = *file_buf_ptr;
  if (c == '+' || c == '-') {
    is_negative = (c == '-');
    c = *(++file_buf_ptr);
  }
  if (!c)
    error_exit("Unexpected end of envelope file");
  if (c < '0' || c > '9')
    error_exit("Invalid number format in envelope file");
  n = c - '0';
  while (1) {
    c = *(++file_buf_ptr);
    if (c < '0' || c > '9')
      break;
    n = n * 10 + (c - '0');
  }
  if (is_negative)
    n = -n;
  return n;
}

typedef struct {
  unsigned int  vol_l;
  int           vol_l_inc;
  unsigned char vol_l_mult;
  unsigned int  vol_r;
  int           vol_r_inc;
  unsigned char vol_r_mult;
  long          pb;
  long          pb_inc;
  unsigned char dist;
  unsigned char d;
} EnvelopeState;

static void update_envelope(EnvelopeState *env, unsigned char *p)
{
  if (p > &(envelope_data[ENV_BUF_SIZE - 4]))
    error_exit("Envelope buffer overflow");
  p[0] = (unsigned char) (env->vol_l >> 8);
  p[1] = (unsigned char) (env->vol_r >> 8);
  p[2] = (unsigned char) ((unsigned int) env->pb >> 8);
  p[3] = env->dist | (unsigned char) ((env->pb >> 16) & 0x0F);
  env->d--;
  env->vol_l = env->vol_l & 0x3FFF;
  if (env->vol_l_mult) {
    env->vol_l = (unsigned int) (((unsigned long) env->vol_l
                                  * env->vol_l_mult + 64) >> 7);
  }
  else {
    env->vol_l = env->vol_l + env->vol_l_inc;
  }
  if (env->vol_l >= 0x8000U)
    env->vol_l = 0;
  else if (env->vol_l >= 0x4000)
    env->vol_l = 0x3FFF;
  env->vol_r = env->vol_r & 0x3FFF;
  if (env->vol_r_mult) {
    env->vol_r = (unsigned int) (((unsigned long) env->vol_r
                                  * env->vol_r_mult + 64) >> 7);
  }
  else {
    env->vol_r = env->vol_r + env->vol_r_inc;
  }
  if (env->vol_r >= 0x8000U)
    env->vol_r = 0;
  else if (env->vol_r >= 0x4000)
    env->vol_r = 0x3FFF;
  env->pb = env->pb + env->pb_inc;
}

static void parse_volume_l(EnvelopeState *env)
{
  int     n;
  char    c = read_char();
  char    mult_flag = (c == '*');
  if (mult_flag)
    c = read_char();
  if (c != '0')
    error_exit("Syntax error in envelope segment");
  n = read_number();
  if (n < (mult_flag ? 1 : 0) || n >= (mult_flag ? 256 : 64))
    error_exit("Invalid left volume in envelope file");
  if (mult_flag) {
    env->vol_l_mult = (unsigned char) n;
  }
  else {
    env->vol_l_mult = 0;
    if (!env->d) {
      env->vol_l = (env->vol_l & 0xC000U) | ((unsigned int) n << 8) | 0x0080;
    }
    else {
      n = ((n << 8) | 0x0080) - (int) (env->vol_l & 0x3FFF);
      if (n < 0)
        n = n - (env->d >> 1);
      else
        n = n + (env->d >> 1);
      env->vol_l_inc = n / env->d;
    }
  }
}

static void parse_volume_r(EnvelopeState *env)
{
  int     n;
  char    c = read_char();
  char    mult_flag = (c == '*');
  if (mult_flag)
    c = read_char();
  if (c != '0')
    error_exit("Syntax error in envelope segment");
  n = read_number();
  if (n < (mult_flag ? 1 : 0) || n >= (mult_flag ? 256 : 64))
    error_exit("Invalid right volume in envelope file");
  if (mult_flag) {
    env->vol_r_mult = (unsigned char) n;
  }
  else {
    env->vol_r_mult = 0;
    if (!env->d) {
      env->vol_r = ((unsigned int) n << 8) | 0x0080;
    }
    else {
      n = ((n << 8) | 0x0080) - (int) (env->vol_r & 0x3FFF);
      if (n < 0)
        n = n - (env->d >> 1);
      else
        n = n + (env->d >> 1);
      env->vol_r_inc = n / env->d;
    }
  }
}

static void parse_pitch_bend(EnvelopeState *env, unsigned char is_drum)
{
  long    pb;
  int     n;
  if (read_char() != '0')
    error_exit("Syntax error in envelope segment");
  n = read_number();
  if (n < -2048 || n > 2047 || (is_drum && n != 0))
    error_exit("Invalid pitch bend in envelope file");
  pb = ((long) n << 8) | 0x0080;
  env->pb_inc = 0;
  if (!env->d) {
    env->pb = pb;
  }
  else if (pb != env->pb) {
    pb = pb - env->pb;
    if (pb < 0)
      pb = pb - (env->d >> 1);
    else
      pb = pb + (env->d >> 1);
    env->pb_inc = pb / env->d;
  }
}

static void parse_instr_layer2(int n)
{
  int     c, p;
  unsigned char *ptr;
  if (read_char() != '0')
    error_exit("Syntax error in envelope file");
  c = read_number();
  if (c < -15 || c > 15)
    error_exit("Invalid channel offset in envelope file");
  if (read_char() != ',' || read_char() != '0')
    error_exit("Syntax error in envelope file");
  p = read_number();
  if (p < -127 || p > 127)
    error_exit("Invalid pitch offset in envelope file");
  n = n << 1;
  if (n < 0)
    ptr = midi_drum_layer2 - n;
  else
    ptr = midi_pgm_layer2 + n;
  *(ptr++) = (unsigned char) c & 0x0F;
  *ptr = (unsigned char) p & 0x7F;
}

unsigned int compile_envelopes(char *file_buf, unsigned int fsize)
{
  EnvelopeState env;
  unsigned char *p = envelope_data;
  char    c;
  int     n;

  strip_space(file_buf, fsize);
  file_buf_ptr = file_buf;
  memset_fast(pgm_env_offsets, 0x80, sizeof(unsigned int) * 128);
  memset_fast(drum_env_offsets, 0x80, sizeof(unsigned int) * 128);
  memset_fast(midi_pgm_layer2, 0xFF, 256);
  memset_fast(midi_drum_layer2, 0xFF, 256);
  while ((c = read_char()) != '\0') {
    int     *instr_list = (int *) file_buf;
    unsigned char env_flags = 0x20;     /* 0x20 = no sustain, 0x10 = release */
    unsigned char is_drum = 0;
    while (1) {
      n = read_number();
      if (n < -127 || n > 127)
        error_exit("Invalid program number in envelope file");
      if (instr_list > (int *) file_buf && (unsigned char) (n < 0) != is_drum)
        error_exit("Instrument type error in envelope file");
      is_drum = (unsigned char) (n < 0);
      c = read_char();
      *(instr_list++) = n;
      {
        unsigned int  env_pos = (unsigned int) (p - envelope_data) >> 1;
        if (n == 9)
          env_pos |= 0x4000;
        while ((c | 0x20) == 'p' || (c | 0x20) == 'd') {
          if ((c | 0x20) == 'p')
            env_pos ^= 0x4000;
          else
            env_flags = 0x30;
          c = read_char();
        }
        if (!is_drum)
          pgm_env_offsets[n] = env_pos;
        else
          drum_env_offsets[-n] = env_pos;
      }
      if (c == ':') {
        parse_instr_layer2(n);
        c = read_char();
      }
      if (c == '{')
        break;
      if (c != ',')
        error_exit("Syntax error in envelope file");
    }
    env.vol_l = 0x0080;
    env.vol_l_inc = 0;
    env.vol_l_mult = 0;
    env.vol_r = 0x0080;
    env.vol_r_inc = 0;
    env.vol_r_mult = 0;
    env.pb = 0x0080;
    env.pb_inc = 0;
    env.dist = 0x00;
    env.d = 0;
    while (1) {
      if (env.d) {
        update_envelope(&env, p);
        p = p + 4;
        continue;
      }
      c = read_char();
      if (c == '}')
        break;
      /* duration */
      if (read_char() != '0')
        error_exit("Syntax error in envelope segment");
      n = read_number();
      if (n < 0 || n > 255)
        error_exit("Invalid envelope segment duration");
      env.d = (unsigned char) n;
      if (c != '0' && (unsigned char) n) {
        switch (c | 0x20) {
        case 'l':                       /* begin loop */
          if (env_flags != 0x20)
            error_exit("Invalid loop in envelope file");
          env.vol_l |= 0x4000U;
          env_flags = 0x00;
          c = '0';
          break;
        case 'r':                       /* end loop, release */
          if (env_flags == 0x10)
            error_exit("Invalid loop in envelope file");
          env.vol_l |= (env_flags ? 0xC000U : 0x8000U);
          env_flags = 0x10;
          c = '0';
          break;
        case 's':                       /* hold single frame, release */
          if (env_flags != 0x20)
            error_exit("Invalid loop in envelope file");
          /* flags are not set for compatibility */
          env.vol_l |= 0xC000U;
          c = '0';
          break;
        }
      }
      if (c != '0' || read_char() != ',')
        error_exit("Syntax error in envelope segment");
      parse_volume_l(&env);
      if (read_char() != ',')
        error_exit("Syntax error in envelope segment");
      parse_volume_r(&env);
      if (read_char() != ',')
        error_exit("Syntax error in envelope segment");
      parse_pitch_bend(&env, is_drum);
      /* distortion */
      if (read_char() != ',')
        error_exit("Syntax error in envelope segment");
      if (read_char() != '0')
        error_exit("Syntax error in envelope segment");
      n = read_number();
      if (n < 0 || n > 255 || (n & (is_drum ? 0x00 : 0x0F)) != 0)
        error_exit("Invalid distortion in envelope file");
      env.dist = (unsigned char) n;
      if (read_char() != ';')
        error_exit("Syntax error in envelope segment");
    }
    if (p > &(envelope_data[ENV_BUF_SIZE - 2]))
      error_exit("Envelope buffer overflow");
    p[0] = 0x80;
    p[1] = (!env_flags ? 0x00 : 0xFF);
    p = p + 2;
    while (instr_list > (int *) file_buf) {
      n = *(--instr_list);
      if (n >= 0)
        pgm_env_offsets[n] |= ((unsigned int) env_flags << 8);
      else
        drum_env_offsets[-n] |= ((unsigned int) env_flags << 8);
    }
  }
  return (unsigned int) (p - envelope_data);
}

void load_envelopes(const char *file_name,
                    unsigned char *file_buf, unsigned int file_buf_size)
{
  unsigned int  n;
  exos_irq_handler(1);
  if (exos_open_channel(1, file_name) != 0)
    error_exit("Error opening envelope file");
  n = exos_read_block(1, file_buf, file_buf_size);
  if (n < 10 || n >= file_buf_size)
    error_exit("Invalid envelope file size");
  exos_close_channel(1);
  exos_irq_handler(0);
  status_message("Compiling envelopes...");
  n = compile_envelopes((char *) file_buf, n);
  status_message((char *) 0);
  if (n) {
    exos_irq_handler(1);
    if (exos_create_channel(1, "envelope.bin") == 0) {
      exos_write_block(1, midi_pgm_layer2, 256);
      exos_write_block(1, midi_drum_layer2, 256);
      exos_write_block(1, pgm_env_offsets, sizeof(unsigned int) * 128);
      exos_write_block(1, drum_env_offsets, sizeof(unsigned int) * 128);
      exos_write_block(1, envelope_data, n);
    }
    exos_close_channel(1);
    exos_irq_handler(0);
  }
}
