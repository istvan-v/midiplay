#ifndef MIDIPLAY_MIDI_IN_H
#define MIDIPLAY_MIDI_IN_H

extern unsigned char midi_ctrl_state[16][4];
/* 128 * (channel offset, pitch offset), 0xFF, 0xFF = no second layer */
extern unsigned char midi_pgm_layer2[256];
extern unsigned char midi_drum_layer2[256];

__sfr __at 0xF6 midi_statuscmd_port;
__sfr __at 0xF7 midi_data_port;

void midi_reset(void);

void midi_note_off(unsigned char chn, unsigned char key);
void midi_note_on(unsigned char chn, unsigned char key, unsigned char veloc);
void midi_poly_aft(unsigned char chn, unsigned char key, unsigned char value);
void midi_control_change(unsigned char chn,
                         unsigned char ctrl, unsigned char value);
void midi_program_change(unsigned char chn, unsigned char pgm);
void midi_channel_aft(unsigned char chn, unsigned char value);
void midi_pitch_bend(unsigned char chn, unsigned int pbval);
void midi_clock(void);
void midi_start(void);
void midi_continue(void);
void midi_stop(void);

extern void (*midi_port_read)(void);
unsigned char midi_file_load(const char *file_name,
                             unsigned char *file_buf,
                             unsigned int file_buf_size);
void midi_file_rewind(void);

#endif  /* MIDIPLAY_MIDI_IN_H */
