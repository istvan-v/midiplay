#include "daveplay.h"
#include "envelope.h"
#include "midi_in.h"
#include "eplib.h"

static unsigned char  file_buf[31744];
static unsigned char  no_file_chooser = 0;

static unsigned char load_midi_file(void)
{
  char    name_buf[256];
  if (!no_file_chooser) {
    exos_irq_handler(1);
    no_file_chooser = get_file_name(name_buf);
    exos_irq_handler(0);
  }
  if (no_file_chooser)
    return midi_file_load("mididata.bin", file_buf, sizeof(file_buf));
  return midi_file_load(name_buf, file_buf, sizeof(file_buf));
}

int main(void)
{
  unsigned char fkeys;
  do {
    if (!load_midi_file())
      load_envelopes("envelope.txt", file_buf, sizeof(file_buf));
    exos_irq_handler(0);
    status_message("Creating tables...");
    dave_init();
    status_message((char *) 0);
    midi_reset();
    set_irq_callback(&dave_play);
    do {
      __asm__ (
          "halt\n"
      );
      dave_keyboard_port = 4;
      fkeys = (dave_keyboard_port ^ 0xFF) & 0x8B;
      if (fkeys) {
        set_irq_callback((void (*)(void)) 0);
        if (fkeys & 0x09) {
          if (fkeys & 0x08)
            midi_file_rewind();
          else
            midi_stop();
          set_irq_callback(&dave_play);
        }
      }
    } while (!(fkeys & 0x82));
    midi_reset();
  } while (!(fkeys & 0x02));
  return 0;
}
