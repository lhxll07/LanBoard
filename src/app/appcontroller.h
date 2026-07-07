#pragma once

#include <QObject>
#include <QJsonArray>
#include <QVariantList>

#include "src/common/types.h"
#include "src/game/doudizhucontroller.h"
#include "src/game/flightchesscontroller.h"
#include "src/game/gamecontroller.h"
#include "src/game/survivorcontroller.h"
#include "src/lobby/roommanager.h"
#include "src/network/networkmanager.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RoomManager *roomManager READ roomManager CONSTANT)
    Q_PROPERTY(GameController *gameController READ gameController CONSTANT)
    Q_PROPERTY(DouDiZhuController *douDiZhuController READ douDiZhuController CONSTANT)
    Q_PROPERTY(FlightChessController *flightChessController READ flightChessController CONSTANT)
    Q_PROPERTY(SurvivorController *survivorController READ survivorController CONSTANT)
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
    Q_PROPERTY(QVariantList availableGames READ availableGames CONSTANT)
    Q_PROPERTY(QString lobbyGameId READ lobbyGameId NOTIFY lobbyGameChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    RoomManager *roomManager() const { return m_roomManager; }
    GameController *gameController() const { return m_gameController; }
    DouDiZhuController *douDiZhuController() const { return m_douDiZhuController; }
    FlightChessController *flightChessController() const { return m_flightChessController; }
    SurvivorController *survivorController() const { return m_survivorController; }
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
    QVariantList availableGames() const { return LanBoard::availableGames(); }
    QString lobbyGameId() const { return m_lobbyGameId; }

    Q_INVOKABLE void startLocalGame(const QString &gameId);
    Q_INVOKABLE void startSoloSurvivorSession();
    Q_INVOKABLE void startRoomAsHost(const QString &gameId = QStringLiteral("gomoku"));
    Q_INVOKABLE void joinRoom(const QString &ip, int port, const QString &playerName,
                              const QString &gameId = QStringLiteral("gomoku"));
    Q_INVOKABLE void leaveRoom();
    Q_INVOKABLE void toggleLocalReady();
    Q_INVOKABLE void switchRoomGame(const QString &gameId);
    Q_INVOKABLE void requestSeatChange(const QString &seatType);
    Q_INVOKABLE bool playDouDiZhuCards(const QVariantList &cardIds);
    Q_INVOKABLE bool passDouDiZhuTurn();
    Q_INVOKABLE void restartDouDiZhuGame();
    Q_INVOKABLE void refreshOnlineRooms();
    Q_INVOKABLE void createOnlineRoom(const QString &gameId,
                                      const QString &roomName = QString());
    Q_INVOKABLE void joinOnlineRoom(const QString &roomId);
    Q_INVOKABLE void openLobbyForGame(const QString &gameId);
    Q_INVOKABLE bool updateNickname(const QString &nickname);
    Q_INVOKABLE bool updateDefaultPort(int port);
    Q_INVOKABLE bool updateOnlineServerEndpoint(const QString &host, int port);
    Q_INVOKABLE bool copyText(const QString &text);

signals:
    void modeChanged();
    void settingsChanged();
    void lobbyGameChanged();
    void roomReady();  // Host: server started. Client: connected & received room_state
    void navigationRequested(int page);

private slots:
    void onJoinRequested(const QString &name, int playerId);
    void onRemoteReadyChanged(int playerId, bool ready);
    void onRemoteMoveReceived(int playerId, int row, int col);
    void onRemoteFlightRoll(int playerId);
    void onRemoteFlightMove(int playerId, int planeIndex);
    void onRemoteSurrender(int playerId);
    void onRemoteSeatChanged(int playerId, const QString &seatType);
    void onRemoteStartGame(const QString &gameId);
    void onRemoteDouDiZhuPlay(int playerId, const QJsonArray &cardIds);
    void onRemoteDouDiZhuPass(int playerId);
    void onClientDisconnected(int playerId);
    void broadcastCurrentRoomState();
    void broadcastDouDiZhuStates();

private:
    void loadSettings();
    void saveSettings() const;
    LanBoard::RoomSnapshot currentRoomSnapshot() const;
    QJsonArray currentRoomState() const;
    QString currentGameId() const;
    int currentGamePage() const;
    GameControllerBase *activeController() const;
    void setLobbyGameId(const QString &gameId);
    void setModeState(bool hostMode, bool clientMode, int playerId);
    void syncActiveGuestPlayerId();
    void applyReceivedGameOver(int winner);
    void handleActiveGuestDisconnectInCurrentGame();
    void resetGameControllers();
    void resetRoomSession(const QString &gameId, int localPlayerId = -1);
    void startCurrentGameSession();
    void finishCurrentGameSession(int winner, bool resetOfflineRoom);
    void startCurrentGameRuntime(bool waitForRemoteState = false);
    LanBoard::GameControllerKind currentControllerKind() const;
    bool isCurrentGame(LanBoard::GameControllerKind kind) const;
    void navigateToCurrentGame();
    void configureRoomGame(const QString &gameId);
    void normalizeRoomSeatsForCurrentGame();
    int roomCapacity() const;

    RoomManager *m_roomManager = nullptr;
    GameController *m_gameController = nullptr;
    DouDiZhuController *m_douDiZhuController = nullptr;
    FlightChessController *m_flightChessController = nullptr;
    SurvivorController *m_survivorController = nullptr;
    NetworkManager *m_networkManager = nullptr;

    bool m_isHostMode = false;
    bool m_isClientMode = false;
    bool m_isDedicatedServerRoom = false;
    int m_networkPlayerId = 0;
    int m_activeGuestPlayerId = -1;
    QString m_nickname;
    quint16 m_defaultPort = 44567;
    QString m_recentJoinIp;
    quint16 m_recentJoinPort = 44567;
    QString m_onlineServerName = QStringLiteral("ECS \u6f14\u793a\u670d\u52a1\u5668");
    QString m_onlineServerHost = QStringLiteral("47.105.54.227");
    quint16 m_onlineServerPort = 44567;
    QString m_lobbyGameId = QStringLiteral("gomoku");
};
