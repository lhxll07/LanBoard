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

public:
    explicit RoomManager(QObject *parent = nullptr);

    Q_INVOKABLE void addPlayer(const QString &name, bool host = false, bool ready = false,
                               int playerId = -1);
    Q_INVOKABLE void addTestPlayer(const QString &name);
    Q_INVOKABLE void toggleReady();
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void reset();

    QVariantList playerList() const;
    bool isHost() const;
    bool canStart() const;
    int localPlayerIndex() const;
    void setLocalPlayerId(int playerId);
    bool setPlayerReadyById(int playerId, bool ready);
    bool clearReadyStates();
    bool removePlayerById(int playerId);
    int firstGuestPlayerId() const;

signals:
    void playerListChanged();
    void hostChanged();
    void gameStarted();
    void localPlayerIndexChanged();

private:
    struct Player {
        int playerId = -1;
        QString name;
        bool isHost = false;
        bool isReady = false;
    };

    int indexOfPlayerId(int playerId) const;
    void emitStateChanged();

    QVector<Player> m_players;
    int m_localPlayerId = 0;
    int m_nextGeneratedPlayerId = 1000;
};
