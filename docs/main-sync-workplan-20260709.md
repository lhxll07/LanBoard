# 同步最新 main 的合并前工作计划

计划日期：2026-07-09

文档性质：当前审阅计划。本文件记录 2026-07-09 检查后，`refactor/network-internals` 再次同步最新 `origin/main` 前应执行的工作。历史计划、报告和测试记录见 `docs/document-index.md`。

## 1. 当前实际情况

当前检查时间为 2026-07-09 下午，远端引用已经通过 `git fetch --all --prune --tags` 刷新。

当前分支：

- 本地仓库路径：`D:\CodeWork\LanBoard`
- 当前分支：`refactor/network-internals`
- 当前提交：`9a27d57 fix: respect requested seat change`
- 当前提交时间：`2026-07-08 12:06:16 +0800`
- 远端同名分支：`origin/refactor/network-internals`
- 本地当前分支与远端同名分支一致：ahead `0`，behind `0`

当前主线：

- 远端默认分支：`origin/main`
- 最新主线提交：`5849e43 docs: refresh architecture docs and rewrite dev log`
- 最新主线提交时间：`2026-07-09 00:52:16 +0800`
- 本地 `main` 仍停在 `22a6ff7 Merge branch 'solo-dev'`，落后 `origin/main` 28 个提交，因此本轮判断以 `origin/main` 为准，不以本地 `main` 为准。

分支关系：

```text
origin/main...HEAD = 8 22
```

含义：

- 当前分支缺少 `origin/main` 的 8 个较新提交。
- 当前分支保留 22 个当前分支独有提交。
- 当前分支本身没有过期到需要废弃，但已经不是最新主线基线。

## 2. 工作区状态

编写本计划前的检查状态为：工作区没有已跟踪文件改动，但有两个未跟踪文件：

```text
AGENTS.md
docs/答辩学习文档-一周冲刺.md
```

远端当前分支和远端主线均没有这两个文件。

本计划文件本身属于 2026-07-09 新增的审阅文档，后续应和 `docs/document-index.md`、README 相关链接一起提交。

初步判断：

- `AGENTS.md` 是仓库协作规范，内容覆盖项目结构、构建命令、测试要求、开发约束和完成标准，有纳入版本控制的价值。
- `docs/答辩学习文档-一周冲刺.md` 是面向项目答辩的学习材料，属于项目交付和答辩辅助文档，也可以纳入 `docs/`，但应单独提交，避免和主线同步提交混在一起。

本轮建议：

- 如果审阅确认这两个文件都应保留，则先单独提交文档。
- 如果暂时不确认，则先保留未跟踪状态，不在主线同步提交中处理。
- 不建议在解决主线冲突时顺手提交这两个文件。

## 3. 本轮目标

本轮目标是让 `refactor/network-internals` 重新包含最新 `origin/main`，并保留当前分支的网络内部拆分成果。

必须完成：

- 创建同步前备份分支。
- 明确处理本地未跟踪文档的策略。
- 把最新 `origin/main` 合入当前分支。
- 人工解决 `README.md` 和 `src/app/appcontroller.cpp` 冲突。
- 复核 `CMakeLists.txt` 自动合并结果。
- 确认当前分支新增网络模块仍被构建系统纳入。
- 重新配置、构建并完成最小功能验证。
- 记录验证结果，必要时新增对应测试记录或工作报告。

不做事项：

- 不直接推送到 `main`。
- 不用整文件 `ours` 或 `theirs` 覆盖冲突文件。
- 不删除当前分支的网络拆分模块来迎合主线。
- 不回退 `origin/main` 新增的 Survivor、房间权威、HUD、ECS 文档或架构文档更新。
- 不提交构建产物、临时测试输出、exe、APK、缓存目录或 IDE 配置。

## 4. 推荐同步策略

推荐继续使用 merge，把 `origin/main` 合入当前分支。

推荐原因：

