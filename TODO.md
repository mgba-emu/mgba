# Android 原生版复刻落地 TODO

> 目标：在当前 mGBA 仓库内新增一个 **原生 Android 端口**，复用现有 C/C++ 模拟核心，补齐 Android 应用壳层、JNI 桥、渲染、音频、输入、存档、设置、ROM 库和发布链路，最终达到 Win / macOS / Linux 桌面版主要用户功能的 Android 形态复刻。

## 0. 当前仓库基线结论

- [ ] 当前仓库已有核心模拟层：`src/core`、`src/gba`、`src/gb`、`include/mgba/**`。
- [ ] 当前仓库已有桌面/主机前端：`src/platform/qt`、`src/platform/sdl`，以及主机/掌机端口：`src/platform/3ds`、`src/platform/switch`、`src/platform/psp2`、`src/platform/wii`、`src/platform/libretro`。
- [ ] `PORTING.md` 明确要求新平台代码放在 `src/platform/<port>` 下，并尽量减少对核心树的侵入。
- [ ] 顶层 `CMakeLists.txt` 已有部分 `ANDROID` 判断和 POSIX/VFS 支持，但没有 Android Gradle 工程、Manifest、Activity、JNI 入口、Android 音视频/输入/存储实现。
- [ ] mGBA 核心已经提供 Android 可复用的关键 API：
  - ROM 探测/加载：`mCoreFindVF`、`mCoreFind`、`mCoreLoadFile`、`core->loadROM`。
  - 文件抽象：`VFileFromFD`、`VFileOpen`、`VFileFromMemory`、`VDirOpen`。
  - 视频：`core->setVideoBuffer`、`core->currentVideoSize`、`mGLES2Context`。
  - 音频：`core->getAudioBuffer`、`core->audioSampleRate`、`core->setAudioBufferSize`、`mAudioResampler`。
  - 线程：`mCoreThreadStart`、`mCoreThreadPause`、`mCoreThreadUnpause`、`mCoreThreadRunFunction`、`mCoreThreadEnd`。
  - 输入：`core->setKeys`、`core->addKeys`、`core->clearKeys`、`GBA_KEY_*`。
  - 存档/即时存档：`mCoreAutoloadSave`、`mCoreSaveState`、`mCoreLoadState`、`mCoreSaveStateNamed`、`mCoreLoadStateNamed`。
  - 配置：`mCoreInitConfig`、`mCoreConfigLoadDefaults`、`mCoreLoadConfig`、`mCoreConfigSet*`。

## 当前实现进度

