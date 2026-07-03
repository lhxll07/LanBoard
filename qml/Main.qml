import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    width: 960
    height: 640
    visible: true
    title: "LanBoard"

    StackView {
        anchors.fill: parent
        initialItem: Qt.resolvedUrl("pages/HomePage.qml")
    }
}
