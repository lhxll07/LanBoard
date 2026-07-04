#pragma once

#include <QObject>
#include <QVariantList>
#include <array>

class FlightChessController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList planes READ planes NOTIFY boardChanged)
    Q_PROPERTY(QVariantList scores READ scores NOTIFY boardChanged)
    Q_PROPERTY(int currentPlayer READ currentPlayer NOTIFY turnChanged)
    Q_PROPERTY(int diceValue READ diceValue NOTIFY diceChanged)
    Q_PROPERTY(bool hasRolled READ hasRolled NOTIFY diceChanged)
    Q_PROPERTY(bool gameOver READ isGameOver NOTIFY gameOverChanged)
    Q_PROPERTY(int winner READ winner NOTIFY gameOverChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit FlightChessController(QObject *parent = nullptr);

    Q_INVOKABLE void startNewGame();
    Q_INVOKABLE void reset();
    Q_INVOKABLE int rollDice();
    Q_INVOKABLE bool setDiceValue(int value);
    Q_INVOKABLE bool movePlane(int planeIndex);
    Q_INVOKABLE void setGameOver(int winner);

    QVariantList planes() const;
    QVariantList scores() const;
    int currentPlayer() const { return m_currentPlayer; }
    int diceValue() const { return m_diceValue; }
    bool hasRolled() const { return m_hasRolled; }
    bool isGameOver() const { return m_gameOver; }
    int winner() const { return m_winner; }
    QString statusText() const;

signals:
    void boardChanged();
    void turnChanged();
    void diceChanged();
    void gameOverChanged();
    void statusChanged();

private:
    static constexpr int PlayerCount = 2;
    static constexpr int PlaneCount = 4;
    static constexpr int BasePosition = -1;
    static constexpr int LaunchPosition = -2;
    static constexpr int TrackLength = 52;
    static constexpr int HomeLength = 6;
    static constexpr int FinishPosition = TrackLength + HomeLength;

    std::array<std::array<int, PlaneCount>, PlayerCount> m_positions = {};
    std::array<std::array<bool, PlaneCount>, PlayerCount> m_completed = {};
    std::array<int, PlayerCount> m_scores = {};
    int m_currentPlayer = 1;
    int m_diceValue = 0;
    bool m_hasRolled = false;
    bool m_gameOver = false;
    int m_winner = 0;

    int globalTrackPosition(int player, int position) const;
    int startOffsetForPlayer(int player) const;
    int trackColor(int globalPosition) const;
    int nextSameColorPosition(int player, int position) const;
    int shortcutDestination(int player, int position) const;
    bool canMove(int player, int planeIndex) const;
    bool playerHasMove(int player) const;
    void captureAt(int player, int position);
    void finishTurn();
    void updateWinner();
};