- [x] 已创建 `src/platform/android` Android 原生工程骨架。
- [x] 已创建 Kotlin Activity 壳层：`MainActivity`、`EmulatorActivity`、`MGBApplication`。
- [x] 已创建 JNI 桥入口：`NativeBridge.nativeGetVersion`、`nativeCreate`、`nativeDestroy`。
- [x] 已创建 native CMake target：`mgba-android`。
- [x] 已将 Android native target 链接到现有 mGBA static core。
- [x] 已用 Android NDK 28.2.13676358 直接验证 `arm64-v8a`、`armeabi-v7a`、`x86_64` 均可编译 `libmgba-android.so`。
- [x] 已用 Gradle 9.3.1 + AGP 9.1.0 完整验证 `:app:assembleDebug`。
- [x] 已实现 Android SAF 文件选择到 native fd 的 ROM 加载探测链路。
- [x] 已实现 `SurfaceView` 到 native `ANativeWindow` / EGL / GLES2 的最小渲染链路。
- [x] 已实现 native run loop，加载 ROM 后可连续 `runFrame` 并上传 frame buffer 到 GLES2 texture。
- [x] 已实现触屏虚拟手柄视图，并把 A/B/L/R/Start/Select/D-pad 映射为 mGBA core key bitmask。
- [x] 已实现 JNI `nativeSetKeys` 到 `core->setKeys` 的输入链路。
- [x] 已实现 OpenSL ES 音频输出器，从 mGBA audio buffer 拉取样本并重采样到 Android 48 kHz stereo PCM。
- [x] 已实现实体键盘、D-pad、gamepad buttons 和 joystick axes 到 mGBA key bitmask 的基础映射。
- [x] 已实现基础电池存档链路：按 ROM CRC32 在 app 私有 `saves/` 目录创建 `.sav` 并交给 core 读写。
- [x] 已实现 1-9 槽即时存档/读档 native 链路，并在 emulator 画面提供基础槽位工具条。
- [x] 已实现基础运行控制：暂停/继续、重置和快进开关。
- [x] 已通过 `mCoreTakeScreenshotVF` 实现 PNG 截图保存，输出到 app 私有 `screenshots/` 目录。
- [x] 已实现截图只读 `ContentProvider` 分享，可从 emulator 工具条直接调用 Android 分享面板。
- [x] 已实现 Android 10+ MediaStore 截图导出到 `Pictures/mGBA`。
- [x] 已实现 SAF URI 最近打开列表，可从首页直接重新打开已授权 ROM。
- [x] 已实现 SAF 文件夹递归扫描 ROM，并在首页渲染可直接启动的 ROM 库列表。
- [x] 已实现 BIOS SAF 导入到 app 私有目录，并在 native ROM 加载时尝试交给 core。
- [x] 已实现 patch SAF 导入到 app 私有目录，并在 native ROM 加载后尝试应用。
- [x] 已实现电池存档从 native 内存克隆导出，并通过 MediaStore 写入 Android 10+ `Documents/mGBA`。
- [x] 已实现 SAF 电池存档导入，直接恢复到当前 core savedata 并写回当前游戏存档。
- [x] 已实现 SAF cheat 文件导入，native 侧调用 `mCheatParseFile` 解析并应用到当前 core。
- [x] 已实现渲染缩放模式切换：Fit、Fill、Integer。
- [x] 已实现运行时音频静音/恢复控制，并清空 OpenSL 队列避免旧音频积压。
- [x] 已持久化 emulator 缩放模式和静音状态。
- [x] 已将 native run loop 帧节奏改为使用 core 的 `frameCycles()` / `frequency()` 计算。
- [x] 已实现虚拟手柄显示/隐藏开关并持久化。
- [x] 已实现即时存档槽位覆盖确认。
- [x] 已实现 emulator 画面沉浸全屏与游玩时保持屏幕常亮。
- [x] 已实现虚拟按键按下时的系统 haptic feedback。
- [x] 已提交 Gradle wrapper，Android 工程可直接使用 `./gradlew :app:assembleDebug` 构建。
- [x] 已新增 GitHub Actions Android debug APK 构建 workflow。
- [x] 已修复 Android backup/data-extraction XML 规则，并验证 `:app:bundleRelease` 可生成 release AAB。
- [x] 已扩展 GitHub Actions Android build：debug APK、release APK、release AAB 和 native symbols artifacts。
- [x] 已新增 Android 输入映射单元测试，并验证 `:app:testDebugUnitTest`。
- [x] 已为 ROM 库首页新增搜索过滤、结果计数和空匹配状态。
- [x] 已补齐 ROM 扫描扩展名过滤：`.gba`、`.agb`、`.gb`、`.gbc`、`.sgb`、`.zip`、`.7z`。
- [x] 已新增 Android About 对话框，展示 native 版本、mGBA copyright、MPL 2.0 和不捆绑 ROM/BIOS 声明。
- [x] 已将 ROM 文件夹扫描移到后台线程，扫描期间禁用重复触发并在完成后刷新库。
- [x] 已实现 ROM 文件夹扫描取消，避免旧扫描结果覆盖新库状态。
- [x] 已为 ROM 库记录并显示 last played 时间，重新扫描时保留已有游玩元数据。
- [x] 已为 ROM 库新增收藏标记，收藏项优先排序并随重新扫描保留。
- [x] 已为 ROM 库条目新增删除记录确认操作，只移除索引不删除用户文件。
- [x] 已新增 Emulator debug stats overlay：native 帧计数、视频尺寸、运行状态和 Kotlin 侧 FPS 估算。
- [x] 已新增 Android logcat 导出入口，导出最近日志到 `Documents/mGBA`。
- [x] 已新增 per-game override 基础存储，当前游戏的缩放、静音和虚拟手柄显示不再污染全局设置。
- [x] 已为 ROM 文件夹扫描接入 native 探测，库条目可保存并显示 mGBA 读取到的标题和平台。
- [x] 已将 native CRC32 透传到 ROM 库模型，并支持按 CRC32 搜索。
- [x] 已为 ROM 库记录并显示 DocumentsProvider 文件大小。
- [x] 已为 ROM 库扫描计算 SHA1，并支持按 SHA1 搜索。
- [x] 已新增 ROM 库筛选按钮，支持 All / Favorites / GBA / GB 并与搜索叠加。
- [x] 已将截图回写为 ROM 库 coverPath，并在首页显示截图缩略图。
- [x] 已新增即时存档槽位删除 native/JNI/UI 链路，并带删除确认。
- [x] 已新增单个即时存档槽位 SAF 导出链路。
- [x] 已新增单个即时存档槽位 SAF 导入链路，并在覆盖前确认。
- [x] 已为 ROM 库记录并显示累计 play time。
- [x] 已新增每游戏硬件键位重映射存储、模拟器内配置入口和输入映射单测。
- [x] 已将硬件键位重映射扩展为按 `InputDevice.descriptor` 区分的每设备 profile。
- [x] 已新增即时存档槽位缩略图：保存时生成、切换槽位显示、删除/导入时清理。
- [x] 已新增 ROM 库按 SAF 文件夹来源增量合并扫描，避免重扫一个目录时冲掉其他库来源。
- [x] 已接入 native `mPERIPH_RUMBLE` 到 Android `Vibrator` 的运行时震动反馈。
- [x] 已接入 Android accelerometer/gyroscope 到 native `mPERIPH_ROTATION`，并提供 Tilt 开关和 Cal 校准。
- [x] 已接入 GBA solar luminance 外设，提供手动 Solar 滑条和可选 Android light sensor。
- [x] 已新增硬件输入 profile JSON 导入/导出，支持通过 SAF 备份/迁移当前映射。
- [x] 已新增 ROM 库手动封面导入/清除，封面复制到 app 私有 `covers/` 并按 ROM 稳定 hash 保存。
- [x] 已新增 per-game frame skip 0-3 档运行控制，并在 native run loop 中跳过部分渲染帧。
- [x] 已新增暂停态单帧步进控制，native 可执行一帧并立即刷新画面。
- [x] 已新增 per-game joystick deadzone 25/35/45/55/65% 档位，并用于实体摇杆方向映射。
- [x] 已将 Android trigger axes（LTRIGGER/RTRIGGER/BRAKE/GAS）映射到 GBA L/R。
- [x] 已新增实体输入 debug 面板，显示最后输入设备、按键码、轴值、映射结果和 deadzone。
- [x] 已新增虚拟手柄大小/透明度设置，并按全局或 per-game override 持久化。
- [x] 已新增虚拟手柄触控震动反馈开关，并复用全局/per-game 设置链路。
- [x] 已新增虚拟手柄左右手布局开关，可互换 D-pad 和 A/B 两侧。
- [x] 已新增虚拟手柄按键间距设置，支持实时调整并持久化。
- [x] 已新增虚拟手柄横屏/竖屏自动布局，竖屏时将肩键收进下半区控制区域。
- [x] 已用单测锁定 Android `GbaKeyMask` 与 native `GBA_KEY_*` bit position 的对应关系。
- [x] 已为虚拟手柄 PadCfg 新增 Reset，一键恢复默认布局样式。
- [x] 已补齐 Android 画面比例模式：Fit / Fill / Integer / Original / Stretch，并保持旧 Fit 设置兼容。
- [x] 已新增 Android 旋转模式切换：Follow / Landscape / Portrait，并按游戏持久化。
- [x] 已新增 Android pause/unload 电池存档显式 flush，使用临时文件替换降低写坏风险。
- [x] 已修复 Surface 重建后非用户暂停状态不会自动 resume 的生命周期问题。
- [x] 已在 EmulatorActivity 真正 finish 时关闭 EmulatorSession，释放 native runner/thread/audio。
- [x] 已收敛 Android Surface 生命周期验收：Surface 销毁释放 EGL/window，恢复时重绑并继续渲染。
- [x] 已新增 Android Skip BIOS 全局/每游戏设置，启动前传入 native 并可在 reset/下次启动生效。
- [x] 已新增 Android ZIP archive ROM 启动：单 ROM 自动解压，多 ROM 弹出选择列表，解压结果写入 cache。
- [x] 已新增 Android ZIP cache 手动清理入口。
- [x] 已新增 Android ZIP cache 256 MiB 上限清理，解压后自动删除较旧 cache 文件。
- [x] 已接入 Android `onTrimMemory`，低内存时将 ZIP cache 收缩到 64 MiB。
- [x] 已新增 Android 画面 Pix/Smooth 过滤切换，JNI 透传到 GLES texture filtering 并按游戏持久化。
- [x] 已新增 Android 音量控制：工具条 100/75/50/25% 循环、per-game 持久化、JNI/native PCM 输出缩放和 stats overlay 展示。
- [x] 已新增 Android 音频 buffer 模式：Low/Balanced/Stable 三档、全局启动前应用、运行时 per-game 覆盖和 stats overlay 展示。
- [x] 已新增 Android 音频 underrun 计数，OpenSL 输出补零时累计并在 debug stats overlay 展示。
- [x] 已扩展 Android debug stats overlay：展示 FPS、frame time、core frame counter、ROM platform/title、音频 buffer 和 underrun。
- [x] 已新增 Android 音频低通滤波：复用 libretro 风格单极滤波，支持 Off/40/60/80 全局和 per-game 切换。
- [x] 已新增 Android per-game patch 导入：模拟器内选择 patch 后保存到私有目录，立即调用 `core->loadPatch` 并在下次启动同一 ROM 时自动应用。
- [x] 已增强 Android BIOS 管理：导入后显示文件大小和 SHA-1 摘要，并支持从 app 私有目录清除 BIOS。
- [x] 已新增 Android per-game cheat 持久化：模拟器内导入 cheat 后保存到 `files/cheats`，并在下次启动同一 ROM 时自动应用。
- [ ] 首帧真机/模拟器截图验证待连接 Android 设备后执行。

## 1. 产品目标和范围

### 1.1 必须复刻的核心体验

- [ ] Android 上可以选择并启动 `.gba`、`.gb`、`.gbc` ROM。
- [ ] 支持从 `.zip` 和 `.7z` 中选择/自动识别 ROM。
- [ ] 支持自动存档、电池存档、即时存档 1-9 槽位。
- [x] 支持即时读档、即时存档缩略图、存档覆盖确认。
- [x] 支持 BIOS 配置、内置 BIOS、跳过 BIOS。
- [ ] 支持 IPS / UPS / BPS 补丁自动加载和手动加载。
- [ ] 支持作弊码导入、启用、禁用、编辑、保存。
- [ ] 支持截图。
- [ ] 支持快进、暂停、重置、单帧步进、倒带、帧跳过。
- [ ] 支持触屏虚拟手柄。
- [x] 支持实体手柄、键盘、蓝牙控制器重映射。
- [ ] 支持震动、陀螺仪/加速度计、亮度/太阳传感器替代输入。
- [ ] 支持 Game Boy Camera 图片源的 Android 摄像头桥接。
- [ ] 支持横屏/竖屏布局、沉浸模式、屏幕比例/整数缩放/滤镜设置。
- [ ] 支持 ROM 库、最近打开、搜索、封面/标题信息、最后游玩时间。
- [ ] 支持 per-game override：每个游戏独立 BIOS、画面、音频、输入、作弊、传感器配置。
- [ ] 支持导入/导出存档、状态、截图、配置。
- [ ] 支持崩溃日志、运行日志、性能信息导出。

### 1.2 Android 原生定义

