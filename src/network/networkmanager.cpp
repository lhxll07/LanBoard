#include "networkmanager.h"

#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkDatagram>
#include <QNetworkProxy>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <limits>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {

bool isUsableIpv4(const QHostAddress &address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
        return false;

    if (address == QHostAddress::LocalHost)
        return false;

    const QString ip = address.toString();
    return !ip.startsWith(QStringLiteral("169.254."));
}

int ipPreferenceScore(const QNetworkInterface &iface, const QHostAddress &address)
{
    int score = 0;
    const QString humanName = (iface.humanReadableName() + QLatin1Char(' ') + iface.name()).toLower();
    const QString ip = address.toString();

    if (ip.startsWith(QStringLiteral("192.168.")) || ip.startsWith(QStringLiteral("10.")))
        score += 40;
    else if (ip.startsWith(QStringLiteral("172.")))
        score += 30;
    else
        score += 10;

    if (humanName.contains(QStringLiteral("wlan"))
        || humanName.contains(QStringLiteral("wi-fi"))
        || humanName.contains(QStringLiteral("wifi"))) {
        score += 30;
    }

    if (humanName.contains(QStringLiteral("rmnet"))
        || humanName.contains(QStringLiteral("cell"))
        || humanName.contains(QStringLiteral("mobile"))
        || humanName.contains(QStringLiteral("tun"))
        || humanName.contains(QStringLiteral("tap"))
        || humanName.contains(QStringLiteral("vpn"))
        || humanName.contains(QStringLiteral("virtual"))) {
        score -= 20;
    }

    return score;
}

}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    m_discoveryTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_discoveryTimer, &QTimer::timeout, this, &NetworkManager::broadcastDiscoveryQuery);

    m_discoveryPruneTimer.setInterval(1000);
    connect(&m_discoveryPruneTimer, &QTimer::timeout, this, &NetworkManager::pruneDiscoveredRooms);
}

// ── Public API ──

void NetworkManager::startServer(quint16 port)
{
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_server = new QTcpServer(this);
    m_server->setProxy(QNetworkProxy::NoProxy);
    connect(m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);

    if (!m_server->listen(QHostAddress::AnyIPv4, port)) {
        emit errorOccurred(QStringLiteral("无法监听端口 %1: %2")
                               .arg(port)
                               .arg(m_server->errorString()));
        return;
    }
    m_isHost = true;
    m_nextPlayerId = 1;
    m_serverPort = port;
    emit connectionChanged();
    emit serverStarted(port);
}

void NetworkManager::connectToServer(const QString &ip, quint16 port,
                                     const QString &playerName)
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_socket = new QTcpSocket(this);
    m_socket->setProxy(QNetworkProxy::NoProxy);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);

    m_connectedIp = ip;
    m_connectedPort = port;
    m_socket->connectToHost(ip, port);

    // Send join message when connected (handled in onConnected)
    m_socket->setProperty("playerName", playerName);
}

void NetworkManager::disconnectAll()
{
    if (m_isHost) {
        for (auto *c : m_clients) {
            c->disconnect(this);
            c->disconnectFromHost();
            c->deleteLater();
        }
        m_clients.clear();
        if (m_server) {
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
        }
    }
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_isHost = false;
    m_discoveryGameInProgress = false;
    m_serverPort = 0;
    m_connectedIp.clear();
    m_connectedPort = 0;
    emit connectionChanged();
    emit clientCountChanged();
}

bool NetworkManager::isConnected() const
{
    if (m_isHost)
        return m_server && m_server->isListening();
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString NetworkManager::localIp() const
{
    QHostAddress bestAddress;
    int bestScore = std::numeric_limits<int>::min();

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (!isUsableIpv4(address))
                continue;

            const int score = ipPreferenceScore(iface, address);
            if (score <= bestScore)
                continue;

            bestScore = score;
            bestAddress = address;
        }
    }

    return bestAddress.isNull() ? QStringLiteral("127.0.0.1") : bestAddress.toString();
}

// ── Send actions (client → server) ──

void NetworkManager::sendReady(bool ready)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("ready");
    msg[QStringLiteral("ready")] = ready;
    sendJson(m_socket, msg);
}

void NetworkManager::sendPlacePiece(int row, int col)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("place_piece");
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    sendJson(m_socket, msg);
}

void NetworkManager::sendSurrender()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("surrender");
    sendJson(m_socket, msg);
}

