#include "flightchesscontroller.h"

#include <QRandomGenerator>
#include <QVariantMap>

FlightChessController::FlightChessController(QObject *parent)
    : QObject(parent)
{
    reset();
}

void FlightChessController::startNewGame()
{
    reset();
}

void FlightChessController::reset()
{
    for (auto &playerPlanes : m_positions)
        playerPlanes.fill(BasePosition);
    for (auto &playerCompleted : m_completed)
        playerCompleted.fill(false);
    m_scores.fill(0);

    m_currentPlayer = 1;
    m_diceValue = 0;
    m_hasRolled = false;
    m_gameOver = false;
    m_winner = 0;

    emit boardChanged();
    emit turnChanged();
    emit diceChanged();
    emit gameOverChanged();
    emit statusChanged();
}

int FlightChessController::rollDice()
{
    if (m_gameOver || m_hasRolled)
        return m_diceValue;

    m_diceValue = static_cast<int>(QRandomGenerator::global()->bounded(1, 7));
    m_hasRolled = true;
    emit diceChanged();
    emit statusChanged();

    if (!playerHasMove(m_currentPlayer))
        finishTurn();

    return m_diceValue;
}

bool FlightChessController::setDiceValue(int value)
{
    if (m_gameOver || m_hasRolled || value < 1 || value > 6)
        return false;

    m_diceValue = value;
    m_hasRolled = true;
    emit diceChanged();
    emit statusChanged();

    if (!playerHasMove(m_currentPlayer))
        finishTurn();

    return true;
}

bool FlightChessController::movePlane(int planeIndex)
{
    if (m_gameOver || !m_hasRolled)
        return false;
    if (planeIndex < 0 || planeIndex >= PlaneCount)
        return false;
    if (!canMove(m_currentPlayer, planeIndex))
        return false;

    int &position = m_positions[m_currentPlayer - 1][planeIndex];
    if (position == BasePosition) {
        position = LaunchPosition;
    } else if (position == LaunchPosition) {
        position = m_diceValue - 1;
    } else {
        position += m_diceValue;
        if (position > FinishPosition)
            position = FinishPosition - (position - FinishPosition);
    }

    const int jumpedPosition = nextSameColorPosition(m_currentPlayer, position);
    if (jumpedPosition >= 0)
        position = jumpedPosition;

    const int shortcutPosition = shortcutDestination(m_currentPlayer, position);
    if (shortcutPosition >= 0)
        position = shortcutPosition;

    if (position == FinishPosition) {
        m_completed[m_currentPlayer - 1][planeIndex] = true;
        ++m_scores[m_currentPlayer - 1];
        position = BasePosition;
    } else {
        captureAt(m_currentPlayer, position);
    }

    updateWinner();

    emit boardChanged();
    if (m_gameOver) {
        emit gameOverChanged();
        emit statusChanged();
        return true;
    }

    finishTurn();
    return true;
}

void FlightChessController::setGameOver(int winner)
{
    if (winner < 1 || winner > PlayerCount)
        winner = 0;
    if (m_gameOver && m_winner == winner)
        return;

    m_gameOver = true;
    m_winner = winner;
    m_hasRolled = false;
    emit diceChanged();
    emit gameOverChanged();
    emit statusChanged();
}

QVariantList FlightChessController::planes() const
{
    QVariantList result;
    for (int player = 1; player <= PlayerCount; ++player) {
        for (int i = 0; i < PlaneCount; ++i) {
            const int position = m_positions[player - 1][i];
            QVariantMap plane;
            plane[QStringLiteral("player")] = player;
            plane[QStringLiteral("index")] = i;
            plane[QStringLiteral("position")] = position;
            plane[QStringLiteral("inBase")] = position == BasePosition;
            plane[QStringLiteral("inLaunch")] = position == LaunchPosition;
            plane[QStringLiteral("finished")] = m_completed[player - 1][i];
            plane[QStringLiteral("globalPosition")] = globalTrackPosition(player, position);
            plane[QStringLiteral("canMove")] = canMove(player, i);
            result.append(plane);
        }
    }
    return result;
}

