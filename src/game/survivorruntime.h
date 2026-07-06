#pragma once

#include <QVariantList>
#include <QVector2D>

#include "survivorworld.h"

namespace LanBoard::Survivor::Runtime {

void initializeSessionPlayers(MatchState &state,
                              const QVariantList &activePlayers,
                              int localPlayerId);

PlayerState *playerStateById(MatchState &state, int playerId);
const PlayerState *playerStateById(const MatchState &state, int playerId);
PlayerState *localPlayerState(MatchState &state, int localPlayerId);
const PlayerState *localPlayerState(const MatchState &state, int localPlayerId);
QList<int> livingPlayerIndices(const MatchState &state);
int nearestLivingPlayerIndex(const MatchState &state, const QVector2D &position);
QVector2D cameraAnchor(const MatchState &state, int localPlayerId);
QVector2D playerAnchor(const MatchState &state, int localPlayerId);

qreal currentDamageMultiplier(const PlayerState &player);
qreal currentAreaMultiplier(const PlayerState &player);
qreal currentCooldownMultiplier(const PlayerState &player);
qreal currentDurationMultiplier();
qreal currentProjectileSpeedMultiplier();
qreal currentMoveSpeed(const PlayerState &player);
qreal currentMagnetRange(const PlayerState &player);
int currentMaxHpValue(const PlayerState &player);

void syncPlayerMaxHp(MatchState &state);
RenderSnapshot buildRenderSnapshot(const MatchState &state);

}  // namespace LanBoard::Survivor::Runtime
