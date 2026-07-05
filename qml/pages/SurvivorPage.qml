import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "survivorPage"

    property real touchDx: 0
    property real touchDy: 0
    property bool keyUp: false
    property bool keyDown: false
    property bool keyLeft: false
    property bool keyRight: false
    property bool compactLayout: width < 460
    property real hudHeight: compactLayout ? 126 : 120
    property real radarCardSize: compactLayout ? 72 : 78
    property var weaponModel: AppCtrl.survivorController.weaponSlots
    property var passiveModel: AppCtrl.survivorController.passiveSlots

    function updateMovement() {
        var dx = touchDx
        var dy = touchDy

        if (Math.abs(dx) < 0.01 && Math.abs(dy) < 0.01) {
            dx = (keyRight ? 1 : 0) - (keyLeft ? 1 : 0)
            dy = (keyDown ? 1 : 0) - (keyUp ? 1 : 0)
        }

        AppCtrl.survivorController.setMoveInput(dx, dy)
    }

    function leaveCurrentGame() {
        AppCtrl.survivorController.stopRun()
        if (AppCtrl.networkManager.isHost || AppCtrl.networkManager.isConnected)
            AppCtrl.leaveRoom()
        AppCtrl.openLobbyForGame("survivor")
    }

    function slotData(model, index) {
        if (!model || index >= model.length)
            return { title: "", subtitle: "", filled: false, accent: "#4A655C" }
        return model[index]
    }

    function slotShortLabel(slot) {
        if (!slot || !slot.filled || !slot.title)
            return ""
        return slot.title.length <= 2 ? slot.title : slot.title.slice(0, 2)
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#13221D" }
            GradientStop { position: 1.0; color: "#1E332C" }
        }
    }

    focus: true
    Keys.onPressed: function(event) {
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_W || event.key === Qt.Key_Up)
            keyUp = true
        else if (event.key === Qt.Key_S || event.key === Qt.Key_Down)
            keyDown = true
        else if (event.key === Qt.Key_A || event.key === Qt.Key_Left)
            keyLeft = true
        else if (event.key === Qt.Key_D || event.key === Qt.Key_Right)
            keyRight = true
        updateMovement()
    }
    Keys.onReleased: function(event) {
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_W || event.key === Qt.Key_Up)
            keyUp = false
        else if (event.key === Qt.Key_S || event.key === Qt.Key_Down)
            keyDown = false
        else if (event.key === Qt.Key_A || event.key === Qt.Key_Left)
            keyLeft = false
        else if (event.key === Qt.Key_D || event.key === Qt.Key_Right)
            keyRight = false
        updateMovement()
    }

    Item {
        anchors.fill: parent

        SurvivorRenderItem {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: hudOverlay.bottom
            anchors.bottom: parent.bottom
            controller: AppCtrl.survivorController
            compactLayout: root.compactLayout
        }

        Rectangle {
            id: hudOverlay
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: root.hudHeight
            color: "#0C130E"
            opacity: 0.72
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: levelBadge.left
            anchors.rightMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.leftMargin: 10
            height: 18
            radius: 4
            color: "#111612"
            border.width: 2
            border.color: "#E2B76A"

            Rectangle {
                width: Math.max(4, (parent.width - 4) * (AppCtrl.survivorController.exp / Math.max(1, AppCtrl.survivorController.expToNext)))
                height: parent.height - 4
                radius: 3
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                color: "#E0C15D"
            }
        }

        Rectangle {
            id: levelBadge
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.right: radarFrame.left
            anchors.rightMargin: 8
            width: 44
            height: 18
            radius: 4
            color: "#111612"
            border.width: 2
            border.color: "#E2B76A"

            Text {
                anchors.centerIn: parent
                text: "Lv." + AppCtrl.survivorController.level
                color: "#F7EFE0"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 44
            spacing: 3

            Repeater {
                model: 6

                delegate: Rectangle {
                    width: 24
                    height: 24
                    radius: 4
                    property var slot: root.slotData(root.weaponModel, index)
                    color: "#141915"
                    border.width: slot.filled ? 1.5 : 1
                    border.color: slot.filled ? slot.accent : "#54624E"

                    Text {
                        anchors.centerIn: parent
                        text: root.slotShortLabel(parent.slot)
                        color: parent.slot.filled ? "#F4EBD8" : "#708078"
                        font.pixelSize: 8
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 72
            spacing: 3

            Repeater {
                model: 6

                delegate: Rectangle {
                    width: 16
                    height: 16
                    radius: 3
                    property var slot: root.slotData(root.passiveModel, index)
                    color: "#141915"
                    border.width: slot.filled ? 1.2 : 1
                    border.color: slot.filled ? slot.accent : "#54624E"

                    Text {
                        anchors.centerIn: parent
                        text: root.slotShortLabel(parent.slot)
                        color: parent.slot.filled ? "#F4EBD8" : "#708078"
                        font.pixelSize: 7
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 98
            width: compactLayout ? 112 : 124
            height: 7
            radius: 3.5
            color: "#365349"

            Rectangle {
                width: parent.width * (AppCtrl.survivorController.hp / Math.max(1, AppCtrl.survivorController.maxHp))
                height: parent.height
                radius: parent.radius
                color: "#E16658"
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 38
            text: Qt.formatTime(new Date(0, 0, 0, 0, 0, AppCtrl.survivorController.survivalTimeSec), "mm:ss")
            color: "#FFFFFF"
            font.pixelSize: compactLayout ? 24 : 26
            font.weight: Font.Black
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 62
            text: AppCtrl.survivorController.waveLabel
            color: "#C8D7CC"
            font.pixelSize: 10
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 78
            spacing: 1

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "击杀 " + AppCtrl.survivorController.killCount
                color: "#FFFFFF"
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Text {
                text: AppCtrl.survivorController.networkPrototype ? "online" : "local"
                color: "#BFD3C6"
                font.pixelSize: 9
            }
        }

        Rectangle {
            id: radarFrame
            anchors.top: parent.top
            anchors.topMargin: 36
            anchors.right: parent.right
            anchors.rightMargin: 10
            width: root.radarCardSize
            height: root.radarCardSize
            radius: compactLayout ? 14 : 16
            color: "#213830"
            border.width: 1.4
            border.color: "#38594D"

            SurvivorRenderItem {
                anchors.fill: parent
                anchors.margins: compactLayout ? 9 : 10
                controller: AppCtrl.survivorController
                radarMode: true
                compactLayout: root.compactLayout
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 18
            z: 20

            property real pressX: 0
            property real pressY: 0
            property bool armed: false

            onPressed: function(mouse) {
                pressX = mouse.x
                pressY = mouse.y
                armed = true
            }

            onPositionChanged: function(mouse) {
                if (!armed)
                    return

                const dx = mouse.x - pressX
                const dy = mouse.y - pressY
                if (dx > 70 && Math.abs(dy) < 48) {
                    armed = false
                    root.leaveCurrentGame()
                } else if (dx < -8 || Math.abs(dy) > 56) {
                    armed = false
                }
            }

            onReleased: armed = false
            onCanceled: armed = false
        }

        Rectangle {
            id: joystickBase
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 20
            anchors.bottomMargin: 22
            width: compactLayout ? 88 : 96
            height: width
            radius: width / 2
            color: "#20342D"
            border.width: 2
            border.color: "#335346"

            property real anchorX: width / 2
            property real anchorY: height / 2

            Rectangle {
                x: joystickBase.anchorX - width / 2 + root.touchDx * 20
                y: joystickBase.anchorY - height / 2 + root.touchDy * 20
                width: 34
                height: 34
                radius: 17
                color: "#EAD7B0"
            }

            MouseArea {
                anchors.fill: parent
                onPressed: updatePad(mouse.x, mouse.y)
                onPositionChanged: updatePad(mouse.x, mouse.y)
                onReleased: resetPad()
                onCanceled: resetPad()

                function updatePad(x, y) {
                    var dx = (x - joystickBase.anchorX) / 30
                    var dy = (y - joystickBase.anchorY) / 30
                    var len = Math.sqrt(dx * dx + dy * dy)
                    if (len > 1) {
                        dx /= len
                        dy /= len
                    }
                    root.touchDx = dx
                    root.touchDy = dy
                    root.updateMovement()
                }

                function resetPad() {
                    root.touchDx = 0
                    root.touchDy = 0
                    root.updateMovement()
                }
            }
        }

        Rectangle {
            visible: AppCtrl.survivorController.levelUpPending
                     && !AppCtrl.survivorController.chestPending
            anchors.centerIn: parent
            width: Math.min(parent.width - 30, 330)
            height: levelUpColumn.implicitHeight + 28
            radius: 24
            color: "#F2E8D8"
            border.width: 1
            border.color: "#D1C2AA"

            Column {
                id: levelUpColumn
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    width: parent.width
                    text: "等级提升"
                    color: AppTheme.textPrimary
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: AppCtrl.survivorController.levelUpChoices

                    delegate: Rectangle {
                        width: levelUpColumn.width
                        height: 82
                        radius: 18
                        color: "#FFF7EB"
                        border.width: 1
                        border.color: "#D9C9AD"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.category
                                        + " · Lv." + modelData.currentLevel
                                        + "/" + modelData.maxLevel
                                        + " -> " + (modelData.currentLevel + 1)
                                    color: "#8A7662"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: AppTheme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    color: AppTheme.textSecondary
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }
                            }

                            ActionButton {
                                Layout.preferredWidth: 72
                                text: "选择"
                                onClicked: AppCtrl.survivorController.chooseLevelUp(modelData.id)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: AppCtrl.survivorController.chestPending
            anchors.centerIn: parent
            width: Math.min(parent.width - 28, 340)
            height: chestColumn.implicitHeight + 28
            radius: 24
            color: "#F2E8D8"
            border.width: 1
            border.color: "#D1C2AA"

            Column {
                id: chestColumn
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    width: parent.width
                    text: AppCtrl.survivorController.chestTitle
                    color: AppTheme.textPrimary
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: AppCtrl.survivorController.chestRewards

                    delegate: Rectangle {
                        width: chestColumn.width
                        height: 76
                        radius: 18
                        color: modelData.evolved ? "#FFF2D8" : "#FFF7EB"
                        border.width: 1
                        border.color: modelData.evolved ? "#E2B76A" : "#D9C9AD"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            Text {
                                width: parent.width
                                text: modelData.category + (modelData.evolved ? " · 超武" : "")
                                color: modelData.evolved ? "#A56D20" : "#8A7662"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }

                            Text {
                                width: parent.width
                                text: modelData.title
                                color: AppTheme.textPrimary
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }

                            Text {
                                width: parent.width
                                text: modelData.description
                                color: AppTheme.textSecondary
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                ActionButton {
                    width: parent.width
                    text: "继续"
                    onClicked: AppCtrl.survivorController.closeChestRewards()
                }
            }
        }

        Rectangle {
            visible: AppCtrl.survivorController.gameOver
            anchors.centerIn: parent
            width: Math.min(parent.width - 36, 300)
            height: 164
            radius: 24
            color: "#F2E8D8"
            border.width: 1
            border.color: "#D1C2AA"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: "本局结束"
                    color: AppTheme.textPrimary
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "生存 " + AppCtrl.survivorController.survivalTimeSec
                        + " 秒 · 击杀 " + AppCtrl.survivorController.killCount
                    color: AppTheme.textSecondary
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }

                ActionButton {
                    Layout.fillWidth: true
                    text: "返回房间页"
                    onClicked: root.leaveCurrentGame()
                }
            }
        }
    }
}
