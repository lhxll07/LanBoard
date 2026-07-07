# 并入 main 前测试记录

记录日期：2026-07-05

文档性质：历史验证记录。本文件对应 `docs/pre-main-merge-workplan-20260705.md`；后续同步主线后的验证记录见 `docs/main-sync-test-results-20260706.md`。

## 当前分支状态

- 分支：`refactor/network-internals`
- 测试执行基线：`1bcca38 docs: record GUI multiplayer test results`
- 主线基线：`2c1a3ba Add networked flight chess and 8-player rooms`
- 本地未跟踪文件：`AGENTS.md`

## 已执行测试

### 1. 常规构建

命令：

```powershell
cmake --build build\codex-branch-check
```

结果：通过。

说明：该构建目录已是最新状态，输出 `ninja: no work to do.`。

### 2. Qt 6.10.3 preset 配置和构建

命令：

```powershell
cmake --preset qt-mingw-desktop
cmake --build --preset qt-mingw-desktop
```

结果：通过。

说明：

- 已确认 `CMakePresets.json` 使用 `C:/Qt/6.10.3/mingw_64` 可以正常配置。
- 完整构建生成 `lanboardServer.exe` 和 `appLanBoard.exe`。
- 构建过程中仍有已知非阻塞提示：
  - `WrapVulkanHeaders` 未找到。
  - Qt QML policy `QTP0004` dev warning。

### 3. 局域网发现自动冒烟测试

测试方式：

- 在被 `.gitignore` 覆盖的临时目录 `build/codex-lan-discovery-check` 中创建 Qt 控制台测试工程。
- 测试工程直接链接生产代码：
  - `src/network/roomdiscoveryservice.cpp`
  - `src/network/networkaddressutils.cpp`
- 同一进程中创建两个 `RoomDiscoveryService` 实例：
  - publisher：模拟房主发布房间。
  - discoverer：模拟客户端扫描房间。

测试命令：

```powershell
cmake -S build\codex-lan-discovery-check -B build\codex-lan-discovery-check\build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\mingw_64 -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build\codex-lan-discovery-check\build
build\codex-lan-discovery-check\build\lanDiscoveryCheck.exe
```

验证点：

- discoverer 能发现 publisher 发布的房间。
- 发现字段包含并正确更新：
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
- publisher 更新房间状态后，discoverer 能收到更新。
- publisher 停止发布后，discoverer 能在过期清理后移除房间。
- publisher 不会把自己发布的房间加入自己的 `discoveredRooms`。

结果：通过。

### 4. 控制层端到端联机冒烟测试

测试方式：

- 在被 `.gitignore` 覆盖的临时目录 `build/codex-appcontroller-e2e` 中创建 Qt 控制台测试工程。
- 测试工程直接链接生产代码中的 `AppController`、`RoomManager`、`NetworkManager`、三个游戏 controller 和网络辅助模块。
- 不经过 QML，直接模拟 QML 会触发的控制层调用。

测试命令：

```powershell
cmake -S build\codex-appcontroller-e2e -B build\codex-appcontroller-e2e\build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\mingw_64 -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe
cmake --build build\codex-appcontroller-e2e\build
build\codex-appcontroller-e2e\build\appControllerE2E.exe
```

验证点：

- 五子棋：
  - 主机创建房间。
  - 客户端加入。
  - 双方准备。
  - 房主开始游戏。
  - 主机和客户端交替落子。
  - 主机五连胜负同步。
  - 双方收到结束状态并返回房间页。
- 飞行棋：
  - 主机创建房间并切换到飞行棋。
  - 客户端加入。
  - 双方准备并开始游戏。
  - 主机掷骰同步。
  - 主机移动飞机同步。
  - 客户端掷骰请求可到达主机并广播回客户端。
- 斗地主：
  - 主机创建斗地主房间。
  - 两个客户端加入，形成 3 个游戏位玩家。
  - 三方准备并开始游戏。
  - 两个客户端收到各自私有 `ddz_state`。
  - 两个客户端的手牌视角不同。

结果：通过。

发现并修复的问题：

