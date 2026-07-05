#include "networkmanager.h"

#include "linejsonprotocol.h"
#include "networkaddressutils.h"
#include "roomdiscoveryservice.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QDebug>

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

}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_roomDiscovery(new RoomDiscoveryService(this))
{
    connect(m_roomDiscovery, &RoomDiscoveryService::discoveredRoomsChanged,
            this, &NetworkManager::discoveredRoomsChanged);
    connect(m_roomDiscovery, &RoomDiscoveryService::errorOccurred,
            this, &NetworkManager::errorOccurred);
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
    updateDiscoveryIdentity();
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
    updateDiscoveryIdentity();
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
    return NetworkAddressUtils::bestLocalIpv4();
}

QVariantList NetworkManager::discoveredRooms() const
{
    return m_roomDiscovery ? m_roomDiscovery->discoveredRooms() : QVariantList();
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
    if (m_roomDiscovery)
        m_roomDiscovery->start();
}

void NetworkManager::stopRoomDiscovery()
{
    if (m_roomDiscovery)
        m_roomDiscovery->stop();
}

void NetworkManager::refreshRoomDiscovery()
{
    if (m_roomDiscovery)
        m_roomDiscovery->refresh();
}

void NetworkManager::clearDiscoveredRooms()
{
    if (m_roomDiscovery)
        m_roomDiscovery->clear();
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
    updateDiscoveryIdentity();
}

void NetworkManager::setDiscoveryGameInProgress(bool inProgress)
{
    m_discoveryGameInProgress = inProgress;
    updateDiscoveryIdentity();
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
    updateDiscoveryIdentity();
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
        updateDiscoveryIdentity();
    }
}

void NetworkManager::onReadyRead()
{
    QTcpSocket *sender = qobject_cast<QTcpSocket *>(QObject::sender());
    if (!sender)
        return;

    QByteArray buf = sender->property("readBuffer").toByteArray();
    buf.append(sender->readAll());

    QStringList errors;
    const QList<QJsonObject> messages = LineJsonProtocol::takeMessages(&buf, &errors);
    for (const QString &error : errors)
        qWarning() << "JSON parse error:" << error;
    for (const QJsonObject &message : messages)
        processMessage(sender, message);

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
        updateDiscoveryIdentity();
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

    const QList<QJsonObject> messages = LineJsonProtocol::takeMessages(&buf);
    for (const QJsonObject &msg : messages) {
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

// ── Private ──

void NetworkManager::sendJson(QTcpSocket *socket, const QJsonObject &obj)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;

    socket->write(LineJsonProtocol::encode(obj));
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

void NetworkManager::updateDiscoveryIdentity()
{
    if (!m_roomDiscovery)
        return;

    const bool activeHost = m_isHost && m_server && m_server->isListening();
    m_roomDiscovery->setPublishedRoom(m_discoveryHostName,
                                      m_serverPort,
                                      activeHost ? 1 + m_clients.size() : 0,
                                      m_discoveryRoomCapacity,
                                      m_discoveryMaxPlayers,
                                      m_discoveryGameId,
                                      m_discoveryGameName,
                                      m_discoveryGameInProgress,
                                      activeHost);
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
