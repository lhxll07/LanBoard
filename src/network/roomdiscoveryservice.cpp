#include "roomdiscoveryservice.h"

#include "networkaddressutils.h"
#include "src/common/types.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkDatagram>
#include <QNetworkProxy>
#include <QSet>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QtCore/qcoreapplication_platform.h>
#endif

RoomDiscoveryService::RoomDiscoveryService(QObject *parent)
    : QObject(parent)
    , m_roomUid(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    m_discoveryTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_discoveryTimer, &QTimer::timeout,
            this, &RoomDiscoveryService::broadcastDiscoveryQuery);

    m_pruneTimer.setInterval(1000);
    connect(&m_pruneTimer, &QTimer::timeout, this, [this]() {
        pruneExpired();
    });

    m_publishTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_publishTimer, &QTimer::timeout,
            this, &RoomDiscoveryService::broadcastPublishedRoom);
}

RoomDiscoveryService::~RoomDiscoveryService()
{
#ifdef Q_OS_ANDROID
    releaseMulticastLock();
#endif
}

void RoomDiscoveryService::setPublishedRoom(const QString &hostName,
                                            quint16 port,
                                            int playerCount,
                                            int roomCapacity,
                                            int maxPlayers,
                                            const QString &gameId,
                                            const QString &gameName,
                                            bool inGame,
                                            bool active)
{
    const bool wasActive = m_active;
    m_hostName = hostName.trimmed();
    m_port = port;
    m_playerCount = qMax(0, playerCount);
    m_roomCapacity = qMax(2, roomCapacity);
    m_maxPlayers = qMax(2, maxPlayers);
    m_gameId = LanBoard::normalizeGameId(gameId);
    m_gameName = gameName.trimmed().isEmpty() ? LanBoard::gameName(m_gameId) : gameName.trimmed();
    m_inGame = inGame;
    m_active = active && m_port != 0;

    if (m_active) {
        if (!ensureSocket()) {
            m_active = false;
            return;
        }
#ifdef Q_OS_ANDROID
        acquireMulticastLock();
#endif
        if (!m_publishTimer.isActive())
            m_publishTimer.start();
        broadcastPublishedRoom();
        return;
    }

    if (wasActive)
        m_publishTimer.stop();
#ifdef Q_OS_ANDROID
    if (!m_discoveryRunning)
        releaseMulticastLock();
#endif
}

void RoomDiscoveryService::start()
{
    if (!ensureSocket())
        return;

    m_discoveryRunning = true;
#ifdef Q_OS_ANDROID
    acquireMulticastLock();
#endif
    if (!m_pruneTimer.isActive())
        m_pruneTimer.start();
    if (!m_discoveryTimer.isActive())
        m_discoveryTimer.start();
    refresh();
}

void RoomDiscoveryService::stop()
{
    m_discoveryRunning = false;
    m_discoveryTimer.stop();
    m_pruneTimer.stop();
#ifdef Q_OS_ANDROID
    if (!m_active)
        releaseMulticastLock();
#endif
}

void RoomDiscoveryService::refresh()
{
    if (!ensureSocket())
        return;
    pruneExpired();
    broadcastDiscoveryQuery();
}

void RoomDiscoveryService::clear()
{
    if (m_rooms.isEmpty())
        return;
    m_rooms.clear();
    rebuildDiscoveredRooms();
}

void RoomDiscoveryService::onReadyRead()
{
    if (!m_socket)
        return;

    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        const QByteArray payload = datagram.data().trimmed();
        if (payload.isEmpty())
            continue;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            continue;

        const QJsonObject message = document.object();
        if (message.value(QStringLiteral("app")).toString() != QStringLiteral("LanBoard"))
            continue;
        if (message.value(QStringLiteral("version")).toInt() != 1)
            continue;

        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("discover_room")) {
            if (m_active)
                sendRoomAnnouncement(datagram.senderAddress(), datagram.senderPort());
        } else if (type == QStringLiteral("room_announce")) {
            processAnnouncement(message, datagram.senderAddress());
        }
    }
}

