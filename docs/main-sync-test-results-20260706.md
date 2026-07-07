# 同步最新 main 后验证记录

记录日期：2026-07-06

文档性质：历史验证记录。本文件对应 `docs/main-sync-workplan-20260706.md`；2026-07-07 最新合并前计划见 `docs/main-sync-workplan-20260707.md`。

## 1. 同步范围

当前分支：

- `refactor/network-internals`

同步前分支提交：

- `0f62045 docs: add main sync work plan`

同步的主线提交：

- `e3526f9 chore: update gitignore for local build artifacts`

本次操作：

- 已在当前分支合入 `origin/main`。
- 已创建本地备份分支：

```text
backup/refactor-network-internals-before-main-sync-20260706
```

## 2. 冲突处理结果

本次合并出现冲突的文件：

```text
README.md
src/app/appcontroller.cpp
src/network/networkmanager.cpp
src/network/networkmanager.h
```

处理原则：

- 保留 `main` 新增的统一游戏入口和 Survivor 相关代码。
- 保留当前分支的网络层拆分：
  - `LineJsonProtocol`
  - `NetworkAddressUtils`
  - `RoomDiscoveryService`
- 保留最后一步同步修复，确保五子棋和飞行棋结束时最后一步先于 `game_over` 应用。
- `NetworkManager` 继续作为网络门面，不重新承担 UDP 房间发现细节。
- `RoomDiscoveryService` 改为使用 `LanBoard::normalizeGameId()` 和 `LanBoard::gameName()`，避免 Survivor 房间被错误归一化为五子棋。

## 3. 合并中发现并修复的问题

### 房主开始游戏路径被 `isConnected()` 截走

现象：

- 控制层 E2E 测试中，五子棋房间已满足开始条件。
- `RoomManager::canStart()` 为 true。
- 调用 `RoomManager::startGame()` 后，房主和客户端都没有进入游戏页。

原因：

- 最新主线中 `NetworkManager::isConnected()` 对房主也会返回 true。
- `AppController::startCurrentGameSession()` 先判断 `isConnected()`，导致房主误走客户端发送 `start_game` 的路径，然后直接返回。
- 房主没有执行本地启动、广播 `game_start` 和页面跳转。

修复：

- 在 `AppController::startCurrentGameSession()` 中先判断是否为房主。
- 只有非房主且已连接时，才走客户端 `sendStartGame()` 路径。

结果：

- 修复后控制层 E2E 重新通过。

## 4. 已执行验证

### 4.1 格式检查

命令：

```powershell
git diff --check
git diff --cached --check
```

结果：通过。

### 4.2 主项目构建

命令：

```powershell
cmake --build build\codex-branch-check
```

结果：通过。

说明：

- 合并后 CMake 自动重新配置。
- `appLanBoard.exe` 和 `lanboardServer.exe` 构建成功。
- 仍有已知非阻塞提示：
  - `WrapVulkanHeaders` 未找到。
  - Qt QML policy `QTP0004` dev warning。

