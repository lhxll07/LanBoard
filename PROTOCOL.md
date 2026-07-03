# LanBoard 网络协议初稿

本文档定义 LanBoard 第一阶段局域网联机使用的消息协议。协议服务于 MVP 闭环：创建房间、加入房间、准备、开始游戏、五子棋落子同步和胜负结果同步。

协议目标：

- 简单，方便 QML/C++ 调试。
- 跨平台，Windows 和 Android 共用。
- 可扩展，后续可接服务器中继模式。
- 明确消息边界，避免 TCP 粘包和半包问题。

## 1. 传输层

第一阶段使用 TCP：

- 默认端口：`45454`
- 房主：`QTcpServer` 监听端口。
- 客户端：`QTcpSocket` 连接房主 IP。
- 编码：UTF-8 JSON。

TCP 帧格式：

```text
+----------------------+----------------------+
| 4 bytes body_length  | JSON body bytes      |
+----------------------+----------------------+
```

字段说明：

- `body_length`：无符号 32 位整数，big-endian。
- `body_length` 表示 JSON body 的字节数，不包含 4 字节长度前缀。
- JSON body 必须是 UTF-8 编码。
- 单条消息建议最大 1 MiB。

## 2. JSON 信封格式

所有消息统一使用下面的 JSON 信封：

