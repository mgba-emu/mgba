#include "JniUtils.h"

namespace mgba::android {

std::string JStringToString(JNIEnv* env, jstring value) {
	if (!value) {
		return {};
	}
	const char* chars = env->GetStringUTFChars(value, nullptr);
	if (!chars) {
		return {};
	}
	std::string result(chars);
	env->ReleaseStringUTFChars(value, chars);
	return result;
}

} // namespace mgba::android
