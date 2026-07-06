#pragma once

#include <QByteArray>
#include <QObject>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QUdpSocket>
#include <QTimer>
#include <QVariantList>

#include <enet/enet.h>

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
    ~NetworkManager() override;

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
    Q_INVOKABLE void sendSurvivorInput(qreal horizontal, qreal vertical);
    Q_INVOKABLE void sendSurvivorChooseLevelUp(const QString &upgradeId);
    Q_INVOKABLE void sendSurvivorCloseChest();
    void sendStartGame();
    void sendGameOverResult(int winner);
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
    void sendRoomStateToPlayer(int playerId, const QJsonObject &state);
    void sendSurvivorFastPacketToPlayer(int playerId, const QByteArray &payload);
    void sendSurvivorHudPacketToPlayer(int playerId, const QByteArray &payload);

    bool isHost() const { return m_isHost; }
    bool isConnected() const;
    int clientCount() const { return m_peerClients.size(); }
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
    void errorOccurred(QString message);

    // Received from remote (for AppController to process)
    void joinRequested(QString name, int playerId);
    void remoteReadyChanged(int playerId, bool ready);
    void remoteMoveReceived(int playerId, int row, int col);
    void remoteFlightRoll(int playerId);
    void remoteFlightMove(int playerId, int planeIndex);
    void flightRollReceived(int player, int diceValue);
    void flightMoveReceived(int player, int planeIndex);
    void remoteSurrender(int playerId);
    void remoteSeatChanged(int playerId, QString seatType);
    void remoteSurvivorInput(int playerId, qreal horizontal, qreal vertical);
    void remoteSurvivorChooseLevelUp(int playerId, QString upgradeId);
    void remoteSurvivorCloseChest(int playerId);
    void remoteStartGame(QString gameId);
    void remoteDouDiZhuPlay(int playerId, QJsonArray cardIds);
    void remoteDouDiZhuPass(int playerId);
    void douDiZhuStateReceived(QJsonObject state);
    void gameOverReceived(int winner);
    void roomStateReceived(QJsonObject state);
    void survivorFastPacketReceived(QByteArray payload);
    void survivorHudPacketReceived(QByteArray payload);
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
    void onDiscoveryReadyRead();
    void pruneDiscoveredRooms();
    void broadcastDiscoveryQuery();
    void broadcastHostedRoomAnnouncement();
    void serviceEnet();

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

    bool ensureDiscoverySocket();
    QString localIpForPeer(const QHostAddress &peer) const;
    void sendRoomAnnouncement(const QHostAddress &address, quint16 port);
    void broadcastRoomAnnouncement();
    void upsertDiscoveredRoom(const QJsonObject &msg, const QHostAddress &senderAddress);
    QVariantMap discoveredRoomToVariant(const DiscoveredRoom &room) const;
    void rebuildDiscoveredRooms();
    void applyOnlineRooms(const QJsonArray &rooms);
    void connectDedicatedPeer(const QString &host, quint16 port,
                              const QString &playerName, const QString &gameId,
                              const QString &action, const QString &roomId = QString(),
                              const QString &roomName = QString());
    void disconnectEnetPeer(ENetPeer *&peer, ENetHost *&host, bool graceful);
    void disconnectAllHostPeers();
    void broadcastJson(const QJsonObject &obj, ENetPeer *exclude = nullptr);
    void processHostPeerMessage(ENetPeer *peer, const QJsonObject &msg);
    bool processHostPeerBinaryPacket(ENetPeer *peer, const QByteArray &payload);
    void processDedicatedServerMessage(const QJsonObject &msg);
    bool processDedicatedServerBinaryPacket(const QByteArray &payload);
    void processOnlineLobbyMessage(const QJsonObject &msg);
    void sendEnetJson(ENetPeer *peer, ENetHost *host, const QJsonObject &msg);
    void sendEnetRaw(ENetPeer *peer, ENetHost *host,
                     const QByteArray &payload, enet_uint8 channel, enet_uint32 flags);
    int allocateHostPlayerId() const;
    ENetPeer *peerForPlayerId(int playerId) const;
#ifdef Q_OS_ANDROID
    void acquireMulticastLock();
    void releaseMulticastLock();
#endif

    struct EnetClientSession
    {
        ENetPeer *peer = nullptr;
        int playerId = -1;
        QString playerName;
    };

    ENetHost *m_hostServer = nullptr;
    QList<EnetClientSession> m_peerClients;
    QUdpSocket *m_discoverySocket = nullptr;
    QTimer m_discoveryTimer;
    QTimer m_discoveryPruneTimer;
    QTimer m_hostAnnouncementTimer;
    bool m_isHost = false;
    bool m_enetActiveConnection = false;
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
    QTimer m_enetServiceTimer;
    ENetHost *m_clientHost = nullptr;
    ENetPeer *m_serverPeer = nullptr;
    ENetHost *m_lobbyHost = nullptr;
    ENetPeer *m_lobbyPeer = nullptr;
    QString m_pendingConnectAction;
    QString m_pendingPlayerName;
    QString m_pendingGameId;
    QString m_pendingRoomId;
    QString m_pendingRoomName;
#ifdef Q_OS_ANDROID
    QJniObject m_multicastLock;
#endif
};