```json
{
  "version": 1,
  "type": "join_room",
  "requestId": "req_001",
  "roomId": "room_001",
  "senderId": "player_001",
  "payload": {}
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `version` | number | 是 | 协议版本。第一版固定为 `1`。 |
| `type` | string | 是 | 消息类型。 |
| `payload` | object | 是 | 业务数据。没有数据时使用 `{}`。 |
| `requestId` | string | 否 | 请求 ID，用于日志、调试和后续 ack。 |
| `roomId` | string | 否 | 房间 ID。第一阶段可以由房间模块生成。 |
| `senderId` | string | 否 | 发送者玩家 ID。 |

网络层最低校验：

- JSON 必须能解析为 object。
- `version` 必须存在且为 `1`。
- `type` 必须是非空字符串。
- `payload` 必须是 object。

业务字段是否合理由 C/D 模块判断。

## 3. 消息方向

方向约定：

- `client -> host`：客户端发给房主。
- `host -> client`：房主发给单个客户端。
- `host -> all`：房主广播给所有客户端。

第一阶段推荐由房主作为状态权威端：

- 客户端把操作发给房主。
- 房主交给 C/D 模块判断。
- 房主广播新的房间状态或游戏状态。

## 4. 第一阶段消息类型

项目任务分工中已约定第一版消息类型：

- `create_room`
- `join_room`
- `player_ready`
- `start_game`
- `place_piece`
- `game_over`

为了联调更顺畅，协议初稿补充两个状态类消息：

- `room_state`
- `error`

`room_state` 用于房主同步房间完整状态；`error` 用于返回可展示的错误。

## 5. 房间消息

### 5.1 create_room

用途：创建房间。第一阶段通常是房主本地触发，若后续接服务器中继，也可以作为网络消息发送。

方向：

- 本地：QML/AppController -> RoomManager/NetworkManager
- 预留：client -> relay server

payload：

```json
{
  "hostName": "Alice",
  "gameType": "gomoku"
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `hostName` | string | 是 | 房主显示名。 |
| `gameType` | string | 是 | 游戏类型。第一阶段使用 `gomoku`。 |

### 5.2 join_room

用途：客户端请求加入房间。

方向：

- `client -> host`

payload：

```json
{
  "playerName": "Bob"
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playerName` | string | 是 | 玩家显示名。 |

房主收到后，由 C 模块决定是否允许加入。允许后建议广播 `room_state`。

### 5.3 player_ready

用途：玩家切换准备状态。

方向：

- `client -> host`
- `host -> all`

payload：

```json
{
  "playerId": "player_002",
  "isReady": true
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playerId` | string | 是 | 玩家 ID。 |
| `isReady` | bool | 是 | 是否准备。 |

建议流程：

1. 客户端发送 `player_ready` 给房主。
2. 房主交给 C 模块更新状态。
3. 房主广播 `room_state`，而不是只广播单个 ready 事件。

### 5.4 room_state

用途：同步完整房间状态。

方向：

- `host -> all`
- `host -> client`

payload：

```json
{
  "roomId": "room_001",
  "hostId": "player_001",
  "hostName": "Alice",
  "isStarted": false,
  "players": [
    {
      "playerId": "player_001",
      "playerName": "Alice",
      "isHost": true,
      "isReady": true
    },
    {
      "playerId": "player_002",
      "playerName": "Bob",
      "isHost": false,
      "isReady": false
    }
  ]
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `roomId` | string | 是 | 房间 ID。 |
| `hostId` | string | 是 | 房主玩家 ID。 |
| `hostName` | string | 是 | 房主显示名。 |
| `isStarted` | bool | 是 | 游戏是否已经开始。 |
| `players` | array | 是 | 玩家列表。 |

玩家对象字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playerId` | string | 是 | 玩家 ID。 |
| `playerName` | string | 是 | 玩家显示名。 |
| `isHost` | bool | 是 | 是否房主。 |
| `isReady` | bool | 是 | 是否准备。 |

### 5.5 start_game

用途：房主开始游戏。

方向：

- `client -> host`：如果客户端误触发，房主应拒绝。
- `host -> all`：房主通知所有人进入游戏。

payload：

```json
{
  "gameType": "gomoku",
  "firstPlayerId": "player_001",
  "boardSize": 15
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `gameType` | string | 是 | 第一阶段固定为 `gomoku`。 |
| `firstPlayerId` | string | 是 | 先手玩家 ID。 |
| `boardSize` | number | 是 | 棋盘大小，五子棋第一阶段建议 `15`。 |

## 6. 游戏消息

### 6.1 place_piece

用途：同步一次落子。

方向：

- `client -> host`
- `host -> all`

payload：

```json
{
  "playerId": "player_001",
  "x": 7,
  "y": 8,
  "piece": "black",
  "turn": 12
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playerId` | string | 是 | 落子玩家 ID。 |
| `x` | number | 是 | 棋盘 x 坐标，从 `0` 开始。 |
| `y` | number | 是 | 棋盘 y 坐标，从 `0` 开始。 |
| `piece` | string | 是 | `black` 或 `white`。 |
| `turn` | number | 是 | 回合序号，从 `1` 开始递增。 |

建议流程：

1. 玩家本地点击棋盘。
2. D 模块生成 `place_piece` 请求。
3. 客户端发送给房主。
4. 房主 D 模块判断是否合法。
5. 合法则房主广播 `place_piece`。
6. 不合法则房主返回 `error` 或重新广播权威游戏状态。

### 6.2 game_over

用途：同步游戏结束结果。

方向：

- `host -> all`

payload：

```json
{
  "winnerId": "player_001",
  "reason": "five_in_row",
  "winningLine": [
    { "x": 5, "y": 8 },
    { "x": 6, "y": 8 },
    { "x": 7, "y": 8 },
    { "x": 8, "y": 8 },
    { "x": 9, "y": 8 }
  ]
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `winnerId` | string/null | 是 | 胜者 ID。平局时为 `null`。 |
| `reason` | string | 是 | 结束原因，例如 `five_in_row`、`draw`、`player_left`。 |
| `winningLine` | array | 否 | 获胜五连坐标。 |

## 7. 错误消息

### 7.1 error

用途：传递业务错误或协议错误。

方向：

- `host -> client`
- `host -> all`

payload：

```json
{
  "code": "room_full",
  "message": "Room is full."
}
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `code` | string | 是 | 稳定错误码。 |
| `message` | string | 是 | 可展示或可记录的错误说明。 |

第一阶段建议错误码：

| 错误码 | 说明 |
| --- | --- |
| `invalid_message` | 消息格式错误。 |
| `unsupported_version` | 协议版本不支持。 |
| `unknown_type` | 未知消息类型。 |
| `room_full` | 房间已满。 |
| `game_started` | 游戏已经开始，不能加入。 |
| `not_host` | 非房主不能执行该操作。 |
| `invalid_move` | 落子非法。 |

## 8. 示例流程

### 8.1 加入房间

```text
client -> host: join_room
host   -> all : room_state
```

客户端发送：

```json
{
  "version": 1,
  "type": "join_room",
  "requestId": "req_join_001",
  "payload": {
    "playerName": "Bob"
  }
}
```

房主广播：

```json
{
  "version": 1,
  "type": "room_state",
  "roomId": "room_001",
  "senderId": "player_001",
  "payload": {
    "roomId": "room_001",
    "hostId": "player_001",
    "hostName": "Alice",
    "isStarted": false,
    "players": [
      {
        "playerId": "player_001",
        "playerName": "Alice",
        "isHost": true,
        "isReady": true
      },
      {
        "playerId": "player_002",
        "playerName": "Bob",
        "isHost": false,
        "isReady": false
      }
    ]
  }
}
```

### 8.2 准备并开始游戏

```text
client -> host: player_ready
host   -> all : room_state
host   -> all : start_game
```

### 8.3 落子并结束

```text
client -> host: place_piece
host   -> all : place_piece
host   -> all : game_over
```

## 9. 兼容性约定

第一阶段协议版本固定为 `1`。

兼容规则：

- 接收方必须忽略自己不认识的可选字段。
- 接收方遇到未知 `type`，可以发送 `error`。
- 必填字段缺失时，接收方应丢弃消息并上报错误。
- 后续新增消息类型，不改变已有消息字段含义。
- 后续新增 payload 字段，不能破坏旧字段。

## 10. B 模块处理边界

B 模块必须处理：

- TCP 连接和断开。
- TCP 帧拆包和组包。
- JSON 解析。
- 协议信封最低校验。
- 将消息转成 signal 上报。

B 模块不处理：

- 玩家是否存在。
- 房间是否满员。
- 谁是房主。
- 是否所有人已准备。
- 落子是否合法。
- 谁赢了。

这些判断由 C/D 模块完成。

## 11. 后续可扩展项

第一阶段完成后，可以再考虑：

- `ping` / `pong` 心跳。
- `ack` / `requestId` 确认机制。
- 局域网房间自动发现。
- 断线重连。
- 房主迁移。
- 服务器中继模式。
- 消息签名或加密。
