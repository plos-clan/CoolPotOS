# CoolPotOS pour x86

## Traductions

- [English](/README.md)
- [简体中文](/readme/README-zh-CN.md)
- **Français**
- [日本語](/readme/README-ja-JP.md)

## Introduction

C'est un système d'exploitation simple pour l'architecture [ia32](https://en.wikipedia.org/wiki/IA-32) et [amd64](https://en.wikipedia.org/wiki/X86-64).

## Modules

- `pl_readline` par min0911Y [plos-clan/pl_readline](https://github.com/plos-clan/pl_readline)
- `os_terminal` par wenxuanjun [plos-clan/libos-terminal](https://github.com/plos-clan/libos-terminal)

## Construire & Exécuter

### Environnement

Vous devez les installer sur votre ordinateur:

- Xmake
- NASM
- Zig (vous pouvez l'installer manuellement si xmake ne peut pas le télécharger pour vous)
- Windows subsystem for Linux (Ubuntu 22.04)
  - xorriso
  - qemu-system-i386

### Étapes

- Exécutez `xmake run` sur votre terminal puis il se construira et s'exécutera automatiquement.

## Licence

Le projet suit la licence MIT. Tout le monde peut l'utiliser gratuitement. Voyez [LICENSE](/LICENSE).

## Contribuer

Bienvenue pour créer des demandes de tirage ou des problèmes pour ce projet. Ensuite, asseyez-vous et détendez-vous.

### Contributeurs

- XIAOYI12 - Développement du Système d'Exploitation
- min0911Y - Développement du Système de Fichiers du SE
- copi143 - Développement d'UserHeap
- QtLittleXu - Rédacteur de la Documentation du SE
- ViudiraTech - Optimisation du Pilote PCI
- Vinbe Wan - Développement d'IIC
- A4-Tacks - Écrire des Scripts de Construction
- wenxuanjun - Développeur du SE
- Minsecrus - Optimisation de l'Utilisation de la Mémoire
- CLimber-Rong - Développeur de Logiciels
- shiyu - Débogage et Écrire des Commentaires
- 27Onion - Traduire README en Français
- LY-Xiang - Optimisé Actions
- suhuajun-github - Correction de plusieurs bugs dans le pilote AHCI
