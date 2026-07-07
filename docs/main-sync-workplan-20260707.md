# 再次同步最新 main 的合并前工作计划

计划日期：2026-07-07

文档性质：当前审阅计划。本文件记录 2026-07-07 检查后，`refactor/network-internals` 再次同步最新 `origin/main` 前应执行的工作。历史计划和记录见 `docs/document-index.md`。

## 1. 当前判断

当前分支 `refactor/network-internals` 本身有效，但不适合直接合并到最新 `origin/main`。

已确认事实：

- 本地仓库有效路径：`D:\CodeWork\LanBoard`
- 当前分支：`refactor/network-internals`
- 当前提交：`b5a88bd docs: record main sync gui validation`
- 远端分支：`origin/refactor/network-internals`
- 本地当前分支与远端同名分支一致。
- 最新主线：`origin/main` = `c27b4cc Fix Android startup rendering and nav click-through`
- 当前分支与 `origin/main` 分叉：当前分支多 14 个提交，`origin/main` 多 5 个提交。
- 合并基点：`e3526f9 chore: update gitignore for local build artifacts`
- 工作区只有一个源码外未跟踪文件：`AGENTS.md`，本轮仍不提交。

已重新验证：

- `git diff --check` 通过。
- `git diff --check origin/main...HEAD` 通过。
- 没有真实合并冲突标记残留。
- `cmake --preset qt-mingw-desktop` 配置成功。
- `cmake --build --preset qt-mingw-desktop` 构建成功。
- `appLanBoard.exe` 和 `lanboardServer.exe` 均成功生成。

已知非阻塞提示：

- `WrapVulkanHeaders` 未找到。
- Qt QML policy `QTP0004` 开发警告。

## 2. 本轮目标

本轮目标不是继续扩大网络重构范围，而是让当前分支重新具备合并条件。

必须完成：

- 把最新 `origin/main` 同步进 `refactor/network-internals`。
- 解决同步主线产生的冲突。
- 保留当前分支的网络层拆分成果。
- 保留 `origin/main` 最近新增或整理的运行时、UI、Survivor 和 Android 启动相关改动。
- 重新构建并完成关键回归验证。
- 更新测试记录和 PR 描述。

不做事项：

- 不直接推送到 `main`。
- 不删除或回退主线新增功能来规避冲突。
- 不把 `RoomDiscoveryService` 重新塞回 `NetworkManager`。
- 不在本轮混入新的 UI 重构、游戏规则重构或 Android 打包重构。
- 不提交构建产物、临时测试输出、APK、exe 或 `AGENTS.md`。

## 3. 推荐合并策略

推荐使用 merge，把 `origin/main` 合入当前分支。

推荐原因：

- 当前分支已经推送到 `origin/refactor/network-internals`。
- 分支已有多份工作计划和测试记录，merge history 更容易解释同步主线的过程。
- 不需要强推，适合组内协作和 PR 审阅。

推荐命令：

```powershell
git fetch origin --prune
git status --short --branch
git branch backup/refactor-network-internals-before-main-sync-20260707
git merge origin/main
```

不推荐：

- 不建议直接在 GitHub 网页上合并。
- 不建议 rebase 后强推，除非团队明确要求线性历史。
- 不建议用 `ours` 或 `theirs` 整文件覆盖解决冲突。

## 4. 预计冲突范围

无工作区改动的合并模拟已经确认冲突集中在以下文件：

```text
README.md
src/app/appcontroller.cpp
src/network/networkmanager.cpp
src/network/networkmanager.h
```

合并时还需要重点复核以下文件是否被自动合并正确：

```text
CMakeLists.txt
CMakePresets.json
src/common/types.h
src/lobby/roommanager.*
src/server/serverapp.*
qml/Main.qml
qml/pages/RoomPage.qml
qml/pages/SurvivorPage.qml
```

原因：

- `origin/main` 已继续推进运行时架构和 Survivor 相关能力。
- 当前分支保留了网络内部拆分、局域网发现服务、逐行 JSON 协议和地址选择工具。
- 两边都改过 `AppController` 和 `NetworkManager`，需要人工整合语义。

## 5. 冲突处理原则

### 5.1 `README.md`

处理原则：

- 以最新 `origin/main` 的项目现状为基础。
- 保留当前分支对网络层拆分的说明。
- 明确 Survivor 的当前能力边界。
- 不再使用过期的“三款游戏已完成全部联机”作为总体描述。

合并后文档应表达：

- 五子棋、斗地主、飞行棋是已完成主要联机闭环的游戏。
- Survivor 已接入入口、页面和本地原型，实时联机同步仍需单独验证或继续开发。
- 网络层由 `NetworkManager`、`LineJsonProtocol`、`NetworkAddressUtils`、`RoomDiscoveryService` 分担职责。
- 在线大厅和独立服务端能力仍保留。

