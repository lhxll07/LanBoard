# LanBoard

LanBoard（桌域）是一个基于 Qt Quick 的桌游联机平台项目，当前仓库为适合多人协作逐步推进的 MVP 骨架。

## 当前范围

- 使用 Qt Quick + QML 构建移动端优先的界面
- 使用 C++ 承担应用控制、房间管理、网络通信和游戏流程逻辑
- 先搭建最小项目结构，后续按模块逐步补全实现

## 模块边界

- `qml/`：界面页面与可复用 QML 组件
- `src/app/`：负责界面层与后端模块之间的总协调
- `src/network/`：负责网络通信入口与会话连接能力
- `src/lobby/`：负责房间状态、玩家状态等大厅逻辑
- `src/game/`：负责游戏流程控制和后续桌游规则扩展
- `src/common/`：负责跨模块共享的轻量类型定义

## 统一开发环境

为避免多人协作时出现“有人能编译、有人不能编译”的情况，项目统一使用以下环境：

- Qt：`6.10.3`
- 桌面版 Qt 架构：`win64_mingw`
- Android 版 Qt 架构：`android_arm64_v8a`
- UI 方案：`Qt Quick + QML`
- 构建方式：`CMake`
- JDK：`21`
- Android SDK Platform：`36`
- Android Build-Tools：`36.0.0`
- Android NDK：`27.2.12479018`

桌面 Qt 安装目录统一为：

- `C:\Qt\6.10.3\mingw_64`

Android Qt 安装目录统一为：

- `C:\Qt\6.10.3\android_arm64_v8a`

## Qt 安装方式

项目推荐使用 `aqtinstall` 安装 Qt，不使用 Qt 官方在线安装器。

### 1. 安装 aqtinstall

```powershell
pip install -U aqtinstall
```

如果默认 `pip` 下载过慢，可以先切到清华源：

```powershell
python -m pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
pip install -U aqtinstall
```

### 2. 生成 aqt 配置文件

项目验证可用的镜像是南京大学镜像：

- `https://mirror.nju.edu.cn/qt`

执行：

```powershell
@'
[aqt]
baseurl: https://mirror.nju.edu.cn/qt
concurrency: 2

[requests]
connection_timeout: 10
response_timeout: 60
max_retries_on_connection_error: 8
retry_backoff: 1.0
max_retries_to_retrieve_hash: 1
hash_algorithm: sha256
INSECURE_NOT_FOR_PRODUCTION_ignore_hash: True

[mirrors]
trusted_mirrors:
    https://mirror.nju.edu.cn/qt
blacklist:
    https://download.qt.io
fallbacks:
    https://mirror.nju.edu.cn/qt
'@ | Set-Content -Encoding ascii "$env:TEMP\aqt-nju.ini"
```

### 3. 安装桌面版 Qt

```powershell
python -m aqt -c "$env:TEMP\aqt-nju.ini" install-qt -O C:\Qt -b https://mirror.nju.edu.cn/qt --UNSAFE-ignore-hash windows desktop 6.10.3 win64_mingw
```

### 4. 安装 Android 版 Qt

```powershell
python -m aqt -c "$env:TEMP\aqt-nju.ini" install-qt -O C:\Qt -b https://mirror.nju.edu.cn/qt --UNSAFE-ignore-hash --autodesktop all_os android 6.10.3 android_arm64_v8a
```

### 5. 安装完成后检查

执行：

```powershell
Get-ChildItem C:\Qt\6.10.3
```

正常应至少看到：

- `mingw_64`
- `android_arm64_v8a`

## 说明

- 不要自行切换到其他 Qt 小版本，例如 `6.11.x` 或 `6.12.x`
- 不要将桌面编译器改为 `MSVC`
- 不要混用不同版本的 Android NDK
- 项目当前统一以 `arm64-v8a` 为 Android 目标架构