void RoomDiscoveryService::processAnnouncement(const QJsonObject &message,
                                               const QHostAddress &senderAddress,
                                               qint64 nowMs)
{
    const int portValue = message.value(QStringLiteral("port")).toInt();
    if (portValue < 1 || portValue > 65535)
        return;
    const quint16 port = static_cast<quint16>(portValue);

    const QHostAddress announcedAddress(message.value(QStringLiteral("hostIp")).toString().trimmed());
    const QHostAddress address = NetworkAddressUtils::isUsableIpv4(announcedAddress)
        ? announcedAddress
        : senderAddress;
    if (!NetworkAddressUtils::isUsableIpv4(address))
        return;

    const QString roomUid = message.value(QStringLiteral("roomUid")).toString().trimmed();
    if (roomUid.isEmpty() || roomUid == m_roomUid)
        return;

    const qint64 timestamp = nowMs >= 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();

    int roomIndex = roomIndexForUid(roomUid);
    if (roomIndex < 0) {
        RoomEntry room;
        m_rooms.append(room);
        roomIndex = m_rooms.size() - 1;
    }

    RoomEntry &room = m_rooms[roomIndex];
    room.roomUid = roomUid;
    updateRoomMetadata(room, message);
    upsertEndpoint(room, address, port, timestamp);

    rebuildDiscoveredRooms();
}

void RoomDiscoveryService::pruneExpired(qint64 nowMs)
{
    const qint64 timestamp = nowMs >= 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();
    bool changed = false;

    for (int roomIndex = m_rooms.size() - 1; roomIndex >= 0; --roomIndex) {
        RoomEntry &room = m_rooms[roomIndex];
        for (int endpointIndex = room.endpoints.size() - 1; endpointIndex >= 0; --endpointIndex) {
            const Endpoint &endpoint = room.endpoints.at(endpointIndex);
            if (timestamp - endpoint.lastSeenMs <= DiscoveryStaleMs)
                continue;
            const QString removedKey = endpointKey(endpoint.address, endpoint.port);
            room.endpoints.removeAt(endpointIndex);
            if (room.preferredEndpointKey == removedKey)
                room.preferredEndpointKey.clear();
            changed = true;
        }
        if (!room.endpoints.isEmpty())
            continue;
        m_rooms.removeAt(roomIndex);
        changed = true;
    }

    if (changed)
        rebuildDiscoveredRooms();
}

void RoomDiscoveryService::broadcastDiscoveryQuery()
{
    if (!m_socket)
        return;

    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("discover_room");
    message[QStringLiteral("app")] = QStringLiteral("LanBoard");
    message[QStringLiteral("version")] = 1;
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);

    QSet<QString> sentBroadcasts;
    for (const auto &entry : NetworkAddressUtils::localIpv4Addresses()) {
        if (entry.broadcast.isNull())
            continue;
        const QString key = entry.broadcast.toString();
        if (sentBroadcasts.contains(key))
            continue;
        sentBroadcasts.insert(key);
        m_socket->writeDatagram(payload, entry.broadcast, DiscoveryPort);
    }
    if (sentBroadcasts.isEmpty())
        m_socket->writeDatagram(payload, QHostAddress::Broadcast, DiscoveryPort);
}

void RoomDiscoveryService::broadcastPublishedRoom()
{
    if (!m_active)
        return;

    QUdpSocket socket;
    socket.setProxy(QNetworkProxy::NoProxy);
    QSet<QString> sentEndpoints;
    for (const auto &entry : NetworkAddressUtils::localIpv4Addresses()) {
        if (entry.broadcast.isNull())
            continue;
        const QString key = entry.address.toString() + QLatin1Char('|') + entry.broadcast.toString();
        if (sentEndpoints.contains(key))
            continue;
        sentEndpoints.insert(key);
        const QByteArray payload = QJsonDocument(roomAnnouncementMessage(entry.address.toString()))
                                       .toJson(QJsonDocument::Compact);
        socket.writeDatagram(payload, entry.broadcast, DiscoveryPort);
    }

    if (sentEndpoints.isEmpty()) {
        const QByteArray payload = QJsonDocument(roomAnnouncementMessage(
            NetworkAddressUtils::bestLocalIpv4())).toJson(QJsonDocument::Compact);
        socket.writeDatagram(payload, QHostAddress::Broadcast, DiscoveryPort);
    }

    const QByteArray loopbackPayload = QJsonDocument(roomAnnouncementMessage(
        NetworkAddressUtils::bestLocalIpv4())).toJson(QJsonDocument::Compact);
    socket.writeDatagram(loopbackPayload, QHostAddress::LocalHost, DiscoveryPort);
}

