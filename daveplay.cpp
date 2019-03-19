
// midiconv: converts MIDI files to Enterprise midiplay format
// Copyright (C) 2017 Istvan Varga <istvanv@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "daveplay.hpp"

#define ENABLE_CHN1_ALLOC       1

// freq_mult_table[n] =
//     (unsigned int) (65536.0 * pow(0.5, 1.0 / (3.0 * (1 << n))) + 0.5)

static const unsigned int freq_mult_table[9] = {
  52016, 58386, 61858, 63670, 64596, 65065, 65300, 65418, 65477
};

DavePlay::DavePlay()
{
  initTables();
  loadEnvelopes((unsigned char *) 0, 0);
  daveReset();
  midiReset();
}

DavePlay::~DavePlay()
{
}

void DavePlay::initTables()
{
  unsigned int  j, k, f;
  unsigned char s;
  oct_table[0] = 34323U;
  std::memset(&(oct_table[1]), 0x00, sizeof(unsigned int) * 767);
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
}

void DavePlay::daveReset()
{
  std::memset(dave_chn, 0x00, sizeof(dave_chn));
  for (size_t c = 0; c < DAVE_VIRT_CHNS; c++) {
    dave_chn[c].env_state = 0xA0;
    dave_chn[c].pitch = 0x0080;
    dave_chn[c].veloc = 127;
    dave_chn[c].pan = 64;
    dave_chn[c].vol = 127;
    dave_chn[c].vol_l = 128;
    dave_chn[c].vol_r = 128;
  }
  std::memset(midi_dave_chn, 0, 16);
  dave_chn0_index = 0;
  dave_chn1_index = 1;
}

void DavePlay::loadEnvelopes(const unsigned char *buf, size_t nBytes)
{
  std::memset(midi_pgm_layer2, 0xFF, sizeof(midi_pgm_layer2));
  std::memset(midi_drum_layer2, 0xFF, sizeof(midi_drum_layer2));
  for (size_t i = 0; i < 128; i++) {
    pgm_env_offsets[i] = 0xA000U;
    drum_env_offsets[i] = 0xA000U;
  }
  std::memset(envelope_data, 0x00, sizeof(envelope_data));
  if (buf != (unsigned char *) 0 && nBytes > 0) {
    if (nBytes > (1024 + 8192))
      nBytes = 1024 + 8192;
    nBytes = nBytes & ~(size_t(1));
    for (size_t i = 0; i < nBytes; i++) {
      if (i < 0x0100)
        midi_pgm_layer2[i] = buf[i];
      else if (i < 0x0200)
        midi_drum_layer2[i & 0xFF] = buf[i];
      else if (i < 0x0300 && (i & 1) == 0)
        pgm_env_offsets[(i & 0xFF) >> 1] = (unsigned int) buf[i];
      else if (i < 0x0300)
        pgm_env_offsets[(i & 0xFF) >> 1] |= ((unsigned int) buf[i] << 8);
      else if (i < 0x0400 && (i & 1) == 0)
        drum_env_offsets[(i & 0xFF) >> 1] = (unsigned int) buf[i];
      else if (i < 0x0400)
        drum_env_offsets[(i & 0xFF) >> 1] |= ((unsigned int) buf[i] << 8);
      else
        envelope_data[i - 0x0400] = buf[i];
    }
  }
  envelope_data[sizeof(envelope_data) - 2] = 0x80;
}

void DavePlay::dave_channel_release(DaveChannel *chn)
{
  if (chn->env_state & 0x10) {
    /* release */
    chn->env_state = 0x60;
    if (*(chn->env_ptr) >= 0xC0)
      chn->env_ptr = chn->env_ptr + 4;
    chn->env_timer = 0;
  }
  else if (chn != &(dave_chn[3])) {
    chn->env_state = 0xA0;
    chn->env_timer = 0;
  }
}

void DavePlay::dave_channel_off(unsigned char c)
{
  dave_chn[c].env_state = 0xA0;
  dave_chn[c].env_timer = 0;
}

