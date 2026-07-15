#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "types.h"

namespace LanBoard {

enum class SeatKind {
    Active = 0,
    Spectator = 1
};

inline SeatKind normalizedSeatKind(QStringView seatType)
{
    return seatType == QStringView(QStringLiteral("spectator"))
        ? SeatKind::Spectator
        : SeatKind::Active;
}

inline QString seatTypeString(SeatKind seatKind)
{
    return seatKind == SeatKind::Spectator
        ? QStringLiteral("spectator")
        : QStringLiteral("active");
}

inline bool isActiveSeat(SeatKind seatKind)
{
    return seatKind == SeatKind::Active;
}

struct RoomPlayerState {
    int playerId = -1;
    QString name;
    bool isHost = false;
    bool isReady = false;
    SeatKind seatKind = SeatKind::Active;
    int piece = 0;
    QString dormDefenseRole = QStringLiteral("defender");

    QString seatType() const
    {
        return seatTypeString(seatKind);
    }

    bool isActive() const
    {
        return isActiveSeat(seatKind);
    }

    QVariantMap toVariantMap() const
    {
        QVariantMap map;
        map[QStringLiteral("playerId")] = playerId;
        map[QStringLiteral("name")] = name;
        map[QStringLiteral("isHost")] = isHost;
        map[QStringLiteral("isReady")] = isReady;
        map[QStringLiteral("seatType")] = seatType();
        map[QStringLiteral("piece")] = piece;
        map[QStringLiteral("dormDefenseRole")] = LanBoard::normalizedDormDefenseRole(dormDefenseRole);
        return map;
    }

    QJsonObject toJsonObject() const
    {
        QJsonObject obj;
        obj[QStringLiteral("playerId")] = playerId;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("isHost")] = isHost;
        obj[QStringLiteral("isReady")] = isReady;
        obj[QStringLiteral("seatType")] = seatType();
        obj[QStringLiteral("piece")] = piece;
        obj[QStringLiteral("dormDefenseRole")] = LanBoard::normalizedDormDefenseRole(dormDefenseRole);
        return obj;
    }

    static RoomPlayerState fromJsonObject(const QJsonObject &obj)
    {
        RoomPlayerState player;
        player.playerId = obj.value(QStringLiteral("playerId")).toInt(-1);
        player.name = obj.value(QStringLiteral("name")).toString();
        player.isHost = obj.value(QStringLiteral("isHost")).toBool();
        player.isReady = obj.value(QStringLiteral("isReady")).toBool();
        player.seatKind = normalizedSeatKind(
            obj.value(QStringLiteral("seatType")).toString(QStringLiteral("active")));
        player.piece = obj.value(QStringLiteral("piece")).toInt();
        player.dormDefenseRole = LanBoard::normalizedDormDefenseRole(
            obj.value(QStringLiteral("dormDefenseRole")).toString(QStringLiteral("defender")));
        return player;
    }
};

struct RoomSnapshot {
    QString roomId;
    QString roomName;
    QString gameId = QStringLiteral("gomoku");
    QString gameName;
    int maxPlayers = LanBoard::maxPlayersForGame(QStringLiteral("gomoku"));
    int roomCapacity = 8;
    int localPlayerId = -1;
    bool gameInProgress = false;
    QVector<RoomPlayerState> players;

    int activePlayerCount() const
    {
        int count = 0;
        for (const RoomPlayerState &player : players) {
            if (player.isActive())
                ++count;
        }
        return count;
    }

    int activeGuestCount() const
    {
        int count = 0;
        for (const RoomPlayerState &player : players) {
            if (!player.isHost && player.isActive())
                ++count;
        }
        return count;
    }

    int firstGuestPlayerId() const
    {
        for (const RoomPlayerState &player : players) {
            if (!player.isHost && player.isActive())
                return player.playerId;
        }
        return -1;
    }

    bool localPlayerIsHost() const
    {
        for (const RoomPlayerState &player : players) {
            if (player.playerId == localPlayerId)
                return player.isHost;
        }
        return false;
    }

    QVector<RoomPlayerState> activePlayers() const
    {
        QVector<RoomPlayerState> active;
        active.reserve(players.size());
        for (const RoomPlayerState &player : players) {
            if (player.isActive())
                active.append(player);
        }
        return active;
    }

    bool allActivePlayersReady() const
    {
        for (const RoomPlayerState &player : players) {
            if (!player.isActive())
                continue;
            if (!player.isReady)
                return false;
        }
        return true;
    }

    int dormDefenseGhostCount() const
    {
        int count = 0;
        for (const RoomPlayerState &player : players) {
            if (player.isActive()
                && LanBoard::normalizedDormDefenseRole(player.dormDefenseRole)
                    == QStringLiteral("ghost")) {
                ++count;
            }
        }
        return count;
    }

    QString dormDefenseRoleForPlayer(int playerId) const
    {
        for (const RoomPlayerState &player : players) {
            if (player.playerId != playerId)
                continue;
            if (!player.isActive())
                return QStringLiteral("spectator");
            return LanBoard::normalizedDormDefenseRole(player.dormDefenseRole);
        }
        return QStringLiteral("spectator");
    }

    bool dormDefenseStartRequirementMet() const
    {
        if (!LanBoard::isDormDefenseGame(gameId))
            return true;

        const int ghostCount = dormDefenseGhostCount();
        if (ghostCount > 1)
            return false;
        if (activePlayerCount() >= LanBoard::maxPlayersForGame(gameId))
            return ghostCount == 1;
        return true;
    }

    bool canStartForLocalHost() const
    {
        if (gameInProgress)
            return false;

        if (!localPlayerIsHost())
            return false;

        if (!hasEnoughActivePlayersToStart(gameId, activePlayerCount())) {
            return false;
        }

        return !requiresReadyForStartForGame(gameId) || allActivePlayersReady();
    }

    QVariantList playerVariantList() const
    {
        QVariantList list;
        list.reserve(players.size());
        for (const RoomPlayerState &player : players)
            list.append(player.toVariantMap());
        return list;
    }

    QVariantList activePlayerVariantList() const
    {
        QVariantList list;
        const QVector<RoomPlayerState> active = activePlayers();
        list.reserve(active.size());
        for (const RoomPlayerState &player : active)
            list.append(player.toVariantMap());
        return list;
    }

    QJsonArray playerJsonArray() const
    {
        QJsonArray array;
        for (const RoomPlayerState &player : players)
            array.append(player.toJsonObject());
        return array;
    }
};

}  // namespace LanBoard