- [ ] UI 使用 Android 原生技术栈：Kotlin + AndroidX + Jetpack Compose 或 XML/View。首选 Kotlin + Compose，但核心渲染面使用 `SurfaceView` / `TextureView` / `GLSurfaceView` 这种 Android 原生组件。
- [ ] 模拟核心继续使用现有 C/C++ mGBA，不重写模拟器。
- [ ] Android 与核心之间使用 JNI/NDK，不引入 Flutter、React Native、Unity、Electron 等跨平台 UI 框架。
- [ ] 不用 SDL 端口直接套 Android。SDL 可作为参考，但 Android 版必须拥有原生 Activity、原生存储授权、原生输入和原生生命周期。
- [ ] 不用 libretro 作为最终 App 壳。`src/platform/libretro` 可参考一帧运行、音频缓冲和传感器桥接方式，但 Android 版要直接调用 mGBA core API。

### 1.3 初版非目标

- [ ] 第一阶段不做 Google Play 发布签名自动化，只做到可安装 APK/AAB。
- [ ] 第一阶段不做云同步。
- [ ] 第一阶段不内置任何商业 ROM、BIOS 或 copyrighted 测试资源。
- [ ] 第一阶段不强制实现桌面调试 UI 的完整复刻；GDB stub 和日志先放开发者模式。
- [ ] 第一阶段不强制实现视频/GIF/WebP/APNG 录制；截图必须做，录制后续用 MediaCodec 或 FFmpeg 再补。

## 2. 推荐目录结构

```text
src/platform/android/
  README.md
  settings.gradle.kts
  build.gradle.kts
  gradle.properties
  app/
    build.gradle.kts
    proguard-rules.pro
    src/main/
      AndroidManifest.xml
      java/io/mgba/android/
        MainActivity.kt
        EmulatorActivity.kt
        MGBApplication.kt
        bridge/
          NativeBridge.kt
          NativeTypes.kt
          EmulatorHandle.kt
        emulator/
          EmulatorController.kt
          EmulatorState.kt
          EmulatorCommand.kt
          FrameStats.kt
        input/
          AndroidInputMapper.kt
          VirtualGamepadView.kt
          GamepadProfile.kt
          GamepadProfileStore.kt
          HapticsController.kt
        library/
          RomLibraryRepository.kt
          RomLibraryViewModel.kt
          RomScanner.kt
          RecentGameStore.kt
        storage/
          DocumentPicker.kt
          DocumentTreeStore.kt
          AndroidPaths.kt
          ImportExportManager.kt
        settings/
          SettingsStore.kt
          CoreOptionsMapper.kt
          PerGameOverrideStore.kt
        ui/
          LibraryScreen.kt
          EmulatorScreen.kt
          SettingsScreen.kt
          InputMappingScreen.kt
          SaveStateScreen.kt
          CheatScreen.kt
          BiosScreen.kt
          AboutScreen.kt
        util/
          FileName.kt
          Result.kt
          Logger.kt
      res/
        drawable/
        mipmap-*/
        values/
        xml/
      cpp/
        CMakeLists.txt
        mgba_android_jni.cpp
        AndroidCoreRunner.cpp
        AndroidCoreRunner.h
        AndroidRendererGLES2.cpp
        AndroidRendererGLES2.h
        AndroidAudioEngine.cpp
        AndroidAudioEngine.h
        AndroidInputBridge.cpp
        AndroidInputBridge.h
        AndroidSensorBridge.cpp
        AndroidSensorBridge.h
        AndroidVfs.cpp
        AndroidVfs.h
        AndroidLogger.cpp
        AndroidLogger.h
        JniUtils.cpp
        JniUtils.h
  test-roms/
    README.md
```

## 3. 构建系统计划

### 3.1 Gradle 工程

- [x] 新建 `src/platform/android/settings.gradle.kts`。
- [x] 新建 `src/platform/android/build.gradle.kts`。
- [x] 新建 `src/platform/android/gradle.properties`。
- [x] 新建 `src/platform/android/app/build.gradle.kts`。
- [x] 包名暂定 `io.mgba.android`。
- [x] App 名称暂定 `mGBA` 或 `mGBA Android`。
- [x] `minSdk` 建议先定 23；如果 AAudio/现代存储策略压力过大，再评估提升到 26。
- [ ] `targetSdk` / Android Gradle Plugin / Kotlin 版本在真正实现前用官方文档确认一次，避免 2026 年工具链细节过期。
- [x] Debug ABI：`arm64-v8a`、`armeabi-v7a`、`x86_64`。
- [ ] Release ABI：至少 `arm64-v8a`，建议保留 `armeabi-v7a` 和 `x86_64` 用于老设备/模拟器。
- [x] 开启 `externalNativeBuild.cmake`，CMake 入口先放在 `app/src/main/cpp/CMakeLists.txt`。

### 3.2 NDK / CMake

- [ ] Android CMake 入口通过 `add_subdirectory(${MGBA_ROOT} ${CMAKE_BINARY_DIR}/mgba-core)` 复用顶层 mGBA 构建。
- [ ] 给 mGBA core 传入 Android 端配置：
  - [ ] `-DBUILD_QT=OFF`
  - [ ] `-DBUILD_SDL=OFF`
  - [ ] `-DBUILD_LIBRETRO=OFF`
  - [ ] `-DBUILD_PERF=OFF`
  - [ ] `-DBUILD_TEST=OFF`
  - [ ] `-DBUILD_SUITE=OFF`
  - [ ] `-DBUILD_HEADLESS=OFF`
  - [ ] `-DBUILD_SHARED=OFF`
  - [ ] `-DBUILD_STATIC=ON`
  - [ ] `-DBUILD_GLES2=ON`
  - [ ] `-DBUILD_GLES3=OFF` 初期关闭，后续再评估。
  - [ ] `-DBUILD_GL=OFF`
  - [ ] `-DUSE_FFMPEG=OFF` 初期关闭，录制功能阶段再开启或换 Android MediaCodec。
  - [ ] `-DUSE_DISCORD_RPC=OFF`
  - [ ] `-DUSE_EDITLINE=OFF`
  - [ ] `-DUSE_LUA=OFF` 初期关闭，脚本功能阶段再评估。
  - [ ] `-DUSE_SQLITE3=ON` 若 ROM 库或核心功能需要；否则先由 Android Room 管理库。
  - [ ] `-DUSE_ZLIB=ON`
  - [ ] `-DUSE_PNG=ON`
  - [ ] `-DUSE_LZMA=ON`
  - [ ] `-DUSE_LIBZIP=OFF` 或 `-DUSE_MINIZIP=ON`，以仓库第三方 zlib/minizip 能否稳定编译为准。
- [ ] 新增 JNI 共享库 target：`mgba-android`。
- [ ] `mgba-android` 链接：
  - [ ] `mgba` 静态库。
  - [ ] Android 系统库：`log`、`android`、`EGL`、`GLESv2`。
  - [ ] 音频库：优先 `aaudio`；如支持低版本则补 `OpenSLES` 或引入 Oboe。
  - [ ] 需要时补 `atomic`。
- [ ] 避免污染桌面构建：所有 Android 特有逻辑尽量在 `src/platform/android/app/src/main/cpp/CMakeLists.txt` 内完成。
- [ ] 如果顶层 CMake 因 Android 交叉编译找包失败，再以最小补丁在顶层 `CMakeLists.txt` 中加 `if(ANDROID)` 默认关闭桌面前端和不可用依赖。

### 3.3 构建命令

- [ ] Debug 构建命令：

```bash
cd src/platform/android
./gradlew :app:assembleDebug
```

- [ ] 安装到设备：

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] 启动：

```bash
adb shell am start -n io.mgba.android/.MainActivity
```

- [ ] Release 验证：

```bash
cd src/platform/android
./gradlew :app:assembleRelease
```

## 4. JNI / Native API 设计

### 4.1 Kotlin 侧 NativeBridge

- [ ] `NativeBridge.kt` 负责加载 `System.loadLibrary("mgba-android")`。
- [ ] JNI handle 用 `Long` 保存 native `AndroidCoreRunner*`。
- [ ] Kotlin 对外 API 先设计成稳定小接口：