DavePlay::DaveChannel *
    DavePlay::find_best_channel(unsigned char c0, unsigned char c1,
                                unsigned char c2)
{
  DaveChannel *chn0 = &(dave_chn[c0]);
  DaveChannel *chn1 = &(dave_chn[c1]);
  unsigned char env0 = chn0->env_state & 0xE0;
  unsigned char env1 = chn1->env_state & 0xE0;
  if (env0 < env1) {
    env0 = env1;
    chn0 = chn1;
  }
  else if (env0 == env1 && chn0->env_timer < chn1->env_timer) {
    chn0 = chn1;
  }
  chn1 = &(dave_chn[c2]);
  env1 = chn1->env_state & 0xE0;
  if (env0 < env1)
    return chn1;
  if (env0 == env1 && chn0->env_timer < chn1->env_timer)
    return chn1;
  return chn0;
}

void DavePlay::pan_note(DaveChannel *chn, unsigned int pitch)
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

void DavePlay::set_channel_params(DaveChannel *chn,
                                  const unsigned char *env_ptr,
                                  unsigned int pitch, unsigned char veloc,
                                  const unsigned char *ctrls)
{
  chn->env_ptr = env_ptr;
  chn->env_loop_ptr = (unsigned char *) 0;
  chn->env_timer = 0;
  chn->pitch = pitch;
  chn->veloc = veloc;
  chn->dist = ctrls[0] << 2;
  chn->pan = ctrls[2];
  chn->vol = ctrls[3];
  chn->vol_l = 0xFF;
  chn->vol_r = 0xFF;
  chn->aftertouch = 0;
}

unsigned char DavePlay::dave_channel_on(unsigned char midi_chn,
                                        unsigned char pgm,
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
      chn = &(dave_chn[c - 1]);
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
  c = (unsigned char) (chn - dave_chn);
  return c;
}

void DavePlay::dave_chn_distortion(unsigned char c, unsigned char value)
{
  dave_chn[c].dist = (value & 0x3F) << 2;
}

void DavePlay::dave_assign_channel(unsigned char midi_chn, unsigned char value)
{
  midi_dave_chn[midi_chn] = (value & 0x1C) >> 2;
}

void DavePlay::dave_chn_set_pan(unsigned char c, unsigned char value)
{
  dave_chn[c].pan = value;
  dave_chn[c].vol_l = 0xFF;
}

void DavePlay::dave_chn_set_volume(unsigned char c, unsigned char value)
{
  dave_chn[c].vol = value;
  dave_chn[c].vol_l = 0xFF;
}

void DavePlay::dave_chn_aftertouch(unsigned char c, unsigned char value)
{
  dave_chn[c].veloc += (value - dave_chn[c].aftertouch);
  dave_chn[c].aftertouch = value;
  dave_chn[c].vol_l = 0xFF;
}

void DavePlay::dave_ctrl_update(DaveChannel *chn)
{
  unsigned char v, vol_l, vol_r;
  v = volume_mult(chn->vol + 1, chn->veloc + 1);
  vol_l = sin_table[(unsigned char) (255 - (chn->pan << 1))];
  vol_r = sin_table[(unsigned char) chn->pan << 1];
  chn->vol_l = volume_mult(v, vol_l);
  chn->vol_r = volume_mult(v, vol_r);
}

void DavePlay::dave_channel_pitch(unsigned char c, unsigned char pb)
{
  dave_chn[c].pitch = (dave_chn[c].pitch & 0xFF00U) | pb;
}

unsigned int DavePlay::pitch_to_dave_freq(unsigned int p)
{
  if (p < 1588 || p >= 0x8000U)
    return 0x0FFF;
  unsigned char s = (unsigned char) (p / 0x0300);
  p = p % 0x0300;
  p = oct_table[p];
  p = ((p >> s) - 1) >> 1;
  return p;
}

static const signed char poly4_offs_table_5[16] = {
   2,  1,  0, -1,  1,  0, -1,  1,  0, -1,  1,  0, -1, -2,  3,  2
};

static const signed char poly4_offs_table_15[16] = {
   0,  0, -1,  0, -1,  1,  0,  0, -1,  1,  0,  1,  0,  0,  1,  0
};

// pb_dist = pitch_bend | (distortion << 8)

unsigned int DavePlay::dave_chn_calc_freq(DaveChannel *chn,
                                          unsigned int pb_dist)
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
    unsigned char m = (unsigned char) (pitch % 15);
    if (chn->dist & 0x08)
      pitch = pitch + poly4_offs_table_5[m];
    else
      pitch = pitch + poly4_offs_table_15[m];
  }
  else if ((d & 0x30) == 0x20) {
    unsigned char m = (unsigned char) (pitch % 31);
    if (m == 30)
      pitch++;
  }
  return (pitch | ((unsigned int) d << 8));
}

