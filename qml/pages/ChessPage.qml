import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LanBoard

Page {
    id: root
    objectName: "chessPage"

    readonly property int boardSize: 8
    readonly property var files: ["a", "b", "c", "d", "e", "f", "g", "h"]
    readonly property var pieceText: ({
        "wK": "♔", "wQ": "♕", "wR": "♖", "wB": "♗", "wN": "♘", "wP": "♙",
        "bK": "♚", "bQ": "♛", "bR": "♜", "bB": "♝", "bN": "♞", "bP": "♟"
    })
    property var board: []
    property string currentTurn: "w"
    property int selectedRow: -1
    property int selectedCol: -1
    property var legalMoves: []
    property var capturedWhite: []
    property var capturedBlack: []
    property var moveHistory: []
    property int whiteSeconds: 600
    property int blackSeconds: 600
    property string winner: ""
    property int halfMove: 1

    function initialBoard() {
        return [
            ["bR", "bN", "bB", "bQ", "bK", "bB", "bN", "bR"],
            ["bP", "bP", "bP", "bP", "bP", "bP", "bP", "bP"],
            ["", "", "", "", "", "", "", ""],
            ["", "", "", "", "", "", "", ""],
            ["", "", "", "", "", "", "", ""],
            ["", "", "", "", "", "", "", ""],
            ["wP", "wP", "wP", "wP", "wP", "wP", "wP", "wP"],
            ["wR", "wN", "wB", "wQ", "wK", "wB", "wN", "wR"]
        ]
    }

    function resetGame() {
        board = initialBoard()
        currentTurn = "w"
        selectedRow = -1
        selectedCol = -1
        legalMoves = []
        capturedWhite = []
        capturedBlack = []
        moveHistory = []
        whiteSeconds = 600
        blackSeconds = 600
        winner = ""
        halfMove = 1
    }

    function pieceAt(row, col) {
        if (row < 0 || row >= boardSize || col < 0 || col >= boardSize)
            return ""
        return board[row][col]
    }

    function pieceColor(piece) {
        return piece.length > 0 ? piece.charAt(0) : ""
    }

    function pieceKind(piece) {
        return piece.length > 1 ? piece.charAt(1) : ""
    }

    function isInside(row, col) {
        return row >= 0 && row < boardSize && col >= 0 && col < boardSize
    }

    function cloneBoard(source) {
        var result = []
        for (var r = 0; r < boardSize; ++r)
            result.push(source[r].slice())
        return result
    }

    function coord(row, col) {
        return files[col] + (8 - row)
    }

    function formatTime(seconds) {
        var safe = Math.max(0, seconds)
        var minutes = Math.floor(safe / 60)
        var rest = safe % 60
        return minutes + ":" + (rest < 10 ? "0" : "") + rest
    }

    function turnText() {
        if (winner.length > 0)
            return winner === "w" ? "白方获胜" : "黑方获胜"
        return currentTurn === "w" ? "白方走棋" : "黑方走棋"
    }

    function addRayMove(moves, row, col, dr, dc, color) {
        var r = row + dr
        var c = col + dc
        while (isInside(r, c)) {
            var target = pieceAt(r, c)
            if (target === "") {
                moves.push({ row: r, col: c })
            } else {
                if (pieceColor(target) !== color)
                    moves.push({ row: r, col: c })
                return
            }
            r += dr
            c += dc
        }
    }

    function pushStep(moves, row, col, color) {
        if (!isInside(row, col))
            return
        var target = pieceAt(row, col)
        if (target === "" || pieceColor(target) !== color)
            moves.push({ row: row, col: col })
    }

    function movesFor(row, col) {
        var piece = pieceAt(row, col)
        var color = pieceColor(piece)
        var kind = pieceKind(piece)
        var moves = []
        if (piece === "")
            return moves

        if (kind === "P") {
            var dir = color === "w" ? -1 : 1
            var startRow = color === "w" ? 6 : 1
            if (pieceAt(row + dir, col) === "") {
                moves.push({ row: row + dir, col: col })
                if (row === startRow && pieceAt(row + dir * 2, col) === "")
                    moves.push({ row: row + dir * 2, col: col })
            }
            for (var pc = -1; pc <= 1; pc += 2) {
                var target = pieceAt(row + dir, col + pc)
                if (target !== "" && pieceColor(target) !== color)
                    moves.push({ row: row + dir, col: col + pc })
            }
        } else if (kind === "N") {
            var knightSteps = [[-2, -1], [-2, 1], [-1, -2], [-1, 2], [1, -2], [1, 2], [2, -1], [2, 1]]
            for (var n = 0; n < knightSteps.length; ++n)
                pushStep(moves, row + knightSteps[n][0], col + knightSteps[n][1], color)
        } else if (kind === "B" || kind === "R" || kind === "Q") {
            var dirs = []
            if (kind === "B" || kind === "Q")
                dirs = dirs.concat([[-1, -1], [-1, 1], [1, -1], [1, 1]])
            if (kind === "R" || kind === "Q")
                dirs = dirs.concat([[-1, 0], [1, 0], [0, -1], [0, 1]])
            for (var d = 0; d < dirs.length; ++d)
                addRayMove(moves, row, col, dirs[d][0], dirs[d][1], color)
        } else if (kind === "K") {
            for (var kr = -1; kr <= 1; ++kr) {
                for (var kc = -1; kc <= 1; ++kc) {
                    if (kr !== 0 || kc !== 0)
                        pushStep(moves, row + kr, col + kc, color)
                }
            }
        }

        return moves
    }

    function isLegalTarget(row, col) {
        for (var i = 0; i < legalMoves.length; ++i) {
            if (legalMoves[i].row === row && legalMoves[i].col === col)
                return true
        }
        return false
    }

    function selectSquare(row, col) {
        if (winner.length > 0)
            return

        var piece = pieceAt(row, col)
        if (selectedRow >= 0 && isLegalTarget(row, col)) {
            movePiece(selectedRow, selectedCol, row, col)
            return
        }

        if (piece !== "" && pieceColor(piece) === currentTurn) {
            selectedRow = row
            selectedCol = col
            legalMoves = movesFor(row, col)
            return
        }

        selectedRow = -1
        selectedCol = -1
        legalMoves = []
    }

    function movePiece(fromRow, fromCol, toRow, toCol) {
        var next = cloneBoard(board)
        var moving = next[fromRow][fromCol]
        var captured = next[toRow][toCol]
        var mover = pieceColor(moving)
        var notation = (mover === "w" ? halfMove + ". " : "... ")
                + pieceText[moving] + " " + coord(fromRow, fromCol) + "-" + coord(toRow, toCol)

        if (captured !== "") {
            notation += " x " + pieceText[captured]
            if (pieceColor(captured) === "w")
                capturedWhite = capturedWhite.concat([captured])
            else
                capturedBlack = capturedBlack.concat([captured])
            if (pieceKind(captured) === "K")
                winner = mover
        }

        if (pieceKind(moving) === "P" && (toRow === 0 || toRow === 7)) {
            moving = mover + "Q"
            notation += "=Q"
        }

        next[toRow][toCol] = moving
        next[fromRow][fromCol] = ""
        board = next
        moveHistory = moveHistory.concat([notation])
        currentTurn = currentTurn === "w" ? "b" : "w"
        if (mover === "b")
            halfMove += 1
        selectedRow = -1
        selectedCol = -1
        legalMoves = []
    }

    Component.onCompleted: resetGame()

    Timer {
        interval: 1000
        repeat: true
        running: winner.length === 0
        onTriggered: {
            if (currentTurn === "w") {
                whiteSeconds -= 1
                if (whiteSeconds <= 0)
                    winner = "b"
            } else {
                blackSeconds -= 1
                if (blackSeconds <= 0)
                    winner = "w"
            }
        }
    }

    background: Rectangle { color: "transparent" }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: pageColumn.height + AppTheme.pageBottomInset
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: pageColumn
            width: AppTheme.contentWidth
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: AppTheme.pageTopInset
            spacing: 14

            Row {
                width: parent.width
                height: 44
                spacing: 10

                ActionButton {
                    width: 82
                    height: 40
                    text: "返回"
                    secondary: true
                    onClicked: root.StackView.view.pop()
                }

                Column {
                    width: parent.width - 92
                    spacing: 2
                    Text {
                        text: "国际象棋"
                        color: AppTheme.textPrimary
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: turnText()
                        color: AppTheme.textSecondary
                        font.pixelSize: 13
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 72
                radius: 8
                color: AppTheme.darkCard

                Row {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Column {
                        width: 96
                        spacing: 4
                        Text { text: "黑方"; color: "#E8EEE9"; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Text { text: formatTime(blackSeconds); color: "#F5F2E9"; font.pixelSize: 24; font.weight: Font.Bold }
                    }

                    Flow {
                        width: parent.width - 108
                        height: parent.height
                        spacing: 2
                        Repeater {
                            model: capturedWhite
                            Text {
                                text: pieceText[modelData]
                                color: "#F6F2EA"
                                font.pixelSize: 18
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: boardFrame
                width: parent.width
                height: width
                radius: 8
                color: "#2A2420"

                Grid {
                    id: boardGrid
                    anchors.fill: parent
                    anchors.margins: 10
                    rows: 8
                    columns: 8

                    Repeater {
                        model: 64

                        Rectangle {
                            id: square
                            required property int index
                            property int row: Math.floor(index / 8)
                            property int col: index % 8
                            property string piece: pieceAt(row, col)
                            property bool selected: row === selectedRow && col === selectedCol
                            property bool legal: isLegalTarget(row, col)

                            width: boardGrid.width / 8
                            height: boardGrid.height / 8
                            color: selected ? "#D9B85F"
                                  : legal ? "#B8C889"
                                  : ((row + col) % 2 === 0 ? "#E9D6B4" : "#8B5E3C")

                            Text {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.leftMargin: 4
                                anchors.topMargin: 2
                                visible: square.col === 0
                                text: 8 - square.row
                                color: (square.row + square.col) % 2 === 0 ? "#735138" : "#F4E7CC"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }

                            Text {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.rightMargin: 4
                                anchors.bottomMargin: 2
                                visible: square.row === 7
                                text: files[square.col]
                                color: (square.row + square.col) % 2 === 0 ? "#735138" : "#F4E7CC"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }

                            Text {
                                anchors.centerIn: parent
                                text: square.piece === "" ? "" : pieceText[square.piece]
                                color: pieceColor(square.piece) === "w" ? "#FCFAF2" : "#191E1A"
                                style: Text.Outline
                                styleColor: pieceColor(square.piece) === "w" ? "#5F5A50" : "#D8C9AB"
                                font.pixelSize: Math.max(24, square.width * 0.62)
                            }

                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                anchors.centerIn: parent
                                visible: square.legal && square.piece === ""
                                color: "#526B3A"
                                opacity: 0.55
                            }

                            TapHandler {
                                onTapped: selectSquare(square.row, square.col)
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 72
                radius: 8
                color: AppTheme.panelBackground
                border.width: 1
                border.color: AppTheme.cardBorder

                Row {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Column {
                        width: 96
                        spacing: 4
                        Text { text: "白方"; color: AppTheme.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Text { text: formatTime(whiteSeconds); color: AppTheme.accent; font.pixelSize: 24; font.weight: Font.Bold }
                    }

                    Flow {
                        width: parent.width - 108
                        height: parent.height
                        spacing: 2
                        Repeater {
                            model: capturedBlack
                            Text {
                                text: pieceText[modelData]
                                color: AppTheme.textPrimary
                                font.pixelSize: 18
                            }
                        }
                    }
                }
            }

            Row {
                width: parent.width
                height: 48
                spacing: 10

                ActionButton {
                    width: (parent.width - 10) / 2
                    height: 48
                    text: "重新开始"
                    onClicked: resetGame()
                }

                ActionButton {
                    width: (parent.width - 10) / 2
                    height: 48
                    text: currentTurn === "w" ? "白方认输" : "黑方认输"
                    secondary: true
                    enabled: winner.length === 0
                    onClicked: winner = currentTurn === "w" ? "b" : "w"
                }
            }

            Rectangle {
                width: parent.width
                height: Math.max(116, historyColumn.height + 28)
                radius: 8
                color: AppTheme.cardBackground
                border.width: 1
                border.color: AppTheme.cardBorder

                Column {
                    id: historyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        text: "走子记录"
                        color: AppTheme.textPrimary
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Text {
                        width: parent.width
                        text: moveHistory.length === 0 ? "开始后会在这里显示每一步。" : moveHistory.join("   ")
                        color: moveHistory.length === 0 ? AppTheme.textMuted : AppTheme.textSecondary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
