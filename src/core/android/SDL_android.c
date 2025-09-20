/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#include "SDL_stdinc.h"
#include "SDL_atomic.h"
#include "SDL_hints.h"
#include "SDL_main.h"
#include "SDL_timer.h"
#include "SDL_version.h"

#ifdef __ANDROID__

#include "SDL_system.h"
#include "SDL_android.h"

#include "../../events/SDL_events_c.h"
#include "../../video/android/SDL_androidkeyboard.h"
#include "../../video/android/SDL_androidmouse.h"
#include "../../video/android/SDL_androidtouch.h"
#include "../../video/android/SDL_androidvideo.h"
#include "../../video/android/SDL_androidwindow.h"
#include "../../joystick/android/SDL_sysjoystick_c.h"
#include "../../haptic/android/SDL_syshaptic_c.h"

#include <android/log.h>
#include <android/configuration.h>
#include <android/asset_manager_jni.h>
#include <sys/system_properties.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

#define SDL_JAVA_PREFIX                               org_libsdl_app
#define CONCAT1(prefix, class, function)              CONCAT2(prefix, class, function)
#define CONCAT2(prefix, class, function)              Java_##prefix##_##class##_##function
#define SDL_JAVA_INTERFACE(function)                  CONCAT1(SDL_JAVA_PREFIX, SDLActivity, function)
#define SDL_JAVA_AUDIO_INTERFACE(function)            CONCAT1(SDL_JAVA_PREFIX, SDLAudioManager, function)
#define SDL_JAVA_CONTROLLER_INTERFACE(function)       CONCAT1(SDL_JAVA_PREFIX, SDLControllerManager, function)
#define SDL_JAVA_INTERFACE_INPUT_CONNECTION(function) CONCAT1(SDL_JAVA_PREFIX, SDLInputConnection, function)

#define TAG "SDL"

// Have error log always available
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifdef DEBUG
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#else
#define LOGV(...)
#define LOGD(...)
#define LOGI(...)
#endif

#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

/* Audio encoding definitions */
#define ENCODING_PCM_8BIT  3
#define ENCODING_PCM_16BIT 2
#define ENCODING_PCM_FLOAT 4

/* Java class SDLActivity */
JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetVersion)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetupJNI)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeRunMain)(
    JNIEnv *env, jclass cls,
    jstring library, jstring function, jobject array);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDropFile)(
    JNIEnv *env, jclass jcls,
    jstring filename);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetScreenResolution)(
    JNIEnv *env, jclass jcls,
    jint surfaceWidth, jint surfaceHeight,
    jint deviceWidth, jint deviceHeight, jfloat rate);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeResize)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceCreated)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceChanged)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyDown)(
    JNIEnv *env, jclass jcls,
    jint keycode);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyUp)(
    JNIEnv *env, jclass jcls,
    jint keycode);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(onNativeSoftReturnKey)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeTouch)(
    JNIEnv *env, jclass jcls,
    jint touch_device_id_in, jint pointer_finger_id_in,
    jint action, jfloat x, jfloat y, jfloat p);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeMouse)(
    JNIEnv *env, jclass jcls,
    jint button, jint action, jfloat x, jfloat y, jboolean relative);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeAccel)(
    JNIEnv *env, jclass jcls,
    jfloat x, jfloat y, jfloat z);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeClipboardChanged)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeLowMemory)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeLocaleChanged)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSendQuit)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeQuit)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePause)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeResume)(
    JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeFocusChanged)(
    JNIEnv *env, jclass cls, jboolean hasFocus);

JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetHint)(
    JNIEnv *env, jclass cls,
    jstring name);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeGetHintBoolean)(
    JNIEnv *env, jclass cls,
    jstring name, jboolean default_value);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetenv)(
    JNIEnv *env, jclass cls,
    jstring name, jstring value);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeOrientationChanged)(
    JNIEnv *env, jclass cls,
    jint orientation);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeAddTouch)(
    JNIEnv *env, jclass cls,
    jint touchId, jstring name);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePermissionResult)(
    JNIEnv *env, jclass cls,
    jint requestCode, jboolean result);

static JNINativeMethod SDLActivity_tab[] = {
    { "nativeGetVersion", "()Ljava/lang/String;", SDL_JAVA_INTERFACE(nativeGetVersion) },
    { "nativeSetupJNI", "()V", SDL_JAVA_INTERFACE(nativeSetupJNI) },
    { "nativeRunMain", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V", SDL_JAVA_INTERFACE(nativeRunMain) },
    { "onNativeDropFile", "(Ljava/lang/String;)V", SDL_JAVA_INTERFACE(onNativeDropFile) },
    { "nativeSetScreenResolution", "(IIIIF)V", SDL_JAVA_INTERFACE(nativeSetScreenResolution) },
    { "onNativeResize", "()V", SDL_JAVA_INTERFACE(onNativeResize) },
    { "onNativeSurfaceCreated", "()V", SDL_JAVA_INTERFACE(onNativeSurfaceCreated) },
    { "onNativeSurfaceChanged", "()V", SDL_JAVA_INTERFACE(onNativeSurfaceChanged) },
    { "onNativeSurfaceDestroyed", "()V", SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed) },
    { "onNativeKeyDown", "(I)V", SDL_JAVA_INTERFACE(onNativeKeyDown) },
    { "onNativeKeyUp", "(I)V", SDL_JAVA_INTERFACE(onNativeKeyUp) },
    { "onNativeSoftReturnKey", "()Z", SDL_JAVA_INTERFACE(onNativeSoftReturnKey) },
    { "onNativeKeyboardFocusLost", "()V", SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost) },
    { "onNativeTouch", "(IIIFFF)V", SDL_JAVA_INTERFACE(onNativeTouch) },
    { "onNativeMouse", "(IIFFZ)V", SDL_JAVA_INTERFACE(onNativeMouse) },
    { "onNativeAccel", "(FFF)V", SDL_JAVA_INTERFACE(onNativeAccel) },
    { "onNativeClipboardChanged", "()V", SDL_JAVA_INTERFACE(onNativeClipboardChanged) },
    { "nativeLowMemory", "()V", SDL_JAVA_INTERFACE(nativeLowMemory) },
    { "onNativeLocaleChanged", "()V", SDL_JAVA_INTERFACE(onNativeLocaleChanged) },
    { "nativeSendQuit", "()V", SDL_JAVA_INTERFACE(nativeSendQuit) },
    { "nativeQuit", "()V", SDL_JAVA_INTERFACE(nativeQuit) },
    { "nativePause", "()V", SDL_JAVA_INTERFACE(nativePause) },
    { "nativeResume", "()V", SDL_JAVA_INTERFACE(nativeResume) },
    { "nativeFocusChanged", "(Z)V", SDL_JAVA_INTERFACE(nativeFocusChanged) },
    { "nativeGetHint", "(Ljava/lang/String;)Ljava/lang/String;", SDL_JAVA_INTERFACE(nativeGetHint) },
    { "nativeGetHintBoolean", "(Ljava/lang/String;Z)Z", SDL_JAVA_INTERFACE(nativeGetHintBoolean) },
    { "nativeSetenv", "(Ljava/lang/String;Ljava/lang/String;)V", SDL_JAVA_INTERFACE(nativeSetenv) },
    { "onNativeOrientationChanged", "(I)V", SDL_JAVA_INTERFACE(onNativeOrientationChanged) },
    { "nativeAddTouch", "(ILjava/lang/String;)V", SDL_JAVA_INTERFACE(nativeAddTouch) },
    { "nativePermissionResult", "(IZ)V", SDL_JAVA_INTERFACE(nativePermissionResult) }
};

/* Java class SDLInputConnection */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText)(
    JNIEnv *env, jclass cls,
    jstring text, jint newCursorPosition);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar)(
    JNIEnv *env, jclass cls,
    jchar chUnicode);

static JNINativeMethod SDLInputConnection_tab[] = {
    { "nativeCommitText", "(Ljava/lang/String;I)V", SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText) },
    { "nativeGenerateScancodeForUnichar", "(C)V", SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar) }
};

/* Java class SDLAudioManager */
JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI)(
    JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL
    SDL_JAVA_AUDIO_INTERFACE(addAudioDevice)(JNIEnv *env, jclass jcls, jboolean is_capture,
                                             jint device_id);

JNIEXPORT void JNICALL
    SDL_JAVA_AUDIO_INTERFACE(removeAudioDevice)(JNIEnv *env, jclass jcls, jboolean is_capture,
                                                jint device_id);

static JNINativeMethod SDLAudioManager_tab[] = {
    { "nativeSetupJNI", "()V", SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI) },
    { "addAudioDevice", "(ZI)V", SDL_JAVA_AUDIO_INTERFACE(addAudioDevice) },
    { "removeAudioDevice", "(ZI)V", SDL_JAVA_AUDIO_INTERFACE(removeAudioDevice) }
};

/* Java class SDLControllerManager */
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI)(
    JNIEnv *env, jclass jcls);

JNIEXPORT jint JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode);

JNIEXPORT jint JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint axis, jfloat value);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint hat_id, jint x, jint y);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name, jstring device_desc, jint vendor_id, jint product_id,
    jint button_mask, jint naxes, jint axis_mask, jint nhats, jboolean can_rumble);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic)(
    JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic)(
    JNIEnv *env, jclass jcls,
    jint device_id);

