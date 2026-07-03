import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root

    property bool inRoom: AppCtrl.networkManager.isHost
                       || AppCtrl.networkManager.isConnected
                       || AppCtrl.roomManager.playerList.length > 0
    property bool networkRoom: AppCtrl.networkManager.isHost
                            || AppCtrl.networkManager.isConnected
    property string joinErrorText: ""

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
                titleText: root.inRoom ? "房间" : AppTheme.zhMatch()
                subtitleText: root.inRoom
                    ? "房间状态只在加入或创建房间后显示。"
                    : "从这里加入房间，也可以直接创建房间或开始本地双人。"
                trailingText: root.inRoom ? (AppCtrl.roomManager.playerList.length + " / 2") : ""
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
                        id: joinCard
                        width: parent.width
                        implicitHeight: joinLayout.implicitHeight + 54
                        radius: AppTheme.radiusCard + 4
                        color: AppTheme.cardBackground
                        border.width: 1
                        border.color: AppTheme.cardBorder
                        opacity: 0
                        transform: Translate { id: joinCardOffset; y: 20 }

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
                        titleText: "创建房间"
                        subtitleText: "使用设置中的默认端口开房，等待另一位玩家加入。"
                        tagText: "主机"
                        opacity: 0
                        transform: Translate { id: hostCardOffset; y: 20 }
                        onClicked: AppCtrl.startRoomAsHost()
                    }

                    GameCard {
                        id: localCard
                        width: parent.width
                        height: 182
                        titleText: "本地双人"
                        subtitleText: "不走网络，直接在当前设备上开始双人对局。"
                        tagText: "本地"
                        dark: true
                        opacity: 0
                        transform: Translate { id: localCardOffset; y: 20 }
                        onClicked: AppCtrl.startLocalMode()
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "默认端口"
                        valueText: AppCtrl.defaultPort + "，可在设置页修改"
                        actionText: ""
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
                                : (AppCtrl.networkManager.isConnected ? "已连接" : "可直接开始")
                            color: AppTheme.accent
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }

                    SettingCard {
                        width: parent.width
                        height: 84
                        titleText: "当前桌游"
                        valueText: "五子棋"
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
                        visible: !root.networkRoom && AppCtrl.roomManager.playerList.length < 2
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
            NumberAnimation { target: localCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: localCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }
}
