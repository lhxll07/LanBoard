# 网络消息表

本文档记录当前主线网络消息。新增联网功能前，应先补充或更新本表。

## 分类说明

- 局域网发现：UDP 广播或单播，用于发现同一局域网中的房间。
- 局域网房间：客户端与主机之间的 ENet JSON 消息。
- 在线房间：客户端与专用服务器之间的 ENet JSON 消息。
- 游戏同步：具体游戏过程中的 ENet JSON 消息或 Survivor 二进制包。
- 通用错误：任意 ENet JSON 连接上的错误反馈。

## 传输约定

- `discover_room` 和 `room_announce` 由 `RoomDiscoveryService` 通过 UDP 发现端口发送。
- 表中带 `type` 字段的业务消息走 ENet channel 0，使用可靠 JSON 包。
- `NetworkManager` 是 QML 和 `AppController` 使用的网络门面；ENet JSON 编解码由 `enetutils.*` 承担，主要消息 type 常量由 `protocolids.h` 维护。
- `Survivor` 实时战斗额外使用 ENet 二进制包：输入和快照走独立 channel，HUD/升级/宝箱交互使用可靠包。
- `LineJsonProtocol` 是早期逐行 JSON 工具，文件仍保留在仓库中，但当前 ENet 主链路不通过它发送或解析业务消息。

## 消息总览

| 消息 type | 分类 | 发送方 | 接收方 | 作用 |
| --- | --- | --- | --- | --- |
| `discover_room` | 局域网发现 | 客户端 | 局域网主机 | 查询可加入房间 |
| `room_announce` | 局域网发现 | 局域网主机 | 客户端 | 发布房间信息 |
| `join` | 局域网房间 | 客户端 | 主机 | 加入局域网主机房间 |
| `create_room` | 在线房间 | 客户端 | 专用服务器 | 创建在线房间 |
| `join_room` | 在线房间 | 客户端 | 专用服务器 | 加入指定在线房间 |
| `list_rooms` | 在线房间 | 客户端 | 专用服务器 | 请求在线房间列表 |
| `rooms_list` | 在线房间 | 专用服务器 | 客户端 | 返回在线房间列表 |
| `room_state` | 房间同步 | 主机或服务器 | 客户端 | 同步房间玩家、座位和游戏信息 |
| `ready` | 房间同步 | 客户端 | 主机或服务器 | 切换准备状态 |
| `change_seat` | 房间同步 | 客户端 | 主机或服务器 | 切换玩家座位类型 |
| `switch_room_game` | 房间同步 | 客户端或房主 | 主机或服务器 | 切换房间游戏类型 |
| `start_game` | 房间同步 | 客户端 | 主机或服务器 | 请求开始游戏 |
| `game_start` | 房间同步 | 主机或服务器 | 客户端 | 通知进入游戏 |
| `place_piece` | 五子棋同步 | 客户端 | 主机或服务器 | 客户端请求落子 |
| `move` | 五子棋同步 | 主机或服务器 | 客户端 | 广播落子结果 |
| `flight_roll` | 飞行棋同步 | 客户端 | 主机或服务器 | 请求掷骰 |
| `flight_roll_result` | 飞行棋同步 | 主机或服务器 | 客户端 | 广播掷骰结果 |
| `flight_move` | 飞行棋同步 | 客户端 | 主机或服务器 | 请求移动飞机 |
| `flight_move_result` | 飞行棋同步 | 主机或服务器 | 客户端 | 广播移动结果 |
| `ddz_play` | 斗地主同步 | 客户端 | 主机或服务器 | 请求出牌 |
| `ddz_pass` | 斗地主同步 | 客户端 | 主机或服务器 | 请求过牌 |
| `ddz_state` | 斗地主同步 | 主机或服务器 | 客户端 | 同步斗地主私有视角状态 |
| `surrender` | 游戏同步 | 客户端 | 主机或服务器 | 请求认输 |
| `game_over` | 游戏同步 | 主机或服务器 | 客户端 | 广播对局结果 |
| `error` | 通用错误 | 主机或服务器 | 客户端 | 返回错误信息 |

## 局域网发现消息

### `discover_room`

用途：客户端扫描局域网房间。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `discover_room` |
| `app` | string | 是 | 固定为 `LanBoard` |
| `version` | number | 是 | 当前为 `1` |

处理要求：

- 通过 UDP 发送到发现端口。
- 主机收到后，如果当前正在监听房间端口，应回复 `room_announce`。

### `room_announce`

用途：主机发布可加入房间信息。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `room_announce` |
| `app` | string | 是 | 固定为 `LanBoard` |
| `version` | number | 是 | 当前为 `1` |
| `roomUid` | string | 建议 | 房间唯一标识 |
| `hostName` | string | 是 | 房主昵称或房间名 |
| `hostIp` | string | 是 | 客户端可连接的主机 IPv4 |
| `port` | number | 是 | ENet 房间端口 |
| `playerCount` | number | 是 | 当前房间人数 |
| `roomCapacity` | number | 是 | 房间总容量 |
| `maxPlayers` | number | 是 | 当前游戏实际参与人数 |
| `gameId` | string | 是 | `gomoku`、`doudizhu`、`flightchess` 或 `survivor` |
| `gameName` | string | 是 | 展示用游戏名 |
| `inGame` | boolean | 是 | 是否正在游戏中 |
| `isFull` | boolean | 是 | 是否已满 |

