import QtQuick
import QtQuick.Layouts
import LanBoard

Rectangle {
    id: root

    property string titleText: ""
    property string valueText: ""
    property string actionText: ""
    property bool emphasized: false

    radius: AppTheme.radiusCard

    // Shadow layers
    Rectangle {
        x: 0
        y: 4
        width: parent.width - 2
        height: parent.height + 1
        radius: parent.radius
        color: AppTheme.shadowMedium
    }

    Rectangle {
        x: 0
        y: 2
        width: parent.width - 1
        height: parent.height
        radius: parent.radius
        color: AppTheme.shadowLight
    }

    color: root.emphasized ? "#EEF1EB" : AppTheme.cardBackground
    border.width: root.emphasized ? 0 : 1
    border.color: AppTheme.cardBorder

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AppTheme.spacingLg + 4
        anchors.rightMargin: AppTheme.spacingLg + 4
        spacing: AppTheme.spacingMd

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.titleText
                color: AppTheme.textPrimary
                font.pixelSize: 15
                font.weight: Font.Medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                text: root.valueText
                color: AppTheme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Text {
            id: actionLabel
            text: root.actionText
            color: AppTheme.accent
            font.pixelSize: 14
            font.weight: Font.Medium
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
