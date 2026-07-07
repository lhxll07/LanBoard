#pragma once

#include <QVector2D>

#include "survivorworld.h"

namespace LanBoard::Survivor {

// ---- 武器/被动索引 ----
enum WeaponType {
    WeaponKnife = 0,
    WeaponOrbitBlade,
    WeaponFireWand,
    WeaponMagicWand,
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
    PassiveBracer,
    PassiveSpellbinder,
    PassivePummarola,
    PassiveClover,
    PassiveCount
};

// ---- 每级升级数据 ----
struct WeaponLevelInfo {
    int damage = 0;
    int cooldownMs = 0;         // 0 = keep existing
    int count = 0;              // projectiles / orbitals / amount
    int pierce = 0;
    int durationMs = 0;
    qreal radius = 0.0f;        // worldRuntime absolute radius
    qreal speed = 0.0f;         // absolute projectile / weapon speed
    qreal angularSpeedDeg = 0.0; // orbit rotation speed
    const char *description = nullptr;  // upgrade text at this level
};

// 获取某武器各等级的完整升级数据（公有函数，避免数组在头文件中暴露）
const WeaponLevelInfo *weaponLevelTable(WeaponType type);  // returns array[8], index 0 = level 1

struct PassiveLevelInfo {
    const char *description = nullptr;
};

const PassiveLevelInfo *passiveLevelTable(PassiveType type);  // returns array[5], index 0 = level 1

// ---- 工具函数 ----
int weaponIndexForId(const QString &upgradeId);
int passiveIndexForId(const QString &upgradeId);
bool isWeaponUpgradeId(const QString &upgradeId);

struct UpgradeTemplate {
    const char *id;
    const char *title;
    const char *category;
    int maxLevel;
};

const UpgradeTemplate *upgradeTemplateForId(const QString &upgradeId);

struct EvolutionTemplate {
    const char *weaponId;
    const char *requiredPassiveId;
    const char *evolvedTitle;
    const char *evolvedDescription;
    int requiredWeaponLevel;
    int requiredPassiveLevel;
};

const EvolutionTemplate *evolutionTemplateForWeaponId(const QString &weaponId);

enum AttackProfileType {
    AttackProfileKnife = 0,
    AttackProfileFireWand,
    AttackProfileHellfire,
    AttackProfileMagicWand,
    AttackProfileHolyWand,
    AttackProfileCross,
    AttackProfileSantaWater,
    AttackProfileLaBorra,
    AttackProfileGarlic,
    AttackProfileOrbitBlade,
    AttackProfileCount
};

struct WeaponHitProfile {
    qreal radius = 0.0f;
    qreal speedMultiplier = 1.0f;
    qreal knockback = 0.0f;
    qreal damageVariance = 0.0f;
    qreal areaMultiplier = 1.0f;
    qreal damageMultiplier = 1.0f;
    qreal durationMultiplier = 1.0f;
    qreal returnSpeedMultiplier = 1.0f;
    qreal driftSpeed = 0.0f;
    int lifeMs = 0;
    int tickIntervalMs = 0;
    int remainingHits = 1;
};

const WeaponHitProfile &weaponHitProfile(AttackProfileType type);

struct SpawnWeight {
    int kind;
    int weight;
};

enum WaveEventType {
    WaveEventBatSwarm = 0,
    WaveEventFlowerWall
};

struct WaveTemplate {
    const char *label;
    int spawnIntervalMs;
    int spawnBurst;
    int eliteIntervalMs;
    int eliteBurst;
    int enemyCap;
    const SpawnWeight *spawnWeights;
    int spawnWeightCount;
    const SpawnWeight *eliteWeights;
    int eliteWeightCount;
};

struct BossSpawnTemplate {
    int second;
    int kind;
};

struct WaveEventTemplate {
    int second;
    int type;
    int count;
    qreal primaryValue;
    qreal secondaryValue;
    int tertiaryValue;
};

inline constexpr qreal SpawnDistanceMin = 1.10f;
inline constexpr qreal SpawnDistanceMax = 1.65f;
inline constexpr qreal ProjectileCleanupDistance = 4.80f;
inline constexpr qreal ProjectileCleanupDistanceSquared = ProjectileCleanupDistance * ProjectileCleanupDistance;
inline constexpr qreal EnemySeparationCellSize = 0.11f;
inline constexpr int SurvivorRunDurationSec = 900;
inline constexpr int EvolutionChestStartSec = 300;
inline constexpr int BasePlayerMaxHp = 100;
inline constexpr qreal BasePlayerMoveSpeed = 0.34f;
inline constexpr qreal BasePickupMagnetRange = 0.12f;

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
qreal damageMultiplierForLevel(int spinachLevel);
qreal areaMultiplierForLevel(int candelabradorLevel);
qreal cooldownMultiplierForLevel(int emptyTomeLevel);
qreal moveSpeedMultiplierForLevel(int wingsLevel);
qreal magnetRangeForLevel(int attractorbLevel);
int maxHpValueForLevel(int hollowHeartLevel);
qreal projectileSpeedMultiplierForLevel(int bracerLevel);
qreal durationMultiplierForLevel(int spellbinderLevel);
qreal recoveryPerSecondForLevel(int pummarolaLevel);
qreal luckMultiplierForLevel(int cloverLevel);

const UpgradeTemplate *weaponUpgradePool();
int weaponUpgradePoolCount();
const UpgradeTemplate *passiveUpgradePool();
int passiveUpgradePoolCount();
const WaveTemplate *waveTemplates();
int waveTemplateCount();
const BossSpawnTemplate *bossSpawnSchedule();
int bossSpawnScheduleCount();
const WaveEventTemplate *waveEventSchedule();
int waveEventScheduleCount();

}  // namespace LanBoard::Survivor
