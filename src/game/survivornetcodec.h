#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector2D>

#include "survivorworld.h"

namespace LanBoard::Survivor::NetCodec {

enum class PacketKind : quint8 {
    Invalid = 0,
    FastState = 1,
    HudState = 2,
    Input = 3,
    ChooseLevelUp = 4,
    CloseChest = 5,
};

struct FastNetworkState {
    bool running = false;
    bool gameOver = false;
    int survivalTimeMs = 0;
    int killCount = 0;
    int interactionPlayerId = -1;
    QString waveLabel;
    qreal auraRadius = 0.0;
    bool hasLocalPlayer = false;
    PlayerState localPlayer;
    quint64 seq = 0;
    QVector2D origin;
    QVector<PlayerState> players;
    RenderSnapshot snapshot;
};

struct HudNetworkState {
    int interactionPlayerId = -1;
    QString chestTitle;
    QList<UpgradeChoice> levelUpChoices;
    QList<ChestReward> chestRewards;
};

PacketKind packetKind(const QByteArray &payload);

QByteArray encodeFastNetworkState(const FastNetworkState &state);
bool decodeFastNetworkState(const QByteArray &payload,
                            FastNetworkState &decoded,
                            const QVector<PlayerState> &existingPlayers,
                            int localPlayerId);

QByteArray encodeHudNetworkState(const HudNetworkState &state);
bool decodeHudNetworkState(const QByteArray &payload, HudNetworkState &decoded);

QByteArray encodeInputPacket(qreal horizontal, qreal vertical);
bool decodeInputPacket(const QByteArray &payload, qreal &horizontal, qreal &vertical);

QByteArray encodeChooseLevelUpPacket(const QString &upgradeId);
bool decodeChooseLevelUpPacket(const QByteArray &payload, QString &upgradeId);

QByteArray encodeCloseChestPacket();
bool decodeCloseChestPacket(const QByteArray &payload);

}  // namespace LanBoard::Survivor::NetCodec
