#include "survivorruntime.h"

#include <limits>
#include <QVariantMap>
#include <QtMath>

#include "survivorsimulation.h"

namespace LanBoard::Survivor::Runtime {

void initializeSessionPlayers(MatchState &state,
                              const QVariantList &activePlayers,
                              int localPlayerId)
{
    state.players.clear();

    if (activePlayers.isEmpty()) {
        PlayerState player;
        player.playerId = 0;
        player.local = true;
        player.colorIndex = 0;
        initializePlayerProgression(player);
        state.players.append(player);
        return;
    }

    state.players.reserve(activePlayers.size());
    for (int i = 0; i < activePlayers.size(); ++i) {
        const QVariantMap map = activePlayers.at(i).toMap();
        PlayerState player;
        player.playerId = map.value(QStringLiteral("playerId"), i).toInt();
        player.name = map.value(QStringLiteral("name")).toString();
        player.local = player.playerId == localPlayerId;
        player.colorIndex = i;
        const qreal laneOffset = (static_cast<qreal>(i) - (activePlayers.size() - 1) / 2.0) * 0.08f;
        player.position = QVector2D(static_cast<float>(laneOffset), 0.0f);
        initializePlayerProgression(player);
        state.players.append(player);
    }
}

PlayerState *playerStateById(MatchState &state, int playerId)
{
    for (int i = 0; i < state.players.size(); ++i) {
        if (state.players.at(i).playerId == playerId)
            return &state.players[i];
    }
    return nullptr;
}

const PlayerState *playerStateById(const MatchState &state, int playerId)
{
    for (int i = 0; i < state.players.size(); ++i) {
        if (state.players.at(i).playerId == playerId)
            return &state.players.at(i);
    }
    return nullptr;
}

PlayerState *localPlayerState(MatchState &state, int localPlayerId)
{
    return playerStateById(state, localPlayerId);
}

const PlayerState *localPlayerState(const MatchState &state, int localPlayerId)
{
    return playerStateById(state, localPlayerId);
}

QList<int> livingPlayerIndices(const MatchState &state)
{
    QList<int> result;
    for (int i = 0; i < state.players.size(); ++i) {
        if (state.players.at(i).alive)
            result.append(i);
    }
    return result;
}

int nearestLivingPlayerIndex(const MatchState &state, const QVector2D &position)
{
    int bestIndex = -1;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (int i = 0; i < state.players.size(); ++i) {
        const PlayerState &player = state.players.at(i);
        if (!player.alive)
            continue;
        const qreal distanceSquared = (player.position - position).lengthSquared();
        if (distanceSquared >= bestDistance)
            continue;
        bestDistance = distanceSquared;
        bestIndex = i;
    }
    return bestIndex;
}

QVector2D cameraAnchor(const MatchState &state, int localPlayerId)
{
    if (const PlayerState *localPlayer = localPlayerState(state, localPlayerId)) {
        if (localPlayer->alive)
            return localPlayer->position;
    }

    for (const PlayerState &player : state.players) {
        if (player.alive)
            return player.position;
    }

    if (const PlayerState *localPlayer = localPlayerState(state, localPlayerId))
        return localPlayer->position;

    return state.players.isEmpty() ? QVector2D() : state.players.first().position;
}

QVector2D playerAnchor(const MatchState &state, int localPlayerId)
{
    QVector2D sum;
    int count = 0;
    for (const PlayerState &player : state.players) {
        if (!player.alive)
            continue;
        sum += player.position;
        ++count;
    }
    if (count <= 0)
        return cameraAnchor(state, localPlayerId);
    return sum / static_cast<float>(count);
}

qreal currentDamageMultiplier(const PlayerState &player)
{
    return damageMultiplierForLevel(player.spinachPassiveLevel);
}

qreal currentAreaMultiplier(const PlayerState &player)
{
    return areaMultiplierForLevel(player.candelabradorPassiveLevel);
}

qreal currentCooldownMultiplier(const PlayerState &player)
{
    return cooldownMultiplierForLevel(player.emptyTomePassiveLevel);
}

qreal currentDurationMultiplier(const PlayerState &player)
{
    return durationMultiplierForLevel(player.spellbinderPassiveLevel);
}

qreal currentProjectileSpeedMultiplier(const PlayerState &player)
{
    return projectileSpeedMultiplierForLevel(player.bracerPassiveLevel);
}

qreal currentMoveSpeed(const PlayerState &player)
{
    return BasePlayerMoveSpeed * moveSpeedMultiplierForLevel(player.wingsPassiveLevel);
}

qreal currentMagnetRange(const PlayerState &player)
{
    return magnetRangeForLevel(player.attractorbPassiveLevel);
}

int currentMaxHpValue(const PlayerState &player)
{
    return maxHpValueForLevel(player.hollowHeartPassiveLevel);
}

qreal currentRecoveryPerSecond(const PlayerState &player)
{
    return recoveryPerSecondForLevel(player.pummarolaPassiveLevel);
}

qreal currentLuckMultiplier(const PlayerState &player)
{
    return luckMultiplierForLevel(player.cloverPassiveLevel);
}

void syncPlayerMaxHp(MatchState &state)
{
    for (PlayerState &player : state.players) {
        player.maxHp = currentMaxHpValue(player);
        player.hp = qMin(player.hp, player.maxHp);
        player.alive = player.hp > 0;
    }
}

RenderSnapshot buildRenderSnapshot(const MatchState &state)
{
    RenderSnapshot snapshot;

    snapshot.players.reserve(state.players.size());
    for (const PlayerState &player : state.players) {
        snapshot.players.append({
            player.position.x(),
            player.position.y(),
            player.hp,
            player.maxHp,
            player.alive,
            player.local,
            player.colorIndex,
            player.garlicLevel > 0 ? state.worldRuntime.garlicRadius * currentAreaMultiplier(player) : 0.0,
            player.garlicEvolved
        });
    }

    snapshot.enemies.reserve(state.enemies.size());
    for (const Enemy &enemy : state.enemies) {
        snapshot.enemies.append({
            enemy.id,
            enemy.position.x(),
            enemy.position.y(),
            enemy.radius,
            enemy.hp,
            enemy.maxHp,
            enemy.elite,
            enemy.chestCarrier,
            enemy.kind,
            enemy.hitFlashMs
        });
    }

    for (const PlayerState &player : state.players) {
        if (!player.alive || player.orbitBladeCount <= 0 || player.orbitBladeActiveMs <= 0)
            continue;

        const qreal angleStep = 360.0 / player.orbitBladeCount;
        const qreal orbitRadius = state.worldRuntime.orbitBladeRadius * currentAreaMultiplier(player);
        const qreal orbitalRadius = (0.014f + 0.002f * qMin(3, player.orbitBladeLevel))
            * currentAreaMultiplier(player);

        for (int i = 0; i < player.orbitBladeCount; ++i) {
            const QVector2D offset = rotatedVector(QVector2D(orbitRadius, 0.0f),
                                                   state.worldRuntime.orbitAngleDeg + angleStep * i);
            snapshot.orbitals.append({
                player.position.x() + offset.x(),
                player.position.y() + offset.y(),
                orbitalRadius
            });
        }
    }

    snapshot.projectiles.reserve(state.projectiles.size());
    for (const Projectile &projectile : state.projectiles) {
        snapshot.projectiles.append({
            projectile.position.x(),
            projectile.position.y(),
            projectile.radius,
            projectile.kind
        });
    }

    snapshot.pickups.reserve(state.pickups.size());
    for (const Pickup &pickup : state.pickups) {
        snapshot.pickups.append({
            pickup.position.x(),
            pickup.position.y(),
            pickup.radius,
            pickup.exp,
            pickup.kind
        });
    }

    snapshot.zones.reserve(state.zones.size());
    for (const Zone &zone : state.zones) {
        snapshot.zones.append({
            zone.position.x(),
            zone.position.y(),
            zone.radius,
            zone.lifeMs,
            zone.totalLifeMs,
            zone.kind
        });
    }

    snapshot.damageNumbers.reserve(state.damageNumbers.size());
    for (const DamageNumber &number : state.damageNumbers) {
        snapshot.damageNumbers.append({
            number.position.x(),
            number.position.y(),
            number.amount,
            number.lifeMs,
            number.totalLifeMs,
            number.elite
        });
    }

    return snapshot;
}

}  // namespace LanBoard::Survivor::Runtime
