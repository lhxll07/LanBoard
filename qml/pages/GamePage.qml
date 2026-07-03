import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root

    background: Rectangle {
        color: "transparent"
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

        // -- "轮到你了" 提示 --
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "轮到你了"
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

                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = "#8A7248";
                    ctx.lineWidth = 1.2;

                    const cells = 14;
                    const stepX = width / cells;
                    const stepY = height / cells;

                    for (let i = 0; i <= cells; ++i) {
                        const x = i * stepX;
                        const y = i * stepY;
                        ctx.beginPath();
                        ctx.moveTo(x, 0);
                        ctx.lineTo(x, height);
                        ctx.stroke();

                        ctx.beginPath();
                        ctx.moveTo(0, y);
                        ctx.lineTo(width, y);
                        ctx.stroke();
                    }

                    const pieces = [
                        { x: 6, y: 6, fill: "#17382F", stroke: "" },
                        { x: 7, y: 7, fill: "#F7F2E8", stroke: "#B2905D" },
                        { x: 8, y: 8, fill: "#17382F", stroke: "" },
                        { x: 9, y: 9, fill: "#F7F2E8", stroke: "#B2905D" }
                    ];

                    for (const piece of pieces) {
                        const px = piece.x * stepX;
                        const py = piece.y * stepY;
                        ctx.beginPath();
                        ctx.arc(px, py, 8, 0, Math.PI * 2);
                        ctx.fillStyle = piece.fill;
                        ctx.fill();
                        if (piece.stroke) {
                            ctx.strokeStyle = piece.stroke;
                            ctx.stroke();
                        }
                    }
                }
            }
        }

        // -- 当前玩家 --
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "黑子 · 你"
            color: AppTheme.textSecondary
            font.pixelSize: 14
        }

        // -- 底部按钮 --
        RowLayout {
            Layout.fillWidth: true
            spacing: AppTheme.spacingMd

            ActionButton {
                Layout.fillWidth: true
                text: "退出"
                secondary: true
                onClicked: StackView.view.pop()
            }

            ActionButton {
                Layout.fillWidth: true
                text: "认输"
            }
        }
    }
}
