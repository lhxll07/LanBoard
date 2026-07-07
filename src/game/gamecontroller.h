#pragma once

#include <QVariantList>

#include "src/common/gamecontrollerbase.h"

class GameController : public GameControllerBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList board READ board NOTIFY boardChanged)
    Q_PROPERTY(int currentPlayer READ currentPlayer NOTIFY turnChanged)
    Q_PROPERTY(int winner READ winner NOTIFY gameOverChanged)
    Q_PROPERTY(bool gameOver READ isGameOver NOTIFY gameOverChanged)

public:
    explicit GameController(QObject *parent = nullptr);

    Q_INVOKABLE bool placePiece(int row, int col, int player = 0);
    Q_INVOKABLE bool surrender(int player = 0);
    Q_INVOKABLE void setGameOver(int winner);

    // ---- GameControllerBase ----
    void startNewGame() override;
    void reset() override;
    bool isGameOver() const override { return m_gameOver; }
    int winner() const override { return m_winner; }

    QVariantList board() const;
    int currentPlayer() const { return m_currentPlayer; }

signals:
    void boardChanged();
    void turnChanged();

private:
    static constexpr int BOARD_SIZE = 14;

    int m_board[BOARD_SIZE][BOARD_SIZE] = {};
    int m_currentPlayer = 1;   // 1 = black, 2 = white
    int m_winner = 0;           // 0 = none, 1 = black, 2 = white
    bool m_gameOver = false;

    bool checkWin(int row, int col) const;
    int countDirection(int row, int col, int dr, int dc) const;
};
