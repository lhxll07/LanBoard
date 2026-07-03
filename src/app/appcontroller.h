#pragma once

#include <QObject>
#include <QJsonArray>
#include <QTcpSocket>
#include "src/lobby/roommanager.h"
#include "src/game/gamecontroller.h"
#include "src/network/networkmanager.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RoomManager *roomManager READ roomManager CONSTANT)
    Q_PROPERTY(GameController *gameController READ gameController CONSTANT)
    Q_PROPERTY(NetworkManager *networkManager READ networkManager CONSTANT)
    Q_PROPERTY(bool isHostMode READ isHostMode NOTIFY modeChanged)
    Q_PROPERTY(bool isClientMode READ isClientMode NOTIFY modeChanged)
    Q_PROPERTY(int networkPlayerId READ networkPlayerId NOTIFY modeChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    RoomManager *roomManager() const { return m_roomManager; }
    GameController *gameController() const { return m_gameController; }
    NetworkManager *networkManager() const { return m_networkManager; }
    bool isHostMode() const { return m_isHostMode; }
    bool isClientMode() const { return m_isClientMode; }
    int networkPlayerId() const { return m_networkPlayerId; }

    Q_INVOKABLE void startRoomAsHost();
    Q_INVOKABLE void joinRoom(const QString &ip, const QString &playerName);

signals:
    void modeChanged();
    void roomReady();  // Host: server started. Client: connected & received room_state
    void navigationRequested(int page); // 0=home, 1=room, 2=game

private slots:
    void onJoinRequested(const QString &name, QTcpSocket *socket);
    void onRemoteReadyChanged(int playerId, bool ready);
    void onRemoteMoveReceived(int playerId, int row, int col);
    void onRemoteSurrender(int playerId);
    void onRemoteStartGame();
    void broadcastCurrentRoomState();

private:
    RoomManager *m_roomManager = nullptr;
    GameController *m_gameController = nullptr;
    NetworkManager *m_networkManager = nullptr;

    bool m_isHostMode = false;
    bool m_isClientMode = false;
    int m_networkPlayerId = 0;
};
