<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>Un système d'exploitation jouet simple.</h3>

<img alt="Licence GitHub" src="https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Dernière version GitHub" src="https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Étoiles du dépôt GitHub" src="https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Problèmes GitHub" src="https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square"/>
<img alt="Matériel" src="https://img.shields.io/badge/Matériel-i386_x64-blue?style=flat-square"/>
</div>

---

Langues
: [English](../README.md)
| [简体中文](README-zh-CN.md)
| **Français**
| [日本語](README-ja-JP.md)

## Introduction

Ceci est un système d'exploitation simple pour les architectures [ia32](https://en.wikipedia.org/wiki/IA-32)
et [amd64](https://en.wikipedia.org/wiki/X86-64).

## Modules

- `pl_readline` par min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` par wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `liballoc` par wenxuanjun [plos-clan/liballoc](https://github.com/plos-clan/liballoc)

## Compilation et exécution

Vous devez installer les outils suivants sur votre ordinateur :

- xmake
- xorriso
- QEMU
- NASM (uniquement pour i386)
- Zig (uniquement pour i386, installé automatiquement par xmake)
- git (uniquement pour x86_64, pour la macro `GIT_VERSION`)
- clang (uniquement pour x86_64)
- lld (uniquement pour x86_64, pour lier les objets LTO)
- Chaîne d'outils Rust nightly (uniquement pour x86_64)
- cbindgen (uniquement pour x86_64, utilisez `cargo install cbindgen`)
- oib (uniquement pour x86_64 et la création d'image, utilisez `cargo install oib`)

Récupérez d'abord les sous-modules :

```bash
git submodule update --init --recursive
```

### Commandes disponibles

- `xmake run run32` - Compiler et exécuter la version i386.
- `xmake run run64` - Compiler et exécuter la version x86_64 (ISO).
- `xmake build iso32` - Créer une image ISO pour CoolPotOS i386.
- `xmake build iso64` - Créer une image ISO pour CoolPotOS x86_64.
- `xmake build img64` - Créer une image de disque amorçable pour CoolPotOS x86_64.

## Développement

Vous pouvez générer un fichier `compile_commands.json` avec :

```bash
xmake project -k compile_commands
```

Cela permet à votre éditeur de localiser les fichiers sources et d'améliorer la coloration syntaxique.

## Licence

Ce projet est sous [Licence MIT](LICENSE).

## Fonctionnalités

### AMD64

Basé sur un démarrage UEFI BIOS. \
Utilise le chargeur d'amorçage Limine.

- [x] Gestion de la mémoire avec table de pages à 4 niveaux
- [x] xAPIC et x2APIC
- [x] Module du noyau
- [x] Pilote de disque AHCI
- [x] Multi-tâches (processus et threads)
- [x] Clavier et souris PS/2
- [x] Énumération des dispositifs PCIe
- [x] Gestion de l'alimentation ACPI
- [x] Interface VFS VDisk
- [x] File de messages IPC
- [ ] Signaux de processus
- [x] Ordonnanceur multiprocesseur
- [x] Programmes utilisateur
- [x] Système de fichiers de périphériques
- [x] Unité de calcul en virgule flottante
- [ ] Pilote IIC
- [ ] Pilotes NVMe et USB
- [ ] Pilotes PCNET et RTL8169
- [x] Pilotes SB16 et PCSpeaker
- [ ] Pilote TTY

## Contribution

Vous êtes invité à créer des demandes de tirage ou à signaler des problèmes sur ce projet. Ensuite, détendez-vous.

### Contributeurs

- XIAOYI12 - Développeur du système d'exploitation
- min0911Y - Développeur du système de fichiers
- copi143 - Développeur du tas utilisateur
- QtLittleXu - Documentation XuYuxuan OS
- ViudiraTech - Optimisation du pilote PCI
- Vinbe Wan - Développeur IIC
- A4-Tacks - Écriture de scripts de construction
- wenxuanjun(blurryrect) - Développeur du système d'exploitation
- Minsecrus - Optimisation de l'utilisation de la mémoire
- CLimber-Rong - Développeur logiciel
- shiyu - Débogueur et rédacteur de commentaires
- 27Onion - Traduction du README en français
- LY-Xiang - Optimisation du processus des actions
- suhuajun-github - Correction de plusieurs bogues dans le pilote AHCI
- FengHeting - Développeur du pilote SMBIOS
