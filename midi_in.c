#include "midi_in.h"
#include "daveplay.h"
#include "envelope.h"
#include "eplib.h"
#include "exos.h"

unsigned char         midi_ctrl_state[16][4];
static unsigned char  midi_key_state[2048];
static unsigned int   midi_chn_pitch[16];
static unsigned char  midi_chn_program[16];
static unsigned char  dave_midi_chn[DAVE_VIRT_CHNS];
unsigned char         midi_pgm_layer2[256];
unsigned char         midi_drum_layer2[256];

#define midi_key_state_ptr(x)   \
    (midi_key_state + (((unsigned int) ((unsigned char) (x)) << 8) >> 1))

static void midi_read_hw(void);
static void midi_read_file(void);

void (*midi_port_read)(void) = &midi_read_hw;

static unsigned char  *midi_file_buf;
static const unsigned char  *midi_file_end;
static const unsigned char  *midi_file_ptr;
static unsigned int   midi_delta_time;
static unsigned char  midi_prv_status;

void midi_reset(void)
{
  unsigned char i;
  for (i = 0; i < DAVE_VIRT_CHNS; i++) {
    dave_channel_off(i);
    dave_midi_chn[i] = 0xFF;
  }
  memset_fast(midi_key_state, 0, sizeof(midi_key_state));
  for (i = 0; i < 16; i++) {
    midi_ctrl_state[i][0] = 0x00;       /* distortion */
    midi_ctrl_state[i][1] = 0;          /* channel allocation control */
    midi_ctrl_state[i][2] = 64;         /* pan */
    midi_ctrl_state[i][3] = 127;        /* volume */
    midi_chn_pitch[i] = 0x0080;
    midi_chn_program[i] = 0;
    dave_assign_channel(i, 0);
  }
  midi_statuscmd_port = 0x00;
}

static void midi_note_off_(unsigned int chn_key) __z88dk_fastcall
{
  DaveChannel   *d;
  unsigned char chn = (unsigned char) (chn_key >> 8);
  unsigned char key = (unsigned char) chn_key;
  unsigned char c = midi_key_state_ptr(chn)[key];
  if (c) {
    midi_key_state_ptr(chn)[key] = 0;
    c--;
    d = dave_channel_ptr(c);
    if (((unsigned char *) d)[8] == key) {      /* chn->pitch MSB */
      dave_midi_chn[c] = 0xFF;
      dave_channel_release(d);
    }
  }
}

void midi_note_off(unsigned char chn, unsigned char key)
{
  const unsigned char *p;
  unsigned char c;
  midi_note_off_(((unsigned int) chn << 8) + key);
  if (chn == 9)
    p = midi_drum_layer2 + (unsigned char) (key << 1);
  else
    p = midi_pgm_layer2 + (unsigned char) (midi_chn_program[chn] << 1);
  c = *p;
  if (c == 0xFF)
    return;
  chn = (chn + c) & 0x0F;
  key = (key + *(++p)) & 0x7F;
  midi_note_off_(((unsigned int) chn << 8) + key);
}

void midi_note_on(unsigned char chn, unsigned char key, unsigned char veloc)
{
  unsigned char is_clone = 0;
  while (1) {
    const unsigned char *p;
    unsigned char c, pgm;
    ((unsigned char *) &(midi_chn_pitch[chn]))[1] = key;
    c = midi_key_state_ptr(chn)[key];
    if (c) {
      dave_channel_off(--c);
      dave_midi_chn[c] = 0xFF;
    }
    pgm = midi_chn_program[chn];
    c = dave_channel_on(chn, pgm, midi_chn_pitch[chn], veloc,
                        midi_ctrl_state[chn]);
    dave_midi_chn[c] = chn;
    midi_key_state_ptr(chn)[key] = c + 1;
    if (is_clone)
      return;
    if (chn == 9)
      p = midi_drum_layer2 + (unsigned char) (key << 1);
    else
      p = midi_pgm_layer2 + (unsigned char) (pgm << 1);
    c = *p;
    if (c == 0xFF)
      return;
    chn = (chn + c) & 0x0F;
    key = (key + *(++p)) & 0x7F;
    is_clone = 1;
  }
}

void midi_poly_aft(unsigned char chn, unsigned char key, unsigned char value)
{
  unsigned char c = midi_key_state_ptr(chn)[key];
  if (c) {
    DaveChannel *d;
    c--;
    d = dave_channel_ptr(c);
    if ((unsigned char) (d->pitch >> 8) == key)
      dave_chn_aftertouch(c, value);
  }
}