static JNINativeMethod SDLControllerManager_tab[] = {
    { "nativeSetupJNI", "()V", SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI) },
    { "onNativePadDown", "(II)I", SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown) },
    { "onNativePadUp", "(II)I", SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp) },
    { "onNativeJoy", "(IIF)V", SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy) },
    { "onNativeHat", "(IIII)V", SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat) },
    { "nativeAddJoystick", "(ILjava/lang/String;Ljava/lang/String;IIIIIIZ)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick) },
    { "nativeRemoveJoystick", "(I)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick) },
    { "nativeAddHaptic", "(ILjava/lang/String;)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic) },
    { "nativeRemoveHaptic", "(I)V", SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic) }
};

/* Uncomment this to log messages entering and exiting methods in this file */
/* #define DEBUG_JNI */

static void checkJNIReady(void);

/*******************************************************************************
 This file links the Java side of Android with libsdl
*******************************************************************************/
#include <jni.h>

/*******************************************************************************
                               Globals
*******************************************************************************/
static pthread_key_t mThreadKey;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static JavaVM *mJavaVM = NULL;

/* Main activity */
static jclass mActivityClass;

/* method signatures */
typedef struct {
    const char *name;
    const char *signature;
} function_definition;

typedef enum {
    SDLActivity_clipboardGetText,
    SDLActivity_clipboardHasText,
    SDLActivity_clipboardSetText,
    SDLActivity_createCustomCursor,
    SDLActivity_destroyCustomCursor,
    SDLActivity_getContext,
    SDLActivity_getDisplayDPI,
    SDLActivity_getManifestEnvironmentVariables,
    SDLActivity_getNativeSurface,
    SDLActivity_initTouch,
    SDLActivity_isAndroidTV,
    SDLActivity_isChromebook,
    SDLActivity_isDeXMode,
    SDLActivity_isScreenKeyboardShown,
    SDLActivity_isTablet,
    SDLActivity_manualBackButton,
    SDLActivity_minimizeWindow,
    SDLActivity_openURL,
    SDLActivity_requestPermission,
    SDLActivity_showToast,
    SDLActivity_sendMessage,
    SDLActivity_setActivityTitle,
    SDLActivity_setCustomCursor,
    SDLActivity_setOrientation,
    SDLActivity_setRelativeMouseEnabled,
    SDLActivity_setSystemCursor,
    SDLActivity_setWindowStyle,
    SDLActivity_shouldMinimizeOnFocusLoss,
    SDLActivity_showTextInput,
    SDLActivity_supportsRelativeMouse,
    SDL_JavaFuncs_count
} SDL_Java_funcs_enum; 
static jmethodID jnicall[SDL_JavaFuncs_count];

static const function_definition SDLActivity_ifc[] = {
    { "clipboardGetText", "()Ljava/lang/String;" },
    { "clipboardHasText", "()Z" },
    { "clipboardSetText", "(Ljava/lang/String;)V" },
    { "createCustomCursor", "([IIIII)I" },
    { "destroyCustomCursor", "(I)V" },
    { "getContext", "()Landroid/content/Context;" },
    { "getDisplayDPI", "()Landroid/util/DisplayMetrics;" },
    { "getManifestEnvironmentVariables", "()Z" },
    { "getNativeSurface", "()Landroid/view/Surface;" },
    { "initTouch", "()V" },
    { "isAndroidTV", "()Z" },
    { "isChromebook", "()Z" },
    { "isDeXMode", "()Z" },
    { "isScreenKeyboardShown", "()Z" },
    { "isTablet", "()Z" },
    { "manualBackButton", "()V" },
    { "minimizeWindow", "()V" },
    { "openURL", "(Ljava/lang/String;)I" },
    { "requestPermission", "(Ljava/lang/String;I)V" },
    { "showToast", "(Ljava/lang/String;IIII)I" },
    { "sendMessage", "(II)Z" },
    { "setActivityTitle", "(Ljava/lang/String;)Z" },
    { "setCustomCursor", "(I)Z" },
    { "setOrientation", "(IIZLjava/lang/String;)V" },
    { "setRelativeMouseEnabled", "(Z)Z" },
    { "setSystemCursor", "(I)Z" },
    { "setWindowStyle", "(Z)V" },
    { "shouldMinimizeOnFocusLoss", "()Z" },
    { "showTextInput", "(IIII)Z" },
    { "supportsRelativeMouse", "()Z" },
};
SDL_COMPILE_TIME_ASSERT(activities_funcs, SDL_arraysize(SDLActivity_ifc) == (int)SDL_JavaFuncs_count);

/* audio manager */
static jclass mAudioManagerClass;

/* method signatures */
typedef enum {
    SDLAudio_audioDetectDevices,
    SDLAudio_audioOpen,
    SDLAudio_audioWriteByteBuffer,
    SDLAudio_audioWriteShortBuffer,
    SDLAudio_audioWriteFloatBuffer,
    SDLAudio_audioClose,
    SDLAudio_captureOpen,
    SDLAudio_captureReadByteBuffer,
    SDLAudio_captureReadShortBuffer,
    SDLAudio_captureReadFloatBuffer,
    SDLAudio_captureClose,
    SDLAudio_audioSetThreadPriority,
    SDL_AudioFuncs_count
} SDL_Audio_funcs_enum; 
static jmethodID jnicall_audio[SDL_AudioFuncs_count];

static const function_definition SDLAudioManager_ifc[] = {
    { "audioDetectDevices", "()V" },
    { "audioOpen", "(IIIII)[I" },
    { "audioWriteByteBuffer", "([B)V" },
    { "audioWriteShortBuffer", "([S)V" },
    { "audioWriteFloatBuffer", "([F)V" },
    { "audioClose", "()V" },
    { "captureOpen", "(IIIII)[I" },
    { "captureReadByteBuffer", "([BZ)I" },
    { "captureReadShortBuffer", "([SZ)I" },
    { "captureReadFloatBuffer", "([FZ)I" },
    { "captureClose", "()V" },
    { "audioSetThreadPriority", "(ZI)V" },
};
SDL_COMPILE_TIME_ASSERT(audio_funcs, SDL_arraysize(SDLAudioManager_ifc) == (int)SDL_AudioFuncs_count);

/* controller manager */
static jclass mControllerManagerClass;

/* method signatures */
typedef enum {
    SDLController_joystickSubscribe,
    SDLController_joystickUnsubscribe,
    SDLController_joystickRumble,
    SDLController_pollHapticDevices,
    SDLController_hapticRun,
    SDLController_hapticStop,
    SDL_ControllerFuncs_count
} SDL_ControllerFuncs_enum;
static jmethodID jnicall_ctrl[SDL_ControllerFuncs_count];

static const function_definition SDLControllerManager_ifc[] = {
    { "joystickSubscribe", "()V" },
    { "joystickUnsubscribe", "()V" },
    { "joystickRumble", "(IFFI)V" },
    { "pollHapticDevices", "()V" },
    { "hapticRun", "(IFI)V" },
    { "hapticStop", "(I)V" },
};
SDL_COMPILE_TIME_ASSERT(controller_funcs, SDL_arraysize(SDLControllerManager_ifc) == (int)SDL_ControllerFuncs_count);

/* Accelerometer data storage */
static SDL_DisplayOrientation displayOrientation;
static float fLastAccelerometer[3];
static SDL_bool bHasNewData;

static SDL_bool bHasEnvironmentVariables;

static SDL_atomic_t bPermissionRequestPending;
static SDL_bool bPermissionRequestResult;

/* Android AssetManager */
static void Internal_Android_Create_AssetManager(void);
static void Internal_Android_Destroy_AssetManager(void);
static AAssetManager *asset_manager = NULL;
static jobject javaAssetManagerRef = 0;

/*******************************************************************************
                 Functions called by JNI
*******************************************************************************/

/* From http://developer.android.com/guide/practices/jni.html
 * All threads are Linux threads, scheduled by the kernel.
 * They're usually started from managed code (using Thread.start), but they can also be created elsewhere and then
 * attached to the JavaVM. For example, a thread started with pthread_create can be attached with the
 * JNI AttachCurrentThread or AttachCurrentThreadAsDaemon functions. Until a thread is attached, it has no JNIEnv,
 * and cannot make JNI calls.
 * Attaching a natively-created thread causes a java.lang.Thread object to be constructed and added to the "main"
 * ThreadGroup, making it visible to the debugger. Calling AttachCurrentThread on an already-attached thread
 * is a no-op.
 * Note: You can call this function any number of times for the same thread, there's no harm in it
 */

/* From http://developer.android.com/guide/practices/jni.html
 * Threads attached through JNI must call DetachCurrentThread before they exit. If coding this directly is awkward,
 * in Android 2.0 (Eclair) and higher you can use pthread_key_create to define a destructor function that will be
 * called before the thread exits, and call DetachCurrentThread from there. (Use that key with pthread_setspecific
 * to store the JNIEnv in thread-local-storage; that way it'll be passed into your destructor as the argument.)
 * Note: The destructor is not called unless the stored value is != NULL
 * Note: You can call this function any number of times for the same thread, there's no harm in it
 *       (except for some lost CPU cycles)
 */

/* Set local storage value */
static int Android_JNI_SetEnv(JNIEnv *env)
{
    int status = pthread_setspecific(mThreadKey, env);
    if (status < 0) {
        LOGE("Failed pthread_setspecific() in Android_JNI_SetEnv() (err=%d)", status);
    }
    return status;
}

/* Get local storage value */
JNIEnv *Android_JNI_GetEnv(void)
{
    /* Get JNIEnv from the Thread local storage */
    JNIEnv *env = pthread_getspecific(mThreadKey);
    if (!env) {
        /* If it fails, try to attach ! (e.g the thread isn't created with SDL_CreateThread() */
        int status;

        /* There should be a JVM */
        if (!mJavaVM) {
            LOGE("Failed, there is no JavaVM");
            return NULL;
        }

        /* Attach the current thread to the JVM and get a JNIEnv.
         * It will be detached by pthread_create destructor 'Android_JNI_ThreadDestroyed' */
        status = (*mJavaVM)->AttachCurrentThread(mJavaVM, &env, NULL);
        if (status < 0) {
            LOGE("Failed to attach current thread (err=%d)", status);
            return NULL;
        }

        /* Save JNIEnv into the Thread local storage */
        if (Android_JNI_SetEnv(env) < 0) {
            return NULL;
        }
    }

    return env;
}

/* Set up an external thread for using JNI with Android_JNI_GetEnv() */
int Android_JNI_SetupThread(void)
{
    JNIEnv *env;
    int status;

    /* There should be a JVM */
    if (!mJavaVM) {
        LOGE("Failed, there is no JavaVM");
        return 0;
    }

    /* Attach the current thread to the JVM and get a JNIEnv.
     * It will be detached by pthread_create destructor 'Android_JNI_ThreadDestroyed' */
    status = (*mJavaVM)->AttachCurrentThread(mJavaVM, &env, NULL);
    if (status < 0) {
        LOGE("Failed to attach current thread (err=%d)", status);
        return 0;
    }

    /* Save JNIEnv into the Thread local storage */
    if (Android_JNI_SetEnv(env) < 0) {
        return 0;
    }

    return 1;
}

/* Destructor called for each thread where mThreadKey is not NULL */
static void Android_JNI_ThreadDestroyed(void *value)
{
    /* The thread is being destroyed, detach it from the Java VM and set the mThreadKey value to NULL as required */
    JNIEnv *env = (JNIEnv *)value;
    if (env) {
        (*mJavaVM)->DetachCurrentThread(mJavaVM);
        Android_JNI_SetEnv(NULL);
    }
}

/* Creation of local storage mThreadKey */
static void Android_JNI_CreateKey(void)
{
    int status = pthread_key_create(&mThreadKey, Android_JNI_ThreadDestroyed);
    if (status < 0) {
        LOGE("Error initializing mThreadKey with pthread_key_create() (err=%d)", status);
    }
}

static void Android_JNI_CreateKey_once(void)
{
    int status = pthread_once(&key_once, Android_JNI_CreateKey);
    if (status < 0) {
        LOGE("Error initializing mThreadKey with pthread_once() (err=%d)", status);
    }
}

static void register_methods(JNIEnv *env, const char *classname, JNINativeMethod *methods, int nb)
{
    jclass clazz = (*env)->FindClass(env, classname);
    if (!clazz || (*env)->RegisterNatives(env, clazz, methods, nb) < 0) {
        LOGE("Failed to register methods of %s", classname);
        return;
    }
}

/* Library init */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;

    mJavaVM = vm;

    if ((*mJavaVM)->GetEnv(mJavaVM, (void **)&env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("Failed to get JNI Env");
        return JNI_VERSION_1_4;
    }

    register_methods(env, "org/libsdl/app/SDLActivity", SDLActivity_tab, SDL_arraysize(SDLActivity_tab));
    register_methods(env, "org/libsdl/app/SDLInputConnection", SDLInputConnection_tab, SDL_arraysize(SDLInputConnection_tab));
    register_methods(env, "org/libsdl/app/SDLAudioManager", SDLAudioManager_tab, SDL_arraysize(SDLAudioManager_tab));
    register_methods(env, "org/libsdl/app/SDLControllerManager", SDLControllerManager_tab, SDL_arraysize(SDLControllerManager_tab));

    return JNI_VERSION_1_4;
}

void checkJNIReady(void)
{
    if (!mActivityClass || !mAudioManagerClass || !mControllerManagerClass) {
        /* We aren't fully initialized, let's just return. */
        return;
    }

    SDL_SetMainReady();
}

/* Get SDL version -- called before SDL_main() to verify JNI bindings */
JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetVersion)(JNIEnv *env, jclass cls)
{
    char version[128];

    SDL_snprintf(version, sizeof(version), "%d.%d.%d", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);

    return (*env)->NewStringUTF(env, version);
}

