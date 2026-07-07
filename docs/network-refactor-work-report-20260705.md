# 网络重构工作报告

报告日期：2026-07-05

文档性质：历史工作报告。本文件记录网络拆分完成时的状态；后续同步主线和验证记录见 `docs/main-sync-workplan-20260706.md`、`docs/main-sync-test-results-20260706.md` 和 `docs/main-sync-workplan-20260707.md`。

## 结论

当前分支 `refactor/network-internals` 有效。分支基于 `origin/main` 的 `2c1a3ba Add networked flight chess and 8-player rooms`。代码重构完成于 `492d51a refactor: extract LAN room discovery service`，后续文档提交不改变本报告对网络拆分有效性的判断。最新提交以 `git log -1 --oneline` 为准。

本轮工作没有改变 `NetworkManager` 暴露给 QML 和 `AppController` 的主要 public API，而是将网络层内部职责拆为三个更小模块：

- `LineJsonProtocol`：换行分隔 JSON 编解码。
- `NetworkAddressUtils`：本机 IPv4 选择和面向对端的地址选择。
- `RoomDiscoveryService`：局域网 UDP 房间发现、发布、缓存、去重、排序和过期清理。

桌面端构建已通过，分支已推送到远端。`AGENTS.md` 是本地未跟踪文件，未纳入提交。

## 检查范围

本次检查覆盖仓库内非构建产物文件：

- Git 跟踪文件：51 个。
- 本地非构建文件：52 个，其中额外 1 个为未跟踪的 `AGENTS.md`。
- 构建目录、缓存、二进制产物按 `.gitignore` 约定排除。

重点检查文件：

- 构建配置：`CMakeLists.txt`、`CMakePresets.json`、`.gitignore`。
- 项目文档：`README.md`、`任务分工.md`、`Git协作流程.md`、`Qt安装流程.md`、`docs/network-refactor-workplan-20260705.md`、`docs/network-messages.md`。
- QML 入口和房间页：`qml/Main.qml`、`qml/pages/RoomPage.qml`。
- 应用协调层：`src/app/appcontroller.*`。
- 房间逻辑：`src/lobby/roommanager.*`。
- 网络层：`src/network/*`。
- 游戏逻辑：`src/game/*`。
- 在线服务端：`src/server/*`。

执行过的检查：

```powershell
git status --short --branch
git log --oneline --decorate --graph -10
git diff --name-status origin/main...HEAD
git diff --stat origin/main...HEAD
git diff --check origin/main...HEAD
rg -n "<<<<<<<|=======|>>>>>>>|TODO|FIXME|XXX|HACK" .
cmake --build build\codex-branch-check
```

检查结果：

- 未发现合并冲突标记。
- `git diff --check origin/main...HEAD` 通过。
- 文本扫描只命中工作计划中的说明文字 `sendXXX()`，不是遗留 TODO。
- `cmake --build build\codex-branch-check` 通过。

## 提交回顾

### `c286d59 docs: add network refactor work plan`

新增：

- `docs/network-refactor-workplan-20260705.md`
- `docs/network-messages.md`

作用：

- 明确不直接合并旧网络重构分支。
- 明确本轮以最新主线为基线，只迁移可控的内部拆分。
- 记录当前网络消息 type、字段、发送方和接收方。

### `04d74f2 refactor: extract line-delimited JSON protocol`

新增：

- `src/network/linejsonprotocol.h`
- `src/network/linejsonprotocol.cpp`

修改：

- `NetworkManager::sendJson()` 改为使用 `LineJsonProtocol::encode()`。
- `NetworkManager::onReadyRead()` 和在线大厅读取复用 `LineJsonProtocol::takeMessages()`。

效果：

- JSON 换行帧处理从 `NetworkManager` 中抽离。
- 保留 per-socket `readBuffer`，没有改变消息格式。

### `061640c refactor: extract network address selection`

新增：

- `src/network/networkaddressutils.h`
- `src/network/networkaddressutils.cpp`

修改：

- `NetworkManager::localIp()` 使用 `NetworkAddressUtils::bestLocalIpv4()`。
- 原本面向对端选择本地 IPv4 的逻辑迁移到 `NetworkAddressUtils::localIpv4ForPeer()`。

效果：

- IP 选择逻辑集中维护。
- 保留私有网段优先、Wi-Fi 优先、VPN/虚拟网卡降权、过滤 localhost 和 `169.254.*` 的策略。

### `492d51a refactor: extract LAN room discovery service`

新增：

- `src/network/roomdiscoveryservice.h`
- `src/network/roomdiscoveryservice.cpp`

修改：

- `src/network/networkmanager.h`
- `src/network/networkmanager.cpp`
- `CMakeLists.txt`

效果：

- UDP socket、房间扫描、主机发布、查询响应、缓存去重、排序和过期清理迁移到 `RoomDiscoveryService`。
- `NetworkManager` 继续保留 `startRoomDiscovery()`、`stopRoomDiscovery()`、`refreshRoomDiscovery()`、`clearDiscoveredRooms()` 和 `discoveredRooms` 属性。
- 主机创建房间后会通过 `updateDiscoveryIdentity()` 同步发布信息。
- 房间人数、游戏类型、容量、游戏中状态变化后会同步更新到发现服务。

