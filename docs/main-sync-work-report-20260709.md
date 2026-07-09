# 同步最新 main 工作报告

报告日期：2026-07-09

文档性质：工作报告。本文件记录 `refactor/network-internals` 在 2026-07-09 同步最新 `origin/main` 的实际处理、验证结果和剩余风险。

## 1. 更新结论

本轮已完成一次本地主线同步。

- 当前分支：`refactor/network-internals`
- 同步前备份分支：`backup/refactor-network-internals-before-main-sync-20260709`
- 同步前提交：`9a27d57 fix: respect requested seat change`
- 本轮计划提交：`0b77b10 docs: add 20260709 main sync work plan`
- 同步的最新主线：`origin/main = 5849e43 docs: refresh architecture docs and rewrite dev log`
- 同步后 merge commit：`bd883da merge: sync main into network internals`

merge commit 后关系：

```text
origin/main...HEAD = 0 24
```

提交本报告后的最终关系：

```text
origin/main...HEAD = 0 25
```

说明：

- 当前本地分支已经包含最新 `origin/main`。
- 当前本地分支相对 `origin/refactor/network-internals` 领先，需要后续推送。
- 本轮没有提交本地未跟踪文件 `AGENTS.md` 和 `docs/答辩学习文档-一周冲刺.md`。

## 2. 实际更新方式

按既有文档策略执行 merge，而不是 rebase。

执行路径：

```powershell
git fetch --all --prune --tags
git branch backup/refactor-network-internals-before-main-sync-20260709
git commit -m "docs: add 20260709 main sync work plan"
git merge origin/main
```

选择 merge 的原因：

- 当前分支已经存在远端同名分支。
- 分支历史中已有多轮主线同步和验证记录。
- merge commit 能保留本轮同步上下文，避免强推风险。

## 3. 冲突和处理

本次实际冲突文件：

```text
README.md
src/app/appcontroller.cpp
```

### 3.1 README.md

处理原则：

- 保留 `origin/main` 的最新项目完成度、构建目录、ECS 部署文档、技术开发日志和架构图说明。
- 保留当前分支对网络内部拆分的说明，包括 ENet、地址选择、局域网发现和协议工具。
- 合并相关文档列表，加入 2026-07-09 合并前工作计划和本轮同步报告。
- 避免重复的 `相关文档` 段落。

合并后 README 对 Survivor 的描述保持为 MVP 状态：

- 已支持本地试玩、房间入口和在线联机战斗。
- 已接入升级、宝箱、HUD、雷达、结算与实时同步。
- 多人同步、性能、节奏、表现和更完整权威化方案仍在继续打磨。

### 3.2 src/app/appcontroller.cpp

处理原则：

- 远端座位切换使用 `RoomManager::tryChangeSeat()` 作为唯一入口。
- 不再重复调用 `setPlayerSeatById()`，避免合法座位切换被二次设置误判为无变化。
- Host 游戏结束时保留当前分支的 `QTimer::singleShot(0, ...)` 延后一拍广播，确保最后一次状态先被发送。
- Host 游戏结束收尾使用主线的 `RoomManager::concludeGame()`，同时清理 `gameInProgress` 和 active 玩家准备状态。

最终合并语义：

```text
onRemoteSeatChanged()
  -> tryChangeSeat()
  -> syncActiveGuestPlayerId()
  -> broadcastCurrentRoomState()

finishCurrentGameSession()
  -> setDiscoveryGameInProgress(false)
  -> next event loop tick
  -> broadcastGameOver()
  -> concludeGame()
  -> broadcastCurrentRoomState()
```

### 3.3 CMakeLists.txt

`CMakeLists.txt` 自动合并成功，已人工复核：

- 保留当前分支新增网络模块：
  - `linejsonprotocol`
  - `networkaddressutils`
  - `roomdiscoveryservice`
- 保留主线 Survivor 和 server 文件。
- `appLanBoard` 和 `lanboardServer` 两个目标均继续存在。

## 4. 合并后补充修正

本轮没有新增源码修复，只做了冲突语义合并和文档整合。

格式修正：

- 主线新增的 `技术开发日志.md` 存在若干行尾空格。
- 已做机械去尾空格处理，避免 `git diff --check --cached` 失败。

未跟踪文件处理：

- `AGENTS.md` 本轮继续保留为未跟踪文件。
- `docs/答辩学习文档-一周冲刺.md` 本轮继续保留为未跟踪文件。
- 二者不混入本次主线同步提交。

## 5. 验证结果

基础检查：

```powershell
rg -n "^<<<<<<<|^=======|^>>>>>>>" .
git diff --check --cached
```

结果：通过。

说明：

- 没有真实冲突标记残留。
- staged diff 没有行尾空格错误。

桌面配置：

```powershell
cmake --preset qt-mingw-desktop
```

结果：通过。

非阻塞提示：

```text
Could NOT find WrapVulkanHeaders (missing: Vulkan_INCLUDE_DIR)
```

桌面构建：

```powershell
cmake --build --preset qt-mingw-desktop
```

结果：通过。

产物：

```text
build-qt-ascii/appLanBoard.exe
build-qt-ascii/lanboardServer.exe
```

局域网发现服务级测试：

```powershell
build\codex-lan-discovery-check\build\lanDiscoveryCheck.exe
```

结果：通过，退出码为 0。该临时测试无 stdout 输出。

控制层 E2E：

```powershell
build\codex-appcontroller-e2e\build\appControllerE2E.exe
```

结果：通过。

覆盖：

- 五子棋创建房间、加入、准备、开始、落子同步。
- 飞行棋创建房间、加入、准备、开始、掷骰、移动、客户端请求。
- 斗地主三人加入、准备、开始和私有手牌状态。
- 斗地主房主退出后客户端清理。
- 座位切换请求按请求者生效：`seat_change_respects_requester`。

本地在线服务端 ENet E2E：

```powershell
build\codex-online-server-e2e\build\onlineServerEnetE2E.exe
```

结果：通过。

输出：

```text
PASS online server ENet E2E
```

## 6. 未覆盖项

以下内容本轮没有完成，需要后续人工或外部环境验证：

- 桌面 GUI 多实例人工复查。
- Android 真机安装和运行。
- 跨设备同 Wi-Fi UDP 广播发现。
- 真实 ECS 公网在线房间流程。
- Survivor 多人实时战斗长时间同步稳定性。

## 7. 当前建议

建议下一步：

1. 审阅本报告和本轮冲突解决。
2. 决定是否提交 `AGENTS.md` 和 `docs/答辩学习文档-一周冲刺.md`。
3. 推送 `refactor/network-internals`。
4. 更新 PR，说明当前分支已包含 `origin/main = 5849e43`，并列出本轮自动验证结果和未覆盖项。

当前判断：

- 当前本地分支已重新具备相对最新主线的合并基础。
- 网络拆分成果仍保留。
- 主线最新 Survivor、房间权威、HUD、ECS 文档和架构文档更新已吸收。
- 自动验证通过，但仍需要 GUI、Android、跨设备和真实 ECS 验证。
