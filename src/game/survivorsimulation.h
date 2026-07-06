#pragma once

#include <QVector2D>

#include "survivorworld.h"

namespace LanBoard::Survivor {

struct UpgradeTemplate {
    const char *id;
    const char *title;
    const char *category;
    int maxLevel;
};

struct WaveTemplate {
    const char *label;
    int spawnIntervalMs;
    int spawnBurst;
    int eliteIntervalMs;
    int enemyCap;
};

struct BossSpawnTemplate {
    int second;
    int kind;
};

inline constexpr qreal SpawnDistanceMin = 1.10f;
inline constexpr qreal SpawnDistanceMax = 1.65f;
inline constexpr qreal ProjectileCleanupDistance = 4.80f;
inline constexpr qreal ProjectileCleanupDistanceSquared = ProjectileCleanupDistance * ProjectileCleanupDistance;
inline constexpr qreal EnemySeparationCellSize = 0.11f;
inline constexpr int EvolutionChestStartSec = 210;

QVector2D normalizedInput(const QVector2D &value);
QVector2D rotatedVector(const QVector2D &value, qreal degrees);
int expRequirementForLevel(int level);
quint64 spatialCellKey(int cellX, int cellY);
int pickupKindForExp(int exp);
qreal pickupRadiusForKind(int kind);
bool circlesOverlap(const QVector2D &lhs, const QVector2D &rhs, qreal combinedRadius);
qreal lerpReal(qreal from, qreal to, qreal alpha);
int lerpInt(int from, int to, qreal alpha);
void initializeMatchState(MatchState &state);
void initializeWorldRuntimeState(WorldRuntimeState &state);
void initializePlayerProgression(PlayerState &player);

const UpgradeTemplate *weaponUpgradePool();
int weaponUpgradePoolCount();
const UpgradeTemplate *passiveUpgradePool();
int passiveUpgradePoolCount();
const WaveTemplate *waveTemplates();
int waveTemplateCount();
const BossSpawnTemplate *bossSpawnSchedule();
int bossSpawnScheduleCount();

}  // namespace LanBoard::Survivor
