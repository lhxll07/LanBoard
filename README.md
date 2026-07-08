# LanBoard（桌域）

一个基于 `Qt 6 + Qt Quick + QML + C++` 的联机桌游与轻量多人游戏项目，面向课程大作业、局域网演示和小规模在线联机演示。

当前主线已经完成一轮较大规模重构，核心方向包括：

- 统一 `游戏控制器 / 房间 / 网络 / 页面壳层` 的运行模型
- 收拢首页、联机页、设置页与房间页的职责边界
- 用更稳定的客户端 / 服务端链路承载局域网与在线大厅
- 为后续继续推进 `Survivor` 联机同步预留统一基础设施

当前仓库包含 4 个游戏入口：

- `五子棋`：双人回合制对弈
- `斗地主`：三人联机纸牌
- `飞行棋`：双人回合制桌游
- `Survivor MVP`：生存类原型，已打通本地试玩、房间入口和在线联机战斗，实时同步与整体体验仍在继续完善

项目当前支持：

- `本地模式`
- `局域网联机`
- `在线联机（ECS 房间大厅）`
- `独立服务端房间大厅`

## 当前状态

### 已完成

- 首页统一展示游戏入口与简介
- 房间页统一承载加入房间、创建房间、局域网扫描、在线房间列表和房间状态
- 房主可在房间内切换当前游戏
- 房间支持最多 `8` 人，区分 `游戏位 / 旁观位`
- 切换游戏时会按该游戏人数规则自动整理游戏位
- 对局结束后自动返回房间，并清空准备状态
- 设置通过 `QSettings` 持久化保存
- 设置页支持直接复制当前局域网地址
- UI 壳层、底部导航、房间页结构与动画已统一整理

### 联机覆盖范围

- `五子棋`：本地、局域网、在线房间、大厅服务端
- `斗地主`：本地、局域网、在线房间、大厅服务端
- `飞行棋`：本地、局域网、在线房间、大厅服务端
- `Survivor MVP`：本地原型已可玩，房间流转与在线联机战斗已接入，实时同步、稳定性与平衡仍在持续完善

### 网络能力

- 手动输入 `IP + 端口` 加入局域网房间
- 通过 `UDP` 广播自动发现同一局域网中的房间
- 连接 ECS 在线大厅并拉取多个在线房间
- 创建在线房间、加入在线房间、同步房间状态
- 独立服务端负责在线房间大厅与房间同步
- 客户端与服务端底层网络已统一收敛到同一套运行时链路

## 设置持久化

通过 `QSettings` 保存以下配置：

- `昵称`
- `默认端口`
- `最近一次加入房间的 IP`
- `最近一次加入房间的端口`
- `在线服务器 Host`
- `在线服务器 Port`

## 页面说明

- `首页`：游戏入口与简介
- `房间页`：局域网大厅、在线大厅、创建房间、加入房间、房间状态
- `五子棋页`：棋盘、回合提示、认输、结算后返回房间
- `斗地主页`：手牌、出牌/不出、回合状态、联网同步
- `飞行棋页`：掷骰、移动、回合切换、联网同步
- `SurvivorPage`：生存原型、HUD、升级选择、宝箱结算、在线联机战斗
- `设置页`：昵称、默认端口、局域网地址复制、在线服务器地址等

## 项目结构

```text
LanBoard/
├── qml/
│   ├── Main.qml
│   ├── components/
│   │   ├── ActionButton.qml
│   │   ├── AppTheme.qml
│   │   ├── BottomTextNav.qml
│   │   ├── GameCard.qml
│   │   ├── PageHeader.qml
│   │   ├── PlayerAvatar.qml
│   │   ├── PlayerCard.qml
│   │   └── SettingCard.qml
│   └── pages/
│       ├── HomePage.qml
│       ├── RoomPage.qml
│       ├── GamePage.qml
│       ├── DouDiZhuPage.qml
│       ├── FlightChessPage.qml
│       ├── SurvivorPage.qml
│       └── SettingsPage.qml
├── src/
│   ├── app/       AppController，负责流程调度、设置持久化、导航
│   ├── common/    共享类型、房间快照、控制器基类
│   ├── game/      五子棋 / 斗地主 / 飞行棋 / Survivor 控制器、运行时与渲染层
│   ├── lobby/     房间、玩家、准备状态、座位状态
│   ├── network/   ENet 连接、消息分发、在线大厅、局域网发现和地址选择
│   └── server/    独立在线服务端
├── CMakeLists.txt
├── CMakePresets.json
├── build-qt6103/
└── build-android/
```

## 关键行为

### 局域网联机

- 开房后可被同一局域网中的设备发现
- 房间列表显示房主、游戏类型、人数、状态
- 支持手动刷新扫描结果
- 多网卡场景下对重复房间做去重

### 在线联机

- App 可连接 ECS 在线大厅
- 在线大厅展示多个房间，而不是单一全局房间
- 房主可创建指定游戏类型的在线房间
- 玩家加入后统一由房主控制当前房间游戏类型
- 当前在线服务端主要负责大厅、房间流转与房间状态同步