## 模块检查结论

### 文档

已新增网络重构计划和消息表，能支撑组员审阅当前网络协议。

本报告初次检查时发现过以下文档一致性问题，后续应以当前文件内容为准复核：

- `README.md` 需要准确反映五子棋、斗地主、飞行棋三款游戏。
- `任务分工.md` 需要反映当前三游戏、在线房间和 8 人房间状态。
- `CMakePresets.json` 需要与 `README.md`、`Qt安装流程.md` 和当前可用 Qt `6.10.3` 环境保持一致。

### 构建配置

`CMakeLists.txt` 已包含本轮新增的三个网络模块源文件和头文件：

- `linejsonprotocol`
- `networkaddressutils`
- `roomdiscoveryservice`

桌面构建通过。之前完整构建时出现过非阻塞提示：

- `WrapVulkanHeaders` 未找到。
- Qt QML policy `QTP0004` 开发警告。

这两个提示不影响当前桌面端编译，但可以在后续环境整理时统一处理。

### QML

`RoomPage.qml` 继续通过 `AppCtrl.networkManager.discoveredRooms`、`startRoomDiscovery()`、`refreshRoomDiscovery()` 使用局域网发现能力。由于 `NetworkManager` 对外属性和方法保持稳定，QML 不需要配合本轮拆分改动。

当前房间页仍然是体量最大的 UI 文件，后续若继续做 UI 或交互优化，应优先考虑拆分房间页内部区域。

### 应用协调层

`AppController` 仍是 QML、房间、网络、游戏控制器之间的总协调层。

本轮拆分后，`AppController` 不需要直接了解 UDP 发现细节，只继续调用：

- `setDiscoveryHostName()`
- `setDiscoveryGameInProgress()`
- `setDiscoveryRoomInfo()`

这符合计划中“网络层内部拆分、外部调用不变”的原则。

### 房间逻辑

`RoomManager` 保持负责玩家列表、准备状态、座位状态、游戏类型和开始条件。

当前房间容量为 8，具体游戏参与人数由 `maxPlayers()` 控制：

- 斗地主：3 人。
- 五子棋和飞行棋：2 人。

这与本轮发现服务发布的 `roomCapacity` 和 `maxPlayers` 字段匹配。

### 网络层

当前网络层职责已经更清晰：

- `NetworkManager`：TCP 连接、消息发送、消息分发、对上层暴露网络门面。
- `LineJsonProtocol`：TCP 上的换行 JSON 帧处理。
- `NetworkAddressUtils`：本机地址选择。
- `RoomDiscoveryService`：局域网 UDP 房间发现。

保留的主要消息 type 包括：

- 局域网发现：`discover_room`、`room_announce`
- 房间同步：`join`、`ready`、`change_seat`、`switch_room_game`、`room_state`
- 游戏同步：`place_piece`、`move`、`flight_roll`、`flight_roll_result`、`flight_move`、`flight_move_result`、`ddz_play`、`ddz_pass`、`ddz_state`、`surrender`、`game_over`
- 在线房间：`create_room`、`join_room`、`list_rooms`、`rooms_list`
- 通用错误：`error`

### 游戏逻辑

`src/game` 中五子棋、斗地主、飞行棋控制器没有在本轮重构中被修改。网络层仍只负责收发和分发，游戏规则仍留在对应 controller 中。

### 在线服务端

`ServerApp` 当前仍内联处理换行 JSON 解析，没有复用 `LineJsonProtocol`。这不是本轮必须项，因为本轮计划优先保持客户端网络门面稳定；但后续可以考虑让客户端和服务端共用协议解析工具，减少重复实现。

## 风险和未完成项

当前已经完成：

- 分支有效性确认。
- 三个网络内部模块拆分。
- 协议文档和工作计划文档。
- 桌面端构建验证。
- 推送到远端分支。

仍需补齐：

- 桌面双实例局域网发现实测。
- 五子棋双实例完整闭环实测。
- 飞行棋联网核心流程实测。
- 斗地主联网核心流程实测。
- 在线房间连接 ECS 的实测。
- 文档一致性修正，尤其是 README、任务分工和 CMakePresets 的版本描述。

## 下一步建议

优先级从高到低：

1. 执行桌面双实例冒烟测试，先验证局域网发现、加入房间、准备、开始、断开和房间过期消失。
2. 分别跑五子棋、飞行棋、斗地主的最小联网闭环，记录失败步骤和日志。
3. 修正 README、任务分工和 CMakePresets 中与当前代码不一致的描述。
4. 如果组员继续向 `main` 增加功能，每天开始前先 `git fetch origin`，再把 `origin/main` 合入或 rebase 到当前分支，冲突优先保留主线新功能。
5. 冒烟测试通过后发起 PR，PR 描述应包含本报告、构建结果、手动测试结果和剩余风险。

## 当前状态

```text
分支：refactor/network-internals
远端：origin/refactor/network-internals
代码重构完成提交：492d51a refactor: extract LAN room discovery service
主线基线：2c1a3ba Add networked flight chess and 8-player rooms
构建结果：通过
本地未跟踪文件：AGENTS.md
```
