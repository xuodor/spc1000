#ifndef OSD_H_
#define OSD_H_

#include "common.h"
#include "key.h"

typedef void (*osd_dlg_callback)(char *sel, char *errmsg);

extern byte *osd_;
extern int osd_visible_;

void osd_init();
int osd_should_close_toast();
int osd_dialog_on();
byte osd_data(int addr);
void osd_exit();
void osd_show(int show);
void osd_print(int x, int y, char *str, int inverse);
void osd_toast(char *msg, int vloc, int inverse);
void osd_open_dialog(char *title, char *globp, osd_dlg_callback cb);
void osd_close_dialog();
void osd_process_key(KeyCode key);

#endif