- 当前分支已经存在远端同名分支。
- 历史中已经有多轮 `main` 同步记录和工作报告，merge history 更容易审查。
- 不需要强推，适合组内协作。
- 当前分支包含较多文档和验证记录，rebase 会增加审阅成本。

推荐命令：

```powershell
git fetch --all --prune --tags
git status --short --branch
git branch backup/refactor-network-internals-before-main-sync-20260709
git merge origin/main
```

不推荐：

- 不建议直接在 GitHub 网页合并。
- 不建议 rebase 后强推，除非团队明确要求线性历史。
- 不建议跳过本地构建验证后直接推送。

## 5. 已知主线新增内容

当前分支缺少的 `origin/main` 8 个提交为：

```text
5849e43 docs: refresh architecture docs and rewrite dev log
946c656 tune survivor late game pacing and performance
ba5a8a6 Tune survivor pacing and strengthen late-game pressure
f066cb9 Refactor room authority and tighten survivor sync
ae623cc docs: expand technical development timeline
8646f43 docs: add ecs deployment guide and finalize mobile joystick
c804a14 fix(survivor): preserve spectator HUD and game-over flow
09eb5d9 fix(survivor): stabilize online progression and multiplayer HUD
```

这些提交不是纯文档更新，主要影响：

- `SurvivorPage.qml`
- `AppController`
- `RoomManager`
- `ServerApp`
- Survivor 控制器、运行时、模拟、渲染和网络编解码
- README、ECS 部署文档、技术开发日志和架构图

因此本轮同步属于功能和文档共同整合，不能只按文档冲突处理。

## 6. 预计冲突范围

无工作区改动的模拟合并显示，本轮存在真实内容冲突：

```text
README.md
src/app/appcontroller.cpp
```

自动合并但需要人工复核的重点文件：

```text
CMakeLists.txt
qml/pages/SurvivorPage.qml
src/common/roomtypes.h
src/game/survivorcontroller.*
src/game/survivornetcodec.*
src/game/survivorrenderitem.cpp
src/game/survivorruntime.cpp
src/game/survivorsimulation.cpp
src/game/survivorworld.h
src/lobby/roommanager.*
src/server/serverapp.*
```

双方从共同祖先开始都有改动的文件为：

```text
CMakeLists.txt
README.md
src/app/appcontroller.cpp
```

其中 `README.md` 和 `src/app/appcontroller.cpp` 已确认会冲突，`CMakeLists.txt` 可以自动合并，但仍应人工检查新增文件列表。

## 7. 冲突处理原则

### 7.1 `README.md`

处理原则：

- 以 `origin/main` 最新项目完成度描述为基础。
- 保留当前分支关于网络内部拆分的说明。
- 避免把 Survivor 描述成完全稳定的最终版本。
- 同时体现桌面端、Android、ECS 在线大厅和独立服务端现状。
- 保持相关文档列表指向最新计划和报告。

合并后 README 应表达：

- 五子棋、斗地主、飞行棋已经完成主要联机闭环。
- Survivor 是 MVP，已接入本地战斗、房间流转和在线联机战斗，但实时同步、稳定性、平衡和外部环境验证仍需继续完善。
- 网络层包含 `NetworkManager`、`LineJsonProtocol`、`NetworkAddressUtils`、`RoomDiscoveryService`、`ENet` 工具和协议定义。
- ECS 在线大厅和独立服务端仍是当前项目能力的一部分。

### 7.2 `src/app/appcontroller.cpp`

处理原则：

- 保留 `origin/main` 对 Survivor 同步、HUD、游戏结束流程和房间权威的近期修复。
- 保留当前分支对座位切换、网络拆分后调用路径和房间状态广播的修复。
- 不用整段覆盖方式解决冲突，必须按函数语义合并。
- 合并后要分别检查本地模式、局域网 Host、局域网 Client、ECS 在线房间四类路径。

重点复核：

