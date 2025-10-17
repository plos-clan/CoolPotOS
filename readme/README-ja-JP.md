<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>シンプルなトイオペレーティングシステム</h3>

![GitHub Repo stars](https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square)
![GitHub issues](https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square)
![GitHub License](https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square)
![GitHub release (latest by date)](https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square)
![Hardware](https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/plos-clan/CoolPotOS)
</div>

---

Languages
: [English](../README.md)
| [简体中文](README-zh-CN.md)
| [Français](README-fr-FR.md)
| *日本語*

## Introduction

これは、[ia32](https://en.wikipedia.org/wiki/IA-32)および[amd64](https://en.wikipedia.org/wiki/X86-64)
アーキテクチャ向けのシンプルなオペレーティングシステムです。

## Modules

- `pl_readline` by min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` by wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `plant-vfs` by min0911Y [plos-clan/plant-vfs](https://github.com/plos-clan/plant-vfs)
- `EEVDF` by xiaoyi1212 [plos-clan/EEVDF](https://github.com/plos-clan/EEVDF)

## Build & Run

以下のツールをコンピュータにインストールする必要があります：

- xmake
- xorriso
- QEMU
- NASM (i386のみ)
- Zig (i386のみ、xmakeにより自動インストール)
- git (x86_64のみ、`GIT_VERSION`マクロ用)
- clang (x86_64のみ)
- lld (x86_64のみ、LTOオブジェクトのリンク用)
- Rust nightlyツールチェイン (x86_64のみ)
- cbindgen (x86_64のみ、`cargo install cbindgen`でインストール)
- oib (x86_64およびイメージ構築のみ、`cargo install oib`でインストール)

### Options

以下のコマンドを使用して、ターゲットアーキテクチャを設定できます（デフォルトは`x86_64`）：

```bash
xmake f -y --arch=i686
```

### Commands

- `xmake run` - **ISO**イメージをビルドして実行します。
- `xmake build iso` - 起動可能なISOイメージをビルドします。
- `xmake build img` - 起動可能なディスクイメージをビルドします (x86_64のみ)。

## Development

以下のコマンドで`compile_commands.json`ファイルを生成できます：

```bash
xmake project -k compile_commands
```

これにより、エディタがソースファイルを見つけ、シンタックスハイライトを適用できます。

## License

このプロジェクトは[MITライセンス](LICENSE)の下でライセンスされています。

## ドキュメント

詳細な技術ドキュメントについては、[docs/README.md](docs/README.md)を参照してください。

## Feature

### AMD64

UEFI BIOSブートに基づいています。\
Limineブートローダーを使用。

- [x] 4レベルページテーブルメモリ管理
- [x] xAPICおよびx2APIC
- [x] カーネルモジュール
- [x] AHCIディスクドライバ
- [x] マルチタスク（プロセスとスレッド）
- [x] PS/2キーボードとマウス
- [x] PCIeデバイス列挙
- [x] ACPI電源管理
- [x] VFS VDiskインターフェース
- [x] IPCメッセージキュー
- [ ] プロセスシグナル
- [x] マルチプロセッサスケジューラ
- [x] ユーザープログラム
- [x] デバイスファイルシステム
- [x] 浮動小数点ユニット
- [ ] IICドライバ
- [ ] NVMeおよびUSBドライバ
- [ ] PCNETおよびRTL8169ドライバ
- [x] SB16およびPCSpeakerドライバ
- [x] TTYドライバ
- [x] SATA/SATAPIドライバ

## Contributing

このプロジェクトにプルリクエストやイシューを作成することを歓迎します。その後、ゆっくりとお待ちください。

### Contributors

* [CoolPotOS | Website](cpos.plos-clan.org) に行って、貢献者リストを見てください。
