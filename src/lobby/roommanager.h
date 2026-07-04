#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>

class RoomManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList playerList READ playerList NOTIFY playerListChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY hostChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY playerListChanged)
    Q_PROPERTY(int localPlayerIndex READ localPlayerIndex NOTIFY localPlayerIndexChanged)
    Q_PROPERTY(QString gameId READ gameId NOTIFY gameChanged)
    Q_PROPERTY(QString gameName READ gameName NOTIFY gameChanged)
    Q_PROPERTY(int maxPlayers READ maxPlayers NOTIFY gameChanged)
    Q_PROPERTY(int roomCapacity READ roomCapacity CONSTANT)
    Q_PROPERTY(int activePlayerCount READ activePlayerCount NOTIFY playerListChanged)

public:
    explicit RoomManager(QObject *parent = nullptr);

    Q_INVOKABLE void addPlayer(const QString &name, bool host = false, bool ready = false,
                               int playerId = -1,
                               const QString &seatType = QStringLiteral("active"));
    Q_INVOKABLE void addTestPlayer(const QString &name);
    Q_INVOKABLE void toggleReady();
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void setGameId(const QString &gameId);

    QVariantList playerList() const;
    bool isHost() const;
    bool canStart() const;
    int localPlayerIndex() const;
    QString gameId() const { return m_gameId; }
    QString gameName() const;
    int maxPlayers() const;
    int roomCapacity() const { return 8; }
    int activePlayerCount() const;
    void setLocalPlayerId(int playerId);
    bool setPlayerReadyById(int playerId, bool ready);
    bool setPlayerSeatById(int playerId, const QString &seatType);
    bool clearReadyStates();
    bool removePlayerById(int playerId);
    int firstGuestPlayerId() const;
    int firstSpectatorPlayerId() const;
    int activeGuestCount() const;
    QString seatTypeById(int playerId) const;
    bool isPlayerActive(int playerId) const;
    bool localPlayerIsActive() const;

signals:
    void playerListChanged();
    void hostChanged();
    void gameStarted();
    void localPlayerIndexChanged();
    void gameChanged();

private:
    struct Player {
        int playerId = -1;
        QString name;
        bool isHost = false;
        bool isReady = false;
        QString seatType = QStringLiteral("active");
    };

    int indexOfPlayerId(int playerId) const;
    void emitStateChanged();

    QVector<Player> m_players;
    int m_localPlayerId = 0;
    int m_nextGeneratedPlayerId = 1000;
    QString m_gameId = QStringLiteral("gomoku");
};
