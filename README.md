# LanBoard（桌域）

一个基于 `Qt Quick + QML + C++` 的局域网桌游对战项目，当前第一款游戏为五子棋。

## 当前状态

当前项目已经可以跑通一个完整闭环：

1. 首页展示当前游戏和本地配置
2. 房间页负责加入房间、创建房间和本地双人
3. 主机和客户端在房间内同步玩家、准备状态和开始游戏
4. 双方进入五子棋页面并同步落子
5. 对局结束后自动返回房间，并清空双方准备状态
6. 设置页可修改昵称和默认端口，且会本地持久化
7. 最近一次加入房间的 IP 和端口会自动记住

## 页面说明

- `首页`：只展示游戏简介和当前本地配置
- `房间页`：默认显示加入/创建/本地双人入口，进入房间后才显示房间状态
- `游戏页`：五子棋棋盘、回合提示、认输并返回房间
- `设置页`：昵称、默认端口、局域网地址

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
│   │   ├── PlayerCard.qml
│   │   └── SettingCard.qml
│   └── pages/
│       ├── GamePage.qml
│       ├── HomePage.qml
│       ├── RoomPage.qml
│       └── SettingsPage.qml
├── src/
│   ├── app/       AppController，负责整体流程与设置持久化
│   ├── common/    预留共享类型目录
│   ├── game/      GameController，负责五子棋规则
│   ├── lobby/     RoomManager，负责房间玩家与准备状态
│   └── network/   NetworkManager，负责局域网 TCP 通信
├── design/
├── build-qt6103/
└── build-android/
```

## 已实现的关键行为

### 房间与联机

- 加入房间时需要手动输入 IP 和端口
- 创建房间时使用设置中的默认端口
- 房间页只在真正进入房间后展示玩家列表和房间状态
- 客户端断开后会自动退出房间状态

### 对局流程

- 房主开始游戏后同步进入棋盘
- 网络对局只允许当前玩家落子
- 认输会直接结束对局并返回房间
- 任意一方胜负确定后，双方都会自动返回房间
- 返回房间后双方准备状态会被重置，必须重新准备才能开始下一局

### 设置与持久化

通过 `QSettings` 本地保存：

- `昵称`
- `默认端口`
- `最近一次加入房间的 IP`
- `最近一次加入房间的端口`

## 构建方式

### 桌面端

首次配置：

```powershell
cmake -S . -B build-qt6103 -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\mingw_64"
```

编译：

```powershell
cmake --build build-qt6103 --parallel 8
```

可执行文件输出到：

```text
build-qt6103/appLanBoard.exe
```

### Android

如果本地已经配置好 Android Qt Kit，可以直接编译：

```powershell
cmake --build build-android --parallel 8
```

## 常见问题

### 1. `cannot open output file appLanBoard.exe: Permission denied`

说明桌面版程序还在运行，占用了输出文件。先关闭或结束：

```powershell
Get-Process appLanBoard -ErrorAction SilentlyContinue | Stop-Process -Force
```

然后重新执行构建。

### 2. 加入房间失败

优先检查：

- 主机 IP 是否正确
- 端口是否与主机一致
- 主机是否已经创建房间
- Windows 防火墙是否拦截了程序

### 3. 设置修改后没有生效

- 昵称会影响之后新建/加入的房间
- 默认端口会影响之后创建房间时使用的端口
- 最近加入房间的 IP/端口会用于回填输入框

## 开发说明

- UI 主体在 `qml/`
- 业务协调集中在 `src/app/appcontroller.cpp`
- 房间逻辑集中在 `src/lobby/roommanager.cpp`
- 网络同步集中在 `src/network/networkmanager.cpp`
- 当前最大和最复杂的页面是 `qml/pages/RoomPage.qml`

详见：

- [Qt安装流程.md](./Qt安装流程.md)
- [Git协作流程.md](./Git协作流程.md)
- [任务分工.md](./任务分工.md)
