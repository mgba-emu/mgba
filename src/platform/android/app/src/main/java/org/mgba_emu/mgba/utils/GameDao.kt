/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */


package org.mgba_emu.mgba.utils

import androidx.room.Dao
import androidx.room.Entity
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.PrimaryKey
import androidx.room.Query
import kotlinx.coroutines.flow.Flow
import org.mgba_emu.mgba.core.Platform

@Entity(tableName = "games")
data class GameEntity(
    @PrimaryKey val uri: String,
    val fileName: String,
    val code: String,
    val iconUrl: String,
    val platform: Platform,
    val title: String,
    val version: String,
    var lastPlayed: Long = 0L
)

@Dao
interface GameDao {
    @Query("SELECT * FROM games")
    fun getAllGames(): Flow<List<GameEntity>>

    @Query("SELECT uri FROM games")
    suspend fun getAllCachedUris(): List<String>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertGames(games: List<GameEntity>)

    @Query("UPDATE games SET lastPlayed = :timestamp WHERE uri = :uri")
    suspend fun updateLastPlayed(uri: String, timestamp: Long)

    @Query("DELETE FROM games WHERE uri NOT IN (:existingUris)")
    suspend fun deleteOrphans(existingUris: List<String>)
}