```kotlin
object NativeBridge {
    external fun nativeCreate(basePath: String, cachePath: String): Long
    external fun nativeDestroy(handle: Long)
    external fun nativeSetSurface(handle: Long, surface: Surface?)
    external fun nativeLoadRomFd(handle: Long, fd: Int, displayName: String): NativeLoadResult
    external fun nativeStart(handle: Long)
    external fun nativePause(handle: Long)
    external fun nativeResume(handle: Long)
    external fun nativeStop(handle: Long)
    external fun nativeReset(handle: Long)
    external fun nativeSetKeys(handle: Long, keys: Int)
    external fun nativeAddKeys(handle: Long, keys: Int)
    external fun nativeClearKeys(handle: Long, keys: Int)
    external fun nativeSetFastForward(handle: Long, enabled: Boolean)
    external fun nativeSetRewind(handle: Long, enabled: Boolean)
    external fun nativeSaveState(handle: Long, slot: Int): Boolean
    external fun nativeLoadState(handle: Long, slot: Int): Boolean
    external fun nativeDeleteState(handle: Long, slot: Int): Boolean
    external fun nativeTakeScreenshot(handle: Long, outputFd: Int): Boolean
    external fun nativeApplyConfig(handle: Long, config: NativeCoreConfig)
    external fun nativeGetGameInfo(handle: Long): NativeGameInfo
    external fun nativeGetFrameStats(handle: Long): NativeFrameStats
}
```

- [ ] 初期如果 `NativeLoadResult` / `NativeCoreConfig` 用 JNI object 复杂度太高，可以先用 primitive + JSON 字符串过桥，等 API 稳定后再优化。
- [ ] 所有 native 方法必须：
  - [ ] 校验 handle 是否为 0。
  - [ ] 捕获 native 异常/错误并返回可展示错误码。
  - [ ] 不在 UI 线程执行长时间 I/O。
  - [ ] 不把 Java 层传入的 fd 直接长期占用，必须 `dup(fd)` 后交给 `VFileFromFD`。

### 4.2 Native CoreRunner

- [ ] 新建 `AndroidCoreRunner`，封装一个正在运行的 mGBA 实例。
- [ ] 成员建议：
  - [ ] `struct mCore* core`
  - [ ] `struct mCoreThread thread`
  - [ ] `struct mCoreOptions options`
  - [ ] `struct mStandardLogger logger`
  - [ ] `AndroidRendererGLES2 renderer`
  - [ ] `AndroidAudioEngine audio`
  - [ ] `AndroidInputBridge input`
  - [ ] `AndroidSensorBridge sensors`
  - [ ] `std::atomic<bool> running`
  - [ ] `std::atomic<bool> surfaceReady`
  - [ ] `std::mutex lifecycleMutex`
  - [ ] `std::string basePath/cachePath/savePath/statePath/screenshotPath/cheatPath/biosPath`
- [ ] `create` 只初始化路径、日志、默认配置，不加载 ROM。
- [ ] `loadRomFd` 流程：
  - [ ] Kotlin 通过 SAF `ContentResolver.openFileDescriptor(uri, "r")` 获取 fd。
  - [ ] JNI 侧 `dup(fd)`。
  - [ ] `VFileFromFD(dupFd)`。
  - [ ] `mCoreFindVF(vf)` 判断 GBA / GB / GBC。
  - [ ] `core->init(core)`。
  - [ ] `mCoreInitConfig(core, "android")`。
  - [ ] 配置默认 `mCoreOptions`。
  - [ ] `mCoreConfigLoadDefaults`。
  - [ ] 将 Android app 私有目录映射到 `savegamePath`、`savestatePath`、`screenshotPath`、`patchPath`、`cheatsPath`。
  - [ ] `core->baseVideoSize`、`core->currentVideoSize`。
  - [ ] 分配 RGB565 视频 buffer，GBA/GBC 最大按 256x224 初始，实际大小由 `currentVideoSize` 更新。
  - [ ] `core->setVideoBuffer(core, outputBuffer, stride)`。
  - [ ] `core->setAudioBufferSize`。
  - [ ] `core->setAVStream` 接入音频/视频回调。
  - [ ] `core->setPeripheral` 接入 rumble / rotation / camera / luminance。
  - [ ] `core->loadROM(core, vf)`。
  - [ ] `mCoreAutoloadSave(core)`、`mCoreAutoloadPatch(core)`、`mCoreAutoloadCheats(core)`。
- [ ] `start` 使用 `mCoreThreadStart`，不要自己裸循环 `runFrame`，除非后续证明 Android 生命周期下需要自定义 loop。
- [ ] `pause/resume/stop` 必须统一走 `mCoreThreadPause`、`mCoreThreadUnpause`、`mCoreThreadEnd`、`mCoreThreadJoin`。
- [ ] 所有跨线程核心操作，比如读档/存档/重置/改配置，统一用 `mCoreThreadRunFunction` 在 core thread 上执行。

### 4.3 日志桥

- [ ] 新建 `AndroidLogger`，把 mGBA log 转到 Android `__android_log_vprint`。
- [ ] mGBA category 和 level 映射：
  - [ ] `mLOG_FATAL` -> `ANDROID_LOG_FATAL`
  - [ ] `mLOG_ERROR` -> `ANDROID_LOG_ERROR`
  - [ ] `mLOG_WARN` -> `ANDROID_LOG_WARN`
  - [ ] `mLOG_INFO` -> `ANDROID_LOG_INFO`
  - [ ] `mLOG_DEBUG` -> `ANDROID_LOG_DEBUG`
- [ ] Kotlin 层保留运行日志 ring buffer，用于“导出日志”。

## 5. 视频渲染计划

### 5.1 初版渲染路径

- [ ] Activity 内使用 `SurfaceView` 或 `GLSurfaceView` 承载模拟画面。
- [ ] 初版推荐 `SurfaceView + native EGL`：
  - [ ] Kotlin `SurfaceHolder.Callback` 把 `Surface` 传给 native。
  - [ ] Native 创建 EGLDisplay / EGLContext / EGLSurface。
  - [ ] Native GL 线程负责 `eglMakeCurrent`、上传纹理、绘制、`eglSwapBuffers`。
- [ ] 画面 buffer 使用 RGB565，匹配 mGBA 默认 `mColor`。
- [ ] GBA/GBC 视频尺寸变化时：
  - [ ] 调用 `core->currentVideoSize`。
  - [ ] 更新 texture 尺寸和 viewport。
  - [ ] 通知 Kotlin 更新 aspect ratio。
- [ ] 初版可先实现最小渲染：
  - [ ] 每帧 `glTexSubImage2D` 上传 core outputBuffer。
  - [ ] 一个全屏 quad。
  - [x] nearest / linear filtering 可切换。
  - [ ] letterbox/pillarbox 维持比例。
- [ ] 第二阶段接入现有 `src/platform/opengl/gles2.c` / `mGLES2Context`：
  - [ ] 复用 shader pipeline。
  - [ ] 复用 integer scaling、filter、interframe blending。
  - [ ] 适配 Android 的 `swap` callback。
  - [ ] 适配 Android 的 shader 文件加载路径。

### 5.2 Surface 生命周期

- [ ] `surfaceCreated`：
  - [x] native 保存 ANativeWindow。
  - [x] 初始化 EGL。
  - [x] 如果 ROM 已加载，恢复渲染。
- [x] `surfaceChanged`：
  - [x] 更新 viewport 宽高。
  - [x] 更新缩放策略。
- [x] `surfaceDestroyed`：
  - [x] 停止 GL 绘制。
  - [x] 释放 EGLSurface。
  - [x] 不销毁 core，不丢游戏状态。
- [x] Activity `onPause`：
  - [x] pause core thread。
  - [x] pause audio。
  - [x] 可选自动保存 SRAM。
- [x] Activity `onResume`：
  - [x] 重新绑定 Surface 后再恢复渲染。
  - [x] 恢复 audio。

### 5.3 画面设置

- [x] 比例模式：
  - [x] Original。
  - [x] Fit。
  - [x] Fill。
  - [x] Integer scale。
  - [x] Stretch。
