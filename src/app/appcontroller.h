#pragma once

#include <QObject>
#include <QJsonArray>
#include <QVariantList>
#include <QTcpSocket>
#include "src/lobby/roommanager.h"
#include "src/game/gamecontroller.h"
#include "src/game/doudizhucontroller.h"
#include "src/network/networkmanager.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RoomManager *roomManager READ roomManager CONSTANT)
    Q_PROPERTY(GameController *gameController READ gameController CONSTANT)
    Q_PROPERTY(DouDiZhuController *douDiZhuController READ douDiZhuController CONSTANT)
    Q_PROPERTY(NetworkManager *networkManager READ networkManager CONSTANT)
    Q_PROPERTY(bool isHostMode READ isHostMode NOTIFY modeChanged)
    Q_PROPERTY(bool isClientMode READ isClientMode NOTIFY modeChanged)
    Q_PROPERTY(int networkPlayerId READ networkPlayerId NOTIFY modeChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY settingsChanged)
    Q_PROPERTY(quint16 defaultPort READ defaultPort NOTIFY settingsChanged)
    Q_PROPERTY(QString recentJoinIp READ recentJoinIp NOTIFY settingsChanged)
    Q_PROPERTY(quint16 recentJoinPort READ recentJoinPort NOTIFY settingsChanged)
    Q_PROPERTY(QString onlineServerName READ onlineServerName NOTIFY settingsChanged)
    Q_PROPERTY(QString onlineServerHost READ onlineServerHost NOTIFY settingsChanged)
    Q_PROPERTY(quint16 onlineServerPort READ onlineServerPort NOTIFY settingsChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    RoomManager *roomManager() const { return m_roomManager; }
    GameController *gameController() const { return m_gameController; }
    DouDiZhuController *douDiZhuController() const { return m_douDiZhuController; }
    NetworkManager *networkManager() const { return m_networkManager; }
    bool isHostMode() const { return m_isHostMode; }
    bool isClientMode() const { return m_isClientMode; }
    int networkPlayerId() const { return m_networkPlayerId; }
    QString nickname() const { return m_nickname; }
    quint16 defaultPort() const { return m_defaultPort; }
    QString recentJoinIp() const { return m_recentJoinIp; }
    quint16 recentJoinPort() const { return m_recentJoinPort; }
    QString onlineServerName() const { return m_onlineServerName; }
    QString onlineServerHost() const { return m_onlineServerHost; }
    quint16 onlineServerPort() const { return m_onlineServerPort; }

    Q_INVOKABLE void startLocalMode();
    Q_INVOKABLE void startDouDiZhuLocalMode();
    Q_INVOKABLE void startRoomAsHost();
    Q_INVOKABLE void startDouDiZhuRoomAsHost();
    Q_INVOKABLE void joinRoom(const QString &ip, int port, const QString &playerName);
    Q_INVOKABLE void leaveRoom();
    Q_INVOKABLE void toggleLocalReady();
    Q_INVOKABLE void openOnlinePage();
    Q_INVOKABLE void openDouDiZhuPage();
    Q_INVOKABLE bool playDouDiZhuCards(const QVariantList &cardIds);
    Q_INVOKABLE bool passDouDiZhuTurn();
    Q_INVOKABLE void restartDouDiZhuGame();
    Q_INVOKABLE void joinOnlineServer();
    Q_INVOKABLE bool updateNickname(const QString &nickname);
    Q_INVOKABLE bool updateDefaultPort(int port);
    Q_INVOKABLE bool updateOnlineServerEndpoint(const QString &host, int port);

signals:
    void modeChanged();
    void settingsChanged();
    void roomReady();  // Host: server started. Client: connected & received room_state
    void navigationRequested(int page); // 0=home, 1=room, 2=game, 3=online page, 4=doudizhu

private slots:
    void onJoinRequested(const QString &name, QTcpSocket *socket);
    void onRemoteReadyChanged(int playerId, bool ready);
    void onRemoteMoveReceived(int playerId, int row, int col);
    void onRemoteSurrender(int playerId);
    void onRemoteStartGame(const QString &gameId);
    void onRemoteDouDiZhuPlay(int playerId, const QJsonArray &cardIds);
    void onRemoteDouDiZhuPass(int playerId);
    void onClientDisconnected(int playerId);
    void broadcastCurrentRoomState();
    void broadcastDouDiZhuStates();

private:
    void loadSettings();
    void saveSettings() const;
    QJsonArray currentRoomState() const;
    bool isDouDiZhuRoom() const;
    void configureRoomGame(const QString &gameId);

    RoomManager *m_roomManager = nullptr;
    GameController *m_gameController = nullptr;
    DouDiZhuController *m_douDiZhuController = nullptr;
    NetworkManager *m_networkManager = nullptr;

    bool m_isHostMode = false;
    bool m_isClientMode = false;
    int m_networkPlayerId = 0;
    int m_activeGuestPlayerId = -1;
    QString m_nickname;
    quint16 m_defaultPort = 44567;
    QString m_recentJoinIp;
    quint16 m_recentJoinPort = 44567;
    QString m_onlineServerName = QStringLiteral("ECS 演示服务器");
    QString m_onlineServerHost = QStringLiteral("47.105.54.227");
    quint16 m_onlineServerPort = 44567;
};