/* Activity initialization -- called before SDL_main() to initialize JNI bindings */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls)
{
    LOGV("nativeSetupJNI()");

    /*
     * Create mThreadKey so we can keep track of the JNIEnv assigned to each thread
     * Refer to http://developer.android.com/guide/practices/design/jni.html for the rationale behind this
     */
    Android_JNI_CreateKey_once();

    /* Save JNIEnv of SDLActivity */
    Android_JNI_SetEnv(env);

    if (!mJavaVM) {
        LOGE("failed to found a JavaVM");
    }

    /* Use a mutex to prevent concurrency issues between Java Activity and Native thread code, when using 'Android_Window'.
     * (Eg. Java sending Touch events, while native code is destroying the main SDL_Window. )
     */
    if (!Android_ActivityMutex) {
        Android_ActivityMutex = SDL_CreateMutex(); /* Could this be created twice if onCreate() is called a second time ? */
    }

    if (!Android_ActivityMutex) {
        LOGE("failed to create Android_ActivityMutex mutex");
    }

    Android_PauseSem = SDL_CreateSemaphore(0);
    if (!Android_PauseSem) {
        LOGE("failed to create Android_PauseSem semaphore");
    }

    Android_ResumeSem = SDL_CreateSemaphore(0);
    if (!Android_ResumeSem) {
        LOGE("failed to create Android_ResumeSem semaphore");
    }

    mActivityClass = (jclass)((*env)->NewGlobalRef(env, cls));

    for (int i = 0; i < SDL_JavaFuncs_count; i++) {
        jnicall[i] = (*env)->GetStaticMethodID(env, mActivityClass, SDLActivity_ifc[i].name, SDLActivity_ifc[i].signature);
    }
    for (int i = 0; i < SDL_JavaFuncs_count; i++) {
        if (!jnicall[i]) {
            LOGW("Missing some Java callbacks, do you have the latest version of SDLActivity.java? (idx=%d)", i);
            break;
        }
    }

    checkJNIReady();
}

/* Audio initialization -- called before SDL_main() to initialize JNI bindings */
JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls)
{
    LOGV("AUDIO nativeSetupJNI()");

    mAudioManagerClass = (jclass)((*env)->NewGlobalRef(env, cls));

    for (int i = 0; i < SDL_AudioFuncs_count; i++) {
        jnicall_audio[i] = (*env)->GetStaticMethodID(env, mAudioManagerClass, SDLAudioManager_ifc[i].name, SDLAudioManager_ifc[i].signature);
    }
    for (int i = 0; i < SDL_AudioFuncs_count; i++) {
        if (!jnicall_audio[i]) {
            LOGW("Missing some Java callbacks, do you have the latest version of SDLAudioManager.java? (idx=%d)", i);
            break;
        }
    }

    checkJNIReady();
}

/* Controller initialization -- called before SDL_main() to initialize JNI bindings */
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls)
{
    LOGV("CONTROLLER nativeSetupJNI()");

    mControllerManagerClass = (jclass)((*env)->NewGlobalRef(env, cls));

    for (int i = 0; i < SDL_ControllerFuncs_count; i++) {
        jnicall_ctrl[i] = (*env)->GetStaticMethodID(env, mControllerManagerClass, SDLControllerManager_ifc[i].name, SDLControllerManager_ifc[i].signature);
    }
    for (int i = 0; i < SDL_ControllerFuncs_count; i++) {
        if (!jnicall_ctrl[i]) {
            LOGW("Missing some Java callbacks, do you have the latest version of SDLControllerManager.java? (idx=%d)", i);
            break;
        }
    }

    checkJNIReady();
}

/* SDL main function prototype */
typedef int (*SDL_main_func)(int argc, char *argv[]);

/* Start up the SDL app */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeRunMain)(JNIEnv *env, jclass cls, jstring library, jstring function, jobject array)
{
    // int status = -1;
    const char *library_file;
    void *library_handle;

    LOGV("nativeRunMain()");

    /* Save JNIEnv of SDLThread */
    Android_JNI_SetEnv(env);

    library_file = (*env)->GetStringUTFChars(env, library, NULL);
    library_handle = dlopen(library_file, RTLD_GLOBAL);

    if (library_handle == NULL) {
        /* When deploying android app bundle format uncompressed native libs may not extract from apk to filesystem.
           In this case we should use lib name without path. https://bugzilla.libsdl.org/show_bug.cgi?id=4739 */
        const char *library_name = SDL_strrchr(library_file, '/');
        if (library_name && *library_name) {
            library_name += 1;
            library_handle = dlopen(library_name, RTLD_GLOBAL);
        }
    }

    if (library_handle) {
        const char *function_name;
        SDL_main_func SDL_main;

        function_name = (*env)->GetStringUTFChars(env, function, NULL);
        SDL_main = (SDL_main_func)dlsym(library_handle, function_name);
        if (SDL_main) {
            int i;
            int argc;
            int len;
            char **argv;
            SDL_bool isstack;

            /* Prepare the arguments. */
            len = (*env)->GetArrayLength(env, array);
            argv = SDL_small_alloc(char *, 1 + len + 1, &isstack); /* !!! FIXME: check for NULL */
            argc = 0;
            /* Use the name "app_process" so PHYSFS_platformCalcBaseDir() works.
               https://bitbucket.org/MartinFelis/love-android-sdl2/issue/23/release-build-crash-on-start
             */
            argv[argc++] = SDL_strdup("app_process");
            for (i = 0; i < len; ++i) {
                const char *utf;
                char *arg = NULL;
                jstring string = (*env)->GetObjectArrayElement(env, array, i);
                if (string) {
                    utf = (*env)->GetStringUTFChars(env, string, 0);
                    if (utf) {
                        arg = SDL_strdup(utf);
                        (*env)->ReleaseStringUTFChars(env, string, utf);
                    }
                    (*env)->DeleteLocalRef(env, string);
                }
                if (arg == NULL) {
                    arg = SDL_strdup("");
                }
                argv[argc++] = arg;
            }
            argv[argc] = NULL;

            /* Run the application. */
            /*status =*/ SDL_main(argc, argv);

            /* Release the arguments. */
            for (i = 0; i < argc; ++i) {
                SDL_free(argv[i]);
            }
            SDL_small_free(argv, isstack);

        } else {
            LOGE("nativeRunMain(): Couldn't find function %s in library %s", function_name, library_file);
        }
        (*env)->ReleaseStringUTFChars(env, function, function_name);

        dlclose(library_handle);

    } else {
        LOGE("nativeRunMain(): Couldn't load library %s", library_file);
    }
    (*env)->ReleaseStringUTFChars(env, library, library_file);

    /* This is a Java thread, it doesn't need to be Detached from the JVM.
     * Set to mThreadKey value to NULL not to call pthread_create destructor 'Android_JNI_ThreadDestroyed' */
    Android_JNI_SetEnv(NULL);

    /* Do not issue an exit or the whole application will terminate instead of just the SDL thread */
    /* exit(status); */
}

/* Drop file */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDropFile)(
    JNIEnv *env, jclass jcls,
    jstring filename)
{
    const char *path = (*env)->GetStringUTFChars(env, filename, NULL);
    SDL_SendDropFile(NULL, path);
    (*env)->ReleaseStringUTFChars(env, filename, path);
    SDL_SendDropComplete(NULL);
}

/* Lock / Unlock Mutex */
void Android_ActivityMutex_Lock(void)
{
    SDL_LockMutex(Android_ActivityMutex);
}

void Android_ActivityMutex_Unlock(void)
{
    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Lock the Mutex when the Activity is in its 'Running' state */
void Android_ActivityMutex_Lock_Running(void)
{
    int pauseSignaled = 0;
    int resumeSignaled = 0;

retry:

    SDL_LockMutex(Android_ActivityMutex);

    pauseSignaled = SDL_SemValue(Android_PauseSem);
    resumeSignaled = SDL_SemValue(Android_ResumeSem);

    if (pauseSignaled > resumeSignaled) {
        SDL_UnlockMutex(Android_ActivityMutex);
        SDL_Delay(50);
        goto retry;
    }
}

/* Set screen resolution */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetScreenResolution)(
    JNIEnv *env, jclass jcls,
    jint surfaceWidth, jint surfaceHeight,
    jint deviceWidth, jint deviceHeight, jfloat rate)
{
    SDL_LockMutex(Android_ActivityMutex);

    Android_SetScreenResolution(surfaceWidth, surfaceHeight, deviceWidth, deviceHeight, rate);

    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Resize */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeResize)(
    JNIEnv *env, jclass jcls)
{
    SDL_LockMutex(Android_ActivityMutex);

    Android_OnResize();

    SDL_UnlockMutex(Android_ActivityMutex);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeOrientationChanged)(
    JNIEnv *env, jclass jcls,
    jint orientation)
{
    SDL_LockMutex(Android_ActivityMutex);

    displayOrientation = (SDL_DisplayOrientation)orientation;

    Android_OnOrientationChanged(displayOrientation);

    SDL_UnlockMutex(Android_ActivityMutex);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeAddTouch)(
    JNIEnv *env, jclass cls,
    jint touchId, jstring name)
{
    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);

    SDL_AddTouch((SDL_TouchID)touchId, SDL_TOUCH_DEVICE_DIRECT, utfname);

    (*env)->ReleaseStringUTFChars(env, name, utfname);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePermissionResult)(
    JNIEnv *env, jclass cls,
    jint requestCode, jboolean result)
{
    bPermissionRequestResult = result;
    SDL_AtomicSet(&bPermissionRequestPending, SDL_FALSE);
}

