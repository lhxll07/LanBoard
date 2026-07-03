import QtQuick
import QtQuick.Controls
import LanBoard

Page {
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
                width: parent.width
                height: 84
                titleText: AppTheme.zhNickname()
                valueText: "lhx"
                actionText: AppTheme.zhEdit()
                emphasized: true
            }

            SettingCard {
                width: parent.width
                height: 84
                titleText: AppTheme.zhDefaultPort()
                valueText: "44567"
                actionText: AppTheme.zhModify()
            }

            SettingCard {
                width: parent.width
                height: 84
                titleText: AppTheme.zhServerValidation()
                valueText: AppTheme.zhReservedServer()
                actionText: AppTheme.zhClose()
            }
        }
    }
}
