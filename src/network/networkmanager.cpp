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
#include <QUuid>
#include <algorithm>
#include <limits>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
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
    , m_discoveryRoomUid(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    m_discoveryTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_discoveryTimer, &QTimer::timeout, this, &NetworkManager::broadcastDiscoveryQuery);

    m_discoveryPruneTimer.setInterval(1000);
    connect(&m_discoveryPruneTimer, &QTimer::timeout, this, &NetworkManager::pruneDiscoveredRooms);

    m_hostAnnouncementTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_hostAnnouncementTimer, &QTimer::timeout,
            this, &NetworkManager::broadcastHostedRoomAnnouncement);
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
    if (!m_hostAnnouncementTimer.isActive())
        m_hostAnnouncementTimer.start();
    broadcastHostedRoomAnnouncement();
    emit connectionChanged();
    emit serverStarted(port);
}

void NetworkManager::connectToServer(const QString &ip, quint16 port,
                                     const QString &playerName,
                                     const QString &gameId)
{
    connectClientSocket(ip,
                        port,
                        playerName,
                        gameId,
                        QStringLiteral("legacy_join"));
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
    m_hostAnnouncementTimer.stop();
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

void NetworkManager::sendFlightRoll()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_roll");
    sendJson(m_socket, msg);
}

void NetworkManager::sendFlightMove(int planeIndex)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_move");
    msg[QStringLiteral("planeIndex")] = planeIndex;
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

void NetworkManager::sendDouDiZhuPlay(const QVariantList &cardIds)
{
    QJsonArray cards;
    for (const auto &id : cardIds)
        cards.append(id.toInt());

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("ddz_play");
    msg[QStringLiteral("cards")] = cards;
    sendJson(m_socket, msg);
}

void NetworkManager::sendDouDiZhuPass()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("ddz_pass");
    sendJson(m_socket, msg);
}

void NetworkManager::sendChangeSeat(const QString &seatType)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("change_seat");
    msg[QStringLiteral("seatType")] = seatType == QStringLiteral("spectator")
        ? QStringLiteral("spectator")
        : QStringLiteral("active");
    sendJson(m_socket, msg);
}

void NetworkManager::startRoomDiscovery()
{
    if (!ensureDiscoverySocket())
        return;

    if (!m_discoveryPruneTimer.isActive())
        m_discoveryPruneTimer.start();

    if (!m_discoveryTimer.isActive())
        m_discoveryTimer.start();

#ifdef Q_OS_ANDROID
    acquireMulticastLock();
#endif
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

void NetworkManager::requestOnlineRooms(const QString &host, quint16 port)
{
    if (host.trimmed().isEmpty() || port == 0)
        return;

    if (m_onlineLobbySocket) {
        m_onlineLobbySocket->disconnect(this);
        m_onlineLobbySocket->disconnectFromHost();
        m_onlineLobbySocket->deleteLater();
        m_onlineLobbySocket = nullptr;
    }

    m_onlineLobbySocket = new QTcpSocket(this);
    m_onlineLobbySocket->setProxy(QNetworkProxy::NoProxy);
    connect(m_onlineLobbySocket, &QTcpSocket::connected, this, &NetworkManager::onOnlineLobbyConnected);
    connect(m_onlineLobbySocket, &QTcpSocket::readyRead, this, &NetworkManager::onOnlineLobbyReadyRead);
    connect(m_onlineLobbySocket, &QTcpSocket::disconnected, this, &NetworkManager::onOnlineLobbyDisconnected);
    connect(m_onlineLobbySocket, &QTcpSocket::errorOccurred, this, &NetworkManager::onOnlineLobbyError);
    m_onlineLobbySocket->connectToHost(host.trimmed(), port);
}

void NetworkManager::clearOnlineRooms()
{
    if (m_onlineRooms.isEmpty())
        return;

    m_onlineRooms.clear();
    emit onlineRoomsChanged();
}

void NetworkManager::createOnlineRoom(const QString &host, quint16 port,
                                      const QString &playerName, const QString &gameId,
                                      const QString &roomName)
{
    connectClientSocket(host,
                        port,
                        playerName,
                        gameId,
                        QStringLiteral("online_create_room"),
                        QString(),
                        roomName);
}

void NetworkManager::joinOnlineRoom(const QString &host, quint16 port,
                                    const QString &playerName, const QString &roomId)
{
    connectClientSocket(host,
                        port,
                        playerName,
                        QString(),
                        QStringLiteral("online_join_room"),
                        roomId);
}

void NetworkManager::sendSwitchRoomGame(const QString &gameId)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("switch_room_game");
    msg[QStringLiteral("gameId")] = normalizedGameId(gameId);
    sendJson(m_socket, msg);
}