- [ ] 滤镜：
  - [x] Nearest。
  - [x] Linear。
  - [ ] mGBA shader preset，后续阶段。
- [x] 旋转：
  - [x] 跟随系统。
  - [x] 锁定横屏。
  - [x] 锁定竖屏。
- [ ] 遮挡处理：
  - [ ] 虚拟手柄覆盖层不改变渲染比例。
  - [ ] 竖屏下画面在上，手柄在下。
  - [ ] 横屏下画面居中，手柄左右分区。

### 5.4 视频验收标准

- [ ] 能连续运行 30 分钟不黑屏、不闪退。
- [ ] 锁屏/解锁、切后台/切前台后画面恢复。
- [ ] 旋转屏幕后画面比例正确。
- [ ] GBA 240x160、GB 160x144 均显示正确。
- [ ] 帧率接近目标帧率，正常设备无明显 frame pacing 抖动。
- [ ] 画面无错色、无上下颠倒、无 stride 错位。

## 6. 音频计划

### 6.1 初版音频路径

- [ ] 优先 native 音频线程，避免每个 callback 穿越 JNI。
- [ ] Android API >= 26 使用 AAudio。
- [ ] 如果保留 `minSdk < 26`，补 OpenSL ES fallback 或引入 Oboe 包装 AAudio/OpenSL。
- [ ] 音频格式：PCM 16-bit stereo。
- [ ] 输出采样率：优先设备 native sample rate，通常 48000；使用 `mAudioResampler` 从 core sample rate 转换。
- [ ] buffer 策略参考 `src/platform/sdl/sdl-audio.c`：
  - [ ] `core->getAudioBuffer(core)` 作为 source。
  - [ ] `mAudioResamplerSetSource`。
  - [ ] `mAudioResamplerProcess`。
  - [ ] `mAudioBufferRead` 填充 Android 输出 buffer。
  - [x] underrun 时补零并计数。
- [ ] 暂停时停止音频 callback 并清空短 buffer，恢复时重新同步。

### 6.2 音频同步

- [ ] 使用 `mCoreSyncLockAudio` / `mCoreSyncConsumeAudio`，让音频 high water mark 控制核心速度。
- [ ] 快进时：
  - [ ] 可选择静音或降低音量。
  - [ ] 调整 resampler source rate。
  - [ ] 禁用过度等待，避免快进被音频拖住。
- [ ] 倒带时：
  - [ ] 清空音频输出 buffer。
  - [ ] 避免倒带后的旧样本继续播放。

### 6.3 音频设置

- [x] 音量。
- [x] 静音。
- [x] 低通滤波，参考 libretro 的 `audioLowPass` 实现。
- [x] 音频 buffer 大小：低延迟 / 平衡 / 稳定。
- [ ] 后台音频策略：默认切后台暂停。

### 6.4 音频验收标准

- [ ] 正常设备 30 分钟无持续爆音。
- [ ] 快进/暂停/恢复后音频不永久静音。
- [ ] 蓝牙耳机切换后可恢复音频。
- [ ] 横竖屏旋转不重启音频核心。
- [x] underrun 计数可在 debug overlay 查看。

## 7. 输入计划

### 7.1 GBA 键位映射

- [x] 统一使用 `include/mgba/internal/gba/input.h` 的枚举：
  - [x] `GBA_KEY_A`
  - [x] `GBA_KEY_B`
  - [x] `GBA_KEY_SELECT`
  - [x] `GBA_KEY_START`
  - [x] `GBA_KEY_RIGHT`
  - [x] `GBA_KEY_LEFT`
  - [x] `GBA_KEY_UP`
  - [x] `GBA_KEY_DOWN`
  - [x] `GBA_KEY_R`
  - [x] `GBA_KEY_L`
- [x] Kotlin 层维护一个 `Int keys` bitmask。
- [x] 每次触摸/手柄事件变化时调用 `nativeSetKeys(handle, keys)`。
- [x] Native 侧最终调用 `core->setKeys(core, keys)`。
- [x] 防止 Android 多点触控事件丢点导致按键卡住：
  - [x] `ACTION_CANCEL` 清空所有触摸按键。
  - [x] Activity pause 清空所有按键。
  - [x] Surface destroyed 清空所有按键。

### 7.2 虚拟手柄

- [x] 新建 `VirtualGamepadView`。
- [x] 支持多点触控。
- [x] 默认布局：
  - [x] 左侧 D-pad。
  - [x] 右侧 A/B。
  - [x] 上方或肩部 L/R。
  - [x] 中部 Start/Select/Menu。
- [x] 设置项：
  - [x] 透明度。
  - [x] 大小。
  - [x] 间距。
  - [x] 左右手模式。
  - [x] 横屏布局。
  - [x] 竖屏布局。
  - [x] 震动反馈开关。
- [ ] 支持布局编辑模式：
  - [ ] 拖动按钮。
  - [ ] 双指缩放按钮。
  - [x] 重置布局。
  - [ ] 保存为 profile。

### 7.3 实体手柄/键盘

- [x] 监听 `KeyEvent`：
  - [x] DPAD -> 方向。
  - [x] BUTTON_A / BUTTON_B / BUTTON_X / BUTTON_Y -> A/B 可配置。
  - [x] BUTTON_L1 / BUTTON_R1 -> L/R。
  - [x] BUTTON_START / BUTTON_SELECT。
- [x] 监听 `MotionEvent`：
  - [x] `AXIS_X` / `AXIS_Y` 左摇杆。
  - [x] `AXIS_HAT_X` / `AXIS_HAT_Y` 十字键。
  - [x] `AXIS_LTRIGGER` / `AXIS_RTRIGGER` 可映射。
- [x] 支持 deadzone。
- [ ] 支持每个设备独立 profile：
  - [x] 使用 `InputDevice.descriptor` 作为稳定 key。
  - [x] 允许手动绑定。
  - [x] 支持导入/导出 profile。
- [x] Debug 页面显示当前输入设备、轴值、按键码。

### 7.4 特殊输入/外设

- [ ] Rumble：
  - [x] Native 接 mGBA `mPERIPH_RUMBLE`。
  - [x] Kotlin 使用 `Vibrator` / `VibrationEffect` / controller haptics。
- [ ] Rotation / tilt：
  - [x] Native 接 `mPERIPH_ROTATION`。
  - [x] Kotlin 监听 accelerometer / gyroscope。
  - [x] 提供校准按钮。
- [ ] Solar sensor：
  - [x] 默认提供手动亮度滑条。
  - [x] 可选使用 Android light sensor。
- [ ] Game Boy Camera：
  - [ ] 后期接 CameraX。
  - [ ] 先提供静态图片导入作为 image source。

### 7.5 输入验收标准

- [ ] 虚拟按键无明显延迟。
- [ ] 多点触控可同时按方向+A/B。
- [ ] 切后台后不会出现按键卡住。
- [ ] Xbox / DualShock / Switch Pro / 常见蓝牙手柄至少验证两类。
- [x] 键位重映射保存后重启仍生效。

## 8. 存储 / SAF / VFS 计划

### 8.1 Android 存储策略

- [ ] 不申请 broad storage 权限作为默认路径。
- [ ] ROM 选择使用 `ACTION_OPEN_DOCUMENT`。
- [ ] ROM 文件夹扫描使用 `ACTION_OPEN_DOCUMENT_TREE`。
- [ ] 对选中的 ROM / 文件夹调用 `takePersistableUriPermission`。
- [ ] App 内生成文件使用私有目录：
  - [ ] `files/saves`
  - [ ] `files/states`
  - [ ] `files/screenshots`
  - [ ] `files/cheats`
  - [ ] `files/bios`
  - [ ] `files/patches`
  - [ ] `files/config`
  - [ ] `cache/imports`
- [ ] 导出时使用 `ACTION_CREATE_DOCUMENT` 或分享 sheet。

### 8.2 ROM 加载策略

- [ ] 优先从 SAF fd 加载：
  - [ ] Kotlin `ParcelFileDescriptor.detachFd()` 或 `dup` 后传 native。
  - [ ] Native `dup(fd)`。
  - [ ] Native `VFileFromFD(dupFd)`。
  - [ ] Native `mCoreFindVF(vf)`。
