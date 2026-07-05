#include "roomdiscoveryservice.h"

#include "networkaddressutils.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAddressEntry>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {

QString normalizedGameId(const QString &gameId)
{
    if (gameId == QStringLiteral("doudizhu"))
        return QStringLiteral("doudizhu");
    if (gameId == QStringLiteral("flightchess"))
        return QStringLiteral("flightchess");
    return QStringLiteral("gomoku");
}

QString defaultGameName(const QString &gameId)
{
    if (gameId == QStringLiteral("doudizhu"))
        return QStringLiteral("斗地主");
    if (gameId == QStringLiteral("flightchess"))
        return QStringLiteral("飞行棋");
    return QStringLiteral("五子棋");
}

} // namespace

RoomDiscoveryService::RoomDiscoveryService(QObject *parent)
    : QObject(parent)
    , m_roomUid(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    m_discoveryTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_discoveryTimer, &QTimer::timeout, this, &RoomDiscoveryService::broadcastDiscoveryQuery);

    m_pruneTimer.setInterval(1000);
    connect(&m_pruneTimer, &QTimer::timeout, this, &RoomDiscoveryService::pruneDiscoveredRooms);

    m_publishTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_publishTimer, &QTimer::timeout, this, &RoomDiscoveryService::broadcastPublishedRoom);
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
    m_gameId = normalizedGameId(gameId);
    m_gameName = gameName.trimmed().isEmpty() ? defaultGameName(m_gameId) : gameName.trimmed();
    m_inGame = inGame;
    m_active = active && m_port != 0;

    if (m_active) {
        if (!ensureSocket())
            return;

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
    if (wasActive && !m_discoveryTimer.isActive())
        releaseMulticastLock();
#endif
}

void RoomDiscoveryService::start()
{
    if (!ensureSocket())
        return;

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

    pruneDiscoveredRooms();
    broadcastDiscoveryQuery();
}

void RoomDiscoveryService::clear()
{
    if (m_roomEntries.isEmpty())
        return;

    m_roomEntries.clear();
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

        const QJsonObject msg = document.object();
        if (msg.value(QStringLiteral("app")).toString() != QStringLiteral("LanBoard"))
            continue;
        if (msg.value(QStringLiteral("version")).toInt() != 1)
            continue;

        const QString type = msg.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("discover_room")) {
            if (m_active)
                sendRoomAnnouncement(datagram.senderAddress(), datagram.senderPort());
        } else if (type == QStringLiteral("room_announce")) {
            upsertDiscoveredRoom(msg, datagram.senderAddress());
        }
    }
}

void RoomDiscoveryService::pruneDiscoveredRooms()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;

    for (int i = m_roomEntries.size() - 1; i >= 0; --i) {
        if (now - m_roomEntries.at(i).lastSeenMs <= DiscoveryStaleMs)
            continue;

        m_roomEntries.removeAt(i);
        changed = true;
    }

    if (changed)
        rebuildDiscoveredRooms();
}

void RoomDiscoveryService::broadcastDiscoveryQuery()
{
    if (!m_socket)
        return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("discover_room");
    msg[QStringLiteral("app")] = QStringLiteral("LanBoard");
    msg[QStringLiteral("version")] = 1;

    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    bool sent = false;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            if (entry.broadcast().isNull())
                continue;

            m_socket->writeDatagram(payload, entry.broadcast(), DiscoveryPort);
            sent = true;
        }
    }

    if (!sent)
        m_socket->writeDatagram(payload, QHostAddress::Broadcast, DiscoveryPort);
}

void RoomDiscoveryService::broadcastPublishedRoom()
{
    if (!m_active)
        return;

    QUdpSocket socket;
    socket.setProxy(QNetworkProxy::NoProxy);

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_announce");
    msg[QStringLiteral("app")] = QStringLiteral("LanBoard");
    msg[QStringLiteral("version")] = 1;
    msg[QStringLiteral("roomUid")] = m_roomUid;
    msg[QStringLiteral("hostName")] = m_hostName.isEmpty() ? QStringLiteral("host") : m_hostName;
    msg[QStringLiteral("port")] = static_cast<int>(m_port);
    msg[QStringLiteral("playerCount")] = m_playerCount;
    msg[QStringLiteral("roomCapacity")] = m_roomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_maxPlayers;
    msg[QStringLiteral("gameId")] = m_gameId;
    msg[QStringLiteral("gameName")] = m_gameName;
    msg[QStringLiteral("inGame")] = m_inGame;
    msg[QStringLiteral("isFull")] = m_playerCount >= m_roomCapacity;

    bool sent = false;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!NetworkAddressUtils::isUsableIpv4(ip) || entry.broadcast().isNull())
                continue;

            QJsonObject scopedMsg = msg;
            scopedMsg[QStringLiteral("hostIp")] = ip.toString();
            const QByteArray scopedPayload = QJsonDocument(scopedMsg).toJson(QJsonDocument::Compact);
            socket.writeDatagram(scopedPayload, entry.broadcast(), DiscoveryPort);
            sent = true;
        }
    }

    if (!sent) {
        msg[QStringLiteral("hostIp")] = NetworkAddressUtils::bestLocalIpv4();
        socket.writeDatagram(QJsonDocument(msg).toJson(QJsonDocument::Compact),
                             QHostAddress::Broadcast,
                             DiscoveryPort);
    }
}

bool RoomDiscoveryService::ensureSocket()
{
    if (m_socket)
        return true;

    m_socket = new QUdpSocket(this);
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QUdpSocket::readyRead, this, &RoomDiscoveryService::onReadyRead);

    const bool bound = m_socket->bind(QHostAddress::AnyIPv4,
                                      DiscoveryPort,
                                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        emit errorOccurred(QStringLiteral("无法启动局域网发现: %1").arg(m_socket->errorString()));
        m_socket->deleteLater();
        m_socket = nullptr;
        return false;
    }

    return true;
}

