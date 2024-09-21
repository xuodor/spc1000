#include "osd.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "sysdep.h"
#include "MC6847.h"

#define MAXFL 8

int osd_visible_;
int64_t osd_toast_begin_ms_;
int osd_dialog_;

glob_t globbuf_;
char *osd_dlg_fnames_;
int osd_dlg_cnt_;
int osd_dlg_sel_;
int osd_dlg_top_;
int osd_dlg_max_ = 8; /* max visible lines */
char *osd_dlg_sel_str_;
osd_dlg_callback osd_dlg_cb_;

/*
 * OSD superimposed on MC6847.
 */

void osd_init() {
  osd_dlg_sel_str_ = (char *)malloc(256);
  osd_toast_begin_ms_ = -1;
  osd_visible_ = 0;
  osd_dialog_ = 0;
}

void osd_print(int x, int y, char *str, int inverse) {
  byte *p = vdg_text_pos(x, y);
  while (*str) {
    *p = *str;
    if (inverse) p[0x800] |= 0x01;
    else p[0x800] &= ~0x01;
    str++;
    p++;
  }
}

int osd_is_toast_on() {
  return osd_toast_begin_ms_ > 0;
}

int osd_should_close_toast() {
  if (!osd_is_toast_on()) return 0;

  int64_t t = get_timestamp_ms();
  return t - osd_toast_begin_ms_ > 2000; /* 2 secs */
}

/**
 * Display a modeless toast message on OSD.
 *
 * @param str Message string. Null-terminated.
 * @param vloc 0 - center
 *             1 - bottom of the screen
 * @param inverse Inverse attr
 */
void osd_toast(char *str, int vloc, int inverse) {
  int len;

  if (osd_is_toast_on()) return;

  len = strlen(str);
  if (len > 30) str[31] = 0;

  osd_show(1);

  int x = (32-len)/2-1;
  int y = (16-1)/2-1;
  int flen = len+2+1;
  char *ur = (char*)calloc(1, len+2+1);
  char *mr = (char*)calloc(1, len+2+1);
  char *dr = (char*)calloc(1, len+2+1);

  /* draw border */
  memset(ur, 0x8B, len+2);
  memset(dr, 0x8B, len+2);
  mr[0] = mr[len+1] = 0x8A;
  ur[0] = 0x87;
  ur[len+1] = 0x85;
  dr[0] = 0x88;
  dr[len+1] = 0x86;

  memcpy(mr+1, str, len);

  osd_print(x, y, ur, 0);
  osd_print(x, y+1, mr, 0);
  osd_print(x, y+2, dr, 0);
  free(ur);
  free(mr);
  free(dr);

  osd_toast_begin_ms_ = get_timestamp_ms();
}

void osd_exit() {
  free(osd_dlg_sel_str_);
}

char *_osd_dlg_fstr(int ind) {
  return osd_dlg_fnames_ + ind*9;
}

int osd_dialog_on() {
  return osd_dialog_;
}

void _osd_dlg_update() {
  for (int i = 0; i< MIN(osd_dlg_max_, osd_dlg_cnt_); ++i) {
    int ind = osd_dlg_top_+i;
    int inv = (osd_dlg_sel_ == ind) ? 1: 0;
    char *p = _osd_dlg_fstr(ind);
    char buf[16];
    memset(buf, ' ', MAXFL);
    buf[MAXFL] = 0;
    memcpy(buf, p, strlen(p));
    osd_print(12, 3+i, buf, inv);
  }
}

/**
 * Open modal dialog.
 */
void osd_open_dialog(char *title, char *globp, osd_dlg_callback cb) {
  glob(globp, 0, 0, &globbuf_);
  int nfiles = globbuf_.gl_pathc;
  if (nfiles == 0) {
    cb(NULL, "NO FILES");
    return;
  }
  osd_dlg_cb_ = cb;
  osd_show(1);
  char tline[128] = "\x87\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x85";
  int tpos = (strlen(tline)-strlen(title))/2;
  if (tpos > 0) memcpy(tline+tpos, title, strlen(title));
  osd_print(10, 2, tline, 0);
  osd_print(10, 3, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10, 4, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10, 5, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10, 6, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10, 7, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10, 8, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10, 9, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10,10, "\x8a\x20\x20\x20    \x20\x20\x20\x8a",0);
  osd_print(10,11, "\x8f\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8e",0);
  osd_print(10,12, "\x8a\x5e\x83 ESC/RET\x8a",0);
  osd_print(10,13, "\x88\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x8b\x86",0);
  osd_dialog_ = 1;

  osd_dlg_fnames_ = (char*)calloc(1, nfiles*(MAXFL+1));
  for (int i = 0; i < nfiles; ++i) {
    char n[256];
    strcpy(n, globbuf_.gl_pathv[i]);
    n[strlen(n)-4] = '\0'; /* remove file extension */
    n[MAXFL] = '\0'; /* truncate up to MAXFL chars */
    strncpy(&osd_dlg_fnames_[i*(MAXFL+1)], n, MAXFL);
  }
  if (osd_dlg_cnt_ != nfiles) osd_dlg_sel_ = 0;
  osd_dlg_cnt_ = nfiles;

  _osd_dlg_update();
}

void osd_close_dialog() {
  if (!osd_dialog_on()) return;
  osd_show(0);
  osd_dialog_ = 0;
  globfree(&globbuf_);
  free(osd_dlg_fnames_);
}

void osd_process_key(KeyCode key) {
  if (key == VK_ESCAPE || key == VK_RETURN) {
    if (key == VK_RETURN) {
      strcpy(osd_dlg_sel_str_, globbuf_.gl_pathv[osd_dlg_sel_]);
    } else {
      *osd_dlg_sel_str_ = '\0';
    }
    osd_close_dialog();

    osd_dlg_cb_(osd_dlg_sel_str_, "CANCELED");

  } else if (key == VK_UP) {
    if (osd_dlg_sel_ == 0) return;
    --osd_dlg_sel_;
    if (osd_dlg_sel_ < osd_dlg_top_) {
      osd_dlg_top_ = MAX(osd_dlg_top_-osd_dlg_max_/2, 0);
    }
    _osd_dlg_update();
  } else if (key == VK_DOWN) {
    if (osd_dlg_sel_ == osd_dlg_cnt_-1) return;
    ++osd_dlg_sel_;
    if (osd_dlg_sel_ > osd_dlg_top_ + osd_dlg_max_-1) {
      osd_dlg_top_ = MIN(osd_dlg_top_+osd_dlg_max_/2, osd_dlg_cnt_-osd_dlg_max_);
    }
    _osd_dlg_update();
  }
}

void osd_show(int show) {
  if (show == osd_visible_ || (show && !vdg_display_char())) return;

  if (show) {
    vdg_save_page();
  } else {
    vdg_restore_page();
    osd_toast_begin_ms_ = -1;
  }
  osd_visible_ = show;
  UpdateMC6847Text(MC6847_UPDATEALL);
}