- [ ] 对不支持 seek/mmap 的 provider：
  - [ ] 探测 `vf->seek` 和 `vf->size` 是否可用。
  - [ ] 失败时复制到 `cache/imports/<hash>.<ext>` 再 `VFileOpen`。
- [ ] 对 archive：
  - [ ] 如果 `VDirOpenArchive(path)` 只能走路径，先复制 archive 到 cache，再用现有 archive VFS。
  - [ ] 后续可实现 `VDirOpenArchiveVF`，减少大文件复制。
- [ ] ROM hash：
  - [ ] 加载后计算 CRC32/SHA1。
  - [ ] 用 hash 作为保存/状态/封面/配置的稳定 key。

### 8.3 Save / State 命名

- [ ] 保存文件命名建议：
  - [ ] `files/saves/<romHash>/<displayName>.sav`
  - [ ] `files/states/<romHash>/slot-1.ss`
  - [ ] `files/states/<romHash>/slot-1.png` 或状态 extdata 缩略图 cache。
  - [ ] `files/screenshots/<displayName>-yyyyMMdd-HHmmss.png`
  - [ ] `files/cheats/<romHash>.cheats`
  - [ ] `files/patches/<romHash>.<ips|ups|bps>`
- [ ] 设置 `mCoreOptions`：
  - [ ] `savegamePath`
  - [ ] `savestatePath`
  - [ ] `screenshotPath`
  - [ ] `patchPath`
  - [ ] `cheatsPath`
- [ ] 如果 mGBA directory set 依赖 `VDirOpen(path)`，Android 私有目录可直接提供真实 filesystem path。

### 8.4 导入导出

- [ ] 从用户选择的 `.sav` 导入到当前 ROM。
- [ ] 导出当前 ROM `.sav`。
- [ ] 导出/导入单个 savestate。
- [ ] 导出截图。
- [ ] 导出完整游戏数据包：
  - [ ] save。
  - [ ] states。
  - [ ] cheats。
  - [ ] per-game settings。
- [ ] 导入时校验 ROM hash，不匹配则提示但允许用户继续。

## 9. ROM 库计划

### 9.1 数据模型

- [ ] `RomEntry`
  - [ ] id。
  - [ ] uri。
  - [ ] displayName。
  - [x] platform: GBA / GB（GBC 细分待 native metadata 扩展）。
  - [x] title from `mGameInfo`。
  - [x] game code。
  - [x] maker。
  - [x] version。
  - [x] crc32。
  - [x] sha1。
  - [x] file size。
  - [x] lastPlayedAt。
  - [x] playTimeSeconds。
  - [x] favorite。
  - [x] coverPath 或 screenshot thumbnail。
- [ ] 数据存储：
  - [ ] 首选 Room。
  - [ ] 若不想引 AndroidX Room，初期用 DataStore/JSON，后期迁移。

### 9.2 扫描流程

- [ ] 用户选择文件夹。
- [ ] 保存 tree URI 权限。
- [x] 后台遍历文档树。
- [x] 过滤扩展名：`.gba`、`.agb`、`.gb`、`.gbc`、`.sgb`、`.zip`、`.7z`。
- [ ] 对每个候选文件：
  - [x] 打开 fd。
  - [x] native 探测 `mCoreFindVF`。
  - [x] 读取 `mGameInfo`。
  - [x] 计算 CRC32。
  - [x] 计算 SHA1。
  - [x] 入库。
- [x] 扫描进度可取消。
- [x] 扫描不应阻塞 UI。

### 9.3 Library UI

- [x] 首页不是营销页，直接显示 ROM 库。
- [ ] 空状态提供“打开 ROM”和“添加文件夹”两个动作。
- [ ] ROM 列表支持：
  - [x] 最近。
  - [x] 收藏。
  - [x] GBA / GB 筛选。
  - [ ] GBC 细分筛选。
  - [x] 搜索。
  - [ ] 网格/列表切换。
- [ ] 每个 ROM item 显示：
  - [x] 标题。
  - [x] 平台。
  - [x] 最近游玩。
  - [x] 缩略图。
  - [ ] 菜单：
    - [x] 启动。
    - [ ] 设置。
    - [ ] 存档。
    - [ ] 作弊。
    - [x] 导出。
    - [x] 删除记录。

## 10. 设置计划

### 10.1 全局设置

- [ ] Video：
  - [x] 缩放模式。
  - [x] 整数缩放。
  - [x] 滤镜。
  - [x] 帧跳过。
  - [ ] interframe blending。
  - [ ] 着色器，后续。
- [ ] Audio：
  - [x] 音量。
  - [x] 静音。
  - [x] buffer 模式。
  - [x] 低通滤波。
- [ ] Emulation：
  - [ ] 使用 BIOS。
  - [ ] BIOS 文件路径。
  - [ ] 跳过 BIOS。
  - [x] 快进倍率。
  - [x] 倒带开关。
  - [x] 倒带 buffer。
  - [ ] RTC 策略。
- [ ] Input：
  - [x] 触屏显示。
  - [ ] 触屏布局编辑。
  - [ ] 手柄映射。
  - [ ] 震动。
  - [x] 允许相反方向。
- [ ] Storage：
  - [ ] ROM 文件夹。
  - [ ] 导入/导出。
  - [ ] 清理 cache。
- [ ] Advanced：
  - [ ] 日志级别。
  - [x] Debug overlay。
  - [ ] GDB stub。
  - [x] 崩溃日志导出。

### 10.2 Per-game override

- [ ] 每个游戏可以覆盖：
  - [ ] BIOS。
  - [ ] 补丁。
  - [ ] 作弊自动启用。
  - [x] 视频缩放。
  - [x] 视频滤镜。
  - [x] 虚拟手柄显示。
  - [ ] 输入布局编辑。
  - [x] 传感器校准。
  - [x] 快进/倒带。
- [x] 覆盖层写入 `PerGameOverrideStore`。
- [ ] 加载 ROM 后把 override 映射到 `mCoreConfigSetOverride*` 或 `mCoreOptions`。

## 11. 即时存档 / 截图 / 作弊

### 11.1 即时存档

- [ ] UI 做 9 个槽位，和桌面功能对应。
- [ ] `SaveStateScreen` 展示：
  - [ ] 槽位号。
  - [ ] 缩略图。
  - [ ] 保存时间。
  - [ ] 操作：
    - [x] 保存。
    - [x] 读取。
    - [x] 删除。
    - [ ] 导出。
- [ ] Native 操作必须在 core thread 执行。
- [x] 保存 flags 使用 `SAVESTATE_ALL`。
- [ ] 缩略图：
  - [ ] 优先读取 state extdata 中的 screenshot。
  - [x] 若没有，保存时额外生成 PNG cache。
- [ ] 自动保存策略：
  - [ ] Activity pause 可触发 SRAM flush。
  - [ ] 可选 auto-state on exit。

### 11.2 截图

- [x] 使用 `mCoreTakeScreenshotVF(core, vf)`。
- [x] Kotlin 创建目标 document 或 app 私有文件。
- [x] Native 用 `VFileFromFD` 写入。
- [x] 截图完成后发送 MediaStore scan 或分享入口。

### 11.3 作弊

- [x] 使用 `core->cheatDevice(core)`。
- [x] 导入 `.cheats` / GameShark / Action Replay 文本。
- [ ] 列表展示：
  - [ ] 名称。
  - [ ] 代码。
  - [ ] 启用开关。
  - [ ] 错误信息。
- [x] 保存到 `files/cheats/<romHash>.cheats`。
- [x] ROM 加载后自动应用已保存 cheats。

## 12. BIOS / Patch / Archive

### 12.1 BIOS

- [ ] BIOS 管理页支持：
  - [ ] 选择 GBA BIOS。
  - [ ] 选择 GB/GBC BIOS，如核心支持。
  - [x] 显示文件 hash 和大小。
  - [x] 删除 BIOS。
