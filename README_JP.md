mGBA
====

mGBAは、ゲームボーイアドバンスのゲームを実行するためのエミュレーターです。mGBAの目標は、既存の多くのゲームボーイアドバンスエミュレーターよりも高速かつ正確であり、他のエミュレーターにはない機能を追加することです。また、ゲームボーイおよびゲームボーイカラーのゲームもサポートしています。

最新のニュースとダウンロードは、[mgba.io](https://mgba.io/)で見つけることができます。

[![Build status](https://buildbot.mgba.io/badges/build-win32.svg)](https://buildbot.mgba.io)
[![Translation status](https://hosted.weblate.org/widgets/mgba/-/svg-badge.svg)](https://hosted.weblate.org/engage/mgba)

特徴
--------

- 高精度なゲームボーイアドバンスハードウェアのサポート[<sup>[1]</sup>](#missing)。
- ゲームボーイ/ゲームボーイカラーのハードウェアサポート。
- 高速なエミュレーション。ネットブックなどの低スペックハードウェアでもフルスピードで動作することが知られています。
- 重量級と軽量級のフロントエンドのためのQtおよびSDLポート。
- ローカル（同じコンピュータ）リンクケーブルのサポート。
- フラッシュメモリサイズを含む保存タイプの検出[<sup>[2]</sup>](#flashdetect)。
- モーションセンサーと振動機能を備えたカートリッジのサポート（ゲームコントローラーでのみ使用可能）。
- 設定なしでもリアルタイムクロックのサポート。
- ボクタイゲームのためのソーラーセンサーのサポート。
- ゲームボーイカメラとゲームボーイプリンターのサポート。
- 内蔵BIOS実装と外部BIOSファイルの読み込み機能。
- Luaを使用したスクリプトサポート。
- Tabキーを押し続けることでターボ/早送りサポート。
- バッククォートを押し続けることで巻き戻し。
- 最大10まで設定可能なフレームスキップ。
- スクリーンショットのサポート。
- チートコードのサポート。
- 9つのセーブステートスロット。セーブステートはスクリーンショットとしても表示可能。
- ビデオ、GIF、WebP、およびAPNGの録画。
- e-Readerのサポート。
- キーボードとゲームパッドのリマップ可能なコントロール。
- ZIPおよび7zファイルからの読み込み。
- IPS、UPS、およびBPSパッチのサポート。
- コマンドラインインターフェースとGDBリモートサポートを介したゲームデバッグ、GhidraおよびIDA Proと互換性あり。
- 設定可能なエミュレーションの巻き戻し。
- GameSharkおよびAction Replayスナップショットの読み込みおよびエクスポートのサポート。
- RetroArch/LibretroおよびOpenEmu用のコア。
- [Weblate](https://hosted.weblate.org/engage/mgba)を介した複数の言語のコミュニティ提供の翻訳。
- その他、多くの小さな機能。

#### ゲームボーイマッパー

以下のマッパーが完全にサポートされています：

- MBC1
- MBC1M
- MBC2
- MBC3
- MBC3+RTC
- MBC30
- MBC5
- MBC5+Rumble
- MBC7
- Wisdom Tree（非公式）
- NT "old type" 1 and 2（非公式マルチカート）
- NT "new type"（非公式MBC5類似）
- Pokémon Jade/Diamond（非公式）
- Sachen MMC1（非公式）

以下のマッパーが部分的にサポートされています：

- MBC6（フラッシュメモリ書き込みサポートなし）
- MMM01
- Pocket Cam
- TAMA5（RTCサポート不完全）
- HuC-1（IRサポートなし）
- HuC-3（IRサポートなし）
- Sachen MMC2（代替配線サポートなし）
- BBD（ロゴ切り替えなし）
- Hitek（ロゴ切り替えなし）
- GGB-81（ロゴ切り替えなし）
- Li Cheng（ロゴ切り替えなし）

### 計画されている機能

- ネットワーク対応のマルチプレイヤーリンクケーブルサポート。
- Dolphin/JOYバスリンクケーブルサポート。
- MP2kオーディオミキシング、ハードウェアより高品質のサウンド。
- ツールアシストランのための再録サポート。
- 包括的なデバッグスイート。
- ワイヤレスアダプターのサポート。

サポートされているプラットフォーム
-------------------

- Windows 7以降
- OS X 10.9（Mavericks）[<sup>[3]</sup>](#osxver)以降
- Linux
- FreeBSD
- Nintendo 3DS
- Nintendo Switch
- Wii
- PlayStation Vita

他のUnix系プラットフォーム（OpenBSDなど）も動作することが知られていますが、テストされておらず、完全にはサポートされていません。

### システム要件

要件は最小限です。Windows Vista以降を実行できるコンピュータであれば、エミュレーションを処理できるはずです。OpenGL 1.1以降のサポートも必要であり、シェーダーや高度な機能にはOpenGL 3.2以降が必要です。

ダウンロード
---------

ダウンロードは公式ウェブサイトの[ダウンロード][downloads]セクションで見つけることができます。ソースコードは[GitHub][source]で見つけることができます。

コントロール
--------

コントロールは設定メニューで設定可能です。多くのゲームコントローラーはデフォルトで自動的にマッピングされるはずです。デフォルトのキーボードコントロールは次のとおりです：

- **A**：X
- **B**：Z
- **L**：A
- **R**：S
- **Start**：Enter
- **Select**：Backspace

コンパイル
---------

コンパイルにはCMake 3.1以降の使用が必要です。GCC、Clang、およびVisual Studio 2019はmGBAのコンパイルに使用できることが知られています。

#### Dockerビルド

ほとんどのプラットフォームでのビルドにはDockerを使用することをお勧めします。いくつかのプラットフォームでmGBAをビルドするために必要なツールチェーンと依存関係を含むいくつかのDockerイメージが提供されています。

注意：Windows 10以前の古いWindowsシステムを使用している場合、DockerがVirtualBox共有フォルダーを使用して現在の`mgba`チェックアウトディレクトリをDockerイメージの作業ディレクトリに正しくマッピングするように構成する必要がある場合があります。（詳細については、issue [#1985](https://mgba.io/i/1985)を参照してください。）

Dockerイメージを使用してmGBAをビルドするには、mGBAのチェックアウトのルートで次のコマンドを実行します：

	docker run --rm -it -v ${PWD}:/home/mgba/src mgba/windows:w32

Dockerコンテナを起動した後、ビルド成果物を含む`build-win32`ディレクトリが生成されます。他のプラットフォーム用のDockerイメージに置き換えると、対応する他のディレクトリが生成されます。Docker Hubで利用可能なDockerイメージは次のとおりです：

- mgba/3ds
- mgba/switch
- mgba/ubuntu:xenial
- mgba/ubuntu:bionic
- mgba/ubuntu:focal
- mgba/ubuntu:groovy
- mgba/vita
- mgba/wii
- mgba/windows:w32
- mgba/windows:w64

ビルドプロセスを高速化したい場合は、`-e MAKEFLAGS=-jN`フラグを追加して、`N`個のCPUコアでmGBAの並列ビルドを行うことを検討してください。

#### *nixビルド

UnixベースのシステムでCMakeを使用してビルドするには、次のコマンドを実行することをお勧めします：

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

これにより、mGBAがビルドされ、`/usr/bin`および`/usr/lib`にインストールされます。インストールされている依存関係は自動的に検出され、依存関係が見つからない場合に無効になる機能は、`cmake`コマンドを実行した後に警告として表示されます。

macOSを使用している場合、手順は少し異なります。homebrewパッケージマネージャーを使用していると仮定すると、依存関係を取得してビルドするための推奨コマンドは次のとおりです：

	brew install cmake ffmpeg libzip qt5 sdl2 libedit lua pkg-config
	mkdir build
	cd build
	cmake -DCMAKE_PREFIX_PATH=`brew --prefix qt5` ..
	make

macOSでは`make install`を実行しないでください。正しく動作しないためです。

#### Windows開発者ビルド

##### MSYS2

Windowsでの開発用ビルドにはMSYS2を使用することをお勧めします。MSYS2の[ウェブサイト](https://msys2.github.io)に記載されているインストール手順に従ってください。32ビットバージョン（「MSYS2 MinGW 32-bit」）を実行していることを確認してください（x86_64用にビルドする場合は64ビットバージョン「MSYS2 MinGW 64-bit」を実行してください）。必要な依存関係をインストールするために次の追加コマンド（中括弧を含む）を実行します（このコマンドは1100MiB以上のパッケージをダウンロードするため、長時間かかることに注意してください）：

	pacman -Sy --needed base-devel git ${MINGW_PACKAGE_PREFIX}-{cmake,ffmpeg,gcc,gdb,libelf,libepoxy,libzip,lua,pkgconf,qt5,SDL2,ntldd-git}

次のコマンドを実行してソースコードをチェックアウトします：

	git clone https://github.com/mgba-emu/mgba.git

最後に、次のコマンドを実行してビルドします：

	mkdir -p mgba/build
	cd mgba/build
	cmake .. -G "MSYS Makefiles"
	make -j$(nproc --ignore=1)

このWindows用mGBAビルドは、実行に必要なDLLが分散しているため、配布には適していないことに注意してください。ただし、開発には最適です。ただし、そのようなビルドを配布する必要がある場合（たとえば、MSYS2環境がインストールされていないマシンでのテスト用）、`cpack -G ZIP`を実行すると、必要なDLLをすべて含むzipファイルが準備されます。

##### Visual Studio

Visual Studioを使用してビルドするには、同様に複雑なセットアップが必要です。まず、[vcpkg](https://github.com/Microsoft/vcpkg)をインストールする必要があります。vcpkgをインストールした後、いくつかの追加パッケージをインストールする必要があります：

    vcpkg install ffmpeg[vpx,x264] libepoxy libpng libzip lua sdl2 sqlite3

このインストールでは、Nvidiaハードウェアでのハードウェアアクセラレーションビデオエンコーディングはサポートされません。これが重要な場合は、事前にCUDAをインストールし、前のコマンドに`ffmpeg[vpx,x264,nvcodec]`を置き換えます。

Qtもインストールする必要があります。ただし、Qtは合理的な組織ではなく、困窮している会社によって所有および運営されているため、最新バージョンのオフラインオープンソースエディションインストーラーは存在しないため、[旧バージョンのインストーラー](https://download.qt.io/official_releases/qt/5.12/5.12.9/qt-opensource-windows-x86-5.12.9.exe)に戻る必要があります（これには無用なアカウントの作成が必要ですが、一時的に無効なプロキシを設定するか、ネットワークを無効にすることで回避できます）、オンラインインストーラーを使用する（いずれにしてもアカウントが必要です）、またはvcpkgを使用してビルドする（遅い）。これらはすべて良い選択肢ではありません。インストーラーを使用する場合は、適用可能なMSVCバージョンをインストールする必要があります。オフラインインストーラーはMSVC 2019をサポートしていないことに注意してください。vcpkgを使用する場合、次のようにインストールする必要があります。特にクアッドコア以下のコンピュータではかなりの時間がかかります：

    vcpkg install qt5-base qt5-multimedia

次に、Visual Studioを開き、「リポジトリのクローンを作成」を選択し、`https://github.com/mgba-emu/mgba.git`を入力します。Visual Studioがクローンを完了したら、「ファイル」>「CMake」に移動し、チェックアウトされたリポジトリのルートにあるCMakeLists.txtファイルを開きます。そこから、他のVisual Studio CMakeプロジェクトと同様にVisual StudioでmGBAを開発できます。

#### ツールチェーンビルド

devkitARM（3DS用）、devkitPPC（Wii用）、devkitA64（Switch用）、またはvitasdk（PS Vita用）を持っている場合は、次のコマンドを使用してビルドできます：

	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../src/platform/3ds/CMakeToolchain.txt ..
	make

次のプラットフォーム用に`-DCMAKE_TOOLCHAIN_FILE`パラメータを置き換えます：

- 3DS：`../src/platform/3ds/CMakeToolchain.txt`
- Switch：`../src/platform/switch/CMakeToolchain.txt`
- Vita：`../src/platform/psp2/CMakeToolchain.vitasdk`
- Wii：`../src/platform/wii/CMakeToolchain.txt`

### 依存関係

mGBAには厳密な依存関係はありませんが、特定の機能には次のオプションの依存関係が必要です。依存関係が見つからない場合、これらの機能は無効になります。

- Qt 5：GUIフロントエンド用。オーディオにはQt MultimediaまたはSDLが必要です。
- SDL：より基本的なフロントエンドおよびQtフロントエンドでのゲームパッドサポート用。SDL 2が推奨されますが、1.2もサポートされています。
- zlibおよびlibpng：スクリーンショットサポートおよびPNG内セーブステートサポート用。
- libedit：コマンドラインデバッガーサポート用。
- ffmpegまたはlibav：ビデオ、GIF、WebP、およびAPNGの録画用。
- libzipまたはzlib：zipファイルに保存されたROMの読み込み用。
- SQLite3：ゲームデータベース用。
- libelf：ELF読み込み用。
- Lua：スクリプト用。
- json-c：スクリプトの`storage` API用。

SQLite3、libpng、およびzlibはエミュレーターに含まれているため、最初に外部でコンパイルする必要はありません。

脚注
---------

<a name="missing">[1]</a> 現在欠けている機能は次のとおりです

- モード3、4、および5のOBJウィンドウ（[バグ#5](http://mgba.io/b/5)）

<a name="flashdetect">[2]</a> フラッシュメモリサイズの検出は一部のケースで機能しません。これらは実行時に構成できますが、そのようなケースに遭遇した場合はバグを報告することをお勧めします。

<a name="osxver">[3]</a> 10.9はQtポートにのみ必要です。10.7またはそれ以前のバージョンでQtポートをビルドまたは実行することは可能かもしれませんが、公式にはサポートされていません。SDLポートは10.5で動作することが知られており、古いバージョンでも動作する可能性があります。

[downloads]: http://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba/

著作権
---------

mGBAの著作権は© 2013 – 2023 Jeffrey Pfauに帰属します。これは[Mozilla Public License version 2.0](https://www.mozilla.org/MPL/2.0/)の下で配布されています。配布されたLICENSEファイルにライセンスのコピーが含まれています。

mGBAには次のサードパーティライブラリが含まれています：

- [inih](https://github.com/benhoyt/inih)、著作権© 2009 – 2020 Ben Hoyt、BSD 3-clauseライセンスの下で使用。
- [LZMA SDK](http://www.7-zip.org/sdk.html)、パブリックドメイン。
- [MurmurHash3](https://github.com/aappleby/smhasher)、Austin Applebyによる実装、パブリックドメイン。
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/)、パブリックドメイン。
- [SQLite3](https://www.sqlite.org)、パブリックドメイン。

ゲームパブリッシャーであり、商業利用のためにmGBAのライセンスを取得したい場合は、[licensing@mgba.io](mailto:licensing@mgba.io)までメールでお問い合わせください。
