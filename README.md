# LanBoard

LanBoard (`桌域`) is a Qt Quick-based board game networking platform MVP.

## Current scope

- Qt Quick + QML mobile-first client
- C++ application, lobby, network, and game modules
- MVP repository skeleton for incremental team development

## Module boundaries

- `qml/`: UI pages and reusable QML components
- `src/app/`: top-level coordination between UI and backend modules
- `src/network/`: transport and session communication entry points
- `src/lobby/`: room and player state management
- `src/game/`: game flow management and future board game logic
- `src/common/`: shared types and lightweight cross-module definitions
