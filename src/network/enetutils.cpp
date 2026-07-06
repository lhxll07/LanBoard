#include "enetutils.h"

#include <atomic>

#include <QJsonDocument>
#include <QJsonParseError>

namespace {

std::atomic<int> g_enetInitCount {0};

}

namespace LanBoard::Enet {

bool initialize()
{
    const int previous = g_enetInitCount.fetch_add(1);
    if (previous > 0)
        return true;

    if (enet_initialize() == 0)
        return true;

    g_enetInitCount.store(0);
    return false;
}

void deinitialize()
{
    const int previous = g_enetInitCount.fetch_sub(1);
    if (previous <= 1) {
        g_enetInitCount.store(0);
        enet_deinitialize();
    }
}

bool resolveAddress(const QString &host, quint16 port, ENetAddress &address)
{
    address = {};
    address.port = port;
    const QByteArray hostUtf8 = host.trimmed().toUtf8();
    return !hostUtf8.isEmpty()
        && enet_address_set_host(&address, hostUtf8.constData()) == 0;
}

bool decodeJsonPacket(const ENetPacket *packet, QJsonObject &object)
{
    object = {};
    if (!packet || !packet->data || packet->dataLength == 0)
        return false;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(reinterpret_cast<const char *>(packet->data),
                   static_cast<qsizetype>(packet->dataLength)),
        &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;

    object = document.object();
    return true;
}

bool sendJson(ENetPeer *peer, const QJsonObject &object)
{
    if (!isConnected(peer))
        return false;

    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    ENetPacket *packet = enet_packet_create(payload.constData(),
                                            static_cast<size_t>(payload.size()),
                                            ENET_PACKET_FLAG_RELIABLE);
    if (!packet)
        return false;

    if (enet_peer_send(peer, 0, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }

    return true;
}

bool sendRaw(ENetPeer *peer, enet_uint8 channel, const QByteArray &payload, enet_uint32 flags)
{
    if (!isConnected(peer) || payload.isEmpty())
        return false;

    ENetPacket *packet = enet_packet_create(payload.constData(),
                                            static_cast<size_t>(payload.size()),
                                            flags);
    if (!packet)
        return false;

    if (enet_peer_send(peer, channel, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }

    return true;
}

bool isConnected(const ENetPeer *peer)
{
    return peer && peer->state == ENET_PEER_STATE_CONNECTED;
}

}  // namespace LanBoard::Enet
