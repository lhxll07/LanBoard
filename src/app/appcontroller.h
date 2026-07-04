#pragma once

#include <QObject>
#include <QJsonArray>
#include <QTcpSocket>
#include "src/lobby/roommanager.h"
#include "src/game/gamecontroller.h"
#include "src/game/flightchesscontroller.h"
#include "src/network/networkmanager.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RoomManager *roomManager READ roomManager CONSTANT)
    Q_PROPERTY(GameController *gameController READ gameController CONSTANT)
    Q_PROPERTY(FlightChessController *flightChessController READ flightChessController CONSTANT)
    Q_PROPERTY(NetworkManager *networkManager READ networkManager CONSTANT)
    Q_PROPERTY(bool isHostMode READ isHostMode NOTIFY modeChanged)
    Q_PROPERTY(bool isClientMode READ isClientMode NOTIFY modeChanged)
    Q_PROPERTY(int networkPlayerId READ networkPlayerId NOTIFY modeChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY settingsChanged)
    Q_PROPERTY(quint16 defaultPort READ defaultPort NOTIFY settingsChanged)
    Q_PROPERTY(QString recentJoinIp READ recentJoinIp NOTIFY settingsChanged)
    Q_PROPERTY(quint16 recentJoinPort READ recentJoinPort NOTIFY settingsChanged)
    Q_PROPERTY(QString currentGameKey READ currentGameKey NOTIFY currentGameChanged)
    Q_PROPERTY(QString currentGameName READ currentGameName NOTIFY currentGameChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    RoomManager *roomManager() const { return m_roomManager; }
    GameController *gameController() const { return m_gameController; }
    FlightChessController *flightChessController() const { return m_flightChessController; }
    NetworkManager *networkManager() const { return m_networkManager; }
    bool isHostMode() const { return m_isHostMode; }
    bool isClientMode() const { return m_isClientMode; }
    int networkPlayerId() const { return m_networkPlayerId; }
    QString nickname() const { return m_nickname; }
    quint16 defaultPort() const { return m_defaultPort; }
    QString recentJoinIp() const { return m_recentJoinIp; }
    quint16 recentJoinPort() const { return m_recentJoinPort; }
    QString currentGameKey() const { return m_currentGameKey; }
    QString currentGameName() const;

    Q_INVOKABLE void startLocalMode();
    Q_INVOKABLE void startRoomAsHost();
    Q_INVOKABLE void startFlightChessRoomAsHost();
    Q_INVOKABLE void startFlightChessLocalRoom();
    Q_INVOKABLE void joinRoom(const QString &ip, int port, const QString &playerName);
    Q_INVOKABLE void leaveRoom();
    Q_INVOKABLE void toggleLocalReady();
    Q_INVOKABLE void openOnlinePage();
    Q_INVOKABLE void startFlightChessLocalMode();
    Q_INVOKABLE bool updateNickname(const QString &nickname);
    Q_INVOKABLE bool updateDefaultPort(int port);

signals:
    void modeChanged();
    void settingsChanged();
    void roomReady();  // Host: server started. Client: connected & received room_state
    void navigationRequested(int page); // 0=home, 1=room, 2=game
    void currentGameChanged();

private slots:
    void onJoinRequested(const QString &name, QTcpSocket *socket);
    void onRemoteReadyChanged(int playerId, bool ready);
    void onRemoteMoveReceived(int playerId, int row, int col);
    void onRemoteFlightRoll(int playerId);
    void onRemoteFlightMove(int playerId, int planeIndex);
    void onRemoteSurrender(int playerId);
    void onRemoteStartGame();
    void onClientDisconnected(int playerId);
    void broadcastCurrentRoomState();

private:
    void loadSettings();
    void saveSettings() const;
    QJsonArray currentRoomState() const;
    void startGameByCurrentType(bool broadcast);
    int networkRoleForPlayerId(int playerId) const;

    RoomManager *m_roomManager = nullptr;
    GameController *m_gameController = nullptr;
    FlightChessController *m_flightChessController = nullptr;
    NetworkManager *m_networkManager = nullptr;

    bool m_isHostMode = false;
    bool m_isClientMode = false;
    int m_networkPlayerId = 0;
    int m_activeGuestPlayerId = -1;
    QString m_nickname;
    quint16 m_defaultPort = 44567;
    QString m_recentJoinIp;
    quint16 m_recentJoinPort = 44567;
    QString m_currentGameKey = QStringLiteral("gomoku");
};