- 五子棋和飞行棋中，主机的最后一步操作可能先触发 `game_over` 广播，再由 QML 广播最后一步 `move` / `flight_move_result`。
- 客户端收到 `game_over` 后会进入结束状态，随后可能拒绝应用最后一步棋盘变化。
- 已修复为在 `AppController` 中把主机侧 `game_over` 和房间状态广播延迟到当前调用栈之后执行，保证最后一步同步消息先发出。

### 5. GUI 本地多实例联机测试

测试方式：

- 使用 `build-qt-ascii/appLanBoard.exe` 启动多个可见桌面窗口。
- A 窗口作为房主，B/C 窗口作为客户端。
- 由人工按房间页和游戏页实际按钮完成操作，观察 UI 状态和同步结果。

测试项：

- 五子棋双实例：
  - B 能发现 A 的局域网房间。
  - B 能加入房间。
  - 双方准备、A 开始游戏正常。
  - A/B 落子互相同步。
  - 页面无报错、无卡住、无明显状态异常。
- 飞行棋双实例：
  - B 能发现 A 的飞行棋房间。
  - B 能加入房间。
  - 双方准备、A 开始游戏正常。
  - A 掷骰和移动同步到 B。
  - B 掷骰和移动同步到 A。
  - 页面无报错、无卡住、无明显状态异常。
- 斗地主三实例：
  - B、C 均能发现并加入 A 的斗地主房间。
  - 三人准备、A 开始游戏正常。
  - 三个窗口均进入斗地主页面。
  - 出牌、过牌和状态同步正常。
  - 页面无报错、无卡住、无明显状态异常。

结果：通过。

### 6. 在线房间本地服务端端到端测试

测试方式：

- 使用 `build-qt-ascii/lanboardServer.exe` 启动本地专用服务端。
- 使用临时 PowerShell 测试脚本创建 3 个 TCP 客户端：
  - Host：创建在线房间并作为房主。
  - Guest：加入在线房间并进行游戏操作。
  - Observer：请求在线房间列表并验证错误消息。
- 服务端使用随机本地端口，本次测试端口为 `62456`。

测试命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build\codex-online-server-e2e\online-server-e2e.ps1
```

验证点：

- 未加入房间的客户端发送 `ready` 后收到 `error: not_joined`。
- Host 发送 `create_room` 后收到 `room_state`，并分配为五子棋黑方 `piece = 1`。
- Observer 发送 `list_rooms` 后收到 `rooms_list`，能看到新建房间。
- 房间列表字段正确：
  - `roomId`
  - `roomName`
  - `hostName`
  - `gameId`
  - `gameName`
  - `playerCount`
  - `roomCapacity`
  - `maxPlayers`
  - `inGame`
  - `isFull`
- Guest 发送 `join_room` 后 Host 和 Guest 都收到 2 人 `room_state`。
- Host 和 Guest 均发送 `ready` 后，双方收到 active 玩家全员 ready 的 `room_state`。
- Host 发送 `start_game` 后，双方收到 `game_start`。
- 游戏开始后 Observer 再次请求 `list_rooms`，对应房间 `inGame = true`。
- Host 五子棋落子后，Host 和 Guest 均收到 `move`。
- Guest 五子棋落子后，Host 和 Guest 均收到 `move`。

结果：通过。

说明：

- 本项覆盖本地 `lanboardServer` 的在线房间最小端到端流程。
- 本项未覆盖远端 ECS 实例的真实公网连通性。

## 本次未覆盖

以下项目仍需人工或更完整的端到端测试：

- 远端 ECS 实例的在线房间端到端流程。
- Android 真机或同 Wi-Fi 设备测试。
- 跨设备同一 Wi-Fi 下的真实 UDP 广播发现。

当前环境说明：

- `adb devices -l` 未检测到已连接 Android 设备，因此 Android 真机安装和运行测试未执行。
- 当前只有本机自动化环境，缺少第二台同 Wi-Fi 设备，因此真实跨设备 UDP 广播发现未执行。

## 当前判断

本次测试说明 `RoomDiscoveryService` 的核心服务级行为可用，控制层三款游戏的本地 TCP 联机主流程可用，桌面 GUI 本地多实例联机流程可用，本地 `lanboardServer` 在线房间最小端到端流程可用。

在并入 `main` 前，远端 ECS、跨设备同 Wi-Fi UDP 广播和 Android 真机测试仍需要外部环境或设备；这些项目可作为合并风险项由团队决定是否阻塞。