extern void SDL_AddAudioDevice(const SDL_bool iscapture, const char *name, SDL_AudioSpec *spec, void *handle);
extern void SDL_RemoveAudioDevice(const SDL_bool iscapture, void *handle);

JNIEXPORT void JNICALL
SDL_JAVA_AUDIO_INTERFACE(addAudioDevice)(JNIEnv *env, jclass jcls, jboolean is_capture,
                                         jint device_id)
{
    if (SDL_GetCurrentAudioDriver() != NULL) {
        char device_name[64];
        SDL_snprintf(device_name, sizeof(device_name), "%d", device_id);
        LOGV("Adding device with name %s, capture %d", device_name, is_capture);
        SDL_AddAudioDevice(is_capture, SDL_strdup(device_name), NULL, (void *)((size_t)device_id + 1));
    }
}

JNIEXPORT void JNICALL
SDL_JAVA_AUDIO_INTERFACE(removeAudioDevice)(JNIEnv *env, jclass jcls, jboolean is_capture,
                                            jint device_id)
{
    if (SDL_GetCurrentAudioDriver() != NULL) {
        LOGV("Removing device with handle %d, capture %d", device_id + 1, is_capture);
        SDL_RemoveAudioDevice(is_capture, (void *)((size_t)device_id + 1));
    }
}

/* Paddown */
JNIEXPORT jint JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode)
{
    return Android_OnPadDown(device_id, keycode);
}

/* Padup */
JNIEXPORT jint JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint keycode)
{
    return Android_OnPadUp(device_id, keycode);
}

/* Joy */
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint axis, jfloat value)
{
    Android_OnJoy(device_id, axis, value);
}

/* POV Hat */
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat)(
    JNIEnv *env, jclass jcls,
    jint device_id, jint hat_id, jint x, jint y)
{
    Android_OnHat(device_id, hat_id, x, y);
}

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name, jstring device_desc,
    jint vendor_id, jint product_id,
    jint button_mask, jint naxes, jint axis_mask, jint nhats, jboolean can_rumble)
{
    const char *name = (*env)->GetStringUTFChars(env, device_name, NULL);
    const char *desc = (*env)->GetStringUTFChars(env, device_desc, NULL);

    Android_AddJoystick(device_id, name, desc, vendor_id, product_id, button_mask, naxes, axis_mask, nhats, can_rumble);

    (*env)->ReleaseStringUTFChars(env, device_name, name);
    (*env)->ReleaseStringUTFChars(env, device_desc, desc);
}

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick)(
    JNIEnv *env, jclass jcls,
    jint device_id)
{
    Android_RemoveJoystick(device_id);
}

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic)(
    JNIEnv *env, jclass jcls, jint device_id, jstring device_name)
{
#ifdef SDL_HAPTIC_ANDROID
    const char *name = (*env)->GetStringUTFChars(env, device_name, NULL);

    Android_AddHaptic(device_id, name);

    (*env)->ReleaseStringUTFChars(env, device_name, name);
#endif
}

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic)(
    JNIEnv *env, jclass jcls, jint device_id)
{
#ifdef SDL_HAPTIC_ANDROID
    Android_RemoveHaptic(device_id);
#endif
}

/* Called from surfaceCreated() */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceCreated)(JNIEnv *env, jclass jcls)
{
    ANativeWindow *wnd;

    SDL_LockMutex(Android_ActivityMutex);

    wnd = Android_JNI_GetNativeWindow();
    if (wnd == NULL) {
        SDL_SetError("Could not fetch native window from UI thread");
    }

    Android_OnSurfaceCreated(wnd);

    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Called from surfaceChanged() */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceChanged)(JNIEnv *env, jclass jcls)
{
    SDL_LockMutex(Android_ActivityMutex);

    Android_OnSurfaceChanged();

    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Called from surfaceDestroyed() */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed)(JNIEnv *env, jclass jcls)
{
    int nb_attempt = 50;

retry:

    SDL_LockMutex(Android_ActivityMutex);

    if (Android_OnSurfaceDestroyed(nb_attempt == 0) < 0) {
        nb_attempt -= 1;
        SDL_UnlockMutex(Android_ActivityMutex);
        SDL_Delay(10);
        goto retry;
    }

    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Keydown */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyDown)(
    JNIEnv *env, jclass jcls,
    jint keycode)
{
    Android_OnKeyDown(keycode);
}

/* Keyup */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyUp)(
    JNIEnv *env, jclass jcls,
    jint keycode)
{
    Android_OnKeyUp(keycode);
}

/* Virtual keyboard return key might stop text input */
JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(onNativeSoftReturnKey)(
    JNIEnv *env, jclass jcls)
{
    if (SDL_GetHintBoolean(SDL_HINT_RETURN_KEY_HIDES_IME, SDL_FALSE)) {
        SDL_StopTextInput();
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

/* Keyboard Focus Lost */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost)(
    JNIEnv *env, jclass jcls)
{
    /* Calling SDL_StopTextInput will take care of hiding the keyboard and cleaning up the DummyText widget */
    SDL_StopTextInput();
}

/* Touch */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeTouch)(
    JNIEnv *env, jclass jcls,
    jint touch_device_id_in, jint pointer_finger_id_in,
    jint action, jfloat x, jfloat y, jfloat p)
{
    SDL_LockMutex(Android_ActivityMutex);

    Android_OnTouch(touch_device_id_in, pointer_finger_id_in, action, x, y, p);

    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Mouse */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeMouse)(
    JNIEnv *env, jclass jcls,
    jint button, jint action, jfloat x, jfloat y, jboolean relative)
{
    SDL_LockMutex(Android_ActivityMutex);

    Android_OnMouse(button, action, x, y, relative);

    SDL_UnlockMutex(Android_ActivityMutex);
}

/* Accelerometer */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeAccel)(
    JNIEnv *env, jclass jcls,
    jfloat x, jfloat y, jfloat z)
{
    fLastAccelerometer[0] = x;
    fLastAccelerometer[1] = y;
    fLastAccelerometer[2] = z;
    bHasNewData = SDL_TRUE;
}

/* Clipboard */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeClipboardChanged)(
    JNIEnv *env, jclass jcls)
{
    SDL_SendClipboardUpdate();
}

/* Low memory */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeLowMemory)(
    JNIEnv *env, jclass cls)
{
    SDL_SendAppEvent(SDL_APP_LOWMEMORY);
}

/* Locale
 * requires android:configChanges="layoutDirection|locale" in AndroidManifest.xml */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeLocaleChanged)(
    JNIEnv *env, jclass cls)
{
    SDL_SendAppEvent(SDL_LOCALECHANGED);
}

/* Send Quit event to "SDLThread" thread */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSendQuit)(
    JNIEnv *env, jclass cls)
{
    /* Discard previous events. The user should have handled state storage
     * in SDL_APP_WILLENTERBACKGROUND. After nativeSendQuit() is called, no
     * events other than SDL_QUIT and SDL_APP_TERMINATING should fire */
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    /* Inject a SDL_QUIT event */
    SDL_SendQuit();
    SDL_SendAppEvent(SDL_APP_TERMINATING);
    /* Robustness: clear any pending Pause */
    while (SDL_SemTryWait(Android_PauseSem) == 0) {
        /* empty */
    }
    /* Resume the event loop so that the app can catch SDL_QUIT which
     * should now be the top event in the event queue. */
    SDL_SemPost(Android_ResumeSem);
}

/* Activity ends */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeQuit)(
    JNIEnv *env, jclass cls)
{
    if (Android_ActivityMutex) {
        SDL_DestroyMutex(Android_ActivityMutex);
        Android_ActivityMutex = NULL;
    }

    if (Android_PauseSem) {
        SDL_DestroySemaphore(Android_PauseSem);
        Android_PauseSem = NULL;
    }

    if (Android_ResumeSem) {
        SDL_DestroySemaphore(Android_ResumeSem);
        Android_ResumeSem = NULL;
    }

    Internal_Android_Destroy_AssetManager();
}

/* Pause */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePause)(
    JNIEnv *env, jclass cls)
{
    LOGV("nativePause()");

    /* Signal the pause semaphore so the event loop knows to pause and (optionally) block itself.
     * Sometimes 2 pauses can be queued (eg pause/resume/pause), so it's always increased. */
    SDL_SemPost(Android_PauseSem);
}

/* Resume */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeResume)(
    JNIEnv *env, jclass cls)
{
    LOGV("nativeResume()");

    /* Signal the resume semaphore so the event loop knows to resume and restore the GL Context
     * We can't restore the GL Context here because it needs to be done on the SDL main thread
     * and this function will be called from the Java thread instead.
     */
    SDL_SemPost(Android_ResumeSem);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeFocusChanged)(
    JNIEnv *env, jclass cls, jboolean hasFocus)
{
    SDL_LockMutex(Android_ActivityMutex);

    LOGV("nativeFocusChanged()");

    Android_OnFocusChanged(hasFocus);

    SDL_UnlockMutex(Android_ActivityMutex);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText)(
    JNIEnv *env, jclass cls,
    jstring text, jint newCursorPosition)
{
    const char *utftext = (*env)->GetStringUTFChars(env, text, NULL);

    SDL_SendKeyboardText(utftext);

    (*env)->ReleaseStringUTFChars(env, text, utftext);
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar)(
    JNIEnv *env, jclass cls,
    jchar chUnicode)
{
    SDL_SendKeyboardUnicodeKey(chUnicode);
}

JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetHint)(
    JNIEnv *env, jclass cls,
    jstring name)
{
    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);
    const char *hint = SDL_GetHint(utfname);

    jstring result = (*env)->NewStringUTF(env, hint);
    (*env)->ReleaseStringUTFChars(env, name, utfname);

    return result;
}

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeGetHintBoolean)(
    JNIEnv *env, jclass cls,
    jstring name, jboolean default_value)
{
    jboolean result;

    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);
    result = SDL_GetHintBoolean(utfname, default_value);
    (*env)->ReleaseStringUTFChars(env, name, utfname);

    return result;
}

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetenv)(
    JNIEnv *env, jclass cls,
    jstring name, jstring value)
{
    const char *utfname = (*env)->GetStringUTFChars(env, name, NULL);
    const char *utfvalue = (*env)->GetStringUTFChars(env, value, NULL);

    SDL_setenv(utfname, utfvalue, 1);

    (*env)->ReleaseStringUTFChars(env, name, utfname);
    (*env)->ReleaseStringUTFChars(env, value, utfvalue);
}

/*******************************************************************************
             Functions called by SDL into Java
*******************************************************************************/

static SDL_atomic_t s_active;
struct LocalReferenceHolder
{
    JNIEnv *m_env;
    const char *m_func;
};

