# LanBoard ECS 部署流程

本文整理当前项目的 `ECS / Ubuntu` 在线服务端部署流程，目标是把 `lanboardServer` 部署到公网服务器上，为 App 的“在线联机”提供大厅与房间服务。

当前项目约定：

- ECS 公网地址示例：`47.105.54.227`
- 在线服务默认端口：`44567`
- 底层协议：`ENet / UDP`
- 局域网发现仍然只用于同一局域网，不走 ECS

## 1. 服务端作用

当前 ECS 服务端负责：

- 在线大厅连接
- 在线房间列表
- 在线房间创建 / 加入 / 离开
- 房间状态同步
- 在线 Survivor 对战所需的服务端链路

它和局域网开房不是一套入口：

- `局域网模式`：玩家设备自己开房，靠局域网 UDP 广播发现
- `在线模式`：所有人连到 ECS 上的 `lanboardServer`

## 2. 服务器前置条件

推荐环境：

- Ubuntu 22.04 / 24.04
- 具有公网 IP 的阿里云 ECS
- 已放行 `44567/udp`

阿里云侧至少要确认：

- 安全组已放行 `44567/udp`
- 如果开了系统防火墙，Ubuntu 本机也要放行 `44567/udp`

## 3. Ubuntu 安装依赖

先登录服务器：

```bash
ssh root@47.105.54.227
```

安装基础依赖：

```bash
apt update
apt install -y git build-essential cmake ninja-build pkg-config qt6-base-dev
```

说明：

- 当前独立服务端只需要 Qt6 Core / Gui / Network 对应开发包
- `ENet` 由项目的 `FetchContent` 在 CMake 配置阶段自动拉取

## 4. 拉取代码

推荐把服务端部署在单独目录，例如：

```bash
mkdir -p /root/lanboard-deploy
cd /root/lanboard-deploy
git clone https://github.com/lhxll07/LanBoard.git .
git checkout main
```

如果目录里已经有仓库，增量更新即可：

```bash
cd /root/lanboard-deploy
git fetch origin
git checkout main
git pull --ff-only origin main
```

## 5. 构建独立服务端

只构建服务端，不构建桌面 App：

```bash
cd /root/lanboard-deploy
cmake -S . -B build-server -G Ninja -DLANBOARD_BUILD_APP=OFF -DLANBOARD_BUILD_SERVER=ON
cmake --build build-server --parallel
```

构建完成后，产物一般在：

```text
/root/lanboard-deploy/build-server/lanboardServer
```

也可以手动验证一下：

```bash
/root/lanboard-deploy/build-server/lanboardServer --port 44567
```

看到类似下面的日志，说明监听成功：

```text
LanBoard ENet server listening on port 44567
```

## 6. 配置 systemd 常驻运行

创建服务文件：

```bash
cat >/etc/systemd/system/lanboard.service <<'EOF'
[Unit]
Description=LanBoard Dedicated Server
After=network.target

[Service]
Type=simple
WorkingDirectory=/root/lanboard-deploy
ExecStart=/root/lanboard-deploy/build-server/lanboardServer --port 44567
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF
```

重载并启动：

```bash
systemctl daemon-reload
systemctl enable lanboard
systemctl restart lanboard
```

检查状态：

```bash
systemctl status lanboard --no-pager
journalctl -u lanboard -n 80 --no-pager
```

## 7. 放行端口

如果 Ubuntu 开启了 `ufw`，再执行：

```bash
ufw allow 44567/udp
ufw status
```

同时确认阿里云安全组也已经放行：

- 协议：`UDP`
- 端口：`44567`
- 来源：演示阶段可先放 `0.0.0.0/0`，正式使用应按需收紧

## 8. 客户端如何连接

App 设置页里填：

- 在线服务器 Host：`47.105.54.227`
- 在线服务器 Port：`44567`

然后在联机页切到“在线联机”，客户端就会去拉 ECS 上的在线房间列表。

## 9. 增量部署流程

后续代码更新后，不需要重装整台服务器，直接：

```bash
cd /root/lanboard-deploy
git fetch origin
git checkout main
git pull --ff-only origin main
cmake --build build-server --parallel
systemctl restart lanboard
systemctl status lanboard --no-pager
```

如果这次改动涉及 CMake 选项、Qt 依赖或服务端源码结构变化，建议补一次重新配置：

```bash
cd /root/lanboard-deploy
cmake -S . -B build-server -G Ninja -DLANBOARD_BUILD_APP=OFF -DLANBOARD_BUILD_SERVER=ON
cmake --build build-server --parallel
systemctl restart lanboard
```

## 10. 常用排障命令

看服务是否在跑：

```bash
systemctl status lanboard --no-pager
```

看最近日志：

```bash
journalctl -u lanboard -n 120 --no-pager
```

看端口是否已经监听：

```bash
ss -lunp | grep 44567
```

看进程：

```bash
ps -ef | grep lanboardServer
```

## 11. 常见问题

### 1. 客户端连不上 ECS

优先检查：

- App 里的在线服务器 Host / Port 是否正确
- `lanboard.service` 是否正常运行
- `44567/udp` 是否同时在阿里云安全组和 Ubuntu 防火墙放行

### 2. 服务端构建失败

优先检查：

- `qt6-base-dev` 是否已安装
- CMake 是否重新配置过
- 服务器是否能正常访问 GitHub 拉取 `ENet`

### 3. 在线房间列表为空

优先检查：

- 是否已经连到了 ECS
- 是否真的创建了在线房间
- 服务端日志里是否出现创建房间或连接异常

## 12. 备注

- 当前在线服务与局域网开房是并存关系，不互相替代
- 局域网扫描只能发现本地网段房间，发现不到 ECS 房间
- 如需更换默认公网服地址，可改 `src/app/appcontroller.h` 中的默认 `Host / Port`
