package io.mgba.android.settings

import android.content.Context
import io.mgba.android.input.GbaButtons
import io.mgba.android.input.HardwareKeyProfile
import org.json.JSONObject

class InputMappingStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("input_mappings", Context.MODE_PRIVATE)

    fun profile(gameId: String?, deviceDescriptor: String? = null): HardwareKeyProfile {
        var profile = HardwareKeyProfile.defaultProfile()
        readScopes(gameId, deviceDescriptor).forEach { scope ->
            GbaButtons.All.forEach { button ->
                val key = keyForScope(scope, button.mask)
                if (preferences.contains(key)) {
                    profile = profile.withKeyCode(button.mask, preferences.getInt(key, 0))
                }
            }
        }
        return profile
    }

    fun setKeyCode(gameId: String?, mask: Int, keyCode: Int): Boolean {
        return setKeyCode(gameId, null, mask, keyCode)
    }

    fun setKeyCode(gameId: String?, deviceDescriptor: String?, mask: Int, keyCode: Int): Boolean {
        if (!HardwareKeyProfile.isSupportedMask(mask)) {
            return false
        }
        val scope = writeScope(gameId, deviceDescriptor)
        val editor = preferences.edit()
        GbaButtons.All.forEach { button ->
            val key = keyForScope(scope, button.mask)
            if (button.mask != mask && preferences.getInt(key, Int.MIN_VALUE) == keyCode) {
                editor.remove(key)
            }
        }
        editor.putInt(keyForScope(scope, mask), keyCode).apply()
        return true
    }

    fun reset(gameId: String?) {
        reset(gameId, deviceDescriptor = null)
    }

    fun reset(gameId: String?, deviceDescriptor: String?) {
        val scopes = if (deviceDescriptor.isNullOrBlank()) {
            listOfNotNull(legacyGameScope(gameId), gameScope(gameId)).distinct().ifEmpty { listOf(GLOBAL_SCOPE) }
        } else {
            listOfNotNull(deviceScope(deviceDescriptor))
        }
        val editor = preferences.edit()
        scopes.forEach { scope ->
            GbaButtons.All.forEach { button ->
                editor.remove(keyForScope(scope, button.mask))
            }
        }
        editor.apply()
    }

    fun exportProfileJson(gameId: String?, deviceDescriptor: String?, deviceName: String?): String {
        val profile = profile(gameId, deviceDescriptor)
        val mappings = JSONObject()
        GbaButtons.All.forEach { button ->
            mappings.put(button.label, profile.keyCodeForMask(button.mask) ?: 0)
        }
        return JSONObject()
            .put("version", 1)
            .put("scope", writeScope(gameId, deviceDescriptor))
            .put("deviceDescriptor", deviceDescriptor.orEmpty())
            .put("deviceName", deviceName.orEmpty())
            .put("mappings", mappings)
            .toString(2)
    }

    fun importProfileJson(gameId: String?, deviceDescriptor: String?, json: String): Boolean {
        val mappings = runCatching {
            JSONObject(json).optJSONObject("mappings")
        }.getOrNull() ?: return false

        reset(gameId, deviceDescriptor)
        var imported = 0
        GbaButtons.All.forEach { button ->
            if (mappings.has(button.label)) {
                val keyCode = mappings.optInt(button.label, 0)
                if (keyCode > 0 && setKeyCode(gameId, deviceDescriptor, button.mask, keyCode)) {
                    imported += 1
                }
            }
        }
        return imported > 0
    }

    private fun readScopes(gameId: String?, deviceDescriptor: String?): List<String> {
        return buildList {
            add(GLOBAL_SCOPE)
            legacyGameScope(gameId)?.let { add(it) }
            gameScope(gameId)?.let { add(it) }
            deviceScope(deviceDescriptor)?.let { add(it) }
        }.distinct()
    }

    private fun writeScope(gameId: String?, deviceDescriptor: String?): String {
        return deviceScope(deviceDescriptor) ?: gameScope(gameId) ?: GLOBAL_SCOPE
    }

    private fun legacyGameScope(gameId: String?): String? {
        return gameId?.takeIf { it.isNotBlank() }
    }

    private fun gameScope(gameId: String?): String? {
        val id = gameId?.takeIf { it.isNotBlank() } ?: return null
        return "game::$id"
    }

    private fun deviceScope(deviceDescriptor: String?): String? {
        val descriptor = deviceDescriptor?.takeIf { it.isNotBlank() } ?: return null
        return "device::$descriptor"
    }

    private fun keyForScope(scope: String, mask: Int): String {
        return "$scope::$mask"
    }

    private companion object {
        const val GLOBAL_SCOPE = "global"
    }
}
