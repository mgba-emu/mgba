package io.mgba.android

import android.app.Application
import io.mgba.android.storage.AppLogStore
import kotlin.system.exitProcess

class MGBApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        val previousHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            AppLogStore.append(this, "Uncaught exception on ${thread.name}", throwable)
            if (previousHandler != null) {
                previousHandler.uncaughtException(thread, throwable)
            } else {
                exitProcess(2)
            }
        }
        AppLogStore.append(this, "Application started")
    }
}