static void midi_chn_reset_ctrl(unsigned char chn)
{
  unsigned char c;
  midi_ctrl_state[chn][0] = 0x00;
  midi_ctrl_state[chn][1] = 0;
  midi_ctrl_state[chn][2] = 64;
  midi_ctrl_state[chn][3] = 127;
  midi_chn_pitch[chn] = (midi_chn_pitch[chn] & 0xFF00U) | 0x80;
  for (c = 0; c < DAVE_VIRT_CHNS; c++) {
    if (dave_midi_chn[c] == chn) {
      dave_chn_distortion(c, 0x00);
      dave_chn_set_pan(c, 64);
      dave_chn_set_volume(c, 127);
    }
  }
  dave_assign_channel(chn, 0);
}

static void midi_chn_notes_off(unsigned char chn)
{
  DaveChannel   *d = dave_chn;
  unsigned char c;
  for (c = 0; c < DAVE_VIRT_CHNS; c++, d++) {
    if (dave_midi_chn[c] == chn)
      dave_channel_release(d);
  }
}

void midi_control_change(unsigned char chn,
                         unsigned char ctrl, unsigned char value)
{
  unsigned char c;
  switch (ctrl) {
  case 7:
    midi_ctrl_state[chn][3] = value;
    for (c = 0; c < DAVE_VIRT_CHNS; c++) {
      if (dave_midi_chn[c] == chn)
        dave_chn_set_volume(c, value);
    }
    break;
  case 10:
    midi_ctrl_state[chn][2] = value;
    for (c = 0; c < DAVE_VIRT_CHNS; c++) {
      if (dave_midi_chn[c] == chn)
        dave_chn_set_pan(c, value);
    }
    break;
  case 70:
  case 77:
    midi_ctrl_state[chn][1] = value;
    dave_assign_channel(chn, value);
    break;
  case 71:
  case 76:
    midi_ctrl_state[chn][0] = value;
    for (c = 0; c < DAVE_VIRT_CHNS; c++) {
      if (dave_midi_chn[c] == chn)
        dave_chn_distortion(c, value);
    }
    break;
  case 121:
    midi_chn_reset_ctrl(chn);
    break;
  case 123:
    midi_chn_notes_off(chn);
    break;
  }
}

void midi_program_change(unsigned char chn, unsigned char pgm)
{
  midi_chn_program[chn] = pgm;
}

void midi_channel_aft(unsigned char chn, unsigned char value)
{
  unsigned char c;
  for (c = 0; c < DAVE_VIRT_CHNS; c++) {
    if (dave_midi_chn[c] == chn)
      dave_chn_aftertouch(c, value);
  }
}

void midi_pitch_bend(unsigned char chn, unsigned int pbval)
{
  unsigned char c;
  unsigned char pb = (unsigned char) ((pbval << 2) >> 8);
  *((unsigned char *) &(midi_chn_pitch[chn])) = pb;
  for (c = 0; c < DAVE_VIRT_CHNS; c++) {
    if (dave_midi_chn[c] == chn)
      dave_channel_pitch(c, pb);
  }
}

void midi_clock(void)
{
}

static void midi_all_notes_off(void)
{
  DaveChannel   *d = dave_chn;
  unsigned char i;
  for (i = 0; i < DAVE_VIRT_CHNS; i++, d++) {
    dave_midi_chn[i] = 0xFF;
    dave_channel_release(d);
  }
  memset_fast(midi_key_state, 0, sizeof(midi_key_state));
}

void midi_start(void)
{
}

void midi_continue(void)
{
}

void midi_stop(void)
{
  midi_all_notes_off();
}

static void midi_read_hw(void)
{
  while (midi_statuscmd_port < 0x80) {
    unsigned char st = midi_data_port;
    unsigned char d1 = 0x00;
    unsigned char d2 = 0x00;
    if (st < 0x80)
      continue;
    if (st < 0xF0) {
      d1 = midi_data_port & 0x7F;
      if (st < 0xC0 || st >= 0xE0)
        d2 = midi_data_port & 0x7F;
      switch (st & 0xF0) {
      case 0x80:
        midi_note_off(st & 0x0F, d1);
        break;
      case 0x90:
        if (!d2)
          midi_note_off(st & 0x0F, d1);
        else
          midi_note_on(st & 0x0F, d1, d2);
        break;
      case 0xA0:
        midi_poly_aft(st & 0x0F, d1, d2);
        break;
      case 0xB0:
        midi_control_change(st & 0x0F, d1, d2);
        break;
      case 0xC0:
        midi_program_change(st & 0x0F, d1);
        break;
      case 0xD0:
        midi_channel_aft(st & 0x0F, d1);
        break;
      case 0xE0:
        midi_pitch_bend(st & 0x0F, (((unsigned int) d2 << 8) >> 1) | d1);
        break;
      }
    }
    else {
      switch (st) {
      case 0xF8:
        midi_clock();
        break;
      case 0xFA:
        midi_start();
        break;
      case 0xFB:
        midi_continue();
        break;
      case 0xFC:
        midi_stop();
        break;
      }
    }
  }
}