static struct LocalReferenceHolder LocalReferenceHolder_Setup(const char *func)
{
    struct LocalReferenceHolder refholder;
    refholder.m_env = NULL;
    refholder.m_func = func;
#ifdef DEBUG_JNI
    SDL_Log("Entering function %s", func);
#endif
    return refholder;
}

static SDL_bool LocalReferenceHolder_Init(struct LocalReferenceHolder *refholder, JNIEnv *env)
{
    const int capacity = 16;
    if ((*env)->PushLocalFrame(env, capacity) < 0) {
        SDL_SetError("Failed to allocate enough JVM local references");
        return SDL_FALSE;
    }
    SDL_AtomicIncRef(&s_active);
    refholder->m_env = env;
    return SDL_TRUE;
}

static void LocalReferenceHolder_Cleanup(struct LocalReferenceHolder *refholder)
{
#ifdef DEBUG_JNI
    SDL_Log("Leaving function %s", refholder->m_func);
#endif
    if (refholder->m_env) {
        JNIEnv *env = refholder->m_env;
        (*env)->PopLocalFrame(env, NULL);
        SDL_AtomicDecRef(&s_active);
    }
}

ANativeWindow *Android_JNI_GetNativeWindow(void)
{
    ANativeWindow *anw = NULL;
    jobject s;
    JNIEnv *env = Android_JNI_GetEnv();

    s = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getNativeSurface]);
    if (s) {
        anw = ANativeWindow_fromSurface(env, s);
        (*env)->DeleteLocalRef(env, s);
    }

    return anw;
}

void Android_JNI_SetActivityTitle(const char *title)
{
    JNIEnv *env = Android_JNI_GetEnv();

    jstring jtitle = (*env)->NewStringUTF(env, title);
    (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_setActivityTitle], jtitle);
    (*env)->DeleteLocalRef(env, jtitle);
}

void Android_JNI_SetWindowStyle(SDL_bool fullscreen)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_setWindowStyle], fullscreen ? 1 : 0);
}

void Android_JNI_SetOrientation(int w, int h, int resizable, const char *hint)
{
    JNIEnv *env = Android_JNI_GetEnv();

    jstring jhint = (*env)->NewStringUTF(env, (hint ? hint : ""));
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_setOrientation], w, h, (resizable ? 1 : 0), jhint);
    (*env)->DeleteLocalRef(env, jhint);
}

void Android_JNI_MinizeWindow(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_minimizeWindow]);
}

SDL_bool Android_JNI_ShouldMinimizeOnFocusLoss(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_shouldMinimizeOnFocusLoss]);
}

SDL_bool Android_JNI_GetAccelerometerValues(float values[3])
{
    int i;
    SDL_bool retval = SDL_FALSE;

    if (bHasNewData) {
        for (i = 0; i < 3; ++i) {
            values[i] = fLastAccelerometer[i];
        }
        bHasNewData = SDL_FALSE;
        retval = SDL_TRUE;
    }

    return retval;
}

/*
 * Audio support
 */
static int audioBufferFormat = 0;
static jobject audioBuffer = NULL;
static void *audioBufferPinned = NULL;
static int captureBufferFormat = 0;
static jobject captureBuffer = NULL;

void Android_DetectDevices(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioDetectDevices]);
}

int Android_JNI_OpenAudioDevice(SDL_bool iscapture, int device_id, SDL_AudioSpec *spec)
{
    int audioformat, len;
    jobject jbufobj = NULL;
    jobject result;
    int *resultElements;
    jboolean isCopy;

    JNIEnv *env = Android_JNI_GetEnv();

    switch (spec->format) {
    case AUDIO_U8:
        audioformat = ENCODING_PCM_8BIT;
        break;
    case AUDIO_S16:
        audioformat = ENCODING_PCM_16BIT;
        break;
    case AUDIO_F32:
        audioformat = ENCODING_PCM_FLOAT;
        break;
    default:
        return SDL_SetError("Unsupported audio format: 0x%x", spec->format);
    }

    if (iscapture) {
        LOGV("SDL audio: opening device for capture");
        result = (*env)->CallStaticObjectMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureOpen], spec->freq, audioformat, spec->channels, spec->samples, device_id);
    } else {
        LOGV("SDL audio: opening device for output");
        result = (*env)->CallStaticObjectMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioOpen], spec->freq, audioformat, spec->channels, spec->samples, device_id);
    }
    if (!result) {
        /* Error during audio initialization, error printed from Java */
        return SDL_SetError("Java-side initialization failed");
    }
    len = (*env)->GetArrayLength(env, (jintArray)result);
    if (len != 4) {
        return SDL_SetError("Unexpected results from Java, expected 4, got %d", len);
    }
    isCopy = JNI_FALSE;
    resultElements = (*env)->GetIntArrayElements(env, (jintArray)result, &isCopy);
    spec->freq = resultElements[0];
    audioformat = resultElements[1];
    switch (audioformat) {
    case ENCODING_PCM_8BIT:
        spec->format = AUDIO_U8;
        break;
    case ENCODING_PCM_16BIT:
        spec->format = AUDIO_S16;
        break;
    case ENCODING_PCM_FLOAT:
        spec->format = AUDIO_F32;
        break;
    default:
        return SDL_SetError("Unexpected audio format from Java: %d", audioformat);
    }
    spec->channels = resultElements[2];
    spec->samples = resultElements[3];
    (*env)->ReleaseIntArrayElements(env, (jintArray)result, resultElements, JNI_ABORT);
    (*env)->DeleteLocalRef(env, result);

    /* Allocating the audio buffer from the Java side and passing it as the return value for audioInit no longer works on
     * Android >= 4.2 due to a "stale global reference" error. So now we allocate this buffer directly from this side. */
    switch (audioformat) {
    case ENCODING_PCM_8BIT:
    {
        jbyteArray audioBufferLocal = (*env)->NewByteArray(env, spec->samples * spec->channels);
        if (audioBufferLocal) {
            jbufobj = (*env)->NewGlobalRef(env, audioBufferLocal);
            (*env)->DeleteLocalRef(env, audioBufferLocal);
        }
    } break;
    case ENCODING_PCM_16BIT:
    {
        jshortArray audioBufferLocal = (*env)->NewShortArray(env, spec->samples * spec->channels);
        if (audioBufferLocal) {
            jbufobj = (*env)->NewGlobalRef(env, audioBufferLocal);
            (*env)->DeleteLocalRef(env, audioBufferLocal);
        }
    } break;
    case ENCODING_PCM_FLOAT:
    {
        jfloatArray audioBufferLocal = (*env)->NewFloatArray(env, spec->samples * spec->channels);
        if (audioBufferLocal) {
            jbufobj = (*env)->NewGlobalRef(env, audioBufferLocal);
            (*env)->DeleteLocalRef(env, audioBufferLocal);
        }
    } break;
    default:
        SDL_assume(!"Unexpected audio format from Java: %d");
    }

    if (!jbufobj) {
        LOGW("SDL audio: could not allocate an audio buffer");
        return SDL_OutOfMemory();
    }

    if (iscapture) {
        captureBufferFormat = audioformat;
        captureBuffer = jbufobj;
    } else {
        audioBufferFormat = audioformat;
        audioBuffer = jbufobj;
    }

    if (!iscapture) {
        isCopy = JNI_FALSE;

        switch (audioformat) {
        case ENCODING_PCM_8BIT:
            audioBufferPinned = (*env)->GetByteArrayElements(env, (jbyteArray)audioBuffer, &isCopy);
            break;
        case ENCODING_PCM_16BIT:
            audioBufferPinned = (*env)->GetShortArrayElements(env, (jshortArray)audioBuffer, &isCopy);
            break;
        case ENCODING_PCM_FLOAT:
            audioBufferPinned = (*env)->GetFloatArrayElements(env, (jfloatArray)audioBuffer, &isCopy);
            break;
        default:
            SDL_assume(!"Unexpected audio format from Java: %d");
        }
    }
    return 0;
}

SDL_DisplayOrientation Android_JNI_GetDisplayOrientation(void)
{
    return displayOrientation;
}

int Android_JNI_GetDisplayDPI(float *ddpi, float *xdpi, float *ydpi)
{
    JNIEnv *env = Android_JNI_GetEnv();

    jobject jDisplayObj = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getDisplayDPI]);
    jclass jDisplayClass = (*env)->GetObjectClass(env, jDisplayObj);

    jfieldID fidXdpi = (*env)->GetFieldID(env, jDisplayClass, "xdpi", "F");
    jfieldID fidYdpi = (*env)->GetFieldID(env, jDisplayClass, "ydpi", "F");
    jfieldID fidDdpi = (*env)->GetFieldID(env, jDisplayClass, "densityDpi", "I");

    float nativeXdpi = (*env)->GetFloatField(env, jDisplayObj, fidXdpi);
    float nativeYdpi = (*env)->GetFloatField(env, jDisplayObj, fidYdpi);
    int nativeDdpi = (*env)->GetIntField(env, jDisplayObj, fidDdpi);

    (*env)->DeleteLocalRef(env, jDisplayObj);
    (*env)->DeleteLocalRef(env, jDisplayClass);

    if (ddpi) {
        *ddpi = (float)nativeDdpi;
    }
    if (xdpi) {
        *xdpi = nativeXdpi;
    }
    if (ydpi) {
        *ydpi = nativeYdpi;
    }

    return 0;
}

void *Android_JNI_GetAudioBuffer(void)
{
    return audioBufferPinned;
}

void Android_JNI_WriteAudioBuffer(void)
{
    JNIEnv *env = Android_JNI_GetEnv();

    switch (audioBufferFormat) {
    case ENCODING_PCM_8BIT:
        (*env)->ReleaseByteArrayElements(env, (jbyteArray)audioBuffer, (jbyte *)audioBufferPinned, JNI_COMMIT);
        (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioWriteByteBuffer], (jbyteArray)audioBuffer);
        break;
    case ENCODING_PCM_16BIT:
        (*env)->ReleaseShortArrayElements(env, (jshortArray)audioBuffer, (jshort *)audioBufferPinned, JNI_COMMIT);
        (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioWriteShortBuffer], (jshortArray)audioBuffer);
        break;
    case ENCODING_PCM_FLOAT:
        (*env)->ReleaseFloatArrayElements(env, (jfloatArray)audioBuffer, (jfloat *)audioBufferPinned, JNI_COMMIT);
        (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioWriteFloatBuffer], (jfloatArray)audioBuffer);
        break;
    default:
        LOGW("SDL audio: unhandled audio buffer format");
        break;
    }

    /* JNI_COMMIT means the changes are committed to the VM but the buffer remains pinned */
}

