#pragma once

#include <QObject>
#include <QVariantList>

class GameController : public QObject
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
    Q_INVOKABLE void startNewGame();
    Q_INVOKABLE void reset();

    QVariantList board() const;
    int currentPlayer() const { return m_currentPlayer; }
    int winner() const { return m_winner; }
    bool isGameOver() const { return m_gameOver; }

signals:
    void boardChanged();
    void turnChanged();
    void gameOverChanged();

private:
    static constexpr int BOARD_SIZE = 14;

    int m_board[BOARD_SIZE][BOARD_SIZE] = {};
    int m_currentPlayer = 1;   // 1 = black, 2 = white
    int m_winner = 0;           // 0 = none, 1 = black, 2 = white
    bool m_gameOver = false;

    bool checkWin(int row, int col) const;
    int countDirection(int row, int col, int dr, int dc) const;
};
