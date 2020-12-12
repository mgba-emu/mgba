mGBA
====

### [English](./README.md) | [Deutsch](./README_DE.md) | [Español](./README_ES.md) | 日本語 | [中文（简体）](./README_ZH_CN.md)

mGBAは、ゲームボーイアドバンスのゲームを実行するためのエミュレーターです。このエミュレーターは、既存の多くのGBAエミュレーターよりも高速で高精度であることを目指しています。また、他のエミュレーターにはない機能も追加されます。mGBAは、ゲームボーイおよびゲームボーイカラーゲームもサポートしています。

最新のニュースとダウンロードは、[mgba.io](https://mgba.io)をご覧ください。

[![Build status](https://travis-ci.org/mgba-emu/mgba.svg?branch=master)](https://travis-ci.org/mgba-emu/mgba)

機能
--------

- ゲームボーイアドバンスのハードウェアをエミュレート(高精度)[<sup>[1]</sup>](#missing)。
- ゲームボーイ／ゲームボーイカラーのハードウェアをエミュレート
- 高速エミュレーション（ネットブックなどのローエンドハードウェアでもフルスピードで実行可能)
- 重量フロントエンドと軽量フロントエンド用のQtおよびSDLポート
- 同じパソコンでローカルリンクケーブル機能
- セーブタイプの検出機能(フラッシュメモリサイズでも可)[<sup>[2]</sup>](#flashdetect)
- モーションセンサーと振動機能(ゲームコントローラーでのみ使用可能)
- リアルタイムクロック／RTS(設定不要)
- 『太陽アクションRPG』シリーズで太陽センサー
- ゲームボーイカメラ／ゲームボーイプリンター
- 内蔵BIOS、および外部BIOSファイルをロード機能。
- ターボ／早送り機能(Tabキーを押し)
- 巻き戻し機能(半角/全角キーを押し)
- フレームスキップ(最大10まで構成可能)
- スクリーンショット機能
- チートコード機能
- 9つのセーブステートのスロット(スクリーンショットとしても表示可能)
- 録画機能／GIF、WebPとAPNG記録機能
- カードe
- 再マップ可能なコントロール(キーボードとゲームパッド)
- ZIPおよび7zファイルからの読み込み
- IPS、UPSおよびBPSパッチを使用
- コマンドラインインターフェイスとGDBリモートを介したゲームのデバッグ(IDAProと互換性があり)
- エミュレーションの巻き戻し構成可能
- GameSharkとアクションリプレイのスナップショットをロードまたは書き出し可能
- RetroArch／LibretroとOpenEmuのコア
- その他

#### ゲームボーイマッパー(Mappers)

全にサポートされているマッパー:

- MBC1
- MBC1M
- MBC2
- MBC3
- MBC3+RTC
- MBC5
- MBC5+振動機能
- MBC7
- Wisdom Tree(無免許)
- Pokémon Jade/Diamond(無免許)
- BBD(無免許／MBC5)
- Hitek(無免許／MBC5)

部分的にサポートされているマッパー:

- MBC6(フラッシュメモリの書き込み不可)
- MMM01
- Pocket Cam
- TAMA5(RTC不可)
- HuC-1(IR不可)
- HuC-3(RTCとIR不可)

### 機能の追加予定

- ネットワーク化マルチプレイヤーリンクケーブル機能
- Dolphin／JOYバスリンクケーブル機能
- MP2kオーディオミキシング機能(本物よりも高品質な音声)
- ツールアシステッドスピードラン(TAS)の再記録機能
- Luaスクリプト機能
- 包括的なデバッグスイート
- ワイヤレスアダプター機能

サポートされているプラットフォーム
-------------------

- Windows Vista以降
- OS X 10.8(Mountain Lion)[<sup>[3]</sup>](#osxver)以降
- Linux
- FreeBSD
- Nintendo 3DS
- Nintendo Switch
- Wii
- PlayStation Vita

OpenBSDなどの他のUnixライクなプラットフォームも実行できますが、テストされておらず、完全にはサポートされていません。

### システム要求

システム要件は最小限です。Windows Vista以降を実行できるパソコンは、エミュレーターを処理できます。OpenGL1.1以降も必要です。シェーダーと高度な機能には、OpenGL3.2以降が必要です。

ダウンロード
---------

公式ウェブサイトの[ダウンロード(Downloads)][downloads]セクションをご覧ください。ソースコードは[GitHub][source]をご覧ください。

コントロール
--------

コントロールは「設定」メニューで設定できます。多くのゲームコントローラーは、デフォルトで自動的にマップされます。デフォルトのキーボードコントロールは次のとおりです:

- **A**: X
- **B**: Z
- **L**: A
- **R**: S
- **Start**: Enterキー
- **Select**: Backspaceキー

コンパイル
---------

*このセクションはまだ建設中です。*

脚注
---------

<a name="missing">[1]</a> 現在不足している機能:

- モード3、4、および5のOBJウィンドウ([Bug #5](https://mgba.io/b/5))

<a name="flashdetect">[2]</a> フラッシュメモリのサイズ検出機能が動作しない場合があります。これらは実行時に構成できますが、そのような場合はバグを報告することをお勧めします。

<a name="osxver">[3]</a> 10.8はQtポートにのみ必要です。10.7以前でQtポートを構築または実行することは可能かもしれませんが、これは公式にはサポートされていません。SDLポートは10.5で動作することが知られており、古いバージョンでも動作する可能性があります。

[downloads]: https://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba

著作権
---------

Copyright © 2013 – 2020 Jeffrey Pfau、[Mozilla Public License バージョン2.0(https://www.mozilla.org/en-US/MPL/2.0)で配布。このライセンスのコピーは、配布されたLICENSEで入手できます。

mGBAには、次のサードパーティライブラリが含まれています:

- [inih](https://github.com/benhoyt/inih) Copyright © 2009 – 2020 Ben Hoyt、BSD3節ライセンス(BSD 3-clause license)で配布
- [blip-buf](https://code.google.com/archive/p/blip-buf) Copyright © 2003 – 2009 Shay Green、Lesser GNU一般公衆ライセンス(Lesser GNU Public License)で配布
- [LZMA SDK](https://7-zip.org/sdk.html)(パブリックドメイン)
- [MurmurHash3](https://github.com/aappleby/smhasher)、実装者:Austin Appleby(パブリックドメイン)
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio)(パブリックドメイン)
- [SQLite3](https://sqlite.org)(パブリックドメイン)

ゲームパブリッシャーであり、商用利用のためにmGBAのライセンスを取得したい場合, メール([licensing@mgba.io](mailto:licensing@mgba.io))でお問い合わせください。