static const unsigned char  chn_index_table[8] = {
  4, 5, 4, 5, 6, 7, 0, 1
};

void DavePlay::update_chn_01_index()
{
  if (dave_chn[0].env_state & dave_chn[4].env_state & dave_chn[6].env_state
      & 0x80) {
    dave_chn0_index = 0;
  }
  else {
    do {
      dave_chn0_index = chn_index_table[dave_chn0_index];
    } while (dave_chn[dave_chn0_index].env_state >= 0x80);
  }
  if (dave_chn[1].env_state & dave_chn[5].env_state & dave_chn[7].env_state
      & 0x80) {
    dave_chn1_index = 1;
  }
  else {
    do {
      dave_chn1_index = chn_index_table[dave_chn1_index];
    } while (dave_chn[dave_chn1_index].env_state >= 0x80);
  }
}

void DavePlay::update(unsigned char *dave_regs)
{
  std::memset(dave_regs, 0x00, 16);
  DaveChannel   *chn = dave_chn;
  for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++, chn++) {
    if (chn->env_state < 0x80) {
      const unsigned char *p = chn->env_ptr;
      unsigned char vol_l = *p;
      unsigned char *vol_ptr, *freq_ptr;
      unsigned int  freq;
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
      if (c == 2 || c == 3) {
        freq_ptr = dave_regs + (c << 1);
        vol_ptr = dave_regs + (8 + c);
      }
      else if (c == dave_chn0_index || c == dave_chn1_index) {
        freq_ptr = dave_regs + ((c & 1) << 1);
        vol_ptr = dave_regs + (8 + (c & 1));
      }
      else {
        continue;
      }
      if (chn->vol_l == 0xFF)
        dave_ctrl_update(chn);
      vol_ptr[0] = volume_mult(chn->vol_l, vol_l);
      vol_ptr[4] = volume_mult(chn->vol_r, p[1]);
      freq = (unsigned int) p[2] | ((unsigned int) p[3] << 8);
      freq = dave_chn_calc_freq(chn, freq);
      freq_ptr[0] = (unsigned char) (freq & 0xFF);
      freq_ptr[1] = (unsigned char) (freq >> 8);
    }
  }
  update_chn_01_index();
}

void DavePlay::midiReset()
{
  for (unsigned char i = 0; i < DAVE_VIRT_CHNS; i++) {
    dave_channel_off(i);
    dave_midi_chn[i] = 0xFF;
  }
  std::memset(midi_key_state, 0, sizeof(midi_key_state));
  for (unsigned char i = 0; i < 16; i++) {
    midi_ctrl_state[i][0] = 0x00;       // distortion
    midi_ctrl_state[i][1] = 0;          // channel allocation control
    midi_ctrl_state[i][2] = 64;         // pan
    midi_ctrl_state[i][3] = 127;        // volume
    midi_chn_pitch[i] = 0x0080;
    midi_chn_program[i] = 0;
    dave_assign_channel(i, 0);
  }
}

void DavePlay::midi_note_off_(unsigned char chn, unsigned char key)
{
  unsigned char c = midi_key_state[((unsigned int) chn << 7) | key];
  if (c) {
    midi_key_state[((unsigned int) chn << 7) | key] = 0;
    c--;
    DaveChannel *d = &(dave_chn[c]);
    if ((unsigned char) (d->pitch >> 8) == key) {       // chn->pitch MSB
      dave_midi_chn[c] = 0xFF;
      dave_channel_release(d);
    }
  }
}

void DavePlay::midi_note_off(unsigned char chn, unsigned char key)
{
  const unsigned char *p;
  unsigned char c;
  midi_note_off_(chn, key);
  if (chn == 9)
    p = midi_drum_layer2 + (unsigned char) (key << 1);
  else
    p = midi_pgm_layer2 + (unsigned char) (midi_chn_program[chn] << 1);
  c = *p;
  if (c == 0xFF)
    return;
  chn = (chn + c) & 0x0F;
  key = (key + *(++p)) & 0x7F;
  midi_note_off_(chn, key);
}

