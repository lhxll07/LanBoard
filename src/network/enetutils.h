#pragma once

#include <QByteArray>
#include <QString>
#include <QJsonObject>

#include <enet/enet.h>

namespace LanBoard::Enet {

bool initialize();
void deinitialize();

bool resolveAddress(const QString &host, quint16 port, ENetAddress &address);
bool decodeJsonPacket(const ENetPacket *packet, QJsonObject &object);

bool sendJson(ENetPeer *peer, const QJsonObject &object);
bool sendRaw(ENetPeer *peer, enet_uint8 channel, const QByteArray &payload, enet_uint32 flags);
bool isConnected(const ENetPeer *peer);

}  // namespace LanBoard::Enet
