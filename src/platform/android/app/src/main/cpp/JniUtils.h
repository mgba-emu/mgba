#ifndef MGBA_ANDROID_JNI_UTILS_H
#define MGBA_ANDROID_JNI_UTILS_H

#include <jni.h>

#include <string>

namespace mgba::android {

std::string JStringToString(JNIEnv* env, jstring value);

} // namespace mgba::android

#endif
