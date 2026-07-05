#pragma once

#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantList>
#include <QVariantMap>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

class RoomDiscoveryService : public QObject
{
    Q_OBJECT

public:
    explicit RoomDiscoveryService(QObject *parent = nullptr);

    QVariantList discoveredRooms() const { return m_discoveredRooms; }

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

signals:
    void discoveredRoomsChanged();
    void errorOccurred(QString message);

private slots:
    void onReadyRead();
    void pruneDiscoveredRooms();
    void broadcastDiscoveryQuery();
    void broadcastPublishedRoom();

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

    bool ensureSocket();
    void sendRoomAnnouncement(const QHostAddress &address, quint16 port);
    void upsertDiscoveredRoom(const QJsonObject &msg, const QHostAddress &senderAddress);
    QVariantMap discoveredRoomToVariant(const DiscoveredRoom &room) const;
    void rebuildDiscoveredRooms();

#ifdef Q_OS_ANDROID
    void acquireMulticastLock();
    void releaseMulticastLock();
#endif

    QUdpSocket *m_socket = nullptr;
    QTimer m_discoveryTimer;
    QTimer m_pruneTimer;
    QTimer m_publishTimer;
    QList<DiscoveredRoom> m_roomEntries;
    QVariantList m_discoveredRooms;

    QString m_roomUid;
    QString m_hostName;
    quint16 m_port = 0;
    int m_playerCount = 0;
    int m_roomCapacity = 2;
    int m_maxPlayers = 2;
    QString m_gameId = QStringLiteral("gomoku");
    QString m_gameName = QStringLiteral("五子棋");
    bool m_inGame = false;
    bool m_active = false;

#ifdef Q_OS_ANDROID
    QJniObject m_multicastLock;
#endif
};
