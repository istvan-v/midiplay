#ifndef MIDIPLAY_ENVELOPE_H
#define MIDIPLAY_ENVELOPE_H

#define ENV_BUF_SIZE    8192

extern unsigned char  envelope_data[ENV_BUF_SIZE];
extern unsigned int   pgm_env_offsets[128];
extern unsigned int   drum_env_offsets[128];

unsigned int compile_envelopes(char *file_buf, unsigned int fsize);
void load_envelopes(const char *file_name,
                    unsigned char *file_buf, unsigned int file_buf_size);

#endif  /* MIDIPLAY_ENVELOPE_H */
