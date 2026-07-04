import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "onlinePage"
    property int lobbyMode: 0

    property bool networkRoom: AppCtrl.networkManager.isHost
                            || AppCtrl.networkManager.isConnected
    property bool inRoom: networkRoom
    property string joinErrorText: ""

    function syncDiscoveryState() {
        if (visible && !inRoom && lobbyMode === 0) {
            AppCtrl.networkManager.startRoomDiscovery()
            AppCtrl.networkManager.refreshRoomDiscovery()
        } else {
            AppCtrl.networkManager.stopRoomDiscovery()
        }
    }

    function joinCurrentRoom() {
        var ip = ipInput.text.trim()
        var port = parseInt(portInput.text.trim(), 10)

        if (ip.length === 0) {
            joinErrorText = "请输入主机 IP 地址"
            return
        }
        if (!(port >= 1 && port <= 65535)) {
            joinErrorText = "请输入 1 - 65535 的端口"
            return
        }

        joinErrorText = ""
        AppCtrl.joinRoom(ip, port, AppCtrl.nickname)
    }

    function joinDiscoveredRoom(room) {
        joinErrorText = ""
        AppCtrl.joinRoom(room.hostIp, room.port, AppCtrl.nickname, room.gameId || "gomoku")
    }

    function joinOnlineRoom(gameId) {
        joinErrorText = ""
        AppCtrl.joinOnlineServer(gameId || "gomoku")
    }

    function isOnlineServerConnection() {
        return AppCtrl.networkManager.isConnected
            && AppCtrl.networkManager.connectedIp === AppCtrl.onlineServerHost
            && AppCtrl.networkManager.connectedPort === AppCtrl.onlineServerPort
    }

    function localPlayer() {
        var idx = AppCtrl.roomManager.localPlayerIndex
        var players = AppCtrl.roomManager.playerList
        return idx >= 0 && idx < players.length ? players[idx] : null
    }

    function toggleReadyState() {
        var player = localPlayer()
        if (!player)
            return

        if (networkRoom && !AppCtrl.networkManager.isHost) {
            AppCtrl.networkManager.sendReady(!player.isReady)
        } else {
            AppCtrl.toggleLocalReady()
        }
    }

    background: Rectangle {
        color: "transparent"
    }

    Component.onCompleted: syncDiscoveryState()
    onVisibleChanged: syncDiscoveryState()
    onLobbyModeChanged: syncDiscoveryState()
    onInRoomChanged: {
        if (inRoom) {
            AppCtrl.networkManager.stopRoomDiscovery()
            AppCtrl.networkManager.clearDiscoveredRooms()
        } else if (visible) {
            AppCtrl.networkManager.clearDiscoveredRooms()
            syncDiscoveryState()
        }
    }

    Connections {
        target: AppCtrl.networkManager
        function onErrorOccurred(message) {
            if (!root.inRoom)
                root.joinErrorText = message
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + AppTheme.pageBottomInset
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            width: AppTheme.contentWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: AppTheme.pageTopInset
            spacing: 18

            PageHeader {
                titleText: root.inRoom ? "房间" : "联机游戏"
                subtitleText: root.inRoom
                    ? (root.isOnlineServerConnection()
                        ? "当前已进入 ECS 在线演示房间。"
                        : "房间状态只在加入或创建房间后显示。")
                    : (root.lobbyMode === 0
                        ? "自动发现局域网房间，也可以手动输入地址加入。"
                        : "连接 ECS 演示服务器，直接进入在线联机大厅。")
                trailingText: root.inRoom
                    ? (AppCtrl.roomManager.playerList.length + " / " + AppCtrl.roomManager.maxPlayers)
                    : ""
            }

            Rectangle {
                width: parent.width
                height: 56
                radius: 28
                color: "#ECE4D8"
                visible: !root.inRoom

                Rectangle {
                    width: (parent.width - 8) / 2
                    height: 40
                    x: root.lobbyMode === 0 ? 4 : parent.width / 2
                    y: 8
                    radius: 20
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#203E36" }
                        GradientStop { position: 1.0; color: "#2F5A4F" }
                    }

                    Behavior on x {
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }
                }

                Row {
                    anchors.fill: parent

                    Item {
                        width: parent.width / 2
                        height: parent.height

                        Text {
                            anchors.centerIn: parent
                            text: "局域网"
                            color: root.lobbyMode === 0 ? "#F4EFE7" : AppTheme.textMuted
                            font.pixelSize: root.lobbyMode === 0 ? 15 : 14
                            font.weight: root.lobbyMode === 0 ? Font.DemiBold : Font.Medium
                        }

                        TapHandler {
                            onTapped: root.lobbyMode = 0
                        }
                    }

                    Item {
                        width: parent.width / 2
                        height: parent.height

                        Text {
                            anchors.centerIn: parent
                            text: "在线"
                            color: root.lobbyMode === 1 ? "#F4EFE7" : AppTheme.textMuted
                            font.pixelSize: root.lobbyMode === 1 ? 15 : 14
                            font.weight: root.lobbyMode === 1 ? Font.DemiBold : Font.Medium
                        }

                        TapHandler {
                            onTapped: root.lobbyMode = 1
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: joinStateColumn.height
                visible: !root.inRoom

                Column {
                    id: joinStateColumn
                    width: parent.width
                    spacing: 18

                    Rectangle {
                        id: discoverCard
                        width: parent.width
                        implicitHeight: discoverLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        transform: Translate { id: discoverCardOffset; y: 20 }
                        visible: root.lobbyMode === 0

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: discoverLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: "局域网可用房间"
                                        color: AppTheme.textPrimary
                                        font.pixelSize: AppTheme.fontSizeHeading
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "自动扫描同一局域网内正在开房的主机。"
                                        color: AppTheme.textSecondary
                                        font.pixelSize: AppTheme.fontSizeBody
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ActionButton {
                                    Layout.preferredWidth: 88
                                    text: "刷新"
                                    secondary: true
                                    onClicked: AppCtrl.networkManager.refreshRoomDiscovery()
                                }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 10

                                Repeater {
                                    model: AppCtrl.networkManager.discoveredRooms

                                    delegate: Rectangle {
                                        width: discoverLayout.width
                                        implicitHeight: roomLayout.implicitHeight + 24
                                        radius: 18
                                        color: AppTheme.cardBackgroundSoft
                                        border.width: 1
                                        border.color: AppTheme.cardBorder

                                        RowLayout {
                                            id: roomLayout
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 12

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 3

                                                Text {
                                                    text: modelData.hostName.length > 0 ? modelData.hostName : "未命名房主"
                                                    color: AppTheme.textPrimary
                                                    font.pixelSize: 15
                                                    font.weight: Font.DemiBold
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: (modelData.gameName || "五子棋") + " · "
                                                        + modelData.hostIp + " : " + modelData.port
                                                    color: AppTheme.textMuted
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            ColumnLayout {
                                                spacing: 4

                                                Text {
                                                    Layout.alignment: Qt.AlignRight
                                                    text: modelData.playerCount + " / " + modelData.maxPlayers
                                                    color: AppTheme.textPrimary
                                                    font.pixelSize: 13
                                                    font.weight: Font.Medium
                                                }

                                                Text {
                                                    Layout.alignment: Qt.AlignRight
                                                    text: modelData.inGame
                                                        ? "对局中"
                                                        : modelData.isFull ? "房间已满" : "可加入"
                                                    color: modelData.inGame || modelData.isFull
                                                        ? AppTheme.textMuted
                                                        : AppTheme.accent
                                                    font.pixelSize: AppTheme.fontSizeCaption
                                                }
                                            }
                                        }

                                        TapHandler {
                                            enabled: !modelData.isFull && !modelData.inGame
                                            onTapped: root.joinDiscoveredRoom(modelData)
                                        }
                                    }
                                }

                                Text {
                                    width: parent.width
                                    visible: AppCtrl.networkManager.discoveredRooms.length === 0
                                    text: "还没有发现可用房间。确认双方在同一局域网，并且房主已经创建房间。"
                                    color: AppTheme.textMuted
                                    font.pixelSize: AppTheme.fontSizeCaption
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: joinCard
                        width: parent.width
                        implicitHeight: joinLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        transform: Translate { id: joinCardOffset; y: 20 }
                        visible: root.lobbyMode === 0

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: joinLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            Text {
                                text: "加入房间"
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fontSizeHeading
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "输入主机地址和端口，使用当前昵称加入。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }

                            TextField {
                                id: ipInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                text: AppCtrl.recentJoinIp
                                placeholderText: "输入主机 IP 地址"
                                font.pixelSize: 15
                                color: AppTheme.textPrimary
                                leftPadding: 16
                                verticalAlignment: TextInput.AlignVCenter
                                background: Rectangle {
                                    radius: 14
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: ipInput.activeFocus ? AppTheme.accent : AppTheme.cardBorder
                                }
                                onTextChanged: root.joinErrorText = ""
                            }

                            TextField {
                                id: portInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                text: AppCtrl.recentJoinPort.toString()
                                placeholderText: "输入端口"
                                font.pixelSize: 15
                                color: AppTheme.textPrimary
                                inputMethodHints: Qt.ImhDigitsOnly
                                leftPadding: 16
                                verticalAlignment: TextInput.AlignVCenter
                                background: Rectangle {
                                    radius: 14
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: portInput.activeFocus ? AppTheme.accent : AppTheme.cardBorder
                                }
                                onTextChanged: root.joinErrorText = ""
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "昵称: " + AppCtrl.nickname
                                color: AppTheme.textMuted
                                font.pixelSize: AppTheme.fontSizeCaption
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: root.joinErrorText.length > 0
                                text: root.joinErrorText
                                color: "#B14E44"
                                font.pixelSize: AppTheme.fontSizeCaption
                                wrapMode: Text.WordWrap
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                text: "加入房间"
                                onClicked: root.joinCurrentRoom()
                            }
                        }
                    }

                    GameCard {
                        id: hostCard
                        width: parent.width
                        height: 182
                        titleText: "创建五子棋房间"
                        subtitleText: "使用默认端口开房，等待另一位玩家加入。"
                        tagText: "2 人联机"
                        opacity: 0
                        transform: Translate { id: hostCardOffset; y: 20 }
                        visible: root.lobbyMode === 0
                        onClicked: AppCtrl.startRoomAsHost()
                    }

                    GameCard {
                        id: ddzHostCard
                        width: parent.width
                        height: 182
                        titleText: "创建斗地主房间"
                        subtitleText: "房主为地主，等待一位玩家加入，另一家由机器人托管。"
                        tagText: "2 人联机"
                        dark: true
                        opacity: 0
                        transform: Translate { id: ddzHostCardOffset; y: 20 }
                        visible: root.lobbyMode === 0
                        onClicked: AppCtrl.startDouDiZhuRoomAsHost()
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "默认端口"
                        valueText: AppCtrl.defaultPort + "，可在设置页修改"
                        actionText: ""
                        visible: root.lobbyMode === 0
                    }

                    GameCard {
                        id: onlineGomokuCard
                        width: parent.width
                        height: 182
                        titleText: "在线五子棋"
                        subtitleText: "连接 ECS 演示服务器，进入在线五子棋房间。"
                        tagText: "在线联机"
                        opacity: 0
                        visible: root.lobbyMode === 1
                        transform: Translate { id: onlineGomokuCardOffset; y: 20 }
                        onClicked: root.joinOnlineRoom("gomoku")
                    }

                    GameCard {
                        id: onlineDouDiZhuCard
                        width: parent.width
                        height: 182
                        titleText: "在线斗地主"
                        subtitleText: "连接 ECS 演示服务器，进入两人在线斗地主房间，第三家由机器人托管。"
                        tagText: "在线联机"
                        dark: true
                        opacity: 0
                        visible: root.lobbyMode === 1
                        transform: Translate { id: onlineDouDiZhuCardOffset; y: 20 }
                        onClicked: root.joinOnlineRoom("doudizhu")
                    }

                    Text {
                        width: parent.width
                        visible: root.lobbyMode === 1 && root.joinErrorText.length > 0
                        text: root.joinErrorText
                        color: "#B14E44"
                        font.pixelSize: AppTheme.fontSizeCaption
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        id: serverCard
                        width: parent.width
                        implicitHeight: serverLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        visible: root.lobbyMode === 1
                        transform: Translate { id: serverCardOffset; y: 20 }

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: serverLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true

                                Row {
                                    spacing: 10

                                    Rectangle {
                                        width: 10
                                        height: 10
                                        radius: 5
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: root.isOnlineServerConnection() ? "#34A853" : "#FBBC04"
                                    }

                                    Column {
                                        spacing: 4

                                        Text {
                                            text: AppCtrl.onlineServerName
                                            color: AppTheme.textPrimary
                                            font.pixelSize: AppTheme.fontSizeHeading
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: AppCtrl.onlineServerHost + " : " + AppCtrl.onlineServerPort
                                            color: AppTheme.textSecondary
                                            font.pixelSize: AppTheme.fontSizeBody
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                ActionButton {
                                    Layout.preferredWidth: 88
                                    text: root.isOnlineServerConnection() ? "重连" : "连接"
                                    onClicked: root.joinOnlineRoom()
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.isOnlineServerConnection()
                                    ? "当前已连接演示服务器，进入房间后可直接准备并开始对局。"
                                    : "固定连接 ECS 演示服务器，适合异地演示，不依赖同一局域网。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Rectangle {
                        id: roomListCard
                        width: parent.width
                        implicitHeight: roomListLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        visible: root.lobbyMode === 1
                        transform: Translate { id: roomListCardOffset; y: 20 }

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: roomListLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            Text {
                                text: "在线房间"
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fontSizeHeading
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "当前服务端还是单房间演示模式，这里先展示一个固定在线房间入口，后面再接真正的多房间列表。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: onlineRoomRow.implicitHeight + 24
                                radius: 18
                                color: AppTheme.cardBackgroundSoft
                                border.width: 1
                                border.color: AppTheme.cardBorder

                                RowLayout {
                                    id: onlineRoomRow
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Text {
                                            text: "演示大厅"
                                            color: AppTheme.textPrimary
                                            font.pixelSize: 15
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "固定单房间 · " + AppCtrl.onlineServerHost + " : " + AppCtrl.onlineServerPort
                                            color: AppTheme.textMuted
                                            font.pixelSize: AppTheme.fontSizeCaption
                                            elide: Text.ElideRight
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 4

                                        Text {
                                            Layout.alignment: Qt.AlignRight
                                            text: "1 个入口"
                                            color: AppTheme.textPrimary
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignRight
                                            text: "进入服务器"
                                            color: AppTheme.accent
                                            font.pixelSize: AppTheme.fontSizeCaption
                                        }
                                    }
                                }

                                TapHandler {
                                    onTapped: root.joinOnlineRoom()
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: quickJoinCard
                        width: parent.width
                        implicitHeight: quickJoinLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        visible: root.lobbyMode === 1
                        transform: Translate { id: quickJoinCardOffset; y: 20 }

                        Rectangle {
                            x: 0
                            y: 6
                            width: parent.width - 2
                            height: parent.height + 2
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 3
                            width: parent.width - 1
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        ColumnLayout {
                            id: quickJoinLayout
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            Text {
                                text: "在线入口"
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fontSizeHeading
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "如果只是演示公网联机，可以直接快速进入在线大厅，省掉手动输入地址。"
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fontSizeBody
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 38
                                    radius: 19
                                    color: AppTheme.cardBackgroundSoft
                                    border.width: 1
                                    border.color: AppTheme.cardBorder

                                    Text {
                                        anchors.centerIn: parent
                                        text: AppCtrl.onlineServerHost + " : " + AppCtrl.onlineServerPort
                                        color: AppTheme.textMuted
                                        font.pixelSize: AppTheme.fontSizeCaption
                                    }
                                }

                                ActionButton {
                                    Layout.preferredWidth: 126
                                    text: "快速进入大厅"
                                    onClicked: root.joinOnlineRoom()
                                }
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: roomStateColumn.height
                visible: root.inRoom

                Column {
                    id: roomStateColumn
                    width: parent.width
                    spacing: 18

                    Rectangle {
                        width: parent.width
                        height: 96
                        radius: AppTheme.radiusCard
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder

                        Rectangle {
                            x: 0
                            y: 4
                            width: parent.width - 2
                            height: parent.height + 1
                            radius: parent.radius
                            color: AppTheme.shadowMedium
                        }

                        Rectangle {
                            x: 0
                            y: 2
                            width: parent.width - 1
                            height: parent.height
                            radius: parent.radius
                            color: AppTheme.shadowLight
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: root.networkRoom
                                    ? (AppCtrl.networkManager.isConnected ? "#34A853" : "#FBBC04")
                                    : AppTheme.accent
                            }

                            Column {
                                spacing: 4

                                Text {
                                    text: {
                                        if (AppCtrl.networkManager.isHost)
                                            return "主机房间"
                                        if (root.isOnlineServerConnection())
                                            return "在线演示房间"
                                        if (AppCtrl.networkManager.isConnected)
                                            return "已加入房间"
                                        return "本地双人"
                                    }
                                    color: AppTheme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    text: {
                                        if (AppCtrl.networkManager.isHost)
                                            return "IP: " + AppCtrl.networkManager.localIp
                                                + " : " + AppCtrl.networkManager.serverPort
                                        if (root.isOnlineServerConnection())
                                            return "ECS: " + AppCtrl.networkManager.connectedIp
                                                + " : " + AppCtrl.networkManager.connectedPort
                                        if (AppCtrl.networkManager.isConnected)
                                            return "IP: " + AppCtrl.networkManager.connectedIp
                                                + " : " + AppCtrl.networkManager.connectedPort
                                        return "当前设备上的本地双人房间"
                                    }
                                    color: AppTheme.textMuted
                                    font.pixelSize: 12
                                }
                            }
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            text: AppCtrl.networkManager.isHost
                                ? (AppCtrl.networkManager.clientCount > 0 ? "等待开始" : "等待加入...")
                                : (root.isOnlineServerConnection()
                                    ? "在线已连接"
                                    : (AppCtrl.networkManager.isConnected ? "已连接" : "可直接开始"))
                            color: AppTheme.accent
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "当前桌游"
                        valueText: AppCtrl.roomManager.gameName
                        actionText: ""
                    }

                    Text {
                        width: parent.width
                        text: AppTheme.zhPlayers()
                        color: AppTheme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: AppCtrl.roomManager.playerList

                        delegate: PlayerCard {
                            width: parent.width
                            height: 84
                            playerName: modelData.name
                            roleText: modelData.isHost ? AppTheme.zhHost() : AppTheme.zhMember()
                            statusText: modelData.isReady ? AppTheme.zhReady() : AppTheme.zhNotReady()
                            ready: modelData.isReady
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: AppTheme.spacingMd

                        ActionButton {
                            width: (parent.width - parent.spacing) / 2
                            text: {
                                var player = root.localPlayer()
                                return player && player.isReady ? "取消准备" : AppTheme.zhPrepare()
                            }
                            secondary: true
                            onClicked: root.toggleReadyState()
                        }

                        ActionButton {
                            width: (parent.width - parent.spacing) / 2
                            text: AppTheme.zhStartGame()
                            enabled: AppCtrl.roomManager.canStart
                            onClicked: AppCtrl.roomManager.startGame()
                        }
                    }

                    ActionButton {
                        width: parent.width
                        text: "添加本地对手"
                        secondary: true
                        visible: !root.networkRoom
                                 && AppCtrl.roomManager.playerList.length < AppCtrl.roomManager.maxPlayers
                        onClicked: AppCtrl.roomManager.addTestPlayer("本地对手")
                    }

                    ActionButton {
                        width: parent.width
                        text: "退出房间"
                        secondary: true
                        onClicked: AppCtrl.leaveRoom()
                    }
                }
            }
        }
    }

    SequentialAnimation {
        id: joinEntryAnim
        running: !root.inRoom

        ParallelAnimation {
            NumberAnimation { target: discoverCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: discoverCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: joinCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: joinCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: hostCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: hostCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: ddzHostCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: ddzHostCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: onlineGomokuCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: onlineGomokuCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: onlineDouDiZhuCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: onlineDouDiZhuCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: serverCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: serverCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: roomListCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: roomListCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 70 }
        ParallelAnimation {
            NumberAnimation { target: quickJoinCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: quickJoinCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
