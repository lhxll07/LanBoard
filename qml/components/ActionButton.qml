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
        color: control.secondary ? AppTheme.panelBackground : AppTheme.accent
        border.width: control.secondary ? 1 : 0
        border.color: AppTheme.cardBorder

        Behavior on color {
            ColorAnimation { duration: 180 }
        }

        scale: control.down ? 0.985 : 1.0
        opacity: control.enabled ? 1.0 : 0.55

        Behavior on scale {
            NumberAnimation { duration: 140 }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.secondary ? AppTheme.accent : "#F8F8F5"
        font.pixelSize: 15
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
