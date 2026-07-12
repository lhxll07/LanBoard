import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "dormDefensePage"
    focus: true

    property real cameraPanX: 0
    property real cameraPanY: 0
    property bool moveCameraLeft: false
    property bool moveCameraRight: false
    property bool moveCameraUp: false
    property bool moveCameraDown: false
    property int highlightedTurretRow: -1
    property int highlightedTurretColumn: -1
    property int highlightedTurretRange: 0
    property bool buildPanelVisible: false
    property int buildPanelRow: -1
    property int buildPanelColumn: -1
    property bool actionPanelVisible: false
    property string pendingActionType: ""
    property string pendingActionVerb: ""
    property int pendingActionRow: -1
    property int pendingActionColumn: -1
    property int pendingActionGoldCost: 0
    property int pendingActionPowerCost: 0
    property string pendingActionTitle: ""
    property string pendingActionDetail: ""
    property string pendingActionRequirement: ""
    property bool pendingActionRequirementMet: true
    property bool ghostHoldLeft: false
    property bool ghostHoldRight: false
    property bool ghostHoldUp: false
    property bool ghostHoldDown: false
    property bool ghostHoldQ: false
    property bool ghostHoldE: false
    property bool ghostHoldZ: false
    property bool ghostHoldC: false
    property real ghostMoveRowDelta: 0
    property real ghostMoveColumnDelta: 0
    property var turretBursts: []
    property var damageBursts: []
    property int lastProcessedVolleySerial: 0
    property int effectSequence: 1
    readonly property bool synchronizedHumanGhost: networkRoom && ctrl.humanGhostControlled
    readonly property int ghostMoveDurationMs: synchronizedHumanGhost
                                                ? 40
                                                : (ctrl.playerControlsGhost ? 28 : 1040)
    readonly property real keyboardCameraSpeed: mapViewport ? mapViewport.cellSize * 0.22 : 8
    readonly property int focusBedRow: ctrl.playerBedRow
    readonly property int focusBedColumn: ctrl.playerBedColumn
    readonly property real visibleRoomColumns: ctrl.playerRoomSelected ? 7 : 12
    readonly property real visibleRoomRows: ctrl.playerRoomSelected ? 8 : 11
    readonly property bool cameraPanEnabled: !ctrl.playerControlsGhost || ctrl.timeRemaining > 0
    readonly property bool ghostMovementMode: ctrl.playerControlsGhost
                                             && !ctrl.roleSelectionRequired
                                             && !ctrl.gameOver
                                             && ctrl.timeRemaining <= 0
    readonly property bool localDefenderEliminated: ctrl.localPlayerEliminated && !ctrl.gameOver
    readonly property bool networkRoom: AppCtrl.networkManager.isHost
                                     || AppCtrl.networkManager.isConnected
    readonly property var ctrl: AppCtrl.dormDefenseController
    readonly property bool localResultVictory: ctrl.playerControlsGhost
                                                ? ctrl.winner === 2
                                                : ctrl.winner === 1
    readonly property string resultTitle: ctrl.playerControlsGhost
                                           ? (localResultVictory ? "猛鬼获胜" : "猛鬼失败")
                                           : (localResultVictory ? "守夜人获胜" : "守夜人失败")
    readonly property string resultMessage: ctrl.playerControlsGhost
                                             ? (localResultVictory
                                                ? "所有房门已被摧毁，鬼方获得胜利。"
                                                : "鬼被炮塔击败，守夜人获得胜利。")
                                             : (localResultVictory
                                                ? "鬼已被击败，守夜人获得胜利。"
                                                : "所有房门已被摧毁，鬼方获得胜利。")
    readonly property var botProfiles: [
        { accent: "#7BE0FF", face: "#E2E9F2", hair: "#2D3640" },
        { accent: "#FFE082", face: "#F2D7C9", hair: "#4A2D2B" },
        { accent: "#7CFF94", face: "#E9DDD2", hair: "#171819" },
        { accent: "#FFC7E7", face: "#D8C1AE", hair: "#101012" },
        { accent: "#C7CFFF", face: "#E5D8CF", hair: "#3B2A20" }
    ]
    readonly property var teammates: [
        { accent: "#62CBFF", face: "#E2E9F2", hair: "#2D3640" },
        { accent: "#FFFFFF", face: "#F2D7C9", hair: "#4A2D2B" },
        { accent: "#68FF82", face: "#E9DDD2", hair: "#171819" },
        { accent: "#FFFFFF", face: "#D8C1AE", hair: "#101012" }
    ]

    function leaveCurrentGame() {
        resetGhostKeyState()
        AppCtrl.returnFromDormDefenseGame()
    }

    function clampValue(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function setCameraPan(nextPanX, nextPanY) {
        if (!mapViewport)
            return

        cameraPanX = clampValue(nextPanX, mapViewport.minPanX, mapViewport.maxPanX)
        cameraPanY = clampValue(nextPanY, mapViewport.minPanY, mapViewport.maxPanY)
    }

    function updateKeyboardStickVisual() {
        if (!cameraStick)
            return

        let dirX = 0
        let dirY = 0

        if (moveCameraLeft)
            dirX -= 1
        if (moveCameraRight)
            dirX += 1
        if (moveCameraUp)
            dirY -= 1
        if (moveCameraDown)
            dirY += 1

        if (dirX === 0 && dirY === 0)
            return

        cameraStick.suppressCameraSync = true
        cameraStick.x = clampValue(cameraStick.centerX + dirX * cameraStick.maxDistance * 0.82,
                                   cameraStick.centerX - cameraStick.maxDistance,
                                   cameraStick.centerX + cameraStick.maxDistance)
        cameraStick.y = clampValue(cameraStick.centerY + dirY * cameraStick.maxDistance * 0.82,
                                   cameraStick.centerY - cameraStick.maxDistance,
                                   cameraStick.centerY + cameraStick.maxDistance)
        cameraStick.suppressCameraSync = false
    }

    function cardCostText(type) {
        if (type === "generator")
            return "G:200  P:0"
        if (type === "turret")
            return "G:8  P:1"
        if (type === "bed")
            return "G:" + String(25 * Math.pow(2, Math.max(0, ctrl.bedLevel - 1)))
        if (type === "door")
            return "G:" + String(ctrl.doorMaxHp * 2)
        return ""
    }

    function clearTurretRange() {
        highlightedTurretRow = -1
        highlightedTurretColumn = -1
        highlightedTurretRange = 0
    }

    function closeBuildPanel() {
        buildPanelVisible = false
        buildPanelRow = -1
        buildPanelColumn = -1
    }

    function closeActionPanel() {
        actionPanelVisible = false
        pendingActionType = ""
        pendingActionVerb = ""
        pendingActionRow = -1
        pendingActionColumn = -1
        pendingActionGoldCost = 0
        pendingActionPowerCost = 0
        pendingActionTitle = ""
        pendingActionDetail = ""
        pendingActionRequirement = ""
        pendingActionRequirementMet = true
    }

    function resetGhostKeyState() {
        ghostHoldLeft = false
        ghostHoldRight = false
        ghostHoldUp = false
        ghostHoldDown = false
        ghostHoldQ = false
        ghostHoldE = false
        ghostHoldZ = false
        ghostHoldC = false
        ghostMoveRowDelta = 0
        ghostMoveColumnDelta = 0
    }

    function refreshTurretRange() {
        if (highlightedTurretRow < 0 || highlightedTurretColumn < 0) {
            clearTurretRange()
            return
        }

        const index = highlightedTurretRow * ctrl.columns + highlightedTurretColumn
        const cell = ctrl.cells[index]
        if (!cell || cell.buildingType !== "turret") {
            clearTurretRange()
            return
        }

        highlightedTurretRange = 2 + cell.level
    }

    function showTurretRange(row, column) {
        highlightedTurretRow = row
        highlightedTurretColumn = column
        refreshTurretRange()
    }

    function openBuildPanel(row, column) {
        closeActionPanel()
        buildPanelRow = row
        buildPanelColumn = column
        buildPanelVisible = true
    }

    function openActionPanel(type, arg2, arg3, arg4, arg5, arg6, arg7) {
        let row = arg2
        let column = arg3
        if (typeof arg2 === "string") {
            row = arg3
            column = arg4
        }

        const info = ctrl.actionInfo(type, row, column)
        closeBuildPanel()
        pendingActionType = type
        pendingActionVerb = info.verb
        pendingActionRow = row
        pendingActionColumn = column
        pendingActionGoldCost = info.goldCost
        pendingActionPowerCost = info.powerCost
        pendingActionTitle = info.title
        pendingActionDetail = info.detailText
        pendingActionRequirement = info.requirementText
        pendingActionRequirementMet = info.requirementMet && info.available
        actionPanelVisible = true
    }

    function canAffordPendingAction() {
        return ctrl.gold >= pendingActionGoldCost && ctrl.power >= pendingActionPowerCost
    }

    function canAffordBuildType(type) {
        const info = ctrl.actionInfo(type, buildPanelRow, buildPanelColumn)
        return info.available && info.requirementMet
            && ctrl.gold >= info.goldCost && ctrl.power >= info.powerCost
    }

    function confirmPendingAction() {
        if (!actionPanelVisible)
            return

        if (pendingActionType === "generator" || pendingActionType === "turret") {
            ctrl.buildOrUpgradeAt(pendingActionRow, pendingActionColumn, pendingActionType)
        } else if (pendingActionType === "bed") {
            ctrl.upgradeBed()
        } else if (pendingActionType === "door") {
            ctrl.upgradeDoor()
        }

        closeActionPanel()
    }

    function buildSelectedType(type) {
        if (!buildPanelVisible)
            return
        ctrl.buildOrUpgradeAt(buildPanelRow, buildPanelColumn, type)
        closeBuildPanel()
    }

    function setGhostMoveDirection(rowDelta, columnDelta) {
        ghostMoveRowDelta = rowDelta
        ghostMoveColumnDelta = columnDelta
    }

    function updateGhostMoveDirectionFromPad(normalizedX, normalizedY) {
        const threshold = 0.28
        const rowDelta = Math.abs(normalizedY) >= threshold ? normalizedY : 0
        const columnDelta = Math.abs(normalizedX) >= threshold ? normalizedX : 0

        setGhostMoveDirection(rowDelta, columnDelta)
    }

    function updateGhostMoveDirectionFromKeys() {
        let rowDelta = 0
        let columnDelta = 0

        if (ghostHoldDown || ghostHoldZ || ghostHoldC)
            rowDelta += 1
        if (ghostHoldUp || ghostHoldQ || ghostHoldE)
            rowDelta -= 1
        if (ghostHoldRight || ghostHoldE || ghostHoldC)
            columnDelta += 1
        if (ghostHoldLeft || ghostHoldQ || ghostHoldZ)
            columnDelta -= 1

        rowDelta = Math.max(-1, Math.min(1, rowDelta))
        columnDelta = Math.max(-1, Math.min(1, columnDelta))
        setGhostMoveDirection(rowDelta, columnDelta)
    }

    function clearGhostMoveDirection(rowDelta, columnDelta) {
        if (ghostMoveRowDelta === rowDelta && ghostMoveColumnDelta === columnDelta) {
            ghostMoveRowDelta = 0
            ghostMoveColumnDelta = 0
        }
    }

    function pruneCombatEffects() {
        const now = Date.now()
        turretBursts = turretBursts.filter((burst) => now - burst.startTime < burst.duration)
        damageBursts = damageBursts.filter((burst) => now - burst.startTime < burst.duration)
    }

    function spawnTurretVolley() {
        if (ctrl.turretVolleySerial <= 0 || ctrl.turretVolleySerial === lastProcessedVolleySerial)
            return

        lastProcessedVolleySerial = ctrl.turretVolleySerial
        const now = Date.now()
        const nextBursts = turretBursts.slice()
        const volley = ctrl.turretVolley

        for (let i = 0; i < volley.length; ++i) {
            const shot = volley[i]
            nextBursts.push({
                id: effectSequence++,
                sourceRow: shot.row,
                sourceColumn: shot.column,
                targetRow: ctrl.ghostRow,
                targetColumn: ctrl.ghostColumn,
                damage: shot.damage,
                startTime: now,
                duration: 260
            })
        }

        turretBursts = nextBursts

        if (ctrl.lastTurretDamage > 0) {
            const nextDamageBursts = damageBursts.slice()
            nextDamageBursts.push({
                id: effectSequence++,
                amount: ctrl.lastTurretDamage,
                row: ctrl.ghostRow,
                column: ctrl.ghostColumn,
                startTime: now,
                duration: 620
            })
            damageBursts = nextDamageBursts
        }

        combatEffectsCanvas.requestPaint()
    }

    Keys.onPressed: (event) => {
        if (event.isAutoRepeat)
            return

        if (ctrl.playerControlsGhost && event.key === Qt.Key_Left) {
            root.ghostHoldLeft = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_Right) {
            root.ghostHoldRight = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_Up) {
            root.ghostHoldUp = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_Down) {
            root.ghostHoldDown = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_Q) {
            root.ghostHoldQ = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_E) {
            root.ghostHoldE = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_Z) {
            root.ghostHoldZ = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (ctrl.playerControlsGhost && event.key === Qt.Key_C) {
            root.ghostHoldC = true
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_A) {
            moveCameraLeft = true
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_D) {
            moveCameraRight = true
            event.accepted = true
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_W) {
            moveCameraUp = true
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_S) {
            moveCameraDown = true
            event.accepted = true
        }
    }

    Keys.onReleased: (event) => {
        if (event.isAutoRepeat)
            return

        if (ctrl.playerControlsGhost && (event.key === Qt.Key_Left
                                         || event.key === Qt.Key_Right
                                         || event.key === Qt.Key_Up
                                         || event.key === Qt.Key_Down
                                         || event.key === Qt.Key_Q
                                         || event.key === Qt.Key_E
                                         || event.key === Qt.Key_Z
                                         || event.key === Qt.Key_C)) {
            if (event.key === Qt.Key_Left)
                root.ghostHoldLeft = false
            else if (event.key === Qt.Key_Right)
                root.ghostHoldRight = false
            else if (event.key === Qt.Key_Up)
                root.ghostHoldUp = false
            else if (event.key === Qt.Key_Down)
                root.ghostHoldDown = false
            else if (event.key === Qt.Key_Q)
                root.ghostHoldQ = false
            else if (event.key === Qt.Key_E)
                root.ghostHoldE = false
            else if (event.key === Qt.Key_Z)
                root.ghostHoldZ = false
            else if (event.key === Qt.Key_C)
                root.ghostHoldC = false
            root.updateGhostMoveDirectionFromKeys()
            event.accepted = true
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_A) {
            moveCameraLeft = false
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_D) {
            moveCameraRight = false
            event.accepted = true
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_W) {
            moveCameraUp = false
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_S) {
            moveCameraDown = false
            event.accepted = true
        }
    }

    Component.onCompleted: forceActiveFocus()

    Connections {
        target: ctrl
        function onRoleChanged() {
            root.resetGhostKeyState()
            if (cameraStick)
                cameraStick.recenterStick()
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: root.cameraPanEnabled
                 && (root.moveCameraLeft || root.moveCameraRight || root.moveCameraUp || root.moveCameraDown)
        onTriggered: {
            let deltaX = 0
            let deltaY = 0

            if (root.moveCameraLeft)
                deltaX += root.keyboardCameraSpeed
            if (root.moveCameraRight)
                deltaX -= root.keyboardCameraSpeed
            if (root.moveCameraUp)
                deltaY += root.keyboardCameraSpeed
            if (root.moveCameraDown)
                deltaY -= root.keyboardCameraSpeed

            root.setCameraPan(root.cameraPanX + deltaX, root.cameraPanY + deltaY)
            root.updateKeyboardStickVisual()
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: ctrl.playerControlsGhost
                 && !ctrl.roleSelectionRequired
                 && !ctrl.gameOver
                 && (root.ghostMoveRowDelta !== 0 || root.ghostMoveColumnDelta !== 0)
        onTriggered: ctrl.moveGhostVector(root.ghostMoveRowDelta, root.ghostMoveColumnDelta)
    }

    function isActiveRoomFloor(modelData) {
        return modelData.tileType === "buildable"
            || modelData.tileType === "bed"
            || modelData.tileType === "otherBed"
            || modelData.buildingType === "generator"
            || modelData.buildingType === "turret"
    }

    function isRoomTile(modelData) {
        return isActiveRoomFloor(modelData) || modelData.tileType === "otherRoom"
    }

    function cellFill(modelData) {
        switch (modelData.tileType) {
        case "corridor":
            return "#2A2D31"
        case "wall":
            return "#1E262D"
        case "door":
            return "#855629"
        case "otherDoor":
            return "#696F76"
        case "bed":
        case "otherBed":
        case "buildable":
            return "#6F808A"
        case "otherRoom":
            return "#444B53"
        default:
            return modelData.buildingType !== "none" ? "#6F808A" : "transparent"
        }
    }

    function cellBorder(modelData) {
        switch (modelData.tileType) {
        case "corridor":
            return "#383C41"
        case "wall":
            return "#BAC5CF"
        case "door":
            return "#F0D37A"
        case "otherDoor":
            return "#C8CDD2"
        case "otherRoom":
            return "#616971"
        default:
            return "#93A5AF"
        }
    }

    function roomBotSpawns() {
        const bots = []
        if (!ctrl.playerControlsGhost)
            return bots

        const cells = ctrl.cells
        let botIndex = 0

        for (let i = 0; i < cells.length; ++i) {
            const cell = cells[i]
            if (cell.tileType !== "otherBed" || cell.roomActive === false)
                continue

            const profile = root.botProfiles[botIndex % root.botProfiles.length]
            bots.push({
                row: cell.row,
                column: cell.column,
                accent: profile.accent,
                face: profile.face,
                hair: profile.hair,
                sleepTag: botIndex % 2 === 0 ? "Z" : "AI"
            })
            botIndex += 1
        }

        return bots
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1A1E22" }
            GradientStop { position: 0.55; color: "#15191C" }
            GradientStop { position: 1.0; color: "#111417" }
        }
    }

    Item {
        anchors.fill: parent

        Item {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: 20
            anchors.bottomMargin: 14

            Row {
                id: avatarStrip
                anchors.left: parent.left
                anchors.top: parent.top
                spacing: 8

                Repeater {
                    model: root.teammates

                    delegate: Rectangle {
                        width: 62
                        height: 62
                        radius: 12
                        color: "#24000000"
                        border.width: 1
                        border.color: "#56FFFFFF"

                        Rectangle {
                            anchors.centerIn: parent
                            width: 40
                            height: 40
                            radius: 11
                            color: "#13171A"
                            border.width: 1
                            border.color: "#44FFFFFF"

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.fillStyle = modelData.hair
                                    ctx.beginPath()
                                    ctx.arc(width * 0.50, height * 0.34, width * 0.23, Math.PI, 0)
                                    ctx.lineTo(width * 0.73, height * 0.48)
                                    ctx.lineTo(width * 0.27, height * 0.48)
                                    ctx.closePath()
                                    ctx.fill()
                                    ctx.fillStyle = modelData.face
                                    ctx.beginPath()
                                    ctx.arc(width * 0.50, height * 0.57, width * 0.18, 0, Math.PI * 2)
                                    ctx.fill()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: avatarStrip.bottom
                anchors.topMargin: 10
                width: 230
                height: 56
                radius: 14
                color: "#24000000"
                border.width: 1
                border.color: "#48FFFFFF"
                visible: !ctrl.playerControlsGhost

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 14

                    Canvas {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = "#FFC735"
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2, 11, 0, Math.PI * 2)
                            ctx.fill()
                            ctx.fillStyle = "#FFFFFF"
                            ctx.font = "bold 14px sans-serif"
                            ctx.textAlign = "center"
                            ctx.textBaseline = "middle"
                            ctx.fillText("G", width / 2, height / 2 + 0.5)
                        }
                    }

                    Text {
                        text: ctrl.gold
                        color: "#FFFFFF"
                        font.pixelSize: 22
                        font.weight: Font.Black
                    }

                    Canvas {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = "#67E7FF"
                            ctx.beginPath()
                            ctx.moveTo(16, 3)
                            ctx.lineTo(10, 14)
                            ctx.lineTo(16, 14)
                            ctx.lineTo(11, 25)
                            ctx.lineTo(20, 12)
                            ctx.lineTo(14, 12)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }

                    Text {
                        text: ctrl.power
                        color: "#FFFFFF"
                        font.pixelSize: 22
                        font.weight: Font.Black
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: avatarStrip.bottom
                anchors.topMargin: 72
                width: 430
                height: debugText.implicitHeight + 12
                radius: 10
                color: "#2A000000"
                border.width: 1
                border.color: "#34FFFFFF"

                Text {
                    id: debugText
                    anchors.fill: parent
                    anchors.margins: 6
                    text: ctrl.debugState
                    color: "#D9F5FF"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: avatarStrip.bottom
                anchors.topMargin: 10
                width: 250
                height: 48
                radius: 14
                color: "#24000000"
                border.width: 1
                border.color: "#48FFFFFF"
                visible: ctrl.playerControlsGhost

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: "鬼方视角"
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.weight: Font.Black
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Lv." + ctrl.ghostLevel + "   HP " + ctrl.ghostHp + "/" + ctrl.ghostMaxHp
                        color: "#FFD5D5"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: avatarStrip.bottom
                anchors.topMargin: 12
                visible: !ctrl.playerControlsGhost && ctrl.timeRemaining > 0 && !ctrl.playerRoomSelected
                width: Math.min(parent.width - 40, 360)
                height: 42
                radius: 12
                color: "#8C1A2025"
                border.width: 1
                border.color: "#48FFFFFF"

                Text {
                    anchors.centerIn: parent
                    text: "准备阶段点击任意房间入住，30 秒后鬼开始行动"
                    color: "#F3FBFF"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
            }

            Item {
                id: mapViewport
                anchors.fill: parent
                anchors.topMargin: 0
                anchors.bottomMargin: 18
                z: -1
                clip: true

                readonly property real cellSize: Math.min(width / root.visibleRoomColumns,
                                                          height / root.visibleRoomRows)
                readonly property real contentWidth: ctrl.columns * cellSize
                readonly property real contentHeight: ctrl.rows * cellSize
                readonly property real focusedCenterX: (ctrl.playerControlsGhost
                                                       ? ctrl.ghostCenterColumn
                                                       : (root.focusBedColumn + 0.5)) * cellSize
                readonly property real focusedCenterY: (ctrl.playerControlsGhost
                                                       ? ctrl.ghostCenterRow
                                                       : (root.focusBedRow + 0.5)) * cellSize
                readonly property real baseOffsetX: width / 2 - focusedCenterX
                readonly property real baseOffsetY: height / 2 - focusedCenterY
                readonly property real minMapOffsetX: Math.min(0, width - contentWidth)
                readonly property real minMapOffsetY: Math.min(0, height - contentHeight)
                readonly property real maxMapOffsetX: 0
                readonly property real maxMapOffsetY: 0
                readonly property real minPanX: minMapOffsetX - baseOffsetX
                readonly property real maxPanX: maxMapOffsetX - baseOffsetX
                readonly property real minPanY: minMapOffsetY - baseOffsetY
                readonly property real maxPanY: maxMapOffsetY - baseOffsetY
                readonly property real mapOffsetX: width / 2 - focusedCenterX + (root.cameraPanEnabled ? root.cameraPanX : 0)
                readonly property real mapOffsetY: height / 2 - focusedCenterY + (root.cameraPanEnabled ? root.cameraPanY : 0)

                Rectangle {
                    anchors.fill: parent
                    color: "#121619"
                }

                property real dragStartPanX: 0
                property real dragStartPanY: 0

                DragHandler {
                    target: null
                    enabled: root.cameraPanEnabled

                    onActiveChanged: {
                        if (active) {
                            mapViewport.dragStartPanX = root.cameraPanX
                            mapViewport.dragStartPanY = root.cameraPanY
                        }
                    }

                    onTranslationChanged: {
                        root.setCameraPan(mapViewport.dragStartPanX + translation.x,
                                          mapViewport.dragStartPanY + translation.y)
                    }
                }

                Item {
                    x: mapViewport.mapOffsetX
                    y: mapViewport.mapOffsetY
                    width: mapViewport.contentWidth
                    height: mapViewport.contentHeight
                    layer.enabled: false
                    layer.smooth: false

                    Behavior on x {
                        enabled: root.synchronizedHumanGhost && ctrl.playerControlsGhost
                        NumberAnimation {
                            duration: root.ghostMoveDurationMs
                            easing.type: Easing.Linear
                        }
                    }

                    Behavior on y {
                        enabled: root.synchronizedHumanGhost && ctrl.playerControlsGhost
                        NumberAnimation {
                            duration: root.ghostMoveDurationMs
                            easing.type: Easing.Linear
                        }
                    }

                    Canvas {
                        id: mainMapCanvas
                        anchors.fill: parent
                        renderTarget: Canvas.Image
                        renderStrategy: Canvas.Cooperative

                        function drawDoor(ctx, x, y, size, otherDoor) {
                            const outer = otherDoor ? "#C5CBD1" : "#D5DCE2"
                            const inner = otherDoor ? "#70757C" : "#6B4D25"
                            const bar = otherDoor ? "#8D949B" : "#B78C45"
                            ctx.fillStyle = outer
                            ctx.fillRect(x + size * 0.12, y + size * 0.08, size * 0.76, size * 0.84)
                            ctx.fillStyle = inner
                            ctx.fillRect(x + size * 0.18, y + size * 0.14, size * 0.64, size * 0.72)
                            ctx.fillStyle = bar
                            ctx.fillRect(x + size * 0.28, y + size * 0.52, size * 0.42, size * 0.12)
                        }

                        function drawBed(ctx, x, y, size) {
                            ctx.fillStyle = "#3F4B52"
                            ctx.fillRect(x + size * 0.18, y + size * 0.50, size * 0.60, size * 0.16)
                            ctx.fillStyle = "#EBF3F7"
                            ctx.fillRect(x + size * 0.24, y + size * 0.34, size * 0.30, size * 0.18)
                            ctx.fillStyle = "#6C8998"
                            ctx.fillRect(x + size * 0.52, y + size * 0.34, size * 0.18, size * 0.18)
                        }

                        function drawGenerator(ctx, x, y, size) {
                            ctx.fillStyle = "#29312C"
                            ctx.fillRect(x + size * 0.24, y + size * 0.24, size * 0.52, size * 0.52)
                            ctx.fillStyle = "#28DE6B"
                            ctx.beginPath()
                            ctx.moveTo(x + size * 0.56, y + size * 0.14)
                            ctx.lineTo(x + size * 0.40, y + size * 0.50)
                            ctx.lineTo(x + size * 0.55, y + size * 0.50)
                            ctx.lineTo(x + size * 0.42, y + size * 0.82)
                            ctx.lineTo(x + size * 0.66, y + size * 0.40)
                            ctx.lineTo(x + size * 0.50, y + size * 0.40)
                            ctx.closePath()
                            ctx.fill()
                        }

                        function drawTurret(ctx, x, y, size) {
                            ctx.fillStyle = "#42413D"
                            ctx.fillRect(x + size * 0.28, y + size * 0.52, size * 0.44, size * 0.14)
                            ctx.fillStyle = "#DDD1C1"
                            ctx.fillRect(x + size * 0.44, y + size * 0.22, size * 0.12, size * 0.30)
                            ctx.fillRect(x + size * 0.54, y + size * 0.28, size * 0.18, size * 0.08)
                        }

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()

                            const size = mapViewport.cellSize
                            const cells = ctrl.cells

                            for (let i = 0; i < cells.length; ++i) {
                                const cell = cells[i]
                                if (cell.tileType === "empty")
                                    continue

                                const x = cell.column * size
                                const y = cell.row * size

                                ctx.fillStyle = root.cellFill(cell)
                                ctx.fillRect(x, y, size, size)

                                if (cell.tileType === "corridor") {
                                    ctx.strokeStyle = "#3B4045"
                                    ctx.lineWidth = 1
                                    ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, size - 1), Math.max(0, size - 1))
                                } else if (cell.tileType === "wall") {
                                    ctx.fillStyle = "#0D1216"
                                    ctx.fillRect(x + 2, y + 2, Math.max(0, size - 4), Math.max(0, size - 4))
                                    ctx.strokeStyle = "#CAD3DC"
                                    ctx.lineWidth = 2
                                    ctx.strokeRect(x + 3, y + 3, Math.max(0, size - 6), Math.max(0, size - 6))
                                } else if (cell.tileType === "otherRoom") {
                                    ctx.fillStyle = "#505860"
                                    ctx.globalAlpha = 0.78
                                    ctx.fillRect(x + 6, y + 6, Math.max(0, size - 12), Math.max(0, size - 12))
                                    ctx.globalAlpha = 1.0
                                } else if (root.isActiveRoomFloor(cell)) {
                                    ctx.fillStyle = cell.tileType === "bed" ? "#7C8E98" : "#71838D"
                                    ctx.globalAlpha = cell.tileType === "buildable" && cell.buildingType === "none" ? 0.42 : 0.82
                                    ctx.fillRect(x + 6, y + 6, Math.max(0, size - 12), Math.max(0, size - 12))
                                    ctx.globalAlpha = 1.0

                                    if (cell.tileType === "buildable" && cell.buildingType === "none") {
                                        ctx.strokeStyle = "#9EAFB9"
                                        ctx.lineWidth = 1
                                        ctx.strokeRect(x + 6.5, y + 6.5, Math.max(0, size - 13), Math.max(0, size - 13))
                                        ctx.fillStyle = "#E3EDF4"
                                        ctx.font = `bold ${Math.max(10, size * 0.34)}px sans-serif`
                                        ctx.textAlign = "center"
                                        ctx.textBaseline = "middle"
                                        ctx.fillText("+", x + size / 2, y + size / 2)
                                    }
                                }

                                if (cell.tileType === "door")
                                    drawDoor(ctx, x, y, size, false)
                                else if (cell.tileType === "otherDoor")
                                    drawDoor(ctx, x, y, size, true)

                                if (cell.tileType === "bed" || cell.tileType === "otherBed")
                                    drawBed(ctx, x, y, size)
                                else if (cell.buildingType === "generator")
                                    drawGenerator(ctx, x, y, size)
                                else if (cell.buildingType === "turret")
                                    drawTurret(ctx, x, y, size)

                                if (cell.tileType === "door" || cell.tileType === "otherDoor"
                                        || cell.tileType === "bed" || cell.tileType === "otherBed"
                                        || cell.buildingType !== "none") {
                                    const label = cell.tileType === "door" || cell.tileType === "otherDoor"
                                        ? cell.label.split("\n")[1]
                                        : ((cell.tileType === "bed" || cell.tileType === "otherBed")
                                            ? cell.label.split("\n")[1]
                                            : "L" + cell.level)
                                    ctx.fillStyle = "#F7FBFF"
                                    ctx.font = `600 ${Math.max(8, size * 0.20)}px sans-serif`
                                    ctx.textAlign = "center"
                                    ctx.textBaseline = "bottom"
                                    if (label)
                                        ctx.fillText(label, x + size / 2, y + size - 2)
                                }
                            }
                        }
                    }

                    Canvas {
                        id: combatEffectsCanvas
                        anchors.fill: parent
                        z: 18
                        renderTarget: Canvas.Image
                        renderStrategy: Canvas.Cooperative

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()

                            const now = Date.now()
                            const size = mapViewport.cellSize

                            for (let i = 0; i < root.turretBursts.length; ++i) {
                                const burst = root.turretBursts[i]
                                const progress = Math.max(0, Math.min(1, (now - burst.startTime) / burst.duration))
                                const startX = (burst.sourceColumn + 0.50) * size
                                const startY = (burst.sourceRow + 0.46) * size
                                const endX = (burst.targetColumn + 0.50) * size
                                const endY = (burst.targetRow + 0.48) * size
                                const currentX = startX + (endX - startX) * progress
                                const currentY = startY + (endY - startY) * progress

                                ctx.strokeStyle = "rgba(255, 220, 170, 0.42)"
                                ctx.lineWidth = Math.max(1.2, size * 0.07)
                                ctx.beginPath()
                                ctx.moveTo(startX, startY)
                                ctx.lineTo(currentX, currentY)
                                ctx.stroke()

                                ctx.fillStyle = "#FFD79D"
                                ctx.beginPath()
                                ctx.arc(currentX, currentY, Math.max(2.4, size * 0.10), 0, Math.PI * 2)
                                ctx.fill()
                            }

                            for (let i = 0; i < root.damageBursts.length; ++i) {
                                const burst = root.damageBursts[i]
                                const progress = Math.max(0, Math.min(1, (now - burst.startTime) / burst.duration))
                                const alpha = 1 - progress
                                const x = (burst.column + 0.50) * size
                                const y = (burst.row + 0.06) * size - progress * size * 0.85

                                ctx.globalAlpha = alpha
                                ctx.fillStyle = "#FFD36F"
                                ctx.font = `bold ${Math.max(10, size * 0.20)}px sans-serif`
                                ctx.textAlign = "center"
                                ctx.textBaseline = "middle"
                                ctx.fillText("-" + burst.amount, x, y)
                                ctx.globalAlpha = 1
                            }
                        }
                    }

                    Rectangle {
                        visible: root.highlightedTurretRange > 0
                        z: 17
                        x: (root.highlightedTurretColumn - root.highlightedTurretRange) * mapViewport.cellSize
                        y: (root.highlightedTurretRow - root.highlightedTurretRange) * mapViewport.cellSize
                        width: (root.highlightedTurretRange * 2 + 1) * mapViewport.cellSize
                        height: (root.highlightedTurretRange * 2 + 1) * mapViewport.cellSize
                        color: "#14FFD38A"
                        border.width: 2
                        border.color: "#76FFD38A"
                        radius: 12
                    }

                    Timer {
                        interval: 16
                        repeat: true
                        running: root.turretBursts.length > 0 || root.damageBursts.length > 0
                        onTriggered: {
                            root.pruneCombatEffects()
                            combatEffectsCanvas.requestPaint()
                        }
                    }

                    Item {
                        id: ghostSprite
                        visible: ctrl.ghostVisible
                        readonly property real footprintCells: 0.78
                        width: mapViewport.cellSize * footprintCells
                        height: mapViewport.cellSize * footprintCells
                        x: (ctrl.ghostCenterColumn - footprintCells / 2) * mapViewport.cellSize
                        y: (ctrl.ghostCenterRow - footprintCells / 2) * mapViewport.cellSize
                        z: 20

                        Behavior on x {
                            enabled: root.synchronizedHumanGhost || !root.ghostMovementMode
                            NumberAnimation {
                                duration: root.ghostMoveDurationMs
                                easing.type: Easing.Linear
                            }
                        }

                        Behavior on y {
                            enabled: root.synchronizedHumanGhost || !root.ghostMovementMode
                            NumberAnimation {
                                duration: root.ghostMoveDurationMs
                                easing.type: Easing.Linear
                            }
                        }

                        Rectangle {
                            width: Math.max(mapViewport.cellSize * 0.78, 38)
                            height: 18
                            x: ghostSprite.width + 8
                            y: (ghostSprite.height - height) / 2 - 2
                            radius: 8
                            color: "#9E1B1214"
                            border.width: 1
                            border.color: "#88FFD3D6"

                            Column {
                                anchors.fill: parent
                                anchors.leftMargin: 5
                                anchors.rightMargin: 5
                                anchors.topMargin: 2
                                anchors.bottomMargin: 2
                                spacing: 1

                                Row {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        text: "Ghost Lv." + ctrl.ghostLevel
                                        color: "#FFF2F2"
                                        font.pixelSize: Math.max(6, mapViewport.cellSize * 0.12)
                                        font.weight: Font.Black
                                    }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: 4
                                    radius: 2
                                    color: "#351012"
                                    border.width: 1
                                    border.color: "#66FFD0D0"

                                    Rectangle {
                                        width: parent.width * (ctrl.ghostMaxHp > 0 ? ctrl.ghostHp / ctrl.ghostMaxHp : 0)
                                        height: parent.height
                                        radius: parent.radius
                                        color: "#FF5A66"
                                    }
                                }
                            }
                        }

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                ctx.fillStyle = "#E8E0DA"
                                ctx.beginPath()
                                ctx.arc(width * 0.50, height * 0.32, width * 0.16, Math.PI, 0)
                                ctx.lineTo(width * 0.68, height * 0.70)
                                ctx.lineTo(width * 0.58, height * 0.58)
                                ctx.lineTo(width * 0.50, height * 0.72)
                                ctx.lineTo(width * 0.42, height * 0.58)
                                ctx.lineTo(width * 0.32, height * 0.70)
                                ctx.closePath()
                                ctx.fill()
                                ctx.fillStyle = "#2B1314"
                                ctx.beginPath()
                                ctx.arc(width * 0.44, height * 0.34, width * 0.03, 0, Math.PI * 2)
                                ctx.arc(width * 0.56, height * 0.34, width * 0.03, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }
                    }

                    Repeater {
                        model: root.roomBotSpawns()

                        delegate: Item {
                            width: mapViewport.cellSize * 0.86
                            height: mapViewport.cellSize * 0.78
                            x: (modelData.column + 0.10) * mapViewport.cellSize
                            y: (modelData.row + 0.12) * mapViewport.cellSize
                            z: 14

                            SequentialAnimation on y {
                                loops: Animation.Infinite
                                running: !ctrl.gameOver && !root.ghostMovementMode
                                NumberAnimation {
                                    from: (modelData.row + 0.12) * mapViewport.cellSize
                                    to: (modelData.row + 0.06) * mapViewport.cellSize
                                    duration: 1000
                                    easing.type: Easing.InOutSine
                                }
                                NumberAnimation {
                                    from: (modelData.row + 0.06) * mapViewport.cellSize
                                    to: (modelData.row + 0.12) * mapViewport.cellSize
                                    duration: 1000
                                    easing.type: Easing.InOutSine
                                }
                            }

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()

                                    ctx.fillStyle = "#141A1E"
                                    ctx.globalAlpha = 0.26
                                    ctx.beginPath()
                                    ctx.ellipse(width * 0.55, height * 0.82, width * 0.28, height * 0.10, 0, 0, Math.PI * 2)
                                    ctx.fill()
                                    ctx.globalAlpha = 1.0

                                    ctx.fillStyle = modelData.accent
                                    ctx.fillRect(width * 0.28, height * 0.36, width * 0.38, height * 0.24)

                                    ctx.fillStyle = modelData.face
                                    ctx.beginPath()
                                    ctx.arc(width * 0.44, height * 0.34, width * 0.13, 0, Math.PI * 2)
                                    ctx.fill()

                                    ctx.fillStyle = modelData.hair
                                    ctx.beginPath()
                                    ctx.arc(width * 0.44, height * 0.30, width * 0.15, Math.PI, 0)
                                    ctx.lineTo(width * 0.58, height * 0.36)
                                    ctx.lineTo(width * 0.30, height * 0.36)
                                    ctx.closePath()
                                    ctx.fill()

                                    ctx.fillStyle = "#EDF6FF"
                                    ctx.beginPath()
                                    ctx.arc(width * 0.40, height * 0.34, width * 0.015, 0, Math.PI * 2)
                                    ctx.arc(width * 0.48, height * 0.34, width * 0.015, 0, Math.PI * 2)
                                    ctx.fill()
                                }
                            }

                            Rectangle {
                                width: Math.max(18, mapViewport.cellSize * 0.28)
                                height: Math.max(12, mapViewport.cellSize * 0.22)
                                radius: 6
                                color: "#B0181F24"
                                border.width: 1
                                border.color: "#48FFFFFF"
                                x: width * 0.58
                                y: height * 0.04

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.sleepTag
                                    color: "#EAF5FF"
                                    font.pixelSize: Math.max(7, mapViewport.cellSize * 0.12)
                                    font.weight: Font.Black
                                }
                            }
                        }
                    }

                    Item {
                        visible: ctrl.playerRoomSelected && !ctrl.playerControlsGhost
                        width: mapViewport.cellSize * 0.86
                        height: mapViewport.cellSize * 0.78
                        x: (ctrl.playerBedColumn + 0.10) * mapViewport.cellSize
                        y: (ctrl.playerBedRow + 0.12) * mapViewport.cellSize
                        z: 15

                        SequentialAnimation on y {
                            loops: Animation.Infinite
                            running: !ctrl.gameOver && !root.ghostMovementMode
                            NumberAnimation {
                                from: (ctrl.playerBedRow + 0.12) * mapViewport.cellSize
                                to: (ctrl.playerBedRow + 0.06) * mapViewport.cellSize
                                duration: 1000
                                easing.type: Easing.InOutSine
                            }
                            NumberAnimation {
                                from: (ctrl.playerBedRow + 0.06) * mapViewport.cellSize
                                to: (ctrl.playerBedRow + 0.12) * mapViewport.cellSize
                                duration: 1000
                                easing.type: Easing.InOutSine
                            }
                        }

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()

                                ctx.fillStyle = "#141A1E"
                                ctx.globalAlpha = 0.26
                                ctx.beginPath()
                                ctx.ellipse(width * 0.55, height * 0.82, width * 0.28, height * 0.10, 0, 0, Math.PI * 2)
                                ctx.fill()
                                ctx.globalAlpha = 1.0

                                ctx.fillStyle = "#62CBFF"
                                ctx.fillRect(width * 0.28, height * 0.36, width * 0.38, height * 0.24)

                                ctx.fillStyle = "#E2E9F2"
                                ctx.beginPath()
                                ctx.arc(width * 0.44, height * 0.34, width * 0.13, 0, Math.PI * 2)
                                ctx.fill()

                                ctx.fillStyle = "#2D3640"
                                ctx.beginPath()
                                ctx.arc(width * 0.44, height * 0.30, width * 0.15, Math.PI, 0)
                                ctx.lineTo(width * 0.58, height * 0.36)
                                ctx.lineTo(width * 0.30, height * 0.36)
                                ctx.closePath()
                                ctx.fill()
                            }
                        }

                        Rectangle {
                            width: Math.max(18, mapViewport.cellSize * 0.28)
                            height: Math.max(12, mapViewport.cellSize * 0.22)
                            radius: 6
                            color: "#B0181F24"
                            border.width: 1
                            border.color: "#48FFFFFF"
                            x: width * 0.58
                            y: height * 0.04

                            Text {
                                anchors.centerIn: parent
                                text: "我"
                                color: "#EAF5FF"
                                font.pixelSize: Math.max(7, mapViewport.cellSize * 0.12)
                                font.weight: Font.Black
                            }
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: (eventPoint) => {
                            const column = Math.floor(eventPoint.position.x / mapViewport.cellSize)
                            const row = Math.floor(eventPoint.position.y / mapViewport.cellSize)
                            if (row < 0 || row >= ctrl.rows || column < 0 || column >= ctrl.columns)
                                return

                            const index = row * ctrl.columns + column
                            const cell = ctrl.cells[index]
                            if (!cell)
                                return
                            const inLocalRoom = cell.localRoom === true

                            if (ctrl.playerControlsGhost) {
                                if (cell.tileType === "door" || cell.tileType === "otherDoor"
                                        || cell.tileType === "bed" || cell.tileType === "otherBed"
                                        || cell.tileType === "buildable" || cell.tileType === "otherRoom") {
                                    ctrl.selectGhostTargetAt(row, column)
                                    root.closeActionPanel()
                                    root.closeBuildPanel()
                                }
                                return
                            }

                            if (ctrl.timeRemaining > 0 && !ctrl.playerRoomSelected) {
                                if (cell.tileType === "otherDoor" || cell.tileType === "otherBed"
                                        || cell.tileType === "otherRoom") {
                                    ctrl.chooseDefenderRoom(row, column)
                                }
                                return
                            }

                            if (cell.buildingType === "turret") {
                                root.showTurretRange(row, column)
                                const nextLevel = cell.level + 1
                                root.openActionPanel("turret",
                                                     "升级",
                                                     row,
                                                     column,
                                                     8 * Math.pow(2, Math.max(0, nextLevel - 2)),
                                                     Math.pow(2, Math.max(0, nextLevel - 1)),
                                                     "升级炮台 Lv." + cell.level + " -> Lv." + nextLevel)
                                return
                            }

                            root.clearTurretRange()

                            if (cell.buildingType === "generator") {
                                const nextLevel = cell.level + 1
                                root.openActionPanel("generator",
                                                     "升级",
                                                     row,
                                                     column,
                                                     200 * Math.pow(2, Math.max(0, nextLevel - 2)),
                                                     0,
                                                     "升级发电机 Lv." + cell.level + " -> Lv." + nextLevel)
                                return
                            }

                            if (cell.tileType === "bed"
                                    || (cell.tileType === "otherBed" && inLocalRoom)) {
                                const nextLevel = ctrl.bedLevel + 1
                                root.openActionPanel("bed",
                                                     "升级",
                                                     row,
                                                     column,
                                                     25 * Math.pow(2, Math.max(0, nextLevel - 2)),
                                                     0,
                                                     "升级床 Lv." + ctrl.bedLevel + " -> Lv." + nextLevel)
                                return
                            }

                            if (cell.tileType === "door"
                                    || (cell.tileType === "otherDoor" && inLocalRoom)) {
                                root.openActionPanel("door",
                                                     "升级",
                                                     row,
                                                     column,
                                                     ctrl.doorMaxHp * 2,
                                                     0,
                                                     "升级门 " + ctrl.doorMaxHp + " -> " + (ctrl.doorMaxHp * 2))
                                return
                            }

                            if (cell.buildable && cell.buildingType === "none") {
                                root.openBuildPanel(row, column)
                                return
                            }

                            root.closeActionPanel()
                            root.closeBuildPanel()
                        }
                    }

                    Connections {
                        target: ctrl

                        function onBoardChanged() {
                            mainMapCanvas.requestPaint()
                            root.refreshTurretRange()
                        }

                        function onTurretVolleyChanged() {
                            root.spawnTurretVolley()
                        }
                    }

                    onWidthChanged: {
                        mainMapCanvas.requestPaint()
                        combatEffectsCanvas.requestPaint()
                    }
                    onHeightChanged: {
                        mainMapCanvas.requestPaint()
                        combatEffectsCanvas.requestPaint()
                    }
                }
            }

            Rectangle {
                id: miniMapCard
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                width: 128
                height: 104
                radius: 12
                color: "#A0121619"
                border.width: 1
                border.color: "#55D6E3EC"

                readonly property real padding: 10
                readonly property real headerHeight: 0
                readonly property real mapAreaWidth: width - padding * 2
                readonly property real mapAreaHeight: height - padding * 2 - headerHeight
                readonly property real miniCellSize: Math.min(mapAreaWidth / ctrl.columns,
                                                              mapAreaHeight / ctrl.rows)
                readonly property real miniMapWidth: ctrl.columns * miniCellSize
                readonly property real miniMapHeight: ctrl.rows * miniCellSize
                readonly property real mapOriginX: padding + (mapAreaWidth - miniMapWidth) / 2
                readonly property real mapOriginY: padding + headerHeight + (mapAreaHeight - miniMapHeight) / 2
                Text {
                    visible: false
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    text: "地图"
                    color: "#F1F7FC"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Item {
                    anchors.fill: parent

                    Canvas {
                        id: miniMapCanvas
                        anchors.fill: parent
                        renderTarget: Canvas.Image
                        renderStrategy: Canvas.Cooperative

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()

                            const cells = ctrl.cells
                            for (let i = 0; i < cells.length; ++i) {
                                const cell = cells[i]
                                if (cell.tileType === "empty")
                                    continue

                                let fill = "transparent"
                                if (cell.tileType === "wall")
                                    fill = "#D4DCE4"
                                else if (cell.tileType === "door")
                                    fill = "#C48A38"
                                else if (cell.tileType === "otherDoor")
                                    fill = "#8C949B"
                                else if (cell.tileType === "buildable" || cell.tileType === "bed" || cell.tileType === "otherBed")
                                    fill = "#748690"
                                else if (cell.tileType === "otherRoom")
                                    fill = "#56616A"
                                else if (cell.tileType === "corridor")
                                    fill = "#2B2E32"

                                const x = miniMapCard.mapOriginX + cell.column * miniMapCard.miniCellSize
                                const y = miniMapCard.mapOriginY + cell.row * miniMapCard.miniCellSize
                                ctx.fillStyle = fill
                                ctx.fillRect(x, y, miniMapCard.miniCellSize, miniMapCard.miniCellSize)

                            }

                        }
                    }

                    Rectangle {
                        id: miniGhost
                        visible: ctrl.ghostVisible
                        width: Math.max(4, miniMapCard.miniCellSize * 0.80)
                        height: width
                        radius: width / 2
                        color: "#FF6B6B"
                        border.width: 1
                        border.color: "#FFF4F4"
                        x: miniMapCard.mapOriginX + ctrl.ghostCenterColumn * miniMapCard.miniCellSize - width / 2
                        y: miniMapCard.mapOriginY + ctrl.ghostCenterRow * miniMapCard.miniCellSize - height / 2

                        Behavior on x {
                            enabled: root.synchronizedHumanGhost || !root.ghostMovementMode
                            NumberAnimation {
                                duration: root.ghostMoveDurationMs
                                easing.type: Easing.Linear
                            }
                        }

                        Behavior on y {
                            enabled: root.synchronizedHumanGhost || !root.ghostMovementMode
                            NumberAnimation {
                                duration: root.ghostMoveDurationMs
                                easing.type: Easing.Linear
                            }
                        }
                    }
                }

                Connections {
                    target: ctrl

                    function onBoardChanged() {
                        mainMapCanvas.requestPaint()
                        miniMapCanvas.requestPaint()
                    }
                }

                onMiniCellSizeChanged: miniMapCanvas.requestPaint()
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 150
                width: 110
                height: 110
                radius: 55
                visible: false
                color: ctrl.doorHp < ctrl.doorMaxHp ? "#2FBE66" : "#355C43"
                border.width: 6
                border.color: ctrl.doorHp < ctrl.doorMaxHp ? "#A0F2C1" : "#6E947B"
                opacity: ctrl.gameOver ? 0.45 : 1.0

                MouseArea {
                    anchors.fill: parent
                    enabled: false
                    onClicked: ctrl.repairDoor()
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "修"
                        color: "#FFFFFF"
                        font.pixelSize: 34
                        font.weight: Font.Black
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "修门"
                        color: "#F3FFF7"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                }
            }

            Rectangle {
                id: cameraPad
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                width: 108
                height: 108
                radius: 54
                visible: (root.cameraPanEnabled || root.ghostMovementMode) && !ctrl.gameOver
                color: "#16000000"
                border.width: 3
                border.color: root.ghostMovementMode ? "#4AF4AAAA" : "#37FFFFFF"

                Text {
                    visible: false
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 9
                    text: "视角"
                    color: "#EAF4FF"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    id: cameraStick
                    width: 38
                    height: 38
                    radius: 19
                    color: root.ghostMovementMode ? "#FFD7D7" : "#D8F3FF"
                    border.width: 2
                    border.color: root.ghostMovementMode ? "#E89191" : "#7ACAE8"
                    x: (cameraPad.width - width) / 2
                    y: (cameraPad.height - height) / 2

                    property real centerX: (cameraPad.width - width) / 2
                    property real centerY: (cameraPad.height - height) / 2
                    property real maxDistance: 26
                    property real panOriginX: 0
                    property real panOriginY: 0
                    property bool suppressCameraSync: false

                    function recenterStick() {
                        suppressCameraSync = true
                        x = centerX
                        y = centerY
                        suppressCameraSync = false
                        root.setGhostMoveDirection(0, 0)
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.OpenHandCursor
                        drag.target: parent
                        drag.minimumX: parent.centerX - parent.maxDistance
                        drag.maximumX: parent.centerX + parent.maxDistance
                        drag.minimumY: parent.centerY - parent.maxDistance
                        drag.maximumY: parent.centerY + parent.maxDistance

                        onPressed: {
                            cursorShape = Qt.ClosedHandCursor
                            root.forceActiveFocus()
                            parent.panOriginX = root.cameraPanX
                            parent.panOriginY = root.cameraPanY
                        }

                        onPositionChanged: {
                            if (parent.suppressCameraSync)
                                return

                            const normalizedX = (parent.x - parent.centerX) / parent.maxDistance
                            const normalizedY = (parent.y - parent.centerY) / parent.maxDistance

                            if (root.ghostMovementMode) {
                                root.updateGhostMoveDirectionFromPad(normalizedX, normalizedY)
                            } else {
                                root.setCameraPan(parent.panOriginX + normalizedX * mapViewport.cellSize * 5.0,
                                                  parent.panOriginY + normalizedY * mapViewport.cellSize * 5.0)
                            }
                        }

                        onReleased: {
                            cursorShape = Qt.OpenHandCursor
                            parent.recenterStick()
                        }

                        onCanceled: {
                            cursorShape = Qt.OpenHandCursor
                            parent.recenterStick()
                        }
                    }
                }
            }

            Rectangle {
                visible: root.buildPanelVisible
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                width: Math.min(parent.width - 34, 320)
                height: 126
                radius: 18
                color: "#D2192025"
                border.width: 1
                border.color: "#66E7F0F6"
                z: 40

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: "选择要建造的建筑"
                        color: "#F8FCFF"
                        font.pixelSize: 15
                        font.weight: Font.Black
                        horizontalAlignment: Text.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            radius: 14
                            color: "#163329"
                            border.width: 1
                            border.color: "#4BE37D"

                            MouseArea {
                                anchors.fill: parent
                                enabled: root.canAffordBuildType("generator")
                                onClicked: root.buildSelectedType("generator")
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 2

                                Text {
                                    text: "发电机"
                                    color: "#F7FFFA"
                                    font.pixelSize: 14
                                    font.weight: Font.Black
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: "G:200  P:0"
                                    color: root.canAffordBuildType("generator") ? "#A8F1C7" : "#FFB3B3"
                                    font.pixelSize: 11
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            radius: 14
                            color: "#33251A"
                            border.width: 1
                            border.color: "#FFD2B0"

                            MouseArea {
                                anchors.fill: parent
                                enabled: root.canAffordBuildType("turret")
                                onClicked: root.buildSelectedType("turret")
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 2

                                Text {
                                    text: "炮台"
                                    color: "#FFF8F2"
                                    font.pixelSize: 14
                                    font.weight: Font.Black
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: "G:8  P:1"
                                    color: root.canAffordBuildType("turret") ? "#FFD9B2" : "#FFB3B3"
                                    font.pixelSize: 11
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        secondary: true
                        text: "取消"
                        onClicked: root.closeBuildPanel()
                    }
                }
            }

            Rectangle {
                visible: root.actionPanelVisible
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                width: Math.min(parent.width - 34, 290)
                height: 168
                radius: 18
                color: "#D61A2025"
                border.width: 1
                border.color: "#66E7F0F6"
                z: 40

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: root.pendingActionTitle
                        color: "#F8FCFF"
                        font.pixelSize: 15
                        font.weight: Font.Black
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.pendingActionDetail
                        color: "#D7E8F2"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.pendingActionRequirement
                        color: root.pendingActionRequirementMet ? "#C7FFD8" : "#FFB3B3"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "金币 " + root.pendingActionGoldCost
                              + (root.pendingActionPowerCost > 0 ? "   电力 " + root.pendingActionPowerCost : "")
                        color: root.canAffordPendingAction() ? "#A8F1C7" : "#FFB3B3"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ActionButton {
                            Layout.fillWidth: true
                            secondary: true
                            text: "取消"
                            onClicked: root.closeActionPanel()
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: root.pendingActionVerb
                            enabled: root.pendingActionRequirementMet && root.canAffordPendingAction()
                            opacity: enabled ? 1.0 : 0.55
                            onClicked: root.confirmPendingAction()
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: ctrl.roleSelectionRequired
            color: "#B8000000"
            z: 80

            Rectangle {
                width: Math.min(parent.width - 40, 360)
                height: 250
                anchors.centerIn: parent
                radius: 24
                color: "#251B1A"
                border.width: 1
                border.color: "#5FFFFFFF"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "选择阵营"
                        color: "#FFFFFF"
                        font.pixelSize: 24
                        font.weight: Font.Black
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "开始游戏后有 10 秒选择时间。超时未选择会随机分配，全场只会有 1 个鬼。"
                        color: "#E8E8E8"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "倒计时 " + ctrl.roleSelectionCountdown + "s"
                        color: "#FFD27A"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        text: ctrl.roleSelectionChoice === "defender" ? "守夜人 - 已选择" : "选择守夜人"
                        secondary: ctrl.roleSelectionChoice === "ghost"
                        onClicked: ctrl.submitRoleSelection(false)
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        secondary: ctrl.roleSelectionChoice !== "ghost"
                        enabled: !ctrl.ghostRoleLocked || ctrl.roleSelectionChoice === "ghost"
                        opacity: enabled ? 1.0 : 0.55
                        text: ctrl.roleSelectionChoice === "ghost"
                            ? "鬼 - 已选择"
                            : (ctrl.ghostRoleLocked ? "鬼 - 已被选择" : "选择鬼")
                        onClicked: ctrl.submitRoleSelection(true)
                    }
                }
            }
        }

        Rectangle {
            visible: root.localDefenderEliminated
            z: 900
            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 36, 240)
            height: 54
            radius: 8
            color: "#D9292020"
            border.width: 1
            border.color: "#8FFF9B91"

            Text {
                anchors.centerIn: parent
                text: "你已死亡"
                color: "#FFB0A8"
                font.pixelSize: 20
                font.weight: Font.Bold
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: ctrl.gameOver
            z: 1000
            color: "#A0000000"

            Rectangle {
                width: Math.min(parent.width - 40, 340)
                height: Math.max(220, resultLayout.implicitHeight + 32)
                anchors.centerIn: parent
                radius: 24
                color: "#251B1A"
                border.width: 1
                border.color: "#5FFFFFFF"

                ColumnLayout {
                    id: resultLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: root.resultTitle
                        color: root.localResultVictory ? "#8FF0AD" : "#FF9B91"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 24
                        font.weight: Font.Black
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: root.resultMessage
                        color: "#E8E8E8"
                        font.pixelSize: 14
                    }

                    Item {
                        Layout.fillHeight: true
                        Layout.minimumHeight: 4
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ActionButton {
                            Layout.fillWidth: true
                            text: root.networkRoom ? "返回房间" : "返回首页"
                            onClicked: root.leaveCurrentGame()
                        }
                    }
                }
            }
        }
    }
}
