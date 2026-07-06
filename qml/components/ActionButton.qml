import QtQuick
import QtQuick.Controls
import LanBoard

Button {
    id: control

    property bool secondary: false

    implicitWidth: 150
    implicitHeight: 54

    background: Rectangle {
        radius: AppTheme.radiusButton
        color: control.secondary ? AppTheme.surfaceRaised : AppTheme.accent
        border.width: 1
        border.color: control.secondary ? AppTheme.cardBorder : "#203E36"

        Behavior on color {
            ColorAnimation { duration: 180 }
        }

        scale: control.down ? 0.988 : 1.0
        opacity: control.enabled ? 1.0 : 0.55

        Behavior on scale {
            NumberAnimation { duration: 140 }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height * 0.52
            radius: parent.radius
            color: control.secondary ? "#32FFFFFF" : "#16FFFFFF"
        }
    }

    contentItem: Text {
        text: control.text
        color: control.secondary ? AppTheme.accent : "#F8F8F5"
        font.pixelSize: AppTheme.fontSizeButton
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