void NetworkManager::sendStartGame()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("start_game");
    sendJson(m_socket, msg);
}

void NetworkManager::startRoomDiscovery()
{
    if (!ensureDiscoverySocket())
        return;

#ifdef Q_OS_ANDROID
    acquireMulticastLock();
#endif

    if (!m_discoveryPruneTimer.isActive())
        m_discoveryPruneTimer.start();

    if (!m_discoveryTimer.isActive())
        m_discoveryTimer.start();

    refreshRoomDiscovery();
}

void NetworkManager::stopRoomDiscovery()
{
    m_discoveryTimer.stop();
    m_discoveryPruneTimer.stop();
#ifdef Q_OS_ANDROID
    releaseMulticastLock();
#endif
}

void NetworkManager::refreshRoomDiscovery()
{
    if (!ensureDiscoverySocket())
        return;

    pruneDiscoveredRooms();
    broadcastDiscoveryQuery();
}

void NetworkManager::clearDiscoveredRooms()
{
    if (m_discoveredRoomEntries.isEmpty())
        return;

    m_discoveredRoomEntries.clear();
    rebuildDiscoveredRooms();
}

void NetworkManager::setDiscoveryHostName(const QString &hostName)
{
    m_discoveryHostName = hostName.trimmed();
}

void NetworkManager::setDiscoveryGameInProgress(bool inProgress)
{
    m_discoveryGameInProgress = inProgress;
}

// ── Broadcast from host ──

void NetworkManager::broadcastRoomState(const QJsonArray &players)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_state");
    msg[QStringLiteral("players")] = players;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameStarted()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_start");
    broadcastJson(msg);
}

void NetworkManager::broadcastMove(int player, int row, int col)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("move");
    msg[QStringLiteral("player")] = player;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameOver(int winner)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_over");
    msg[QStringLiteral("winner")] = winner;
    broadcastJson(msg);
}

// ── Slots ──

void NetworkManager::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *client = m_server->nextPendingConnection();
        connect(client, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
        connect(client, &QTcpSocket::disconnected, this, &NetworkManager::onClientDisconnected);
        m_clients.append(client);

        // Assign a player ID
        int playerId = m_nextPlayerId++;
        client->setProperty("playerId", playerId);

        emit clientCountChanged();
    }
}

void NetworkManager::onReadyRead()
{
    QTcpSocket *sender = qobject_cast<QTcpSocket *>(QObject::sender());
    if (!sender)
        return;

    // Per-socket read buffer (stored as property)
    QByteArray buf = sender->property("readBuffer").toByteArray();
    buf.append(sender->readAll());

    // Process complete messages (newline-delimited JSON)
    while (true) {
        int idx = buf.indexOf('\n');
        if (idx < 0)
            break;

        QByteArray line = buf.left(idx).trimmed();
        buf.remove(0, idx + 1);

        if (line.isEmpty())
            continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << err.errorString();
            continue;
        }
        if (!doc.isObject())
            continue;

        processMessage(sender, doc.object());
    }

    // Save remaining partial data
    sender->setProperty("readBuffer", buf);
}

void NetworkManager::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(QObject::sender());
    if (client) {
        const int playerId = client->property("playerId").toInt();
        m_clients.removeOne(client);
        client->deleteLater();
        emit clientDisconnected(playerId);
        emit clientCountChanged();
    }
}

void NetworkManager::onConnected()
{
    // Send join message with player name
    QString name = m_socket->property("playerName").toString();
    if (name.isEmpty())
        name = QStringLiteral("player");

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("join");
    msg[QStringLiteral("name")] = name;
    sendJson(m_socket, msg);

    emit connectionChanged();
}

void NetworkManager::onDisconnected()
{
    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_connectedIp.clear();
    m_connectedPort = 0;
    emit connectionChanged();
}

void NetworkManager::onError(QAbstractSocket::SocketError)
{
    if (m_socket)
        emit errorOccurred(m_socket->errorString());
}

void NetworkManager::onDiscoveryReadyRead()
{
    if (!m_discoverySocket)
        return;

    while (m_discoverySocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_discoverySocket->receiveDatagram();
        const QByteArray payload = datagram.data().trimmed();
        if (payload.isEmpty())
            continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject msg = doc.object();
        if (msg.value(QStringLiteral("app")).toString() != QStringLiteral("LanBoard"))
            continue;
        if (msg.value(QStringLiteral("version")).toInt() != 1)
            continue;

        const QString type = msg.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("discover_room")) {
            if (m_isHost && m_server && m_server->isListening())
                sendRoomAnnouncement(datagram.senderAddress(), datagram.senderPort());
        } else if (type == QStringLiteral("room_announce")) {
            upsertDiscoveredRoom(msg, datagram.senderAddress());
        }
    }
}

