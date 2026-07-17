import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "flightChessPage"

    readonly property var playerColors: ["#000000", "#D94B45", "#3169B8", "#54B96E", "#F2C94C"]
    readonly property var boardColors: ["#F2C94C", "#5AA7E8", "#54B96E", "#E7584F"]
    readonly property var boardLines: ["#C59A1F", "#2A6DA5", "#2F7D43", "#A9322B"]
    readonly property int trackLength: 52
    readonly property int homeLength: 6
    property bool diceRolling: false
    property int rollingDiceValue: 0
    property real diceTiltX: 0
    property real diceTiltY: 0
    property real diceSpin: 0
    property var pendingMoveStart: null
    property bool moveAnimating: false
    property int animatedPlanePlayer: 0
    property int animatedPlaneIndex: -1
    property real animatedPlaneX: 0
    property real animatedPlaneY: 0
    property var animatedPath: []
    property int animatedPathIndex: 0
    readonly property bool networkRoom: AppCtrl.networkManager.isHost
                                     || AppCtrl.networkManager.isConnected

    function localRoomPlayer() {
        var players = AppCtrl.roomManager.playerList
        var idx = AppCtrl.roomManager.localPlayerIndex
        return idx >= 0 && idx < players.length ? players[idx] : null
    }

    function localIsSpectator() {
        var player = localRoomPlayer()
        return !!player && (player.seatType || "active") === "spectator"
    }

    function localFlightRole() {
        var player = localRoomPlayer()
        if (!player || (player.seatType || "active") !== "active")
            return 0
        return player.isHost ? 1 : 2
    }

    function localCanOperate() {
        if (!networkRoom)
            return true
        if (localIsSpectator())
            return false
        return localFlightRole() === AppCtrl.flightChessController.currentPlayer
    }

    function triggerRoll() {
        root.diceRolling = true
        root.rollingDiceValue = 1 + Math.floor(Math.random() * 6)
        diceRollTimer.ticks = 0
        diceRollTimer.restart()
    }

    function leaveCurrentGame() {
        AppCtrl.returnFromFlightChessGame()
    }

    function boardMetrics() {
        var side = Math.min(boardCanvas.width, boardCanvas.height) * 0.92
        var cell = side / 15
        return { cell: cell, left: (boardCanvas.width - side) / 2, top: (boardCanvas.height - side) / 2 }
    }

    function gridPoint(row, col) {
        var m = boardMetrics()
        return Qt.point(m.left + (col + 0.5) * m.cell,
                        m.top + (row + 0.5) * m.cell)
    }

    function pathCell(globalPosition) {
        var cells = [
            [6, 0], [6, 1], [6, 2], [6, 3], [6, 4], [6, 5], [5, 6], [4, 6], [3, 6], [2, 6], [1, 6], [0, 6], [0, 7],
            [0, 8], [1, 8], [2, 8], [3, 8], [4, 8], [5, 8], [6, 9], [6, 10], [6, 11], [6, 12], [6, 13], [6, 14], [7, 14],
            [8, 14], [8, 13], [8, 12], [8, 11], [8, 10], [8, 9], [9, 8], [10, 8], [11, 8], [12, 8], [13, 8], [14, 8], [14, 7],
            [14, 6], [13, 6], [12, 6], [11, 6], [10, 6], [9, 6], [8, 5], [8, 4], [8, 3], [8, 2], [8, 1], [8, 0], [7, 0]
        ]
        return cells[globalPosition % cells.length]
    }

    function boardPoint(globalPosition) {
        var cell = pathCell(globalPosition)
        return gridPoint(cell[0], cell[1])
    }

    function startOffsetForPlayer(player) {
        return player === 1 ? 41 : 15
    }

    function relativePoint(player, position) {
        if (position >= root.trackLength) {
            var laneStep = position - root.trackLength + 1
            return homeLanePoint(playerHomeEdge(player), laneStep)
        }

        var globalPosition = (startOffsetForPlayer(player) + position) % root.trackLength
        return boardPoint(globalPosition)
    }

    function centerPoint() {
        return gridPoint(7, 7)
    }

    function homeLanePoint(edge, laneStep) {
        var lanes = [
            [[1, 7], [2, 7], [3, 7], [4, 7], [5, 7], [6, 7]],
            [[7, 13], [7, 12], [7, 11], [7, 10], [7, 9], [7, 8]],
            [[13, 7], [12, 7], [11, 7], [10, 7], [9, 7], [8, 7]],
            [[7, 1], [7, 2], [7, 3], [7, 4], [7, 5], [7, 6]]
        ]
        var cell = lanes[edge][Math.max(0, Math.min(root.homeLength - 1, laneStep - 1))]
        return gridPoint(cell[0], cell[1])
    }

    function playerHomeEdge(player) {
        return player === 1 ? 2 : 0
    }

    function baseRect(baseIndex) {
        var m = boardMetrics()
        var size = m.cell * 5
        if (baseIndex === 0)
            return Qt.rect(m.left, m.top, size, size)
        if (baseIndex === 1)
            return Qt.rect(m.left + m.cell * 10, m.top, size, size)
        if (baseIndex === 2)
            return Qt.rect(m.left, m.top + m.cell * 10, size, size)
        return Qt.rect(m.left + m.cell * 10, m.top + m.cell * 10, size, size)
    }

    function playerBaseIndex(player) {
        return player === 1 ? 2 : 1
    }

    function baseSpotPoint(baseIndex, spotIndex) {
        var base = baseRect(baseIndex)
        return Qt.point(base.x + base.width * (spotIndex % 2 === 0 ? 0.32 : 0.68),
                        base.y + base.height * (spotIndex < 2 ? 0.32 : 0.68))
    }

    function launchPoint(player) {
        return player === 1 ? gridPoint(12, 5) : gridPoint(2, 9)
    }

    function launchSpotPoint(player, spotIndex) {
        var p = launchPoint(player)
        var m = boardMetrics()
        var offset = m.cell * 0.22
        return Qt.point(p.x + (spotIndex % 2 === 0 ? -offset : offset),
                        p.y + (spotIndex < 2 ? -offset : offset))
    }

    function trackFillColor(globalPosition) {
        var colorIndex = ((globalPosition - 2) % 4 + 4) % 4
        return root.boardColors[colorIndex]
    }

    function trackLineColor(globalPosition) {
        var colorIndex = ((globalPosition - 2) % 4 + 4) % 4
        return root.boardLines[colorIndex]
    }

    function launchReady(player) {
        if (AppCtrl.flightChessController.gameOver
                || !AppCtrl.flightChessController.hasRolled
                || AppCtrl.flightChessController.diceValue !== 6
                || AppCtrl.flightChessController.currentPlayer !== player) {
            return false
        }

        var planes = AppCtrl.flightChessController.planes
        for (var i = 0; i < planes.length; ++i) {
            var plane = planes[i]
            if (plane.player === player && plane.inBase && plane.canMove)
                return true
        }
        return false
    }

    function scoreForPlayer(player) {
        var scores = AppCtrl.flightChessController.scores
        for (var i = 0; i < scores.length; ++i) {
            if (scores[i].player === player)
                return scores[i].value
        }
        return 0
    }

    function drawRoundRect(ctx, x, y, width, height, radius) {
        var r = Math.min(radius, width / 2, height / 2)
        ctx.beginPath()
        ctx.moveTo(x + r, y)
        ctx.lineTo(x + width - r, y)
        ctx.quadraticCurveTo(x + width, y, x + width, y + r)
        ctx.lineTo(x + width, y + height - r)
        ctx.quadraticCurveTo(x + width, y + height, x + width - r, y + height)
        ctx.lineTo(x + r, y + height)
        ctx.quadraticCurveTo(x, y + height, x, y + height - r)
        ctx.lineTo(x, y + r)
        ctx.quadraticCurveTo(x, y, x + r, y)
        ctx.closePath()
    }

    function drawBoardCell(ctx, x, y, size, fillColor, strokeColor, highlight) {
        ctx.save()
        ctx.shadowColor = highlight ? "rgba(0, 0, 0, 0.14)" : "rgba(0, 0, 0, 0.06)"
        ctx.shadowBlur = highlight ? 4 : 1.5
        ctx.shadowOffsetY = highlight ? 2 : 1
        root.drawRoundRect(ctx, x - size / 2, y - size / 2, size, size, size * 0.1)
        ctx.fillStyle = fillColor
        ctx.fill()
        ctx.shadowColor = "transparent"
        ctx.strokeStyle = strokeColor
        ctx.lineWidth = highlight ? 2.2 : 1.2
        ctx.stroke()

        ctx.beginPath()
        ctx.moveTo(x - size * 0.34, y - size * 0.2)
        ctx.quadraticCurveTo(x - size * 0.14, y - size * 0.38, x + size * 0.18, y - size * 0.34)
        ctx.strokeStyle = "rgba(255, 255, 255, 0.35)"
        ctx.lineWidth = 1.4
        ctx.stroke()
        ctx.restore()
    }

    function drawLaunchCell(ctx, x, y, size, fillColor, strokeColor, active) {
        ctx.save()
        if (active) {
            ctx.shadowColor = "rgba(245, 212, 94, 0.9)"
            ctx.shadowBlur = size * 0.75
            ctx.shadowOffsetY = 0
            root.drawRoundRect(ctx, x - size * 0.62, y - size * 0.62,
                               size * 1.24, size * 1.24, size * 0.16)
            ctx.fillStyle = "rgba(245, 212, 94, 0.26)"
            ctx.fill()
        }

        ctx.shadowColor = active ? "rgba(0, 0, 0, 0.2)" : "rgba(0, 0, 0, 0.07)"
        ctx.shadowBlur = active ? 5 : 1.5
        ctx.shadowOffsetY = active ? 2 : 1
        root.drawRoundRect(ctx, x - size / 2, y - size / 2, size, size, size * 0.12)
        ctx.fillStyle = fillColor
        ctx.fill()
        ctx.shadowColor = "transparent"
        ctx.strokeStyle = active ? "#F5D45E" : strokeColor
        ctx.lineWidth = active ? 2.6 : 1.2
        ctx.stroke()

        ctx.beginPath()
        ctx.moveTo(x - size * 0.2, y + size * 0.2)
        ctx.lineTo(x, y - size * 0.24)
        ctx.lineTo(x + size * 0.2, y + size * 0.2)
        ctx.closePath()
        ctx.fillStyle = active ? "rgba(255, 255, 255, 0.9)" : "rgba(255, 255, 255, 0.42)"
        ctx.fill()
        ctx.restore()
    }

    function planePoint(plane) {
        if (plane.inBase || plane.position === -1)
            return baseSpotPoint(playerBaseIndex(plane.player), plane.index)
        if (plane.inLaunch || plane.position === -2)
            return launchSpotPoint(plane.player, plane.index)
        if (plane.finished) {
            var center = centerPoint()
            var offset = 8
            return Qt.point(center.x + (plane.index % 2) * offset - offset / 2,
                            center.y + Math.floor(plane.index / 2) * offset - offset / 2)
        }
        if (plane.position >= root.trackLength) {
            var laneStep = plane.position - root.trackLength + 1
            return homeLanePoint(playerHomeEdge(plane.player), laneStep)
        }

        return boardPoint(plane.globalPosition)
    }

    function planeSnapshot(plane) {
        return {
            player: plane.player,
            index: plane.index,
            position: plane.position,
            globalPosition: plane.globalPosition,
            inBase: plane.inBase,
            inLaunch: plane.inLaunch,
            finished: plane.finished
        }
    }

    function findPlane(player, index) {
        var planes = AppCtrl.flightChessController.planes
        for (var i = 0; i < planes.length; ++i) {
            if (planes[i].player === player && planes[i].index === index)
                return planes[i]
        }
        return null
    }

    function pointForSnapshot(snapshot) {
        if (snapshot.inBase || snapshot.position === -1)
            return baseSpotPoint(playerBaseIndex(snapshot.player), snapshot.index)
        if (snapshot.inLaunch || snapshot.position === -2)
            return launchSpotPoint(snapshot.player, snapshot.index)
        if (snapshot.finished) {
            var center = centerPoint()
            var offset = 8
            return Qt.point(center.x + (snapshot.index % 2) * offset - offset / 2,
                            center.y + Math.floor(snapshot.index / 2) * offset - offset / 2)
        }
        return relativePoint(snapshot.player, snapshot.position)
    }

    function buildMovePath(start, end) {
        var rawPath = [pointForSnapshot(start)]

        if (start.position === -1 || end.position === -2) {
            rawPath.push(pointForSnapshot(planeSnapshot(end)))
            return orthogonalMovePath(rawPath)
        }

        if (start.position === -2) {
            rawPath.push(launchSpotPoint(start.player, start.index))
            for (var launchStep = 0; launchStep <= end.position; ++launchStep)
                rawPath.push(relativePoint(start.player, launchStep))
            return orthogonalMovePath(rawPath)
        }

        if (end.position < 0) {
            rawPath.push(pointForSnapshot(planeSnapshot(end)))
            return orthogonalMovePath(rawPath)
        }

        var step = end.position >= start.position ? 1 : -1
        for (var pos = start.position + step; step > 0 ? pos <= end.position : pos >= end.position; pos += step)
            rawPath.push(relativePoint(start.player, pos))

        return orthogonalMovePath(rawPath)
    }

    function orthogonalMovePath(rawPath) {
        if (rawPath.length <= 1)
            return rawPath

        var path = [rawPath[0]]
        for (var i = 1; i < rawPath.length; ++i) {
            var previous = path[path.length - 1]
            var next = rawPath[i]
            var dx = Math.abs(next.x - previous.x)
            var dy = Math.abs(next.y - previous.y)

            if (dx > 1 && dy > 1)
                path.push(Qt.point(next.x, previous.y))

            path.push(next)
        }

        return path
    }

    function startMoveAnimation(start, end) {
        var path = buildMovePath(start, end)
        if (path.length < 2) {
            boardCanvas.requestPaint()
            return
        }

        root.animatedPlanePlayer = start.player
        root.animatedPlaneIndex = start.index
        root.animatedPath = path
        root.animatedPathIndex = 0
        root.animatedPlaneX = path[0].x
        root.animatedPlaneY = path[0].y
        root.moveAnimating = true
        moveStepTimer.restart()
        boardCanvas.requestPaint()
    }

    function consumePendingMoveAnimation() {
        if (root.pendingMoveStart === null)
            return false

        var start = root.pendingMoveStart
        root.pendingMoveStart = null
        var end = findPlane(start.player, start.index)
        if (end === null)
            return false

        startMoveAnimation(start, end)
        return true
    }

    function drawPlane(ctx, x, y, color, label, selected, scale) {
        ctx.save()
        ctx.translate(x, y)
        ctx.scale(scale, scale)

        ctx.shadowColor = "rgba(0, 0, 0, 0.28)"
        ctx.shadowBlur = selected ? 9 : 4
        ctx.shadowOffsetY = 3

        ctx.fillStyle = color
        ctx.strokeStyle = selected ? "#F5D45E" : "#FFFFFF"
        ctx.lineWidth = selected ? 2.8 : 1.8

        ctx.beginPath()
        ctx.moveTo(0, -19)
        ctx.quadraticCurveTo(6, -11, 5, -2)
        ctx.lineTo(20, 6)
        ctx.quadraticCurveTo(23, 8, 21, 12)
        ctx.lineTo(19, 15)
        ctx.lineTo(5, 10)
        ctx.lineTo(3, 18)
        ctx.lineTo(11, 24)
        ctx.lineTo(9, 28)
        ctx.lineTo(0, 24)
        ctx.lineTo(-9, 28)
        ctx.lineTo(-11, 24)
        ctx.lineTo(-3, 18)
        ctx.lineTo(-5, 10)
        ctx.lineTo(-19, 15)
        ctx.lineTo(-21, 12)
        ctx.quadraticCurveTo(-23, 8, -20, 6)
        ctx.lineTo(-5, -2)
        ctx.quadraticCurveTo(-6, -11, 0, -19)
        ctx.closePath()
        ctx.fill()
        ctx.stroke()

        ctx.shadowColor = "transparent"
        ctx.fillStyle = "rgba(255, 255, 255, 0.72)"
        ctx.beginPath()
        ctx.ellipse(0, -7, 4.2, 7.6, 0, 0, Math.PI * 2)
        ctx.fill()
        ctx.strokeStyle = "rgba(0, 0, 0, 0.12)"
        ctx.lineWidth = 1
        ctx.stroke()

        ctx.beginPath()
        ctx.moveTo(-3, -15)
        ctx.lineTo(0, -20)
        ctx.lineTo(3, -15)
        ctx.closePath()
        ctx.fillStyle = "#FFFFFF"
        ctx.fill()

        ctx.fillStyle = "#FFFFFF"
        ctx.font = "bold 9px sans-serif"
        ctx.textAlign = "center"
        ctx.textBaseline = "middle"
        ctx.fillText(label, 0, 7.5)

        ctx.restore()
    }

    background: Rectangle {
        color: "transparent"
    }

    Connections {
        target: AppCtrl.flightChessController
        function onBoardChanged() {
            if (!root.consumePendingMoveAnimation())
                boardCanvas.requestPaint()
        }
        function onTurnChanged() { boardCanvas.requestPaint() }
        function onDiceChanged() { boardCanvas.requestPaint() }
        function onGameOverChanged() { boardCanvas.requestPaint() }
    }

    Timer {
        id: moveStepTimer
        interval: 60
        repeat: true

        onTriggered: {
            if (!root.moveAnimating || root.animatedPathIndex >= root.animatedPath.length - 1) {
                stop()
                root.moveAnimating = false
                boardCanvas.requestPaint()
                return
            }

            root.animatedPathIndex += 1
            var point = root.animatedPath[root.animatedPathIndex]
            root.animatedPlaneX = point.x
            root.animatedPlaneY = point.y
            boardCanvas.requestPaint()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: AppTheme.spacingLg
        anchors.rightMargin: AppTheme.spacingLg
        anchors.topMargin: 20
        anchors.bottomMargin: 10
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: "飞行棋"
                    color: AppTheme.textPrimary
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: root.networkRoom ? "联机双人" : "本地双人"
                    color: AppTheme.textMuted
                    font.pixelSize: 13
                }
            }

            Rectangle {
                Layout.preferredWidth: 74
                Layout.preferredHeight: 54
                radius: 16
                color: "#EFE6D0"

                Rectangle {
                    id: diceFace
                    width: 44
                    height: 44
                    anchors.centerIn: parent
                    color: "transparent"
                    scale: root.diceRolling ? 0.94 + (diceRollTimer.ticks % 3) * 0.03 : 1

                    transform: [
                        Rotation {
                            origin.x: diceFace.width / 2
                            origin.y: diceFace.height / 2
                            axis.x: 1
                            axis.y: 0
                            axis.z: 0
                            angle: root.diceRolling ? root.diceTiltX : 0
                        },
                        Rotation {
                            origin.x: diceFace.width / 2
                            origin.y: diceFace.height / 2
                            axis.x: 0
                            axis.y: 1
                            axis.z: 0
                            angle: root.diceRolling ? root.diceTiltY : 0
                        },
                        Rotation {
                            origin.x: diceFace.width / 2
                            origin.y: diceFace.height / 2
                            axis.x: 0
                            axis.y: 0
                            axis.z: 1
                            angle: root.diceRolling ? root.diceSpin : 0
                        }
                    ]

                    property int displayValue: root.diceRolling
                        ? root.rollingDiceValue
                        : AppCtrl.flightChessController.diceValue

                    function dotVisible(index) {
                        if (displayValue <= 0)
                            return false
                        if (displayValue === 1)
                            return index === 4
                        if (displayValue === 2)
                            return index === 0 || index === 8
                        if (displayValue === 3)
                            return index === 0 || index === 4 || index === 8
                        if (displayValue === 4)
                            return index === 0 || index === 2 || index === 6 || index === 8
                        if (displayValue === 5)
                            return index === 0 || index === 2 || index === 4 || index === 6 || index === 8
                        return index !== 1 && index !== 7 && index !== 4
                    }

                    Canvas {
                        id: diceCanvas
                        anchors.fill: parent

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()

                            var sideShade = root.diceRolling ? "#B9A267" : "#D7C8A0"
                            var frontShade = root.diceRolling ? "#FFF8E5" : "#FFFFFF"

                            ctx.fillStyle = sideShade
                            ctx.beginPath()
                            ctx.moveTo(9, 9)
                            ctx.lineTo(37, 9)
                            ctx.lineTo(41, 13)
                            ctx.lineTo(41, 37)
                            ctx.lineTo(37, 41)
                            ctx.lineTo(9, 41)
                            ctx.closePath()
                            ctx.fill()

                            ctx.fillStyle = "#B59E63"
                            ctx.beginPath()
                            ctx.moveTo(36, 4)
                            ctx.lineTo(40, 8)
                            ctx.lineTo(40, 36)
                            ctx.lineTo(36, 40)
                            ctx.closePath()
                            ctx.fill()

                            ctx.fillStyle = "#E3D193"
                            ctx.beginPath()
                            ctx.moveTo(8, 4)
                            ctx.lineTo(36, 4)
                            ctx.lineTo(40, 8)
                            ctx.lineTo(12, 8)
                            ctx.closePath()
                            ctx.fill()

                            ctx.fillStyle = frontShade
                            ctx.strokeStyle = "#BFAE82"
                            ctx.lineWidth = 2
                            ctx.beginPath()
                            ctx.moveTo(8, 4)
                            ctx.lineTo(36, 4)
                            ctx.quadraticCurveTo(40, 4, 40, 8)
                            ctx.lineTo(40, 36)
                            ctx.quadraticCurveTo(40, 40, 36, 40)
                            ctx.lineTo(8, 40)
                            ctx.quadraticCurveTo(4, 40, 4, 36)
                            ctx.lineTo(4, 8)
                            ctx.quadraticCurveTo(4, 4, 8, 4)
                            ctx.closePath()
                            ctx.fill()
                            ctx.stroke()

                            ctx.beginPath()
                            ctx.moveTo(9, 8)
                            ctx.lineTo(35, 8)
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.55)"
                            ctx.lineWidth = 1.2
                            ctx.stroke()
                        }
                    }

                    Repeater {
                        model: 9

                        Rectangle {
                            width: 7
                            height: 7
                            radius: 3.5
                            color: "#173A31"
                            visible: diceFace.dotVisible(index)
                            x: 8.5 + (index % 3) * 10
                            y: 8.5 + Math.floor(index / 3) * 10
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: diceFace.displayValue <= 0
                        text: "-"
                        color: AppTheme.textMuted
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        Timer {
            id: diceRollTimer
            interval: 55
            repeat: true
            property int ticks: 0

            onTriggered: {
                root.rollingDiceValue = 1 + Math.floor(Math.random() * 6)
                root.diceTiltX = -34 + Math.random() * 68
                root.diceTiltY = -34 + Math.random() * 68
                root.diceSpin += 42 + Math.random() * 38
                diceCanvas.requestPaint()
                ticks += 1
                if (ticks < 12)
                    return

                stop()
                root.diceRolling = false
                root.diceTiltX = 0
                root.diceTiltY = 0
                root.diceSpin = 0
                diceCanvas.requestPaint()
                ticks = 0
                if (AppCtrl.networkManager.isHost) {
                    var diceValue = AppCtrl.flightChessController.rollDice()
                    if (diceValue > 0)
                        AppCtrl.networkManager.broadcastFlightRoll(1, diceValue)
                } else if (root.networkRoom) {
                    AppCtrl.networkManager.sendFlightRoll()
                } else {
                    AppCtrl.flightChessController.rollDice()
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AppCtrl.flightChessController.statusText
            color: AppTheme.textPrimary
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }

        Rectangle {
            id: boardContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 300
            Layout.maximumWidth: 382
            Layout.alignment: Qt.AlignHCenter
            radius: 26
            color: "#F8F2E4"
            border.width: 1
            border.color: "#E0D0AD"

            Canvas {
                id: boardCanvas
                anchors.fill: parent
                anchors.margins: 14

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()

                    var center = root.centerPoint()
                    var m = root.boardMetrics()
                    var bases = [
                        { rect: root.baseRect(0), color: "#F2C94C", line: "#C59A1F" },
                        { rect: root.baseRect(1), color: "#5AA7E8", line: "#2A6DA5" },
                        { rect: root.baseRect(2), color: "#E7584F", line: "#A9322B" },
                        { rect: root.baseRect(3), color: "#54B96E", line: "#2F7D43" }
                    ]

                    ctx.fillStyle = "#FFFFFF"
                    ctx.fillRect(0, 0, width, height)

                    for (var b = 0; b < bases.length; ++b) {
                        var base = bases[b]
                        root.drawRoundRect(ctx, base.rect.x, base.rect.y, base.rect.width, base.rect.height, m.cell * 0.28)
                        ctx.fillStyle = base.color
                        ctx.strokeStyle = base.line
                        ctx.lineWidth = 2
                        ctx.fill()
                        ctx.stroke()

                        root.drawRoundRect(ctx,
                                           base.rect.x + m.cell * 0.55,
                                           base.rect.y + m.cell * 0.55,
                                           base.rect.width - m.cell * 1.1,
                                           base.rect.height - m.cell * 1.1,
                                           m.cell * 0.32)
                        ctx.fillStyle = "rgba(255, 255, 255, 0.24)"
                        ctx.fill()

                        for (var spot = 0; spot < 4; ++spot) {
                            var spotPoint = root.baseSpotPoint(b, spot)
                            root.drawBoardCell(ctx, spotPoint.x, spotPoint.y, m.cell * 0.9,
                                               "#FFF8EA", "#FFFFFF", true)
                        }
                    }

                    var launchZones = [
                        { player: 1, color: "#E7584F", line: "#A9322B" },
                        { player: 2, color: "#5AA7E8", line: "#2A6DA5" }
                    ]
                    for (var launchIndex = 0; launchIndex < launchZones.length; ++launchIndex) {
                        var launch = launchZones[launchIndex]
                        var launchCenter = root.launchPoint(launch.player)
                        root.drawLaunchCell(ctx, launchCenter.x, launchCenter.y, m.cell * 0.68,
                                            launch.color, launch.line, root.launchReady(launch.player))
                    }

                    var laneColors = ["#5AA7E8", "#54B96E", "#E7584F", "#F2C94C"]
                    for (var edge = 0; edge < 4; ++edge) {
                        for (var lane = 1; lane <= root.homeLength; ++lane) {
                            var lanePoint = root.homeLanePoint(edge, lane)
                            root.drawBoardCell(ctx, lanePoint.x, lanePoint.y, m.cell * 0.64,
                                               lane === root.homeLength ? "#FFFFFF" : laneColors[edge],
                                               laneColors[edge], lane === root.homeLength)
                        }
                    }

                    for (var i = 0; i < root.trackLength; ++i) {
                        var p = root.boardPoint(i)
                        root.drawBoardCell(ctx, p.x, p.y,
                                           i % 13 === 0 ? m.cell * 0.96 : m.cell * 0.82,
                                           root.trackFillColor(i),
                                           root.trackLineColor(i),
                                           i % 13 === 0)
                    }

                    ctx.save()
                    ctx.beginPath()
                    ctx.arc(center.x, center.y, m.cell * 0.84, 0, Math.PI * 2)
                    ctx.fillStyle = "#FFFFFF"
                    ctx.fill()
                    ctx.strokeStyle = "#173A31"
                    ctx.lineWidth = 2
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.moveTo(center.x, center.y - m.cell * 0.62)
                    ctx.lineTo(center.x + m.cell * 0.18, center.y - m.cell * 0.18)
                    ctx.lineTo(center.x + m.cell * 0.62, center.y)
                    ctx.lineTo(center.x + m.cell * 0.18, center.y + m.cell * 0.18)
                    ctx.lineTo(center.x, center.y + m.cell * 0.62)
                    ctx.lineTo(center.x - m.cell * 0.18, center.y + m.cell * 0.18)
                    ctx.lineTo(center.x - m.cell * 0.62, center.y)
                    ctx.lineTo(center.x - m.cell * 0.18, center.y - m.cell * 0.18)
                    ctx.closePath()
                    var finishGradient = ctx.createLinearGradient(center.x - m.cell, center.y - m.cell,
                                                                  center.x + m.cell, center.y + m.cell)
                    finishGradient.addColorStop(0, "#F2C94C")
                    finishGradient.addColorStop(0.34, "#5AA7E8")
                    finishGradient.addColorStop(0.67, "#54B96E")
                    finishGradient.addColorStop(1, "#E7584F")
                    ctx.fillStyle = finishGradient
                    ctx.fill()
                    ctx.restore()

                    var planes = AppCtrl.flightChessController.planes
                    for (var j = 0; j < planes.length; ++j) {
                        var plane = planes[j]
                        var point = root.planePoint(plane)
                        if (root.moveAnimating
                                && plane.player === root.animatedPlanePlayer
                                && plane.index === root.animatedPlaneIndex) {
                            point = Qt.point(root.animatedPlaneX, root.animatedPlaneY)
                        }
                        var color = root.playerColors[plane.player]
                        var scale = plane.inBase ? 0.28 : (plane.inLaunch ? 0.46 : (plane.canMove ? 0.62 : 0.55))
                        root.drawPlane(ctx, point.x, point.y, color, String(plane.index + 1),
                                       plane.canMove && !plane.finished, scale)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: AppCtrl.flightChessController.hasRolled
                             && !AppCtrl.flightChessController.gameOver
                             && !root.moveAnimating
                             && root.localCanOperate()
                    onClicked: function(mouse) {
                        var planes = AppCtrl.flightChessController.planes
                        for (var i = 0; i < planes.length; ++i) {
                            var plane = planes[i]
                            if (!plane.canMove)
                                continue

                            var point = root.planePoint(plane)
                            var dx = mouse.x - point.x
                            var dy = mouse.y - point.y
                            if (Math.sqrt(dx * dx + dy * dy) <= 20) {
                                root.pendingMoveStart = root.planeSnapshot(plane)
                                if (AppCtrl.networkManager.isHost) {
                                    if (!AppCtrl.flightChessController.movePlane(plane.index)) {
                                        root.pendingMoveStart = null
                                        return
                                    }
                                    AppCtrl.networkManager.broadcastFlightMove(1, plane.index)
                                } else if (root.networkRoom) {
                                    AppCtrl.networkManager.sendFlightMove(plane.index)
                                } else if (!AppCtrl.flightChessController.movePlane(plane.index)) {
                                    root.pendingMoveStart = null
                                }
                                return
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: 14
                color: AppCtrl.flightChessController.currentPlayer === 1 ? "#F8DED9" : "#F8E7E5"
                border.width: AppCtrl.flightChessController.currentPlayer === 1 ? 2 : 1
                border.color: AppCtrl.flightChessController.currentPlayer === 1 ? "#D94B45" : "#EBC9C4"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "玩家 1"
                        color: "#9E302B"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: root.scoreForPlayer(1) + " 分"
                        color: "#9E302B"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: 14
                color: AppCtrl.flightChessController.currentPlayer === 2 ? "#DDEBFA" : "#E8F0FA"
                border.width: AppCtrl.flightChessController.currentPlayer === 2 ? 2 : 1
                border.color: AppCtrl.flightChessController.currentPlayer === 2 ? "#3169B8" : "#C9D8EA"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "玩家 2"
                        color: "#244E8D"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: root.scoreForPlayer(2) + " 分"
                        color: "#244E8D"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                }
            }
        }

        ActionButton {
            Layout.fillWidth: true
            text: AppCtrl.flightChessController.gameOver ? "重新开始" : (root.diceRolling ? "投掷中" : "掷骰")
            enabled: (!root.networkRoom && AppCtrl.flightChessController.gameOver)
                     || (!AppCtrl.flightChessController.hasRolled
                         && !root.diceRolling
                         && !AppCtrl.flightChessController.gameOver
                         && root.localCanOperate())
            onClicked: {
                if (AppCtrl.flightChessController.gameOver) {
                    root.pendingMoveStart = null
                    root.moveAnimating = false
                    moveStepTimer.stop()
                    AppCtrl.restartFlightChessGame()
                    return
                }

                root.triggerRoll()
            }
        }

        ActionButton {
            Layout.fillWidth: true
            text: root.networkRoom ? "返回房间" : "返回首页"
            secondary: true
            onClicked: root.leaveCurrentGame()
        }
    }
}
