# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projet

Passerelle MIDI sur réseau WiFi embarquée sur **ESP32-S3**, construite avec **ESP-IDF v6.0**.

## Commandes essentielles

```bash
# Configurer l'environnement (à faire une fois par shell)
source ~/.espressif/v6.0/esp-idf/export.sh

# Compiler
idf.py build

# Flasher + moniteur série (PORT détecté automatiquement si omis)
idf.py -p PORT flash monitor

# Configuration Kconfig
idf.py menuconfig

# Nettoyage complet (rebuild from scratch)
idf.py fullclean
```

> `compile_commands.json` est généré dans `build/` à chaque `idf.py build` — nécessaire pour que clangd fonctionne correctement.

## Architecture

```
main/
  main.cpp          # Point d'entrée app_main() — init NVS + sous-systèmes
  networking.cpp/h  # Init WiFi STA, canal fixe (NETWORKING_WIFI_CHANNEL)
CMakeLists.txt      # Projet CMake ESP-IDF, target esp32s3, MINIMAL_BUILD ON
```

Dépendances ESP-IDF déclarées dans `main/CMakeLists.txt` : `nvs_flash`, `esp_event`, `esp_netif`, `esp_wifi`, `transport`.

Le WiFi est initialisé en mode STA sur un canal fixe défini dans `networking.h`. La passerelle MIDI utilisera `transport` pour envoyer/recevoir des données MIDI sur le réseau.

## Toolchain & clangd

- Cible : `esp32s3`
- IDF : `/Users/eloi/.espressif/v6.0/esp-idf`
- clangd : `/Users/eloi/.espressif/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/clangd`
- `compile_commands.json` : `build/` (configuré dans `.vscode/settings.json`)

## Conventions C++

Voir `.claude/rules/cpp-standards.md` pour les règles complètes (naming, error handling, FreeRTOS, ISR, logging).

Points clés :
- `esp_err_t` pour toute fonction faillible, `ESP_ERROR_CHECK()` en `app_main`
- `ESP_LOGx(TAG, …)` uniquement — pas de `printf`
- Headers ESP-IDF indirects à conserver : `// IWYU pragma: keep`
- `#define` alignés en colonnes (`AlignConsecutiveMacros` dans `.clang-format`)