void RoomDiscoveryService::sendRoomAnnouncement(const QHostAddress &address, quint16 port)
{
    if (!m_socket || port == 0 || !m_active)
        return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_announce");
    msg[QStringLiteral("app")] = QStringLiteral("LanBoard");
    msg[QStringLiteral("version")] = 1;
    msg[QStringLiteral("roomUid")] = m_roomUid;
    msg[QStringLiteral("hostName")] = m_hostName.isEmpty() ? QStringLiteral("host") : m_hostName;
    msg[QStringLiteral("hostIp")] = NetworkAddressUtils::localIpv4ForPeer(address);
    msg[QStringLiteral("port")] = static_cast<int>(m_port);
    msg[QStringLiteral("playerCount")] = m_playerCount;
    msg[QStringLiteral("roomCapacity")] = m_roomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_maxPlayers;
    msg[QStringLiteral("gameId")] = m_gameId;
    msg[QStringLiteral("gameName")] = m_gameName;
    msg[QStringLiteral("inGame")] = m_inGame;
    msg[QStringLiteral("isFull")] = m_playerCount >= m_roomCapacity;

    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->writeDatagram(payload, address, port);
}

void RoomDiscoveryService::upsertDiscoveredRoom(const QJsonObject &msg,
                                                const QHostAddress &senderAddress)
{
    const quint16 port = static_cast<quint16>(msg.value(QStringLiteral("port")).toInt());
    if (port == 0)
        return;

    const QString announcedIp = msg.value(QStringLiteral("hostIp")).toString().trimmed();
    const QHostAddress announcedAddress(announcedIp);
    const QString roomIp = NetworkAddressUtils::isUsableIpv4(announcedAddress)
        ? announcedAddress.toString()
        : senderAddress.toString();

    if (m_active && roomIp == NetworkAddressUtils::bestLocalIpv4() && port == m_port)
        return;

    DiscoveredRoom room;
    room.roomUid = msg.value(QStringLiteral("roomUid")).toString().trimmed();
    room.hostName = msg.value(QStringLiteral("hostName")).toString().trimmed();
    room.hostIp = roomIp;
    room.port = port;
    room.playerCount = msg.value(QStringLiteral("playerCount")).toInt();
    room.roomCapacity = qMax(2, msg.value(QStringLiteral("roomCapacity")).toInt(room.playerCount));
    room.maxPlayers = qMax(2, msg.value(QStringLiteral("maxPlayers")).toInt(2));
    room.gameId = normalizedGameId(msg.value(QStringLiteral("gameId")).toString(QStringLiteral("gomoku")));
    room.gameName = msg.value(QStringLiteral("gameName")).toString(defaultGameName(room.gameId));
    room.inGame = msg.value(QStringLiteral("inGame")).toBool();
    room.isFull = msg.value(QStringLiteral("isFull")).toBool(room.playerCount >= room.roomCapacity);
    room.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

    bool changed = false;
    for (DiscoveredRoom &existing : m_roomEntries) {
        const bool sameRoomUid = !room.roomUid.isEmpty()
            && !existing.roomUid.isEmpty()
            && existing.roomUid == room.roomUid;
        const bool sameLegacyEndpoint = existing.hostIp == room.hostIp
            && existing.port == room.port;
        if (!sameRoomUid && !sameLegacyEndpoint)
            continue;

        existing = room;
        changed = true;
        break;
    }

    if (!changed) {
        m_roomEntries.append(room);
        changed = true;
    }

    if (changed)
        rebuildDiscoveredRooms();
}

QVariantMap RoomDiscoveryService::discoveredRoomToVariant(const DiscoveredRoom &room) const
{
    QVariantMap map;
    map[QStringLiteral("hostName")] = room.hostName;
    map[QStringLiteral("hostIp")] = room.hostIp;
    map[QStringLiteral("port")] = room.port;
    map[QStringLiteral("playerCount")] = room.playerCount;
    map[QStringLiteral("roomCapacity")] = room.roomCapacity;
    map[QStringLiteral("maxPlayers")] = room.maxPlayers;
    map[QStringLiteral("gameId")] = room.gameId;
    map[QStringLiteral("gameName")] = room.gameName;
    map[QStringLiteral("inGame")] = room.inGame;
    map[QStringLiteral("isFull")] = room.isFull;
    return map;
}

void RoomDiscoveryService::rebuildDiscoveredRooms()
{
    std::sort(m_roomEntries.begin(), m_roomEntries.end(),
              [](const DiscoveredRoom &lhs, const DiscoveredRoom &rhs) {
        if (lhs.isFull != rhs.isFull)
            return !lhs.isFull;
        if (lhs.inGame != rhs.inGame)
            return !lhs.inGame;
        return lhs.lastSeenMs > rhs.lastSeenMs;
    });

    QVariantList rooms;
    rooms.reserve(m_roomEntries.size());
    for (const DiscoveredRoom &room : m_roomEntries)
        rooms.append(discoveredRoomToVariant(room));

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
        "android/content/Context", "WIFI_SERVICE", "Ljava/lang/String;");
    if (!wifiService.isValid())
        return;

    const QJniObject wifiManager = context.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        wifiService.object<jstring>());
    if (!wifiManager.isValid())
        return;

    m_multicastLock = wifiManager.callObjectMethod(
        "createMulticastLock",
        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
        QJniObject::fromString(QStringLiteral("LanBoardDiscovery")).object<jstring>());
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
