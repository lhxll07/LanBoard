import QtQuick
import QtQuick.Layouts
import LanBoard

Item {
    id: root

    property string eyebrowText: ""
    property string titleText: ""
    property string subtitleText: ""
    property string trailingText: ""

    width: AppTheme.contentWidth
    implicitHeight: contentColumn.height

    Column {
        id: contentColumn
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.right: parent.right
        spacing: 6

        Text {
            visible: root.eyebrowText.length > 0
            text: root.eyebrowText
            color: AppTheme.textMuted
            font.pixelSize: 12
            font.letterSpacing: 1.6
        }

        RowLayout {
            width: parent.width
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: root.titleText
                color: AppTheme.textPrimary
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }

            Text {
                visible: root.trailingText.length > 0
                text: root.trailingText
                color: AppTheme.textMuted
                font.pixelSize: 12
                Layout.alignment: Qt.AlignBottom
            }
        }

        Text {
            visible: root.subtitleText.length > 0
            width: parent.width
            text: root.subtitleText
            color: AppTheme.textSecondary
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }
    }
}