bool RoomDiscoveryService::ensureSocket()
{
    if (m_socket)
        return true;

    m_socket = new QUdpSocket(this);
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QUdpSocket::readyRead, this, &RoomDiscoveryService::onReadyRead);
    const bool bound = m_socket->bind(QHostAddress::Any,
                                      DiscoveryPort,
                                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (bound)
        return true;

    emit errorOccurred(QStringLiteral("无法启动局域网发现: %1").arg(m_socket->errorString()));
    m_socket->deleteLater();
    m_socket = nullptr;
    return false;
}

QJsonObject RoomDiscoveryService::roomAnnouncementMessage(const QString &hostIp) const
{
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("room_announce");
    message[QStringLiteral("app")] = QStringLiteral("LanBoard");
    message[QStringLiteral("version")] = 1;
    message[QStringLiteral("roomUid")] = m_roomUid;
    message[QStringLiteral("hostName")] = m_hostName.isEmpty() ? QStringLiteral("host") : m_hostName;
    message[QStringLiteral("hostIp")] = hostIp;
    message[QStringLiteral("port")] = static_cast<int>(m_port);
    message[QStringLiteral("playerCount")] = m_playerCount;
    message[QStringLiteral("roomCapacity")] = m_roomCapacity;
    message[QStringLiteral("maxPlayers")] = m_maxPlayers;
    message[QStringLiteral("gameId")] = m_gameId;
    message[QStringLiteral("gameName")] = m_gameName;
    message[QStringLiteral("inGame")] = m_inGame;
    message[QStringLiteral("isFull")] = m_playerCount >= m_roomCapacity;
    return message;
}

void RoomDiscoveryService::sendRoomAnnouncement(const QHostAddress &address, quint16 port)
{
    if (!m_socket || !m_active || port == 0)
        return;
    const QByteArray payload = QJsonDocument(roomAnnouncementMessage(
        NetworkAddressUtils::localIpv4ForPeer(address))).toJson(QJsonDocument::Compact);
    m_socket->writeDatagram(payload, address, port);
}

QString RoomDiscoveryService::endpointKey(const QHostAddress &address, quint16 port) const
{
    return address.toString() + QLatin1Char(':') + QString::number(port);
}

int RoomDiscoveryService::roomIndexForUid(const QString &roomUid) const
{
    if (roomUid.isEmpty())
        return -1;
    for (int i = 0; i < m_rooms.size(); ++i) {
        if (m_rooms.at(i).roomUid == roomUid)
            return i;
    }
    return -1;
}

void RoomDiscoveryService::updateRoomMetadata(RoomEntry &room, const QJsonObject &message)
{
    room.hostName = message.value(QStringLiteral("hostName")).toString().trimmed();
    room.playerCount = qMax(0, message.value(QStringLiteral("playerCount")).toInt());
    room.roomCapacity = qMax(2, message.value(QStringLiteral("roomCapacity")).toInt(room.playerCount));
    room.maxPlayers = qMax(2, message.value(QStringLiteral("maxPlayers")).toInt(2));
    room.gameId = LanBoard::normalizeGameId(
        message.value(QStringLiteral("gameId")).toString(QStringLiteral("gomoku")));
    room.gameName = message.value(QStringLiteral("gameName")).toString(LanBoard::gameName(room.gameId));
    room.inGame = message.value(QStringLiteral("inGame")).toBool();
    room.isFull = message.value(QStringLiteral("isFull")).toBool(
        room.playerCount >= room.roomCapacity);
}

void RoomDiscoveryService::upsertEndpoint(RoomEntry &room,
                                          const QHostAddress &address,
                                          quint16 port,
                                          qint64 nowMs)
{
    const QString key = endpointKey(address, port);
    const int preference = NetworkAddressUtils::endpointPreference(
        address, NetworkAddressUtils::localIpv4Addresses());

    const Endpoint *current = preferredEndpoint(room);
    const QString currentKey = current
        ? endpointKey(current->address, current->port)
        : QString();

    Endpoint *updatedEndpoint = nullptr;
    for (Endpoint &endpoint : room.endpoints) {
        if (endpointKey(endpoint.address, endpoint.port) != key)
            continue;
        endpoint.lastSeenMs = nowMs;
        endpoint.preference = preference;
        updatedEndpoint = &endpoint;
        break;
    }
    if (!updatedEndpoint) {
        Endpoint endpoint;
        endpoint.address = address;
        endpoint.port = port;
        endpoint.lastSeenMs = nowMs;
        endpoint.preference = preference;
        room.endpoints.append(endpoint);
        updatedEndpoint = &room.endpoints.last();
    }

    const Endpoint *currentAfterUpdate = nullptr;
    for (const Endpoint &endpoint : room.endpoints) {
        if (endpointKey(endpoint.address, endpoint.port) == currentKey) {
            currentAfterUpdate = &endpoint;
            break;
        }
    }
    if (!currentAfterUpdate || updatedEndpoint->preference > currentAfterUpdate->preference)
        room.preferredEndpointKey = key;
    else
        room.preferredEndpointKey = currentKey;
}

