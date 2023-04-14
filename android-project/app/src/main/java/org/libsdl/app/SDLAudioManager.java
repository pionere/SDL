package org.libsdl.app;

import android.content.Context;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

import java.util.Arrays;

public class SDLAudioManager {
    protected static final String TAG = "SDLAudio";

    protected static Context mContext;

    private static AudioDeviceCallback mAudioDeviceCallback;

    private static void addAudioDevices(AudioDeviceInfo[] devices) {
        for (AudioDeviceInfo deviceInfo : devices) {
            boolean isCapture = deviceInfo.isSink();
            if (!isCapture || deviceInfo.getType() != AudioDeviceInfo.TYPE_TELEPHONY) {
                addAudioDevice(isCapture, deviceInfo.getId());
            }
        }
    }

    public static void initialize() {
        mAudioDeviceCallback = null;

        if (Build.VERSION.SDK_INT >= 23 /* Android 6.0 (M) */) {
            mAudioDeviceCallback = new AudioDeviceCallback() {
                @Override
                public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
                    SDLAudioManager.addAudioDevices(addedDevices);
                }

                @Override
                public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
                    for (AudioDeviceInfo deviceInfo : removedDevices) {
                        removeAudioDevice(deviceInfo.isSink(), deviceInfo.getId());
                    }
                }
            };
        }
    }

    public static void setContext(Context context) {
        mContext = context;
        if (context != null) {
            registerAudioDeviceCallback();
        }
    }

    public static void release(Context context) {
        unregisterAudioDeviceCallback(context);
    }

    // Audio
    private static void registerAudioDeviceCallback() {
        if (Build.VERSION.SDK_INT >= 23 /* Android 6.0 (M) */) {
            AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
            audioManager.registerAudioDeviceCallback(mAudioDeviceCallback, null);
        }
    }

    private static void unregisterAudioDeviceCallback(Context context) {
        if (Build.VERSION.SDK_INT >= 23 /* Android 6.0 (M) */) {
            AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            audioManager.unregisterAudioDeviceCallback(mAudioDeviceCallback);
        }
    }

    /**
     * This method is called by SDL using JNI.
     */
    public static void audioDetectDevices() {
        if (Build.VERSION.SDK_INT >= 23 /* Android 6.0 (M) */) {
            AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
            AudioDeviceInfo[] devices = audioManager.getDevices(AudioManager.GET_DEVICES_ALL);
            SDLAudioManager.addAudioDevices(devices);
        }
    }

    /** This method is called by SDL using JNI. */
    public static void audioSetThreadPriority(boolean iscapture, int device_id) {
        try {

            /* Set thread name */
            Thread.currentThread().setName((iscapture ? "SDLAudioC" : "SDLAudioP") + device_id);

            /* Set thread priority */
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);

        } catch (Exception e) {
            Log.v(TAG, "modify thread properties failed " + e.toString());
        }
    }

    public static native void nativeSetupJNI();

    public static native void removeAudioDevice(boolean isCapture, int deviceId);

    public static native void addAudioDevice(boolean isCapture, int deviceId);

}