void NetworkManager::setDiscoveryHostName(const QString &hostName)
{
    m_discoveryHostName = hostName.trimmed();
    if (m_isHost)
        broadcastHostedRoomAnnouncement();
}

void NetworkManager::setDiscoveryGameInProgress(bool inProgress)
{
    m_discoveryGameInProgress = inProgress;
    if (m_isHost)
        broadcastHostedRoomAnnouncement();
}

void NetworkManager::setDiscoveryRoomInfo(const QString &gameId, const QString &gameName,
                                          int roomCapacity, int maxPlayers)
{
    m_discoveryGameId = normalizedGameId(gameId);
    m_discoveryGameName = gameName.trimmed().isEmpty()
        ? defaultGameName(m_discoveryGameId)
        : gameName.trimmed();
    m_discoveryRoomCapacity = qMax(2, roomCapacity);
    m_discoveryMaxPlayers = qMax(2, maxPlayers);
    if (m_isHost)
        broadcastHostedRoomAnnouncement();
}

// ── Broadcast from host ──

void NetworkManager::broadcastRoomState(const QJsonArray &players)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_state");
    msg[QStringLiteral("gameId")] = m_discoveryGameId;
    msg[QStringLiteral("gameName")] = m_discoveryGameName;
    msg[QStringLiteral("roomCapacity")] = m_discoveryRoomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_discoveryMaxPlayers;
    msg[QStringLiteral("players")] = players;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameStarted(const QString &gameId)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_start");
    if (!gameId.isEmpty())
        msg[QStringLiteral("gameId")] = gameId;
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

void NetworkManager::broadcastFlightRoll(int player, int diceValue)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_roll_result");
    msg[QStringLiteral("player")] = player;
    msg[QStringLiteral("diceValue")] = diceValue;
    broadcastJson(msg);
}

void NetworkManager::broadcastFlightMove(int player, int planeIndex)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_move_result");
    msg[QStringLiteral("player")] = player;
    msg[QStringLiteral("planeIndex")] = planeIndex;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameOver(int winner)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_over");
    msg[QStringLiteral("winner")] = winner;
    broadcastJson(msg);
}

void NetworkManager::sendDouDiZhuState(int playerId, const QJsonObject &state)
{
    QJsonObject msg = state;
    msg[QStringLiteral("type")] = QStringLiteral("ddz_state");

    for (QTcpSocket *client : m_clients) {
        if (client->property("playerId").toInt() != playerId)
            continue;
        sendJson(client, msg);
        return;
    }
}

// ── Slots ──

void NetworkManager::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *client = m_server->nextPendingConnection();
        connect(client, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
        connect(client, &QTcpSocket::disconnected, this, &NetworkManager::onClientDisconnected);
        m_clients.append(client);

        int playerId = 1;
        bool used = true;
        while (used) {
            used = false;
            for (QTcpSocket *existing : std::as_const(m_clients)) {
                if (existing == client)
                    continue;
                if (existing->property("playerId").toInt() == playerId) {
                    used = true;
                    ++playerId;
                    break;
                }
            }
        }
        client->setProperty("playerId", playerId);
        m_nextPlayerId = qMax(m_nextPlayerId, playerId + 1);

        emit clientCountChanged();
        broadcastHostedRoomAnnouncement();
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
        broadcastHostedRoomAnnouncement();
    }
}

void NetworkManager::onConnected()
{
    QString name = m_socket->property("playerName").toString().trimmed();
    if (name.isEmpty())
        name = QStringLiteral("player");

    const QString action = m_socket->property("connectAction").toString();
    QJsonObject msg;
    if (action == QStringLiteral("online_create_room")) {
        msg[QStringLiteral("type")] = QStringLiteral("create_room");
        msg[QStringLiteral("name")] = name;
        msg[QStringLiteral("gameId")] = m_socket->property("gameId").toString();
        msg[QStringLiteral("roomName")] = m_socket->property("roomName").toString();
    } else if (action == QStringLiteral("online_join_room")) {
        msg[QStringLiteral("type")] = QStringLiteral("join_room");
        msg[QStringLiteral("name")] = name;
        msg[QStringLiteral("roomId")] = m_socket->property("roomId").toString();
    } else {
        msg[QStringLiteral("type")] = QStringLiteral("join");
        msg[QStringLiteral("name")] = name;
        msg[QStringLiteral("gameId")] = m_socket->property("gameId").toString();
    }
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

void NetworkManager::onOnlineLobbyConnected()
{
    if (!m_onlineLobbySocket)
        return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("list_rooms");
    sendJson(m_onlineLobbySocket, msg);
}

void NetworkManager::onOnlineLobbyReadyRead()
{
    if (!m_onlineLobbySocket)
        return;

    QByteArray buf = m_onlineLobbySocket->property("readBuffer").toByteArray();
    buf.append(m_onlineLobbySocket->readAll());

    while (true) {
        const int idx = buf.indexOf('\n');
        if (idx < 0)
            break;

        const QByteArray line = buf.left(idx).trimmed();
        buf.remove(0, idx + 1);
        if (line.isEmpty())
            continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject msg = doc.object();
        const QString type = msg.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("rooms_list")) {
            applyOnlineRooms(msg.value(QStringLiteral("rooms")).toArray());
        } else if (type == QStringLiteral("error")) {
            emit errorOccurred(msg.value(QStringLiteral("message")).toString(QStringLiteral("Network error")));
        }
    }

    m_onlineLobbySocket->setProperty("readBuffer", buf);
}

