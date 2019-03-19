#ifndef MIDIPLAY_DAVEPLAY_H
#define MIDIPLAY_DAVEPLAY_H

#define DAVE_VIRT_CHNS  8

typedef struct {
  /* b7 = off
   * b6 = releasing
   * b5 = not looped
   * b4 = release enabled
   */
  unsigned char env_state;
  const unsigned char *env_ptr;
  const unsigned char *env_loop_ptr;
  unsigned int  env_timer;
  unsigned int  pitch;
  /* note on velocity */
  unsigned char veloc;
  /* distortion controller */
  unsigned char dist;
  /* pan controller */
  unsigned char pan;
  /* volume controller */
  unsigned char vol;
  /* current left and right volume calculated from velocity and controllers
   * vol_l = 0xFF if volume needs to be updated
   */
  unsigned char vol_l;
  unsigned char vol_r;
  unsigned char aftertouch;
} DaveChannel;

extern DaveChannel  dave_chn[DAVE_VIRT_CHNS];

#define dave_channel_ptr(x)                             \
    ((DaveChannel *) ((unsigned char *) dave_chn        \
                      + (unsigned char) ((x) * sizeof(DaveChannel))))

void dave_init(void);
void dave_reset(void);
void dave_channel_release(DaveChannel *chn) __z88dk_fastcall;
void dave_channel_off(unsigned char c) __z88dk_fastcall;
unsigned char dave_channel_on(unsigned char midi_chn, unsigned char pgm,
                              unsigned int pitch, unsigned char veloc,
                              const unsigned char *ctrls);
void dave_chn_distortion(unsigned char c, unsigned char value);
void dave_assign_channel(unsigned char midi_chn, unsigned char dave_chn);
void dave_chn_set_pan(unsigned char c, unsigned char value);
void dave_chn_set_volume(unsigned char c, unsigned char value);
void dave_chn_aftertouch(unsigned char c, unsigned char value);
void dave_channel_pitch(unsigned char c, unsigned char pb);
void dave_play(void);

#endif  /* MIDIPLAY_DAVEPLAY_H */
