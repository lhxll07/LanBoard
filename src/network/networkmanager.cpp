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
#include <QUuid>
#include <algorithm>
#include <limits>

#include "src/common/roomtypes.h"
#include "src/game/survivornetcodec.h"
#include "src/network/enetutils.h"
#include "src/network/protocolids.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {
constexpr enet_uint8 SurvivorInputChannel = 1;
constexpr enet_uint8 SurvivorFrameChannel = 2;
constexpr enet_uint8 SurvivorHudChannel = 3;

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
    LanBoard::Enet::initialize();

    m_discoveryTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_discoveryTimer, &QTimer::timeout, this, &NetworkManager::broadcastDiscoveryQuery);

    m_discoveryPruneTimer.setInterval(1000);
    connect(&m_discoveryPruneTimer, &QTimer::timeout, this, &NetworkManager::pruneDiscoveredRooms);

    m_hostAnnouncementTimer.setInterval(DiscoveryIntervalMs);
    connect(&m_hostAnnouncementTimer, &QTimer::timeout,
            this, &NetworkManager::broadcastHostedRoomAnnouncement);

    m_enetServiceTimer.setInterval(4);
    m_enetServiceTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_enetServiceTimer, &QTimer::timeout, this, &NetworkManager::serviceEnet);
}

NetworkManager::~NetworkManager()
{
    disconnectAll();
    LanBoard::Enet::deinitialize();
}

// ── Public API ──

void NetworkManager::startServer(quint16 port)
{
    disconnectAll();

    ENetAddress address {};
    address.host = ENET_HOST_ANY;
    address.port = port;
    m_hostServer = enet_host_create(&address, 16, 4, 0, 0);
    if (!m_hostServer) {
        emit errorOccurred(QStringLiteral("无法监听端口 %1").arg(port));
        return;
    }

    m_isHost = true;
    m_serverPort = port;
    if (!m_hostAnnouncementTimer.isActive())
        m_hostAnnouncementTimer.start();
    if (!m_enetServiceTimer.isActive())
        m_enetServiceTimer.start();
    broadcastHostedRoomAnnouncement();
    emit connectionChanged();
}

void NetworkManager::connectToServer(const QString &ip, quint16 port,
                                     const QString &playerName,
                                     const QString &gameId)
{
    connectDedicatedPeer(ip,
                         port,
                         playerName,
                         gameId,
                         LanBoard::Protocol::Join);
}

void NetworkManager::disconnectAll()
{
    if (m_isHost) {
        disconnectAllHostPeers();
        if (m_hostServer) {
            enet_host_destroy(m_hostServer);
            m_hostServer = nullptr;
        }
    }
    disconnectEnetPeer(m_serverPeer, m_clientHost, false);
    disconnectEnetPeer(m_lobbyPeer, m_lobbyHost, false);
    m_pendingConnectAction.clear();
    m_pendingPlayerName.clear();
    m_pendingGameId.clear();
    m_pendingRoomId.clear();
    m_pendingRoomName.clear();
    m_enetActiveConnection = false;
    m_isHost = false;
    m_discoveryGameInProgress = false;
    m_serverPort = 0;
    m_connectedIp.clear();
    m_connectedPort = 0;
    m_hostAnnouncementTimer.stop();
    if (!m_clientHost && !m_lobbyHost && !m_hostServer)
        m_enetServiceTimer.stop();
    emit connectionChanged();
    emit clientCountChanged();
}

