#include "gamecontroller.h"

GameController::GameController(QObject *parent)
    : QObject(parent)
{
}

bool GameController::placePiece(int row, int col, int player)
{
    if (m_gameOver)
        return false;
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
        return false;
    if (m_board[row][col] != 0)
        return false;
    if (player != 0 && player != m_currentPlayer)
        return false;

    m_board[row][col] = m_currentPlayer;
    emit boardChanged();

    if (checkWin(row, col)) {
        m_winner = m_currentPlayer;
        m_gameOver = true;
        emit gameOverChanged();
        return true;
    }

    m_currentPlayer = (m_currentPlayer == 1) ? 2 : 1;
    emit turnChanged();
    return true;
}

bool GameController::surrender(int player)
{
    if (m_gameOver)
        return false;
    if (player != 0 && player != m_currentPlayer)
        return false;

    m_winner = (m_currentPlayer == 1) ? 2 : 1;
    m_gameOver = true;
    emit boardChanged();
    emit gameOverChanged();
    return true;
}

void GameController::setGameOver(int winner)
{
    if (m_gameOver)
        return;
    m_winner = winner;
    m_gameOver = true;
    emit boardChanged();
    emit gameOverChanged();
}

void GameController::startNewGame()
{
    reset();
}

void GameController::reset()
{
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            m_board[r][c] = 0;

    m_currentPlayer = 1;
    m_winner = 0;
    m_gameOver = false;

    emit boardChanged();
    emit turnChanged();
    emit gameOverChanged();
}

QVariantList GameController::board() const
{
    QVariantList rows;
    for (int r = 0; r < BOARD_SIZE; ++r) {
        QVariantList row;
        for (int c = 0; c < BOARD_SIZE; ++c)
            row.append(m_board[r][c]);
        rows.append(QVariant(row));
    }
    return rows;
}

bool GameController::checkWin(int row, int col) const
{
    // Check all 4 directions: horizontal, vertical, diagonal, anti-diagonal
    const int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    for (const auto &dir : directions) {
        int count = 1 + countDirection(row, col, dir[0], dir[1])
                      + countDirection(row, col, -dir[0], -dir[1]);
        if (count >= 5)
            return true;
    }
    return false;
}

int GameController::countDirection(int row, int col, int dr, int dc) const
{
    int count = 0;
    int r = row + dr;
    int c = col + dc;
    const int player = m_board[row][col];

    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE
           && m_board[r][c] == player) {
        ++count;
        r += dr;
        c += dc;
    }
    return count;
}
