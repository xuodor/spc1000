//
// Java native interface for Spc1000Activity.
//
#include <android/log.h>
#include <assert.h>
#include <inttypes.h>
#include <jni.h>
#include <string.h>

// Android log function wrappers
static const char *kTAG = "SPC1000";
#define LOGI(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Struct containing global objects for interfacing Java layer.
typedef struct Context {
  JavaVM *javaVM;
  jclass activityClass;
  jobject activityObj;
} Context;
Context context_;

typedef int bool;

enum { false = 0, true = 1 };

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  memset(&context_, 0, sizeof(context_));

  context_.javaVM = vm;
  if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR; // JNI version not supported.
  }

  context_.activityObj = NULL;
  return JNI_VERSION_1_6;
}

//
// Initiailizes the native object/structs.
//
JNIEXPORT void JNICALL Java_com_jindor_app_spc1000_Spc1000Activity_nativeInit(
    JNIEnv *env, jobject instance) {
  jclass clz = (*env)->GetObjectClass(env, instance);
  context_.activityClass = (*env)->NewGlobalRef(env, clz);
  context_.activityObj = (*env)->NewGlobalRef(env, instance);

  (void)0;
}

int FileDialog(bool open, char *ext, char *out) {
  JNIEnv *env;
  JavaVM *vm = context_.javaVM;

  if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }
  jmethodID fileDialog =
      (*env)->GetMethodID(env, context_.activityClass, "fileDialog",
                          "(ZLjava/lang/String;)Ljava/lang/String;");
  if (!fileDialog) {
    LOGE("Failed to retrieve fileDialog methodID");
    return -1;
  }
  jstring jext = (*env)->NewStringUTF(env, ext);
  jstring jfilename = (jstring)(*env)->CallObjectMethod(
      env, context_.activityObj, fileDialog, open, jext);
  if (!jfilename) {
    LOGE("File name is not valid");
    return -1;
  }
  const char *filename = (*env)->GetStringUTFChars(env, jfilename, NULL);
  strcpy(out, filename);
  (*env)->ReleaseStringUTFChars(env, jfilename, filename);
  return 0;
}

int OpenImageDialog(char *out) { return FileDialog(true, "", out); }

int SaveImageDialog(char *out) { return FileDialog(false, ".SAV", out); }

int OpenTapeDialog(char *out) { return FileDialog(true, "", out); }

int SaveTapeDialog(char *out) { return FileDialog(false, ".TAP", out); }