### 4.3 本地在线服务端 E2E

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build\codex-online-server-e2e\online-server-e2e.ps1
```

结果：通过。

验证点：

- 未加入房间的客户端发送 `ready` 后收到 `error: not_joined`。
- Host 创建五子棋在线房间。
- Observer 能通过 `list_rooms` 看到房间。
- Guest 能通过 `join_room` 加入房间。
- Host 和 Guest 准备后能开始游戏。
- 游戏开始后房间列表显示 `inGame = true`。
- Host 和 Guest 的五子棋落子能双向同步。

本次测试端口：

```text
59696
```

### 4.4 局域网发现服务级测试

命令：

```powershell
cmake -S build\codex-lan-discovery-check -B build\codex-lan-discovery-check\build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\mingw_64 -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build\codex-lan-discovery-check\build
build\codex-lan-discovery-check\build\lanDiscoveryCheck.exe
```

结果：通过。

验证点：

- `RoomDiscoveryService` 能发布房间。
- discoverer 能发现 publisher。
- 房间状态更新后 discoverer 能收到更新。
- publisher 停止发布后，房间能在过期清理后移除。
- publisher 不会发现自己的房间。

### 4.5 控制层 E2E

命令：

```powershell
cmake -S build\codex-appcontroller-e2e -B build\codex-appcontroller-e2e\build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\mingw_64 -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build\codex-appcontroller-e2e\build
build\codex-appcontroller-e2e\build\appControllerE2E.exe
```

结果：通过。

覆盖：

- 五子棋：
  - 创建房间。
  - 加入房间。
  - 准备并开始。
  - 双方落子同步。
  - 胜负和返回房间同步。
- 飞行棋：
  - 创建房间并切换飞行棋。
  - 加入房间。
  - 准备并开始。
  - 掷骰和移动同步。
  - 客户端掷骰请求能到达房主并广播结果。
- 斗地主：
  - 创建斗地主房间。
  - 两个客户端加入。
  - 三人准备并开始。
  - 客户端收到各自私有 `ddz_state`。

### 4.6 桌面 GUI 多实例人工复查

测试方式：

- 使用 `build/codex-branch-check/appLanBoard.exe` 启动多个桌面窗口。
- A 窗口作为房主。
- B/C 窗口作为客户端。
- 由人工按真实页面按钮完成创建、加入、准备、开始和游戏操作。

验证结果：

- 五子棋双实例：通过。
  - 发现、加入、准备、开始、落子同步、返回房间均正常。
- 飞行棋双实例：通过。
  - 发现、加入、准备、开始、掷骰和移动同步均正常。
- 斗地主三实例：通过。
  - 三人加入、准备、开始、出牌和不出同步均正常。
- Survivor 基础检查：通过。
  - 入口、页面、本地试玩和房间选择均无明显异常。

说明：

- 本项重点复查合并后房主点击“开始游戏”的真实 GUI 路径。
- Survivor 本轮只检查基础入口和本地试玩，不覆盖实时多人战斗同步。

## 5. 临时测试工程调整

以下调整只发生在被 `.gitignore` 覆盖的 `build/` 临时测试目录，不进入提交：

- `build/codex-online-server-e2e/online-server-e2e.ps1`
  - 优先使用 `build/codex-branch-check/lanboardServer.exe`。
- `build/codex-lan-discovery-check/CMakeLists.txt`
  - 增加仓库根目录 include 路径，用于找到 `src/common/types.h`。
- `build/codex-appcontroller-e2e/CMakeLists.txt`
  - 增加 Survivor controller 源文件。
  - 增加 Qt Gui 依赖，用于 `QVector2D`。
- `build/codex-appcontroller-e2e/main.cpp`
  - 把旧接口 `startDouDiZhuRoomAsHost()` 改为 `startRoomAsHost("doudizhu")`。

## 6. 当前仍未覆盖

以下项目仍需要人工或外部环境：

- Android 真机安装和运行。
- 跨设备同 Wi-Fi UDP 广播发现。
- 远端 ECS 在线房间真实公网流程。
- Survivor 实时多人战斗同步。

说明：

- Survivor 基础代码已随主线保留并参与主项目构建。
- 本次控制层 E2E 仍重点覆盖五子棋、飞行棋和斗地主三款已完成联机闭环的游戏。

## 7. 当前判断

本次同步最新 `main` 后，当前分支已经完成主要冲突整合。

从自动化和本机验证看：

- 桌面构建可用。
- 网络拆分仍可用。
- 局域网发现服务级行为仍可用。
- 三款回合制游戏的控制层联机闭环仍可用。
- 本地在线服务端最小端到端流程仍可用。

桌面 GUI 多实例人工复查已完成，结果正常。当前分支已经适合更新合并请求并进入组员 review。
