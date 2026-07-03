# LanBoard（桌域）

一个基于 Qt Quick + QML + C++ 的局域网桌游对战平台。

## 项目结构

```
LanBoard/
├── qml/                    # UI 层（QML 页面与可复用组件）
│   ├── Main.qml            应用入口
│   ├── components/         可复用 UI 组件
│   │   ├── AppTheme.qml          全局设计 Token
│   │   ├── ActionButton.qml      操作按钮
│   │   ├── BottomTextNav.qml     底部导航栏
│   │   ├── GameCard.qml          游戏卡片
│   │   ├── PageHeader.qml        页面标题
│   │   ├── PlayerCard.qml        玩家卡片
│   │   └── SettingCard.qml       设置卡片
│   └── pages/              页面级组件
│       ├── HomePage.qml          首页
│       ├── RoomPage.qml          房间页
│       ├── GamePage.qml          游戏页（五子棋）
│       └── SettingsPage.qml      设置页
├── src/                    # C++ 后端
│   ├── app/                AppController 总协调
│   ├── common/             共享类型定义
│   ├── game/               GameController 游戏规则
│   ├── lobby/              RoomManager 房间管理
│   └── network/            NetworkManager 网络通信
├── design/                 设计稿
└── build-qt6103/           构建输出
```

## 团队分工

| 角色 | 负责 | 目录 |
|------|------|------|
| A — 界面与交互 | QML 页面、组件、布局、动效 | `qml/` |
| B — 局域网通信 | TCP 监听/连接、消息收发 | `src/network/` |
| C — 房间大厅逻辑 | 玩家列表、房间状态、准备机制 | `src/lobby/` |
| D — 游戏逻辑与集成 | 五子棋规则、回合控制、模块串联 | `src/game/` + `src/app/` |

## 第一阶段目标

1. 首页进入大厅
2. 创建 / 加入房间
3. 玩家准备
4. 房主开始游戏
5. 进入五子棋页面
6. 双方局域网同步落子
7. 判断胜负并显示结果

## 构建方式

```powershell
cd build-qt6103
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.10.3\mingw_64"
mingw32-make -j8
```

## 统一开发环境

| 环境 | 版本 |
|------|------|
| Qt | 6.10.3 |
| 桌面架构 | win64_mingw |
| Android 架构 | android_arm64_v8a |
| JDK | 21 |
| Android SDK Platform | 36 |
| Android Build-Tools | 36.0.0 |
| Android NDK | 27.2.12479018 |

详见 [Qt安装流程.md](./Qt安装流程.md) 和 [Git协作流程.md](./Git协作流程.md)。