void DavePlay::midi_note_on(unsigned char chn,
                            unsigned char key, unsigned char veloc)
{
  unsigned char is_clone = 0;
  while (1) {
    const unsigned char *p;
    unsigned char c, pgm;
    midi_chn_pitch[chn] =
        (midi_chn_pitch[chn] & 0x00FF) | ((unsigned int) key << 8);
    c = midi_key_state[((unsigned int) chn << 7) | key];
    if (c) {
      dave_channel_off(--c);
      dave_midi_chn[c] = 0xFF;
    }
    pgm = midi_chn_program[chn];
    c = dave_channel_on(chn, pgm, midi_chn_pitch[chn], veloc,
                        midi_ctrl_state[chn]);
    dave_midi_chn[c] = chn;
    midi_key_state[((unsigned int) chn << 7) | key] = c + 1;
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

void DavePlay::midi_poly_aft(unsigned char chn,
                             unsigned char key, unsigned char value)
{
  unsigned char c = midi_key_state[((unsigned int) chn << 7) | key];
  if (c) {
    c--;
    DaveChannel *d = &(dave_chn[c]);
    if ((unsigned char) (d->pitch >> 8) == key)
      dave_chn_aftertouch(c, value);
  }
}

void DavePlay::midi_chn_reset_ctrl(unsigned char chn)
{
  midi_ctrl_state[chn][0] = 0x00;
  midi_ctrl_state[chn][1] = 0;
  midi_ctrl_state[chn][2] = 64;
  midi_ctrl_state[chn][3] = 127;
  midi_chn_pitch[chn] = (midi_chn_pitch[chn] & 0xFF00U) | 0x80;
  for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++) {
    if (dave_midi_chn[c] == chn) {
      dave_chn_distortion(c, 0x00);
      dave_chn_set_pan(c, 64);
      dave_chn_set_volume(c, 127);
    }
  }
  dave_assign_channel(chn, 0);
}

void DavePlay::midi_chn_notes_off(unsigned char chn)
{
  DaveChannel *d = dave_chn;
  for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++, d++) {
    if (dave_midi_chn[c] == chn)
      dave_channel_release(d);
  }
}

void DavePlay::midi_control_change(unsigned char chn,
                                   unsigned char ctrl, unsigned char value)
{
  switch (ctrl) {
  case 7:
    midi_ctrl_state[chn][3] = value;
    for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++) {
      if (dave_midi_chn[c] == chn)
        dave_chn_set_volume(c, value);
    }
    break;
  case 10:
    midi_ctrl_state[chn][2] = value;
    for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++) {
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
    for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++) {
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

void DavePlay::midi_program_change(unsigned char chn, unsigned char pgm)
{
  midi_chn_program[chn] = pgm;
}

void DavePlay::midi_channel_aft(unsigned char chn, unsigned char value)
{
  for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++) {
    if (dave_midi_chn[c] == chn)
      dave_chn_aftertouch(c, value);
  }
}

void DavePlay::midi_pitch_bend(unsigned char chn, unsigned int pbval)
{
  unsigned char pb = (unsigned char) (pbval >> 6);
  midi_chn_pitch[chn] = (midi_chn_pitch[chn] & 0xFF00U) | pb;
  for (unsigned char c = 0; c < DAVE_VIRT_CHNS; c++) {
    if (dave_midi_chn[c] == chn)
      dave_channel_pitch(c, pb);
  }
}

void DavePlay::midi_clock()
{
}

void DavePlay::midi_all_notes_off()
{
  DaveChannel *d = dave_chn;
  for (unsigned char i = 0; i < DAVE_VIRT_CHNS; i++, d++) {
    dave_midi_chn[i] = 0xFF;
    dave_channel_release(d);
  }
  std::memset(midi_key_state, 0, sizeof(midi_key_state));
}

void DavePlay::midi_start()
{
}

void DavePlay::midi_continue()
{
}

void DavePlay::midi_stop()
{
  midi_all_notes_off();
}

void DavePlay::midiEvent(unsigned char st, unsigned char d1, unsigned char d2)
{
  if (st < 0x80)
    return;
  if (st < 0xF0) {
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
      midi_pitch_bend(st & 0x0F, ((unsigned int) d2 << 7) | d1);
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