处理要求：

- 客户端应按 `roomUid` 优先去重，旧数据可按 `hostIp + port` 去重。
- 客户端应过滤本机自己发布的房间。
- 房间列表应优先显示未满、未开局、最近更新的房间。

## 房间和在线大厅消息

### `join`

用途：加入局域网主机房间。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `join` |
| `name` | string | 是 | 玩家昵称 |
| `gameId` | string | 否 | 客户端期望加入的游戏 |

### `create_room`

用途：在专用服务器上创建在线房间。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `create_room` |
| `name` | string | 是 | 玩家昵称 |
| `gameId` | string | 是 | 房间游戏类型 |
| `roomName` | string | 否 | 房间名 |

### `join_room`

用途：加入专用服务器上的指定房间。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `join_room` |
| `name` | string | 是 | 玩家昵称 |
| `roomId` | string | 是 | 在线房间 ID |

### `list_rooms`

用途：请求在线房间列表。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `list_rooms` |

### `rooms_list`

用途：返回在线房间列表。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `rooms_list` |
| `rooms` | array | 是 | 在线房间数组 |

房间条目常用字段：

- `roomId`
- `roomName`
- `hostName`
- `gameId`
- `gameName`
- `playerCount`
- `roomCapacity`
- `maxPlayers`
- `inGame`
- `isFull`

### `room_state`

用途：同步房间状态。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `room_state` |
| `gameId` | string | 是 | 当前房间游戏 |
| `gameName` | string | 建议 | 当前游戏展示名 |
| `roomCapacity` | number | 是 | 房间容量 |
| `maxPlayers` | number | 是 | 当前游戏实际玩家数 |
| `players` | array | 是 | 玩家列表 |
| `yourPlayerId` | number | 部分场景 | 接收方自己的玩家 ID |

玩家条目字段：

- `playerId`
- `name`
- `isHost`
- `isReady`
- `seatType`

### `ready`

用途：客户端切换准备状态。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `ready` |
| `ready` | boolean | 是 | 是否准备 |

### `change_seat`

用途：客户端请求切换座位类型。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `change_seat` |
| `seatType` | string | 是 | `active` 或 `spectator` |

### `switch_room_game`

用途：切换房间游戏类型。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `switch_room_game` |
| `gameId` | string | 是 | `gomoku`、`doudizhu` 或 `flightchess` |

### `start_game`

用途：客户端请求开始游戏。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `start_game` |

### `game_start`

用途：主机或服务器通知客户端进入游戏。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `game_start` |
| `gameId` | string | 建议 | 当前开始的游戏类型 |

## 游戏同步消息

### `place_piece`

用途：五子棋客户端请求落子。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `place_piece` |
| `row` | number | 是 | 行 |
| `col` | number | 是 | 列 |

### `move`

用途：广播五子棋落子结果。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `move` |
| `player` | number | 是 | 棋子或玩家编号 |
| `row` | number | 是 | 行 |
| `col` | number | 是 | 列 |

### `flight_roll`

用途：飞行棋客户端请求掷骰。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `flight_roll` |

### `flight_roll_result`

用途：广播飞行棋掷骰结果。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `flight_roll_result` |
| `player` | number | 是 | 玩家编号 |
| `diceValue` | number | 是 | 骰子点数 |

### `flight_move`

用途：飞行棋客户端请求移动飞机。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `flight_move` |
| `planeIndex` | number | 是 | 飞机索引 |

### `flight_move_result`

用途：广播飞行棋移动结果。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `flight_move_result` |
| `player` | number | 是 | 玩家编号 |
| `planeIndex` | number | 是 | 飞机索引 |

### `ddz_play`

用途：斗地主客户端请求出牌。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `ddz_play` |
| `cards` | array | 是 | 出牌 ID 列表 |

### `ddz_pass`

用途：斗地主客户端请求过牌。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `ddz_pass` |

### `ddz_state`

用途：同步斗地主状态。该消息可能包含玩家私有视角数据，不应盲目广播同一份内容给所有玩家。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `ddz_state` |
| 其他字段 | object | 是 | 由 `DouDiZhuController::stateForPlayer()` 决定 |

### `surrender`

用途：客户端请求认输。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `surrender` |

### `game_over`

用途：广播游戏结果。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `game_over` |
| `winner` | number | 是 | 获胜玩家或阵营 |

## 错误消息

### `error`

用途：返回错误信息。

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 固定为 `error` |
| `message` | string | 是 | 错误说明或错误码 |

当前常见错误：

- `Network error`
- `房间已满`
- `invalid_ddz_play`
- `invalid_ddz_pass`

## 维护规则

- 新增消息时，必须先补充消息表。
- 改字段时，必须同步更新发送方和接收方。
- 如果消息只服务在线服务器，必须标注为在线房间消息。
- 如果消息只服务局域网发现，必须标注为局域网发现消息。
- 斗地主状态消息涉及私有手牌视角，不能默认当作全员同包广播。
