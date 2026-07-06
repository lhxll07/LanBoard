import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root

    property string nicknameDraft: AppCtrl.nickname
    property string portDraft: AppCtrl.defaultPort.toString()
    property string onlineHostDraft: AppCtrl.onlineServerHost
    property string onlinePortDraft: AppCtrl.onlineServerPort.toString()
    property bool addressCopied: false

    function openNicknameDialog() {
        nicknameDraft = AppCtrl.nickname
        nicknameDialog.open()
    }

    function openPortDialog() {
        portDraft = AppCtrl.defaultPort.toString()
        portDialog.open()
    }

    function openOnlineServerDialog() {
        onlineHostDraft = AppCtrl.onlineServerHost
        onlinePortDraft = AppCtrl.onlineServerPort.toString()
        onlineServerDialog.open()
    }

    function replayEntryAnim() {
        nicknameCard.opacity = 0
        nicknameCardOffset.y = 20
        portCard.opacity = 0
        portCardOffset.y = 20
        addressCard.opacity = 0
        addressCardOffset.y = 20
        onlineServerCard.opacity = 0
        onlineServerCardOffset.y = 20
        entryAnim.stop()
        entryAnim.start()
    }

    Component.onCompleted: replayEntryAnim()
    onVisibleChanged: {
        if (visible)
            replayEntryAnim()
    }

    background: Rectangle {
        color: "transparent"
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + AppTheme.pageBottomInset
        clip: true
        boundsBehavior: Flickable.DragAndOvershootBounds

        Column {
            id: contentColumn
            width: AppTheme.contentWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: AppTheme.pageTopInset
            spacing: 20

            PageHeader {
                eyebrowText: "设备与联机"
                titleText: AppTheme.zhSetting()
                subtitleText: "统一维护本机身份、局域网端口和在线服务器配置。"
            }

            Text {
                width: parent.width
                text: "基础配置"
                color: AppTheme.textMuted
                font.pixelSize: AppTheme.fontSizeCaption
                font.weight: Font.Medium
            }

            SettingCard {
                id: nicknameCard
                width: parent.width
                height: 84
                titleText: AppTheme.zhNickname()
                valueText: AppCtrl.nickname
                actionText: AppTheme.zhEdit()
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
                valueText: AppCtrl.networkManager.localIp + " : " + AppCtrl.defaultPort.toString()
                actionText: root.addressCopied ? "已复制" : "复制"
                clickable: true
                onClicked: {
                    if (AppCtrl.copyText(valueText)) {
                        root.addressCopied = true
                        copyResetTimer.restart()
                    }
                }
                opacity: 0
                transform: Translate { id: addressCardOffset; y: 20 }
            }

            Text {
                width: parent.width
                text: "在线联机"
                color: AppTheme.textMuted
                font.pixelSize: AppTheme.fontSizeCaption
                font.weight: Font.Medium
            }

            SettingCard {
                id: onlineServerCard
                width: parent.width
                height: 92
                titleText: "在线服务器"
                valueText: AppCtrl.onlineServerHost + " : " + AppCtrl.onlineServerPort
                actionText: AppTheme.zhModify()
                clickable: true
                onClicked: root.openOnlineServerDialog()
                opacity: 0
                transform: Translate { id: onlineServerCardOffset; y: 20 }
            }
        }
    }

    SequentialAnimation {
        id: entryAnim
        running: false

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
        PauseAnimation { duration: 80 }
        ParallelAnimation {
            NumberAnimation { target: onlineServerCard; property: "opacity"; to: 1; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { target: onlineServerCardOffset; property: "y"; to: 0; duration: 300; easing.type: Easing.OutCubic }
        }
    }

    Timer {
        id: copyResetTimer
        interval: 1600
        repeat: false
        onTriggered: root.addressCopied = false
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

    Dialog {
        id: onlineServerDialog
        modal: true
        width: Math.min(root.width - 40, 340)
        x: (root.width - width) / 2
        y: Math.max(24, (root.height - implicitHeight) / 2)
        title: "修改在线服务器"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: onlineHostInput.forceActiveFocus()
        onAccepted: {
            var parsed = parseInt(onlinePortDraft, 10)
            if (!AppCtrl.updateOnlineServerEndpoint(onlineHostDraft, parsed)) {
                onlineServerError.text = "请输入有效的服务器地址和 1 - 65535 的端口"
                open()
            } else {
                onlineServerError.text = ""
            }
        }
        onRejected: onlineServerError.text = ""

        contentItem: ColumnLayout {
            spacing: 12

            TextField {
                id: onlineHostInput
                Layout.fillWidth: true
                text: root.onlineHostDraft
                placeholderText: "输入服务器地址或域名"
                selectByMouse: true
                onTextChanged: {
                    root.onlineHostDraft = text
                    onlineServerError.text = ""
                }
            }

            TextField {
                id: onlinePortInput
                Layout.fillWidth: true
                text: root.onlinePortDraft
                placeholderText: "输入端口"
                inputMethodHints: Qt.ImhDigitsOnly
                selectByMouse: true
                onTextChanged: {
                    root.onlinePortDraft = text
                    onlineServerError.text = ""
                }
            }

            Text {
                id: onlineServerError
                Layout.fillWidth: true
                visible: text.length > 0
                color: "#B14E44"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }
}
