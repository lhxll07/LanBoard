#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QVector>
#include <QVector2D>

namespace LanBoard::Survivor {

enum EnemyKind {
    BatEnemy = 0,
    ZombieEnemy = 1,
    SkeletonEnemy = 2,
    WerewolfEnemy = 3,
    FlowerEnemy = 4,
    OgreEnemy = 5,
    GiantBatEnemy = 6
};

enum PickupKind {
    BlueGemPickup = 0,
    GreenGemPickup = 1,
    RedGemPickup = 2,
    ChestPickup = 3
};

struct UpgradeChoice {
    QString id;
    QString title;
    QString description;
    QString category;
    int currentLevel = 0;
    int maxLevel = 0;
};

struct ChestReward {
    QString title;
    QString description;
    QString category;
    bool evolved = false;
};

struct Pickup {
    QVector2D position;
    qreal radius = 0.014f;
    int exp = 1;
    int kind = 0;
    int rewardCount = 1;
    bool canEvolve = false;
};

struct Enemy {
    int id = 0;
    QVector2D position;
    qreal radius = 0.025f;
    qreal speed = 0.11f;
    qreal speedScale = 1.0f;
    QVector2D forcedVelocity;
    int forcedMovementMs = 0;
    int hp = 2;
    int maxHp = 2;
    int touchDamage = 8;
    int expReward = 1;
    bool elite = false;
    bool chestCarrier = false;
    int kind = 0;
    qreal knockbackFactor = 1.0f;
    qreal knockbackBonus = 0.0f;
    QHash<int, int> sourceHitCooldownsMs;
    int hitFlashMs = 0;
};

struct Projectile {
    QVector2D position;
    QVector2D velocity;
    qreal radius = 0.012f;
    int sourceId = 0;
    int damage = 1;
    int hitIntervalMs = 0;
    int remainingHits = 1;
    int lifeMs = 800;
    int kind = 0;
    bool returning = false;
    qreal knockback = 0.0f;
    qreal damageVariance = 0.12f;
    QSet<int> hitEnemyIds;
};

struct Zone {
    QVector2D position;
    qreal radius = 0.08f;
    int sourceId = 0;
    int damage = 1;
    int lifeMs = 0;
    int totalLifeMs = 0;
    int tickCooldownMs = 0;
    int tickIntervalMs = 240;
    int kind = 0;
    qreal knockback = 0.0f;
    qreal damageVariance = 0.08f;
};

struct PlayerState {
    int playerId = 0;
    QString name;
    QVector2D position;
    QVector2D moveInput;
    QVector2D facingDirection {1.0f, 0.0f};
    int hp = 100;
    int maxHp = 100;
    qreal contactDamageCarry = 0.0f;
    qreal recoveryCarry = 0.0f;
    int soulEaterHealedHp = 0;
    int soulEaterBonusDamage = 0;
    bool alive = true;
    bool local = false;
    int colorIndex = 0;

    int level = 1;
    int exp = 0;
    int expToNext = 5;
    int pendingLevelUps = 0;

    int attackCooldownMs = 0;
    int attackCooldownBaseMs = 1000;
    int orbitBladeCooldownMs = 0;
    int orbitBladeCooldownBaseMs = 3200;
    int orbitBladeActiveMs = 0;
    int orbitBladeDurationMs = 3100;
    int fireWandCooldownMs = 0;
    int fireWandCooldownBaseMs = 1720;
    int magicWandCooldownMs = 0;
    int magicWandCooldownBaseMs = 1200;
    int garlicCooldownBaseMs = 1300;
    int crossCooldownMs = 0;
    int crossCooldownBaseMs = 1080;
    int santaWaterCooldownMs = 0;
    int santaWaterCooldownBaseMs = 1800;

