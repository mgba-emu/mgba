/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

package org.mgba_emu.mgba.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.flow.stateIn
import org.mgba_emu.mgba.model.GameModel
import org.mgba_emu.mgba.utils.SearchLocationHelper

enum class SearchFilterType {
    ALL,
    RECENTLY_PLAYED,
    FAVORITES
}

class SearchViewModel : ViewModel() {
    val searchQuery = MutableStateFlow("")
    val selectedFilterType = MutableStateFlow(SearchFilterType.ALL)

    val searchResults: StateFlow<List<GameModel>> = combine(
        SearchLocationHelper.gameList,
        searchQuery,
        selectedFilterType
    ) { games, query, filter ->
        filterGames(games, query.trim(), filter)
    }
        .flowOn(Dispatchers.IO)
        .stateIn(
            scope = viewModelScope,
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = emptyList()
        )

    private fun filterGames(
        games: List<GameModel>,
        query: String,
        filter: SearchFilterType
    ): List<GameModel> {
        return games.asSequence()
            .filter { game ->
                query.isEmpty() || (game.title ?: game.fileName).contains(query, ignoreCase = true)
            }
            .filter { game ->
                when (filter) {
                    SearchFilterType.ALL -> true
                    SearchFilterType.RECENTLY_PLAYED -> game.lastPlayed > 0L
                    SearchFilterType.FAVORITES -> false // TODO: Add favorite flag check
                }
            }
            .let { sequence ->
                if (filter == SearchFilterType.RECENTLY_PLAYED) {
                    sequence.sortedByDescending { it.lastPlayed }
                } else {
                    sequence
                }
            }
            .toList()
    }

    fun onSearchQueryChanged(newQuery: String) {
        searchQuery.value = newQuery
    }

    fun onFilterTypeChanged(filterType: SearchFilterType) {
        selectedFilterType.value = filterType
    }
}
