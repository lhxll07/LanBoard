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
        spacing: 5

        Text {
            visible: root.eyebrowText.length > 0
            text: root.eyebrowText
            color: AppTheme.textMuted
            font.pixelSize: 12
            font.letterSpacing: 1.4
            font.weight: Font.Medium
        }

        RowLayout {
            width: parent.width
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: root.titleText
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fontSizeHero
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                visible: root.trailingText.length > 0
                text: root.trailingText
                color: AppTheme.textMuted
                font.pixelSize: 12
                font.weight: Font.Medium
                Layout.alignment: Qt.AlignBottom
            }
        }

        Text {
            visible: root.subtitleText.length > 0
            width: parent.width
            text: root.subtitleText
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fontSizeBody
            wrapMode: Text.WordWrap
        }
    }
}