bool NetworkManager::isConnected() const
{
    if (m_isHost)
        return m_hostServer != nullptr;
    if (LanBoard::Enet::isConnected(m_serverPeer))
        return true;
    return false;
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
    msg[QStringLiteral("type")] = LanBoard::Protocol::Ready;
    msg[QStringLiteral("ready")] = ready;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendPlacePiece(int row, int col)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::PlacePiece;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendFlightRoll()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::FlightRoll;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendFlightMove(int planeIndex)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::FlightMove;
    msg[QStringLiteral("planeIndex")] = planeIndex;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendSurrender()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::Surrender;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendSurvivorInput(qreal horizontal, qreal vertical)
{
    if (!m_enetActiveConnection)
        return;

    sendEnetRaw(m_serverPeer,
                m_clientHost,
                LanBoard::Survivor::NetCodec::encodeInputPacket(horizontal, vertical),
                SurvivorInputChannel,
                0);
}

void NetworkManager::sendSurvivorChooseLevelUp(const QString &upgradeId)
{
    const QString normalizedUpgradeId = upgradeId.trimmed();
    if (normalizedUpgradeId.isEmpty())
        return;

    if (!m_enetActiveConnection)
        return;

    sendEnetRaw(m_serverPeer,
                m_clientHost,
                LanBoard::Survivor::NetCodec::encodeChooseLevelUpPacket(normalizedUpgradeId),
                SurvivorHudChannel,
                ENET_PACKET_FLAG_RELIABLE);
}

void NetworkManager::sendSurvivorCloseChest()
{
    if (!m_enetActiveConnection)
        return;

    sendEnetRaw(m_serverPeer,
                m_clientHost,
                LanBoard::Survivor::NetCodec::encodeCloseChestPacket(),
                SurvivorHudChannel,
                ENET_PACKET_FLAG_RELIABLE);
}

void NetworkManager::sendStartGame()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::StartGame;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendGameOverResult(int winner)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameOver;
    msg[QStringLiteral("winner")] = winner;
    if (m_isHost) {
        broadcastJson(msg);
        return;
    }
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendDouDiZhuPlay(const QVariantList &cardIds)
{
    QJsonArray cards;
    for (const auto &id : cardIds)
        cards.append(id.toInt());

    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::DouDiZhuPlay;
    msg[QStringLiteral("cards")] = cards;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendDouDiZhuPass()
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::DouDiZhuPass;
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendChangeSeat(const QString &seatType)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::ChangeSeat;
    msg[QStringLiteral("seatType")] = LanBoard::seatTypeString(
        LanBoard::normalizedSeatKind(seatType));
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
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

    disconnectEnetPeer(m_lobbyPeer, m_lobbyHost, false);
    connectDedicatedPeer(host.trimmed(),
                         port,
                         QString(),
                         QString(),
                         QStringLiteral("online_list_rooms"));
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
    connectDedicatedPeer(host,
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
    connectDedicatedPeer(host,
                         port,
                         playerName,
                         QString(),
                         QStringLiteral("online_join_room"),
                         roomId);
}

void NetworkManager::sendSwitchRoomGame(const QString &gameId)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::SwitchRoomGame;
    msg[QStringLiteral("gameId")] = LanBoard::normalizeGameId(gameId);
    if (m_enetActiveConnection)
        sendEnetJson(m_serverPeer, m_clientHost, msg);
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
    m_discoveryGameId = LanBoard::normalizeGameId(gameId);
    m_discoveryGameName = gameName.trimmed().isEmpty()
        ? LanBoard::gameName(m_discoveryGameId)
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
    msg[QStringLiteral("type")] = LanBoard::Protocol::RoomState;
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
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameStart;
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
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameOver;
    msg[QStringLiteral("winner")] = winner;
    broadcastJson(msg);
}

void NetworkManager::sendSurvivorFastPacketToPlayer(int playerId, const QByteArray &payload)
{
    ENetPeer *peer = peerForPlayerId(playerId);
    if (!peer || payload.isEmpty())
        return;
    sendEnetRaw(peer, m_hostServer, payload, SurvivorFrameChannel, 0);
}

void NetworkManager::sendSurvivorHudPacketToPlayer(int playerId, const QByteArray &payload)
{
    ENetPeer *peer = peerForPlayerId(playerId);
    if (!peer || payload.isEmpty())
        return;
    sendEnetRaw(peer, m_hostServer, payload, SurvivorHudChannel, ENET_PACKET_FLAG_RELIABLE);
}

void NetworkManager::sendDouDiZhuState(int playerId, const QJsonObject &state)
{
    QJsonObject msg = state;
    msg[QStringLiteral("type")] = LanBoard::Protocol::DouDiZhuState;
    sendRoomStateToPlayer(playerId, msg);
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
            if (m_isHost && m_hostServer)
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
    if (!m_isHost || !m_hostServer)
        return;

    broadcastRoomAnnouncement();
}

void NetworkManager::connectDedicatedPeer(const QString &host, quint16 port,
                                          const QString &playerName, const QString &gameId,
                                          const QString &action, const QString &roomId,
                                          const QString &roomName)
{
    ENetAddress address {};
    if (!LanBoard::Enet::resolveAddress(host, port, address)) {
        emit errorOccurred(QStringLiteral("无法解析服务器地址"));
        return;
    }

    const bool lobbyOnly = action == QStringLiteral("online_list_rooms");
    ENetHost *&hostSlot = lobbyOnly ? m_lobbyHost : m_clientHost;
    ENetPeer *&peerSlot = lobbyOnly ? m_lobbyPeer : m_serverPeer;
    disconnectEnetPeer(peerSlot, hostSlot, false);

    hostSlot = enet_host_create(nullptr, 1, 4, 0, 0);
    if (!hostSlot) {
        emit errorOccurred(QStringLiteral("无法创建 ENet 客户端"));
        return;
    }

    peerSlot = enet_host_connect(hostSlot, &address, 3, 0);
    if (!peerSlot) {
        enet_host_destroy(hostSlot);
        hostSlot = nullptr;
        emit errorOccurred(QStringLiteral("无法发起 ENet 连接"));
        return;
    }

    m_connectedIp = host;
    m_connectedPort = port;
    m_pendingConnectAction = action;
    m_pendingPlayerName = playerName.trimmed();
    m_pendingGameId = LanBoard::normalizeGameId(gameId);
    m_pendingRoomId = roomId.trimmed();
    m_pendingRoomName = roomName.trimmed();
    if (!lobbyOnly)
        m_enetActiveConnection = true;

    if (!m_enetServiceTimer.isActive())
        m_enetServiceTimer.start();
}

void NetworkManager::disconnectEnetPeer(ENetPeer *&peer, ENetHost *&host, bool graceful)
{
    if (peer) {
        if (graceful) {
            enet_peer_disconnect(peer, 0);
            if (host)
                enet_host_flush(host);
        } else {
            enet_peer_disconnect_now(peer, 0);
        }
        peer = nullptr;
    }

    if (host) {
        enet_host_destroy(host);
        host = nullptr;
    }
}

void NetworkManager::sendEnetJson(ENetPeer *peer, ENetHost *host, const QJsonObject &msg)
{
    if (!LanBoard::Enet::sendJson(peer, msg) || !host)
        return;
    enet_host_flush(host);
}

void NetworkManager::sendEnetRaw(ENetPeer *peer, ENetHost *host,
                                 const QByteArray &payload, enet_uint8 channel, enet_uint32 flags)
{
    if (!LanBoard::Enet::sendRaw(peer, channel, payload, flags) || !host)
        return;
    enet_host_flush(host);
}

void NetworkManager::disconnectAllHostPeers()
{
    for (EnetClientSession &session : m_peerClients) {
        if (session.peer)
            enet_peer_disconnect_now(session.peer, 0);
    }
    m_peerClients.clear();
}

void NetworkManager::broadcastJson(const QJsonObject &obj, ENetPeer *exclude)
{
    for (const EnetClientSession &session : std::as_const(m_peerClients)) {
        if (!session.peer || session.peer == exclude)
            continue;
        LanBoard::Enet::sendJson(session.peer, obj);
    }

    if (m_hostServer)
        enet_host_flush(m_hostServer);
}

void NetworkManager::sendRoomStateToPlayer(int playerId, const QJsonObject &state)
{
    ENetPeer *peer = peerForPlayerId(playerId);
    if (!peer)
        return;

    sendEnetJson(peer, m_hostServer, state);
    if (state.value(QStringLiteral("type")).toString() == LanBoard::Protocol::Error)
        enet_peer_disconnect_later(peer, 0);
}

void NetworkManager::processHostPeerMessage(ENetPeer *peer, const QJsonObject &msg)
{
    EnetClientSession *session = nullptr;
    for (EnetClientSession &candidate : m_peerClients) {
        if (candidate.peer == peer) {
            session = &candidate;
            break;
        }
    }
    if (!session)
        return;

    const QString type = msg.value(QStringLiteral("type")).toString();
    if (type == LanBoard::Protocol::Join) {
        session->playerName = msg.value(QStringLiteral("name")).toString();
        emit joinRequested(session->playerName, session->playerId);
    } else if (type == LanBoard::Protocol::Ready) {
        emit remoteReadyChanged(session->playerId, msg.value(QStringLiteral("ready")).toBool());
    } else if (type == LanBoard::Protocol::PlacePiece) {
        emit remoteMoveReceived(session->playerId,
                                msg.value(QStringLiteral("row")).toInt(),
                                msg.value(QStringLiteral("col")).toInt());
    } else if (type == LanBoard::Protocol::FlightRoll) {
        emit remoteFlightRoll(session->playerId);
    } else if (type == LanBoard::Protocol::FlightMove) {
        emit remoteFlightMove(session->playerId,
                              msg.value(QStringLiteral("planeIndex")).toInt(-1));
    } else if (type == LanBoard::Protocol::Surrender) {
        emit remoteSurrender(session->playerId);
    } else if (type == LanBoard::Protocol::ChangeSeat) {
        emit remoteSeatChanged(session->playerId,
                               msg.value(QStringLiteral("seatType")).toString(QStringLiteral("active")));
    } else if (type == LanBoard::Protocol::DouDiZhuPlay) {
        emit remoteDouDiZhuPlay(session->playerId, msg.value(QStringLiteral("cards")).toArray());
    } else if (type == LanBoard::Protocol::DouDiZhuPass) {
        emit remoteDouDiZhuPass(session->playerId);
    }
}

bool NetworkManager::processHostPeerBinaryPacket(ENetPeer *peer, const QByteArray &payload)
{
    EnetClientSession *session = nullptr;
    for (EnetClientSession &candidate : m_peerClients) {
        if (candidate.peer == peer) {
            session = &candidate;
            break;
        }
    }
    if (!session)
        return false;

    const auto kind = LanBoard::Survivor::NetCodec::packetKind(payload);
    qreal horizontal = 0.0;
    qreal vertical = 0.0;
    QString upgradeId;

    switch (kind) {
    case LanBoard::Survivor::NetCodec::PacketKind::Input:
        if (!LanBoard::Survivor::NetCodec::decodeInputPacket(payload, horizontal, vertical))
            return false;
        emit remoteSurvivorInput(session->playerId, horizontal, vertical);
        return true;
    case LanBoard::Survivor::NetCodec::PacketKind::ChooseLevelUp:
        if (!LanBoard::Survivor::NetCodec::decodeChooseLevelUpPacket(payload, upgradeId))
            return false;
        emit remoteSurvivorChooseLevelUp(session->playerId, upgradeId);
        return true;
    case LanBoard::Survivor::NetCodec::PacketKind::CloseChest:
        if (!LanBoard::Survivor::NetCodec::decodeCloseChestPacket(payload))
            return false;
        emit remoteSurvivorCloseChest(session->playerId);
        return true;
    default:
        return false;
    }
}

int NetworkManager::allocateHostPlayerId() const
{
    for (int candidate = 1; candidate < qMax(2, m_discoveryRoomCapacity); ++candidate) {
        bool used = false;
        for (const EnetClientSession &session : m_peerClients) {
            if (session.playerId == candidate) {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
    }
    return qMax(1, m_peerClients.size() + 1);
}

ENetPeer *NetworkManager::peerForPlayerId(int playerId) const
{
    for (const EnetClientSession &session : m_peerClients) {
        if (session.playerId == playerId)
            return session.peer;
    }
    return nullptr;
}

void NetworkManager::serviceEnet()
{
    if (m_hostServer) {
        ENetEvent event {};
        while (enet_host_service(m_hostServer, &event, 0) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                EnetClientSession session;
                session.peer = event.peer;
                session.playerId = allocateHostPlayerId();
                m_peerClients.append(session);
                emit clientCountChanged();
                broadcastHostedRoomAnnouncement();
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                const QByteArray payload(reinterpret_cast<const char *>(event.packet->data),
                                         static_cast<qsizetype>(event.packet->dataLength));
                QJsonObject msg;
                if (processHostPeerBinaryPacket(event.peer, payload)) {
                } else if (LanBoard::Enet::decodeJsonPacket(event.packet, msg)) {
                    processHostPeerMessage(event.peer, msg);
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                for (int i = 0; i < m_peerClients.size(); ++i) {
                    if (m_peerClients.at(i).peer != event.peer)
                        continue;
                    const int playerId = m_peerClients.at(i).playerId;
                    m_peerClients.removeAt(i);
                    emit clientDisconnected(playerId);
                    emit clientCountChanged();
                    broadcastHostedRoomAnnouncement();
                    break;
                }
                break;
            }
            case ENET_EVENT_TYPE_NONE:
            default:
                break;
            }
        }
    }

    auto serviceHost = [this](ENetHost *host, bool lobbyOnly) {
        if (!host)
            return;

        ENetEvent event {};
        while (enet_host_service(host, &event, 0) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                QJsonObject msg;
                if (lobbyOnly) {
                    msg[QStringLiteral("type")] = LanBoard::Protocol::ListRooms;
                    sendEnetJson(event.peer, host, msg);
                    break;
                }

                const QString action = m_pendingConnectAction;
                if (action == QStringLiteral("online_create_room")) {
                    msg[QStringLiteral("type")] = LanBoard::Protocol::CreateRoom;
                    msg[QStringLiteral("name")] = m_pendingPlayerName;
                    msg[QStringLiteral("gameId")] = m_pendingGameId;
                    msg[QStringLiteral("roomName")] = m_pendingRoomName;
                } else if (action == QStringLiteral("online_join_room")) {
                    msg[QStringLiteral("type")] = LanBoard::Protocol::JoinRoom;
                    msg[QStringLiteral("name")] = m_pendingPlayerName;
                    msg[QStringLiteral("roomId")] = m_pendingRoomId;
                } else {
                    msg[QStringLiteral("type")] = LanBoard::Protocol::Join;
                    msg[QStringLiteral("name")] = m_pendingPlayerName;
                    msg[QStringLiteral("gameId")] = m_pendingGameId;
                }
                sendEnetJson(event.peer, host, msg);
                emit connectionChanged();
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                const QByteArray payload(reinterpret_cast<const char *>(event.packet->data),
                                         static_cast<qsizetype>(event.packet->dataLength));
                QJsonObject msg;
                if (!lobbyOnly && processDedicatedServerBinaryPacket(payload)) {
                } else if (LanBoard::Enet::decodeJsonPacket(event.packet, msg)) {
                    if (lobbyOnly)
                        processOnlineLobbyMessage(msg);
                    else
                        processDedicatedServerMessage(msg);
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                if (lobbyOnly) {
                    if (event.peer == m_lobbyPeer)
                        m_lobbyPeer = nullptr;
                    if (!m_clientHost && !m_lobbyPeer && !m_hostServer)
                        m_enetServiceTimer.stop();
                } else {
                    if (event.peer == m_serverPeer)
                        m_serverPeer = nullptr;
                    const bool hadConnection = m_enetActiveConnection;
                    m_enetActiveConnection = false;
                    if (hadConnection)
                        emit connectionChanged();
                }
                break;
            case ENET_EVENT_TYPE_NONE:
            default:
                break;
            }
        }
    };

    serviceHost(m_lobbyHost, true);
    serviceHost(m_clientHost, false);

    if (!m_lobbyHost && !m_clientHost && !m_hostServer)
        m_enetServiceTimer.stop();
}

void NetworkManager::processDedicatedServerMessage(const QJsonObject &msg)
{
    const QString type = msg.value(QStringLiteral("type")).toString();
    if (type == LanBoard::Protocol::RoomState) {
        emit roomStateReceived(msg);
    } else if (type == LanBoard::Protocol::RoomsList) {
        applyOnlineRooms(msg.value(QStringLiteral("rooms")).toArray());
    } else if (type == LanBoard::Protocol::GameStart) {
        emit remoteStartGame(msg.value(QStringLiteral("gameId")).toString());
    } else if (type == LanBoard::Protocol::GameOver) {
        emit gameOverReceived(msg.value(QStringLiteral("winner")).toInt());
    } else if (type == LanBoard::Protocol::DouDiZhuState) {
        emit douDiZhuStateReceived(msg);
    } else if (type == QStringLiteral("move")) {
        emit remoteMoveReceived(msg.value(QStringLiteral("player")).toInt(),
                                msg.value(QStringLiteral("row")).toInt(),
                                msg.value(QStringLiteral("col")).toInt());
    } else if (type == QStringLiteral("flight_roll_result")) {
        emit flightRollReceived(msg.value(QStringLiteral("player")).toInt(),
                                msg.value(QStringLiteral("diceValue")).toInt());
    } else if (type == QStringLiteral("flight_move_result")) {
        emit flightMoveReceived(msg.value(QStringLiteral("player")).toInt(),
                                msg.value(QStringLiteral("planeIndex")).toInt(-1));
    } else if (type == LanBoard::Protocol::Error) {
        emit errorOccurred(msg.value(QStringLiteral("message")).toString(QStringLiteral("Network error")));
    }
}

bool NetworkManager::processDedicatedServerBinaryPacket(const QByteArray &payload)
{
    switch (LanBoard::Survivor::NetCodec::packetKind(payload)) {
    case LanBoard::Survivor::NetCodec::PacketKind::FastState:
        emit survivorFastPacketReceived(payload);
        return true;
    case LanBoard::Survivor::NetCodec::PacketKind::HudState:
        emit survivorHudPacketReceived(payload);
        return true;
    default:
        return false;
    }
}

void NetworkManager::processOnlineLobbyMessage(const QJsonObject &msg)
{
    if (msg.value(QStringLiteral("type")).toString() == LanBoard::Protocol::RoomsList) {
        applyOnlineRooms(msg.value(QStringLiteral("rooms")).toArray());
        disconnectEnetPeer(m_lobbyPeer, m_lobbyHost, false);
        if (!m_clientHost && !m_hostServer)
            m_enetServiceTimer.stop();
        return;
    }

    processDedicatedServerMessage(msg);
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
    msg[QStringLiteral("playerCount")] = 1 + m_peerClients.size();
    msg[QStringLiteral("roomCapacity")] = m_discoveryRoomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_discoveryMaxPlayers;
    msg[QStringLiteral("gameId")] = m_discoveryGameId;
    msg[QStringLiteral("gameName")] = m_discoveryGameName;
    msg[QStringLiteral("inGame")] = m_discoveryGameInProgress;
    msg[QStringLiteral("isFull")] = (1 + m_peerClients.size()) >= m_discoveryRoomCapacity;

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
    msg[QStringLiteral("playerCount")] = 1 + m_peerClients.size();
    msg[QStringLiteral("roomCapacity")] = m_discoveryRoomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_discoveryMaxPlayers;
    msg[QStringLiteral("gameId")] = m_discoveryGameId;
    msg[QStringLiteral("gameName")] = m_discoveryGameName;
    msg[QStringLiteral("inGame")] = m_discoveryGameInProgress;
    msg[QStringLiteral("isFull")] = (1 + m_peerClients.size()) >= m_discoveryRoomCapacity;

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
    room.gameId = LanBoard::normalizeGameId(
        msg.value(QStringLiteral("gameId")).toString(QStringLiteral("gomoku")));
    room.gameName = msg.value(QStringLiteral("gameName")).toString(LanBoard::gameName(room.gameId));
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
        const QString gameId = LanBoard::normalizeGameId(
            room.value(QStringLiteral("gameId")).toString(QStringLiteral("gomoku")));
        map[QStringLiteral("gameId")] = gameId;
        map[QStringLiteral("gameName")] = room.value(QStringLiteral("gameName")).toString(
            LanBoard::gameName(gameId));
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
