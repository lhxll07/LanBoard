import QtQuick
import LanBoard

Rectangle {
    id: root

    property string playerName: ""
    property string roleText: ""
    property string statusText: ""
    property bool ready: false
    property string actionText: ""
    property bool actionEnabled: false

    signal actionTriggered()

    radius: AppTheme.radiusCard

    Rectangle {
        x: 0
        y: 6
        width: parent.width
        height: parent.height
        radius: parent.radius
        color: AppTheme.shadowLight
    }

    color: AppTheme.cardBackground
    border.width: 1
    border.color: AppTheme.cardBorder

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.46
        radius: parent.radius
        color: AppTheme.surfaceHighlight
        opacity: 0.16
    }

    Rectangle {
        id: avatar
        anchors.left: parent.left
        anchors.leftMargin: AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        radius: 16
        color: root.ready ? "#DDE7DF" : AppTheme.cardBackgroundSoft
        border.width: 1
        border.color: root.ready ? "#D2DFD6" : AppTheme.cardBorder
    }

    Column {
        anchors.left: avatar.right
        anchors.leftMargin: AppTheme.spacingMd
        anchors.right: actionButton.visible ? actionButton.left : parent.right
        anchors.rightMargin: actionButton.visible ? AppTheme.spacingMd : AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            width: parent.width
            text: root.playerName
            color: AppTheme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: root.roleText
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fontSizeBody
            elide: Text.ElideRight
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
            font.weight: Font.DemiBold
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.ready && !actionButton.visible
        text: root.statusText
        color: AppTheme.textMuted
        font.pixelSize: 12
        font.weight: Font.Medium
    }

    ActionButton {
        id: actionButton
        visible: root.actionText.length > 0
        anchors.right: parent.right
        anchors.rightMargin: AppTheme.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        width: 84
        height: 34
        text: root.actionText
        secondary: true
        enabled: root.actionEnabled
        onClicked: root.actionTriggered()
    }
}
