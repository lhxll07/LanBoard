import QtQuick
import LanBoard

Item {
    id: root

    property int currentIndex: 0
    signal tabSelected(int index)

    implicitHeight: 56

    // Shadow layers for floating effect
    Rectangle {
        anchors.horizontalCenter: shell.horizontalCenter
        anchors.top: shell.bottom
        anchors.topMargin: -2
        width: AppTheme.contentWidth - 4
        height: 8
        radius: 4
        color: AppTheme.shadowDeep
    }

    Rectangle {
        anchors.horizontalCenter: shell.horizontalCenter
        anchors.top: shell.bottom
        anchors.topMargin: 0
        width: AppTheme.contentWidth - 2
        height: 6
        radius: 3
        color: AppTheme.shadowMedium
    }

    Rectangle {
        anchors.horizontalCenter: shell.horizontalCenter
        anchors.top: shell.bottom
        anchors.topMargin: 4
        width: AppTheme.contentWidth
        height: 4
        radius: 2
        color: AppTheme.shadowLight
    }

    Rectangle {
        id: shell
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: AppTheme.contentWidth
        height: 52
        radius: 26
        color: "#FCFCF9"
    }

    Row {
        anchors.fill: shell
        spacing: 0

        Repeater {
            model: [
                { "title": AppTheme.zhHome() },
                { "title": AppTheme.zhMatch() },
                { "title": AppTheme.zhSetting() }
            ]

            delegate: Item {
                width: shell.width / 3
                height: shell.height

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData.title
                    color: index === root.currentIndex ? AppTheme.accent : AppTheme.textMuted
                    font.pixelSize: index === root.currentIndex ? 15 : 14
                    font.weight: index === root.currentIndex ? Font.DemiBold : Font.Medium

                    Behavior on color {
                        ColorAnimation { duration: 180 }
                    }

                    Behavior on font.pixelSize {
                        NumberAnimation { duration: 180 }
                    }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: label.bottom
                    anchors.topMargin: 8
                    width: index === root.currentIndex ? 24 : 0
                    height: 3
                    radius: 1.5
                    color: AppTheme.accent

                    Behavior on width {
                        NumberAnimation {
                            duration: 300
                            easing.type: Easing.OutBack
                            easing.overshoot: 1.2
                        }
                    }
                }

                TapHandler {
                    onTapped: root.tabSelected(index)
                }
            }
        }
    }
}