int Android_JNI_CaptureAudioBuffer(void *buffer, int buflen)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jboolean isCopy = JNI_FALSE;
    jint br = -1;

    switch (captureBufferFormat) {
    case ENCODING_PCM_8BIT:
        SDL_assert((*env)->GetArrayLength(env, (jshortArray)captureBuffer) == buflen);
        br = (*env)->CallStaticIntMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureReadByteBuffer], (jbyteArray)captureBuffer, JNI_TRUE);
        if (br > 0) {
            jbyte *ptr = (*env)->GetByteArrayElements(env, (jbyteArray)captureBuffer, &isCopy);
            SDL_memcpy(buffer, ptr, br);
            (*env)->ReleaseByteArrayElements(env, (jbyteArray)captureBuffer, ptr, JNI_ABORT);
        }
        break;
    case ENCODING_PCM_16BIT:
        SDL_assert((*env)->GetArrayLength(env, (jshortArray)captureBuffer) == (buflen / sizeof(Sint16)));
        br = (*env)->CallStaticIntMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureReadShortBuffer], (jshortArray)captureBuffer, JNI_TRUE);
        if (br > 0) {
            jshort *ptr = (*env)->GetShortArrayElements(env, (jshortArray)captureBuffer, &isCopy);
            br *= sizeof(Sint16);
            SDL_memcpy(buffer, ptr, br);
            (*env)->ReleaseShortArrayElements(env, (jshortArray)captureBuffer, ptr, JNI_ABORT);
        }
        break;
    case ENCODING_PCM_FLOAT:
        SDL_assert((*env)->GetArrayLength(env, (jfloatArray)captureBuffer) == (buflen / sizeof(float)));
        br = (*env)->CallStaticIntMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureReadFloatBuffer], (jfloatArray)captureBuffer, JNI_TRUE);
        if (br > 0) {
            jfloat *ptr = (*env)->GetFloatArrayElements(env, (jfloatArray)captureBuffer, &isCopy);
            br *= sizeof(float);
            SDL_memcpy(buffer, ptr, br);
            (*env)->ReleaseFloatArrayElements(env, (jfloatArray)captureBuffer, ptr, JNI_ABORT);
        }
        break;
    default:
        LOGW("SDL audio: unhandled capture buffer format");
        break;
    }
    return br;
}

void Android_JNI_FlushCapturedAudio(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
#if 0 /* !!! FIXME: this needs API 23, or it'll do blocking reads and never end. */
    switch (captureBufferFormat) {
    case ENCODING_PCM_8BIT:
        {
            const jint len = (*env)->GetArrayLength(env, (jbyteArray)captureBuffer);
            while ((*env)->CallStaticIntMethod(env, mActivityClass, jnicall_audio[SDLAudio_captureReadByteBuffer], (jbyteArray)captureBuffer, JNI_FALSE) == len) { /* spin */ }
        }
        break;
    case ENCODING_PCM_16BIT:
        {
            const jint len = (*env)->GetArrayLength(env, (jshortArray)captureBuffer);
            while ((*env)->CallStaticIntMethod(env, mActivityClass,jnicall_audio[SDLAudio_captureReadShortBuffer], (jshortArray)captureBuffer, JNI_FALSE) == len) { /* spin */ }
        }
        break;
    case ENCODING_PCM_FLOAT:
        {
            const jint len = (*env)->GetArrayLength(env, (jfloatArray)captureBuffer);
            while ((*env)->CallStaticIntMethod(env, mActivityClass, jnicall_audio[SDLAudio_captureReadFloatBuffer], (jfloatArray)captureBuffer, JNI_FALSE) == len) { /* spin */ }
        }
        break;
    default:
        LOGW("SDL audio: flushing unhandled capture buffer format");
        break;
    }
#else
    switch (captureBufferFormat) {
    case ENCODING_PCM_8BIT:
        (*env)->CallStaticIntMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureReadByteBuffer], (jbyteArray)captureBuffer, JNI_FALSE);
        break;
    case ENCODING_PCM_16BIT:
        (*env)->CallStaticIntMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureReadShortBuffer], (jshortArray)captureBuffer, JNI_FALSE);
        break;
    case ENCODING_PCM_FLOAT:
        (*env)->CallStaticIntMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureReadFloatBuffer], (jfloatArray)captureBuffer, JNI_FALSE);
        break;
    default:
        LOGW("SDL audio: flushing unhandled capture buffer format");
        break;
    }
#endif
}

void Android_JNI_CloseAudioDevice(const SDL_bool iscapture)
{
    JNIEnv *env = Android_JNI_GetEnv();

    if (iscapture) {
        (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_captureClose]);
        if (captureBuffer) {
            (*env)->DeleteGlobalRef(env, captureBuffer);
            captureBuffer = NULL;
        }
    } else {
        (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioClose]);
        if (audioBuffer) {
            (*env)->DeleteGlobalRef(env, audioBuffer);
            audioBuffer = NULL;
            audioBufferPinned = NULL;
        }
    }
}

void Android_JNI_AudioSetThreadPriority(SDL_bool iscapture, int device_id)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mAudioManagerClass, jnicall_audio[SDLAudio_audioSetThreadPriority], iscapture, device_id);
}

/* Test for an exception and call SDL_SetError with its detail if one occurs */
/* If the parameter silent is truthy then SDL_SetError() will not be called. */
static SDL_bool Android_JNI_ExceptionOccurred(SDL_bool silent)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jthrowable exception;

    /* Detect mismatch LocalReferenceHolder_Init/Cleanup */
    SDL_assert(SDL_AtomicGet(&s_active) > 0);

    exception = (*env)->ExceptionOccurred(env);
    if (exception != NULL) {
        jmethodID mid;

        /* Until this happens most JNI operations have undefined behaviour */
        (*env)->ExceptionClear(env);

        if (!silent) {
            jclass exceptionClass = (*env)->GetObjectClass(env, exception);
            jclass classClass = (*env)->FindClass(env, "java/lang/Class");
            jstring exceptionName;
            const char *exceptionNameUTF8;
            jstring exceptionMessage;

            mid = (*env)->GetMethodID(env, classClass, "getName", "()Ljava/lang/String;");
            exceptionName = (jstring)(*env)->CallObjectMethod(env, exceptionClass, mid);
            exceptionNameUTF8 = (*env)->GetStringUTFChars(env, exceptionName, 0);

            mid = (*env)->GetMethodID(env, exceptionClass, "getMessage", "()Ljava/lang/String;");
            exceptionMessage = (jstring)(*env)->CallObjectMethod(env, exception, mid);

            if (exceptionMessage != NULL) {
                const char *exceptionMessageUTF8 = (*env)->GetStringUTFChars(env, exceptionMessage, 0);
                SDL_SetError("%s: %s", exceptionNameUTF8, exceptionMessageUTF8);
                (*env)->ReleaseStringUTFChars(env, exceptionMessage, exceptionMessageUTF8);
            } else {
                SDL_SetError("%s", exceptionNameUTF8);
            }

            (*env)->ReleaseStringUTFChars(env, exceptionName, exceptionNameUTF8);
        }

        return SDL_TRUE;
    }

    return SDL_FALSE;
}

static void Internal_Android_Create_AssetManager(void)
{

    struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
    JNIEnv *env = Android_JNI_GetEnv();
    jmethodID mid;
    jobject context;
    jobject javaAssetManager;

    if (!LocalReferenceHolder_Init(&refs, env)) {
        LocalReferenceHolder_Cleanup(&refs);
        return;
    }

    /* context = SDLActivity.getContext(); */
    context = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getContext]);

    /* javaAssetManager = context.getAssets(); */
    mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                              "getAssets", "()Landroid/content/res/AssetManager;");
    javaAssetManager = (*env)->CallObjectMethod(env, context, mid);

    /**
     * Given a Dalvik AssetManager object, obtain the corresponding native AAssetManager
     * object.  Note that the caller is responsible for obtaining and holding a VM reference
     * to the jobject to prevent its being garbage collected while the native object is
     * in use.
     */
    javaAssetManagerRef = (*env)->NewGlobalRef(env, javaAssetManager);
    asset_manager = AAssetManager_fromJava(env, javaAssetManagerRef);

    if (!asset_manager) {
        (*env)->DeleteGlobalRef(env, javaAssetManagerRef);
        Android_JNI_ExceptionOccurred(SDL_TRUE);
    }

    LocalReferenceHolder_Cleanup(&refs);
}

static void Internal_Android_Destroy_AssetManager(void)
{
    JNIEnv *env = Android_JNI_GetEnv();

    if (asset_manager) {
        (*env)->DeleteGlobalRef(env, javaAssetManagerRef);
        asset_manager = NULL;
    }
}

int Android_JNI_FileOpen(SDL_RWops *ctx,
                         const char *fileName, const char *mode)
{
    AAsset *asset = NULL;
    ctx->hidden.androidio.asset = NULL;

    if (!asset_manager) {
        Internal_Android_Create_AssetManager();
    }

    if (!asset_manager) {
        return SDL_SetError("Couldn't create asset manager");
    }

    asset = AAssetManager_open(asset_manager, fileName, AASSET_MODE_UNKNOWN);
    if (!asset) {
        return SDL_SetError("Couldn't open asset '%s'", fileName);
    }

    ctx->hidden.androidio.asset = (void *)asset;
    return 0;
}

size_t Android_JNI_FileRead(SDL_RWops *ctx, void *buffer,
                            size_t size, size_t maxnum)
{
    size_t result;
    AAsset *asset = (AAsset *)ctx->hidden.androidio.asset;
    result = AAsset_read(asset, buffer, size * maxnum);

    if (result > 0) {
        /* Number of chuncks */
        return result / size;
    } else {
        /* Error or EOF */
        return result;
    }
}

size_t Android_JNI_FileWrite(SDL_RWops *ctx, const void *buffer,
                             size_t size, size_t num)
{
    SDL_SetError("Cannot write to Android package filesystem");
    return 0;
}

Sint64 Android_JNI_FileSize(SDL_RWops *ctx)
{
    off64_t result;
    AAsset *asset = (AAsset *)ctx->hidden.androidio.asset;
    result = AAsset_getLength64(asset);
    return result;
}

Sint64 Android_JNI_FileSeek(SDL_RWops *ctx, Sint64 offset, int whence)
{
    off64_t result;
    AAsset *asset = (AAsset *)ctx->hidden.androidio.asset;
    result = AAsset_seek64(asset, offset, whence);
    return result;
}

int Android_JNI_FileClose(SDL_RWops *ctx)
{
    AAsset *asset = (AAsset *)ctx->hidden.androidio.asset;
    AAsset_close(asset);
    return 0;
}

int Android_JNI_SetClipboardText(const char *text)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jstring string = (*env)->NewStringUTF(env, text);
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_clipboardSetText], string);
    (*env)->DeleteLocalRef(env, string);
    return 0;
}

