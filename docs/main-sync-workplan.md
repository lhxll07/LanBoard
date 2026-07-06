# 同步最新 main 的下一步工作计划

计划日期：2026-07-06

## 1. 当前实际情况

当前分支：

- 本地分支：`refactor/network-internals`
- 远端分支：`origin/refactor/network-internals`
- 当前提交：`9c804b1 docs: record online server e2e test results`
- 当前本地和远端分支一致。
- 当前未跟踪文件只有 `AGENTS.md`，后续仍不提交该文件。

当前主线：

- 最新 `origin/main`：`e3526f9 chore: update gitignore for local build artifacts`
- `origin/main` 比当前分支多 3 个提交。
- 当前分支比 `origin/main` 多 11 个提交。

分支关系：

```text
origin/main...HEAD = 3 11
```

说明：

- 当前分支本身有效，网络重构、文档和测试记录都已经提交并推送。
- 但当前分支已经不是基于最新 `main`。
- 云端 `main` 已合入 `unify-game-runtime`，新增 Survivor 游戏入口，并改动了 `AppController`、`NetworkManager`、`RoomPage`、`CMakeLists.txt` 等文件。
- 只读合并预检查已经确认存在真实冲突，不能直接合并。

## 2. 当前分支的价值

当前分支仍然有明确价值，不应废弃。

已经完成的核心工作：

- 拆出 `LineJsonProtocol`，统一逐行 JSON 编解码。
- 拆出 `NetworkAddressUtils`，集中处理本机 IPv4 地址选择。
- 拆出 `RoomDiscoveryService`，把局域网房间发现从 `NetworkManager` 中分离。
- 修复五子棋和飞行棋联机结束时，最后一步可能晚于 `game_over` 到达的问题。
- 补充网络消息文档、重构报告、并入前计划和测试记录。

已经验证过的测试：

- 桌面构建通过。
- 局域网发现服务级冒烟测试通过。
- 控制层联机端到端测试通过。
- 桌面 GUI 本地多实例测试通过：
  - 五子棋双实例。
  - 飞行棋双实例。
  - 斗地主三实例。
- 本地 `lanboardServer` 在线房间端到端测试通过。
- Debug Android APK 已能构建。

## 3. 当前主要问题

当前主要问题不是你的分支无效，而是主线已经前进，产生了整合成本。

已知冲突集中在：

```text
CMakeLists.txt
README.md
src/app/appcontroller.cpp
src/network/networkmanager.cpp
src/network/networkmanager.h
```

其中最重要的是：

- `AppController`：
  - `main` 已把多游戏启动和结束流程整理为统一函数。
  - 当前分支在旧结构中修复了最后一步同步顺序。
  - 整合时应保留 `main` 的统一流程，同时重新应用最后一步同步修复。

- `NetworkManager`：
  - `main` 仍保留内置局域网发现逻辑，并接入了 `LanBoard::normalizeGameId()`、`LanBoard::gameName()` 等公共游戏定义。
  - 当前分支已经把局域网发现拆到 `RoomDiscoveryService`。
  - 整合时应保留拆分结构，同时接入 `main` 的公共游戏定义和 Survivor 支持。

- `CMakeLists.txt`：
  - `main` 新增 Survivor 相关 C++ 和 QML 文件。
  - 当前分支新增网络辅助模块。
  - 整合时两边都要保留。

- `README.md` 和分工文档：
  - 当前分支文档仍偏向三款游戏。
  - 最新 `main` 已包含 Survivor。
  - 整合后文档要改成当前真实状态。

## 4. 总体策略

推荐策略：把 `origin/main` 合入当前分支，而不是 rebase。

原因：

- 当前分支已经推送，并且可能已经用于合并请求或组内 review。
- 使用 merge 不需要强推，风险更低。
- 合并历史能清楚显示这次是“同步最新主线并解决冲突”。

不建议：

- 不建议直接点 GitHub 网页合并。
- 不建议强推。
- 不建议为了快速合并而删除 Survivor 或回退主线改动。
- 不建议把局域网发现重新塞回 `NetworkManager`。

