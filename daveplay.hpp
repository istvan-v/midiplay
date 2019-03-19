
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

#ifndef MIDICONV_DAVEPLAY_HPP
#define MIDICONV_DAVEPLAY_HPP

class DavePlay {
 protected:
  static const size_t DAVE_VIRT_CHNS = 8;
  // --------
  struct DaveChannel {
    // b7 = off
    // b6 = releasing
    // b5 = not looped
    // b4 = release enabled
    unsigned char env_state;
    const unsigned char *env_ptr;
    const unsigned char *env_loop_ptr;
    unsigned int  env_timer;
    unsigned int  pitch;
    // note on velocity
    unsigned char veloc;
    // distortion controller
    unsigned char dist;
    // pan controller
    unsigned char pan;
    // volume controller
    unsigned char vol;
    // current left and right volume calculated from velocity and controllers
    // vol_l = 0xFF if volume needs to be updated
    unsigned char vol_l;
    unsigned char vol_r;
    unsigned char aftertouch;
  };
  // oct_table[n] =
  //   (unsigned int) (250000.0 / (440.0 * pow(2.0, (n / 64.0 - 71.0) / 12.0))
  //                   + 0.5)
  unsigned int  oct_table[768];
  // sin_table[n] = (int) (sin(n * PI * 0.5 / 255.0) * 181.02 + 0.5)
  unsigned char sin_table[256];
  DaveChannel   dave_chn[DAVE_VIRT_CHNS];
  unsigned char midi_dave_chn[16];
  unsigned char dave_chn0_index;        // 0, 4, 6
  unsigned char dave_chn1_index;        // 1, 5, 7
  unsigned char midi_ctrl_state[16][4];
  unsigned char midi_key_state[2048];
  unsigned int  midi_chn_pitch[16];
  unsigned char midi_chn_program[16];
  unsigned char dave_midi_chn[DAVE_VIRT_CHNS];
  unsigned char midi_pgm_layer2[256];
  unsigned char midi_drum_layer2[256];
  unsigned int  pgm_env_offsets[128];
  unsigned int  drum_env_offsets[128];
  unsigned char envelope_data[8192];
  // --------
  void initTables();
  static inline unsigned char volume_mult(unsigned char v1, unsigned char v2)
  {
    unsigned int  v = ((unsigned int) v1 * (unsigned int) v2 + 64) >> 7;
    if (v < 128)
      return (unsigned char) v;
    return 128;
  }
  void dave_channel_release(DaveChannel *chn);
  void dave_channel_off(unsigned char c);
  DaveChannel *find_best_channel(unsigned char c0, unsigned char c1,
                                 unsigned char c2);
  static void pan_note(DaveChannel *chn, unsigned int pitch);
  static void set_channel_params(DaveChannel *chn, const unsigned char *env_ptr,
                                 unsigned int pitch, unsigned char veloc,
                                 const unsigned char *ctrls);
  unsigned char dave_channel_on(unsigned char midi_chn, unsigned char pgm,
                                unsigned int pitch, unsigned char veloc,
                                const unsigned char *ctrls);
  void dave_chn_distortion(unsigned char c, unsigned char value);
  void dave_assign_channel(unsigned char midi_chn, unsigned char value);
  void dave_chn_set_pan(unsigned char c, unsigned char value);
  void dave_chn_set_volume(unsigned char c, unsigned char value);
  void dave_chn_aftertouch(unsigned char c, unsigned char value);
  void dave_ctrl_update(DaveChannel *chn);
  void dave_channel_pitch(unsigned char c, unsigned char pb);
  unsigned int pitch_to_dave_freq(unsigned int p);
  unsigned int dave_chn_calc_freq(DaveChannel *chn, unsigned int pb_dist);
  void update_chn_01_index();
  void midi_note_off_(unsigned char chn, unsigned char key);
  void midi_note_off(unsigned char chn, unsigned char key);
  void midi_note_on(unsigned char chn, unsigned char key, unsigned char veloc);
  void midi_poly_aft(unsigned char chn, unsigned char key, unsigned char value);
  void midi_chn_reset_ctrl(unsigned char chn);
  void midi_chn_notes_off(unsigned char chn);
  void midi_control_change(unsigned char chn,
                           unsigned char ctrl, unsigned char value);
  void midi_program_change(unsigned char chn, unsigned char pgm);
  void midi_channel_aft(unsigned char chn, unsigned char value);
  void midi_pitch_bend(unsigned char chn, unsigned int pbval);
  void midi_clock();
  void midi_all_notes_off();
  void midi_start();
  void midi_continue();
  void midi_stop();
 public:
  DavePlay();
  virtual ~DavePlay();
  void daveReset();
  void loadEnvelopes(const unsigned char *buf, size_t nBytes);
  void update(unsigned char *dave_regs);
  void midiReset();
  void midiEvent(unsigned char st, unsigned char d1, unsigned char d2);
};

#endif  // MIDICONV_DAVEPLAY_HPP

