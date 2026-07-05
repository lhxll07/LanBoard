#pragma once

#include <QObject>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QVariantList>

#include "src/common/types.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

class NetworkManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isHost READ isHost NOTIFY connectionChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)
    Q_PROPERTY(quint16 serverPort READ serverPort NOTIFY connectionChanged)
    Q_PROPERTY(QString connectedIp READ connectedIp NOTIFY connectionChanged)
    Q_PROPERTY(quint16 connectedPort READ connectedPort NOTIFY connectionChanged)
    Q_PROPERTY(QString localIp READ localIp CONSTANT)
    Q_PROPERTY(QVariantList discoveredRooms READ discoveredRooms NOTIFY discoveredRoomsChanged)
    Q_PROPERTY(QVariantList onlineRooms READ onlineRooms NOTIFY onlineRoomsChanged)

public:
    explicit NetworkManager(QObject *parent = nullptr);

    void startServer(quint16 port = 44567);
    void connectToServer(const QString &ip, quint16 port = 44567,
                         const QString &playerName = QString(),
                         const QString &gameId = QStringLiteral("gomoku"));
    void disconnectAll();

    Q_INVOKABLE void sendReady(bool ready);
    Q_INVOKABLE void sendPlacePiece(int row, int col);
    Q_INVOKABLE void sendFlightRoll();
    Q_INVOKABLE void sendFlightMove(int planeIndex);
    Q_INVOKABLE void sendSurrender();
    void sendStartGame();
    void sendDouDiZhuPlay(const QVariantList &cardIds);
    void sendDouDiZhuPass();
    void sendChangeSeat(const QString &seatType);
    Q_INVOKABLE void startRoomDiscovery();
    Q_INVOKABLE void stopRoomDiscovery();
    Q_INVOKABLE void refreshRoomDiscovery();
    Q_INVOKABLE void clearDiscoveredRooms();
    void requestOnlineRooms(const QString &host, quint16 port = 44567);
    void clearOnlineRooms();
    void createOnlineRoom(const QString &host, quint16 port,
                          const QString &playerName, const QString &gameId,
                          const QString &roomName = QString());
    void joinOnlineRoom(const QString &host, quint16 port,
                        const QString &playerName, const QString &roomId);
    void sendSwitchRoomGame(const QString &gameId);

    bool isHost() const { return m_isHost; }
    bool isConnected() const;
    int clientCount() const { return m_clients.size(); }
    quint16 serverPort() const { return m_serverPort; }
    QString connectedIp() const { return m_connectedIp; }
    quint16 connectedPort() const { return m_connectedPort; }
    QString localIp() const;
    QVariantList discoveredRooms() const { return m_discoveredRooms; }
    QVariantList onlineRooms() const { return m_onlineRooms; }

    void setDiscoveryHostName(const QString &hostName);
    void setDiscoveryGameInProgress(bool inProgress);
    void setDiscoveryRoomInfo(const QString &gameId, const QString &gameName,
                              int roomCapacity, int maxPlayers);

signals:
    void connectionChanged();
    void clientCountChanged();
    void discoveredRoomsChanged();
    void onlineRoomsChanged();
    void serverStarted(quint16 port);
    void errorOccurred(QString message);

    // Received from remote (for AppController to process)
    void joinRequested(QString name, QTcpSocket *socket);
    void remoteReadyChanged(int playerId, bool ready);
    void remoteMoveReceived(int playerId, int row, int col);
    void remoteFlightRoll(int playerId);
    void remoteFlightMove(int playerId, int planeIndex);
    void flightRollReceived(int player, int diceValue);
    void flightMoveReceived(int player, int planeIndex);
    void remoteSurrender(int playerId);
    void remoteSeatChanged(int playerId, QString seatType);
    void remoteStartGame(QString gameId);
    void remoteDouDiZhuPlay(int playerId, QJsonArray cardIds);
    void remoteDouDiZhuPass(int playerId);
    void douDiZhuStateReceived(QJsonObject state);
    void gameOverReceived(int winner);
    void roomStateReceived(QJsonObject state);
    void clientDisconnected(int playerId);