    int attackDamage = 7;
    int bladeWeaponLevel = 0;
    int projectileCount = 1;
    int projectilePierce = 1;
    int orbitBladeLevel = 0;
    int orbitBladeCount = 0;
    int orbitBladeDamage = 0;
    int fireWandLevel = 0;
    int fireWandDamage = 24;
    int fireWandAmount = 1;
    qreal fireWandProjectileSpeedMultiplier = 1.0f;
    int magicWandLevel = 0;
    int magicWandDamage = 10;
    int magicWandAmount = 1;
    int garlicLevel = 1;
    int garlicDamage = 4;
    int crossLevel = 0;
    int crossDamage = 12;
    int crossAmount = 1;
    int crossPierce = 1000;
    int santaWaterLevel = 0;
    int santaWaterDamage = 10;
    int santaWaterAmount = 1;
    int santaWaterDurationMs = 1800;
    int wingsPassiveLevel = 0;
    int emptyTomePassiveLevel = 0;
    int candelabradorPassiveLevel = 0;
    int attractorbPassiveLevel = 0;
    int hollowHeartPassiveLevel = 0;
    int spinachPassiveLevel = 0;
    int bracerPassiveLevel = 0;
    int spellbinderPassiveLevel = 0;
    int pummarolaPassiveLevel = 0;
    int cloverPassiveLevel = 0;
    bool bladeWeaponEvolved = false;
    bool orbitBladeEvolved = false;
    bool fireWandEvolved = false;
    bool magicWandEvolved = false;
    bool garlicEvolved = false;
    bool crossEvolved = false;
    bool santaWaterEvolved = false;

    QList<UpgradeChoice> levelUpChoices;
    QList<ChestReward> chestRewardEntries;
    QList<Pickup> queuedChests;
    QString chestTitle;
};

struct DamageNumber {
    QVector2D position;
    QVector2D velocity;
    int amount = 0;
    int lifeMs = 0;
    int totalLifeMs = 0;
    bool elite = false;
};

struct WorldRuntimeState {
    qreal projectileSpeed = 1.00f;
    qreal orbitBladeRadius = 0.14f;
    qreal orbitBladeAngularSpeedDeg = 140.0f;
    qreal orbitAngleDeg = 0.0f;
    qreal garlicRadius = 0.10f;
    qreal crossSpeed = 0.82f;
    qreal crossRadius = 0.018f;
    qreal santaWaterRadius = 0.08f;
};

struct MatchState {
    QVector<PlayerState> players;
    QList<Enemy> enemies;
    QList<Projectile> projectiles;
    QList<Pickup> pickups;
    QList<Zone> zones;
    QList<DamageNumber> damageNumbers;
    WorldRuntimeState worldRuntime;

    bool running = false;
    bool gameOver = false;
    bool levelUpPending = false;
    bool chestPending = false;
    int pendingInteractionPlayerId = -1;
    int pendingInteractionElapsedMs = 0;
    int level = 1;
    int exp = 0;
    int expToNext = 5;
    int killCount = 0;
    int survivalTimeMs = 0;
    int tickAccumulatorMs = 0;
    int spawnAccumulatorMs = 0;
    int eliteSpawnAccumulatorMs = 0;
    int nextBossSpawnIndex = 0;
    int nextEnemyId = 1;
    int nextSourceId = 1;
    int lastWaveIndex = -1;
    int nextWaveEventIndex = 0;
    QString chestTitle;
    QString waveLabel;
};

struct RenderPlayer {
    qreal x = 0.0;
    qreal y = 0.0;
    int hp = 0;
    int maxHp = 0;
    bool alive = true;
    bool local = false;
    int colorIndex = 0;
};

struct RenderEnemy {
    int id = 0;
    qreal x = 0.0;
    qreal y = 0.0;
    qreal radius = 0.0;
    int hp = 0;
    int maxHp = 0;
    bool elite = false;
    bool chestCarrier = false;
    int kind = 0;
    int hitFlashMs = 0;
};

struct RenderOrbital {
    qreal x = 0.0;
    qreal y = 0.0;
    qreal radius = 0.0;
};

struct RenderProjectile {
    qreal x = 0.0;
    qreal y = 0.0;
    qreal radius = 0.0;
    int kind = 0;
};

struct RenderPickup {
    qreal x = 0.0;
    qreal y = 0.0;
    qreal radius = 0.0;
    int exp = 0;
    int kind = 0;
};

struct RenderZone {
    qreal x = 0.0;
    qreal y = 0.0;
    qreal radius = 0.0;
    int lifeMs = 0;
    int totalLifeMs = 0;
    int kind = 0;
};

struct RenderDamageNumber {
    qreal x = 0.0;
    qreal y = 0.0;
    int amount = 0;
    int lifeMs = 0;
    int totalLifeMs = 0;
    bool elite = false;
};

struct RenderSnapshot {
    QList<RenderPlayer> players;
    QList<RenderEnemy> enemies;
    QList<RenderOrbital> orbitals;
    QList<RenderProjectile> projectiles;
    QList<RenderPickup> pickups;
    QList<RenderZone> zones;
    QList<RenderDamageNumber> damageNumbers;
};

}  // namespace LanBoard::Survivor
