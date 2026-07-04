pragma Singleton

import QtQuick

QtObject {
    readonly property color pageBackground: "#F6F2EA"
    readonly property color pageBackgroundAlt: "#EEF2EC"
    readonly property color panelBackground: "#FBFAF6"
    readonly property color cardBackground: "#FFFFFF"
    readonly property color cardBackgroundSoft: "#F8F7F3"
    readonly property color cardBorder: "#E2E5DE"
    readonly property color accent: "#173A31"
    readonly property color accentSoft: "#E2EEE8"
    readonly property color textPrimary: "#1F302A"
    readonly property color textSecondary: "#738179"
    readonly property color textMuted: "#8A958D"
    readonly property color line: "#E4E6E0"
    readonly property color shadowTint: "#16211D"
    readonly property color warmBoard: "#F4E5C7"
    readonly property color warmBoardDeep: "#E1C287"
    readonly property color darkCard: "#202E2A"
    readonly property color darkCardAccent: "#355449"
    readonly property color goldAccent: "#E5D2A1"

    // Shadow colors (ARGB hex: #AARRGGBB)
    readonly property color shadowLight: "#08000000"
    readonly property color shadowMedium: "#04000000"
    readonly property color shadowDeep: "#02000000"

    readonly property int radiusWindow: 32
    readonly property int radiusCard: 28
    readonly property int radiusButton: 26

    readonly property int spacingXs: 8
    readonly property int spacingSm: 12
    readonly property int spacingMd: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 32
    readonly property int spacingXxl: 40
    readonly property int contentWidth: 334
    readonly property int pageTopInset: 20
    readonly property int pageBottomInset: 12
    readonly property int navHeight: 54

    // Font size tokens
    readonly property int fontSizeCaption: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeButton: 15
    readonly property int fontSizeSubtitle: 16
    readonly property int fontSizeTitle: 20
    readonly property int fontSizeHeading: 24
    readonly property int fontSizeHero: 26

    function zhSetting() { return "\u8bbe\u7f6e"; }
    function zhNickname() { return "\u6635\u79f0"; }
    function zhEdit() { return "\u7f16\u8f91"; }
    function zhDefaultPort() { return "\u9ed8\u8ba4\u7aef\u53e3"; }
    function zhModify() { return "\u4fee\u6539"; }
    function zhServerValidation() { return "\u670d\u52a1\u5668\u9a8c\u8bc1"; }
    function zhReservedServer() { return "\u5148\u9884\u7559\uff0c\u540e\u7eed\u7528\u4e8e\u6f14\u793a\u73af\u5883\u63a5\u5165"; }
    function zhClose() { return "\u5173\u95ed"; }
    function zhVisualPrinciples() { return "\u89c6\u89c9\u539f\u5219"; }
    function zhVisualLine1() { return "\u5e73\u53f0\u5c42\u7edf\u4e00\u4f7f\u7528\u7eb8\u9762\u3001\u77f3\u611f\u3001\u58a8\u8272\u3002"; }
    function zhVisualLine2() { return "\u4e0d\u540c\u684c\u6e38\u53ea\u5728\u81ea\u8eab\u9875\u9762\u91cc\u653e\u5927\u4e2a\u6027\u3002"; }
    function zhVisualLine3() { return "\u5e95\u90e8\u5bfc\u822a\u575a\u6301\u7eaf\u6587\u5b57\uff0c\u4e0d\u5f15\u5165\u56fe\u6807\u566a\u97f3\u3002"; }
    function zhHome() { return "\u9996\u9875"; }
    function zhMatch() { return "\u623f\u95f4"; }
    function zhPlayers() { return "\u73a9\u5bb6"; }
    function zhHost() { return "\u623f\u4e3b"; }
    function zhMember() { return "\u6210\u5458"; }
    function zhReady() { return "\u5df2\u51c6\u5907"; }
    function zhNotReady() { return "\u672a\u51c6\u5907"; }
    function zhPrepare() { return "\u51c6\u5907"; }
    function zhStartGame() { return "\u5f00\u59cb\u6e38\u620f"; }
}
