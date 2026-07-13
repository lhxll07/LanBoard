import QtQuick
import QtQuick.Controls
import QtQuick.Window

Page {
    id: root
    objectName: "work3Page"
    property int previousWindowWidth: 0
    property int previousWindowHeight: 0
    property bool portraitRestored: true

    function leaveCurrentGame() {
        restorePortraitOrientation()
        if (StackView.view && StackView.view.depth > 1)
            StackView.view.pop()
    }

    function restorePortraitOrientation() {
        if (portraitRestored)
            return

        portraitRestored = true
        if (typeof AppCtrl !== "undefined")
            AppCtrl.lockPortraitOrientation()

        if (Qt.platform.os === "android")
            return

        var appWindow = Window.window
        if (!appWindow || previousWindowWidth <= 0 || previousWindowHeight <= 0)
            return

        appWindow.width = previousWindowWidth
        appWindow.height = previousWindowHeight
        previousWindowWidth = 0
        previousWindowHeight = 0
    }

    Component.onCompleted: {
        portraitRestored = false
        if (typeof AppCtrl !== "undefined") {
            AppCtrl.lockLandscapeOrientation()
        }

        if (Qt.platform.os === "android")
            return

        var appWindow = Window.window
        if (!appWindow)
            return

        previousWindowWidth = appWindow.width
        previousWindowHeight = appWindow.height
        appWindow.width = 960
        appWindow.height = 540
    }

    Component.onDestruction: {
        restorePortraitOrientation()
    }

    onVisibleChanged: {
        if (!visible)
            restorePortraitOrientation()
    }

    background: Rectangle {
        color: "#141115"
    }

    Item {
        id: stage
        width: 960
        height: 540
        anchors.centerIn: parent
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.Center

        Work3Game {
            anchors.fill: parent
            onExitRequested: root.leaveCurrentGame()
        }
    }
}