const RoomDiscoveryService::Endpoint *RoomDiscoveryService::preferredEndpoint(
    const RoomEntry &room) const
{
    const Endpoint *best = nullptr;
    for (const Endpoint &endpoint : room.endpoints) {
        const QString key = endpointKey(endpoint.address, endpoint.port);
        if (key == room.preferredEndpointKey)
            best = &endpoint;
    }
    if (best)
        return best;

    for (const Endpoint &endpoint : room.endpoints) {
        if (!best || endpoint.preference > best->preference)
            best = &endpoint;
    }
    return best;
}

qint64 RoomDiscoveryService::roomLastSeen(const RoomEntry &room) const
{
    qint64 lastSeen = 0;
    for (const Endpoint &endpoint : room.endpoints)
        lastSeen = qMax(lastSeen, endpoint.lastSeenMs);
    return lastSeen;
}

QVariantMap RoomDiscoveryService::roomToVariant(const RoomEntry &room) const
{
    QVariantMap map;
    const Endpoint *endpoint = preferredEndpoint(room);
    map[QStringLiteral("roomUid")] = room.roomUid;
    map[QStringLiteral("hostName")] = room.hostName;
    map[QStringLiteral("hostIp")] = endpoint ? endpoint->address.toString() : QString();
    map[QStringLiteral("port")] = endpoint ? endpoint->port : 0;
    map[QStringLiteral("playerCount")] = room.playerCount;
    map[QStringLiteral("roomCapacity")] = room.roomCapacity;
    map[QStringLiteral("maxPlayers")] = room.maxPlayers;
    map[QStringLiteral("gameId")] = room.gameId;
    map[QStringLiteral("gameName")] = room.gameName;
    map[QStringLiteral("inGame")] = room.inGame;
    map[QStringLiteral("isFull")] = room.isFull;
    map[QStringLiteral("endpointCount")] = room.endpoints.size();
    return map;
}

void RoomDiscoveryService::rebuildDiscoveredRooms()
{
    std::sort(m_rooms.begin(), m_rooms.end(), [this](const RoomEntry &lhs, const RoomEntry &rhs) {
        if (lhs.isFull != rhs.isFull)
            return !lhs.isFull;
        if (lhs.inGame != rhs.inGame)
            return !lhs.inGame;
        return roomLastSeen(lhs) > roomLastSeen(rhs);
    });

    QVariantList rooms;
    rooms.reserve(m_rooms.size());
    for (const RoomEntry &room : m_rooms)
        rooms.append(roomToVariant(room));
    m_discoveredRooms = rooms;
    emit discoveredRoomsChanged();
}

#ifdef Q_OS_ANDROID
void RoomDiscoveryService::acquireMulticastLock()
{
    if (m_multicastLock.isValid())
        return;

    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return;
    const QJniObject wifiService = QJniObject::getStaticObjectField(
        "android/content/Context",
        "WIFI_SERVICE",
        "Ljava/lang/String;");
    const QJniObject wifiManager = context.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        wifiService.object<jstring>());
    if (!wifiManager.isValid())
        return;

    const QJniObject tag = QJniObject::fromString(QStringLiteral("LanBoardDiscovery"));
    m_multicastLock = wifiManager.callObjectMethod(
        "createMulticastLock",
        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
        tag.object<jstring>());
    if (!m_multicastLock.isValid())
        return;
    m_multicastLock.callMethod<void>("setReferenceCounted", "(Z)V", false);
    m_multicastLock.callMethod<void>("acquire");
}

void RoomDiscoveryService::releaseMulticastLock()
{
    if (!m_multicastLock.isValid())
        return;
    m_multicastLock.callMethod<void>("release");
    m_multicastLock = QJniObject();
}
#endif