QVariantList FlightChessController::scores() const
{
    QVariantList result;
    for (int player = 1; player <= PlayerCount; ++player) {
        QVariantMap score;
        score[QStringLiteral("player")] = player;
        score[QStringLiteral("value")] = m_scores[player - 1];
        result.append(score);
    }
    return result;
}

QString FlightChessController::statusText() const
{
    if (m_gameOver)
        return QStringLiteral("玩家 %1 获胜").arg(m_winner);

    if (!m_hasRolled)
        return QStringLiteral("玩家 %1 掷骰").arg(m_currentPlayer);

    if (!playerHasMove(m_currentPlayer))
        return QStringLiteral("玩家 %1 无法移动").arg(m_currentPlayer);

    for (int i = 0; i < PlaneCount; ++i) {
        if (!m_completed[m_currentPlayer - 1][i]
            && m_positions[m_currentPlayer - 1][i] == BasePosition
            && m_diceValue == 6) {
            return QStringLiteral("玩家 %1 选择飞机进入起飞区").arg(m_currentPlayer);
        }
    }

    return QStringLiteral("玩家 %1 选择飞机").arg(m_currentPlayer);
}

int FlightChessController::globalTrackPosition(int player, int position) const
{
    if (position < 0 || position >= TrackLength)
        return -1;

    return (startOffsetForPlayer(player) + position) % TrackLength;
}

int FlightChessController::startOffsetForPlayer(int player) const
{
    if (player == 1)
        return 41;
    if (player == 2)
        return 15;
    return 0;
}

int FlightChessController::trackColor(int globalPosition) const
{
    if (globalPosition < 0)
        return 0;

    for (int player = 1; player <= PlayerCount; ++player) {
        const int offset = (globalPosition - startOffsetForPlayer(player) + TrackLength) % TrackLength;
        if (offset % 4 == 0)
            return player;
    }

    return 0;
}

int FlightChessController::nextSameColorPosition(int player, int position) const
{
    if (position < 0 || position >= TrackLength)
        return -1;

    const int globalPosition = globalTrackPosition(player, position);
    if (trackColor(globalPosition) != player)
        return -1;

    const int jumpedPosition = position + 4;
    if (jumpedPosition >= TrackLength)
        return -1;

    return jumpedPosition;
}

int FlightChessController::shortcutDestination(int player, int position) const
{
    Q_UNUSED(player);
    if (position < 0 || position >= TrackLength)
        return -1;

    // Traditional shortcut: fly across the center line from the mid jump point.
    if (position == 18)
        return 30;

    return -1;
}

bool FlightChessController::canMove(int player, int planeIndex) const
{
    if (m_gameOver || !m_hasRolled || player != m_currentPlayer)
        return false;
    if (planeIndex < 0 || planeIndex >= PlaneCount)
        return false;

    const int position = m_positions[player - 1][planeIndex];
    if (m_completed[player - 1][planeIndex])
        return false;
    if (position == BasePosition)
        return m_diceValue == 6;
    return true;
}

bool FlightChessController::playerHasMove(int player) const
{
    for (int i = 0; i < PlaneCount; ++i) {
        if (canMove(player, i))
            return true;
    }
    return false;
}

void FlightChessController::captureAt(int player, int position)
{
    if (position < 0 || position >= TrackLength)
        return;

    const int globalPosition = globalTrackPosition(player, position);
    const int opponent = player == 1 ? 2 : 1;
    for (int i = 0; i < PlaneCount; ++i) {
        const int opponentPosition = m_positions[opponent - 1][i];
        if (opponentPosition >= 0
            && opponentPosition < TrackLength
            && globalTrackPosition(opponent, opponentPosition) == globalPosition) {
            m_positions[opponent - 1][i] = BasePosition;
        }
    }
}

void FlightChessController::finishTurn()
{
    const bool extraTurn = m_diceValue == 6;
    m_hasRolled = false;
    if (!extraTurn)
        m_currentPlayer = m_currentPlayer == 1 ? 2 : 1;

    emit turnChanged();
    emit diceChanged();
    emit statusChanged();
}

void FlightChessController::updateWinner()
{
    for (int player = 1; player <= PlayerCount; ++player) {
        if (m_scores[player - 1] >= PlaneCount) {
            m_winner = player;
            m_gameOver = true;
            return;
        }
    }
}