static void midi_file_reset(void)
{
  midi_file_ptr = midi_file_buf;
  midi_delta_time = 0;
  midi_prv_status = 0x00;
  midi_reset();
}

static void midi_file_dtime(void)
{
  const unsigned char *p = midi_file_ptr;
  unsigned int  dt;
  if (p >= midi_file_end) {
    midi_file_reset();
    p = midi_file_ptr;
  }
  dt = *(p++);
  if (dt & 0x80)
    dt = (((dt & 0x7F) << 8) >> 1) | *(p++);
  midi_file_ptr = p;
  midi_delta_time = dt;
}

void midi_file_rewind(void)
{
  midi_file_reset();
  if (midi_port_read == &midi_read_file)
    midi_file_dtime();
}

static void midi_read_file(void)
{
  if (midi_delta_time) {
    midi_delta_time--;
    return;
  }
  do {
    unsigned char st, d1, d2;
    st = *(midi_file_ptr++);
    d2 = 0x00;
    if (st < 0x80) {
      d1 = st;
      st = midi_prv_status;
    }
    else {
      midi_prv_status = st;
      d1 = *(midi_file_ptr++) & 0x7F;
    }
    if ((st & 0xE0) != 0xC0)
      d2 = *(midi_file_ptr++) & 0x7F;
    switch (st & 0xF0) {
    case 0x80:
      midi_note_off(st & 0x0F, d1);
      break;
    case 0x90:
      if (!d2)
        midi_note_off(st & 0x0F, d1);
      else
        midi_note_on(st & 0x0F, d1, d2);
      break;
    case 0xA0:
      midi_poly_aft(st & 0x0F, d1, d2);
      break;
    case 0xB0:
      midi_control_change(st & 0x0F, d1, d2);
      break;
    case 0xC0:
      midi_program_change(st & 0x0F, d1);
      break;
    case 0xD0:
      midi_channel_aft(st & 0x0F, d1);
      break;
    case 0xE0:
      midi_pitch_bend(st & 0x0F, (((unsigned int) d2 << 8) >> 1) | d1);
      break;
    }
    midi_file_dtime();
  } while (!midi_delta_time);
  midi_delta_time--;
}

static void midi_file_read_blk(void *buf, unsigned int nbytes)
{
  if (exos_read_block(1, buf, nbytes) != nbytes)
    error_exit("Error reading MIDI file");
}

unsigned char midi_file_load(const char *file_name,
                             unsigned char *file_buf,
                             unsigned int file_buf_size)
{
  unsigned int  env_size, midi_size;
  midi_port_read = &midi_read_hw;
  exos_irq_handler(1);
  if (exos_open_channel(1, file_name) != 0) {
    exos_close_channel(1);
    exos_irq_handler(0);
    return 0;
  }
  if (exos_read_block(1, file_buf, 16) < 10)
    error_exit("Error reading MIDI file header");
  if (*((unsigned int *) file_buf) != 0x6D00) { /* 'm' */
    exos_close_channel(1);
    exos_irq_handler(0);
    load_envelopes("envelope.txt", file_buf, file_buf_size);
    exos_irq_handler(1);
    if (exos_open_channel(1, file_name) != 0) {
      exos_close_channel(1);
      exos_irq_handler(0);
      return 0;
    }
    midi_size = exos_read_block(1, file_buf, file_buf_size);
    if (midi_size < 3 || midi_size >= file_buf_size)
      error_exit("Invalid MIDI data size in MIDI file");
  }
  else {
    env_size = ((unsigned int *) file_buf)[2];
    midi_size = ((unsigned int *) file_buf)[3];
    if (env_size < (1024 + 6) || env_size > (1024 + ENV_BUF_SIZE))
      error_exit("Invalid envelope data size in MIDI file");
    if (midi_size < 3 || midi_size >= file_buf_size)
      error_exit("Invalid MIDI data size in MIDI file");
    midi_file_read_blk(midi_pgm_layer2, 256);
    midi_file_read_blk(midi_drum_layer2, 256);
    midi_file_read_blk(pgm_env_offsets, sizeof(unsigned int) * 128);
    midi_file_read_blk(drum_env_offsets, sizeof(unsigned int) * 128);
    midi_file_read_blk(envelope_data, env_size - 1024);
    midi_file_read_blk(file_buf, midi_size);
  }
  exos_close_channel(1);
  exos_irq_handler(0);
  midi_file_buf = file_buf;
  midi_file_ptr = file_buf;
  midi_file_end = file_buf + midi_size;
  midi_file_dtime();
  midi_prv_status = 0x00;
  midi_port_read = &midi_read_file;
  return 1;
}
