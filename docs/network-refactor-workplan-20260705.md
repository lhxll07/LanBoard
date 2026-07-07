# 网络重构工作计划

计划日期：2026-07-05

文档性质：历史阶段计划。本文件记录网络内部拆分启动时的方案；执行结果见 `docs/network-refactor-work-report-20260705.md`，后续并入前验证见 `docs/pre-main-merge-workplan-20260705.md` 和 `docs/pre-main-merge-test-results-20260705.md`。

## 当前基线

- 云端基线：`origin/main`
- 当前基线提交：`2c1a3ba Add networked flight chess and 8-player rooms`
- 工作分支建议：`refactor/network-internals`
- 旧网络重构分支：`origin/feat/network-refactor`
- 旧分支用途：只作为实现参考，不直接合并，不整体 cherry-pick。

## 当前判断

`origin/main` 已经包含五子棋、斗地主、飞行棋、在线房间、8 人房间和座位切换等联网功能。旧网络重构分支只覆盖较早的五子棋和局域网发现版本，直接合并会在 `src/network/networkmanager.cpp` 和 `src/network/networkmanager.h` 产生冲突，并可能丢失主线新增接口。

因此，本轮工作采用保守移植路线：保持 `NetworkManager` 对外接口稳定，只拆分内部职责。

## 不采用的方案

- 不直接 merge `origin/feat/network-refactor`。
- 不基于旧分支继续堆提交。
- 不一次性重写整个网络协议系统。
- 不在本轮大改 QML、`AppController` 或 `ServerApp` 的调用方式。
- 不把房间规则或游戏规则放进网络层。

## 总目标

在最新 `origin/main` 上完成网络内部拆分，并保证主线现有联网功能不丢失、桌面端可构建、核心联机流程可验证。

需要拆出的三个内部模块：

- `LineJsonProtocol`：换行分隔 JSON 编解码。
- `NetworkAddressUtils`：本机 IPv4 选择和面向对端的地址选择。
- `RoomDiscoveryService`：局域网 UDP 房间发现、房间缓存、过期清理和排序。

## 边界原则

- `NetworkManager` 继续作为 QML、`AppController` 和 `ServerApp` 之外的网络门面。
- 第一阶段保留 `NetworkManager` 的 public API、signals、slots 和 QML 属性。
- `src/network` 只负责连接、收发、解析、发现和分发消息。
- 房间容量、准备状态、座位规则留在 `src/lobby` 和 `src/app`。
- 五子棋、斗地主、飞行棋规则留在 `src/game`。
- 在线房间服务器逻辑留在 `src/server`。

## 阶段 1：建立工作分支和 API 清单

预计时间：15 到 30 分钟

操作：

```powershell
git fetch origin
git switch -c refactor/network-internals origin/main
```

检查点：

- 当前分支基于 `origin/main`。
- `src/network/networkmanager.h` 中现有 QML 属性、`Q_INVOKABLE`、signals 和 slots 先记录，不在第一阶段改名或删除。
- 当前已有未跟踪文件先确认来源，避免误提交。

验收标准：

- `git status --short --branch` 显示位于 `refactor/network-internals`。
- 没有引入代码改动。

## 阶段 2：拆出 LineJsonProtocol

预计时间：0.5 天

新增文件：

- `src/network/linejsonprotocol.h`
- `src/network/linejsonprotocol.cpp`

修改文件：

- `src/network/networkmanager.cpp`
- `CMakeLists.txt`

实现要求：

- `sendJson()` 使用 `LineJsonProtocol::encode()`。
- `onReadyRead()` 使用 `LineJsonProtocol::takeMessages()`。
- `onOnlineLobbyReadyRead()` 尽量复用同一解析逻辑。
- 保留 per-socket `readBuffer`。
- 不改变任何消息 type。
- 不改变任何对外 API。

重点保留消息：

- `ready`
- `place_piece`
- `flight_roll`
- `flight_move`
- `ddz_play`
- `ddz_pass`
- `room_state`
- `rooms_list`
- `game_start`
- `error`

验收标准：

```powershell
cmake --build build\codex-branch-check
```

- 构建通过。
- `NetworkManager` 对外接口无变化。
- `processMessage()` 中主线已有消息分支没有丢失。

建议提交：

```text
refactor: extract line-delimited JSON protocol
```

## 阶段 3：拆出 NetworkAddressUtils

预计时间：0.5 天

新增文件：

- `src/network/networkaddressutils.h`
- `src/network/networkaddressutils.cpp`

修改文件：

- `src/network/networkmanager.cpp`
- `CMakeLists.txt`

实现要求：

- `NetworkManager::localIp()` 调用 `NetworkAddressUtils::bestLocalIpv4()`。
- 原 `localIpForPeer()` 逻辑迁移为 `NetworkAddressUtils::localIpv4ForPeer()`。
- 保留私有网段优先、Wi-Fi 优先、VPN/虚拟网卡降权逻辑。
- 保留 `169.254.*` 和 localhost 过滤。
- 不改变 `room_announce` 字段。

验收标准：

