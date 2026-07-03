# B 模块：局域网通信设计初稿

本文档面向 `B：局域网通信` 负责人，用于统一第一阶段网络模块的开发边界、接口形态和验收标准。

当前项目仍是 MVP 骨架，`src/network/NetworkManager` 还没有实际逻辑。B 模块第一阶段的目标不是做完整联网平台，而是先给房间大厅和五子棋同步提供一条稳定的局域网消息通道。

## 1. 模块定位

B 模块负责目录：

- `src/network/`

核心职责：

- 房主启动局域网监听。
- 客户端通过 IP 和端口连接房主。
- 双方可靠收发 JSON 消息。
- 向上层报告连接状态、断开状态和错误。
- 为后续服务器中继模式预留接口，但第一阶段只实现局域网直连。

不负责的内容：

- 不判断玩家是否能准备。
- 不判断房主是否能开始游戏。
- 不判断五子棋落子是否合法。
- 不判断胜负。
- 不维护最终房间状态。

这些业务分别属于：

- `src/lobby/`：房间、玩家、准备状态。
- `src/game/`：棋盘、回合、胜负判断。
- `src/app/`：QML、房间、网络、游戏之间的总协调。

## 2. 第一阶段目标

第一阶段只做最小可演示闭环：

1. 房主调用 `startHost()` 后监听默认端口。
2. 客户端输入房主 IP 后调用 `connectToHost()`。
3. 客户端能发送 `join_room`。
4. 房主能收到并上报给大厅模块。
5. 房主能广播 `room_state`、`start_game`、`place_piece` 等消息。
6. 客户端能收到广播并上报给上层。
7. 任意一方断开时，另一方能得到断开通知。

第一阶段先不做：

- 自动搜索局域网房间。
- NAT 穿透。
- 云服务器登录、匹配、转发。
- 加密和账号认证。
- 断线重连后的完整状态恢复。

## 3. 技术方案

使用 Qt 自带网络模块：

- `QTcpServer`：房主监听和接受客户端连接。
- `QTcpSocket`：客户端连接房主，以及所有 TCP 消息收发。
- `QJsonObject` / `QJsonDocument`：统一消息体。
- `QHostAddress` / `QNetworkInterface`：获取本机局域网 IP。

需要在 `CMakeLists.txt` 中引入 `Network`：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network)

target_link_libraries(appLanBoard
    PRIVATE Qt6::Quick
            Qt6::QuickControls2
            Qt6::Network
)
```

Android 端后续还需要确认 Manifest 中有网络权限：

```xml
<uses-permission android:name="android.permission.INTERNET" />
```

## 4. 推荐架构

```text
QML / AppController
        |
        v
NetworkManager
        |
        v
Qt Network: QTcpServer / QTcpSocket
        |
        v
Windows / Android
```

`NetworkManager` 是网络层唯一对外入口。上层不要直接操作 `QTcpSocket`，否则后续很难统一错误处理和消息协议。

## 5. 推荐类职责

第一版可以只实现一个 `NetworkManager`，内部拆小函数即可：

- 监听管理：启动监听、停止监听、接受连接。
- 连接管理：连接房主、断开连接、清理 socket。
- 消息编码：把 JSON 消息编码为 TCP 字节流。
- 消息解码：从 TCP 字节流中按帧解析 JSON。
- 状态上报：通过 signal 通知上层。
- 地址工具：列出本机可用 IPv4 地址。

后续如果代码变复杂，再考虑拆出：

- `MessageCodec`
- `PeerConnection`
- `LanDiscovery`

MVP 阶段不建议一开始拆太多类。

## 6. 推荐对外接口

建议 `NetworkManager` 先暴露这些接口：

```cpp
Q_INVOKABLE bool startHost(quint16 port = 45454);
Q_INVOKABLE void stopHost();

Q_INVOKABLE void connectToHost(const QString &address, quint16 port = 45454);
Q_INVOKABLE void disconnectFromHost();

Q_INVOKABLE bool sendMessage(const QString &type, const QJsonObject &payload = {});
Q_INVOKABLE bool broadcastMessage(const QString &type, const QJsonObject &payload = {});

Q_INVOKABLE QStringList localAddresses() const;
```

如果后续需要从 QML 直接传 `payload`，也可以把公开接口改成 `QVariantMap`，在 C++ 内部再转成 `QJsonObject`。如果统一由 `AppController` 调用网络层，则可以继续使用 `QJsonObject`。

接口含义：

- `startHost`：房主开始监听。成功返回 `true`，失败通过 `errorOccurred` 报告原因。
- `stopHost`：关闭监听，断开所有客户端。
- `connectToHost`：客户端连接房主。
- `disconnectFromHost`：主动断开当前连接。
- `sendMessage`：客户端发给房主；房主也可以用于给单个默认连接发送。
- `broadcastMessage`：房主广播给所有客户端。
- `localAddresses`：返回本机局域网 IPv4，方便房主展示给客户端输入。

## 7. 推荐信号

建议先定义这些 signal：

```cpp
void hostStarted(quint16 port);
void hostStopped();