- `SurvivorController::gameOverChanged` 中是否重复结算或漏结算。
- `remoteSurvivorChooseLevelUp` 和 `remoteSurvivorCloseChest` 在非法选择时是否仍触发必要重同步。
- `networkSyncRequested` 是否仍按玩家发送 fast packet 和 HUD packet。
- 房主结束游戏后是否正确清理 `DiscoveryGameInProgress`、房间状态和准备状态。
- 客户端断开、房主断开和房间返回流程是否仍能清理页面状态。
- `finishCurrentGameSession()`、`broadcastCurrentRoomState()` 和服务端房间同步调用顺序是否合理。

不接受的解决方式：

- 只保留当前分支版本，丢掉主线 Survivor 修复。
- 只保留主线版本，丢掉当前分支的座位和网络同步修复。
- 通过删除分支功能来消除冲突。

### 7.3 `CMakeLists.txt`

处理原则：

- 保留当前分支新增的网络辅助模块。
- 保留主线新增或调整的 Survivor、server、Android 相关文件。
- 确认 app 和 server 两个构建目标都能正常生成。

合并后 `appLanBoard` 至少应继续包含：

```text
src/network/linejsonprotocol.cpp
src/network/networkaddressutils.cpp
src/network/roomdiscoveryservice.cpp
src/network/networkmanager.cpp
src/game/survivorcontroller.cpp
src/game/survivornetcodec.cpp
src/game/survivorruntime.cpp
src/game/survivorsimulation.cpp
src/game/survivorrenderitem.cpp
```

QML 模块应继续包含：

```text
qml/pages/SurvivorPage.qml
```

SOURCES 应继续包含：

```text
src/network/linejsonprotocol.h
src/network/networkaddressutils.h
src/network/roomdiscoveryservice.h
src/network/networkmanager.h
src/game/survivorcontroller.h
src/game/survivornetcodec.h
src/game/survivorruntime.h
src/game/survivorsimulation.h
src/game/survivorworld.h
src/game/survivorrenderitem.h
```

## 8. 分阶段执行计划

### 阶段 1：同步前保护

目标：确保操作可回退。

操作：

```powershell
git fetch --all --prune --tags
git status --short --branch
git rev-list --left-right --count origin/main...HEAD
git branch backup/refactor-network-internals-before-main-sync-20260709
```

验收标准：

- 当前仍在 `refactor/network-internals`。
- 当前分支与 `origin/refactor/network-internals` 一致。
- 备份分支创建成功。
- 未跟踪文件策略已确认。

### 阶段 2：处理本地未跟踪文档

目标：避免文档文件混入主线同步冲突。

推荐方案 A：先单独提交文档。

```powershell
git add AGENTS.md docs/答辩学习文档-一周冲刺.md
git commit -m "docs: add repository and defense study notes"
```

推荐方案 B：暂不提交，继续保持未跟踪。

适用条件：

- 需要人工审阅文档内容。
- 不确定答辩学习文档是否属于仓库交付物。

不推荐方案：

- 不建议把这两个文件和 `merge: sync main into network internals` 放在同一个提交中。

### 阶段 3：合入最新主线

目标：把 `origin/main` 的 8 个较新提交纳入当前分支。

操作：

```powershell
git merge origin/main
```

预期结果：

- `README.md` 冲突。
- `src/app/appcontroller.cpp` 冲突。
- 其他文件自动合并。

验收标准：

- 所有冲突标记被人工解决。
- `git status` 不再显示 `UU` 文件。
- 没有使用整文件覆盖规避冲突。

### 阶段 4：人工复核语义

目标：确认自动合并没有造成隐性行为退化。

重点检查：

```powershell
git diff --check
git diff --name-status HEAD
git diff -- CMakeLists.txt
git diff -- src/app/appcontroller.cpp
git diff -- README.md
```

人工复核清单：

- `CMakeLists.txt` 是否同时包含网络拆分文件和 Survivor 文件。
- `AppController` 是否保留主线 Survivor 同步修复。
- `AppController` 是否保留当前分支座位切换和房间广播修复。
- README 中的完成度描述是否和代码状态一致。
- 相关文档链接是否指向当前最新计划或报告。