## 5. 阶段 1：同步前安全检查

目标：确保后续操作有可回退点。

操作：

```powershell
git fetch origin --prune
git status --short --branch
git rev-list --left-right --count origin/main...HEAD
git log --oneline --decorate -6
```

建议创建本地备份分支：

```powershell
git branch backup/refactor-network-internals-before-main-sync-20260706
```

验收标准：

- 当前仍在 `refactor/network-internals`。
- `HEAD` 与 `origin/refactor/network-internals` 一致。
- 工作区只剩 `AGENTS.md` 未跟踪。
- 备份分支创建成功。

## 6. 阶段 2：合入最新 main 并解决冲突

目标：让当前分支基于最新 `origin/main`。

操作：

```powershell
git merge origin/main
```

预计会出现冲突。冲突处理原则如下。

### 6.1 `CMakeLists.txt`

处理原则：

- 保留 `main` 新增的 Survivor 文件。
- 保留当前分支新增的网络辅助模块。
- Android 权限配置继续保留。
- 服务端构建继续保留。

合并后 `appLanBoard` 应同时包含：

```text
src/network/linejsonprotocol.cpp
src/network/networkaddressutils.cpp
src/network/networkmanager.cpp
src/network/roomdiscoveryservice.cpp
src/game/survivorcontroller.cpp
src/game/survivorrenderitem.cpp
```

QML 模块应继续包含：

```text
qml/pages/SurvivorPage.qml
```

SOURCES 应同时包含：

```text
src/network/linejsonprotocol.h
src/network/networkaddressutils.h
src/network/networkmanager.h
src/network/roomdiscoveryservice.h
src/game/survivorcontroller.h
src/game/survivorrenderitem.h
```

### 6.2 `src/network/networkmanager.h`

处理原则：

- 保留当前分支的 `RoomDiscoveryService *m_roomDiscovery`。
- 不恢复 `NetworkManager` 内部的 UDP socket、发现定时器、发现房间结构体。
- 保留 `main` 引入的公共游戏定义头文件：

```cpp
#include "src/common/types.h"
```

- 移除因主线旧实现带来的重复发现相关成员：

```text
QUdpSocket *m_discoverySocket
QTimer m_discoveryTimer
QTimer m_discoveryPruneTimer
QTimer m_hostAnnouncementTimer
DiscoveredRoom
onDiscoveryReadyRead()
pruneDiscoveredRooms()
broadcastDiscoveryQuery()
broadcastHostedRoomAnnouncement()
ensureDiscoverySocket()
sendRoomAnnouncement()
broadcastRoomAnnouncement()
upsertDiscoveredRoom()
discoveredRoomToVariant()
rebuildDiscoveredRooms()
acquireMulticastLock()
releaseMulticastLock()
```

说明：

- Android 组播锁能力不应丢失，但应保留在 `RoomDiscoveryService` 中，而不是回到 `NetworkManager`。

### 6.3 `src/network/networkmanager.cpp`

处理原则：

- 保留 `LineJsonProtocol::encode()` 和 `LineJsonProtocol::takeMessages()`。
- 保留 `RoomDiscoveryService` 的启动、停止、刷新、清空、发布房间信息更新。
- 使用 `LanBoard::normalizeGameId()`、`LanBoard::gameName()` 替代当前分支中的本地 `normalizedGameId()` 和 `defaultGameName()`。
- 保留在线房间列表标准化逻辑。
- 保留 `sendSwitchRoomGame()` 中对游戏 ID 的标准化。
- 删除合并冲突中重复出现的旧 UDP 发现实现。

重点确认：

```cpp
setDiscoveryRoomInfo()
updateDiscoveryIdentity()
applyOnlineRooms()
connectClientSocket()
sendJson()
onReadyRead()
onOnlineLobbyReadyRead()
```

这些函数合并后必须仍然可用。

### 6.4 `src/network/roomdiscoveryservice.*`

处理原则：

