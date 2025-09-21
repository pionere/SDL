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
#include <jni.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define SDL_JAVA_PREFIX                               org_libsdl_app
#define CONCAT1(prefix, class, function)              CONCAT2(prefix, class, function)
#define CONCAT2(prefix, class, function)              Java_ ## prefix ## _ ## class ## _ ## function

#define SDL_JAVA_INTERFACE(function)                  CONCAT1(SDL_JAVA_PREFIX, SDLActivity, function)
#define SDL_JAVA_AUDIO_INTERFACE(function)            CONCAT1(SDL_JAVA_PREFIX, SDLAudioManager, function)
#define SDL_JAVA_CONTROLLER_INTERFACE(function)       CONCAT1(SDL_JAVA_PREFIX, SDLControllerManager, function)
#define SDL_JAVA_INTERFACE_INPUT_CONNECTION(function) CONCAT1(SDL_JAVA_PREFIX, SDLInputConnection, function)


JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetVersion)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass cls);
JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass jcls);
JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeSetupJNI)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeRunMain)(JNIEnv *env, jclass cls,
    jstring library, jstring function, jobject array);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeDropFile)(JNIEnv *env, jclass jcls,
    jstring filename);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetScreenResolution)(JNIEnv *env, jclass jcls,
    jint surfaceWidth, jint surfaceHeight, jint deviceWidth, jint deviceHeight, jfloat rate);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeResize)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeOrientationChanged)(JNIEnv *env, jclass cls,
    jint orientation);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeAddTouch)(JNIEnv *env, jclass cls,
    jint touchId, jstring name);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePermissionResult)(JNIEnv *env, jclass cls,
    jint requestCode, jboolean result);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceCreated)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceChanged)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeSurfaceDestroyed)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyDown)(JNIEnv *env, jclass jcls,
    jint keycode);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyUp)(JNIEnv *env, jclass jcls,
    jint keycode);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(onNativeSoftReturnKey)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeKeyboardFocusLost)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeTouch)(JNIEnv *env, jclass jcls,
    jint touch_device_id_in, jint pointer_finger_id_in, jint action, jfloat x, jfloat y, jfloat p);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeMouse)(JNIEnv *env, jclass jcls,
    jint button, jint action, jfloat x, jfloat y, jboolean relative);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeAccel)(JNIEnv *env, jclass jcls,
    jfloat x, jfloat y, jfloat z);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeClipboardChanged)(JNIEnv *env, jclass jcls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeLowMemory)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(onNativeLocaleChanged)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSendQuit)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeQuit)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativePause)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeResume)(JNIEnv *env, jclass cls);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeFocusChanged)(JNIEnv *env, jclass cls,
    jboolean hasFocus);

JNIEXPORT jstring JNICALL SDL_JAVA_INTERFACE(nativeGetHint)(JNIEnv *env, jclass cls,
    jstring name);

JNIEXPORT jboolean JNICALL SDL_JAVA_INTERFACE(nativeGetHintBoolean)(JNIEnv *env, jclass cls,
    jstring name, jboolean default_value);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeSetenv)(JNIEnv *env, jclass cls,
    jstring name, jstring value);

/* Java class SDLInputConnection */
JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeCommitText)(JNIEnv *env, jclass cls,
    jstring text, jint newCursorPosition);

JNIEXPORT void JNICALL SDL_JAVA_INTERFACE_INPUT_CONNECTION(nativeGenerateScancodeForUnichar)(JNIEnv *env, jclass cls,
    jchar chUnicode);

JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(addAudioDevice)(JNIEnv *env, jclass jcls,
    jboolean is_capture, jint device_id);

JNIEXPORT void JNICALL SDL_JAVA_AUDIO_INTERFACE(removeAudioDevice)(JNIEnv *env, jclass jcls,
    jboolean is_capture, jint device_id);

JNIEXPORT jint JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadDown)(JNIEnv *env, jclass jcls,
    jint device_id, jint keycode);

JNIEXPORT jint JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativePadUp)(JNIEnv *env, jclass jcls,
    jint device_id, jint keycode);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeJoy)(JNIEnv *env, jclass jcls,
    jint device_id, jint axis, jfloat value);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(onNativeHat)(JNIEnv *env, jclass jcls,
    jint device_id, jint hat_id, jint x, jint y);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddJoystick)(JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name, jstring device_desc, jint vendor_id, jint product_id,
    jint button_mask, jint naxes, jint axis_mask, jint nhats, jboolean can_rumble);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveJoystick)(JNIEnv *env, jclass jcls,
    jint device_id);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeAddHaptic)(JNIEnv *env, jclass jcls,
    jint device_id, jstring device_name);

JNIEXPORT void JNICALL SDL_JAVA_CONTROLLER_INTERFACE(nativeRemoveHaptic)(JNIEnv *env, jclass jcls,
    jint device_id);

#define HID_DEVICE_MANAGER_JAVA_INTERFACE(function)   CONCAT1(SDL_JAVA_PREFIX, HIDDeviceManager, function)

JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceRegisterCallback)(JNIEnv *env, jobject thiz);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceReleaseCallback)(JNIEnv *env, jobject thiz);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceConnected)(JNIEnv *env, jobject thiz,
    jint nDeviceID, jstring sIdentifier, jint nVendorId, jint nProductId, jstring sSerialNumber, jint nReleaseNumber, jstring sManufacturer,
    jstring sProduct, jint nInterface, jint nInterfaceClass, jint nInterfaceSubclass, jint nInterfaceProtocol);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceOpenPending)(JNIEnv *env, jobject thiz,
    jint nDeviceID);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceOpenResult)(JNIEnv *env, jobject thiz,
    jint nDeviceID, jboolean bOpened);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceDisconnected)(JNIEnv *env, jobject thiz,
    jint nDeviceID);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceInputReport)(JNIEnv *env, jobject thiz,
    jint nDeviceID, jbyteArray value);
JNIEXPORT void JNICALL HID_DEVICE_MANAGER_JAVA_INTERFACE(HIDDeviceFeatureReport)(JNIEnv *env, jobject thiz,
    jint nDeviceID, jbyteArray value);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

/* vi: set ts=4 sw=4 expandtab: */
