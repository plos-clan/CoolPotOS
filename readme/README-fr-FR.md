<div align="center">
<img height="200px" src="https://github.com/user-attachments/assets/9542ad95-0f48-43ad-9617-a750db84e907" />

<h1 align="center">CoolPotOS</h1>
<h3>Un système d'exploitation jouet simple.</h3>

![GitHub Repo stars](https://img.shields.io/github/stars/plos-clan/CoolPotOS?style=flat-square)
![GitHub issues](https://img.shields.io/github/issues/plos-clan/CoolPotOS?style=flat-square)
![GitHub License](https://img.shields.io/github/license/plos-clan/CoolPotOS?style=flat-square)
![GitHub release (latest by date)](https://img.shields.io/github/v/release/plos-clan/CoolPotOS?style=flat-square)
![Hardware](https://img.shields.io/badge/Hardware-i386_x64-blue?style=flat-square)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/plos-clan/CoolPotOS)
</div>

---

Langues
: [English](../README.md)
| [简体中文](README-zh-CN.md)
| *Français*
| [日本語](README-ja-JP.md)

## Introduction

Ceci est un système d'exploitation simple pour les architectures [ia32](https://en.wikipedia.org/wiki/IA-32)
et [amd64](https://en.wikipedia.org/wiki/X86-64).

## Modules

- `pl_readline` par min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` par wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)
- `plant-vfs` par min0911Y [plos-clan/plant-vfs](https://github.com/plos-clan/plant-vfs)
- `EEVDF` par xiaoyi1212 [plos-clan/EEVDF](https://github.com/plos-clan/EEVDF)

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

### Options

Vous pouvez utiliser la commande pour définir l'architecture cible (`x86_64` par défaut) :

```bash
xmake f -y --arch=i686
```

### Commandes

- `xmake run` - Construit et exécute l'image **ISO**.
- `xmake build iso` - Construit une image ISO amorçable.
- `xmake build img` - Construit une image disque amorçable (x86_64 uniquement).

## Développement

Vous pouvez générer un fichier `compile_commands.json` avec:

```bash
xmake project -k compile_commands
```

Cela permet à votre éditeur de localiser les fichiers sources et d'améliorer la coloration syntaxique.

## Licence

Ce projet est sous [Licence MIT](LICENSE).

## Documentation

Pour une documentation technique détaillée, veuillez consulter [docs/README.md](docs/README.md).

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
- [x] Pilote TTY
- [x] Pilote SATA/SATAPI

## Contribution

Vous êtes invité à créer des demandes de tirage ou à signaler des problèmes sur ce projet. Ensuite, détendez-vous.

### Contributeurs

* Allez sur [CoolPotOS | Site web](cpos.plos-clan.org) pour voir la liste des contributeurs.