- 构建通过。
- `hostIp` 仍由当前有效网卡地址生成。
- `NetworkManager` 中没有重复维护 IP 评分逻辑。

建议提交：

```text
refactor: extract network address selection
```

## 阶段 4：拆出 RoomDiscoveryService

预计时间：1 天

新增文件：

- `src/network/roomdiscoveryservice.h`
- `src/network/roomdiscoveryservice.cpp`

修改文件：

- `src/network/networkmanager.h`
- `src/network/networkmanager.cpp`
- `CMakeLists.txt`

必须保留的 `NetworkManager` 对外接口：

```cpp
Q_INVOKABLE void startRoomDiscovery();
Q_INVOKABLE void stopRoomDiscovery();
Q_INVOKABLE void refreshRoomDiscovery();
Q_INVOKABLE void clearDiscoveredRooms();
QVariantList discoveredRooms() const;
void setDiscoveryHostName(const QString &hostName);
void setDiscoveryGameInProgress(bool inProgress);
void setDiscoveryRoomInfo(const QString &gameId, const QString &gameName,
                          int roomCapacity, int maxPlayers);
```

必须保留的发现字段：

- `roomUid`
- `hostName`
- `hostIp`
- `port`
- `playerCount`
- `roomCapacity`
- `maxPlayers`
- `gameId`
- `gameName`
- `inGame`
- `isFull`

实现要求：

- `RoomDiscoveryService` 负责 UDP socket、发现查询、房间广播、房间缓存、过期清理和排序。
- `NetworkManager` 负责把当前房间信息传给 `RoomDiscoveryService`。
- 主机创建房间后，即使没有主动打开发现列表，也要能响应其他客户端的 `discover_room`。
- 房间人数、游戏类型、游戏中状态变化后，发布信息要同步更新。
- `RoomPage.qml` 尽量不需要改动。

验收标准：

- 构建通过。
- 桌面双实例中客户端能发现主机房间。
- 发现列表不显示自己的房间。
- 斗地主、飞行棋房间发现不丢 `gameId`、`gameName`、`roomCapacity`。
- 满员和游戏中状态能在发现数据中体现。

建议提交：

```text
refactor: extract LAN room discovery service
```

## 阶段 5：协议文档和协作规则

预计时间：0.5 天

文档文件：

- `docs/network-messages.md`

维护要求：

- 新增联网功能前，先登记消息 type、字段、发送方、接收方。
- 若新增 `sendXXX()`、`broadcastXXX()`、`remoteXXX()`，必须说明用途和消息归属。
- 局域网房间消息、在线服务器消息、游戏内同步消息要区分清楚。

建议提交：

```text
docs: document network message flow
```

## 阶段 6：联机冒烟测试

预计时间：0.5 到 1 天

最低测试矩阵：

### 桌面双实例五子棋

- A 创建房间。
- B 发现房间。
- B 加入房间。
- 双方准备。
- 房主开始游戏。
- 双方落子同步。
- 胜负后返回房间。

### 桌面双实例飞行棋

- 创建飞行棋房间。
- 客户端加入。
- 房主开始游戏。
- 掷骰同步。
- 移动同步。
- 结束同步。

### 斗地主联网核心流程

- 创建斗地主房间。
- 玩家加入。
- 开始游戏。
- 出牌同步。
- 过牌同步。
- 状态同步。

### 房间发现异常流程

- 刷新房间列表。
- 房间满员。
- 游戏中房间。
- 客户端断开。
- 房间过期消失。

验收标准：

- 桌面构建通过。
- 五子棋双实例完整闭环通过。
- 飞行棋和斗地主没有明显协议缺失或崩溃。
- 房间发现不重复、不显示本机房间。
- 断开连接后房间状态能更新。

## 提交和 PR 策略

建议提交顺序：

```text
docs: add network refactor work plan
refactor: extract line-delimited JSON protocol
refactor: extract network address selection
refactor: extract LAN room discovery service
docs: document network message flow
```

PR 描述需要包含：

- 基线提交。
- 重构目标。
- 保留的对外 API。
- 构建结果。
- 手动测试结果。
- 未覆盖风险。

## 风险和应对

### 风险：组员继续修改 main

应对：

- 每天开始前同步 `origin/main`。
- 每完成一个小提交后尽快 rebase 或 merge main。
- 冲突优先保留 main 新功能，再重新套内部拆分。

### 风险：RoomDiscoveryService 拆分范围过大

应对：

- 先完成 `LineJsonProtocol` 和 `NetworkAddressUtils` 两个低风险提交。
- 如果发现服务拆分卡住，先不强行合入第三步。

### 风险：协议继续膨胀

应对：

- 所有新增消息必须补充到 `docs/network-messages.md`。
- 网络层只分发，不承接业务规则。

## 完成定义

本轮工作完成时应满足：

- 基于最新 `origin/main`。
- 主线已有联网功能不丢失。
- `LineJsonProtocol`、`NetworkAddressUtils`、`RoomDiscoveryService` 至少完成前两项，第三项在验证通过后合入。
- 桌面构建通过。
- 五子棋双实例联机闭环通过。
- 协议消息表已更新。
- PR 可以由组员按文档审阅。
