import QtQuick
import QtQuick.Layouts
import LanBoard

Rectangle {
    id: root

    property string titleText: ""
    property string valueText: ""
    property string actionText: ""
    property bool emphasized: false
    property bool clickable: false

    signal clicked()

    radius: AppTheme.radiusCard
    scale: cardMouse.pressed && root.clickable ? 0.992 : 1.0

    Behavior on scale {
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    Rectangle {
        x: 0
        y: 6
        width: parent.width
        height: parent.height
        radius: parent.radius
        color: AppTheme.shadowLight
    }

    color: root.emphasized ? AppTheme.surfaceTint : AppTheme.cardBackground
    border.width: 1
    border.color: root.emphasized ? AppTheme.cardBorderStrong : AppTheme.cardBorder

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.48
        radius: parent.radius
        color: AppTheme.surfaceHighlight
        opacity: root.emphasized ? 0.3 : 0.18
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AppTheme.spacingLg
        anchors.rightMargin: AppTheme.spacingLg
        spacing: AppTheme.spacingMd

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: root.titleText
                color: AppTheme.textPrimary
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                text: root.valueText
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fontSizeBody
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        Text {
            id: actionLabel
            text: root.actionText
            color: AppTheme.accent
            font.pixelSize: AppTheme.fontSizeBody
            font.weight: Font.Medium
            Layout.alignment: Qt.AlignVCenter
        }
    }

    MouseArea {
        id: cardMouse
        anchors.fill: parent
        enabled: root.clickable
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