### 房间状态

- 房主固定为 `游戏位`
- 普通玩家可在 `游戏位 / 旁观位` 间切换
- 切换游戏时会自动整理房间座位
- 只有满足当前游戏人数要求且已准备完成时才允许开局

### Survivor MVP

- 主页和房间页已接入 Survivor 入口
- 当前已经支持本地体验和在线联机战斗
- HUD、小地图、升级面板、宝箱面板已接入
- 联机房间流转与实时战斗链路已接入，但同步细节、稳定性与数值体验仍处于持续打磨阶段

## 架构说明

- `AppController` 负责导航、设置持久化、房间流转和控制器协调
- `GameControllerBase` 抽象了不同游戏控制器的公共能力，减少了房间页与网络层的分支逻辑
- `RoomManager + RoomSnapshot` 统一描述房间状态、座位、准备状态和游戏人数规则
- `NetworkManager` 统一承载局域网发现、房间连接、在线大厅连接和实时/半实时消息收发
- `ServerApp` 独立负责 ECS 在线大厅与在线房间服务，不影响局域网 Host/Client 直连模式

## 构建

### 桌面端

首次配置：

```powershell
cmake -S . -B build-qt6103 -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\mingw_64"
```

编译：

```powershell
cmake --build build-qt6103 --parallel 8
```

产物：

```text
build-qt6103/appLanBoard.exe
build-qt6103/lanboardServer.exe
```

### Android

如果本地已经配置好 Qt Android Kit：

```powershell
cmake --build build-android --parallel 8
```

当前仓库环境下常用产物位置：

```text
build-android/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
build-android/LanBoard-android-arm64-release-signed.apk
```

说明：

- `unsigned.apk` 是 Android 构建产物
- `signed.apk` 是额外签名后的可安装包

### 独立服务端

如需只构建服务端：

```powershell
cmake -S . -B build-server -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\mingw_64" -DLANBOARD_BUILD_APP=OFF -DLANBOARD_BUILD_SERVER=ON
cmake --build build-server --parallel 8
```

说明：

- `appLanBoard` 是桌面 / Android 客户端
- `lanboardServer` 是 ECS 上运行的在线大厅与房间服务端
- 当前服务端主要负责大厅与房间服务，不承担完整的 Survivor 权威战斗模拟

## 常见问题

### 1. `cannot open output file appLanBoard.exe: Permission denied`

说明桌面程序还在运行，占用了输出文件：

```powershell
Get-Process appLanBoard -ErrorAction SilentlyContinue | Stop-Process -Force
```

然后重新构建。

### 2. 局域网发现不到房间

优先检查：

- 双方是否在同一局域网
- 房主是否已经创建房间
- 路由器或系统防火墙是否拦截 UDP 广播
- Android 端是否使用支持局域网广播的 Wi-Fi 网络

### 3. 无法加入房间

优先检查：

- 主机 IP 是否正确
- 端口是否一致
- 房间是否已满
- 房主当前游戏类型与人数要求是否匹配

### 4. 在线房间连接失败

优先检查：

- `设置页` 中在线服务器 Host / Port 是否正确
- ECS 端口是否已放行
- 服务器进程是否已启动

### 5. Survivor 当前是什么状态？

当前 `Survivor` 还处于 MVP 阶段，已经接入：

- 首页入口
- 房间入口
- 本地战斗原型
- 在线联机战斗

目前的问题不再是“不能在线联机”，而是这套联机战斗仍在继续完善，包括：

- 实时同步细节
- 稳定性与流畅度
- 数值和平衡
- 更完整的权威化方案

因此 README 中将其标注为 `MVP / 原型`，但它已经具备可用的在线联机战斗能力。

## 开发说明

- UI 主体在 `qml/`
- 页面协调集中在 `src/app/appcontroller.cpp`
- 房间逻辑集中在 `src/lobby/roommanager.*`
- 网络门面集中在 `src/network/networkmanager.cpp`
- ENet 工具、地址选择和局域网发现分别在 `src/network/enetutils.*`、`src/network/networkaddressutils.*`、`src/network/roomdiscoveryservice.*`
- 在线服务端逻辑集中在 `src/server/serverapp.cpp`
- 当前最复杂页面是 `qml/pages/RoomPage.qml`
- 当前实验性游戏逻辑主要集中在 `src/game/survivorcontroller.* / survivorsimulation.* / survivorruntime.*`

## 相关文档

- [Qt安装流程.md](./Qt安装流程.md)
- [Git协作流程.md](./Git协作流程.md)
- [任务分工.md](./任务分工.md)
- [项目文档索引](./docs/document-index.md)
- [网络消息表](./docs/network-messages.md)
- [网络重构工作报告（2026-07-05）](./docs/network-refactor-work-report-20260705.md)
- [当前合并前工作计划（2026-07-07）](./docs/main-sync-workplan-20260707.md)
- [最新 main 同步工作报告（2026-07-08）](./docs/main-sync-work-report-20260708.md)
