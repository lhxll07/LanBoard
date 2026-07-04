import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root

    property string nicknameDraft: AppCtrl.nickname
    property string portDraft: AppCtrl.defaultPort.toString()

    function openNicknameDialog() {
        nicknameDraft = AppCtrl.nickname
        nicknameDialog.open()
    }

    function openPortDialog() {
        portDraft = AppCtrl.defaultPort.toString()
        portDialog.open()
    }

    background: Rectangle {
        color: "transparent"
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + AppTheme.pageBottomInset
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            width: AppTheme.contentWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: AppTheme.pageTopInset
            spacing: 18

            PageHeader {
                titleText: AppTheme.zhSetting()
            }

            SettingCard {
                id: nicknameCard
                width: parent.width
                height: 84
                titleText: AppTheme.zhNickname()
                valueText: AppCtrl.nickname
                actionText: AppTheme.zhEdit()
                emphasized: true
                clickable: true
                onClicked: root.openNicknameDialog()
                opacity: 0
                transform: Translate { id: nicknameCardOffset; y: 20 }
            }

            SettingCard {
                id: portCard
                width: parent.width
                height: 84
                titleText: AppTheme.zhDefaultPort()
                valueText: AppCtrl.defaultPort.toString()
                actionText: AppTheme.zhModify()
                clickable: true
                onClicked: root.openPortDialog()
                opacity: 0
                transform: Translate { id: portCardOffset; y: 20 }
            }

            SettingCard {
                id: addressCard
                width: parent.width
                height: 84
                titleText: "局域网地址"
                valueText: AppCtrl.networkManager.localIp + " : " + AppCtrl.defaultPort
                actionText: ""
                opacity: 0
                transform: Translate { id: addressCardOffset; y: 20 }
            }
        }
    }

    SequentialAnimation {
        id: entryAnim
        running: true

        ParallelAnimation {
            NumberAnimation { target: nicknameCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: nicknameCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: portCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: portCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: addressCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: addressCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }

    Dialog {
        id: nicknameDialog
        modal: true
        width: Math.min(root.width - 40, 320)
        x: (root.width - width) / 2
        y: Math.max(24, (root.height - implicitHeight) / 2)
        title: "修改昵称"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: nicknameInput.forceActiveFocus()
        onAccepted: {
            if (!AppCtrl.updateNickname(nicknameDraft)) {
                if (nicknameDraft.trim().length === 0) {
                    nicknameError.text = "昵称不能为空"
                    open()
                }
            } else {
                nicknameError.text = ""
            }
        }
        onRejected: nicknameError.text = ""

        contentItem: ColumnLayout {
            spacing: 12

            TextField {
                id: nicknameInput
                Layout.fillWidth: true
                text: root.nicknameDraft
                placeholderText: "输入昵称"
                selectByMouse: true
                onTextChanged: {
                    root.nicknameDraft = text
                    nicknameError.text = ""
                }
            }

            Text {
                id: nicknameError
                Layout.fillWidth: true
                visible: text.length > 0
                color: "#B14E44"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: portDialog
        modal: true
        width: Math.min(root.width - 40, 320)
        x: (root.width - width) / 2
        y: Math.max(24, (root.height - implicitHeight) / 2)
        title: "修改默认端口"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: portInput.forceActiveFocus()
        onAccepted: {
            var parsed = parseInt(portDraft, 10)
            if (!AppCtrl.updateDefaultPort(parsed)) {
                portError.text = "端口范围必须是 1 - 65535"
                open()
            } else {
                portError.text = ""
            }
        }
        onRejected: portError.text = ""

        contentItem: ColumnLayout {
            spacing: 12

            TextField {
                id: portInput
                Layout.fillWidth: true
                text: root.portDraft
                placeholderText: "输入端口"
                inputMethodHints: Qt.ImhDigitsOnly
                selectByMouse: true
                onTextChanged: {
                    root.portDraft = text
                    portError.text = ""
                }
            }

            Text {
                id: portError
                Layout.fillWidth: true
                visible: text.length > 0
                color: "#B14E44"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }

}