### 5.2 `src/app/appcontroller.cpp`

处理原则：

- 以 `origin/main` 的统一运行时和游戏流程为基础。
- 保留 Survivor 入口、启动、返回和页面流转逻辑。
- 保留当前分支修复的“最后一步同步先于 game_over”行为。
- 房主开始游戏逻辑不能被 `isConnected()` 分支误截走。

重点复核：

- 房主点击开始后必须执行本地启动、广播 `game_start`、页面跳转。
- 非房主点击开始时仍只发送 `start_game` 请求。
- 游戏结束时，最后一次游戏状态或移动结果必须先被客户端应用，再广播 `game_over`。
- 结束后房间准备状态清空，房间状态重新广播。
- Survivor 本地模式和房间入口不被破坏。

建议保留思路：

```cpp
if (m_networkManager->isHost()) {
    m_networkManager->setDiscoveryGameInProgress(false);
    QTimer::singleShot(0, this, [this, winner]() {
        if (!m_networkManager->isHost())
            return;
        m_networkManager->broadcastGameOver(winner);
        m_roomManager->clearReadyStates();
        broadcastCurrentRoomState();
    });
}
```

实际落点应按合并后的函数结构调整，不机械复制旧代码。

### 5.3 `src/network/networkmanager.h`

处理原则：

- `NetworkManager` 继续作为网络门面。
- 局域网发现细节继续由 `RoomDiscoveryService` 承担。
- 不恢复旧的 UDP socket、发现定时器、房间缓存结构体到 `NetworkManager`。
- 保留主线公共游戏定义和当前分支网络拆分头文件。

应保留：

```text
LineJsonProtocol
NetworkAddressUtils
RoomDiscoveryService
LanBoard::normalizeGameId()
LanBoard::gameName()
```

应避免恢复：

```text
NetworkManager 内部 discover_room / room_announce UDP 细节
NetworkManager 内部 discovered room 缓存结构
NetworkManager 内部 Android 组播锁直接管理
```

Android 组播锁如果仍需要，应留在 `RoomDiscoveryService` 中。

### 5.4 `src/network/networkmanager.cpp`

处理原则：

- 保留 `LineJsonProtocol::encode()` 和 `LineJsonProtocol::takeMessages()`。
- 保留 `NetworkAddressUtils` 的本机地址选择。
- 保留 `RoomDiscoveryService` 的启动、停止、刷新、清空、发布信息更新和信号转发。
- 接入 `src/common/types.h` 中的公共游戏定义。
- 保留主线最新在线房间、Survivor、运行时相关消息处理。

重点复核函数：

```text
sendJson()
onReadyRead()
onOnlineLobbyReadyRead()
applyOnlineRooms()
startRoomDiscovery()
stopRoomDiscovery()
refreshRoomDiscovery()
clearDiscoveredRooms()
setDiscoveryHostName()
setDiscoveryGameInProgress()
setDiscoveryRoomInfo()
updateDiscoveryIdentity()
sendSwitchRoomGame()
```

合并后要求：

- 局域网发现列表仍能更新到 QML。
- 房间 `gameId` 不应把 `survivor` 错误归一化为 `gomoku`。
- 在线房间列表中的游戏名、人数、状态保持正确。
- 断开、重连和错误消息路径不丢失。

## 6. 自动合并后第一轮检查

解决冲突后立即执行：

```powershell
git status --short --branch
rg -n --glob '!build*/**' --glob '!cmake-build-*/**' --glob '!android-build/**' '<<<<<<<|=======$|>>>>>>>' .
git diff --check
cmake --preset qt-mingw-desktop
cmake --build --preset qt-mingw-desktop
```

验收标准：

- 没有 unresolved conflict。
- 没有冲突标记残留。
- `git diff --check` 无错误。
- 桌面客户端和独立服务端均构建成功。
- 构建只产生 `.gitignore` 覆盖的构建目录变化。

## 7. 关键回归测试计划

### 7.1 网络发现服务级测试

目标：证明 `RoomDiscoveryService` 拆分仍有效。

覆盖：

- 发布房间。
- 发现房间。
- 刷新扫描。
- 房间状态更新。
- 自发布过滤。
- 去重。
- 过期清理。
- Survivor 或未知游戏 ID 不被错误归一化。

如果旧临时测试工程无法直接编译，应先按合并后代码结构更新临时测试工程。

### 7.2 控制层 E2E

目标：证明 `AppController` 和 `NetworkManager` 整合后仍能完成核心联机闭环。

最低覆盖：

- 五子棋：
  - 创建房间、加入、准备、开始。
  - 双方落子同步。
  - 胜负结束和返回房间。
  - 最后一步先于 `game_over` 应用。

- 飞行棋：
  - 创建房间、加入、准备、开始。
  - 掷骰同步。
  - 移动同步。
  - 最后动作和结束状态顺序正确。

