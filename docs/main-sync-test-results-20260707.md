# 再次同步最新 main 后验证记录

记录日期：2026-07-07

文档性质：当前验证记录。本文件对应 `docs/main-sync-workplan-20260707.md`，记录 `refactor/network-internals` 再次同步最新 `origin/main` 后的冲突处理和自动回归结果。

## 1. 同步范围

当前分支：

- `refactor/network-internals`

同步前备份分支：

- `backup/refactor-network-internals-before-main-sync-20260707`

同步前提交：

- `6d24a08 docs: organize merge planning documents`

同步的主线提交：

- `c27b4cc Fix Android startup rendering and nav click-through`

同步后提交：

- `a0b43a2 merge: sync main into network internals`

分支关系：

```text
HEAD...origin/main = 16 0
```

说明：

- `origin/main` 已经是当前 `HEAD` 的祖先。
- 当前分支包含最新主线内容。
- 当前分支相对 `origin/refactor/network-internals` 领先 7 个提交。
- 工作区只剩本地未跟踪文件 `AGENTS.md`。

## 2. 冲突文件

本次 `git merge origin/main` 出现冲突：

```text
README.md
src/app/appcontroller.cpp
src/network/networkmanager.cpp
src/network/networkmanager.h
```

处理原则：

- 以 `origin/main` 的 ENet、Survivor、运行时和 UI 改动为基础。
- 保留当前分支的 `RoomDiscoveryService`，不恢复 `NetworkManager` 内置 UDP 发现实现。
- `NetworkManager` 继续作为网络门面，ENet 负责连接和房间同步，`RoomDiscoveryService` 负责局域网发现。
- 保留房主开始游戏路径修复：房主不因 `isConnected()` 为 true 而误走客户端 `sendStartGame()` 路径。
- 保留 `game_over` 延迟广播修复，确保最后一步同步先于结束消息被客户端应用。
- README 同时反映主线 ENet/Survivor 状态和当前网络拆分结构。

## 3. 基础检查

命令：

```powershell
git diff --check
git diff --cached --check
cmake --preset qt-mingw-desktop
cmake --build --preset qt-mingw-desktop
```

结果：通过。

说明：

- 首次完整构建因 5 分钟超时中断。
- 重新执行增量构建后通过。
- `appLanBoard.exe` 和 `lanboardServer.exe` 均成功生成。
- 仍有已知非阻塞提示：`WrapVulkanHeaders` 未找到。

## 4. 局域网发现服务级测试

命令：

```powershell
cmake -S build\codex-lan-discovery-check -B build\codex-lan-discovery-check\build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\mingw_64 -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build\codex-lan-discovery-check\build
build\codex-lan-discovery-check\build\lanDiscoveryCheck.exe
```

结果：通过。

覆盖：

- `RoomDiscoveryService` 发布房间。
- discoverer 发现 publisher。
- 房间状态更新。
- 停止发布后的过期清理。
- publisher 不发现自己的房间。

## 5. 控制层 E2E

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
  - 客户端加入。
  - 准备并开始。
  - 双方落子同步。
  - 胜负和返回房间同步。

- 飞行棋：
  - 创建房间并切换飞行棋。
  - 客户端加入。
  - 准备并开始。
  - 掷骰和移动同步。
  - 客户端掷骰请求能到达房主并广播结果。

- 斗地主：
  - 创建斗地主房间。
  - 两个客户端加入。
  - 三人准备并开始。
  - 客户端收到各自私有 `ddz_state`。

临时测试工程调整：

- `build/codex-appcontroller-e2e/CMakeLists.txt`
  - 增加 ENet `FetchContent`。
  - 增加 `enetutils`、`protocolids`、Survivor runtime/simulation/netcodec 和 common 基类依赖。

这些调整发生在 `.gitignore` 覆盖的 `build/` 目录，不进入提交。

## 6. 本地在线服务端 ENet E2E

旧的 `build/codex-online-server-e2e/online-server-e2e.ps1` 是 TCP 逐行 JSON 脚本，已经不适配当前 ENet 版 `ServerApp`。

本次新增临时 ENet C++ 测试工程：

```text
build/codex-online-server-e2e/CMakeLists.txt
build/codex-online-server-e2e/main.cpp
```

命令：

```powershell
cmake -S build\codex-online-server-e2e -B build\codex-online-server-e2e\build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\mingw_64 -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build\codex-online-server-e2e\build
build\codex-online-server-e2e\build\onlineServerEnetE2E.exe
```

结果：通过。

输出：

```text
PASS online server ENet E2E
```

覆盖：

- 未加入房间发送 `ready` 返回 `error: not_joined`。
- Host 创建五子棋在线房间。
- Observer 拉取 `rooms_list` 并能看到新房间。
- Guest 加入房间。
- Host 和 Guest 收到 2 人 `room_state`。
- Host 和 Guest 准备后收到全员 ready 的 `room_state`。
- Host 发送 `start_game` 后双方收到 `game_start`。
- 新 Observer 在游戏开始后拉取房间列表，房间 `inGame = true`。
- Host 和 Guest 的五子棋落子均能广播给双方。

## 7. 当前仍未覆盖

以下项目仍需要人工或外部环境：

- 桌面 GUI 多实例人工复查。
- Android 真机安装和运行。
- 跨设备同 Wi-Fi UDP 广播发现。
- 远端 ECS 在线房间真实公网流程。
- Survivor 实时多人战斗同步完整验证。

说明：

- Survivor 相关代码已参与主项目构建。
- 本轮自动 E2E 重点覆盖五子棋、飞行棋、斗地主三款回合制联机闭环，以及本地 ENet 在线服务端最小流程。

## 8. 当前判断

从本机自动验证看，本次同步最新 `origin/main` 后：

- 桌面构建可用。
- ENet 主线改动已保留。
- 网络拆分仍可用。
- `RoomDiscoveryService` 服务级行为仍可用。
- 三款回合制游戏的控制层联机闭环仍可用。
- 本地在线服务端 ENet 最小端到端流程可用。

建议下一步执行桌面 GUI 多实例人工复查，通过后再推送分支并更新 PR。
