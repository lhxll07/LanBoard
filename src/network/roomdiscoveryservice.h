#pragma once

#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantList>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

class RoomDiscoveryService : public QObject
{
    Q_OBJECT

public:
    static constexpr int DiscoveryStaleMs = 7000;

    explicit RoomDiscoveryService(QObject *parent = nullptr);
    ~RoomDiscoveryService() override;

    QVariantList discoveredRooms() const { return m_discoveredRooms; }
    QString publishedRoomUid() const { return m_roomUid; }

    void setPublishedRoom(const QString &hostName,
                          quint16 port,
                          int playerCount,
                          int roomCapacity,
                          int maxPlayers,
                          const QString &gameId,
                          const QString &gameName,
                          bool inGame,
                          bool active);
    void start();
    void stop();
    void refresh();
    void clear();

    void processAnnouncement(const QJsonObject &message,
                             const QHostAddress &senderAddress,
                             qint64 nowMs = -1);
    void pruneExpired(qint64 nowMs = -1);

signals:
    void discoveredRoomsChanged();
    void errorOccurred(QString message);

private slots:
    void onReadyRead();
    void broadcastDiscoveryQuery();
    void broadcastPublishedRoom();

private:
    struct Endpoint
    {
        QHostAddress address;
        quint16 port = 0;
        qint64 lastSeenMs = 0;
        int preference = 0;
    };

    struct RoomEntry
    {
        QString roomUid;
        QString hostName;
        int playerCount = 0;
        int roomCapacity = 2;
        int maxPlayers = 2;
        QString gameId;
        QString gameName;
        bool inGame = false;
        bool isFull = false;
        QString preferredEndpointKey;
        QList<Endpoint> endpoints;
    };

    static constexpr quint16 DiscoveryPort = 46568;
    static constexpr int DiscoveryIntervalMs = 2500;

    bool ensureSocket();
    QJsonObject roomAnnouncementMessage(const QString &hostIp) const;
    void sendRoomAnnouncement(const QHostAddress &address, quint16 port);
    QString endpointKey(const QHostAddress &address, quint16 port) const;
    int roomIndexForUid(const QString &roomUid) const;
    void updateRoomMetadata(RoomEntry &room, const QJsonObject &message);
    void upsertEndpoint(RoomEntry &room,
                        const QHostAddress &address,
                        quint16 port,
                        qint64 nowMs);
    const Endpoint *preferredEndpoint(const RoomEntry &room) const;
    qint64 roomLastSeen(const RoomEntry &room) const;
    QVariantMap roomToVariant(const RoomEntry &room) const;
    void rebuildDiscoveredRooms();

#ifdef Q_OS_ANDROID
    void acquireMulticastLock();
    void releaseMulticastLock();
#endif

    QUdpSocket *m_socket = nullptr;
    QTimer m_discoveryTimer;
    QTimer m_pruneTimer;
    QTimer m_publishTimer;
    QList<RoomEntry> m_rooms;
    QVariantList m_discoveredRooms;

    QString m_roomUid;
    QString m_hostName;
    quint16 m_port = 0;
    int m_playerCount = 0;
    int m_roomCapacity = 2;
    int m_maxPlayers = 2;
    QString m_gameId = QStringLiteral("gomoku");
    QString m_gameName;
    bool m_inGame = false;
    bool m_active = false;
    bool m_discoveryRunning = false;

#ifdef Q_OS_ANDROID
    QJniObject m_multicastLock;
#endif
};
