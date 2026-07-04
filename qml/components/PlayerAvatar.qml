import QtQuick
import QtQuick.Layouts
import LanBoard

Rectangle {
    id: root

    property string playerName: ""
    property string roleText: ""
    property string statusText: ""
    property string countText: ""
    property bool active: false
    property bool winner: false
    property int tone: 0

    readonly property var palette: [
        "#173A31",
        "#7A3F35",
        "#3D5C8A",
        "#7B5C22",
        "#5A476F"
    ]
    readonly property color statusDotColor: winner ? "#D7B65D" : (active ? "#63B67A" : "#B9C1BA")

    radius: 16
    color: active ? "#173A31" : "#FFFFFF"
    border.width: winner ? 2 : (active ? 0 : 1)
    border.color: winner ? "#D7B65D" : AppTheme.cardBorder

    implicitHeight: 76
    implicitWidth: 104

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Rectangle {
            id: portrait
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            radius: 21
            color: root.winner ? "#D7B65D" : root.palette[Math.abs(root.tone) % root.palette.length]
            border.width: root.active ? 2 : 0
            border.color: "#F8F8F5"

            Text {
                anchors.centerIn: parent
                text: {
                    var source = root.playerName.length > 0 ? root.playerName : root.roleText
                    return source.length > 0 ? source[0].toUpperCase() : "?"
                }
                color: "#F8F8F5"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Rectangle {
                width: 12
                height: 12
                radius: 6
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: root.statusDotColor
                border.width: 2
                border.color: root.active ? "#173A31" : "#FFFFFF"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.playerName.length > 0 ? root.playerName : root.roleText
                color: root.active ? "#F8F8F5" : AppTheme.textPrimary
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                text: root.countText.length > 0
                    ? root.statusText + " · " + root.countText
                    : root.statusText
                color: root.active ? "#DCE4DF" : AppTheme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }
}
