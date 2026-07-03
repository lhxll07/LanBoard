import QtQuick
import LanBoard

Rectangle {
    id: root

    property string playerName: ""
    property string roleText: ""
    property string statusText: ""
    property bool ready: false

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

    color: AppTheme.cardBackground
    border.width: 1
    border.color: AppTheme.cardBorder

    Rectangle {
        id: avatar
        anchors.left: parent.left
        anchors.leftMargin: AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        radius: 16
        color: root.ready ? "#DDE7DF" : "#ECEEE8"
    }

    Column {
        anchors.left: avatar.right
        anchors.leftMargin: AppTheme.spacingMd
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            text: root.playerName
            color: AppTheme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Text {
            text: root.roleText
            color: AppTheme.textSecondary
            font.pixelSize: 13
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        visible: root.ready
        radius: 14
        height: 28
        width: readyLabel.implicitWidth + 18
        color: AppTheme.accentSoft

        Text {
            id: readyLabel
            anchors.centerIn: parent
            text: root.statusText
            color: "#2F5C4D"
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.ready
        text: root.statusText
        color: AppTheme.textMuted
        font.pixelSize: 12
    }
}