### 阶段 5：构建验证

目标：确认合并后的项目能重新配置并构建。

推荐命令：

```powershell
cmake --preset qt-mingw-desktop
cmake --build --preset qt-mingw-desktop
```

如果 preset 不可用，可使用既有手动命令：

```powershell
cd build-qt6103
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\mingw_64"
mingw32-make -j8
```

验收标准：

- `appLanBoard.exe` 构建成功。
- `lanboardServer.exe` 构建成功，除非本地显式关闭 `LANBOARD_BUILD_SERVER`。
- 只保留既有非阻塞警告，例如 `WrapVulkanHeaders` 或 Qt policy 警告。

### 阶段 6：最小功能验证

目标：用最小成本覆盖本轮高风险路径。

必须验证：

- 应用可启动。
- 首页、设置页、房间页、游戏页可以正常切换。
- 创建本地房间成功。
- 切换游戏类型后座位规则正常。
- 玩家可以在游戏位和旁观位之间切换，房主规则不被破坏。
- 五子棋或飞行棋至少完成一次开始和结算返回房间。
- 局域网发现服务可以启动和刷新，不出现明显运行时错误。
- Survivor 页面可以进入，HUD 和结算流程不明显崩溃。

建议验证：

- 双实例 Host / Client 房间同步。
- 斗地主三人房间准备和开局。
- 本地 `lanboardServer` 在线大厅创建、加入和同步。

外部环境待补充：

- Android 真机安装运行。
- 跨设备同 Wi-Fi UDP 广播发现。
- 真实 ECS 公网在线房间。
- Survivor 多人长时间同步稳定性。

### 阶段 7：提交同步结果

目标：形成可审阅提交。

推荐提交：

```powershell
git add README.md src/app/appcontroller.cpp CMakeLists.txt
git add 其他确认属于本轮合并结果的文件
git commit -m "merge: sync main into network internals"
```

如果本轮需要新增验证记录或工作报告，建议单独提交：

```powershell
git add docs/main-sync-work-report-20260709.md docs/document-index.md README.md
git commit -m "docs: record 20260709 main sync results"
```

验收标准：

- `git status --short --branch` 清楚。
- 当前分支包含 `origin/main`。
- 本地未跟踪文件状态符合预期。
- 构建和验证结果已记录。

## 9. 回滚方案

如果合并过程中发现冲突处理不可控，且尚未提交 merge：

```powershell
git merge --abort
```

如果已经提交但需要回到同步前状态：

```powershell
git switch refactor/network-internals
git reset --hard backup/refactor-network-internals-before-main-sync-20260709
```

注意：

- `reset --hard` 是破坏性操作，只应在确认没有需要保留的未提交改动后执行。
- 如果 `AGENTS.md` 和答辩学习文档仍未跟踪，回滚前要确认它们不会被误删或覆盖。

## 10. 审阅重点

建议审阅者重点看以下问题：

- 是否同意把 `AGENTS.md` 和答辩学习文档纳入仓库。
- 是否同意继续使用 merge 而不是 rebase。
- `AppController` 冲突解决是否保留双方关键行为。
- README 对项目完成度的描述是否足够真实，尤其是 Survivor 和外部环境验证边界。
- 构建验证和最小功能验证范围是否满足本轮同步风险。

## 11. 完成标准

本轮同步完成应满足：

- 当前分支已经包含 `origin/main = 5849e43`。
- `README.md` 和 `src/app/appcontroller.cpp` 冲突已人工解决。
- `CMakeLists.txt` 已确认包含当前分支网络拆分模块和主线 Survivor 模块。
- 桌面端配置和构建通过。
- 最小功能验证结果已记录。
- 未跟踪文件处理策略清楚。
- 新增或更新的文档已同步到 `docs/document-index.md` 和 README 相关文档列表。