char *Android_JNI_GetClipboardText(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    char *text = NULL;
    jstring string;

    string = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_clipboardGetText]);
    if (string) {
        const char *utf = (*env)->GetStringUTFChars(env, string, 0);
        if (utf) {
            text = SDL_strdup(utf);
            (*env)->ReleaseStringUTFChars(env, string, utf);
        }
        (*env)->DeleteLocalRef(env, string);
    }

    return (!text) ? SDL_strdup("") : text;
}

SDL_bool Android_JNI_HasClipboardText(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jboolean retval = (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_clipboardHasText]);
    return (retval == JNI_TRUE) ? SDL_TRUE : SDL_FALSE;
}

/* returns 0 on success or -1 on error (others undefined then)
 * returns truthy or falsy value in plugged, charged and battery
 * returns the value in seconds and percent or -1 if not available
 */
int Android_JNI_GetPowerInfo(int *plugged, int *charged, int *battery, int *seconds, int *percent)
{
    struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
    JNIEnv *env = Android_JNI_GetEnv();
    jmethodID mid;
    jobject context;
    jstring action;
    jclass cls;
    jobject filter;
    jobject intent;
    jstring iname;
    jmethodID imid;
    jstring bname;
    jmethodID bmid;
    if (!LocalReferenceHolder_Init(&refs, env)) {
        LocalReferenceHolder_Cleanup(&refs);
        return -1;
    }

    /* context = SDLActivity.getContext(); */
    context = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getContext]);

    action = (*env)->NewStringUTF(env, "android.intent.action.BATTERY_CHANGED");

    cls = (*env)->FindClass(env, "android/content/IntentFilter");

    mid = (*env)->GetMethodID(env, cls, "<init>", "(Ljava/lang/String;)V");
    filter = (*env)->NewObject(env, cls, mid, action);

    (*env)->DeleteLocalRef(env, action);

    mid = (*env)->GetMethodID(env, mActivityClass, "registerReceiver", "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;");
    intent = (*env)->CallObjectMethod(env, context, mid, NULL, filter);

    (*env)->DeleteLocalRef(env, filter);

    cls = (*env)->GetObjectClass(env, intent);

    imid = (*env)->GetMethodID(env, cls, "getIntExtra", "(Ljava/lang/String;I)I");

    /* Watch out for C89 scoping rules because of the macro */
#define GET_INT_EXTRA(var, key)                                  \
    int var;                                                     \
    iname = (*env)->NewStringUTF(env, key);                      \
    (var) = (*env)->CallIntMethod(env, intent, imid, iname, -1); \
    (*env)->DeleteLocalRef(env, iname);

    bmid = (*env)->GetMethodID(env, cls, "getBooleanExtra", "(Ljava/lang/String;Z)Z");

    /* Watch out for C89 scoping rules because of the macro */
#define GET_BOOL_EXTRA(var, key)                                            \
    int var;                                                                \
    bname = (*env)->NewStringUTF(env, key);                                 \
    (var) = (*env)->CallBooleanMethod(env, intent, bmid, bname, JNI_FALSE); \
    (*env)->DeleteLocalRef(env, bname);

    if (plugged) {
        /* Watch out for C89 scoping rules because of the macro */
        GET_INT_EXTRA(plug, "plugged") /* == BatteryManager.EXTRA_PLUGGED (API 5) */
        if (plug == -1) {
            LocalReferenceHolder_Cleanup(&refs);
            return -1;
        }
        /* 1 == BatteryManager.BATTERY_PLUGGED_AC */
        /* 2 == BatteryManager.BATTERY_PLUGGED_USB */
        *plugged = (0 < plug) ? 1 : 0;
    }

    if (charged) {
        /* Watch out for C89 scoping rules because of the macro */
        GET_INT_EXTRA(status, "status") /* == BatteryManager.EXTRA_STATUS (API 5) */
        if (status == -1) {
            LocalReferenceHolder_Cleanup(&refs);
            return -1;
        }
        /* 5 == BatteryManager.BATTERY_STATUS_FULL */
        *charged = (status == 5) ? 1 : 0;
    }

    if (battery) {
        GET_BOOL_EXTRA(present, "present") /* == BatteryManager.EXTRA_PRESENT (API 5) */
        *battery = present ? 1 : 0;
    }

    if (seconds) {
        *seconds = -1; /* not possible */
    }

    if (percent) {
        int level;
        int scale;

        /* Watch out for C89 scoping rules because of the macro */
        {
            GET_INT_EXTRA(level_temp, "level") /* == BatteryManager.EXTRA_LEVEL (API 5) */
            level = level_temp;
        }
        /* Watch out for C89 scoping rules because of the macro */
        {
            GET_INT_EXTRA(scale_temp, "scale") /* == BatteryManager.EXTRA_SCALE (API 5) */
            scale = scale_temp;
        }

        if ((level == -1) || (scale == -1)) {
            LocalReferenceHolder_Cleanup(&refs);
            return -1;
        }
        *percent = level * 100 / scale;
    }

    (*env)->DeleteLocalRef(env, intent);

    LocalReferenceHolder_Cleanup(&refs);
    return 0;
}

/* Add all touch devices */
void Android_JNI_InitTouch(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_initTouch]);
}

void Android_JNI_JoystickSubscribe(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, jnicall_ctrl[SDLController_joystickSubscribe]);
}

void Android_JNI_JoystickUnsubscribe(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, jnicall_ctrl[SDLController_joystickUnsubscribe]);
}

void Android_JNI_JoystickRumble(int device_id, float low_frequency_intensity, float high_frequency_intensity, int length)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, jnicall_ctrl[SDLController_joystickRumble], device_id, low_frequency_intensity, high_frequency_intensity, length);
}

void Android_JNI_PollHapticDevices(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, jnicall_ctrl[SDLController_pollHapticDevices]);
}

void Android_JNI_HapticRun(int device_id, float intensity, int length)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, jnicall_ctrl[SDLController_hapticRun], device_id, intensity, length);
}

void Android_JNI_HapticStop(int device_id)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mControllerManagerClass, jnicall_ctrl[SDLController_hapticStop], device_id);
}

/* See SDLActivity.java for constants. */
#define COMMAND_SET_KEEP_SCREEN_ON 5

int SDL_AndroidSendMessage(Uint32 command, int param)
{
    if (command >= 0x8000) {
        return Android_JNI_SendMessage(command, param);
    }
    return -1;
}

/* sends message to be handled on the UI event dispatch thread */
int Android_JNI_SendMessage(int command, int param)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jboolean success;
    success = (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_sendMessage], command, param);
    return success ? 0 : -1;
}

void Android_JNI_SuspendScreenSaver(SDL_bool suspend)
{
    Android_JNI_SendMessage(COMMAND_SET_KEEP_SCREEN_ON, (suspend == SDL_FALSE) ? 0 : 1);
}

void Android_JNI_ShowScreenKeyboard(SDL_Rect *inputRect)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_showTextInput],
                                    inputRect->x,
                                    inputRect->y,
                                    inputRect->w,
                                    inputRect->h);
}

void Android_JNI_HideScreenKeyboard(void)
{
    /* has to match Activity constant */
    const int COMMAND_TEXTEDIT_HIDE = 3;
    Android_JNI_SendMessage(COMMAND_TEXTEDIT_HIDE, 0);
}

SDL_bool Android_JNI_IsScreenKeyboardShown(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jboolean is_shown = 0;
    is_shown = (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_isScreenKeyboardShown]);
    return is_shown;
}

int Android_JNI_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    JNIEnv *env;
    jclass clazz;
    jmethodID mid;
    jobject context;
    jstring title;
    jstring message;
    jintArray button_flags;
    jintArray button_ids;
    jobjectArray button_texts;
    jintArray colors;
    jobject text;
    jint temp;
    int i;

    env = Android_JNI_GetEnv();

    /* convert parameters */

    clazz = (*env)->FindClass(env, "java/lang/String");

    title = (*env)->NewStringUTF(env, messageboxdata->title);
    message = (*env)->NewStringUTF(env, messageboxdata->message);

    button_flags = (*env)->NewIntArray(env, messageboxdata->numbuttons);
    button_ids = (*env)->NewIntArray(env, messageboxdata->numbuttons);
    button_texts = (*env)->NewObjectArray(env, messageboxdata->numbuttons,
                                          clazz, NULL);
    for (i = 0; i < messageboxdata->numbuttons; ++i) {
        const SDL_MessageBoxButtonData *sdlButton;

        if (messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT) {
            sdlButton = &messageboxdata->buttons[messageboxdata->numbuttons - 1 - i];
        } else {
            sdlButton = &messageboxdata->buttons[i];
        }

        temp = sdlButton->flags;
        (*env)->SetIntArrayRegion(env, button_flags, i, 1, &temp);
        temp = sdlButton->buttonid;
        (*env)->SetIntArrayRegion(env, button_ids, i, 1, &temp);
        text = (*env)->NewStringUTF(env, sdlButton->text);
        (*env)->SetObjectArrayElement(env, button_texts, i, text);
        (*env)->DeleteLocalRef(env, text);
    }

    if (messageboxdata->colorScheme) {
        colors = (*env)->NewIntArray(env, SDL_MESSAGEBOX_COLOR_MAX);
        for (i = 0; i < SDL_MESSAGEBOX_COLOR_MAX; ++i) {
            temp = (0xFFU << 24) |
                   (messageboxdata->colorScheme->colors[i].r << 16) |
                   (messageboxdata->colorScheme->colors[i].g << 8) |
                   (messageboxdata->colorScheme->colors[i].b << 0);
            (*env)->SetIntArrayRegion(env, colors, i, 1, &temp);
        }
    } else {
        colors = NULL;
    }

    (*env)->DeleteLocalRef(env, clazz);

    /* context = SDLActivity.getContext(); */
    context = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getContext]);

    clazz = (*env)->GetObjectClass(env, context);

    mid = (*env)->GetMethodID(env, clazz,
                              "messageboxShowMessageBox", "(ILjava/lang/String;Ljava/lang/String;[I[I[Ljava/lang/String;[I)I");
    *buttonid = (*env)->CallIntMethod(env, context, mid,
                                      messageboxdata->flags,
                                      title,
                                      message,
                                      button_flags,
                                      button_ids,
                                      button_texts,
                                      colors);

    (*env)->DeleteLocalRef(env, context);
    (*env)->DeleteLocalRef(env, clazz);

    /* delete parameters */

    (*env)->DeleteLocalRef(env, title);
    (*env)->DeleteLocalRef(env, message);
    (*env)->DeleteLocalRef(env, button_flags);
    (*env)->DeleteLocalRef(env, button_ids);
    (*env)->DeleteLocalRef(env, button_texts);
    (*env)->DeleteLocalRef(env, colors);

    return 0;
}

/*
//////////////////////////////////////////////////////////////////////////////
//
// Functions exposed to SDL applications in SDL_system.h
//////////////////////////////////////////////////////////////////////////////
*/

void *SDL_AndroidGetJNIEnv(void)
{
    return Android_JNI_GetEnv();
}