- [x] 导入 BIOS 到 app 私有目录，不长期依赖外部 URI。
- [x] 设置 `mCoreOptions.bios`。
- [x] 设置 `mCoreOptions.useBios` / `skipBios`。
- [ ] 加载失败给明确错误提示。

### 12.2 Patch

- [x] 支持手动给当前 ROM 选择 `.ips` / `.ups` / `.bps`。
- [x] 自动 patch 搜索：
  - [x] `<romName>.ips`
  - [x] `<romName>.ups`
  - [x] `<romName>.bps`
  - [x] `<romHash>.<ext>`
- [x] 加载 ROM 后调用 `mCoreAutoloadPatch(core)`。
- [x] 手动 patch 则直接 `core->loadPatch(core, vf)`。

### 12.3 Archive

- [ ] `.zip` / `.7z` 初期通过复制到 cache 后使用现有 path-based archive VFS。
- [x] `.zip` 单 ROM archive 自动启动。
- [x] `.zip` 多 ROM archive 弹出选择列表。
- [ ] `.7z` archive 启动。
- [ ] cache 清理策略：
  - [ ] 最近使用保留。
  - [x] 可手动清空。
  - [x] 超过大小上限自动清理旧文件。

## 13. 高级功能复刻

### 13.1 快进

- [x] UI 提供按住快进和切换快进两种模式。
- [x] 设置快进倍率：2x / 3x / 4x / unlimited。
- [x] Native 调整 sync/audio wait 策略。
- [x] 快进时 overlay 显示倍率。

### 13.2 倒带

- [x] 启用 `rewindEnable`。
- [x] 设置：
  - [x] rewindBufferCapacity。
  - [x] rewindBufferInterval。
- [x] UI 按住倒带。
- [x] 倒带时处理音频清空和画面立即刷新。

### 13.3 录制

- [ ] 第一阶段只做截图。
- [ ] 视频录制后续两条路线评估：
  - [ ] Android MediaCodec：更原生，适合 MP4。
  - [ ] FFmpeg：复刻桌面 GIF/WebP/APNG，但 Android 构建成本高。
- [ ] 若选择 FFmpeg：
  - [ ] 单独做 `USE_FFMPEG=ON` 的 Android 交叉编译方案。
  - [ ] 控制 APK 体积。
  - [ ] 处理 LGPL/GPL 许可边界。

### 13.4 Debug / GDB

- [ ] 开发者模式里开启 GDB stub。
- [ ] 显示监听端口。
- [ ] 仅 debug build 默认允许。
- [ ] release build 需要明确用户确认。
- [ ] 桌面完整调试 UI 不作为首版目标。

### 13.5 Link cable

- [ ] 调研当前 `src/gba/sio` 和 Qt MultiplayerController 的耦合点。
- [ ] 第一阶段不做。
- [ ] 第二阶段实现同设备多实例本地联机：
  - [ ] 同一 Activity 管理 2-4 个 `AndroidCoreRunner`。
  - [ ] 分屏渲染。
  - [ ] 输入按玩家映射。
- [ ] 第三阶段实现局域网联机：
  - [ ] Wi-Fi Direct 或 TCP。
  - [ ] 帧同步和延迟补偿。
  - [ ] 断线恢复。

## 14. Android 生命周期与稳定性

- [ ] Activity lifecycle：
  - [ ] `onCreate` 创建 controller。
  - [ ] `onStart` 绑定 UI。
  - [ ] `onResume` 恢复输入/传感器/音频。
  - [ ] `onPause` 暂停 core/audio/sensors，保存 SRAM。
  - [ ] `onStop` 保持状态但释放高耗资源。
  - [ ] `onDestroy` 如果 finishing，停止 core 并释放 native handle。
- [ ] Surface lifecycle 与 core lifecycle 分离。
- [ ] 设备旋转不重启游戏：
  - [ ] 使用 ViewModel 保存 `EmulatorController`。
  - [ ] 或在 Manifest 处理 configChanges，但要慎用。
- [ ] 内存压力：
  - [x] `onTrimMemory` 清理 ROM archive cache。
  - [ ] `onTrimMemory` 清理封面 cache。
  - [ ] 不在内存里长期保留大型 ROM，除非 provider 不支持 seek。
- [ ] 崩溃恢复：
  - [ ] native crash 无法完全恢复，但下次启动提示导出日志。
  - [ ] Java exception 写入 app log。

## 15. UI 页面清单

### 15.1 MainActivity / Library

- [x] ROM 库首页。
- [ ] 顶部操作：搜索、添加、设置。
- [ ] 最近游戏横向区或列表排序。
- [ ] 空状态。
- [ ] 扫描进度。

### 15.2 EmulatorActivity / EmulatorScreen

- [ ] 全屏沉浸模式。
- [ ] Surface 渲染区域。
- [ ] VirtualGamepad overlay。
- [ ] 顶部/底部临时菜单：
  - [ ] 暂停/继续。
  - [ ] 重置。
  - [x] 快进。
  - [x] 倒带。
  - [ ] 存档。
  - [ ] 读档。
  - [ ] 截图。
  - [ ] 设置。
  - [x] 退出。
- [ ] Debug overlay：
  - [x] FPS。
  - [x] frame time。
  - [x] audio underrun。
  - [x] ROM platform。
  - [x] core frame counter。

### 15.3 Settings

- [ ] 全局设置。
- [x] 当前游戏设置。
- [ ] 输入映射。
- [ ] BIOS。
- [ ] 存储管理。
- [ ] 关于/许可。

### 15.4 SaveState

- [ ] 9 槽列表。
- [ ] 快速保存/读取。
- [ ] 删除/导出。
- [ ] 覆盖确认。

### 15.5 Cheats

- [ ] 当前游戏 cheat 列表。
- [ ] 添加代码。
- [ ] 导入文件。
- [ ] 启用/禁用。
- [ ] 保存。

## 16. 测试计划

### 16.1 Native 单元测试

- [ ] 新增 Android 可跑的 native smoke test：
  - [ ] `mCoreFindVF` 可以识别测试 ROM。
  - [ ] `core->init` 成功。
  - [ ] `core->loadROM` 成功。
  - [ ] `core->runFrame` 连跑 300 帧不崩。
  - [ ] `mCoreSaveStateNamed` / `mCoreLoadStateNamed` 成功。
- [ ] 测试 ROM 必须是 homebrew/public-domain，不提交商业 ROM。
- [ ] 如果仓库已有测试 ROM 规则，遵守现有规则。

### 16.2 Android Instrumented Test

- [ ] 启动 MainActivity。
- [ ] 打开测试 ROM。
- [ ] 等待渲染非黑帧。
- [ ] 点击虚拟 A/B/方向，确认 native 收到 key bit。
- [ ] 保存 state。
- [ ] 读 state。
- [ ] 旋转屏幕。
- [ ] 切后台/恢复。

### 16.3 手工设备矩阵

- [ ] arm64 中端手机。
- [ ] arm64 低端手机。
- [ ] 平板。
- [ ] Android 模拟器 x86_64。
- [ ] 外接蓝牙手柄。
- [ ] 蓝牙耳机。

### 16.4 性能指标

- [ ] GBA 常规游戏 60 FPS 稳定。
- [ ] GB/GBC 常规游戏稳定。
- [ ] 音频 underrun 每 10 分钟少于 1 次，低端设备允许设置更大 buffer。
- [ ] ROM 加载到首帧小于 1 秒，archive/cache 情况另计。
- [ ] 正常游玩 30 分钟 native heap 无持续增长。
- [ ] App 切后台后 CPU 使用接近 0。

### 16.5 回归用例

- [ ] GBA ROM。
- [ ] GB ROM。
- [ ] GBC ROM。
- [ ] 需要 RTC 的游戏。
- [ ] 需要 rumble 的游戏。
- [ ] 需要 tilt 的游戏。
- [ ] 使用 patch 的 ROM。
- [ ] 使用 cheat 的 ROM。
- [ ] ZIP 单 ROM。
- [ ] ZIP 多 ROM。
- [ ] 7z 单 ROM。
- [ ] 大 ROM。
- [ ] 非法文件/损坏 ROM。

## 17. CI / 发布计划

