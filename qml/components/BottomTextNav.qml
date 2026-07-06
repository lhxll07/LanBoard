import QtQuick
import QtQuick.Effects
import LanBoard

Item {
    id: root

    property int currentIndex: 0
    property Item backdropSource: null
    signal tabSelected(int index)

    width: implicitWidth
    height: implicitHeight
    implicitWidth: AppTheme.contentWidth
    implicitHeight: 82
    readonly property real backdropInsetX: glassShell.x
    readonly property real backdropInsetY: glassShell.y
    readonly property real backdropWidth: glassShell.width
    readonly property real backdropHeight: glassShell.height

    Rectangle {
        anchors.horizontalCenter: glassShell.horizontalCenter
        anchors.verticalCenter: glassShell.verticalCenter
        anchors.verticalCenterOffset: 11
        width: glassShell.width - 28
        height: 26
        radius: 13
        color: "#140A1612"
        smooth: true
    }

    Rectangle {
        id: glassShell
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -2
        width: AppTheme.contentWidth - 14
        height: AppTheme.navHeight
        radius: height / 2
        color: "transparent"
        antialiasing: true
        layer.enabled: true
        layer.samples: 4

        Rectangle {
            id: shellMask
            anchors.fill: parent
            radius: glassShell.radius
            color: "white"
            antialiasing: true
            visible: true
        }

        ShaderEffectSource {
            id: shellMaskSource
            anchors.fill: shellMask
            visible: false
            live: true
            hideSource: true
            sourceItem: shellMask
            textureSize: Qt.size(Math.max(256, width * 2), Math.max(128, height * 2))
        }

        ShaderEffectSource {
            id: blurSource
            anchors.fill: parent
            visible: false
            live: true
            recursive: true
            hideSource: false
            sourceItem: root.backdropSource
            textureSize: Qt.size(Math.max(256, width * 2), Math.max(128, height * 2))
        }

        MultiEffect {
            id: blurLayer
            anchors.fill: parent
            source: blurSource
            autoPaddingEnabled: false
            blurEnabled: true
            blurMax: 26
            blur: 0.52
            saturation: 0.14
            brightness: 0.02
            colorization: 0.04
            colorizationColor: "#F4F7F2"
            maskEnabled: true
            maskSource: shellMaskSource
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "#88FBFCF8"
            antialiasing: true
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            antialiasing: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#86FFFFFF" }
                GradientStop { position: 0.28; color: "#30FFFFFF" }
                GradientStop { position: 1.0; color: "#12D8E4DC" }
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            width: parent.width - 54
            height: parent.height * 0.2
            radius: parent.radius
            color: "#14A9D4CB"
            antialiasing: true
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            antialiasing: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#40FFFFFF" }
                GradientStop { position: 0.22; color: "#14FFFFFF" }
                GradientStop { position: 1.0; color: "#00FFFFFF" }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: "#68FFFFFF"
            antialiasing: true
        }
    }

    Rectangle {
        id: activeCapsule
        x: glassShell.x + 8 + root.currentIndex * (glassShell.width / 3)
        y: glassShell.y + 7
        width: glassShell.width / 3 - 16
        height: glassShell.height - 14
        radius: height / 2
        color: "#72FFFFFF"
        antialiasing: true
        layer.enabled: true
        layer.samples: 4

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#A7FFFFFF" }
            GradientStop { position: 0.56; color: "#56F4FCFA" }
            GradientStop { position: 1.0; color: "#22DCEBE4" }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 2
            width: parent.width - 12
            height: parent.height * 0.38
            radius: parent.radius
            color: "#30FFFFFF"
            antialiasing: true
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: "#4EFFFFFF"
            antialiasing: true
        }

        Behavior on x {
            NumberAnimation {
                duration: 360
                easing.type: Easing.OutQuint
            }
        }
    }

    Row {
        id: navRow
        anchors.fill: glassShell
        spacing: 0
        z: 2

        Repeater {
            model: [
                { "title": AppTheme.zhHome() },
                { "title": AppTheme.zhMatch() },
                { "title": AppTheme.zhSetting() }
            ]

            delegate: Item {
                width: parent.width / 3
                height: parent.height

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData.title
                    color: index === root.currentIndex ? AppTheme.accent : AppTheme.textMuted
                    font.pixelSize: index === root.currentIndex ? 15 : 14
                    font.weight: index === root.currentIndex ? Font.DemiBold : Font.Medium
                    anchors.verticalCenterOffset: -1

                    Behavior on color { ColorAnimation { duration: 180 } }
                    Behavior on font.pixelSize { NumberAnimation { duration: 180 } }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: label.bottom
                    anchors.topMargin: 6
                    width: index === root.currentIndex ? 20 : 0
                    height: 2
                    radius: 1
                    color: index === root.currentIndex ? "#163A31" : "transparent"

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
