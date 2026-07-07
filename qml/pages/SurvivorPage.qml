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
    property real arenaScale: Math.max(renderCanvas.width * (compactLayout ? 0.64 : 0.56), 220)
    property real hudHeight: compactLayout ? 82 : 78
    property real radarCardSize: compactLayout ? 76 : 82
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

    function slotSummary(model) {
        if (!model)
            return ""
        var parts = []
        for (var i = 0; i < model.length; ++i) {
            if (model[i] && model[i].filled && model[i].title)
                parts.push(model[i].title)
        }
        return parts.join(" · ")
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
            id: renderCanvas
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            controller: AppCtrl.survivorController
            compactLayout: root.compactLayout
            layer.enabled: true
            layer.smooth: true
            layer.samples: 8
        }

        Item {
            anchors.fill: renderCanvas
            clip: true

            Repeater {
                model: AppCtrl.survivorController.damageNumbers

                delegate: Item {
                    required property var modelData

                    readonly property real screenX: widthParent / 2
                        + (modelData.x - AppCtrl.survivorController.playerX) * root.arenaScale
                    readonly property real screenY: heightParent / 2
                        + (modelData.y - AppCtrl.survivorController.playerY) * root.arenaScale
                    readonly property real lifeRatio: Math.max(0, modelData.lifeMs / Math.max(1, modelData.totalLifeMs))
                    readonly property real textAlpha: 0.35 + lifeRatio * 0.65
                    readonly property real popScale: 0.96 + lifeRatio * 0.24
                    readonly property real widthParent: renderCanvas.width
                    readonly property real heightParent: renderCanvas.height

                    x: screenX - damageText.width / 2
                    y: screenY - damageText.height / 2 - (1.0 - lifeRatio) * 10
                    width: damageText.width
                    height: damageText.height
                    scale: popScale
                    transformOrigin: Item.Center
                    visible: screenX >= -40 && screenX <= widthParent + 40
                        && screenY >= -40 && screenY <= heightParent + 40

                    Text {
                        id: damageShadow
                        anchors.centerIn: parent
                        text: "-" + modelData.amount
                        color: Qt.rgba(10 / 255, 12 / 255, 11 / 255, parent.textAlpha * 0.78)
                        font.family: "STSong"
                        font.pixelSize: modelData.elite ? 28 : 22
                        font.weight: Font.Black
                        x: 2
                        y: 2
                        renderType: Text.QtRendering
                        renderTypeQuality: Text.HighRenderTypeQuality
                    }

                    Text {
                        id: damageText
                        anchors.centerIn: parent
                        text: "-" + modelData.amount
                        color: modelData.elite
                            ? Qt.rgba(255 / 255, 232 / 255, 154 / 255, parent.textAlpha)
                            : Qt.rgba(250 / 255, 214 / 255, 122 / 255, parent.textAlpha)
                        font.family: "STSong"
                        font.pixelSize: modelData.elite ? 28 : 22
                        font.weight: Font.Black
                        renderType: Text.QtRendering
                        renderTypeQuality: Text.HighRenderTypeQuality
                    }
                }
            }

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * 0.5 + 22
                width: 48
                height: 8

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: Qt.rgba(22 / 255, 30 / 255, 27 / 255, 0.86)
                }

                Rectangle {
                    width: parent.width * (AppCtrl.survivorController.hp / Math.max(1, AppCtrl.survivorController.maxHp))
                    height: parent.height
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#EC8576" }
                        GradientStop { position: 1.0; color: "#C94D44" }
                    }
                }
            }
        }

        Rectangle {
            id: hudOverlay
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: root.hudHeight
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(8 / 255, 12 / 255, 10 / 255, 0.62) }
                GradientStop { position: 1.0; color: Qt.rgba(12 / 255, 19 / 255, 16 / 255, 0.00) }
            }
        }

        Item {
            id: expTrackWrap
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 12
            height: 18

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: "#101714"
            }

            Rectangle {
                width: Math.max(6, (parent.width - 4) * (AppCtrl.survivorController.exp / Math.max(1, AppCtrl.survivorController.expToNext)))
                height: parent.height - 4
                radius: 7
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#F3D58D" }
                    GradientStop { position: 1.0; color: "#C89A4A" }
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                height: 12
                radius: 6
                color: Qt.rgba(12 / 255, 18 / 255, 16 / 255, 0.72)

                Text {
                    anchors.centerIn: parent
                    text: "Lv." + AppCtrl.survivorController.level
                    color: "#F7EFE0"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
                }
            }
        }

        Item {
            id: leftHudGroup
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.topMargin: 42
            width: compactLayout ? 186 : 204
            height: compactLayout ? 82 : 86

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 0
                spacing: 4

                Repeater {
                    model: 6

                    delegate: Rectangle {
                        width: compactLayout ? 20 : 22
                        height: compactLayout ? 20 : 22
                        radius: 6
                        property var slot: root.slotData(root.weaponModel, index)
                        color: slot.filled ? Qt.rgba(16 / 255, 23 / 255, 21 / 255, 0.88) : Qt.rgba(16 / 255, 23 / 255, 21 / 255, 0.56)
                        border.width: 0

                        Text {
                            anchors.centerIn: parent
                            text: root.slotShortLabel(parent.slot)
                            color: parent.slot.filled ? "#EEDFC0" : "#6E7D77"
                            font.pixelSize: compactLayout ? 10 : 11
                            font.weight: Font.DemiBold
                            renderType: Text.QtRendering
                            renderTypeQuality: Text.HighRenderTypeQuality
                        }
                    }
                }
            }

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: compactLayout ? 28 : 30
                spacing: 4

                Repeater {
                    model: 6

                    delegate: Rectangle {
                        width: compactLayout ? 20 : 22
                        height: compactLayout ? 20 : 22
                        radius: 6
                        property var slot: root.slotData(root.passiveModel, index)
                        color: slot.filled ? Qt.rgba(16 / 255, 23 / 255, 21 / 255, 0.82) : Qt.rgba(16 / 255, 23 / 255, 21 / 255, 0.46)
                        border.width: 0

                        Text {
                            anchors.centerIn: parent
                            text: root.slotShortLabel(parent.slot)
                            color: parent.slot.filled ? "#EEDFC0" : "#6E7D77"
                            font.pixelSize: compactLayout ? 10 : 11
                            font.weight: Font.DemiBold
                            renderType: Text.QtRendering
                            renderTypeQuality: Text.HighRenderTypeQuality
                        }
                    }
                }
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: compactLayout ? 50 : 52
                text: root.slotSummary(root.weaponModel)
                color: "#D7CCB8"
                font.pixelSize: 10
                elide: Text.ElideRight
                renderType: Text.QtRendering
                renderTypeQuality: Text.HighRenderTypeQuality
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: compactLayout ? 64 : 67
                text: root.slotSummary(root.passiveModel)
                color: "#98AAA1"
                font.pixelSize: 10
                elide: Text.ElideRight
                renderType: Text.QtRendering
                renderTypeQuality: Text.HighRenderTypeQuality
            }
        }

        Item {
            id: centerHudGroup
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 44
            width: compactLayout ? 214 : 238
            height: compactLayout ? 90 : 96

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Qt.formatTime(new Date(0, 0, 0, 0, 0, AppCtrl.survivorController.survivalTimeSec), "mm:ss")
                    color: "#F4EBDC"
                    font.pixelSize: compactLayout ? 25 : 27
                    font.weight: Font.Black
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: centerHudGroup.width
                    horizontalAlignment: Text.AlignHCenter
                    text: AppCtrl.survivorController.waveLabel
                    color: "#85988E"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: centerHudGroup.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "击杀 " + AppCtrl.survivorController.killCount
                        + " · "
                        + (AppCtrl.survivorController.networkSession ? "online" : "local")
                    color: "#708278"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: centerHudGroup.width
                    horizontalAlignment: Text.AlignHCenter
                    text: AppCtrl.survivorController.upgradeSummary
                    color: "#AAB7B0"
                    font.pixelSize: compactLayout ? 10 : 11
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
                }
            }
        }

        Item {
            id: radarFrame
            anchors.top: parent.top
            anchors.topMargin: 44
            anchors.right: parent.right
            anchors.rightMargin: 12
            width: root.radarCardSize
            height: root.radarCardSize

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Qt.rgba(25 / 255, 37 / 255, 33 / 255, 0.72)
            }

            SurvivorRenderItem {
                anchors.fill: parent
                anchors.margins: compactLayout ? 8 : 9
                controller: AppCtrl.survivorController
                radarMode: true
                compactLayout: root.compactLayout
                layer.enabled: true
                layer.smooth: true
                layer.samples: 8
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
            color: Qt.rgba(23 / 255, 37 / 255, 32 / 255, 0.90)
            border.width: 1.2
            border.color: Qt.rgba(225 / 255, 210 / 255, 161 / 255, 0.20)

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 4
                anchors.bottomMargin: -4
                radius: parent.radius
                color: Qt.rgba(0, 0, 0, 0.18)
                z: -1
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: width / 2
                color: Qt.rgba(255, 255, 255, 0.03)
            }

            property real anchorX: width / 2
            property real anchorY: height / 2

            Rectangle {
                x: joystickBase.anchorX - width / 2 + root.touchDx * 20
                y: joystickBase.anchorY - height / 2 + root.touchDy * 20
                width: 34
                height: 34
                radius: 17
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#F4E3BF" }
                    GradientStop { position: 1.0; color: "#D3B07A" }
                }
                border.width: 1
                border.color: Qt.rgba(90 / 255, 66 / 255, 38 / 255, 0.36)
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
            visible: AppCtrl.survivorController.waitingForOtherPlayer
            anchors.centerIn: parent
            width: Math.min(parent.width - 44, 280)
            height: 92
            radius: 22
            color: Qt.rgba(18 / 255, 28 / 255, 24 / 255, 0.88)
            border.width: 1
            border.color: Qt.rgba(238 / 255, 223 / 255, 192 / 255, 0.14)

            Column {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "队友结算中"
                    color: "#F4EBDC"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 224
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: AppCtrl.survivorController.statusText
                    color: "#98AAA1"
                    font.pixelSize: 11
                    renderType: Text.QtRendering
                    renderTypeQuality: Text.HighRenderTypeQuality
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
