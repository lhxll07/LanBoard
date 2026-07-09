#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVariantList>
#include <QVector>

#include "src/common/roomtypes.h"

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
    enum class ActionError {
        None,
        PlayerNotFound,
        PlayerAlreadyExists,
        RoomFull,
        InvalidPlayerId,
        OnlyHostCanStart,
        OnlyHostCanSwitchGame,
        HostLockedActive,
        ActiveSeatFull,
        MissingPlayers,
        PlayersNotReady,
        GameInProgress
    };

    explicit RoomManager(QObject *parent = nullptr);

    void addPlayer(const QString &name, bool host = false, bool ready = false,
                   int playerId = -1,
                   const QString &seatType = QStringLiteral("active"),
                   int piece = 0);
    void setSnapshot(const LanBoard::RoomSnapshot &snapshot);
    void setRoomIdentity(const QString &roomId, const QString &roomName);
    void setGameInProgress(bool inProgress);
    ActionError tryAddRoomPlayer(const QString &name, bool host, int playerId);
    ActionError tryChangeSeat(int playerId, const QString &seatType);
    ActionError trySwitchGame(int playerId, const QString &gameId);
    ActionError tryStartGame(int playerId);
    Q_INVOKABLE void addTestPlayer(const QString &name);
    void toggleReady();
    Q_INVOKABLE void startGame();
    void concludeGame();
    void reset();
    void setGameId(const QString &gameId);

    QVariantList playerList() const;
    bool isHost() const;
    bool canStart() const;
    int localPlayerIndex() const;
    QString roomId() const { return m_roomId; }
    QString roomName() const { return m_roomName; }
    QString gameId() const { return m_gameId; }
    QString gameName() const;
    bool gameInProgress() const { return m_gameInProgress; }
    int maxPlayers() const;
    int roomCapacity() const { return 8; }
    int activePlayerCount() const;
    int allocatePlayerId() const;
    void setLocalPlayerId(int playerId);
    bool setPlayerReadyById(int playerId, bool ready);
    bool setPlayerSeatById(int playerId, const QString &seatType);
    bool clearReadyStates();
    bool removePlayerById(int playerId);
    int firstGuestPlayerId() const;
    int activeGuestCount() const;
    bool isPlayerActive(int playerId) const;
    bool isPlayerHost(int playerId) const;
    int playerPiece(int playerId) const;
    QString playerName(int playerId) const;
    QJsonObject roomStateMessageForPlayer(int playerId, const QString &mode = QString()) const;
    LanBoard::RoomSnapshot snapshot() const;
    QString actionErrorKey(ActionError error) const;

signals:
    void playerListChanged();
    void hostChanged();
    void gameStarted();
    void localPlayerIndexChanged();
    void gameChanged();

private:
    int indexOfPlayerId(int playerId) const;
    void normalizeSeats(bool fillMissingActiveSeats = true);
    void emitStateChanged();

    QVector<LanBoard::RoomPlayerState> m_players;
    int m_localPlayerId = 0;
    int m_nextGeneratedPlayerId = 1000;
    QString m_roomId;
    QString m_roomName;
    QString m_gameId = QStringLiteral("gomoku");
    bool m_gameInProgress = false;
};
