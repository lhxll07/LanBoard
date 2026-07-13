import QtQuick
import LanBoard

Rectangle {
    id: root

    property string titleText: ""
    property string subtitleText: ""
    property string tagText: ""
    property string gameType: "gomoku"
    signal clicked()

    radius: 32
    border.width: 0

    readonly property var theme: {
        if (gameType === "gomoku") return {
            gradientTop: "#F4E5C7", gradientBot: "#E1C287",
            textColor: "#1F302A", mutedColor: "#66593E",
            tagBg: "#F8F0DE", tagText: "#6A5B3C"
        };
        if (gameType === "doudizhu") return {
            gradientTop: "#2E1C1A", gradientBot: "#4A2824",
            textColor: "#F6F3EC", mutedColor: "#CDB7B2",
            tagBg: "#5A342E", tagText: "#E8D5D0"
        };
        if (gameType === "flightchess") return {
            gradientTop: "#1B262C", gradientBot: "#2C3D48",
            textColor: "#F6F3EC", mutedColor: "#B0C4D4",
            tagBg: "#324754", tagText: "#D0E0EC"
        };
        if (gameType === "dormdefense") return {
            gradientTop: "#3B2F2A", gradientBot: "#6F4E37",
            textColor: "#FFF8EE", mutedColor: "#E6D4C2",
            tagBg: "#8B6545", tagText: "#FFF0DE"
        };
        // survivor
        return {
            gradientTop: "#1C2A24", gradientBot: "#2E443A",
            textColor: "#F6F3EC", mutedColor: "#B2CCBE",
            tagBg: "#3A5548", tagText: "#D0E8DA"
        };
    }

    Rectangle {
        x: 0; y: 8
        width: parent.width; height: parent.height
        radius: parent.radius
        color: AppTheme.shadowLight
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: root.theme.gradientTop }
        GradientStop { position: 1.0; color: root.theme.gradientBot }
    }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        border.width: 1
        border.color: "#16FFFFFF"
        color: "transparent"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.52
        radius: parent.radius
        color: "#18FFFFFF"
    }

    Column {
        anchors.left: parent.left; anchors.top: parent.top
        anchors.leftMargin: 28; anchors.topMargin: 26
        spacing: 10

        Text {
            text: root.titleText
            color: root.theme.textColor
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }

        Text {
            width: parent.parent.width - 28 - 112
            text: root.subtitleText
            color: root.theme.mutedColor
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: AppTheme.fontSizeBody
        }
    }

    // Tag badge
    Rectangle {
        anchors.left: parent.left; anchors.leftMargin: 28
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        visible: root.tagText.length > 0
        color: root.theme.tagBg
        opacity: 0.9
        radius: 12
        width: tagLabel.implicitWidth + 18; height: 26

        Text {
            id: tagLabel
            anchors.centerIn: parent
            text: root.tagText
            color: root.theme.tagText
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }

    // ---- Gomoku icon ----
    Item {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 24; anchors.topMargin: 24
        width: 88; height: 88
        visible: root.gameType === "gomoku"

        Rectangle { anchors.fill: parent; radius: 22; color: "#EAD4A7" }

        Canvas {
            anchors.fill: parent; anchors.margins: 12
            onPaint: {
                const ctx = getContext("2d"), s = width / 5;
                ctx.reset(); ctx.strokeStyle = "#8A7248"; ctx.lineWidth = 1;
                for (let i = 0; i < 6; ++i) {
                    ctx.beginPath(); ctx.moveTo(i*s, 0); ctx.lineTo(i*s, height); ctx.stroke();
                    ctx.beginPath(); ctx.moveTo(0, i*s); ctx.lineTo(width, i*s); ctx.stroke();
                }
                ctx.fillStyle = "#17382F";
                ctx.beginPath(); ctx.arc(s*2, s*2, 5, 0, Math.PI*2); ctx.fill();
                ctx.beginPath(); ctx.arc(s*3, s*3, 5, 0, Math.PI*2); ctx.fill();
                ctx.fillStyle = "#F7F2E8"; ctx.strokeStyle = "#B2905D";
                ctx.beginPath(); ctx.arc(s*3.4, s*1.6, 5, 0, Math.PI*2); ctx.fill(); ctx.stroke();
                ctx.beginPath(); ctx.arc(s*1.6, s*3.4, 5, 0, Math.PI*2); ctx.fill(); ctx.stroke();
            }
        }
    }

    // ---- DouDiZhu icon ----
    Item {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 24; anchors.topMargin: 24
        width: 92; height: 76
        visible: root.gameType === "doudizhu"
        rotation: -8

        Rectangle { x: 0; y: 8; width: 34; height: 54; radius: 8; color: "#C73A34" }
        Rectangle { x: 22; y: 16; width: 34; height: 54; radius: 8; color: "#D4B830" }
        Rectangle { x: 44; y: 4; width: 34; height: 54; radius: 8; color: "#3B78D4" }

        Text {
            x: 10; y: 20; text: "\u2665"; color: "#5A1815"; font.pixelSize: 16
        }
        Text {
            x: 56; y: 22; text: "\u2666"; color: "#5A4E10"; font.pixelSize: 16
        }
        Text {
            x: 34; y: 7; text: "\u2660"; color: "#1A3A6A"; font.pixelSize: 16
        }
    }

    // ---- FlightChess icon ----
    Item {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 22; anchors.topMargin: 18
        width: 96; height: 88
        visible: root.gameType === "flightchess"

        Rectangle { x: 4; y: 12; width: 28; height: 28; radius: 8; color: "#E7584F" }
        Rectangle { x: 60; y: 12; width: 28; height: 28; radius: 8; color: "#5AA7E8" }
        Rectangle { x: 4; y: 48; width: 28; height: 28; radius: 8; color: "#F2C94C" }
        Rectangle { x: 60; y: 48; width: 28; height: 28; radius: 8; color: "#54B96E" }

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                // Path lines
                ctx.strokeStyle = "#F8F6EF"; ctx.lineWidth = 1.5;
                ctx.beginPath(); ctx.moveTo(18, 26); ctx.lineTo(48, 40); ctx.lineTo(78, 26); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(18, 62); ctx.lineTo(48, 40); ctx.lineTo(78, 62); ctx.stroke();
                // Position dots on path
                ctx.fillStyle = "#F8F6EF";
                for (let t = 0.15; t <= 0.85; t += 0.125) {
                    ctx.beginPath(); ctx.arc(48, 14 + t*52, 2.5, 0, Math.PI*2); ctx.fill();
                }
                // Center star
                ctx.fillStyle = "#173A31";
                ctx.beginPath(); ctx.arc(48, 40, 5, 0, Math.PI*2); ctx.fill();
                ctx.fillStyle = "#F8F6EF";
                ctx.font = "10px sans-serif"; ctx.textAlign = "center";
                ctx.fillText("\u2708", 48, 43);
            }
        }
    }

    // ---- Survivor icon ----
    Item {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 18; anchors.topMargin: 18
        width: 96; height: 88
        visible: root.gameType === "survivor"

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d"), cx = 48, cy = 42;
                ctx.reset();
                // Outer glow rings
                ctx.strokeStyle = "#E86A54"; ctx.lineWidth = 2; ctx.globalAlpha = 0.5;
                for (let i = 0; i < 4; ++i) {
                    ctx.beginPath(); ctx.arc(cx, cy, 14 + i*8, 0, Math.PI*2); ctx.stroke();
                }
                // Crosshair lines
                ctx.strokeStyle = "#F6E6C7"; ctx.lineWidth = 2.5; ctx.globalAlpha = 0.8;
                ctx.beginPath(); ctx.moveTo(cx-30, cy); ctx.lineTo(cx+30, cy); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(cx, cy-26); ctx.lineTo(cx, cy+26); ctx.stroke();
                // Center dot
                ctx.fillStyle = "#F6E6C7"; ctx.globalAlpha = 1;
                ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI*2); ctx.fill();
                ctx.fillStyle = "#E86A54";
                ctx.beginPath(); ctx.arc(cx, cy, 2, 0, Math.PI*2); ctx.fill();
            }
        }
    }

    // ---- DormDefense icon ----
    Item {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 16; anchors.topMargin: 16
        width: 98; height: 88
        visible: root.gameType === "dormdefense"

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();

                ctx.fillStyle = "#E8D7B5";
                ctx.fillRect(18, 16, 52, 52);

                ctx.fillStyle = "#8A5E3B";
                ctx.fillRect(8, 28, 14, 28);

                ctx.fillStyle = "#6C90B5";
                ctx.fillRect(34, 34, 20, 14);

                ctx.fillStyle = "#D0B13F";
                ctx.beginPath();
                ctx.arc(78, 28, 10, 0, Math.PI * 2);
                ctx.fill();

                ctx.strokeStyle = "#E36A5B";
                ctx.lineWidth = 4;
                ctx.beginPath();
                ctx.arc(76, 28, 18, Math.PI * 0.1, Math.PI * 1.65);
                ctx.stroke();

                ctx.fillStyle = "#E36A5B";
                ctx.font = "bold 18px sans-serif";
                ctx.textAlign = "center";
                ctx.fillText("鬼", 76, 34);
            }
        }
    }

    Behavior on scale {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
    scale: cardTap.pressed ? 0.992 : 1.0

    TapHandler {
        id: cardTap
        onTapped: root.clicked()
    }
}
