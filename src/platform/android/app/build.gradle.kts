import java.util.Base64

plugins {
    id("com.android.application")
}

fun signingValue(propertyName: String, environmentName: String): String? {
    return providers.gradleProperty(propertyName)
        .orElse(providers.environmentVariable(environmentName))
        .orNull
        ?.takeIf { it.isNotBlank() }
}

fun booleanValue(propertyName: String, environmentName: String): Boolean {
    return signingValue(propertyName, environmentName)?.toBooleanStrictOrNull() == true
}

fun listValue(propertyName: String, environmentName: String): List<String> {
    return signingValue(propertyName, environmentName)
        ?.split(",")
        ?.map { it.trim() }
        ?.filter { it.isNotEmpty() }
        .orEmpty()
}

val releaseKeystoreFile = signingValue("mgbaAndroidKeystoreFile", "MGBA_ANDROID_KEYSTORE_FILE")
val releaseKeystoreBase64 = signingValue("mgbaAndroidKeystoreBase64", "MGBA_ANDROID_KEYSTORE_BASE64")
val releaseKeystorePassword = signingValue("mgbaAndroidKeystorePassword", "MGBA_ANDROID_KEYSTORE_PASSWORD")
val releaseKeyAlias = signingValue("mgbaAndroidKeyAlias", "MGBA_ANDROID_KEY_ALIAS")
val releaseKeyPassword = signingValue("mgbaAndroidKeyPassword", "MGBA_ANDROID_KEY_PASSWORD")
val nativeWarningsAsErrors = booleanValue("mgbaAndroidWarningsAsErrors", "MGBA_ANDROID_WARNINGS_AS_ERRORS")
val androidAbiFilters = listValue("mgbaAndroidAbiFilters", "MGBA_ANDROID_ABI_FILTERS")
    .ifEmpty { listOf("arm64-v8a", "armeabi-v7a", "x86_64") }
val generatedReleaseKeystore = layout.buildDirectory.file("generated/signing/mgba-release.jks").get().asFile
val releaseKeystore = releaseKeystoreFile?.let { file(it) } ?: releaseKeystoreBase64?.let { encoded ->
    generatedReleaseKeystore.parentFile.mkdirs()
    generatedReleaseKeystore.writeBytes(Base64.getDecoder().decode(encoded))
    generatedReleaseKeystore
}
val hasReleaseSigning = releaseKeystore != null &&
    releaseKeystorePassword != null &&
    releaseKeyAlias != null &&
    releaseKeyPassword != null

android {
    namespace = "io.mgba.android"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "io.mgba.android"
        minSdk = 23
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DMGBA_ANDROID_WARNINGS_AS_ERRORS=$nativeWarningsAsErrors",
                )
            }
        }

        ndk {
            abiFilters += androidAbiFilters
        }
    }

    signingConfigs {
        if (hasReleaseSigning) {
            create("release") {
                storeFile = releaseKeystore
                storePassword = releaseKeystorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }

    buildTypes {
        debug {
            isJniDebuggable = true
        }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            ndk {
                debugSymbolLevel = "FULL"
            }
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
            if (hasReleaseSigning) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

}

dependencies {
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
}
