# 并入 main 前工作计划

计划日期：2026-07-05

## 当前状态

- 当前分支：`refactor/network-internals`
- 远端分支：`origin/refactor/network-internals`
- 当前提交：`25ad4aa docs: add network refactor work report`
- 主线基线：`2c1a3ba Add networked flight chess and 8-player rooms`
- 当前本地未跟踪文件：`AGENTS.md`
- 本计划保存后会新增未跟踪文件 `docs/pre-main-merge-workplan.md`；如需提交，只暂存本计划文件，继续不提交 `AGENTS.md`。

当前分支相对 `origin/main` 的改动集中在网络层内部拆分和文档：

- `LineJsonProtocol`
- `NetworkAddressUtils`
- `RoomDiscoveryService`
- `docs/network-refactor-workplan.md`
- `docs/network-messages.md`
- `docs/network-refactor-work-report.md`

## 工作目标

在并入 `main` 前，把当前分支从“构建通过”推进到“行为可验收、文档一致、PR 可审阅”。

本阶段不继续增加新功能，重点是：

- 修正文档和配置不一致。
- 完成桌面双实例联机冒烟测试。
- 确认局域网发现拆分没有行为回退。
- 准备清晰的 PR 说明和剩余风险。

## 不做事项

- 不直接推送到 `main`。
- 不继续扩大网络重构范围。
- 不在同一批提交里混入 UI 重构或游戏规则重构。
- 不提交构建产物、缓存、临时文件或 `AGENTS.md`。
- 不强推远端分支，除非团队明确同意。

## 阶段 1：同步和工作区确认

目标：确保后续工作基于最新远端状态。

操作：

```powershell
git fetch origin
git status --short --branch
git log --oneline --decorate -6
git diff --name-status origin/main...HEAD
```

验收标准：

- 当前分支仍为 `refactor/network-internals`。
- `HEAD` 与 `origin/refactor/network-internals` 一致。
- 工作区只存在预期未跟踪文件。
- 如果 `origin/main` 有新提交，先合入或 rebase 主线后再继续。

## 阶段 2：修正文档和配置一致性

目标：让项目文档准确反映当前代码状态。

建议修改：

- `docs/network-refactor-work-report.md`
  - 更新当前提交为 `25ad4aa`。
  - 移除“报告仍未跟踪”的旧状态描述。
- `README.md`
  - 将“当前已落地两款游戏”修正为当前实际的五子棋、斗地主、飞行棋。
  - 项目结构中补充 `FlightChessPage.qml`。
  - 开发说明中把网络层描述更新为已拆分后的 `NetworkManager` + 辅助模块。
- `任务分工.md`
  - 从“五子棋 MVP”现状说明更新为当前三游戏、8 人房间、在线房间状态。
  - 明确网络层当前负责连接、消息分发和房间发现，不负责房间规则或游戏规则。
- `CMakePresets.json`
  - 当前指向 Qt `6.11.1`，而项目文档和本地验证使用 Qt `6.10.3`。
  - 建议统一为 `C:/Qt/6.10.3/mingw_64` 和对应工具链，或在文档中明确该 preset 是个人本地配置。

验收标准：

```powershell
git diff --check
cmake --build build\codex-branch-check
```

建议提交：

```text
docs: align project status before main merge
```

## 阶段 3：构建和环境复核

目标：确认文档修正后仍能构建，并记录非阻塞环境提示。

操作：

```powershell
cmake --build build\codex-branch-check
```

如果需要验证统一桌面环境，再执行：

```powershell
cmake -S . -B build-qt6103 -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\mingw_64"
cmake --build build-qt6103 --parallel 8
```

验收标准：

- 桌面端构建通过。
- 若出现 `WrapVulkanHeaders` 缺失或 Qt `QTP0004` dev warning，记录为非阻塞提示。
- 若出现链接失败且提示 `appLanBoard.exe` 被占用，先关闭正在运行的程序后重试。

## 阶段 4：局域网发现专项测试

目标：验证 `RoomDiscoveryService` 拆分后行为没有退化。

测试准备：

- 打开两个桌面实例，记为 A 和 B。
- A 作为房主，B 作为客户端。
- 两个实例使用不同昵称。
- 确认没有防火墙拦截 UDP 广播和 TCP 房间端口。

测试项：

1. A 创建五子棋房间。
2. B 进入局域网页，自动发现 A 的房间。
3. B 手动刷新后仍能看到同一房间，且不重复。
4. A 切换游戏类型后，B 看到的 `gameId`、`gameName`、人数和容量更新。
5. A 开始游戏后，B 看到房间状态变为游戏中，且不可加入。
6. A 断开或离开后，B 的房间项在过期时间后消失。
7. A 自己的发现列表不显示自己的房间。

