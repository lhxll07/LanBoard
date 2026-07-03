#include "networkmanager.h"

#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHostAddress>
#include <QDebug>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
}

// ── Public API ──

void NetworkManager::startServer(quint16 port)
{
    if (m_server) {
        m_server->close();
        delete m_server;
    }
    m_server = new QTcpServer(this);
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
        m_socket->disconnectFromHost();
        delete m_socket;
    }
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);

    m_connectedIp = ip;
    m_socket->connectToHost(ip, port);

    // Send join message when connected (handled in onConnected)
    m_socket->setProperty("playerName", playerName);
}

void NetworkManager::disconnectAll()
{
    if (m_isHost) {
        for (auto *c : m_clients) {
            c->disconnectFromHost();
            c->deleteLater();
        }
        m_clients.clear();
        if (m_server) {
            m_server->close();
        }
    }
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_isHost = false;
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
    for (const QHostAddress &addr : QNetworkInterface::allAddresses()) {
        if (addr != QHostAddress::LocalHost && addr.protocol() == QAbstractSocket::IPv4Protocol)
            return addr.toString();
    }
    return QStringLiteral("127.0.0.1");
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

void NetworkManager::sendNewGame()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("new_game");
    sendJson(m_socket, msg);
}

// ── Broadcast from host ──

void NetworkManager::broadcastRoomState(const QJsonArray &players)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_state");
    msg[QStringLiteral("players")] = players;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameStarted(int firstPlayer)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_start");
    msg[QStringLiteral("firstPlayer")] = firstPlayer;
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

    m_readBuffer.append(sender->readAll());

    // Process complete messages (newline-delimited JSON)
    while (true) {
        int idx = m_readBuffer.indexOf('\n');
        if (idx < 0)
            break;

        QByteArray line = m_readBuffer.left(idx).trimmed();
        m_readBuffer.remove(0, idx + 1);

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
}

void NetworkManager::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(QObject::sender());
    if (client) {
        m_clients.removeOne(client);
        client->deleteLater();
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
    emit connectionChanged();
}

void NetworkManager::onError(QAbstractSocket::SocketError)
{
    if (m_socket)
        emit errorOccurred(m_socket->errorString());
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
        int playerId = sender->property("playerId").toInt();
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
    else if (type == QStringLiteral("room_state")) {
        emit roomStateReceived(msg);
    }
    else if (type == QStringLiteral("start_game")) {
        emit remoteStartGame();
    }
    else if (type == QStringLiteral("new_game")) {
        emit remoteStartGame(); // reuse the same flow
    }
}
