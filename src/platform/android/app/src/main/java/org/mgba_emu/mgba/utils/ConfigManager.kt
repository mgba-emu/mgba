/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.utils

import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.PreferenceDataStoreFactory
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.emptyPreferences
import androidx.datastore.preferences.core.intPreferencesKey
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import java.io.File
import java.util.concurrent.ConcurrentHashMap

object ConfigManager {
    private var scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    private lateinit var globalDataStore: DataStore<Preferences>
    private var gameDataStore: DataStore<Preferences>? = null
    private val gameDataStoreCache = ConcurrentHashMap<String, DataStore<Preferences>>()

    private val globalPrefsState = MutableStateFlow(emptyPreferences())
    private val gamePrefsState = MutableStateFlow<Preferences?>(null)

    private var globalCollectJob: Job? = null
    private var gameCollectJob: Job? = null

    var gameFileName: String? = null
        set(value) {
            if (field == value) return
            field = value
            setupGameDataStore(value)
        }


    var shouldHandleInGameMenu = false

    init {
        setupGlobalDataStore()
    }

    private fun setupGlobalDataStore() {
        globalCollectJob?.cancel()
        globalDataStore = createDataStore("app_prefs.preferences_pb", scope)

        globalCollectJob = scope.launch {
            globalDataStore.data.collect { globalPrefsState.value = it }
        }
    }

    private fun setupGameDataStore(fileName: String?) {
        gameCollectJob?.cancel()

        if (fileName != null) {
            val dataStore = gameDataStoreCache.getOrPut(fileName) {
                createDataStore("game_config_$fileName.preferences_pb", scope)
            }
            gameDataStore = dataStore

            gamePrefsState.value = runBlocking { dataStore.data.first() }
            gameCollectJob = scope.launch {
                dataStore.data.collect { gamePrefsState.value = it }
            }
        } else {
            gameDataStore = null
            gamePrefsState.value = null
        }
    }

    private fun createDataStore(fileName: String, scope: CoroutineScope): DataStore<Preferences> {
        return PreferenceDataStoreFactory.create(
            scope = scope,
            produceFile = { File(FileUtils.getConfigDir(), fileName) }
        )
    }

    fun getBoolean(key: String, defaultValue: Boolean = false): Boolean {
        val prefKey = booleanPreferencesKey(key)
        gamePrefsState.value?.let { gamePrefs ->
            if (gamePrefs.contains(prefKey)) {
                return gamePrefs[prefKey] ?: defaultValue
            }
        }

        return globalPrefsState.value[prefKey] ?: defaultValue
    }

    fun getBooleanFlow(key: String, defaultValue: Boolean = false): Flow<Boolean> {
        val prefKey = booleanPreferencesKey(key)
        return combine(gamePrefsState, globalPrefsState) { gamePrefs, globalPrefs ->
            if (gamePrefs != null && gamePrefs.contains(prefKey)) {
                return@combine gamePrefs[prefKey] ?: defaultValue
            }

            return@combine globalPrefs[prefKey] ?: defaultValue
        }
    }

    fun getInt(key: String, defaultValue: Int = 0): Int {
        val prefKey = intPreferencesKey(key)

        gamePrefsState.value?.let { gamePrefs ->
            if (gamePrefs.contains(prefKey)) {
                return gamePrefs[prefKey] ?: defaultValue
            }
        }

        return globalPrefsState.value[prefKey] ?: defaultValue
    }

    fun getIntFlow(key: String, defaultValue: Int = 0): Flow<Int> {
        val prefKey = intPreferencesKey(key)
        return combine(gamePrefsState, globalPrefsState) { gamePrefs, globalPrefs ->
            if (gamePrefs != null && gamePrefs.contains(prefKey)) {
                return@combine gamePrefs[prefKey] ?: defaultValue
            }

            return@combine globalPrefs[prefKey] ?: defaultValue
        }
    }

    fun setBoolean(key: String, value: Boolean) {
        val prefKey = booleanPreferencesKey(key)

        if (shouldHandleInGameMenu && !hasGameOverride(key)) {
            return runBlocking { globalDataStore.edit { it[prefKey] = value } }
        }

        runBlocking {
            if (gameDataStore != null) {
                gameDataStore!!.edit { it[prefKey] = value }
            } else {
                globalDataStore.edit { it[prefKey] = value }
            }
        }
    }

    fun setInt(key: String, value: Int) {
        val prefKey = intPreferencesKey(key)

        if (shouldHandleInGameMenu && !hasGameOverride(key)) {
            return runBlocking { globalDataStore.edit { it[prefKey] = value } }
        }

        runBlocking {
            if (gameDataStore != null) {
                gameDataStore!!.edit { it[prefKey] = value }
            } else {
                globalDataStore.edit { it[prefKey] = value }
            }
        }
    }

    fun removeGameOverride(key: String) {
        runBlocking {
            gameDataStore?.edit { prefs ->
                prefs.remove(booleanPreferencesKey(key))
                prefs.remove(intPreferencesKey(key))
            }
        }
    }

    fun hasGameOverride(key: String): Boolean {
        val gamePrefs = gamePrefsState.value ?: return false
        return gamePrefs.contains(booleanPreferencesKey(key)) ||
                gamePrefs.contains(intPreferencesKey(key))
    }

    fun reinit() {
        scope.cancel()
        scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        gameDataStoreCache.clear()
        setupGlobalDataStore()
        setupGameDataStore(gameFileName)
    }
}