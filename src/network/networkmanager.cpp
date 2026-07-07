#include "networkmanager.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <utility>

#include "src/common/roomtypes.h"
#include "src/game/survivornetcodec.h"
#include "src/network/enetutils.h"
#include "src/network/networkaddressutils.h"
#include "src/network/protocolids.h"
#include "src/network/roomdiscoveryservice.h"

namespace {
constexpr size_t EnetChannelCount = 4;
constexpr enet_uint8 SurvivorInputChannel = 1;
constexpr enet_uint8 SurvivorFrameChannel = 2;
constexpr enet_uint8 SurvivorHudChannel = 3;

QJsonObject protocolMessage(QStringView type)
{
    return QJsonObject { { QStringLiteral("type"), type.toString() } };
}

}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_roomDiscovery(new RoomDiscoveryService(this))
{
    LanBoard::Enet::initialize();

    connect(m_roomDiscovery, &RoomDiscoveryService::discoveredRoomsChanged,
            this, &NetworkManager::discoveredRoomsChanged);
    connect(m_roomDiscovery, &RoomDiscoveryService::errorOccurred,
            this, &NetworkManager::errorOccurred);

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
    m_hostServer = enet_host_create(&address, 16, EnetChannelCount, 0, 0);
    if (!m_hostServer) {
        emit errorOccurred(QStringLiteral("无法监听端口 %1").arg(port));
        return;
    }

    m_isHost = true;
    m_serverPort = port;
    if (!m_enetServiceTimer.isActive())
        m_enetServiceTimer.start();
    updateDiscoveryIdentity();
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
    updateDiscoveryIdentity();
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
    return NetworkAddressUtils::bestLocalIpv4();
}

QVariantList NetworkManager::discoveredRooms() const
{
    return m_roomDiscovery ? m_roomDiscovery->discoveredRooms() : QVariantList {};
}

// ── Send actions (client → server) ──

void NetworkManager::sendReady(bool ready)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::Ready);
    msg[QStringLiteral("ready")] = ready;
    sendServerJson(msg);
}

void NetworkManager::sendPlacePiece(int row, int col)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::PlacePiece);
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    sendServerJson(msg);
}

void NetworkManager::sendFlightRoll()
{
    sendServerJson(protocolMessage(LanBoard::Protocol::FlightRoll));
}

void NetworkManager::sendFlightMove(int planeIndex)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::FlightMove);
    msg[QStringLiteral("planeIndex")] = planeIndex;
    sendServerJson(msg);
}

void NetworkManager::sendSurrender()
{
    sendServerJson(protocolMessage(LanBoard::Protocol::Surrender));
}

void NetworkManager::sendSurvivorInput(qreal horizontal, qreal vertical)
{
    sendServerRaw(LanBoard::Survivor::NetCodec::encodeInputPacket(horizontal, vertical),
                  SurvivorInputChannel,
                  0);
}

void NetworkManager::sendSurvivorChooseLevelUp(const QString &upgradeId)
{
    const QString normalizedUpgradeId = upgradeId.trimmed();
    if (normalizedUpgradeId.isEmpty())
        return;

    sendServerRaw(LanBoard::Survivor::NetCodec::encodeChooseLevelUpPacket(normalizedUpgradeId),
                  SurvivorHudChannel,
                  ENET_PACKET_FLAG_RELIABLE);
}

void NetworkManager::sendSurvivorCloseChest()
{
    sendServerRaw(LanBoard::Survivor::NetCodec::encodeCloseChestPacket(),
                  SurvivorHudChannel,
                  ENET_PACKET_FLAG_RELIABLE);
}

void NetworkManager::sendStartGame()
{
    sendServerJson(protocolMessage(LanBoard::Protocol::StartGame));
}

void NetworkManager::sendGameOverResult(int winner)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::GameOver);
    msg[QStringLiteral("winner")] = winner;
    if (m_isHost) {
        broadcastJson(msg);
        return;
    }
    sendServerJson(msg);
}

void NetworkManager::sendDouDiZhuPlay(const QVariantList &cardIds)
{
    QJsonArray cards;
    for (const auto &id : cardIds)
        cards.append(id.toInt());

    QJsonObject msg = protocolMessage(LanBoard::Protocol::DouDiZhuPlay);
    msg[QStringLiteral("cards")] = cards;
    sendServerJson(msg);
}

void NetworkManager::sendDouDiZhuPass()
{
    sendServerJson(protocolMessage(LanBoard::Protocol::DouDiZhuPass));
}

void NetworkManager::sendChangeSeat(const QString &seatType)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::ChangeSeat);
    msg[QStringLiteral("seatType")] = LanBoard::seatTypeString(
        LanBoard::normalizedSeatKind(seatType));
    sendServerJson(msg);
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
    QJsonObject msg = protocolMessage(LanBoard::Protocol::SwitchRoomGame);
    msg[QStringLiteral("gameId")] = LanBoard::normalizeGameId(gameId);
    sendServerJson(msg);
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
    m_discoveryGameId = LanBoard::normalizeGameId(gameId);
    m_discoveryGameName = gameName.trimmed().isEmpty()
        ? LanBoard::gameName(m_discoveryGameId)
        : gameName.trimmed();
    m_discoveryRoomCapacity = qMax(2, roomCapacity);
    m_discoveryMaxPlayers = qMax(2, maxPlayers);
    updateDiscoveryIdentity();
}

