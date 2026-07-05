# 并入 main 前测试记录

记录日期：2026-07-05

## 当前分支状态

- 分支：`refactor/network-internals`
- 测试执行基线：`2e50ae3 docs: align project status before main merge`
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

## 本次未覆盖

以下项目仍需人工或更完整的端到端测试：

- 两个 GUI 实例的实际局域网发现和加入流程。
- 五子棋双实例完整闭环。
- 飞行棋双实例掷骰和移动同步。
- 斗地主三人联机出牌、过牌和私有状态同步。
- ECS 或本地 `lanboardServer` 的在线房间端到端流程。
- Android 真机或同 Wi-Fi 设备测试。

## 当前判断

本次测试说明 `RoomDiscoveryService` 的核心服务级行为可用，未发现需要立即修复的局域网发现代码问题。

在并入 `main` 前，仍应按 `docs/pre-main-merge-workplan.md` 继续完成 GUI 双实例和三款游戏联机冒烟测试。
