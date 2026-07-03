import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root

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
            Layout.preferredHeight: 56

            Column {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                spacing: 6

                Text {
                    text: "五子棋"
                    color: AppTheme.textPrimary
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
            }
        }

        // -- 当前回合提示 --
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AppCtrl.gameController.gameOver
                ? (AppCtrl.gameController.winner === 1 ? "黑子胜！" : "白子胜！")
                : (AppCtrl.gameController.currentPlayer === 1 ? "轮到你了" : "等待对方落子")
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
            color: "#EAD4A7"

            Canvas {
                id: boardCanvas
                anchors.fill: parent
                anchors.margins: 16

                property real cellW: width / 14
                property real cellH: height / 14

                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = "#8A7248";
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
                                ctx.fillStyle = "#17382F";
                                ctx.fill();
                            } else {
                                ctx.fillStyle = "#F7F2E8";
                                ctx.fill();
                                ctx.strokeStyle = "#B2905D";
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
                        const col = Math.floor(mouse.x / boardCanvas.cellW);
                        const row = Math.floor(mouse.y / boardCanvas.cellH);
                        if (col >= 0 && col < 14 && row >= 0 && row < 14) {
                            var inNet = AppCtrl.networkManager.isHost
                                     || AppCtrl.networkManager.isConnected;
                            if (inNet && !AppCtrl.networkManager.isHost) {
                                // Client: send move via network
                                AppCtrl.networkManager.sendPlacePiece(row, col);
                            } else {
                                // Host or local: place on local GameController
                                AppCtrl.gameController.placePiece(row, col);
                            }
                        }
                    }
                }
            }
        }

        // -- 当前玩家 --
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AppCtrl.gameController.currentPlayer === 1 ? "黑子 · 你" : "白子 · 对手"
            color: AppTheme.textSecondary
            font.pixelSize: 14
        }

        // -- 底部按钮 --
        RowLayout {
            Layout.fillWidth: true
            spacing: AppTheme.spacingMd

            ActionButton {
                Layout.fillWidth: true
                text: AppCtrl.gameController.gameOver ? "再来一局" : "退出"
                secondary: AppCtrl.gameController.gameOver
                onClicked: {
                    if (AppCtrl.gameController.gameOver) {
                        var inNet = AppCtrl.networkManager.isHost
                                 || AppCtrl.networkManager.isConnected;
                        if (inNet && AppCtrl.networkManager.isHost) {
                            AppCtrl.gameController.startNewGame();
                            AppCtrl.networkManager.broadcastGameStarted(1);
                        } else if (inNet) {
                            AppCtrl.networkManager.sendNewGame();
                        } else {
                            AppCtrl.gameController.startNewGame();
                        }
                    } else {
                        StackView.view.pop();
                    }
                }
            }

            ActionButton {
                Layout.fillWidth: true
                text: "认输"
                enabled: !AppCtrl.gameController.gameOver
                onClicked: {
                    var inNet = AppCtrl.networkManager.isHost
                             || AppCtrl.networkManager.isConnected;
                    if (inNet && AppCtrl.networkManager.isHost) {
                        AppCtrl.gameController.surrender();
                        AppCtrl.networkManager.broadcastGameOver(
                            AppCtrl.gameController.winner);
                    } else if (inNet) {
                        AppCtrl.networkManager.sendSurrender();
                    } else {
                        AppCtrl.gameController.surrender();
                    }
                }
            }
        }
    }
}