- 斗地主：
  - 三人加入。
  - 三人准备并开始。
  - 私有 `ddz_state` 下发正确。
  - 出牌和不出请求正常。

### 7.3 本地在线服务端 E2E

目标：证明独立服务端和在线房间最小流程没有被破坏。

覆盖：

- 未加入房间发送 `ready` 返回错误。
- Host 创建在线房间。
- Observer 拉取房间列表。
- Guest 加入房间。
- 房间状态同步。
- 开始游戏。
- 至少一款回合制游戏的动作同步。

### 7.4 桌面 GUI 多实例复查

目标：确认真实 QML 页面流程正常。

最低覆盖：

- 五子棋双实例完整流程。
- 飞行棋双实例核心流程。
- 斗地主三实例开局和一轮操作。
- Survivor 基础入口、本地试玩、房间切换不崩溃。

说明：

- Survivor 实时多人战斗同步如果主线尚未完成，不作为本轮网络重构合并的硬阻塞项。
- 但本轮合并不能破坏 Survivor 的入口、页面、构建和基础房间流转。

## 8. 需要更新的文档

合并和验证完成后，建议更新或新增测试记录。

建议新增：

```text
docs/main-sync-test-results-20260707.md
```

记录内容：

- 同步的 `origin/main` 提交。
- 冲突文件列表。
- 冲突处理原则。
- 构建命令和结果。
- 自动测试结果。
- GUI 多实例测试结果。
- 仍未覆盖项。

建议补充更新：

- `docs/network-refactor-work-report-20260705.md`
  - 更新最新主线基线。
  - 说明网络拆分已与最新运行时结构重新整合。

- `README.md`
  - 如果合并过程中 README 已变化，确保最终状态准确反映当前游戏和网络能力。

保留历史文档：

- 不覆盖 `docs/pre-main-merge-workplan-20260705.md`。
- 不覆盖 `docs/main-sync-workplan-20260706.md`。
- 这些文件作为 2026-07-05 和 2026-07-06 的历史记录保留。

## 9. 提交建议

推荐提交结构：

1. merge commit：

```text
merge: sync main into network internals
```

2. 如测试记录在 merge 后补充，单独提交：

```text
docs: record 20260707 main sync validation
```

提交前检查：

```powershell
git status --short --branch
git diff --check
git log --oneline --decorate -8
```

提交时注意：

- 不提交 `AGENTS.md`。
- 不提交 `build*/`。
- 不提交 `*.exe`、`*.apk`、日志或缓存。
- 如果临时测试工程位于 `build/`，保持其不进入 Git。

## 10. PR 更新建议

PR 描述应补充：

- 本分支已同步最新 `origin/main`。
- 已解决 `README.md`、`AppController`、`NetworkManager` 冲突。
- 保留网络内部拆分：
  - `LineJsonProtocol`
  - `NetworkAddressUtils`
  - `RoomDiscoveryService`
- 保留主线运行时、Survivor 和 Android 启动相关改动。
- 已重新验证桌面构建。
- 列出本地自动测试和 GUI 多实例测试结果。
- 明确未覆盖项：
  - Android 真机。
  - 跨设备同 Wi-Fi UDP 广播。
  - 真实 ECS 公网流程。
  - Survivor 实时多人战斗同步。

建议 PR 结论：

```text
当前分支已重新同步最新 main。网络层拆分仍保留，主线新增运行时和 Survivor 改动未回退。桌面构建、核心联机流程和本地服务端流程验证通过后，建议进入组员 review。
```

## 11. 合并前最终通过条件

满足以下条件后，才建议请求合并：

- 当前分支包含最新 `origin/main`。
- GitHub PR 不再显示冲突。
- `git status --short --branch` 没有非预期改动。
- `git diff --check` 通过。
- 桌面构建通过。
- `appLanBoard.exe` 和 `lanboardServer.exe` 可生成。
- 局域网发现服务级测试通过。
- 五子棋、飞行棋、斗地主核心联机流程通过。
- 本地在线服务端最小 E2E 通过。
- Survivor 入口、页面和本地试玩基础检查通过。
- 最新验证结果已经写入文档。
- 未覆盖项已经明确写入 PR 或测试记录。

## 12. 当前执行建议

建议按以下顺序推进：

1. 先提交或确认本计划文档是否需要纳入分支。
2. 创建备份分支。
3. merge 最新 `origin/main`。
4. 解决 4 个已知冲突文件。
5. 运行静态检查和桌面构建。
6. 运行网络发现、控制层、本地服务端测试。
7. 做桌面 GUI 多实例复查。
8. 写入 2026-07-07 验证记录。
9. 推送 `refactor/network-internals`。
10. 更新 PR 并请求组员 review。

当前分支的网络重构成果不应废弃，但必须先完成这轮主线同步后再进入最终合并。
