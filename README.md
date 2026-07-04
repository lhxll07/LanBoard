# LanBoard（桌域）

一个基于 `Qt 6 + Qt Quick + QML + C++` 的联机桌游项目，面向课程大作业和局域网演示场景。

当前已落地两款游戏：

- `五子棋`：双人对弈
- `斗地主`：三人联机

项目同时支持：

- `本地模式`
- `局域网联机`
- `在线联机（ECS 房间大厅）`
- `独立服务端`

## 当前功能

### 游戏与房间

- 首页展示游戏卡片与当前配置
- 房间页负责加入房间、创建房间、局域网扫描和在线房间入口
- 房主可在房间内切换当前桌游
- 房间支持 `游戏位 / 旁观位`
- 五子棋房间按双人规则限制游戏位
- 斗地主房间按三人规则限制游戏位
- 对局结束后自动返回房间，并清空准备状态

### 联机方式

- 支持手动输入 `IP + 端口` 加入局域网房间
- 支持局域网 UDP 广播发现房间
- 支持连接 ECS 在线大厅并列出多个在线房间
- 支持创建在线房间、加入在线房间、同步房间状态

### 设置与持久化

通过 `QSettings` 本地保存：

- `昵称`
- `默认端口`
- `最近一次加入房间的 IP`
- `最近一次加入房间的端口`
- `在线服务器 Host`
- `在线服务器 Port`

## 页面说明

- `首页`：展示游戏入口和当前配置摘要
- `房间页`：联机大厅、局域网发现、在线房间列表、房间状态
- `五子棋页`：棋盘、回合提示、认输、结算后返回房间
- `斗地主页`：手牌、出牌/不出、回合状态、联网同步
- `设置页`：昵称、默认端口、在线服务器地址等

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
│       └── SettingsPage.qml
├── src/
│   ├── app/       AppController，负责流程调度、设置持久化、导航
│   ├── common/    共享类型
│   ├── game/      五子棋 / 斗地主规则控制器
│   ├── lobby/     房间、玩家、准备状态、座位状态
│   ├── network/   客户端 / 局域网 / 在线房间网络逻辑
│   └── server/    独立在线服务端
├── design/
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

### 房间状态

- 房主固定为 `游戏位`
- 普通玩家可在 `游戏位 / 旁观位` 间切换
- 切换游戏时会自动整理房间座位
- 只有满足当前游戏人数要求且已准备完成时才允许开局

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

## 开发说明

- UI 主体在 `qml/`
- 页面协调集中在 `src/app/appcontroller.cpp`
- 房间逻辑集中在 `src/lobby/roommanager.cpp`
- 网络逻辑集中在 `src/network/networkmanager.cpp`
- 在线服务端逻辑集中在 `src/server/serverapp.cpp`
- 当前最复杂页面是 `qml/pages/RoomPage.qml`

## 相关文档

- [Qt安装流程.md](./Qt安装流程.md)
- [Git协作流程.md](./Git协作流程.md)
- [任务分工.md](./任务分工.md)