public slots:
    // Called by AppController to broadcast state changes
    void broadcastRoomState(const QJsonArray &players);
    void broadcastGameStarted(const QString &gameId = QString());
    void broadcastMove(int player, int row, int col);
    void broadcastFlightRoll(int player, int diceValue);
    void broadcastFlightMove(int player, int planeIndex);
    void broadcastGameOver(int winner);
    void sendDouDiZhuState(int playerId, const QJsonObject &state);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onOnlineLobbyConnected();
    void onOnlineLobbyReadyRead();
    void onOnlineLobbyDisconnected();
    void onOnlineLobbyError(QAbstractSocket::SocketError error);
    void onDiscoveryReadyRead();
    void pruneDiscoveredRooms();
    void broadcastDiscoveryQuery();
    void broadcastHostedRoomAnnouncement();

private:
    struct DiscoveredRoom
    {
        QString roomUid;
        QString hostName;
        QString hostIp;
        quint16 port = 0;
        int playerCount = 0;
        int roomCapacity = 2;
        int maxPlayers = 2;
        QString gameId;
        QString gameName;
        bool inGame = false;
        bool isFull = false;
        qint64 lastSeenMs = 0;
    };

    static constexpr quint16 DiscoveryPort = 44568;
    static constexpr int DiscoveryIntervalMs = 2500;
    static constexpr int DiscoveryStaleMs = 7000;

    void sendJson(QTcpSocket *socket, const QJsonObject &obj);
    void broadcastJson(const QJsonObject &obj, QTcpSocket *exclude = nullptr);
    void processMessage(QTcpSocket *sender, const QJsonObject &msg);
    void connectClientSocket(const QString &ip, quint16 port,
                             const QString &playerName, const QString &gameId,
                             const QString &action, const QString &roomId = QString(),
                             const QString &roomName = QString());
    bool ensureDiscoverySocket();
    QString localIpForPeer(const QHostAddress &peer) const;
    void sendRoomAnnouncement(const QHostAddress &address, quint16 port);
    void broadcastRoomAnnouncement();
    void upsertDiscoveredRoom(const QJsonObject &msg, const QHostAddress &senderAddress);
    QVariantMap discoveredRoomToVariant(const DiscoveredRoom &room) const;
    void rebuildDiscoveredRooms();
    void applyOnlineRooms(const QJsonArray &rooms);
#ifdef Q_OS_ANDROID
    void acquireMulticastLock();
    void releaseMulticastLock();
#endif

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_socket = nullptr;       // client's connection to server
    QTcpSocket *m_onlineLobbySocket = nullptr;
    QList<QTcpSocket *> m_clients;        // server's connected clients
    QUdpSocket *m_discoverySocket = nullptr;
    QTimer m_discoveryTimer;
    QTimer m_discoveryPruneTimer;
    QTimer m_hostAnnouncementTimer;
    bool m_isHost = false;
    bool m_discoveryGameInProgress = false;
    QString m_discoveryGameId = LanBoard::normalizeGameId(QStringLiteral("gomoku"));
    QString m_discoveryGameName = LanBoard::gameName(QStringLiteral("gomoku"));
    int m_discoveryRoomCapacity = 2;
    int m_discoveryMaxPlayers = 2;
    quint16 m_serverPort = 0;
    QString m_connectedIp;
    quint16 m_connectedPort = 0;
    QString m_discoveryHostName;
    QString m_discoveryRoomUid;
    QList<DiscoveredRoom> m_discoveredRoomEntries;
    QVariantList m_discoveredRooms;
    QVariantList m_onlineRooms;
#ifdef Q_OS_ANDROID
    QJniObject m_multicastLock;
#endif

    // Track which player ID corresponds to which socket
    int m_nextPlayerId = 1;
};