void NetworkManager::onOnlineLobbyDisconnected()
{
    if (!m_onlineLobbySocket)
        return;

    m_onlineLobbySocket->deleteLater();
    m_onlineLobbySocket = nullptr;
}

void NetworkManager::onOnlineLobbyError(QAbstractSocket::SocketError)
{
    if (m_onlineLobbySocket)
        emit errorOccurred(m_onlineLobbySocket->errorString());
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

void NetworkManager::broadcastHostedRoomAnnouncement()
{
    if (!m_isHost || !m_server || !m_server->isListening())
        return;

    broadcastRoomAnnouncement();
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
    else if (type == QStringLiteral("flight_roll")) {
        const int playerId = sender->property("playerId").toInt();
        emit remoteFlightRoll(playerId);
    }
    else if (type == QStringLiteral("flight_move")) {
        const int playerId = sender->property("playerId").toInt();
        const int planeIndex = msg.value(QStringLiteral("planeIndex")).toInt(-1);
        emit remoteFlightMove(playerId, planeIndex);
    }
    else if (type == QStringLiteral("surrender")) {
        int playerId = sender->property("playerId").toInt();
        emit remoteSurrender(playerId);
    }
    else if (type == QStringLiteral("change_seat")) {
        int playerId = sender->property("playerId").toInt();
        emit remoteSeatChanged(playerId,
                               msg.value(QStringLiteral("seatType")).toString(QStringLiteral("active")));
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
    else if (type == QStringLiteral("flight_roll_result")) {
        const int player = msg.value(QStringLiteral("player")).toInt();
        const int diceValue = msg.value(QStringLiteral("diceValue")).toInt();
        emit flightRollReceived(player, diceValue);
    }
    else if (type == QStringLiteral("flight_move_result")) {
        const int player = msg.value(QStringLiteral("player")).toInt();
        const int planeIndex = msg.value(QStringLiteral("planeIndex")).toInt(-1);
        emit flightMoveReceived(player, planeIndex);
    }
    else if (type == QStringLiteral("room_state")) {
        emit roomStateReceived(msg);
    }
    else if (type == QStringLiteral("rooms_list")) {
        applyOnlineRooms(msg.value(QStringLiteral("rooms")).toArray());
    }
    else if (type == QStringLiteral("game_start")) {
        emit remoteStartGame(msg.value(QStringLiteral("gameId")).toString());
    }
    else if (type == QStringLiteral("start_game")) {
        // Dedicated server accepts this. LAN host currently ignores it.
    }
    else if (type == QStringLiteral("ddz_play")) {
        int playerId = sender->property("playerId").toInt();
        emit remoteDouDiZhuPlay(playerId, msg.value(QStringLiteral("cards")).toArray());
    }
    else if (type == QStringLiteral("ddz_pass")) {
        int playerId = sender->property("playerId").toInt();
        emit remoteDouDiZhuPass(playerId);
    }
    else if (type == QStringLiteral("ddz_state")) {
        emit douDiZhuStateReceived(msg);
    }
    else if (type == QStringLiteral("error")) {
        emit errorOccurred(msg.value(QStringLiteral("message")).toString(QStringLiteral("Network error")));
    }
}

void NetworkManager::connectClientSocket(const QString &ip, quint16 port,
                                         const QString &playerName, const QString &gameId,
                                         const QString &action, const QString &roomId,
                                         const QString &roomName)
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
    m_socket->setProperty("playerName", playerName);
    m_socket->setProperty("gameId", normalizedGameId(gameId));
    m_socket->setProperty("connectAction", action);
    m_socket->setProperty("roomId", roomId);
    m_socket->setProperty("roomName", roomName.trimmed());
    m_socket->connectToHost(ip, port);
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
    msg[QStringLiteral("roomUid")] = m_discoveryRoomUid;
    msg[QStringLiteral("hostName")] = m_discoveryHostName.isEmpty()
        ? QStringLiteral("host")
        : m_discoveryHostName;
    msg[QStringLiteral("hostIp")] = localIpForPeer(address);
    msg[QStringLiteral("port")] = static_cast<int>(m_serverPort);
    msg[QStringLiteral("playerCount")] = 1 + m_clients.size();
    msg[QStringLiteral("roomCapacity")] = m_discoveryRoomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_discoveryMaxPlayers;
    msg[QStringLiteral("gameId")] = m_discoveryGameId;
    msg[QStringLiteral("gameName")] = m_discoveryGameName;
    msg[QStringLiteral("inGame")] = m_discoveryGameInProgress;
    msg[QStringLiteral("isFull")] = (1 + m_clients.size()) >= m_discoveryRoomCapacity;

    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_discoverySocket->writeDatagram(payload, address, port);
}

void NetworkManager::broadcastRoomAnnouncement()
{
    QUdpSocket socket;
    socket.setProxy(QNetworkProxy::NoProxy);

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_announce");
    msg[QStringLiteral("app")] = QStringLiteral("LanBoard");
    msg[QStringLiteral("version")] = 1;
    msg[QStringLiteral("roomUid")] = m_discoveryRoomUid;
    msg[QStringLiteral("hostName")] = m_discoveryHostName.isEmpty()
        ? QStringLiteral("host")
        : m_discoveryHostName;
    msg[QStringLiteral("port")] = static_cast<int>(m_serverPort);
    msg[QStringLiteral("playerCount")] = 1 + m_clients.size();
    msg[QStringLiteral("roomCapacity")] = m_discoveryRoomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_discoveryMaxPlayers;
    msg[QStringLiteral("gameId")] = m_discoveryGameId;
    msg[QStringLiteral("gameName")] = m_discoveryGameName;
    msg[QStringLiteral("inGame")] = m_discoveryGameInProgress;
    msg[QStringLiteral("isFull")] = (1 + m_clients.size()) >= m_discoveryRoomCapacity;

    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    bool sent = false;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!isUsableIpv4(ip) || entry.broadcast().isNull())
                continue;

            QJsonObject scopedMsg = msg;
            scopedMsg[QStringLiteral("hostIp")] = ip.toString();
            const QByteArray scopedPayload = QJsonDocument(scopedMsg).toJson(QJsonDocument::Compact);
            socket.writeDatagram(scopedPayload, entry.broadcast(), DiscoveryPort);
            sent = true;
        }
    }

    if (!sent) {
        msg[QStringLiteral("hostIp")] = localIp();
        socket.writeDatagram(QJsonDocument(msg).toJson(QJsonDocument::Compact),
                             QHostAddress::Broadcast,
                             DiscoveryPort);
    }
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

    if (room.hostIp == localIp() && room.port == m_serverPort)
        return;

    bool changed = false;
    for (DiscoveredRoom &existing : m_discoveredRoomEntries) {
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
    map[QStringLiteral("roomCapacity")] = room.roomCapacity;
    map[QStringLiteral("maxPlayers")] = room.maxPlayers;
    map[QStringLiteral("gameId")] = room.gameId;
    map[QStringLiteral("gameName")] = room.gameName;
    map[QStringLiteral("inGame")] = room.inGame;
    map[QStringLiteral("isFull")] = room.isFull;
    return map;
}