void NetworkManager::pruneDiscoveredRooms()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;

    for (int i = m_discoveredRoomEntries.size() - 1; i >= 0; --i) {
        if (now - m_discoveredRoomEntries.at(i).lastSeenMs <= DiscoveryStaleMs)
            continue;

        m_discoveredRoomEntries.removeAt(i);
        changed = true;
    }

    if (changed)
        rebuildDiscoveredRooms();
}

void NetworkManager::broadcastDiscoveryQuery()
{
    if (!m_discoverySocket)
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

            m_discoverySocket->writeDatagram(payload, entry.broadcast(), DiscoveryPort);
            sent = true;
        }
    }

    if (!sent)
        m_discoverySocket->writeDatagram(payload, QHostAddress::Broadcast, DiscoveryPort);
}

// ── Private ──

void NetworkManager::sendJson(QTcpSocket *socket, const QJsonObject &obj)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    socket->write(data);
    socket->flush();
}

void NetworkManager::broadcastJson(const QJsonObject &obj, QTcpSocket *exclude)
{
    for (QTcpSocket *client : m_clients) {
        if (client != exclude)
            sendJson(client, obj);
    }
}

void NetworkManager::processMessage(QTcpSocket *sender, const QJsonObject &msg)
{
    QString type = msg.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("join")) {
        QString name = msg.value(QStringLiteral("name")).toString();
        sender->setProperty("playerName", name);
        emit joinRequested(name, sender);
    }
    else if (type == QStringLiteral("ready")) {
        int playerId = sender->property("playerId").toInt();
        bool ready = msg.value(QStringLiteral("ready")).toBool();
        emit remoteReadyChanged(playerId, ready);
    }
    else if (type == QStringLiteral("place_piece")) {
        int playerId = sender->property("playerId").toInt();
        int row = msg.value(QStringLiteral("row")).toInt();
        int col = msg.value(QStringLiteral("col")).toInt();
        emit remoteMoveReceived(playerId, row, col);
    }
    else if (type == QStringLiteral("surrender")) {
        int playerId = sender->property("playerId").toInt();
        emit remoteSurrender(playerId);
    }
    else if (type == QStringLiteral("move")) {
        int player = msg.value(QStringLiteral("player")).toInt();
        int row = msg.value(QStringLiteral("row")).toInt();
        int col = msg.value(QStringLiteral("col")).toInt();
        emit remoteMoveReceived(player, row, col);
    }
    else if (type == QStringLiteral("game_over")) {
        int winner = msg.value(QStringLiteral("winner")).toInt();
        emit gameOverReceived(winner);
    }
    else if (type == QStringLiteral("room_state")) {
        emit roomStateReceived(msg);
    }
    else if (type == QStringLiteral("game_start")) {
        emit remoteStartGame();
    }
    else if (type == QStringLiteral("start_game")) {
        // Dedicated server accepts this. LAN host currently ignores it.
    }
}

bool NetworkManager::ensureDiscoverySocket()
{
    if (m_discoverySocket)
        return true;

    m_discoverySocket = new QUdpSocket(this);
    m_discoverySocket->setProxy(QNetworkProxy::NoProxy);
    connect(m_discoverySocket, &QUdpSocket::readyRead, this, &NetworkManager::onDiscoveryReadyRead);

    const bool bound = m_discoverySocket->bind(QHostAddress::AnyIPv4,
                                               DiscoveryPort,
                                               QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        emit errorOccurred(QStringLiteral("无法启动局域网发现: %1")
                               .arg(m_discoverySocket->errorString()));
        m_discoverySocket->deleteLater();
        m_discoverySocket = nullptr;
        return false;
    }

    return true;
}

#ifdef Q_OS_ANDROID
void NetworkManager::acquireMulticastLock()
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

void NetworkManager::releaseMulticastLock()
{
    if (!m_multicastLock.isValid())
        return;

    m_multicastLock.callMethod<void>("release");
    m_multicastLock = QJniObject();
}
#endif

