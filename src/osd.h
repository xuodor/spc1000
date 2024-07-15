#ifndef OSD_H_
#define OSD_H_

#include "common.h"
#include "spckey.h"

typedef void (*osd_dlg_callback)(byte *sel_str);

extern byte *osd_;
extern int osd_visible_;

void osd_init();
int osd_should_close_toast();
int osd_dialog_on();
byte osd_data(int addr);
void osd_exit();
void osd_show(int show);
void osd_print(int x, int y, byte *str, int inverse);
void osd_toast(byte *msg, int vloc, int inverse);
void osd_open_dialog(byte *title, byte *globp, osd_dlg_callback cb);
void osd_close_dialog();
void osd_process_key(SDLKey key);

#endif
