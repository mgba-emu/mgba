package io.mgba.android

import android.app.Application
import android.content.Context
import io.mgba.android.settings.LanguageModes
import io.mgba.android.storage.AppLogStore
import kotlin.system.exitProcess

class MGBApplication : Application() {
    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(LanguageModes.wrapContext(base))
    }

    override fun onCreate() {
        super.onCreate()
        LanguageModes.applyProcessLocale(this)
        val previousHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            val message = "Uncaught exception on ${thread.name}"
            AppLogStore.markCrash(this, message)
            AppLogStore.append(this, message, throwable)
            if (previousHandler != null) {
                previousHandler.uncaughtException(thread, throwable)
            } else {
                exitProcess(2)
            }
        }
        AppLogStore.append(this, "Application started")
    }
}
