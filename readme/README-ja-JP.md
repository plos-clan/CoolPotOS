# x86向けCoolPotOS（クールポットオーエス）

## 翻訳

- [English](/README.md)
- [简体中文](/readme/README-zh-CN.md)
- [Français](/readme/README-fr-FR.md)
- **日本語**

## 紹介

これは [ia32](https://en.wikipedia.org/wiki/IA-32) および [amd64](https://en.wikipedia.org/wiki/X86-64) アーキテクチャ向けの簡易的なオペレーティングシステムです。

## モジュール

- min0911Yの `pl_readline` [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- wenxuanjunの `os_terminal` [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)

## Build & Run

### Environment

次のコンポーネントをインストールするが必要があります。

- Xmake
- NASM
- Zig (xmakeからインストールできなかったら、手動でインストールする必要があるかもしれません。)
- Windows subsystem for Linux (Ubuntu 22.04)
  - xorriso
  - qemu-system-i386

## 実行方法

- ターミナルで`xmake run`と入力すると、自動的にビルドされて実行されます。

## ライセンス

プロジェクトはMITライセンスに従います。誰でも自由的に使用できます。[LICENSE](LICENSE)を参照してください。

## 貢献

プロジェクトへのプルリクエスト・Issueの作成を歓迎します。あとでゆっくりくつろいでください。

## 貢献者

- XIAOYI12 - OS開発者
- min0911Y - OSのファイルシステム開発者
- copi143 - 新しいユーザーヒープフレームワークの開発者
- QtLittleXu - OSドキュメンテーションの作成
- ViudiraTech - PCIドライバーの最適化
- Vinbe Wan - IICドライバーの開発者
- A4-Tacks - ビルドスクリプトの作成
- wenxuanjun - OSコードのリファクタリング
- Minsecrus - メモリー使用量の最適化
- CLimber-Rong - ソフトウェア開発者
- shiyu - デバッグとコメントの作成
- 27Onion - フランス語READMEの翻訳
- LY-Xiang - Actionsの最適化
- suhuajun-github - AHCIドライバーでのバッグを修正
