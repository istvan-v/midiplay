
#ifndef EXOS_H_INCLUDED
#define EXOS_H_INCLUDED

unsigned char exos_open_channel(unsigned char n, const char *fname);
unsigned char exos_create_channel(unsigned char n, const char *fname);
unsigned char exos_close_channel(unsigned char n);
int exos_read_byte(unsigned char chn);
unsigned char exos_write_byte(unsigned char chn, unsigned char b);
unsigned int exos_read_block(unsigned char chn,
                             void *buf, unsigned int nbytes);
unsigned int exos_write_block(unsigned char chn,
                              const void *buf, unsigned int nbytes);
unsigned char exos_special_func(unsigned char chn, unsigned char func,
                                unsigned char param1, unsigned char param2,
                                unsigned char param3);
int exos_channel_read_status(unsigned char chn);
unsigned char exos_set_variable(unsigned char n, unsigned char value);
int exos_get_variable(unsigned char n);
/* status * 256 + segment, < 0: error, > 255: shared */
int exos_alloc(void);
unsigned char exos_free(unsigned char segment);

void print_chn(unsigned char chn, const char *s);
void print_string(const char *s);

#endif

