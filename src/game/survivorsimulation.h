#pragma once

#include <QVector2D>

#include "survivorworld.h"

namespace LanBoard::Survivor {

// ---- 武器/被动索引 ----
enum WeaponType {
    WeaponKnife = 0,
    WeaponOrbitBlade,
    WeaponFireWand,
    WeaponGarlic,
    WeaponCross,
    WeaponSantaWater,
    WeaponCount
};

enum PassiveType {
    PassiveWings = 0,
    PassiveEmptyTome,
    PassiveCandelabrador,
    PassiveAttractorb,
    PassiveHollowHeart,
    PassiveSpinach,
    PassiveCount
};

// ---- 每级升级数据 ----
struct WeaponLevelInfo {
    int damage = 0;
    int cooldownMs = 0;         // 0 = keep existing
    int count = 0;              // projectiles / orbitals / amount
    int pierce = 0;
    int durationMs = 0;
    qreal radiusMult = 1.0;     // worldRuntime radius multiplier
    qreal speedMult = 1.0;      // projectile speed multiplier
    qreal angularSpeedDeg = 0.0; // orbit rotation speed
    const char *description = nullptr;  // upgrade text at this level
};

// 获取某武器各等级的完整升级数据（公有函数，避免数组在头文件中暴露）
const WeaponLevelInfo *weaponLevelTable(WeaponType type);  // returns array[8], index 0 = level 1

// ---- 工具函数 ----
int weaponIndexForId(const QString &upgradeId);
int passiveIndexForId(const QString &upgradeId);

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
