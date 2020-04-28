#include <jni.h>
#include <string>
#include <android/log.h>
#include "native-lib.h"
#include "SocketThread.h"

static SocketThread socketThread;
static JavaVM* jvm = 0;
static jobject mainActivity = 0;
static jmethodID updateTextId = 0;

extern "C" JNIEXPORT jstring JNICALL
Java_com_cubist_dualnetworktest_MainActivity_launchSockets(
        JNIEnv* env,
        jobject /* pThis */ )
{
    socketThread.Run( {
        {},
        { "192.168.1.9" }, // Change this to match your eth addr
        { "192.168.1.16" } // Change this to match your wifi addr
    });

    return env->NewStringUTF( "Launching sockets." );
}

extern "C" JNIEXPORT bool JNICALL
Java_com_cubist_dualnetworktest_MainActivity_nativeInit(
        JNIEnv* env,
        jobject mainActivityObj )
{
    env->GetJavaVM( &jvm );

    mainActivity = env->NewGlobalRef( mainActivityObj );

    jclass localClass = env->FindClass( "com/cubist/dualnetworktest/MainActivity" );
    if ( localClass )
    {
        updateTextId = env->GetMethodID( localClass, "updateText", "(Ljava/lang/String;)V" );
        if ( !updateTextId )
        {
            __android_log_print( ANDROID_LOG_ERROR, "JNI", "Couldn't get main activity updateString method." );
            return false;
        }
    }
    else
    {
        __android_log_print( ANDROID_LOG_ERROR, "JNI", "Couldn't get main activity class." );
        return false;
    }

    return true;
}

void updateActivityText( const std::string& text )
{
    JNIEnv* env;
    jvm->AttachCurrentThread( &env, NULL );

    env->CallVoidMethod( mainActivity, updateTextId, env->NewStringUTF( text.c_str() ) );

    jvm->DetachCurrentThread();
}