void *SDL_AndroidGetActivity(void)
{
    /* See SDL_system.h for caveats on using this function. */

    JNIEnv *env = Android_JNI_GetEnv();
    if (!env) {
        return NULL;
    }

    /* return SDLActivity.getContext(); */
    return (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getContext]);
}

int SDL_GetAndroidSDKVersion(void)
{
    static int sdk_version;
    if (!sdk_version) {
        char sdk[PROP_VALUE_MAX] = { 0 };
        if (__system_property_get("ro.build.version.sdk", sdk) != 0) {
            sdk_version = SDL_atoi(sdk);
        }
    }
    return sdk_version;
}

SDL_bool SDL_IsAndroidTablet(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_isTablet]);
}

SDL_bool SDL_IsAndroidTV(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_isAndroidTV]);
}

SDL_bool SDL_IsChromebook(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_isChromebook]);
}

SDL_bool SDL_IsDeXMode(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_isDeXMode]);
}

void SDL_AndroidBackButton(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_manualBackButton]);
}

const char *SDL_AndroidGetInternalStoragePath(void)
{
    static char *s_AndroidInternalFilesPath = NULL;

    if (!s_AndroidInternalFilesPath) {
        struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
        jmethodID mid;
        jobject context;
        jobject fileObject;
        jstring pathString;
        const char *path;

        JNIEnv *env = Android_JNI_GetEnv();
        if (!LocalReferenceHolder_Init(&refs, env)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        /* context = SDLActivity.getContext(); */
        context = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getContext]);
        if (!context) {
            SDL_SetError("Couldn't get Android context!");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        /* fileObj = context.getFilesDir(); */
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                                  "getFilesDir", "()Ljava/io/File;");
        fileObject = (*env)->CallObjectMethod(env, context, mid);
        if (!fileObject) {
            SDL_SetError("Couldn't get internal directory");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        /* path = fileObject.getCanonicalPath(); */
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, fileObject),
                                  "getCanonicalPath", "()Ljava/lang/String;");
        pathString = (jstring)(*env)->CallObjectMethod(env, fileObject, mid);
        if (Android_JNI_ExceptionOccurred(SDL_FALSE)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        path = (*env)->GetStringUTFChars(env, pathString, NULL);
        s_AndroidInternalFilesPath = SDL_strdup(path);
        (*env)->ReleaseStringUTFChars(env, pathString, path);

        LocalReferenceHolder_Cleanup(&refs);
    }
    return s_AndroidInternalFilesPath;
}

int SDL_AndroidGetExternalStorageState(void)
{
    struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
    jmethodID mid;
    jclass cls;
    jstring stateString;
    const char *state;
    int stateFlags;

    JNIEnv *env = Android_JNI_GetEnv();
    if (!LocalReferenceHolder_Init(&refs, env)) {
        LocalReferenceHolder_Cleanup(&refs);
        return 0;
    }

    cls = (*env)->FindClass(env, "android/os/Environment");
    mid = (*env)->GetStaticMethodID(env, cls,
                                    "getExternalStorageState", "()Ljava/lang/String;");
    stateString = (jstring)(*env)->CallStaticObjectMethod(env, cls, mid);

    state = (*env)->GetStringUTFChars(env, stateString, NULL);

    /* Print an info message so people debugging know the storage state */
    LOGI("external storage state: %s", state);

    if (SDL_strcmp(state, "mounted") == 0) {
        stateFlags = SDL_ANDROID_EXTERNAL_STORAGE_READ |
                     SDL_ANDROID_EXTERNAL_STORAGE_WRITE;
    } else if (SDL_strcmp(state, "mounted_ro") == 0) {
        stateFlags = SDL_ANDROID_EXTERNAL_STORAGE_READ;
    } else {
        stateFlags = 0;
    }
    (*env)->ReleaseStringUTFChars(env, stateString, state);

    LocalReferenceHolder_Cleanup(&refs);
    return stateFlags;
}

const char *SDL_AndroidGetExternalStoragePath(void)
{
    static char *s_AndroidExternalFilesPath = NULL;

    if (!s_AndroidExternalFilesPath) {
        struct LocalReferenceHolder refs = LocalReferenceHolder_Setup(__FUNCTION__);
        jmethodID mid;
        jobject context;
        jobject fileObject;
        jstring pathString;
        const char *path;

        JNIEnv *env = Android_JNI_GetEnv();
        if (!LocalReferenceHolder_Init(&refs, env)) {
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        /* context = SDLActivity.getContext(); */
        context = (*env)->CallStaticObjectMethod(env, mActivityClass, jnicall[SDLActivity_getContext]);

        /* fileObj = context.getExternalFilesDir(); */
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, context),
                                  "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        fileObject = (*env)->CallObjectMethod(env, context, mid, NULL);
        if (!fileObject) {
            SDL_SetError("Couldn't get external directory");
            LocalReferenceHolder_Cleanup(&refs);
            return NULL;
        }

        /* path = fileObject.getAbsolutePath(); */
        mid = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, fileObject),
                                  "getAbsolutePath", "()Ljava/lang/String;");
        pathString = (jstring)(*env)->CallObjectMethod(env, fileObject, mid);

        path = (*env)->GetStringUTFChars(env, pathString, NULL);
        s_AndroidExternalFilesPath = SDL_strdup(path);
        (*env)->ReleaseStringUTFChars(env, pathString, path);

        LocalReferenceHolder_Cleanup(&refs);
    }
    return s_AndroidExternalFilesPath;
}

SDL_bool SDL_AndroidRequestPermission(const char *permission)
{
    return Android_JNI_RequestPermission(permission);
}

int SDL_AndroidShowToast(const char *message, int duration, int gravity, int xOffset, int yOffset)
{
    return Android_JNI_ShowToast(message, duration, gravity, xOffset, yOffset);
}

void Android_JNI_GetManifestEnvironmentVariables(void)
{
    if (!mActivityClass || !jnicall[SDLActivity_getManifestEnvironmentVariables]) {
        LOGW("Request to get environment variables before JNI is ready");
        return;
    }

    if (!bHasEnvironmentVariables) {
        JNIEnv *env = Android_JNI_GetEnv();
        SDL_bool ret = (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_getManifestEnvironmentVariables]);
        if (ret) {
            bHasEnvironmentVariables = SDL_TRUE;
        }
    }
}

int Android_JNI_CreateCustomCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    JNIEnv *env = Android_JNI_GetEnv();
    int custom_cursor = 0;
    jintArray pixels;
    pixels = (*env)->NewIntArray(env, surface->w * surface->h);
    if (pixels) {
        (*env)->SetIntArrayRegion(env, pixels, 0, surface->w * surface->h, (int *)surface->pixels);
        custom_cursor = (*env)->CallStaticIntMethod(env, mActivityClass, jnicall[SDLActivity_createCustomCursor], pixels, surface->w, surface->h, hot_x, hot_y);
        (*env)->DeleteLocalRef(env, pixels);
    } else {
        SDL_OutOfMemory();
    }
    return custom_cursor;
}

void Android_JNI_DestroyCustomCursor(int cursorID)
{
    JNIEnv *env = Android_JNI_GetEnv();
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_destroyCustomCursor], cursorID);
}

SDL_bool Android_JNI_SetCustomCursor(int cursorID)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_setCustomCursor], cursorID);
}

SDL_bool Android_JNI_SetSystemCursor(int cursorID)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_setSystemCursor], cursorID);
}

SDL_bool Android_JNI_SupportsRelativeMouse(void)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_supportsRelativeMouse]);
}

SDL_bool Android_JNI_SetRelativeMouseEnabled(SDL_bool enabled)
{
    JNIEnv *env = Android_JNI_GetEnv();
    return (*env)->CallStaticBooleanMethod(env, mActivityClass, jnicall[SDLActivity_setRelativeMouseEnabled], (enabled == 1));
}

SDL_bool Android_JNI_RequestPermission(const char *permission)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jstring jpermission;
    const int requestCode = 1;

    /* Wait for any pending request on another thread */
    while (SDL_AtomicGet(&bPermissionRequestPending) == SDL_TRUE) {
        SDL_Delay(10);
    }
    SDL_AtomicSet(&bPermissionRequestPending, SDL_TRUE);

    jpermission = (*env)->NewStringUTF(env, permission);
    (*env)->CallStaticVoidMethod(env, mActivityClass, jnicall[SDLActivity_requestPermission], jpermission, requestCode);
    (*env)->DeleteLocalRef(env, jpermission);

    /* Wait for the request to complete */
    while (SDL_AtomicGet(&bPermissionRequestPending) == SDL_TRUE) {
        SDL_Delay(10);
    }
    return bPermissionRequestResult;
}

/* Show toast notification */
int Android_JNI_ShowToast(const char *message, int duration, int gravity, int xOffset, int yOffset)
{
    int result = 0;
    JNIEnv *env = Android_JNI_GetEnv();
    jstring jmessage = (*env)->NewStringUTF(env, message);
    result = (*env)->CallStaticIntMethod(env, mActivityClass, jnicall[SDLActivity_showToast], jmessage, duration, gravity, xOffset, yOffset);
    (*env)->DeleteLocalRef(env, jmessage);
    return result;
}

int Android_JNI_GetLocale(char *buf, size_t buflen)
{
    AConfiguration *cfg;

    SDL_assert(buflen > 6);

    /* Need to re-create the asset manager if locale has changed (SDL_LOCALECHANGED) */
    Internal_Android_Destroy_AssetManager();

    if (!asset_manager) {
        Internal_Android_Create_AssetManager();
    }

    if (!asset_manager) {
        return -1;
    }

    cfg = AConfiguration_new();
    if (!cfg) {
        return -1;
    }

    {
        char language[2] = {};
        char country[2] = {};
        size_t id = 0;

        AConfiguration_fromAssetManager(cfg, asset_manager);
        AConfiguration_getLanguage(cfg, language);
        AConfiguration_getCountry(cfg, country);

        /* copy language (not null terminated) */
        if (language[0]) {
            buf[id++] = language[0];
            if (language[1]) {
                buf[id++] = language[1];
            }
        }

        buf[id++] = '_';

        /* copy country (not null terminated) */
        if (country[0]) {
            buf[id++] = country[0];
            if (country[1]) {
                buf[id++] = country[1];
            }
        }

        buf[id++] = '\0';
        SDL_assert(id <= buflen);
    }

    AConfiguration_delete(cfg);

    return 0;
}

int Android_JNI_OpenURL(const char *url)
{
    JNIEnv *env = Android_JNI_GetEnv();
    jstring jurl = (*env)->NewStringUTF(env, url);
    const int ret = (*env)->CallStaticIntMethod(env, mActivityClass, jnicall[SDLActivity_openURL], jurl);
    (*env)->DeleteLocalRef(env, jurl);
    return ret;
}

#endif /* __ANDROID__ */

/* vi: set ts=4 sw=4 expandtab: */
