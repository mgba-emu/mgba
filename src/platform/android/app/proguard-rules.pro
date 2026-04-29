-keep class io.mgba.android.MGBApplication { *; }
-keep class io.mgba.android.MainActivity { *; }
-keep class io.mgba.android.EmulatorActivity { *; }
-keep class io.mgba.android.storage.ScreenshotShareProvider { *; }
-keep class io.mgba.android.bridge.NativeBridge { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}
-keepattributes Exceptions,InnerClasses,Signature
