# LanBoard 局域网发现

## 职责边界

- `NetworkManager` 负责 ENet 连接、消息收发和向上层分发。
- `RoomDiscoveryService` 负责 UDP 查询、房间广播、缓存和过期清理。
- `networkaddressutils` 负责 IPv4 网卡枚举、子网判断和端点优先级。

房间规则和游戏规则不进入发现服务。

## 房间身份与多网卡

每次应用启动会为本机房间发布者生成一个 `roomUid`。同一房间从 Wi-Fi、以太网、VPN 等多个地址到达时，接收端按 `roomUid` 聚合为一个房间，并保留多个候选端点。界面仍只显示一个房间，连接时优先使用与本机物理网卡同子网的端点。

端点分别维护最后发现时间。单个网卡地址停止广播时只移除该端点；房间的全部端点都过期后才移除房间。

## 广播字段

`room_announce` 在原有字段之外必须包含：

```json
{
    "roomUid": "application-session-uuid",
    "hostIp": "192.168.1.20",
    "port": 44567
}
```

同一个 `roomUid` 可以从多个 `hostIp` 发布。发布者会忽略自己的 `roomUid`，避免把本机房间加入发现列表。

## 协议要求

- `roomUid` 必须是非空字符串。
- 缺少 `roomUid` 或值为空白的公告直接丢弃。
- 不保留无 `roomUid` 的旧版发现协议兼容。

## 验证

桌面构建后运行：

```powershell
ctest --test-dir build-network-clean --output-on-failure
```

手动测试至少包含：双实例发现与连接、同一 Wi-Fi 两台设备互联，以及启用 VPN 或多网卡时同一房间只显示一次。
