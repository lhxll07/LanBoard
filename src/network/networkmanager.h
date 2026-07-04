#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QList>
#include <QByteArray>
#include <QVariantList>

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

public:
    explicit NetworkManager(QObject *parent = nullptr);

    Q_INVOKABLE void startServer(quint16 port = 44567);
    Q_INVOKABLE void connectToServer(const QString &ip, quint16 port = 44567,
                                     const QString &playerName = QString(),
                                     const QString &gameId = QStringLiteral("gomoku"));
    Q_INVOKABLE void disconnectAll();

    Q_INVOKABLE void sendReady(bool ready);
    Q_INVOKABLE void sendPlacePiece(int row, int col);
    Q_INVOKABLE void sendSurrender();
    Q_INVOKABLE void sendStartGame();
    Q_INVOKABLE void sendDouDiZhuPlay(const QVariantList &cardIds);
    Q_INVOKABLE void sendDouDiZhuPass();
    Q_INVOKABLE void startRoomDiscovery();
    Q_INVOKABLE void stopRoomDiscovery();
    Q_INVOKABLE void refreshRoomDiscovery();
    Q_INVOKABLE void clearDiscoveredRooms();

    bool isHost() const { return m_isHost; }
    bool isConnected() const;
    int clientCount() const { return m_clients.size(); }
    quint16 serverPort() const { return m_serverPort; }
    QString connectedIp() const { return m_connectedIp; }
    quint16 connectedPort() const { return m_connectedPort; }
    QString localIp() const;
    QVariantList discoveredRooms() const { return m_discoveredRooms; }

    void setDiscoveryHostName(const QString &hostName);
    void setDiscoveryGameInProgress(bool inProgress);
    void setDiscoveryRoomInfo(const QString &gameId, const QString &gameName, int maxPlayers);

signals:
    void connectionChanged();
    void clientCountChanged();
    void discoveredRoomsChanged();
    void serverStarted(quint16 port);
    void errorOccurred(QString message);

    // Received from remote (for AppController to process)
    void joinRequested(QString name, QTcpSocket *socket);
    void remoteReadyChanged(int playerId, bool ready);
    void remoteMoveReceived(int playerId, int row, int col);
    void remoteSurrender(int playerId);
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
    void broadcastGameOver(int winner);
    void sendDouDiZhuState(int playerId, const QJsonObject &state);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onDiscoveryReadyRead();
    void pruneDiscoveredRooms();
    void broadcastDiscoveryQuery();

private:
    struct DiscoveredRoom
    {
        QString hostName;
        QString hostIp;
        quint16 port = 0;
        int playerCount = 0;
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
    bool ensureDiscoverySocket();
    QString localIpForPeer(const QHostAddress &peer) const;
    void sendRoomAnnouncement(const QHostAddress &address, quint16 port);
    void upsertDiscoveredRoom(const QJsonObject &msg, const QHostAddress &senderAddress);
    QVariantMap discoveredRoomToVariant(const DiscoveredRoom &room) const;
    void rebuildDiscoveredRooms();
#ifdef Q_OS_ANDROID
    void acquireMulticastLock();
    void releaseMulticastLock();
#endif

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_socket = nullptr;       // client's connection to server
    QList<QTcpSocket *> m_clients;        // server's connected clients
    QUdpSocket *m_discoverySocket = nullptr;
    QTimer m_discoveryTimer;
    QTimer m_discoveryPruneTimer;
    bool m_isHost = false;
    bool m_discoveryGameInProgress = false;
    QString m_discoveryGameId = QStringLiteral("gomoku");
    QString m_discoveryGameName = QStringLiteral("五子棋");
    int m_discoveryMaxPlayers = 2;
    quint16 m_serverPort = 0;
    QString m_connectedIp;
    quint16 m_connectedPort = 0;
    QString m_discoveryHostName;
    QList<DiscoveredRoom> m_discoveredRoomEntries;
    QVariantList m_discoveredRooms;
#ifdef Q_OS_ANDROID
    QJniObject m_multicastLock;
#endif

    // Track which player ID corresponds to which socket
    int m_nextPlayerId = 1;
};
