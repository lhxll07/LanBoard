import QtQuick
import LanBoard

Rectangle {
    id: root

    property string eyebrowText: ""
    property string titleText: ""
    property string subtitleText: ""
    property string tagText: ""
    property string gameType: root.dark ? "doudizhu" : "gomoku"
    property bool dark: false
    signal clicked()

    radius: 32
    border.width: 0

    // Shadow layers (rendered behind gradient)
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

    gradient: Gradient {
        GradientStop { position: 0.0; color: root.dark ? AppTheme.darkCard : AppTheme.warmBoard }
        GradientStop { position: 1.0; color: root.dark ? AppTheme.darkCardAccent : AppTheme.warmBoardDeep }
    }

    Column {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.topMargin: 26
        spacing: 10

        Text {
            text: root.eyebrowText
            color: root.dark ? "#CDD7D1" : "#836B43"
            font.pixelSize: 12
            font.letterSpacing: 1.4
        }

        Text {
            text: root.titleText
            color: root.dark ? "#F6F3EC" : AppTheme.textPrimary
            font.pixelSize: 24
            font.weight: Font.DemiBold
        }

        Text {
            width: root.dark
                ? parent.parent.width - 28 - 116
                : parent.parent.width - 28 - 112
            text: root.subtitleText
            color: root.dark ? "#D8E0DB" : "#66593E"
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            font.pixelSize: 14
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        visible: root.tagText.length > 0
        color: root.dark ? "#314C42" : "#F8F0DE"
        opacity: 0.82
        radius: 12
        width: tagLabel.implicitWidth + 18
        height: 24

        Text {
            id: tagLabel
            anchors.centerIn: parent
            text: root.tagText
            color: root.dark ? "#DCE4DF" : "#6A5B3C"
            font.pixelSize: 12
        }
    }

    Item {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 24
        anchors.topMargin: 24
        width: 88
        height: 88
        visible: root.gameType === "gomoku"

        Rectangle {
            anchors.fill: parent
            radius: 22
            color: "#EAD4A7"
        }

        Canvas {
            anchors.fill: parent
            anchors.margins: 14

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = "#8A7248";
                ctx.lineWidth = 1;

                const step = width / 5;
                for (let i = 0; i < 6; ++i) {
                    const p = i * step;
                    ctx.beginPath();
                    ctx.moveTo(p, 0);
                    ctx.lineTo(p, height);
                    ctx.stroke();

                    ctx.beginPath();
                    ctx.moveTo(0, p);
                    ctx.lineTo(width, p);
                    ctx.stroke();
                }

                ctx.fillStyle = "#17382F";
                ctx.beginPath();
                ctx.arc(step * 2.5, step * 2.5, 6, 0, Math.PI * 2);
                ctx.fill();

                ctx.fillStyle = "#F7F2E8";
                ctx.strokeStyle = "#B2905D";
                ctx.beginPath();
                ctx.arc(step * 3.4, step * 3.4, 6, 0, Math.PI * 2);
                ctx.fill();
                ctx.stroke();
            }
        }
    }

    Item {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 20
        anchors.topMargin: 18
        width: 104
        height: 84
        visible: root.gameType === "survivor"

        Rectangle {
            anchors.fill: parent
            radius: 24
            color: "#D99D64"
            opacity: 0.18
        }

        Canvas {
            anchors.fill: parent

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();

                ctx.fillStyle = "#F6E6C7";
                ctx.beginPath();
                ctx.arc(52, 42, 12, 0, Math.PI * 2);
                ctx.fill();

                ctx.strokeStyle = "#F6E6C7";
                ctx.lineWidth = 4;
                ctx.beginPath();
                ctx.moveTo(52, 22);
                ctx.lineTo(52, 66);
                ctx.stroke();

                ctx.beginPath();
                ctx.moveTo(30, 42);
                ctx.lineTo(74, 42);
                ctx.stroke();

                ctx.strokeStyle = "#E86A54";
                ctx.lineWidth = 3;
                for (let i = 0; i < 5; ++i) {
                    const angle = i * Math.PI * 0.4;
                    const ex = 52 + Math.cos(angle) * 34;
                    const ey = 42 + Math.sin(angle) * 26;
                    ctx.beginPath();
                    ctx.arc(ex, ey, 7, 0, Math.PI * 2);
                    ctx.stroke();
                }
            }
        }
    }

    Item {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 24
        anchors.topMargin: 24
        width: 92
        height: 72
        visible: root.gameType === "doudizhu"
        rotation: -10

        Rectangle { x: 0; y: 6; width: 32; height: 52; radius: 10; color: "#E74B45" }
        Rectangle { x: 20; y: 14; width: 32; height: 52; radius: 10; color: "#F1D34B" }
        Rectangle { x: 44; y: 2; width: 32; height: 52; radius: 10; color: "#4B86E8" }
    }

    Item {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 22
        anchors.topMargin: 18
        width: 96
        height: 84
        visible: root.gameType === "flightchess"

        Rectangle { x: 4; y: 10; width: 30; height: 30; radius: 9; color: "#E7584F" }
        Rectangle { x: 58; y: 10; width: 30; height: 30; radius: 9; color: "#5AA7E8" }
        Rectangle { x: 4; y: 44; width: 30; height: 30; radius: 9; color: "#F2C94C" }
        Rectangle { x: 58; y: 44; width: 30; height: 30; radius: 9; color: "#54B96E" }

        Canvas {
            anchors.fill: parent

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();

                ctx.strokeStyle = "#F8F6EF";
                ctx.lineWidth = 2;

                ctx.beginPath();
                ctx.moveTo(18, 25);
                ctx.lineTo(48, 38);
                ctx.lineTo(76, 25);
                ctx.stroke();

                ctx.beginPath();
                ctx.moveTo(18, 59);
                ctx.lineTo(48, 38);
                ctx.lineTo(76, 59);
                ctx.stroke();

                ctx.fillStyle = "#F8F6EF";
                ctx.beginPath();
                ctx.moveTo(48, 18);
                ctx.lineTo(57, 44);
                ctx.lineTo(48, 39);
                ctx.lineTo(39, 44);
                ctx.closePath();
                ctx.fill();

                ctx.fillStyle = "#173A31";
                ctx.beginPath();
                ctx.arc(48, 38, 5, 0, Math.PI * 2);
                ctx.fill();
            }
        }
    }

    Behavior on scale {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    scale: cardTap.pressed ? 0.988 : 1.0

    TapHandler {
        id: cardTap
        onTapped: root.clicked()
    }
}