void NetworkManager::applyOnlineRooms(const QJsonArray &rooms)
{
    QVariantList mappedRooms;
    mappedRooms.reserve(rooms.size());
    for (const auto &value : rooms) {
        const QJsonObject room = value.toObject();
        QVariantMap map;
        map[QStringLiteral("roomId")] = room.value(QStringLiteral("roomId")).toString();
        map[QStringLiteral("roomName")] = room.value(QStringLiteral("roomName")).toString();
        map[QStringLiteral("hostName")] = room.value(QStringLiteral("hostName")).toString();
        const QString gameId = normalizedGameId(
            room.value(QStringLiteral("gameId")).toString(QStringLiteral("gomoku")));
        map[QStringLiteral("gameId")] = gameId;
        map[QStringLiteral("gameName")] = room.value(QStringLiteral("gameName")).toString(
            defaultGameName(gameId));
        map[QStringLiteral("playerCount")] = room.value(QStringLiteral("playerCount")).toInt();
        map[QStringLiteral("roomCapacity")] = room.value(QStringLiteral("roomCapacity")).toInt(
            room.value(QStringLiteral("maxPlayers")).toInt(2));
        map[QStringLiteral("maxPlayers")] = room.value(QStringLiteral("maxPlayers")).toInt(2);
        map[QStringLiteral("inGame")] = room.value(QStringLiteral("inGame")).toBool();
        map[QStringLiteral("isFull")] = room.value(QStringLiteral("isFull")).toBool();
        mappedRooms.append(map);
    }

    m_onlineRooms = mappedRooms;
    emit onlineRoomsChanged();
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
