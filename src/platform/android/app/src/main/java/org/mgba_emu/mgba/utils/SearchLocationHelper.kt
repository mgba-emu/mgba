/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import android.content.Context
import android.content.SharedPreferences
import android.net.Uri
import androidx.core.content.edit
import androidx.core.net.toUri
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.mgba_emu.mgba.core.Core
import org.mgba_emu.mgba.database.AppDatabase
import org.mgba_emu.mgba.mGBAApplication
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.IconMetadataHelper.getIconUrl

object SearchLocationHelper {
    private val prefs: SharedPreferences by lazy {
        mGBAApplication.context.getSharedPreferences("app_prefs", Context.MODE_PRIVATE)
    }
    private const val GAME_FOLDERS = "game_folders"

    private val gameDao = AppDatabase.getDatabase(mGBAApplication.context).gameDao()

    val gameList: StateFlow<List<GameModel>> = gameDao.getAllGames()
        .map { entities ->
            entities.map { entity ->
                GameModel(
                    uri = entity.uri.toUri(),
                    fileName = entity.fileName,
                    code = entity.code,
                    iconUrl = entity.iconUrl,
                    platform = entity.platform,
                    title = entity.title,
                    version = entity.version,
                    lastPlayed = entity.lastPlayed
                )
            }
        }
        .stateIn(
            scope = CoroutineScope(Dispatchers.IO),
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = emptyList()
        )

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading

    private val coreMutex = Mutex()

    init {
        loadRoms()
    }

    fun saveFolderUri(uri: Uri) {
        val savedUris = prefs.getStringSet(GAME_FOLDERS, mutableSetOf())?.toMutableSet() ?: mutableSetOf()
        savedUris.add(uri.toString())
        prefs.edit { putStringSet(GAME_FOLDERS, savedUris) }
    }

    fun removeFolder(uri: Uri) {
        val savedUris = prefs.getStringSet(GAME_FOLDERS, mutableSetOf())?.toMutableSet() ?: mutableSetOf()
        savedUris.remove(uri.toString())
        prefs.edit { putStringSet(GAME_FOLDERS, savedUris) }
    }

    fun getGameFolders(): List<Uri> {
        val savedUris = prefs.getStringSet(GAME_FOLDERS, emptySet()) ?: emptySet()
        return savedUris.map { Uri.parse(it) }
    }

    fun isFolderExists(folder: Uri): Boolean {
        return getGameFolders().contains(folder)
    }

    fun loadRoms() {
        if (_isLoading.value) return
        if (!Core.init()) return
        CoroutineScope(Dispatchers.IO).launch {
            _isLoading.value = true

            val context = mGBAApplication.context
            val folderUris = getGameFolders()

            val searchJobs = folderUris.map { folderUri ->
                async { FileUtils.searchRoms(context, folderUri) }
            }
            val discoveredFiles = searchJobs.awaitAll().flatten()
            val discoveredUris = discoveredFiles.map { it.first.toString() }

            val cachedUrisSet = gameDao.getAllCachedUris().toSet()

            val newFilesToValidate = discoveredFiles.filter { (uri, _) ->
                !cachedUrisSet.contains(uri.toString())
            }

            val newGameEntities = newFilesToValidate.mapNotNull { (fileUri, fileName) ->
                coreMutex.withLock {
                    if (Core.validateRom(fileUri)) {
                        val platform = Core.getPlatform()
                        val title = Core.gameTitle()

                        GameEntity(
                            uri = fileUri.toString(),
                            fileName = fileName,
                            code = Core.gameCode(),
                            iconUrl = getIconUrl(title, platform) ?: "",
                            platform = platform,
                            title = title,
                            version = Core.gameVersion
                        )
                    } else {
                        null
                    }
                }
            }

            if (newGameEntities.isNotEmpty()) {
                gameDao.insertGames(newGameEntities)
            }

            if (discoveredUris.isEmpty()) {
                gameDao.deleteAllGames()
            } else {
                gameDao.deleteOrphans(discoveredUris)
            }

            _isLoading.value = false
        }
    }

    fun updateLastPlayed(uri: String, timestamp: Long) {
        CoroutineScope(Dispatchers.IO).launch {
            gameDao.updateLastPlayed(uri, timestamp)
        }
    }
}
