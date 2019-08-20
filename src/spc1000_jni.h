#ifndef __SPC1000_JNI_H_
#define __SPC1000_JNI_H_

// return: 0 if successful, otherwise -1
int OpenImageDialog(char *name);
int SaveImageDialog(char *name);
int OpenTapeDialog(char *name);
int SaveTapeDialog(char *name);

#endif //  __SPC1000_JNI_H_
