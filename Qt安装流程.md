# Qt 安装流程

本文档用于统一项目组成员的 Qt 开发环境。

## 一、统一环境版本

本项目统一使用以下版本：

- Qt：`6.10.3`
- 桌面版 Qt 架构：`win64_mingw`
- Android 版 Qt 架构：`android_arm64_v8a`
- UI 方案：`Qt Quick + QML`
- 构建方式：`CMake`
- JDK：`21`
- Android SDK Platform：`36`
- Android Build-Tools：`36.0.0`
- Android NDK：`27.2.12479018`

Qt 安装目录统一为：

- `C:\Qt`

## 二、前提条件

电脑上需要提前准备好：

- Python
- Android SDK
- Android NDK
- JDK

本流程主要负责安装 Qt 本体。

## 三、安装 aqtinstall

打开 PowerShell，执行：

```powershell
pip install -U aqtinstall
```

如果默认 `pip` 下载太慢，可以先切到清华源：

```powershell
python -m pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple
pip install -U aqtinstall
```

## 四、生成 aqt 配置文件

项目验证可用的镜像为南京大学镜像：

- `https://mirror.nju.edu.cn/qt`

在 PowerShell 里执行：

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

执行完成后，会在临时目录生成配置文件：

- `%TEMP%\aqt-nju.ini`

## 五、安装桌面版 Qt

执行：

```powershell
python -m aqt -c "$env:TEMP\aqt-nju.ini" install-qt -O C:\Qt -b https://mirror.nju.edu.cn/qt --UNSAFE-ignore-hash windows desktop 6.10.3 win64_mingw
```

安装完成后，桌面版目录应当为：

- `C:\Qt\6.10.3\mingw_64`

## 六、安装 Android 版 Qt

执行：

```powershell
python -m aqt -c "$env:TEMP\aqt-nju.ini" install-qt -O C:\Qt -b https://mirror.nju.edu.cn/qt --UNSAFE-ignore-hash --autodesktop all_os android 6.10.3 android_arm64_v8a
```

安装完成后，Android 版目录应当为：

- `C:\Qt\6.10.3\android_arm64_v8a`

## 七、安装完成后检查

执行：

```powershell
Get-ChildItem C:\Qt\6.10.3
```

正常至少应看到：

- `mingw_64`
- `android_arm64_v8a`

还可以继续检查桌面版工具：

```powershell
Get-ChildItem C:\Qt\6.10.3\mingw_64\bin
```

至少应有：

- `qmake.exe`
- `qt-cmake.bat`
- `androiddeployqt.exe`

## 八、Qt Creator 后续配置

如果后面需要用 Qt Creator，建议统一配置以下路径：

- Qt 桌面版：`C:\Qt\6.10.3\mingw_64`
- Qt Android 版：`C:\Qt\6.10.3\android_arm64_v8a`
- Android SDK：`C:\Users\你的用户名\AppData\Local\Android\Sdk`
- Android NDK：`C:\Users\你的用户名\AppData\Local\Android\Sdk\ndk\27.2.12479018`
- JDK：本机安装的 `JDK 21`

## 九、注意事项

- 不要自行切换到 `Qt 6.11.x` 或 `Qt 6.12.x`
- 不要改成 `MSVC`
- 不要混用不同版本的 Android NDK
- 项目当前统一只要求 `arm64-v8a`

## 十、最简执行版

组员只需要按顺序执行下面三步。

### 1. 安装 aqtinstall

```powershell
pip install -U aqtinstall
```

### 2. 生成配置文件

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

### 3. 安装 Qt

```powershell
python -m aqt -c "$env:TEMP\aqt-nju.ini" install-qt -O C:\Qt -b https://mirror.nju.edu.cn/qt --UNSAFE-ignore-hash windows desktop 6.10.3 win64_mingw
python -m aqt -c "$env:TEMP\aqt-nju.ini" install-qt -O C:\Qt -b https://mirror.nju.edu.cn/qt --UNSAFE-ignore-hash --autodesktop all_os android 6.10.3 android_arm64_v8a
```
