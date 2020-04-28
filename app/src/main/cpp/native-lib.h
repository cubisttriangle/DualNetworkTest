#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_cubist_dualnetworktest_MainActivity_launchSockets( JNIEnv* env, jobject mainActivityObj );

extern "C" JNIEXPORT bool JNICALL
Java_com_cubist_dualnetworktest_MainActivity_nativeInit( JNIEnv* env, jobject mainActivityObj );

void updateActivityText( const std::string& text );
