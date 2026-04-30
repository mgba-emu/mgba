package io.mgba.android

import io.mgba.android.settings.AutoStateSettings
import io.mgba.android.settings.FastForwardModes

object HelpContent {
    fun text(autoStateIntervalSeconds: Int): String {
        val interval = AutoStateSettings.coerceIntervalSeconds(autoStateIntervalSeconds)
        val fastValues = FastForwardModes.multiplierLabels.joinToString(", ")
        return """
            Game toolbar

            Pause / Resume: pause or continue emulation.
            Reset: restart the current game.
            Fast / Hold / 1x-8x: fast-forward. Toggle mode turns it on/off with a tap; Hold mode only runs fast while pressed.
            Rw / Rw*: hold to rewind. Rw* means rewinding is active.
            Stats / Stats*: show or hide performance and runtime information.
            State: open save-state, autosave, battery save, cheat, patch, and exit actions.
            More: open runtime options, video/audio/input options, screenshots, diagnostics, and this Help page.

            State And Data

            Load: choose Autosave or a numbered save-state slot from a thumbnail list.
            Save: choose a numbered save-state slot to write; Autosave Now writes the autosave slot immediately.
            States: manage Autosave and all numbered save-state slots, including delete/export/import.
            AutoSave: write the autosave slot immediately.
            AutoLoad: load the autosave slot immediately.
            Backup: export the battery save file.
            DataOut / DataIn: export/import the full game data package, including settings, battery save, autosave, save states, thumbnails, cheats, patches, BIOS overrides, and camera image.
            Import: import a battery save file.
            Cheats / NoCheat: manage or clear cheats for this game.
            Patch / NoPatch: import or clear a ROM patch for this game.
            Exit: leave the game screen.

            Main settings

            Search: focus the ROM library search box.
            Add: open one ROM or add/scan a folder.
            Settings: show or hide emulator settings.
            Resume Game: return to a running game session.
            Open ROM: pick a ROM file.
            Scan Folder / Add Folder: index ROM files from a folder.
            Library Folders: manage remembered scan folders.
            Import BIOS / Clear BIOS: manage global BIOS files.
            Skip BIOS: start games without the BIOS intro when possible.
            Import Patch: import a global patch file.
            About: version and license information.
            Export Logs: export app logs for debugging.
            Storage: inspect private app storage and game artifacts.
            Clear Cache: clear temporary archive/import cache files.
            Export Settings / Import Settings: back up or restore global settings.

            Values

            Auto State On: automatically loads the autosave when a game starts, saves every Auto Interval while the game screen is active, and saves once more when the game pauses or exits.
            Auto State Off: disables automatic load/save. Manual AutoSave and AutoLoad still work.
            Auto Interval: seconds between periodic autosaves. Current value: ${interval}s. Default: ${AutoStateSettings.DefaultIntervalSeconds}s. Range: ${AutoStateSettings.MinIntervalSeconds}-${AutoStateSettings.MaxIntervalSeconds}s.

            Fast Mode Toggle: tap Fast once to enable, tap again to disable.
            Fast Mode Hold: fast-forward only while pressing Fast.
            Fast Speed: $fastValues. 1x is normal speed; larger values speed up both video and audio.

            Rewind On/Off: enable or disable rewind.
            Rewind Buffer 300/600/900/1200: rewind history capacity. Larger values keep more history and use more memory.
            Rewind Speed 1/2/3/4: frame spacing between rewind points. 1 is smoothest; larger values use less memory and may jump farther.

            Scale Fit: preserve aspect and fit inside the screen.
            Scale Fill: preserve aspect and fill more of the screen.
            Scale Integer: use integer scaling for sharp pixels.
            Scale Original: native-size video.
            Scale Stretch: fill the target area without preserving aspect.

            Filter Pixel: sharp nearest-neighbor pixels.
            Filter Smooth: smoother scaled video.
            Interframe On: blend adjacent frames to reduce flicker.
            Interframe Off: original frame presentation.
            Frame Skip 0/1/2/3: skip no frames or increasingly more frames to trade smoothness for performance.

            Audio Buffer Low Latency: lowest delay, more sensitive to crackle.
            Audio Buffer Balanced: default compromise.
            Audio Buffer Stable: more buffering for smoother audio.
            Low Pass Off/40%/60%/80%: audio smoothing strength; higher values sound softer.
            Volume 100/75/50/25: output volume percent.
            Mute / Sound: disable or restore audio output.

            Rotation Auto: follow device rotation.
            Rotation Landscape: lock landscape.
            Rotation Portrait: lock portrait.
            Pad On/Off: show or hide the virtual controls.
            Pad Settings: adjust virtual control size, opacity, spacing, haptics, handedness, and layout.
            Deadzone 25-65: analog stick threshold; larger values ignore more drift.
            Opposite Directions On: allow Up+Down or Left+Right at the same time.
            Opposite Directions Off: resolve opposing directions to one direction.
            Rumble On/Off: enable or disable vibration.
            Tilt On/Off: map device motion to tilt/gyro input.
            Cal: calibrate current tilt as neutral.
            Solar: set or sensor-drive solar level for Boktai-style games.
            Camera: import/capture a Game Boy Camera image source.
            Keys: map hardware keyboard/controller buttons.
            Diag: export runtime diagnostics.
            GDB: toggle the debugging stub.
            Shot / Export: share or export screenshots.

            Language: choose English, Simplified Chinese, Traditional Chinese, Japanese, or Russian.
            Log Level Warn/Info/Debug: exported/runtime log detail.
            RTC Clock: use real wall-clock time.
            RTC Fixed: freeze the real-time clock at one fixed instant.
            RTC Fake: use a fixed fake epoch.
            RTC Offset: use wall-clock time plus an offset.
        """.trimIndent()
    }
}
