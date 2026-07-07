import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "gamePage"

    function localRoomPlayer() {
        var players = AppCtrl.roomManager.playerList
        var idx = AppCtrl.roomManager.localPlayerIndex
        return idx >= 0 && idx < players.length ? players[idx] : null
    }

    function localIsSpectator() {
        var player = localRoomPlayer()
        return !!player && (player.seatType || "active") === "spectator"
    }

    function gomokuPlayerName(piece) {
        var players = AppCtrl.roomManager.playerList
        for (var i = 0; i < players.length; ++i) {
            var player = players[i]
            if (piece === 1 && player.isHost)
                return player.name
            if (piece === 2 && !player.isHost && (player.seatType || "active") === "active")
                return player.name
        }
        return piece === 1 ? "黑方" : "白方"
    }

    function gomokuStatus(piece) {
        if (AppCtrl.gameController.gameOver)
            return AppCtrl.gameController.winner === piece ? "获胜" : "结束"
        return AppCtrl.gameController.currentPlayer === piece ? "落子中" : "等待"
    }

    function surrenderAndLeave() {
        if (AppCtrl.gameController.gameOver)
            return;

        var inNet = AppCtrl.networkManager.isHost
                 || AppCtrl.networkManager.isConnected;
        if (inNet && AppCtrl.networkManager.isHost) {
            AppCtrl.gameController.surrender(1);
        } else if (inNet) {
            AppCtrl.networkManager.sendSurrender();
        } else {
            AppCtrl.gameController.surrender();
        }
    }

    background: Rectangle {
        color: "transparent"
    }

    Connections {
        target: AppCtrl.gameController
        function onBoardChanged() { boardCanvas.requestPaint() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: AppTheme.spacingLg
        anchors.rightMargin: AppTheme.spacingLg
        anchors.topMargin: 20
        anchors.bottomMargin: 10
        spacing: 12

        // -- 顶部信息区 --
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 124

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: "五子棋"
                    color: AppTheme.textPrimary
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 76
                    spacing: 10

                    PlayerAvatar {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        playerName: root.gomokuPlayerName(1)
                        roleText: "黑方"
                        statusText: root.gomokuStatus(1)
                        active: !AppCtrl.gameController.gameOver
                                && AppCtrl.gameController.currentPlayer === 1
                        winner: AppCtrl.gameController.gameOver
                                && AppCtrl.gameController.winner === 1
                        tone: 0
                    }

                    PlayerAvatar {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        playerName: root.gomokuPlayerName(2)
                        roleText: "白方"
                        statusText: root.gomokuStatus(2)
                        active: !AppCtrl.gameController.gameOver
                                && AppCtrl.gameController.currentPlayer === 2
                        winner: AppCtrl.gameController.gameOver
                                && AppCtrl.gameController.winner === 2
                        tone: 2
                    }
                }
            }
        }

        // -- 当前回合提示 --
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AppCtrl.gameController.gameOver
                ? (AppCtrl.gameController.winner === 1 ? "黑子胜！" : "白子胜！")
                : (function() {
                    var inNet = AppCtrl.networkManager.isHost
                             || AppCtrl.networkManager.isConnected;
                    if (!inNet)
                        return AppCtrl.gameController.currentPlayer === 1
                            ? "黑方落子" : "白方落子";
                    if (root.localIsSpectator())
                        return AppCtrl.gameController.currentPlayer === 1
                            ? "黑方落子中" : "白方落子中";
                    if (AppCtrl.roomManager.isHost)
                        return AppCtrl.gameController.currentPlayer === 1
                            ? "轮到你了" : "等待对方落子";
                    else
                        return AppCtrl.gameController.currentPlayer === 2
                            ? "轮到你了" : "等待对方落子";
                }())
            color: AppTheme.textPrimary
            font.pixelSize: 24
            font.weight: Font.DemiBold
        }

        // -- 棋盘区 (弹性填充) --
        Rectangle {
            id: boardContainer
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 220
            Layout.maximumWidth: 360
            Layout.alignment: Qt.AlignHCenter
            radius: 30
            color: AppTheme.warmBoard

            Canvas {
                id: boardCanvas
                anchors.fill: parent
                anchors.margins: 16

                property real cellW: width / 14
                property real cellH: height / 14

                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = AppTheme.boardGridLine;
                    ctx.lineWidth = 1.2;

                    // Draw grid lines
                    for (let i = 0; i < 14; ++i) {
                        const x = i * cellW + cellW / 2;
                        const y = i * cellH + cellH / 2;

                        ctx.beginPath();
                        ctx.moveTo(cellW / 2, y);
                        ctx.lineTo(width - cellW / 2, y);
                        ctx.stroke();

                        ctx.beginPath();
                        ctx.moveTo(x, cellH / 2);
                        ctx.lineTo(x, height - cellH / 2);
                        ctx.stroke();
                    }

                    // Draw pieces from game board
                    const boardData = AppCtrl.gameController.board;
                    for (let r = 0; r < boardData.length; ++r) {
                        const row = boardData[r];
                        for (let c = 0; c < row.length; ++c) {
                            const val = row[c];
                            if (val === 0) continue;

                            const px = c * cellW + cellW / 2;
                            const py = r * cellH + cellH / 2;

                            ctx.beginPath();
                            ctx.arc(px, py, 7, 0, Math.PI * 2);
                            if (val === 1) {
                                ctx.fillStyle = AppTheme.boardBlackPiece;
                                ctx.fill();
                            } else {
                                ctx.fillStyle = AppTheme.boardWhitePiece;
                                ctx.fill();
                                ctx.strokeStyle = AppTheme.boardWhiteStroke;
                                ctx.lineWidth = 1;
                                ctx.stroke();
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !AppCtrl.gameController.gameOver
                    onClicked: {
                        // Check turn: only your own turn in network mode
                        var inNet = AppCtrl.networkManager.isHost
                                 || AppCtrl.networkManager.isConnected;
                        if (inNet) {
                            if (root.localIsSpectator())
                                return;
                            var myTurn = AppCtrl.roomManager.isHost
                                ? AppCtrl.gameController.currentPlayer === 1
                                : AppCtrl.gameController.currentPlayer === 2;
                            if (!myTurn) return;
                        }

                        const col = Math.floor(mouse.x / boardCanvas.cellW);
                        const row = Math.floor(mouse.y / boardCanvas.cellH);
                        if (col >= 0 && col < 14 && row >= 0 && row < 14) {
                            if (inNet && AppCtrl.networkManager.isHost) {
                                if (AppCtrl.gameController.placePiece(row, col, 1)) {
                                    AppCtrl.networkManager.broadcastMove(1, row, col);
                                }
                            } else if (inNet) {
                                AppCtrl.networkManager.sendPlacePiece(row, col);
                            } else {
                                AppCtrl.gameController.placePiece(row, col);
                            }
                        }
                    }
                }
            }
        }


        // -- 底部按钮 --
        ActionButton {
            Layout.fillWidth: true
            text: root.localIsSpectator() ? "返回房间" : "认输并返回房间"
            secondary: true
            enabled: root.localIsSpectator() || !AppCtrl.gameController.gameOver
            onClicked: {
                if (root.localIsSpectator()) {
                    root.StackView.view.pop()
                    return
                }
                root.surrenderAndLeave()
            }
        }
    }
}