// ── Broadcast from host ──

void NetworkManager::broadcastRoomState(const QJsonArray &players)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::RoomState);
    msg[QStringLiteral("gameId")] = m_discoveryGameId;
    msg[QStringLiteral("gameName")] = m_discoveryGameName;
    msg[QStringLiteral("roomCapacity")] = m_discoveryRoomCapacity;
    msg[QStringLiteral("maxPlayers")] = m_discoveryMaxPlayers;
    msg[QStringLiteral("players")] = players;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameStarted(const QString &gameId)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::GameStart);
    if (!gameId.isEmpty())
        msg[QStringLiteral("gameId")] = gameId;
    broadcastJson(msg);
}

void NetworkManager::broadcastMove(int player, int row, int col)
{
    QJsonObject msg = protocolMessage(QStringLiteral("move"));
    msg[QStringLiteral("player")] = player;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    broadcastJson(msg);
}

void NetworkManager::broadcastFlightRoll(int player, int diceValue)
{
    QJsonObject msg = protocolMessage(QStringLiteral("flight_roll_result"));
    msg[QStringLiteral("player")] = player;
    msg[QStringLiteral("diceValue")] = diceValue;
    broadcastJson(msg);
}

void NetworkManager::broadcastFlightMove(int player, int planeIndex)
{
    QJsonObject msg = protocolMessage(QStringLiteral("flight_move_result"));
    msg[QStringLiteral("player")] = player;
    msg[QStringLiteral("planeIndex")] = planeIndex;
    broadcastJson(msg);
}

void NetworkManager::broadcastGameOver(int winner)
{
    QJsonObject msg = protocolMessage(LanBoard::Protocol::GameOver);
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

    hostSlot = enet_host_create(nullptr, 1, EnetChannelCount, 0, 0);
    if (!hostSlot) {
        emit errorOccurred(QStringLiteral("无法创建 ENet 客户端"));
        return;
    }

    peerSlot = enet_host_connect(hostSlot, &address, EnetChannelCount, 0);
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

void NetworkManager::sendServerJson(const QJsonObject &msg)
{
    if (!m_enetActiveConnection)
        return;
    sendEnetJson(m_serverPeer, m_clientHost, msg);
}

void NetworkManager::sendServerRaw(const QByteArray &payload,
                                   enet_uint8 channel,
                                   enet_uint32 flags)
{
    if (!m_enetActiveConnection)
        return;
    sendEnetRaw(m_serverPeer, m_clientHost, payload, channel, flags);
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
    EnetClientSession *session = sessionForPeer(peer);
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
    EnetClientSession *session = sessionForPeer(peer);
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

NetworkManager::EnetClientSession *NetworkManager::sessionForPeer(ENetPeer *peer)
{
    for (EnetClientSession &session : m_peerClients) {
        if (session.peer == peer)
            return &session;
    }
    return nullptr;
}

const NetworkManager::EnetClientSession *NetworkManager::sessionForPeer(ENetPeer *peer) const
{
    for (const EnetClientSession &session : m_peerClients) {
        if (session.peer == peer)
            return &session;
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
                updateDiscoveryIdentity();
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
                    updateDiscoveryIdentity();
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
                sendEnetJson(event.peer, host, pendingConnectMessage(lobbyOnly));
                if (!lobbyOnly)
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

QJsonObject NetworkManager::pendingConnectMessage(bool lobbyOnly) const
{
    if (lobbyOnly)
        return protocolMessage(LanBoard::Protocol::ListRooms);

    if (m_pendingConnectAction == QStringLiteral("online_create_room")) {
        QJsonObject msg = protocolMessage(LanBoard::Protocol::CreateRoom);
        msg[QStringLiteral("name")] = m_pendingPlayerName;
        msg[QStringLiteral("gameId")] = m_pendingGameId;
        msg[QStringLiteral("roomName")] = m_pendingRoomName;
        return msg;
    }

    if (m_pendingConnectAction == QStringLiteral("online_join_room")) {
        QJsonObject msg = protocolMessage(LanBoard::Protocol::JoinRoom);
        msg[QStringLiteral("name")] = m_pendingPlayerName;
        msg[QStringLiteral("roomId")] = m_pendingRoomId;
        return msg;
    }

    QJsonObject msg = protocolMessage(LanBoard::Protocol::Join);
    msg[QStringLiteral("name")] = m_pendingPlayerName;
    msg[QStringLiteral("gameId")] = m_pendingGameId;
    return msg;
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

void NetworkManager::updateDiscoveryIdentity()
{
    if (!m_roomDiscovery)
        return;

    const bool activeHost = m_isHost && m_hostServer != nullptr;
    m_roomDiscovery->setPublishedRoom(m_discoveryHostName,
                                      m_serverPort,
                                      activeHost ? 1 + m_peerClients.size() : 0,
                                      m_discoveryRoomCapacity,
                                      m_discoveryMaxPlayers,
                                      m_discoveryGameId,
                                      m_discoveryGameName,
                                      m_discoveryGameInProgress,
                                      activeHost);
}