QString NetworkManager::localIpForPeer(const QHostAddress &peer) const
{
    QHostAddress fallbackAddress;
    int fallbackScore = std::numeric_limits<int>::min();

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!isUsableIpv4(ip))
                continue;

            if (peer.isInSubnet(ip, entry.prefixLength()))
                return ip.toString();

            const int score = ipPreferenceScore(iface, ip);
            if (score <= fallbackScore)
                continue;

            fallbackScore = score;
            fallbackAddress = ip;
        }
    }

    return fallbackAddress.isNull() ? localIp() : fallbackAddress.toString();
}

void NetworkManager::sendRoomAnnouncement(const QHostAddress &address, quint16 port)
{
    if (!m_discoverySocket || port == 0)
        return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_announce");
    msg[QStringLiteral("app")] = QStringLiteral("LanBoard");
    msg[QStringLiteral("version")] = 1;
    msg[QStringLiteral("hostName")] = m_discoveryHostName.isEmpty()
        ? QStringLiteral("host")
        : m_discoveryHostName;
    msg[QStringLiteral("hostIp")] = localIpForPeer(address);
    msg[QStringLiteral("port")] = static_cast<int>(m_serverPort);
    msg[QStringLiteral("playerCount")] = 1 + m_clients.size();
    msg[QStringLiteral("maxPlayers")] = 2;
    msg[QStringLiteral("inGame")] = m_discoveryGameInProgress;
    msg[QStringLiteral("isFull")] = (1 + m_clients.size()) >= 2;

    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_discoverySocket->writeDatagram(payload, address, port);
}

void NetworkManager::upsertDiscoveredRoom(const QJsonObject &msg, const QHostAddress &senderAddress)
{
    const quint16 port = static_cast<quint16>(msg.value(QStringLiteral("port")).toInt());
    if (port == 0)
        return;

    const QString announcedIp = msg.value(QStringLiteral("hostIp")).toString().trimmed();
    const QHostAddress announcedAddress(announcedIp);
    const QString roomIp = isUsableIpv4(announcedAddress)
        ? announcedAddress.toString()
        : senderAddress.toString();

    DiscoveredRoom room;
    room.hostName = msg.value(QStringLiteral("hostName")).toString().trimmed();
    room.hostIp = roomIp;
    room.port = port;
    room.playerCount = msg.value(QStringLiteral("playerCount")).toInt();
    room.maxPlayers = qMax(2, msg.value(QStringLiteral("maxPlayers")).toInt(2));
    room.inGame = msg.value(QStringLiteral("inGame")).toBool();
    room.isFull = msg.value(QStringLiteral("isFull")).toBool(room.playerCount >= room.maxPlayers);
    room.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

    if (room.hostIp == localIp() && room.port == m_serverPort)
        return;

    bool changed = false;
    for (DiscoveredRoom &existing : m_discoveredRoomEntries) {
        if (existing.hostIp != room.hostIp || existing.port != room.port)
            continue;

        existing = room;
        changed = true;
        break;
    }

    if (!changed) {
        m_discoveredRoomEntries.append(room);
        changed = true;
    }

    if (changed)
        rebuildDiscoveredRooms();
}

QVariantMap NetworkManager::discoveredRoomToVariant(const DiscoveredRoom &room) const
{
    QVariantMap map;
    map[QStringLiteral("hostName")] = room.hostName;
    map[QStringLiteral("hostIp")] = room.hostIp;
    map[QStringLiteral("port")] = room.port;
    map[QStringLiteral("playerCount")] = room.playerCount;
    map[QStringLiteral("maxPlayers")] = room.maxPlayers;
    map[QStringLiteral("inGame")] = room.inGame;
    map[QStringLiteral("isFull")] = room.isFull;
    return map;
}

void NetworkManager::rebuildDiscoveredRooms()
{
    std::sort(m_discoveredRoomEntries.begin(), m_discoveredRoomEntries.end(),
              [](const DiscoveredRoom &lhs, const DiscoveredRoom &rhs) {
        if (lhs.isFull != rhs.isFull)
            return !lhs.isFull;
        if (lhs.inGame != rhs.inGame)
            return !lhs.inGame;
        return lhs.lastSeenMs > rhs.lastSeenMs;
    });

    QVariantList rooms;
    rooms.reserve(m_discoveredRoomEntries.size());
    for (const DiscoveredRoom &room : m_discoveredRoomEntries)
        rooms.append(discoveredRoomToVariant(room));

    m_discoveredRooms = rooms;
    emit discoveredRoomsChanged();
}