重点观察：

- 多网卡环境下 `hostIp` 是否是 B 可连接的 IPv4。
- 自发布房间过滤是否可靠。
- 房间状态更新是否及时。

如发现自己房间仍出现在列表中，优先修复：

- 在 `RoomDiscoveryService` 中保存本机 `roomUid`。
- 收到 `room_announce` 时优先按本机 `roomUid` 过滤，再按 IP + port 过滤。

## 阶段 5：游戏联机冒烟测试

目标：验证本轮网络内部拆分没有破坏现有三款游戏的最小联网闭环。

### 五子棋

流程：

1. A 创建五子棋房间。
2. B 发现并加入。
3. 双方准备。
4. A 开始游戏。
5. A 和 B 交替落子。
6. 触发胜负或认输。
7. 双方返回房间，准备状态清空。

验收标准：

- 落子同步正确。
- 胜负同步正确。
- 返回房间后房间状态一致。

### 飞行棋

流程：

1. A 创建飞行棋房间。
2. B 加入并准备。
3. A 开始游戏。
4. 当前玩家掷骰。
5. 当前玩家移动飞机。
6. B 的掷骰和移动能同步给 A。
7. 对局结束或断开时状态正常。

验收标准：

- `flight_roll` / `flight_roll_result` 正常。
- `flight_move` / `flight_move_result` 正常。
- 断开 active 玩家时能结束或回收状态。

### 斗地主

流程：

1. A 创建斗地主房间。
2. 两个客户端加入，凑齐 3 个 active 玩家。
3. 全部准备。
4. A 开始游戏。
5. 客户端出牌和过牌。
6. A 分别向玩家发送私有 `ddz_state`。
7. 对局结束后返回房间并清空准备状态。

验收标准：

- 出牌、过牌请求被主机处理。
- 各客户端看到自己的手牌视角。
- `ddz_state` 不被错误地全员同包广播。

## 阶段 6：在线房间最小测试

目标：确认在线房间相关消息仍可用。

前提：

- ECS 或本地 `lanboardServer` 可运行。
- 端口已放行。

测试项：

1. 启动服务端。
2. 客户端请求在线房间列表。
3. 创建在线房间。
4. 第二个客户端加入在线房间。
5. 准备、开始游戏。
6. 验证至少一款游戏的基础同步。

验收标准：

- `list_rooms`、`rooms_list` 正常。
- `create_room`、`join_room` 正常。
- `room_state` 正常。
- 错误消息能显示到客户端。

## 阶段 7：问题修复策略

如果测试失败，按以下优先级修复：

1. 构建失败优先于运行问题。
2. 局域网发现失败优先于游戏内同步问题。
3. 破坏现有 public API 的问题必须回退或调整。
4. 只修当前分支引入的问题，不顺手做大范围重构。

修复后必须执行：

```powershell
git diff --check
cmake --build build\codex-branch-check
```

并重新跑对应失败用例。

## 阶段 8：PR 前最终检查

操作：

```powershell
git fetch origin
git status --short --branch
git log --oneline origin/main..HEAD
git diff --stat origin/main...HEAD
git diff --check origin/main...HEAD
cmake --build build\codex-branch-check
```

验收标准：

- 当前分支与远端分支一致。
- 未跟踪文件只剩明确不提交的本地文件。
- 没有合并冲突标记。
- 构建通过。
- 手动测试结果已记录。

## PR 内容建议

标题：

```text
Refactor network internals
```

正文应包含：

- 基线提交：`2c1a3ba`
- 当前分支提交范围：`c286d59..HEAD`
- 本轮目标：拆分网络内部职责，不改变 QML 主要调用 API。
- 主要改动：
  - `LineJsonProtocol`
  - `NetworkAddressUtils`
  - `RoomDiscoveryService`
  - 网络消息表和工作报告
- 构建结果。
- 手动测试结果。
- 已知风险和未覆盖项。

## 合并条件

满足以下条件后再并入 `main`：

- 文档一致性问题已修正。
- 桌面端构建通过。
- 局域网发现专项测试通过。
- 五子棋双实例闭环通过。
- 飞行棋和斗地主核心联网流程通过，或已明确记录未覆盖风险并获得团队认可。
- `origin/main` 最新提交已同步到当前分支。
- PR 经组员审阅通过。