void connected();
void disconnected();

void clientConnected(QString peerAddress);
void clientDisconnected(QString peerAddress);

void messageReceived(QString type, QJsonObject payload);
void errorOccurred(QString message);
```

如果后续要支持多个客户端，`messageReceived` 可以扩展为：

```cpp
void messageReceived(QString peerId, QString type, QJsonObject payload);
```

第一阶段如果只做一房两人，可以先不引入 `peerId`，但内部最好为多个 socket 留空间。

## 8. 连接角色

网络层建议维护一个当前角色：

```cpp
enum class NetworkRole {
    None,
    Host,
    Client
};
```

角色规则：

- `None`：未监听，也未连接。
- `Host`：当前实例是房主，持有 `QTcpServer` 和客户端 socket 列表。
- `Client`：当前实例是客户端，持有一个连接房主的 `QTcpSocket`。

第一阶段不允许同一个实例同时是 `Host` 和 `Client`。

## 9. TCP 消息边界

TCP 是字节流，不保证一次 `write()` 对应一次 `readyRead()`。因此不能裸发 JSON 字符串。

统一使用：

```text
4 字节长度前缀 + UTF-8 JSON 内容
```

约定：

- 长度前缀为无符号 32 位整数。
- 使用网络字节序，也就是 big-endian。
- 长度表示后续 JSON 内容的字节数，不包含 4 字节前缀。
- 单条消息建议限制为 1 MiB 以内，防止异常数据撑爆内存。

详细协议见 `PROTOCOL.md`。

## 10. 与其他模块的对接

### A 模块：界面与交互

A 模块不直接操作 socket。它可以通过 `AppController` 间接触发：

- 创建房间。
- 加入房间。
- 发送准备状态。
- 显示连接错误。
- 显示本机 IP。

### C 模块：房间大厅逻辑

C 模块负责生成和消费房间类消息：

- `join_room`
- `player_ready`
- `room_state`
- `start_game`

B 模块只传输，不判断业务合法性。

### D 模块：游戏逻辑与集成

D 模块负责生成和消费游戏类消息：

- `place_piece`
- `game_over`

B 模块只保证消息到达上层，不判断棋盘规则。

## 11. 开发顺序

建议按下面顺序开发：

1. 修改 CMake，引入 `Qt6::Network`。
2. 扩展 `NetworkManager` 头文件，先定接口和 signal。
3. 实现 `startHost()` 和 `connectToHost()`。
4. 实现 socket 断开和错误处理。
5. 实现长度前缀 JSON 编码。
6. 实现长度前缀 JSON 解码。
7. 实现 `sendMessage()` 和 `broadcastMessage()`。
8. 实现 `localAddresses()`。
9. 写一个临时 QML 或 C++ 调试入口，验证双实例通信。
10. 再接入房间和游戏模块。

## 12. 调试方式

推荐先在 Windows 桌面端调试：

1. 启动第一个程序实例，调用 `startHost(45454)`。
2. 启动第二个程序实例，调用 `connectToHost("127.0.0.1", 45454)`。
3. 客户端发送一条 `join_room`。
4. 房主收到后打印或显示消息。
5. 房主广播一条 `room_state`。
6. 客户端收到后打印或显示消息。

本机双实例跑通后，再做两台设备同 Wi-Fi 测试：

- 房主显示 `localAddresses()` 返回的局域网 IP。
- 客户端输入该 IP 连接。
- 如果连接失败，先检查防火墙、同一 Wi-Fi、路由器 AP 隔离、校园网隔离。

## 13. 移动端注意事项

Android 真机测试时重点看：

- 手机和房主必须在同一局域网。
- 热点、校园网、公司网可能禁止设备互访。
- Android 作为客户端通常更稳定。
- Android 作为房主理论可行，但更容易受网络环境和省电策略影响。
- Debug 阶段优先让 Windows 桌面端做房主，Android 手机做客户端。

## 14. 验收标准

B 模块第一阶段合格标准：

- 桌面双实例可以连接。
- 两台局域网设备可以连接。
- 客户端能向房主发送 JSON 消息。
- 房主能向客户端广播 JSON 消息。
- 能处理连接失败、端口占用、对端断开。
- 不因半包、粘包导致 JSON 解析错误。
- `PROTOCOL.md` 中第一阶段消息都能被网络层传输。

## 15. 当前待办清单

- [ ] `CMakeLists.txt` 增加 `Qt6::Network`。
- [ ] `NetworkManager` 增加对外接口和 signal。
- [ ] 实现 TCP host/client。
- [ ] 实现长度前缀 JSON codec。
- [ ] 实现本机 IP 查询。
- [ ] 增加临时调试入口。
- [ ] Windows 双实例验证。
- [ ] Android 真机连接验证。