### 17.1 CI

- [x] GitHub Actions 新增 Android workflow：
  - [x] checkout。
  - [x] setup JDK。
  - [x] setup Android SDK / NDK。
  - [x] Gradle cache。
  - [x] `./gradlew :app:assembleDebug`。
  - [x] `./gradlew :app:assembleRelease`。
  - [x] `./gradlew :app:bundleRelease`。
  - [x] `./gradlew :app:testDebugUnitTest`。
  - [x] 上传 debug APK、release APK、release AAB 和 native symbols artifacts。
- [ ] 可选 matrix：
  - [ ] arm64-v8a。
  - [ ] x86_64。
- [ ] Native warnings 作为 PR 检查。

### 17.2 Release

- [x] Debug APK。
- [x] Release APK 构建输出（当前为 unsigned）。
- [ ] Internal Release APK 签名配置。
- [x] AAB。
- [x] Proguard/R8 验证。
- [x] Native symbols 输出：
  - [x] `app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip`
  - [x] CI artifact：`mgba-android-release-native-symbols`
- [ ] Crash 符号化流程文档。
- [ ] MPL 2.0 和第三方 license 页面。

## 18. 许可与合规

- [x] 保留 MPL 2.0 license。
- [x] App 内 About 页面展示：
  - [x] mGBA copyright。
  - [x] MPL 2.0。
  - [ ] 第三方库 license：zlib、libpng、lzma、sqlite、inih 等实际启用项。
- [ ] 不分发 BIOS。
- [ ] 不分发商业 ROM。
- [ ] 如果启用 FFmpeg，明确 LGPL/GPL 组件选择和发布义务。
- [ ] 如果使用 AndroidX / Oboe / CameraX / Room，补充各自 license。

## 19. 分阶段 PR 计划

### PR 1：Android 工程骨架

- [ ] 创建 `src/platform/android` Gradle 工程。
- [ ] Manifest / MainActivity / 空 LibraryScreen。
- [ ] CI 可构建 debug APK。
- [ ] 不接 mGBA core。
- [ ] 验收：
  - [ ] `./gradlew :app:assembleDebug` 通过。
  - [ ] APK 可安装并打开。

### PR 2：NDK + mGBA core 静态链接

- [ ] 新增 CMake JNI target。
- [ ] 链接 mGBA static。
- [ ] `nativeGetVersion()` 返回 `projectVersion` / git info。
- [ ] Android log bridge 初版。
- [ ] 验收：
  - [ ] App About 页面显示 native mGBA version。
  - [ ] 所有 ABI debug 构建通过。

### PR 3：ROM fd 探测和加载

- [ ] SAF 打开 ROM。
- [ ] JNI `nativeLoadRomFd`。
- [ ] `mCoreFindVF` + `core->init` + `core->loadROM`。
- [ ] 返回 game info。
- [ ] 验收：
  - [ ] `.gba` / `.gb` / `.gbc` 测试 ROM 能被识别。
  - [ ] 非 ROM 文件返回友好错误。

### PR 4：视频首帧和持续渲染

- [ ] SurfaceView。
- [ ] Native EGL。
- [ ] RGB565 texture upload。
- [ ] core thread run。
- [ ] 验收：
  - [ ] 测试 ROM 可见画面。
  - [ ] 旋转/切后台恢复。

### PR 5：音频

- [ ] AAudio/OpenSL/Oboe 路线落地。
- [ ] `mAudioResampler` 接入。
- [ ] 暂停/恢复处理。
- [ ] 验收：
  - [ ] 有声音。
  - [ ] 10 分钟无明显爆音。

### PR 6：触屏和实体输入

- [ ] VirtualGamepadView。
- [ ] KeyEvent / MotionEvent 映射。
- [ ] `nativeSetKeys`。
- [ ] 基础 remap。
- [ ] 验收：
  - [ ] 虚拟手柄可完整游玩。
  - [ ] 至少一种实体手柄可完整游玩。

### PR 7：存档/读档/截图

- [ ] 私有目录映射。
- [ ] SRAM 自动保存/加载。
- [ ] 9 槽即时存档。
- [ ] 截图。
- [ ] 验收：
  - [ ] 退出重进后 save 存在。
  - [ ] state save/load 正常。
  - [ ] screenshot 可导出。

### PR 8：ROM 库和扫描

- [ ] Room/JSON 数据层。
- [ ] 文件夹授权。
- [ ] 后台扫描。
- [ ] 最近游戏。
- [ ] 验收：
  - [ ] 可添加文件夹。
  - [ ] 重启后库仍存在。

### PR 9：设置和 per-game override

- [ ] 全局设置。
- [ ] 当前游戏设置。
- [ ] 输入 profile。
- [ ] 视频/音频/核心配置映射。
- [ ] 验收：
  - [ ] 改设置后立即或下次启动生效。
  - [x] per-game 设置不污染全局。

### PR 10：BIOS / Patch / Cheats / Archive

- [ ] BIOS 管理。
- [ ] Patch 手动/自动加载。
- [ ] Cheat UI。
- [ ] ZIP/7z cache 加载。
- [ ] 验收：
  - [ ] BIOS 文件可导入并使用。
  - [ ] patch 生效。
  - [ ] cheat 可启停。
  - [ ] archive ROM 可启动。

### PR 11：高级输入和传感器

- [ ] Rumble。
- [ ] Tilt / gyro。
- [ ] Solar sensor。
- [ ] Game Boy Camera 静态图片源。
- [ ] 验收：
  - [ ] 支持对应硬件特性的测试 ROM 行为正确。

### PR 12：性能、稳定性和 release polish

- [x] Debug overlay。
- [x] Crash/log export。
- [ ] Proguard/R8。
- [ ] Native symbols。
- [ ] License 页面。
- [x] Release APK/AAB 构建输出。
- [ ] 验收：
  - [ ] 设备矩阵通过。
  - [ ] 30 分钟稳定性测试通过。

## 20. 风险清单与应对

- [ ] Android SAF fd 不支持 mmap/seek：
  - [ ] 应对：复制到 cache 后用真实 path/VFileOpen。
- [ ] EGL lifecycle 导致黑屏：
  - [ ] 应对：Surface 与 core 分离；surface destroyed 只释放 EGLSurface，不销毁 core。
- [ ] 音频延迟或爆音：
  - [ ] 应对：提供 buffer 档位；保留 underrun 计数；必要时引入 Oboe。
- [ ] mGBA 顶层 CMake 对 Android find_package 不友好：
  - [ ] 应对：Android CMake 显式关闭桌面依赖；必要时最小修改顶层 `if(ANDROID)`。
- [ ] APK 体积过大：
  - [ ] 应对：按 ABI split；延后 FFmpeg；只启用必要第三方库。
- [ ] 触屏手柄遮挡画面：
  - [ ] 应对：横竖屏独立布局、透明度、布局编辑。
- [ ] 存档路径与 ROM hash 迁移：
  - [ ] 应对：第一版就使用 hash 目录；displayName 只作展示。
- [ ] Native crash 难排查：
  - [ ] 应对：保留 symbols；debug overlay；导出 logcat/native tombstone 指引。
- [ ] 完全复刻桌面 Qt 功能工作量过大：
  - [ ] 应对：先做游玩闭环，再按功能矩阵补齐；每个 PR 都可独立验收。

## 21. 最终 Definition of Done

- [ ] Android App 可以从用户文件/文件夹启动 GBA、GB、GBC ROM。
- [ ] 视频、音频、输入完整可玩。
- [ ] 保存、读档、截图、BIOS、patch、cheat、archive 至少达到桌面常用功能。
- [x] ROM 库和最近游戏可用。
- [ ] 全局设置和 per-game override 可用。
- [ ] 横竖屏、切后台、锁屏、外设变化稳定。
- [x] Debug APK、Release APK/AAB 可构建。
- [x] CI 自动构建 Android。
- [ ] License / 第三方声明完整。
- [ ] 没有提交商业 ROM、BIOS 或不可分发资源。
- [ ] 有一套可重复执行的手工验收清单和至少基础自动化测试。