- 保留当前分支拆出的服务。
- 接入 `LanBoard::normalizeGameId()` 和 `LanBoard::gameName()`。
- 如果需要支持 Survivor 房间发现，应让 `survivor` 不再被归一化回 `gomoku`。
- 保留 Android 组播锁处理。
- 保留 `roomUid` 去重、自发布过滤、过期清理和排序逻辑。

重点修改：

- 删除本文件内部旧的三游戏 `normalizedGameId()` / `defaultGameName()`。
- 改用 `src/common/types.h` 中的公共游戏定义。

### 6.5 `src/app/appcontroller.cpp`

处理原则：

- 以 `main` 的统一游戏流程为基础。
- 保留 Survivor 相关逻辑。
- 重新应用当前分支修复的最后一步同步顺序。

关键点：

- `main` 中已有 `finishCurrentGameSession(int winner, bool resetOfflineRoom)`。
- 当前分支修复方式是把房主侧 `broadcastGameOver()` 和 `broadcastCurrentRoomState()` 放到 `QTimer::singleShot(0, ...)` 中。
- 整合后应把这个修复放进 `finishCurrentGameSession()`，而不是恢复旧的两个分散 lambda。

建议目标形态：

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

还要确认：

- 文件包含 `#include <QTimer>`。
- 离线模式结束逻辑不被破坏。
- Survivor 相关 reset/start/navigate 逻辑不被删除。

### 6.6 `README.md` 和中文文档

处理原则：

- 文档改成当前真实状态：五子棋、斗地主、飞行棋、Survivor。
- 网络层说明保留拆分后的结构。
- 不把旧的“三款游戏”描述继续作为当前状态。
- 测试记录中要明确：此前测试覆盖的是三款联网桌游，不覆盖 Survivor 实时联机。

## 7. 阶段 3：冲突解决后的基础校验

目标：确认代码至少能静态通过 Git 和构建。

操作：

```powershell
git status --short --branch
git diff --check
cmake --preset qt-mingw-desktop
cmake --build --preset qt-mingw-desktop
```

如果 preset 构建慢或已有专用构建目录，也可以先执行：

```powershell
cmake --build build\codex-branch-check
```

验收标准：

- 没有冲突标记。
- `git diff --check` 无错误。
- 桌面端构建通过。
- `appLanBoard.exe` 和 `lanboardServer.exe` 均能生成或保持可用。

## 8. 阶段 4：专项自动测试

目标：确认同步主线后，当前分支的核心价值没有被破坏。

优先级从高到低：

1. 构建测试。
2. `RoomDiscoveryService` 服务级测试。
3. `AppController` 控制层联机测试。
4. 本地 `lanboardServer` 在线房间测试。

需要根据合并后的接口更新临时测试工程，尤其是：

- `src/common/types.h`
- Survivor controller 相关依赖。
- `RoomDiscoveryService` 对公共游戏定义的依赖。

建议执行：

```powershell
cmake --build build\codex-branch-check
```

然后重新运行或重建以下临时测试：

```powershell
build\codex-lan-discovery-check\build\lanDiscoveryCheck.exe
build\codex-appcontroller-e2e\build\appControllerE2E.exe
powershell -NoProfile -ExecutionPolicy Bypass -File build\codex-online-server-e2e\online-server-e2e.ps1
```

如果旧临时测试因为主线结构变化无法直接运行，应更新临时测试后再执行。

验收标准：

- 局域网发现仍能发现、更新、去重和过期清理房间。
- 五子棋最后一步和 `game_over` 顺序仍正确。
- 飞行棋最后一步和 `game_over` 顺序仍正确。
- 斗地主私有状态仍不会被错误广播。
- 本地在线房间 `create_room`、`join_room`、`room_state`、`start_game`、`move` 正常。

## 9. 阶段 5：GUI 回归测试

目标：确认真实页面流程没有被合并破坏。

最低测试范围：

- 五子棋双实例：
  - 发现房间。
  - 加入房间。
  - 准备并开始。
  - 双方落子同步。
  - 胜负结束后返回房间。

- 飞行棋双实例：
  - 发现房间。
  - 加入房间。
  - 准备并开始。
  - 掷骰和移动同步。
  - 结束或返回房间状态正常。

- 斗地主三实例：
  - 两个客户端加入房间。
  - 三人准备。
  - 开始游戏。
  - 出牌和不出同步。

- Survivor 基础检查：
  - 首页能看到 Survivor 入口。
  - 能进入 Survivor 页面。
  - 本地试玩不崩溃。
  - 房间游戏切换到 Survivor 后，人数上限和房间显示不异常。

说明：

- Survivor 实时联机如果主线本身还未完成，不应强行作为本分支合并阻塞项。
- 但不能因为网络重构导致 Survivor 入口、构建或基础页面损坏。

## 10. 阶段 6：文档更新

目标：让文档反映“已经同步最新 main 后的状态”。

需要更新：

- `docs/pre-main-merge-workplan.md`
  - 标记旧计划基于 `2c1a3ba`，已经被本计划补充。
  - 增加“最新主线已前进，需要先完成 main sync”的说明。

- `docs/pre-main-merge-test-results.md`
  - 增加同步最新 `main` 后的复测结果。
  - 明确原测试覆盖三款联网桌游，不覆盖 Survivor 实时联机。

- `docs/network-refactor-work-report.md`
  - 更新当前主线基线为 `e3526f9` 或同步后的 merge 提交。
  - 说明网络拆分已与 `unify-game-runtime` 整合。

可以新增：

```text
docs/main-sync-test-results.md
```

如果复测记录较多，建议新增该文件，避免把旧测试结果文档改得过长。

## 11. 阶段 7：提交策略

推荐提交方式：

1. 先完成 `git merge origin/main`，解决冲突。
2. 构建和关键测试通过。
3. 提交 merge commit。
4. 如果测试文档是在 merge commit 后补充的，可以再做一个文档提交。

推荐提交信息：

```text
merge: sync main into network internals
```

如果单独提交测试记录：

```text
docs: record main sync validation results
```

注意：

- 不提交 `AGENTS.md`。
- 不提交 APK、exe、build 目录或临时测试脚本。
- 不强推，除非团队明确要求改用 rebase。

## 12. 阶段 8：更新合并请求

目标：让组长和组员知道这个分支已经适配最新主线。

PR 描述需要补充：

- 已同步最新 `main`：`e3526f9`
- 已解决与 `unify-game-runtime` 的冲突。
- 保留 Survivor 入口和主线公共游戏定义。
- 保留网络层拆分。
- 保留最后一步同步修复。
- 最新构建和测试结果。

建议补充说明：

```text
本分支已同步最新 main，并解决与统一游戏入口和 Survivor 相关改动的冲突。
网络层拆分继续保留，NetworkManager 不再直接承担局域网发现细节。
桌面构建、局域网发现、本地多实例联机和本地服务端在线房间流程已重新验证。
```

## 13. 合并前最终检查清单

满足以下条件后，才建议请求合并：

- 当前分支包含最新 `origin/main`。
- GitHub PR 不再显示冲突。
- `git status --short --branch` 只剩预期未跟踪文件，或完全干净。
- `git diff --check` 通过。
- 桌面端构建通过。
- 五子棋、飞行棋、斗地主核心联机流程通过。
- 本地在线房间最小流程通过。
- Survivor 基础入口没有被破坏。
- 未覆盖项已写入文档：
  - Android 真机。
  - 跨设备同 Wi-Fi UDP。
  - 远端 ECS。
  - Survivor 实时联机，如果主线尚未完成。

## 14. 当前建议结论

当前分支不应废弃，也不应直接合并。

最合理的下一步是：

1. 在当前分支创建备份点。
2. merge 最新 `origin/main`。
3. 手动解决冲突。
4. 构建。
5. 重新跑关键测试。
6. 更新测试记录。
7. 推送当前分支。
8. 更新或重新发起合并请求。

这条路线能最大限度保留你已经完成的网络重构成果，同时不覆盖组员已经合入 `main` 的新功能。
