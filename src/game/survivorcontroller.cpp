#include "survivorcontroller.h"

#include <algorithm>
#include <QRandomGenerator>
#include <QVariantMap>
#include <QtMath>

namespace {

struct UpgradeTemplate {
    const char *id;
    const char *title;
    const char *category;
    int maxLevel;
};

enum EnemyKind {
    BatEnemy = 0,
    ZombieEnemy = 1,
    SkeletonEnemy = 2,
    WerewolfEnemy = 3,
    FlowerEnemy = 4,
    OgreEnemy = 5,
    GiantBatEnemy = 6
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

constexpr qreal SpawnDistanceMin = 1.10f;
constexpr qreal SpawnDistanceMax = 1.65f;
constexpr qreal ProjectileCleanupDistance = 4.80f;
constexpr qreal EnemySeparationCellSize = 0.11f;
constexpr int EvolutionChestStartSec = 210;

enum PickupKind {
    BlueGemPickup = 0,
    GreenGemPickup = 1,
    RedGemPickup = 2,
    ChestPickup = 3
};

QVector2D normalizedInput(const QVector2D &value)
{
    if (value.lengthSquared() <= 1.0f)
        return value;
    return value.normalized();
}

QVector2D rotatedVector(const QVector2D &value, qreal degrees)
{
    const qreal radians = qDegreesToRadians(degrees);
    const qreal cosValue = qCos(radians);
    const qreal sinValue = qSin(radians);
    return QVector2D(value.x() * cosValue - value.y() * sinValue,
                     value.x() * sinValue + value.y() * cosValue);
}

const UpgradeTemplate kWeaponUpgradePool[] = {
    {"knife_weapon", "飞刀", "武器", 8},
    {"orbit_weapon", "秘典", "武器", 8},
    {"firewand_weapon", "火焰魔杖", "武器", 8},
    {"garlic_weapon", "大蒜", "武器", 8},
    {"cross_weapon", "十字架", "武器", 8},
    {"santawater_weapon", "圣水", "武器", 8}
};

const UpgradeTemplate kPassiveUpgradePool[] = {
    {"wings_passive", "翅膀", "被动", 5},
    {"emptytome_passive", "空白之书", "被动", 5},
    {"candelabrador_passive", "烛台", "被动", 5},
    {"attractorb_passive", "磁力珠", "被动", 5},
    {"hollowheart_passive", "空心心脏", "被动", 5},
    {"spinach_passive", "菠菜", "被动", 5}
};

const WaveTemplate kWaveTemplates[] = {
    {"红眼蝙蝠", 820, 1, 0, 36},
    {"僵尸围拢", 720, 1, 0, 52},
    {"蝙蝠风暴", 440, 2, 0, 76},
    {"骷髅试探", 360, 2, 0, 92},
    {"狼人逼近", 540, 2, 0, 104},
    {"花墙封路", 320, 3, 18000, 136},
    {"经验回合", 260, 4, 16000, 184},
    {"高压混编", 280, 4, 12000, 224},
    {"巨蝠终盘", 250, 4, 10000, 272},
    {"银蝠终盘", 220, 5, 9000, 320}
};

const BossSpawnTemplate kBossSpawnSchedule[] = {
    {120, GiantBatEnemy},
    {180, GiantBatEnemy},
    {300, OgreEnemy},
    {420, GiantBatEnemy},
    {540, GiantBatEnemy},
    {600, OgreEnemy}
};

int expRequirementForLevel(int level)
{
    if (level <= 1)
        return 5;
    if (level < 20)
        return 5 + (level - 1) * 10;
    if (level == 20)
        return 795;
    if (level < 40)
        return 195 + (level - 20) * 13;
    if (level == 40)
        return 2855;
    return 455 + (level - 40) * 16;
}

quint64 spatialCellKey(int cellX, int cellY)
{
    return (static_cast<quint64>(static_cast<quint32>(cellX)) << 32)
        | static_cast<quint32>(cellY);
}

int pickupKindForExp(int exp)
{
    if (exp >= 10)
        return RedGemPickup;
    if (exp >= 3)
        return GreenGemPickup;
    return BlueGemPickup;
}

qreal pickupRadiusForKind(int kind)
{
    switch (kind) {
    case RedGemPickup:
        return 0.019f;
    case GreenGemPickup:
        return 0.016f;
    case BlueGemPickup:
    default:
        return 0.013f;
    }
}

}

SurvivorController::SurvivorController(QObject *parent)
    : QObject(parent)
{
    m_tickTimer.setInterval(TickIntervalMs);
    m_tickTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &SurvivorController::tick);
    resetState();
}

QVariantList SurvivorController::levelUpChoices() const
{
    return m_cachedLevelUpChoices;
}

QVariantList SurvivorController::chestRewards() const
{
    return m_cachedChestRewards;
}

QVariantList SurvivorController::weaponSlots() const
{
    return m_cachedWeaponSlots;
}

QVariantList SurvivorController::passiveSlots() const
{
    return m_cachedPassiveSlots;
}

void SurvivorController::refreshLevelUpChoiceCache()
{
    m_cachedLevelUpChoices.clear();
    m_cachedLevelUpChoices.reserve(m_levelUpChoices.size());
    for (const UpgradeChoice &choice : std::as_const(m_levelUpChoices)) {
        QVariantMap map;
        map[QStringLiteral("id")] = choice.id;
        map[QStringLiteral("title")] = choice.title;
        map[QStringLiteral("description")] = choice.description;
        map[QStringLiteral("category")] = choice.category;
        map[QStringLiteral("currentLevel")] = choice.currentLevel;
        map[QStringLiteral("maxLevel")] = choice.maxLevel;
        m_cachedLevelUpChoices.append(map);
    }
}

void SurvivorController::refreshChestRewardCache()
{
    m_cachedChestRewards.clear();
    m_cachedChestRewards.reserve(m_chestRewardEntries.size());
    for (const ChestReward &reward : std::as_const(m_chestRewardEntries)) {
        QVariantMap map;
        map[QStringLiteral("title")] = reward.title;
        map[QStringLiteral("description")] = reward.description;
        map[QStringLiteral("category")] = reward.category;
        map[QStringLiteral("evolved")] = reward.evolved;
        m_cachedChestRewards.append(map);
    }
}

void SurvivorController::refreshWeaponSlotCache()
{
    m_cachedWeaponSlots.clear();

    auto appendWeapon = [this](const QString &title,
                               const QString &subtitle,
                               bool filled,
                               const QString &accent) {
        QVariantMap slot;
        slot[QStringLiteral("title")] = title;
        slot[QStringLiteral("subtitle")] = subtitle;
        slot[QStringLiteral("filled")] = filled;
        slot[QStringLiteral("accent")] = accent;
        m_cachedWeaponSlots.append(slot);
    };

    appendWeapon(QStringLiteral("飞刀"),
                 m_bladeWeaponLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 伤害 %2 / 数量 %3 / 穿透 %4")
                          .arg(m_bladeWeaponLevel)
                          .arg(qRound(m_attackDamage * currentDamageMultiplier()))
                          .arg(m_projectileCount)
                          .arg(m_projectilePierce)
                    : QStringLiteral("未解锁"),
                 m_bladeWeaponLevel > 0,
                 QStringLiteral("#F6D782"));

    appendWeapon(QStringLiteral("秘典"),
                 m_orbitBladeLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 环刃 %2 / 伤害 %3")
                          .arg(m_orbitBladeLevel)
                          .arg(m_orbitBladeCount)
                          .arg(qRound(m_orbitBladeDamage * currentDamageMultiplier()))
                    : QStringLiteral("未解锁"),
                 m_orbitBladeLevel > 0,
                 QStringLiteral("#B4E0D2"));

    appendWeapon(m_fireWandEvolved ? QStringLiteral("地狱火") : QStringLiteral("火杖"),
                 m_fireWandLevel > 0
                    ? QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 弹速 x%4")
                          .arg(m_fireWandEvolved ? QStringLiteral("已进化 · ") : QString())
                          .arg(m_fireWandLevel)
                          .arg(qRound(m_fireWandDamage * currentDamageMultiplier()))
                          .arg(QString::number(m_fireWandProjectileSpeedMultiplier, 'f', 2))
                    : QStringLiteral("未解锁"),
                 m_fireWandLevel > 0,
                 QStringLiteral("#E98B61"));

    appendWeapon(QStringLiteral("大蒜"),
                 m_garlicLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 伤害 %2 / 半径 %3")
                          .arg(m_garlicLevel)
                          .arg(qRound(m_garlicDamage * currentDamageMultiplier()))
                          .arg(QString::number(m_garlicRadius * currentAreaMultiplier(), 'f', 2))
                    : QStringLiteral("未解锁"),
                 m_garlicLevel > 0,
                 QStringLiteral("#D8F0B5"));

    appendWeapon(QStringLiteral("十字架"),
                 m_crossLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 伤害 %2 / 数量 %3")
                          .arg(m_crossLevel)
                          .arg(qRound(m_crossDamage * currentDamageMultiplier()))
                          .arg(m_crossAmount)
                    : QStringLiteral("未解锁"),
                 m_crossLevel > 0,
                 QStringLiteral("#EFD7A6"));

    appendWeapon(m_santaWaterEvolved ? QStringLiteral("黑波拉") : QStringLiteral("圣水"),
                 m_santaWaterLevel > 0
                    ? QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4")
                          .arg(m_santaWaterEvolved ? QStringLiteral("已进化 · ") : QString())
                          .arg(m_santaWaterLevel)
                          .arg(qRound(m_santaWaterDamage * currentDamageMultiplier()))
                          .arg(m_santaWaterAmount)
                    : QStringLiteral("未解锁"),
                 m_santaWaterLevel > 0,
                 QStringLiteral("#86AAF6"));

    while (m_cachedWeaponSlots.size() < 6) {
        QVariantMap emptySlot;
        emptySlot[QStringLiteral("title")] = QStringLiteral("空槽");
        emptySlot[QStringLiteral("subtitle")] = QStringLiteral("后续武器位");
        emptySlot[QStringLiteral("filled")] = false;
        emptySlot[QStringLiteral("accent")] = QStringLiteral("#4A655C");
        m_cachedWeaponSlots.append(emptySlot);
    }
}

void SurvivorController::refreshPassiveSlotCache()
{
    m_cachedPassiveSlots.clear();

    auto appendPassive = [this](const QString &title,
                                const QString &subtitle,
                                const QString &accent) {
        QVariantMap slot;
        slot[QStringLiteral("title")] = title;
        slot[QStringLiteral("subtitle")] = subtitle;
        slot[QStringLiteral("filled")] = true;
        slot[QStringLiteral("accent")] = accent;
        m_cachedPassiveSlots.append(slot);
    };

    if (m_wingsPassiveLevel > 0) {
        appendPassive(QStringLiteral("翅膀"),
                      QStringLiteral("Lv.%1/5 · 移速 %2").arg(m_wingsPassiveLevel).arg(QString::number(currentMoveSpeed(), 'f', 2)),
                      QStringLiteral("#D7B7F2"));
    }
    if (m_emptyTomePassiveLevel > 0) {
        appendPassive(QStringLiteral("空白之书"),
                      QStringLiteral("Lv.%1/5 · 冷却 x%2").arg(m_emptyTomePassiveLevel).arg(QString::number(currentCooldownMultiplier(), 'f', 2)),
                      QStringLiteral("#E7C96C"));
    }
    if (m_candelabradorPassiveLevel > 0) {
        appendPassive(QStringLiteral("烛台"),
                      QStringLiteral("Lv.%1/5 · 范围 x%2").arg(m_candelabradorPassiveLevel).arg(QString::number(currentAreaMultiplier(), 'f', 2)),
                      QStringLiteral("#F3D48A"));
    }
    if (m_attractorbPassiveLevel > 0) {
        appendPassive(QStringLiteral("磁力珠"),
                      QStringLiteral("Lv.%1/5 · 吸附 %2").arg(m_attractorbPassiveLevel).arg(QString::number(currentMagnetRange(), 'f', 2)),
                      QStringLiteral("#86D9C7"));
    }
    if (m_hollowHeartPassiveLevel > 0) {
        appendPassive(QStringLiteral("空心心脏"),
                      QStringLiteral("Lv.%1/5 · 生命 %2").arg(m_hollowHeartPassiveLevel).arg(currentMaxHpValue()),
                      QStringLiteral("#E48D81"));
    }
    if (m_spinachPassiveLevel > 0) {
        appendPassive(QStringLiteral("菠菜"),
                      QStringLiteral("Lv.%1/5 · 伤害 x%2").arg(m_spinachPassiveLevel).arg(QString::number(currentDamageMultiplier(), 'f', 2)),
                      QStringLiteral("#8ECF74"));
    }

    while (m_cachedPassiveSlots.size() < 6) {
        QVariantMap emptySlot;
        emptySlot[QStringLiteral("title")] = QStringLiteral("空槽");
        emptySlot[QStringLiteral("subtitle")] = QStringLiteral("后续被动位");
        emptySlot[QStringLiteral("filled")] = false;
        emptySlot[QStringLiteral("accent")] = QStringLiteral("#4A655C");
        m_cachedPassiveSlots.append(emptySlot);
    }
}

void SurvivorController::refreshHudSlotCaches()
{
    refreshWeaponSlotCache();
    refreshPassiveSlotCache();
}

void SurvivorController::startRun(bool networkPrototype)
{
    resetState();
    m_networkPrototype = networkPrototype;
    m_running = true;
    updateStatusText();
    m_frameTimer.start();
    m_tickTimer.start();
    refreshFrameCache();
    emit runningChanged();
    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::stopRun()
{
    if (!m_running && !m_gameOver)
        return;

    m_tickTimer.stop();
    m_running = false;
    m_gameOver = false;
    m_levelUpPending = false;
    m_chestPending = false;
    m_pendingLevelUps = 0;
    m_levelUpChoices.clear();
    m_chestRewardEntries.clear();
    m_queuedChests.clear();
    m_chestTitle.clear();
    refreshLevelUpChoiceCache();
    refreshChestRewardCache();
    updateStatusText();
    refreshFrameCache();
    emit runningChanged();
    emit gameOverChanged();
    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::setMoveInput(qreal horizontal, qreal vertical)
{
    m_moveInput = normalizedInput(QVector2D(horizontal, vertical));
    if (!m_moveInput.isNull())
        m_facingDirection = m_moveInput;
}

void SurvivorController::chooseLevelUp(const QString &upgradeId)
{
    if (!m_levelUpPending || upgradeId.trimmed().isEmpty())
        return;

    bool found = false;
    for (const UpgradeChoice &choice : std::as_const(m_levelUpChoices)) {
        if (choice.id == upgradeId) {
            found = true;
            break;
        }
    }
    if (!found)
        return;

    applyUpgrade(upgradeId);
    if (m_pendingLevelUps > 0)
        --m_pendingLevelUps;

    m_levelUpPending = false;
    m_levelUpChoices.clear();
    if (m_pendingLevelUps > 0) {
        prepareLevelUpChoices();
    } else {
        updateStatusText();
        tryOpenQueuedChest();
    }

    refreshLevelUpChoiceCache();
    m_frameTimer.restart();
    refreshFrameCache();
    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::closeChestRewards()
{
    if (!m_chestPending)
        return;

    m_chestPending = false;
    m_chestRewardEntries.clear();
    m_chestTitle.clear();
    refreshChestRewardCache();

    if (m_pendingLevelUps > 0) {
        prepareLevelUpChoices();
    } else {
        tryOpenQueuedChest();
        if (!m_chestPending)
            updateStatusText();
    }

    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::resetState()
{
    m_tickTimer.stop();
    m_playerPos = QVector2D(0.0f, 0.0f);
    m_moveInput = QVector2D(0.0f, 0.0f);
    m_facingDirection = QVector2D(1.0f, 0.0f);
    m_enemies.clear();
    m_projectiles.clear();
    m_pickups.clear();
    m_zones.clear();
    m_damageNumbers.clear();
    m_levelUpChoices.clear();
    m_chestRewardEntries.clear();
    m_queuedChests.clear();
    m_running = false;
    m_gameOver = false;
    m_networkPrototype = false;
    m_levelUpPending = false;
    m_chestPending = false;
    m_hp = 100;
    m_maxHp = 100;
    m_level = 1;
    m_exp = 0;
    m_expToNext = expRequirementForLevel(m_level);
    m_killCount = 0;
    m_pendingLevelUps = 0;
    m_survivalTimeMs = 0;
    m_tickAccumulatorMs = 0;
    m_spawnAccumulatorMs = 0;
    m_eliteSpawnAccumulatorMs = 0;
    m_spawnedBossCount = 0;
    m_nextEnemyId = 1;
    m_contactDamageCarry = 0.0;
    m_attackCooldownMs = 0;
    m_orbitBladeCooldownMs = 0;
    m_orbitBladeActiveMs = 0;
    m_fireWandCooldownMs = 0;
    m_garlicCooldownBaseMs = 1300;
    m_crossCooldownMs = 0;
    m_santaWaterCooldownMs = 0;
    m_attackCooldownBaseMs = 1000;
    m_orbitBladeCooldownBaseMs = 3200;
    m_orbitBladeDurationMs = 3100;
    m_fireWandCooldownBaseMs = 1200;
    m_crossCooldownBaseMs = 1080;
    m_santaWaterCooldownBaseMs = 1260;
    m_attackDamage = 7;
    m_bladeWeaponLevel = 0;
    m_projectileCount = 1;
    m_projectilePierce = 1;
    m_orbitBladeLevel = 0;
    m_orbitBladeCount = 0;
    m_orbitBladeDamage = 0;
    m_fireWandLevel = 0;
    m_fireWandDamage = 18;
    m_fireWandProjectileSpeedMultiplier = 1.0f;
    m_garlicLevel = 1;
    m_garlicDamage = 4;
    m_crossLevel = 0;
    m_crossDamage = 12;
    m_crossAmount = 1;
    m_crossPierce = 1000;
    m_santaWaterLevel = 0;
    m_santaWaterDamage = 10;
    m_santaWaterAmount = 1;
    m_santaWaterDurationMs = 1400;
    m_wingsPassiveLevel = 0;
    m_emptyTomePassiveLevel = 0;
    m_candelabradorPassiveLevel = 0;
    m_attractorbPassiveLevel = 0;
    m_hollowHeartPassiveLevel = 0;
    m_spinachPassiveLevel = 0;
    m_fireWandEvolved = false;
    m_santaWaterEvolved = false;
    m_lastWaveIndex = -1;
    m_triggeredEventSeconds.clear();
    m_moveSpeed = 0.34f;
    m_pickupRange = 0.12f;
    m_projectileSpeed = 1.00f;
    m_orbitBladeRadius = 0.14f;
    m_orbitBladeAngularSpeedDeg = 140.0f;
    m_orbitAngleDeg = 0.0f;
    m_fireWandRange = 4.20f;
    m_garlicRadius = 0.10f;
    m_crossSpeed = 0.82f;
    m_crossRadius = 0.018f;
    m_santaWaterRadius = 0.08f;
    m_renderSnapshot = {};
    m_chestTitle.clear();
    m_frameTimer.invalidate();
    refreshDerivedStats();
    refreshWaveLabel();
    for (int i = 0; i < 10; ++i)
        spawnEnemy();
    refreshUpgradeSummary();
    refreshHudSlotCaches();
    refreshLevelUpChoiceCache();
    refreshChestRewardCache();
    updateStatusText();
    refreshFrameCache();
}

void SurvivorController::spawnEnemy(bool elite, int forcedKind, bool forceChestCarrier)
{
    Enemy enemy;
    enemy.id = m_nextEnemyId++;
    const qreal spawnAngle = QRandomGenerator::global()->generateDouble() * 360.0;
    const qreal spawnDistance = SpawnDistanceMin
        + QRandomGenerator::global()->generateDouble() * (SpawnDistanceMax - SpawnDistanceMin);
    enemy.position = m_playerPos + rotatedVector(QVector2D(spawnDistance, 0.0f), spawnAngle);
    enemy.kind = forcedKind >= 0 ? forcedKind : currentEnemyKind();
    enemy.elite = elite;
    enemy.chestCarrier = forceChestCarrier;
    int baseHp = 0;
    int baseTouchDamage = 0;
    int baseExpReward = 1;
    switch (enemy.kind) {
    case BatEnemy:
        enemy.radius = enemy.chestCarrier ? 0.046f : 0.022f;
        enemy.speed = enemy.chestCarrier ? 0.185f : 0.165f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.18f : 1.0f;
        baseHp = enemy.chestCarrier ? 60 : 1;
        baseTouchDamage = enemy.chestCarrier ? 14 : 4;
        baseExpReward = 1;
        break;
    case ZombieEnemy:
        enemy.radius = enemy.chestCarrier ? 0.052f : 0.028f;
        enemy.speed = enemy.chestCarrier ? 0.110f : 0.075f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.14f : 0.82f;
        baseHp = enemy.chestCarrier ? 80 : 10;
        baseTouchDamage = enemy.chestCarrier ? 16 : 5;
        baseExpReward = 1;
        break;
    case SkeletonEnemy:
        enemy.radius = enemy.chestCarrier ? 0.048f : 0.026f;
        enemy.speed = enemy.chestCarrier ? 0.145f : 0.102f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.12f : 1.0f;
        baseHp = enemy.chestCarrier ? 120 : 20;
        baseTouchDamage = enemy.chestCarrier ? 18 : 6;
        baseExpReward = 2;
        break;
    case WerewolfEnemy:
        enemy.radius = enemy.chestCarrier ? 0.064f : 0.032f;
        enemy.speed = enemy.chestCarrier ? 0.165f : 0.128f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.10f : 0.38f;
        baseHp = enemy.chestCarrier ? 240 : 90;
        baseTouchDamage = enemy.chestCarrier ? 20 : 9;
        baseExpReward = 3;
        break;
    case FlowerEnemy:
        enemy.radius = enemy.chestCarrier ? 0.062f : 0.036f;
        enemy.speed = enemy.chestCarrier ? 0.080f : 0.048f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.10f : 0.34f;
        baseHp = enemy.chestCarrier ? 180 : 80;
        baseTouchDamage = enemy.chestCarrier ? 18 : 7;
        baseExpReward = 2;
        break;
    case OgreEnemy:
        enemy.radius = enemy.chestCarrier ? 0.068f : 0.04f;
        enemy.speed = enemy.chestCarrier ? 0.108f : 0.082f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.08f : 0.24f;
        baseHp = enemy.chestCarrier ? 420 : 160;
        baseTouchDamage = enemy.chestCarrier ? 24 : 10;
        baseExpReward = 3;
        break;
    case GiantBatEnemy:
    default:
        enemy.radius = enemy.chestCarrier ? 0.078f : 0.058f;
        enemy.speed = enemy.chestCarrier ? 0.142f : 0.122f;
        enemy.knockbackFactor = enemy.chestCarrier ? 0.06f : 0.16f;
        baseHp = enemy.chestCarrier ? 900 : 220;
        baseTouchDamage = enemy.chestCarrier ? 26 : 12;
        baseExpReward = 4;
        break;
    }

    enemy.speedScale = 1.0f;
    if (enemy.chestCarrier) {
        enemy.maxHp = baseHp;
        enemy.touchDamage = baseTouchDamage;
        enemy.expReward = 0;
        enemy.elite = true;
    } else if (enemy.elite) {
        enemy.maxHp = qMax(8, qRound(baseHp * 2.5));
        enemy.touchDamage = qRound(baseTouchDamage * 1.4);
        enemy.expReward = qMin(9, qMax(baseExpReward + 2, baseExpReward * 3));
        enemy.radius *= 1.12f;
        enemy.speed *= 1.06f;
        enemy.knockbackFactor *= 0.7f;
    } else {
        enemy.maxHp = baseHp;
        enemy.touchDamage = baseTouchDamage;
        enemy.expReward = baseExpReward;
    }
    enemy.hp = enemy.maxHp;
    m_enemies.append(enemy);
}

void SurvivorController::tick()
{
    if (!m_running || m_gameOver)
        return;
    if (m_levelUpPending || m_chestPending)
        return;

    if (!m_frameTimer.isValid())
        m_frameTimer.start();

    const int previousHp = m_hp;
    const int previousMaxHp = m_maxHp;
    const int previousLevel = m_level;
    const int previousExp = m_exp;
    const int previousExpToNext = m_expToNext;
    const int previousKillCount = m_killCount;
    const int previousSurvivalSec = survivalTimeSec();
    const bool previousLevelUpPending = m_levelUpPending;
    const bool previousChestPending = m_chestPending;
    const QString previousChestTitle = m_chestTitle;
    const QString previousWaveLabel = m_waveLabel;
    const QString previousStatusText = m_statusText;
    const QString previousUpgradeSummary = m_upgradeSummary;

    static constexpr int MaxFrameElapsedMs = 250;
    static constexpr int MaxCatchUpSteps = 6;
    const int realElapsedMs = qBound(1, static_cast<int>(m_frameTimer.restart()), MaxFrameElapsedMs);
    m_tickAccumulatorMs += realElapsedMs;

    int simulatedSteps = 0;
    while (m_tickAccumulatorMs >= TickIntervalMs && simulatedSteps < MaxCatchUpSteps && !m_gameOver) {
        simulateStep(TickIntervalMs);
        m_tickAccumulatorMs -= TickIntervalMs;
        ++simulatedSteps;
    }

    if (m_tickAccumulatorMs > TickIntervalMs * 2)
        m_tickAccumulatorMs = TickIntervalMs * 2;

    if (simulatedSteps <= 0)
        return;

    refreshFrameCache();
    if (m_gameOver) {
        emit runningChanged();
        emit gameOverChanged();
    }
    const bool shouldEmitStateChanged =
        previousHp != m_hp
        || previousMaxHp != m_maxHp
        || previousLevel != m_level
        || previousExp != m_exp
        || previousExpToNext != m_expToNext
        || previousKillCount != m_killCount
        || previousSurvivalSec != survivalTimeSec()
        || previousLevelUpPending != m_levelUpPending
        || previousChestPending != m_chestPending
        || previousChestTitle != m_chestTitle
        || previousWaveLabel != m_waveLabel
        || previousStatusText != m_statusText
        || previousUpgradeSummary != m_upgradeSummary;
    if (shouldEmitStateChanged)
        emit stateChanged();
    emit frameChanged();
}

void SurvivorController::simulateStep(int elapsedMs)
{
    const qreal deltaSec = elapsedMs / 1000.0;
    m_survivalTimeMs += elapsedMs;
    m_spawnAccumulatorMs += elapsedMs;
    m_eliteSpawnAccumulatorMs += elapsedMs;
    m_attackCooldownMs = qMax(0, m_attackCooldownMs - elapsedMs);
    m_orbitBladeCooldownMs = qMax(0, m_orbitBladeCooldownMs - elapsedMs);
    m_orbitBladeActiveMs = qMax(0, m_orbitBladeActiveMs - elapsedMs);
    m_fireWandCooldownMs = qMax(0, m_fireWandCooldownMs - elapsedMs);
    m_crossCooldownMs = qMax(0, m_crossCooldownMs - elapsedMs);
    m_santaWaterCooldownMs = qMax(0, m_santaWaterCooldownMs - elapsedMs);
    refreshWaveLabel();
    triggerWaveEvents();

    for (int i = m_damageNumbers.size() - 1; i >= 0; --i) {
        DamageNumber &number = m_damageNumbers[i];
        number.position += number.velocity * static_cast<float>(deltaSec);
        number.lifeMs -= elapsedMs;
        if (number.lifeMs <= 0)
            m_damageNumbers.removeAt(i);
    }

    const QVector2D direction = normalizedInput(m_moveInput);
    if (!direction.isNull()) {
        m_facingDirection = direction;
        m_playerPos += direction * static_cast<float>(currentMoveSpeed() * deltaSec);
    }

    qreal contactDamage = 0.0;
    for (Enemy &enemy : m_enemies) {
        decayEnemyHitCooldowns(enemy, elapsedMs);
        enemy.hitFlashMs = qMax(0, enemy.hitFlashMs - elapsedMs);
        enemy.speedScale += (1.0f - enemy.speedScale) * qMin<qreal>(1.0, deltaSec * 7.5);
        if (enemy.forcedMovementMs > 0) {
            enemy.forcedMovementMs = qMax(0, enemy.forcedMovementMs - elapsedMs);
            enemy.position += enemy.forcedVelocity * static_cast<float>(deltaSec);
        } else {
            QVector2D toPlayer = m_playerPos - enemy.position;
            if (toPlayer.lengthSquared() > 0.0001f)
                enemy.position += toPlayer.normalized() * static_cast<float>(enemy.speed * enemy.speedScale * deltaSec);
        }

        const qreal collisionRadius = enemy.radius + 0.022;
        if ((enemy.position - m_playerPos).length() <= collisionRadius)
            contactDamage += enemy.touchDamage * deltaSec;
    }

    m_contactDamageCarry += contactDamage;
    const int appliedContactDamage = static_cast<int>(m_contactDamageCarry);
    if (appliedContactDamage > 0) {
        m_contactDamageCarry -= appliedContactDamage;
        m_hp = qMax(0, m_hp - appliedContactDamage);
        if (m_hp == 0) {
            m_gameOver = true;
            m_running = false;
            m_tickTimer.stop();
            updateStatusText();
            return;
        }
    }

    resolveEnemySeparation();

    const int spawnIntervalMs = currentSpawnIntervalMs();
    while (m_spawnAccumulatorMs >= spawnIntervalMs) {
        m_spawnAccumulatorMs -= spawnIntervalMs;
        int spawnBurst = currentSpawnBurstCount();
        if (m_enemies.size() > currentEnemyCap())
            spawnBurst = qMax(1, spawnBurst - 1);
        if (m_enemies.size() > currentEnemyCap() * 3 / 2 && QRandomGenerator::global()->bounded(100) < 70)
            spawnBurst = 0;
        for (int spawnIndex = 0; spawnIndex < spawnBurst; ++spawnIndex)
            spawnEnemy(false);
    }

    const int eliteSpawnIntervalMs = currentEliteSpawnIntervalMs();
    if (eliteSpawnIntervalMs > 0 && m_eliteSpawnAccumulatorMs >= eliteSpawnIntervalMs) {
        m_eliteSpawnAccumulatorMs = 0;
        const int eliteCount = m_enemies.size() > currentEnemyCap() * 4 / 3 ? 1 : currentSpawnBurstCount();
        for (int i = 0; i < eliteCount; ++i)
            spawnEnemy(true, currentEliteKind(), false);
    }

    if (m_spawnedBossCount < static_cast<int>(std::size(kBossSpawnSchedule))
        && survivalTimeSec() >= kBossSpawnSchedule[m_spawnedBossCount].second) {
        spawnEnemy(true, kBossSpawnSchedule[m_spawnedBossCount].kind, true);
        ++m_spawnedBossCount;
    }

    applyAutoAttack();
    updateGarlicAura();
    updateOrbitals(deltaSec, elapsedMs);
    updateProjectiles(deltaSec, elapsedMs);
    updateZones(deltaSec, elapsedMs);
    collectPickups(deltaSec);
}

void SurvivorController::applyAutoAttack()
{
    if (m_enemies.isEmpty())
        return;

    const qreal damageMultiplier = currentDamageMultiplier();
    const qreal cooldownMultiplier = currentCooldownMultiplier();
    const qreal projectileSpeedMultiplier = currentProjectileSpeedMultiplier();

    if (m_bladeWeaponLevel > 0 && m_attackCooldownMs <= 0) {
        m_attackCooldownMs = qMax(120, qRound(m_attackCooldownBaseMs * cooldownMultiplier));
        QVector2D fireDirection = m_facingDirection.lengthSquared() > 0.0001f
            ? m_facingDirection.normalized()
            : QVector2D(1.0f, 0.0f);
        const int knifeDamage = qMax(1, qRound(m_attackDamage * damageMultiplier));
        const qreal centerOffset = (m_projectileCount - 1) / 2.0;
        for (int i = 0; i < m_projectileCount; ++i) {
            const qreal spreadDegrees = (static_cast<qreal>(i) - centerOffset) * 8.0;
            Projectile projectile;
            projectile.kind = 0;
            projectile.sourceId = m_nextSourceId++;
            projectile.position = m_playerPos + fireDirection * 0.02f;
            projectile.velocity = rotatedVector(fireDirection, spreadDegrees) * static_cast<float>(m_projectileSpeed * projectileSpeedMultiplier);
            projectile.damage = knifeDamage;
            projectile.hitIntervalMs = 1000000;
            projectile.remainingHits = m_projectilePierce;
            projectile.lifeMs = 1680 + (m_projectilePierce - 1) * 180;
            projectile.radius = 0.012f + qMin<qreal>(0.006f, 0.0015f * m_projectilePierce);
            projectile.knockback = 0.040f;
            projectile.damageVariance = 0.18f;
            m_projectiles.append(projectile);
        }
    }

    if (m_fireWandLevel > 0 && m_fireWandCooldownMs <= 0) {
        const QList<int> eligibleTargets = [this]() {
            QList<int> indices;
            indices.reserve(m_enemies.size());
            for (int i = 0; i < m_enemies.size(); ++i)
                indices.append(i);
            return indices;
        }();

        if (!eligibleTargets.isEmpty()) {
            m_fireWandCooldownMs = qMax(320, qRound(m_fireWandCooldownBaseMs * cooldownMultiplier));
            const int bestIndex = eligibleTargets.at(QRandomGenerator::global()->bounded(eligibleTargets.size()));
            QVector2D fireDirection = m_enemies.at(bestIndex).position - m_playerPos;
            if (fireDirection.lengthSquared() <= 0.0001f)
                fireDirection = QVector2D(1.0f, 0.0f);
            fireDirection.normalize();

            Projectile projectile;
            projectile.kind = m_fireWandEvolved ? 3 : 1;
            projectile.sourceId = m_nextSourceId++;
            projectile.position = m_playerPos + fireDirection * 0.024f;
            projectile.velocity = fireDirection * static_cast<float>(m_projectileSpeed
                                                                      * (m_fireWandEvolved ? 1.12f : 0.92f)
                                                                      * projectileSpeedMultiplier
                                                                      * m_fireWandProjectileSpeedMultiplier);
            projectile.damage = qMax(1, qRound(m_fireWandDamage * damageMultiplier * (m_fireWandEvolved ? 1.45f : 1.0f)));
            projectile.hitIntervalMs = 1000000;
            projectile.remainingHits = m_fireWandEvolved ? 999 : 1;
            projectile.lifeMs = m_fireWandEvolved ? 3400 : 2400;
            projectile.radius = m_fireWandEvolved ? 0.021f : 0.017f;
            projectile.knockback = m_fireWandEvolved ? 0.072f : 0.048f;
            projectile.damageVariance = m_fireWandEvolved ? 0.10f : 0.16f;
            m_projectiles.append(projectile);
        }
    }

    if (m_crossLevel > 0 && m_crossCooldownMs <= 0) {
        QList<int> targetIndices;
        for (int i = 0; i < m_enemies.size(); ++i)
            targetIndices.append(i);
        std::sort(targetIndices.begin(), targetIndices.end(), [this](int lhs, int rhs) {
            return (m_enemies.at(lhs).position - m_playerPos).lengthSquared()
                < (m_enemies.at(rhs).position - m_playerPos).lengthSquared();
        });

        const int crossShots = qMin(m_crossAmount, targetIndices.size());
        if (crossShots > 0) {
            m_crossCooldownMs = qMax(260, qRound(m_crossCooldownBaseMs * cooldownMultiplier));
            const int crossDamage = qMax(1, qRound(m_crossDamage * damageMultiplier));
            for (int i = 0; i < crossShots; ++i) {
                QVector2D crossDirection = m_enemies.at(targetIndices.at(i)).position - m_playerPos;
                if (crossDirection.lengthSquared() <= 0.0001f)
                    crossDirection = rotatedVector(QVector2D(1.0f, 0.0f), i * 18.0);
                crossDirection.normalize();

                Projectile crossProjectile;
                crossProjectile.kind = 2;
                crossProjectile.sourceId = m_nextSourceId++;
                crossProjectile.position = m_playerPos + crossDirection * 0.024f;
                crossProjectile.velocity = crossDirection * static_cast<float>(m_crossSpeed);
                crossProjectile.damage = crossDamage;
                crossProjectile.hitIntervalMs = 1000000;
                crossProjectile.remainingHits = m_crossPierce;
                crossProjectile.lifeMs = 1620;
                crossProjectile.radius = m_crossRadius * currentAreaMultiplier();
                crossProjectile.returning = false;
                crossProjectile.knockback = 0.060f;
                crossProjectile.damageVariance = 0.12f;
                m_projectiles.append(crossProjectile);
            }
        }
    }

    if (m_santaWaterLevel > 0 && m_santaWaterCooldownMs <= 0) {
        m_santaWaterCooldownMs = qMax(340, qRound(m_santaWaterCooldownBaseMs * cooldownMultiplier));
        const int zoneDamage = qMax(1, qRound(m_santaWaterDamage * damageMultiplier * (m_santaWaterEvolved ? 1.35f : 1.0f)));
        const qreal zoneRadius = m_santaWaterRadius * currentAreaMultiplier() * (m_santaWaterEvolved ? 1.12f : 1.0f);
        for (int i = 0; i < m_santaWaterAmount; ++i) {
            const qreal angle = QRandomGenerator::global()->generateDouble() * 360.0;
            const qreal distance = m_santaWaterEvolved
                ? 0.32 + QRandomGenerator::global()->generateDouble() * 0.42
                : 0.16 + QRandomGenerator::global()->generateDouble() * 0.30;
            Zone zone;
            zone.kind = m_santaWaterEvolved ? 1 : 0;
            zone.sourceId = m_nextSourceId++;
            zone.position = m_playerPos + rotatedVector(QVector2D(distance, 0.0f), angle);
            zone.radius = zoneRadius;
            zone.damage = zoneDamage;
            zone.totalLifeMs = qRound(m_santaWaterDurationMs * currentDurationMultiplier() * (m_santaWaterEvolved ? 1.22f : 1.0f));
            zone.lifeMs = zone.totalLifeMs;
            zone.tickIntervalMs = qMax(120, qRound((m_santaWaterEvolved ? 190 : 250) * cooldownMultiplier));
            zone.tickCooldownMs = 0;
            zone.knockback = m_santaWaterEvolved ? 0.034f : 0.020f;
            zone.damageVariance = m_santaWaterEvolved ? 0.05f : 0.08f;
            m_zones.append(zone);
        }
    }

    if (m_orbitBladeLevel > 0 && m_orbitBladeCount > 0 && m_orbitBladeActiveMs <= 0 && m_orbitBladeCooldownMs <= 0) {
        m_orbitBladeActiveMs = qRound(m_orbitBladeDurationMs * currentDurationMultiplier());
        m_orbitBladeCooldownMs = qMax(450, qRound(m_orbitBladeCooldownBaseMs * cooldownMultiplier));
    }
}

void SurvivorController::updateGarlicAura()
{
    if (m_garlicLevel <= 0 || m_enemies.isEmpty())
        return;

    const qreal auraRadius = m_garlicRadius * currentAreaMultiplier();
    const int auraDamage = qMax(1, qRound(m_garlicDamage * currentDamageMultiplier()));
    const int auraCooldownMs = qMax(650, qRound(m_garlicCooldownBaseMs * currentCooldownMultiplier()));
    for (int enemyIndex = m_enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
        Enemy &enemy = m_enemies[enemyIndex];
        if ((enemy.position - m_playerPos).length() > enemy.radius + auraRadius)
            continue;
        tryApplyHit(enemyIndex,
                    m_playerPos,
                    1000,
                    auraCooldownMs,
                    auraDamage,
                    0.10f,
                    enemy.elite ? 0.022f : 0.040f,
                    true);
    }
}

void SurvivorController::resolveEnemySeparation()
{
    if (m_enemies.size() < 2)
        return;

    const int iterationCount = m_enemies.size() > 180 ? 1 : 2;
    for (int iteration = 0; iteration < iterationCount; ++iteration) {
        QHash<quint64, QList<int>> buckets;
        buckets.reserve(m_enemies.size() * 2);

        for (int i = 0; i < m_enemies.size(); ++i) {
            const Enemy &enemy = m_enemies.at(i);
            const int cellX = qFloor(enemy.position.x() / EnemySeparationCellSize);
            const int cellY = qFloor(enemy.position.y() / EnemySeparationCellSize);
            buckets[spatialCellKey(cellX, cellY)].append(i);
        }

        for (auto bucketIt = buckets.cbegin(); bucketIt != buckets.cend(); ++bucketIt) {
            const QList<int> indices = bucketIt.value();
            for (int ii = 0; ii < indices.size(); ++ii) {
                const int i = indices.at(ii);
                Enemy &first = m_enemies[i];
                const int cellX = qFloor(first.position.x() / EnemySeparationCellSize);
                const int cellY = qFloor(first.position.y() / EnemySeparationCellSize);

                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                        const QList<int> neighborIndices = buckets.value(spatialCellKey(cellX + offsetX, cellY + offsetY));
                        for (int neighborIndex : neighborIndices) {
                            if (neighborIndex <= i)
                                continue;

                            Enemy &second = m_enemies[neighborIndex];
                            QVector2D delta = second.position - first.position;
                            qreal distance = delta.length();
                            const qreal minDistance = first.radius + second.radius + 0.004f;
                            if (distance >= minDistance)
                                continue;

                            if (distance <= 0.0001f) {
                                const qreal nudgeAngle = (i * 37 + neighborIndex * 53) % 360;
                                delta = rotatedVector(QVector2D(1.0f, 0.0f), nudgeAngle);
                                distance = 1.0f;
                            }

                            const QVector2D normal = delta / distance;
                            const qreal overlap = minDistance - distance;
                            const QVector2D separation = normal * static_cast<float>(overlap * 0.5f);
                            first.position -= separation;
                            second.position += separation;
                        }
                    }
                }
            }
        }
    }
}

void SurvivorController::updateOrbitals(qreal deltaSec, int elapsedMs)
{
    if (m_orbitBladeCount <= 0 || m_orbitBladeDamage <= 0 || m_orbitBladeActiveMs <= 0)
        return;

    m_orbitAngleDeg += m_orbitBladeAngularSpeedDeg * deltaSec;
    while (m_orbitAngleDeg >= 360.0f)
        m_orbitAngleDeg -= 360.0f;

    const qreal orbitalRadius = (0.014f + 0.002f * qMin(3, m_orbitBladeLevel)) * currentAreaMultiplier();
    const qreal orbitRadius = m_orbitBladeRadius * currentAreaMultiplier();
    const qreal angleStep = 360.0 / m_orbitBladeCount;
    const int orbitalDamage = qMax(1, qRound(m_orbitBladeDamage * currentDamageMultiplier()));
    Q_UNUSED(elapsedMs)
    for (int orbitalIndex = 0; orbitalIndex < m_orbitBladeCount; ++orbitalIndex) {
        const QVector2D offset = rotatedVector(QVector2D(orbitRadius, 0.0f),
                                               m_orbitAngleDeg + angleStep * orbitalIndex);
        const QVector2D orbitalPos = m_playerPos + offset;
        for (int enemyIndex = m_enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
            if ((m_enemies.at(enemyIndex).position - orbitalPos).length() > m_enemies.at(enemyIndex).radius + orbitalRadius)
                continue;
            tryApplyHit(enemyIndex,
                        orbitalPos,
                        2000 + orbitalIndex,
                        1700,
                        orbitalDamage,
                        0.08f,
                        0.028f);
        }
    }
}

void SurvivorController::updateProjectiles(qreal deltaSec, int elapsedMs)
{
    for (int i = m_projectiles.size() - 1; i >= 0; --i) {
        Projectile &projectile = m_projectiles[i];
        if (projectile.kind == 2) {
            if (!projectile.returning && projectile.lifeMs <= 620) {
                projectile.returning = true;
                projectile.hitEnemyIds.clear();
            }
            if (projectile.returning) {
                QVector2D toPlayer = m_playerPos - projectile.position;
                if (toPlayer.lengthSquared() > 0.0001f)
                    projectile.velocity = toPlayer.normalized() * static_cast<float>(m_crossSpeed * 1.05f);
            }
        }
        projectile.position += projectile.velocity * static_cast<float>(deltaSec);
        projectile.lifeMs -= elapsedMs;

        bool projectileConsumed = false;
        for (int enemyIndex = m_enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
            Enemy &enemy = m_enemies[enemyIndex];
            const int enemyId = enemy.id;
            if ((enemy.position - projectile.position).length() > enemy.radius + projectile.radius)
                continue;
            if (projectile.hitEnemyIds.contains(enemyId))
                continue;

            if (!tryApplyHit(enemyIndex,
                             projectile.position,
                             projectile.sourceId,
                             projectile.hitIntervalMs,
                             projectile.damage,
                             projectile.damageVariance,
                             projectile.knockback))
                continue;

            if (projectile.kind == 2 || projectile.hitIntervalMs >= 1000000)
                projectile.hitEnemyIds.insert(enemyId);
            --projectile.remainingHits;

            if (projectile.remainingHits <= 0) {
                projectileConsumed = true;
                break;
            }
        }

        if (projectileConsumed
            || projectile.lifeMs <= 0
            || (projectile.kind == 2 && projectile.returning
                && (projectile.position - m_playerPos).length() <= 0.04f)
            || (projectile.position - m_playerPos).length() > ProjectileCleanupDistance) {
            m_projectiles.removeAt(i);
        }
    }
}

void SurvivorController::updateZones(qreal deltaSec, int elapsedMs)
{
    for (int zoneIndex = m_zones.size() - 1; zoneIndex >= 0; --zoneIndex) {
        Zone &zone = m_zones[zoneIndex];
        if (zone.kind == 1) {
            QVector2D toPlayer = m_playerPos - zone.position;
            if (toPlayer.lengthSquared() > 0.0001f)
                zone.position += toPlayer.normalized() * static_cast<float>(0.16f * deltaSec);
        }
        zone.lifeMs -= elapsedMs;
        zone.tickCooldownMs = qMax(0, zone.tickCooldownMs - elapsedMs);

        if (zone.tickCooldownMs == 0) {
            zone.tickCooldownMs = zone.tickIntervalMs;
            for (int enemyIndex = m_enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
                if ((m_enemies.at(enemyIndex).position - zone.position).length() > m_enemies.at(enemyIndex).radius + zone.radius)
                    continue;
                tryApplyHit(enemyIndex,
                            zone.position,
                            zone.sourceId,
                            zone.tickIntervalMs,
                            zone.damage,
                            zone.damageVariance,
                            zone.knockback);
            }
        }

        if (zone.lifeMs <= 0)
            m_zones.removeAt(zoneIndex);
    }
}

void SurvivorController::collectPickups(qreal deltaSec)
{
    for (int i = m_pickups.size() - 1; i >= 0; --i) {
        Pickup &pickup = m_pickups[i];
        QVector2D toPlayer = m_playerPos - pickup.position;
        const qreal distance = toPlayer.length();
        if (distance <= pickup.radius + 0.022f) {
            const Pickup collectedPickup = pickup;
            m_pickups.removeAt(i);
            if (collectedPickup.kind == ChestPickup) {
                enqueueChest(collectedPickup);
            } else {
                gainExp(collectedPickup.exp);
            }
            continue;
        }

        if (distance > currentMagnetRange() || distance <= 0.0001f)
            continue;

        const qreal pullSpeed = 0.40f + currentMagnetRange() * 1.8f;
        pickup.position += toPlayer.normalized() * static_cast<float>(pullSpeed * deltaSec);
    }

    if (!m_levelUpPending && !m_chestPending)
        tryOpenQueuedChest();
}

void SurvivorController::defeatEnemy(int index)
{
    if (index < 0 || index >= m_enemies.size())
        return;

    const Enemy enemy = m_enemies.at(index);
    Pickup pickup;
    pickup.position = enemy.position;
    if (enemy.chestCarrier) {
        pickup.kind = ChestPickup;
        pickup.exp = 0;
        pickup.radius = 0.028f;
        pickup.rewardCount = rollChestRewardCount();
        pickup.canEvolve = survivalTimeSec() >= EvolutionChestStartSec;
    } else {
        pickup.exp = enemy.expReward;
        pickup.kind = pickupKindForExp(pickup.exp);
        pickup.radius = pickupRadiusForKind(pickup.kind);
    }

    if (pickup.kind != ChestPickup) {
        const qreal mergeDistance = 0.045f;
        for (int i = m_pickups.size() - 1; i >= 0; --i) {
            Pickup &existing = m_pickups[i];
            if (existing.kind == ChestPickup)
                continue;
            if ((existing.position - pickup.position).lengthSquared() > mergeDistance * mergeDistance)
                continue;

            existing.exp += pickup.exp;
            existing.kind = pickupKindForExp(existing.exp);
            existing.radius = pickupRadiusForKind(existing.kind);
            existing.position = (existing.position + pickup.position) * 0.5f;
            m_enemies.removeAt(index);
            ++m_killCount;
            return;
        }
    }

    m_pickups.append(pickup);
    m_enemies.removeAt(index);
    ++m_killCount;
}

void SurvivorController::prepareLevelUpChoices()
{
    m_levelUpChoices.clear();

    QList<QString> ownedPool;
    QList<QString> newPool;
    auto appendEligible = [this, &ownedPool, &newPool](const UpgradeTemplate *templates, int count) {
        for (int i = 0; i < count; ++i) {
            const QString id = QString::fromLatin1(templates[i].id);
            const int currentLevel = levelForUpgrade(id);
            const int maxLevel = maxLevelForUpgrade(id);
            if (currentLevel >= maxLevel)
                continue;
            if (currentLevel > 0)
                ownedPool.append(id);
            else
                newPool.append(id);
        }
    };

    appendEligible(kWeaponUpgradePool, static_cast<int>(std::size(kWeaponUpgradePool)));
    appendEligible(kPassiveUpgradePool, static_cast<int>(std::size(kPassiveUpgradePool)));

    QList<QString> selectedIds;
    while (selectedIds.size() < 3 && (!ownedPool.isEmpty() || !newPool.isEmpty())) {
        const bool preferOwned = !ownedPool.isEmpty()
            && (newPool.isEmpty() || QRandomGenerator::global()->bounded(100) < 68);
        QList<QString> &sourcePool = preferOwned ? ownedPool : newPool;
        const int selectedIndex = QRandomGenerator::global()->bounded(sourcePool.size());
        const QString selectedId = sourcePool.takeAt(selectedIndex);
        newPool.removeAll(selectedId);
        ownedPool.removeAll(selectedId);
        selectedIds.append(selectedId);
    }

    for (const QString &id : std::as_const(selectedIds)) {
        const int currentLevel = levelForUpgrade(id);
        m_levelUpChoices.append({
            id,
            titleForUpgrade(id),
            descriptionForUpgrade(id, currentLevel),
            categoryForUpgrade(id),
            currentLevel,
            maxLevelForUpgrade(id)
        });
    }

    if (m_levelUpChoices.isEmpty()) {
        m_levelUpPending = false;
        m_pendingLevelUps = 0;
        refreshLevelUpChoiceCache();
        updateStatusText();
        return;
    }

    m_levelUpPending = true;
    refreshLevelUpChoiceCache();
    updateStatusText();
}

void SurvivorController::enqueueChest(const Pickup &pickup)
{
    m_queuedChests.append(pickup);
}

void SurvivorController::tryOpenQueuedChest()
{
    if (m_levelUpPending || m_chestPending || m_queuedChests.isEmpty())
        return;

    openChest(m_queuedChests.takeFirst());
}

int SurvivorController::rollChestRewardCount() const
{
    const int roll = QRandomGenerator::global()->bounded(100);
    if (roll < 5)
        return 5;
    if (roll < 25)
        return 3;
    return 1;
}

QList<QString> SurvivorController::currentChestUpgradeCandidates() const
{
    QList<QString> ids;
    ids.reserve(12);
    auto appendEligible = [this, &ids](const UpgradeTemplate *templates, int count) {
        for (int i = 0; i < count; ++i) {
            const QString id = QString::fromLatin1(templates[i].id);
            const int currentLevel = levelForUpgrade(id);
            if (currentLevel <= 0 || currentLevel >= maxLevelForUpgrade(id))
                continue;
            ids.append(id);
        }
    };

    appendEligible(kWeaponUpgradePool, static_cast<int>(std::size(kWeaponUpgradePool)));
    appendEligible(kPassiveUpgradePool, static_cast<int>(std::size(kPassiveUpgradePool)));
    return ids;
}

bool SurvivorController::canEvolveWeapon(const QString &weaponId) const
{
    if (weaponId == QStringLiteral("firewand_weapon")) {
        return !m_fireWandEvolved
            && m_fireWandLevel >= 8
            && m_spinachPassiveLevel > 0;
    }
    if (weaponId == QStringLiteral("santawater_weapon")) {
        return !m_santaWaterEvolved
            && m_santaWaterLevel >= 8
            && m_attractorbPassiveLevel > 0;
    }
    return false;
}

QList<QString> SurvivorController::currentEvolutionCandidates() const
{
    QList<QString> ids;
    if (canEvolveWeapon(QStringLiteral("firewand_weapon")))
        ids.append(QStringLiteral("firewand_weapon"));
    if (canEvolveWeapon(QStringLiteral("santawater_weapon")))
        ids.append(QStringLiteral("santawater_weapon"));
    return ids;
}

QString SurvivorController::evolvedTitleForWeapon(const QString &weaponId) const
{
    if (weaponId == QStringLiteral("firewand_weapon"))
        return QStringLiteral("地狱火");
    if (weaponId == QStringLiteral("santawater_weapon"))
        return QStringLiteral("黑波拉");
    return titleForUpgrade(weaponId);
}

QString SurvivorController::evolvedDescriptionForWeapon(const QString &weaponId) const
{
    if (weaponId == QStringLiteral("firewand_weapon"))
        return QStringLiteral("火杖进化完成，火球穿透全部敌人并造成更高爆发伤害。");
    if (weaponId == QStringLiteral("santawater_weapon"))
        return QStringLiteral("圣水进化完成，生成会向玩家汇聚的持续伤害圣池。");
    return QStringLiteral("武器进化完成。");
}

bool SurvivorController::applyEvolution(const QString &weaponId)
{
    if (!canEvolveWeapon(weaponId))
        return false;

    if (weaponId == QStringLiteral("firewand_weapon")) {
        m_fireWandEvolved = true;
        m_fireWandDamage = qMax(m_fireWandDamage, 64);
        m_fireWandCooldownBaseMs = qMin(m_fireWandCooldownBaseMs, 1500);
        m_fireWandProjectileSpeedMultiplier = qMax<qreal>(m_fireWandProjectileSpeedMultiplier, 1.75f);
    } else if (weaponId == QStringLiteral("santawater_weapon")) {
        m_santaWaterEvolved = true;
        m_santaWaterDamage = qMax(m_santaWaterDamage, 34);
        m_santaWaterDurationMs = qMax(m_santaWaterDurationMs, 3200);
        m_santaWaterRadius = qMax<qreal>(m_santaWaterRadius, 0.145f);
        m_santaWaterCooldownBaseMs = qMin(m_santaWaterCooldownBaseMs, 980);
    } else {
        return false;
    }

    refreshDerivedStats();
    refreshUpgradeSummary();
    refreshHudSlotCaches();
    return true;
}

void SurvivorController::applyChestReward(const QString &upgradeId, bool evolved)
{
    ChestReward reward;
    reward.category = evolved ? QStringLiteral("进化") : QStringLiteral("宝箱");
    reward.evolved = evolved;

    if (evolved) {
        reward.title = evolvedTitleForWeapon(upgradeId);
        reward.description = evolvedDescriptionForWeapon(upgradeId);
        m_chestRewardEntries.append(reward);
        return;
    }

    const int previousLevel = levelForUpgrade(upgradeId);
    applyUpgrade(upgradeId);
    reward.title = titleForUpgrade(upgradeId);
    reward.description = QStringLiteral("%1 Lv.%2 -> %3")
                             .arg(reward.title)
                             .arg(previousLevel)
                             .arg(previousLevel + 1);
    m_chestRewardEntries.append(reward);
}

void SurvivorController::openChest(const Pickup &pickup)
{
    m_chestRewardEntries.clear();

    int rewardCount = qBound(1, pickup.rewardCount, 5);
    if (pickup.canEvolve) {
        const QList<QString> evolutionCandidates = currentEvolutionCandidates();
        if (!evolutionCandidates.isEmpty()) {
            const QString evolvedId = evolutionCandidates.at(QRandomGenerator::global()->bounded(evolutionCandidates.size()));
            if (applyEvolution(evolvedId)) {
                applyChestReward(evolvedId, true);
                --rewardCount;
            }
        }
    }

    for (int i = 0; i < rewardCount; ++i) {
        QList<QString> upgradeCandidates = currentChestUpgradeCandidates();
        if (upgradeCandidates.isEmpty()) {
            ChestReward reward;
            reward.category = QStringLiteral("补给");
            reward.title = QStringLiteral("经验结晶");
            reward.description = QStringLiteral("宝箱转化为 25 点经验。");
            m_chestRewardEntries.append(reward);
            gainExp(25);
            continue;
        }

        const QString rewardId = upgradeCandidates.at(QRandomGenerator::global()->bounded(upgradeCandidates.size()));
        applyChestReward(rewardId, false);
    }

    m_chestPending = true;
    m_chestTitle = m_chestRewardEntries.size() >= 5
        ? QStringLiteral("五连宝箱")
        : m_chestRewardEntries.size() >= 3
            ? QStringLiteral("三连宝箱")
            : (m_chestRewardEntries.size() == 1 && m_chestRewardEntries.first().evolved
                   ? QStringLiteral("武器进化")
                   : QStringLiteral("宝箱开启"));
    refreshChestRewardCache();
    updateStatusText();
}

void SurvivorController::applyUpgrade(const QString &upgradeId)
{
    const int currentLevel = levelForUpgrade(upgradeId);
    const int maxLevel = maxLevelForUpgrade(upgradeId);
    if (currentLevel >= maxLevel)
        return;

    const int newLevel = currentLevel + 1;
    if (upgradeId == QStringLiteral("knife_weapon")) {
        m_bladeWeaponLevel = newLevel;
        switch (newLevel) {
        case 1:
            m_attackDamage = 7;
            m_projectileCount = 1;
            m_projectilePierce = 1;
            m_attackCooldownBaseMs = 1000;
            m_projectileSpeed = 1.00f;
            break;
        case 2:
            m_projectileCount = qMin(6, m_projectileCount + 1);
            break;
        case 3:
            m_attackDamage += 5;
            m_projectileCount = qMin(6, m_projectileCount + 1);
            break;
        case 4:
            m_projectileCount = qMin(6, m_projectileCount + 1);
            m_attackCooldownBaseMs = 930;
            break;
        case 5:
            m_projectilePierce = qMin(3, m_projectilePierce + 1);
            break;
        case 6:
            m_projectileCount = qMin(6, m_projectileCount + 1);
            m_attackCooldownBaseMs = 860;
            break;
        case 7:
            m_attackDamage += 5;
            m_projectileCount = qMin(6, m_projectileCount + 1);
            break;
        case 8:
            m_projectilePierce = qMin(3, m_projectilePierce + 1);
            m_attackCooldownBaseMs = 790;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("orbit_weapon")) {
        m_orbitBladeLevel = newLevel;
        switch (newLevel) {
        case 1:
            m_orbitBladeCount = 1;
            m_orbitBladeDamage = 8;
            m_orbitBladeRadius = 0.14f;
            m_orbitBladeAngularSpeedDeg = 140.0f;
            m_orbitBladeCooldownBaseMs = 3200;
            m_orbitBladeDurationMs = 3100;
            break;
        case 2:
            m_orbitBladeCount = qMin(4, m_orbitBladeCount + 1);
            break;
        case 3:
            m_orbitBladeRadius = qMin<qreal>(0.16f, m_orbitBladeRadius + 0.020f);
            m_orbitBladeAngularSpeedDeg = qMin<qreal>(180.0f, m_orbitBladeAngularSpeedDeg + 20.0f);
            break;
        case 4:
            m_orbitBladeDurationMs += 500;
            break;
        case 5:
            m_orbitBladeDamage += 4;
            m_orbitBladeCount = qMin(4, m_orbitBladeCount + 1);
            break;
        case 6:
            m_orbitBladeRadius = qMin<qreal>(0.18f, m_orbitBladeRadius + 0.020f);
            m_orbitBladeAngularSpeedDeg = qMin<qreal>(210.0f, m_orbitBladeAngularSpeedDeg + 24.0f);
            break;
        case 7:
            m_orbitBladeDurationMs += 500;
            break;
        case 8:
            m_orbitBladeDamage += 4;
            m_orbitBladeCount = qMin(4, m_orbitBladeCount + 1);
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("firewand_weapon")) {
        m_fireWandLevel = newLevel;
        switch (newLevel) {
        case 1:
            m_fireWandDamage = 18;
            m_fireWandCooldownBaseMs = 1900;
            m_fireWandProjectileSpeedMultiplier = 1.0f;
            break;
        case 2:
            m_fireWandDamage += 10;
            break;
        case 3:
            m_fireWandProjectileSpeedMultiplier = 1.20f;
            break;
        case 4:
            m_fireWandDamage += 10;
            break;
        case 5:
            m_fireWandProjectileSpeedMultiplier = 1.40f;
            break;
        case 6:
            m_fireWandDamage += 10;
            break;
        case 7:
            m_fireWandProjectileSpeedMultiplier = 1.60f;
            break;
        case 8:
            m_fireWandDamage += 10;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("garlic_weapon")) {
        m_garlicLevel = newLevel;
        switch (newLevel) {
        case 1:
            m_garlicDamage = 4;
            m_garlicRadius = 0.10f;
            m_garlicCooldownBaseMs = 1300;
            break;
        case 2:
            m_garlicRadius = qMin<qreal>(0.12f, m_garlicRadius + 0.020f);
            m_garlicDamage += 1;
            break;
        case 3:
            m_garlicDamage += 1;
            m_garlicCooldownBaseMs = 1200;
            break;
        case 4:
            m_garlicRadius = qMin<qreal>(0.14f, m_garlicRadius + 0.020f);
            break;
        case 5:
            m_garlicDamage += 2;
            break;
        case 6:
            m_garlicRadius = qMin<qreal>(0.16f, m_garlicRadius + 0.020f);
            m_garlicCooldownBaseMs = 1080;
            break;
        case 7:
            ++m_garlicDamage;
            break;
        case 8:
            m_garlicRadius = qMin<qreal>(0.18f, m_garlicRadius + 0.020f);
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("cross_weapon")) {
        m_crossLevel = newLevel;
        switch (newLevel) {
        case 1:
            m_crossDamage = 12;
            m_crossAmount = 1;
            m_crossSpeed = 0.82f;
            break;
        case 2:
            m_crossDamage += 8;
            break;
        case 3:
            m_crossRadius = qMin<qreal>(0.020f, m_crossRadius + 0.002f);
            m_crossSpeed = qMin<qreal>(0.92f, m_crossSpeed + 0.10f);
            break;
        case 4:
            m_crossAmount = qMin(3, m_crossAmount + 1);
            break;
        case 5:
            m_crossDamage += 8;
            break;
        case 6:
            m_crossRadius = qMin<qreal>(0.022f, m_crossRadius + 0.002f);
            m_crossSpeed = qMin<qreal>(1.04f, m_crossSpeed + 0.12f);
            break;
        case 7:
            m_crossAmount = qMin(3, m_crossAmount + 1);
            break;
        case 8:
            m_crossDamage += 8;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("santawater_weapon")) {
        m_santaWaterLevel = newLevel;
        switch (newLevel) {
        case 1:
            m_santaWaterDamage = 10;
            m_santaWaterAmount = 1;
            m_santaWaterDurationMs = 1800;
            m_santaWaterCooldownBaseMs = 1400;
            break;
        case 2:
            m_santaWaterAmount = qMin(4, m_santaWaterAmount + 1);
            m_santaWaterRadius = qMin<qreal>(0.095f, m_santaWaterRadius + 0.015f);
            break;
        case 3:
            m_santaWaterDurationMs += 500;
            m_santaWaterDamage += 6;
            break;
        case 4:
            m_santaWaterAmount = qMin(4, m_santaWaterAmount + 1);
            m_santaWaterRadius = qMin<qreal>(0.110f, m_santaWaterRadius + 0.015f);
            break;
        case 5:
            m_santaWaterDurationMs += 250;
            m_santaWaterDamage += 6;
            break;
        case 6:
            m_santaWaterAmount = qMin(4, m_santaWaterAmount + 1);
            m_santaWaterRadius = qMin<qreal>(0.125f, m_santaWaterRadius + 0.015f);
            break;
        case 7:
            m_santaWaterDurationMs += 250;
            m_santaWaterDamage += 6;
            break;
        case 8:
            m_santaWaterRadius = qMin<qreal>(0.140f, m_santaWaterRadius + 0.015f);
            m_santaWaterDamage += 6;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("wings_passive")) {
        m_wingsPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("emptytome_passive")) {
        m_emptyTomePassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("candelabrador_passive")) {
        m_candelabradorPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("attractorb_passive")) {
        m_attractorbPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("hollowheart_passive")) {
        m_hollowHeartPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("spinach_passive")) {
        m_spinachPassiveLevel = newLevel;
    }

    refreshDerivedStats();
    refreshUpgradeSummary();
    refreshHudSlotCaches();
}

void SurvivorController::refreshDerivedStats()
{
    m_moveSpeed = currentMoveSpeed();
    m_pickupRange = currentMagnetRange();
    m_maxHp = currentMaxHpValue();
    m_hp = qMin(m_hp, m_maxHp);
}

void SurvivorController::refreshFrameCache()
{
    m_renderSnapshot.enemies.clear();
    m_renderSnapshot.enemies.reserve(m_enemies.size());
    for (const Enemy &enemy : std::as_const(m_enemies)) {
        m_renderSnapshot.enemies.append({
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

    m_renderSnapshot.orbitals.clear();
    if (m_orbitBladeCount > 0 && m_orbitBladeActiveMs > 0) {
        m_renderSnapshot.orbitals.reserve(m_orbitBladeCount);
        const qreal angleStep = 360.0 / m_orbitBladeCount;
        const qreal orbitRadius = m_orbitBladeRadius * currentAreaMultiplier();
        const qreal orbitalRadius = (0.014f + 0.002f * qMin(3, m_orbitBladeLevel)) * currentAreaMultiplier();
        for (int i = 0; i < m_orbitBladeCount; ++i) {
            const QVector2D offset = rotatedVector(QVector2D(orbitRadius, 0.0f),
                                                   m_orbitAngleDeg + angleStep * i);
            m_renderSnapshot.orbitals.append({
                m_playerPos.x() + offset.x(),
                m_playerPos.y() + offset.y(),
                orbitalRadius
            });
        }
    }

    m_renderSnapshot.projectiles.clear();
    m_renderSnapshot.projectiles.reserve(m_projectiles.size());
    for (const Projectile &projectile : std::as_const(m_projectiles)) {
        m_renderSnapshot.projectiles.append({
            projectile.position.x(),
            projectile.position.y(),
            projectile.radius,
            projectile.kind
        });
    }

    m_renderSnapshot.pickups.clear();
    m_renderSnapshot.pickups.reserve(m_pickups.size());
    for (const Pickup &pickup : std::as_const(m_pickups)) {
        m_renderSnapshot.pickups.append({
            pickup.position.x(),
            pickup.position.y(),
            pickup.radius,
            pickup.exp,
            pickup.kind
        });
    }

    m_renderSnapshot.zones.clear();
    m_renderSnapshot.zones.reserve(m_zones.size());
    for (const Zone &zone : std::as_const(m_zones)) {
        m_renderSnapshot.zones.append({
            zone.position.x(),
            zone.position.y(),
            zone.radius,
            zone.lifeMs,
            zone.totalLifeMs,
            zone.kind
        });
    }

    m_renderSnapshot.damageNumbers.clear();
    m_renderSnapshot.damageNumbers.reserve(m_damageNumbers.size());
    for (const DamageNumber &number : std::as_const(m_damageNumbers)) {
        m_renderSnapshot.damageNumbers.append({
            number.position.x(),
            number.position.y(),
            number.amount,
            number.lifeMs,
            number.totalLifeMs,
            number.elite
        });
    }
}

bool SurvivorController::tryApplyHit(int enemyIndex,
                                     const QVector2D &sourcePosition,
                                     int sourceId,
                                     int hitIntervalMs,
                                     int baseDamage,
                                     qreal damageVariance,
                                     qreal knockback,
                                     bool amplifyKnockback)
{
    if (enemyIndex < 0 || enemyIndex >= m_enemies.size())
        return false;

    Enemy &enemy = m_enemies[enemyIndex];
    if (sourceId != 0 && hitIntervalMs > 0 && enemy.sourceHitCooldownsMs.value(sourceId, 0) > 0)
        return false;

    if (sourceId != 0 && hitIntervalMs > 0)
        enemy.sourceHitCooldownsMs.insert(sourceId, hitIntervalMs);

    if (amplifyKnockback && !enemy.chestCarrier)
        enemy.knockbackBonus = qMin<qreal>(1.0, enemy.knockbackBonus + 0.30f);

    applyKnockbackToEnemy(enemy, sourcePosition, knockback);
    damageEnemy(enemyIndex, rollDamage(baseDamage, damageVariance));
    return true;
}

void SurvivorController::applyKnockbackToEnemy(Enemy &enemy, const QVector2D &sourcePosition, qreal knockback)
{
    if (knockback <= 0.0f || enemy.chestCarrier)
        return;

    QVector2D pushDirection = enemy.position - sourcePosition;
    if (pushDirection.lengthSquared() <= 0.0001f)
        pushDirection = QVector2D(1.0f, 0.0f);
    else
        pushDirection.normalize();

    const qreal effectiveKnockback = knockback * qBound<qreal>(0.05, enemy.knockbackFactor + enemy.knockbackBonus, 1.0);
    enemy.position += pushDirection * static_cast<float>(effectiveKnockback);
    enemy.speedScale = qMin<qreal>(enemy.speedScale, qMax<qreal>(0.52, 1.0 - effectiveKnockback * 6.5));
}

void SurvivorController::decayEnemyHitCooldowns(Enemy &enemy, int elapsedMs)
{
    for (auto it = enemy.sourceHitCooldownsMs.begin(); it != enemy.sourceHitCooldownsMs.end(); ) {
        const int remaining = it.value() - elapsedMs;
        if (remaining <= 0)
            it = enemy.sourceHitCooldownsMs.erase(it);
        else {
            it.value() = remaining;
            ++it;
        }
    }
}

int SurvivorController::rollDamage(int baseDamage, qreal damageVariance) const
{
    const qreal variance = qMax<qreal>(0.0, damageVariance);
    const qreal randomFactor = 1.0 + ((QRandomGenerator::global()->generateDouble() * 2.0) - 1.0) * variance;
    return qMax(1, qRound(baseDamage * randomFactor));
}

int SurvivorController::levelForUpgrade(const QString &upgradeId) const
{
    if (upgradeId == QStringLiteral("knife_weapon"))
        return m_bladeWeaponLevel;
    if (upgradeId == QStringLiteral("orbit_weapon"))
        return m_orbitBladeLevel;
    if (upgradeId == QStringLiteral("firewand_weapon"))
        return m_fireWandLevel;
    if (upgradeId == QStringLiteral("garlic_weapon"))
        return m_garlicLevel;
    if (upgradeId == QStringLiteral("cross_weapon"))
        return m_crossLevel;
    if (upgradeId == QStringLiteral("santawater_weapon"))
        return m_santaWaterLevel;
    if (upgradeId == QStringLiteral("wings_passive"))
        return m_wingsPassiveLevel;
    if (upgradeId == QStringLiteral("emptytome_passive"))
        return m_emptyTomePassiveLevel;
    if (upgradeId == QStringLiteral("candelabrador_passive"))
        return m_candelabradorPassiveLevel;
    if (upgradeId == QStringLiteral("attractorb_passive"))
        return m_attractorbPassiveLevel;
    if (upgradeId == QStringLiteral("hollowheart_passive"))
        return m_hollowHeartPassiveLevel;
    if (upgradeId == QStringLiteral("spinach_passive"))
        return m_spinachPassiveLevel;
    return 0;
}

int SurvivorController::maxLevelForUpgrade(const QString &upgradeId) const
{
    if (upgradeId == QStringLiteral("knife_weapon")
        || upgradeId == QStringLiteral("orbit_weapon")
        || upgradeId == QStringLiteral("firewand_weapon")
        || upgradeId == QStringLiteral("garlic_weapon")
        || upgradeId == QStringLiteral("cross_weapon")
        || upgradeId == QStringLiteral("santawater_weapon")) {
        return 8;
    }
    if (upgradeId == QStringLiteral("wings_passive")
        || upgradeId == QStringLiteral("emptytome_passive")
        || upgradeId == QStringLiteral("candelabrador_passive")
        || upgradeId == QStringLiteral("attractorb_passive")
        || upgradeId == QStringLiteral("hollowheart_passive")
        || upgradeId == QStringLiteral("spinach_passive")) {
        return 5;
    }
    return 0;
}

bool SurvivorController::isWeaponUpgrade(const QString &upgradeId) const
{
    return upgradeId == QStringLiteral("knife_weapon")
        || upgradeId == QStringLiteral("orbit_weapon")
        || upgradeId == QStringLiteral("firewand_weapon")
        || upgradeId == QStringLiteral("garlic_weapon")
        || upgradeId == QStringLiteral("cross_weapon")
        || upgradeId == QStringLiteral("santawater_weapon");
}

qreal SurvivorController::currentDamageMultiplier() const
{
    return 1.0 + m_spinachPassiveLevel * 0.10;
}

qreal SurvivorController::currentAreaMultiplier() const
{
    return 1.0 + m_candelabradorPassiveLevel * 0.10;
}

qreal SurvivorController::currentCooldownMultiplier() const
{
    return qMax<qreal>(0.60, 1.0 - m_emptyTomePassiveLevel * 0.08);
}

qreal SurvivorController::currentDurationMultiplier() const
{
    return 1.0;
}

qreal SurvivorController::currentProjectileSpeedMultiplier() const
{
    return 1.0;
}

qreal SurvivorController::currentMoveSpeed() const
{
    return 0.34 * (1.0 + m_wingsPassiveLevel * 0.10);
}

qreal SurvivorController::currentMagnetRange() const
{
    static const qreal kMagnetRangeSteps[] = {0.12f, 0.18f, 0.24f, 0.30f, 0.39f, 0.52f};
    return kMagnetRangeSteps[qBound(0, m_attractorbPassiveLevel, 5)];
}

int SurvivorController::currentMaxHpValue() const
{
    return qRound(100 * (1.0 + m_hollowHeartPassiveLevel * 0.20));
}

QString SurvivorController::titleForUpgrade(const QString &upgradeId) const
{
    for (const UpgradeTemplate &entry : kWeaponUpgradePool) {
        if (upgradeId == QString::fromLatin1(entry.id))
            return QString::fromUtf8(entry.title);
    }
    for (const UpgradeTemplate &entry : kPassiveUpgradePool) {
        if (upgradeId == QString::fromLatin1(entry.id))
            return QString::fromUtf8(entry.title);
    }
    return QStringLiteral("未知强化");
}

QString SurvivorController::categoryForUpgrade(const QString &upgradeId) const
{
    return isWeaponUpgrade(upgradeId) ? QStringLiteral("武器") : QStringLiteral("被动");
}

QString SurvivorController::descriptionForUpgrade(const QString &upgradeId, int currentLevel) const
{
    const int nextLevel = currentLevel + 1;
    QString effectText;

    if (upgradeId == QStringLiteral("knife_weapon")) {
        switch (nextLevel) {
        case 1: effectText = QStringLiteral("解锁飞刀，沿移动朝向发射定向投射物。"); break;
        case 2: effectText = QStringLiteral("飞刀数量 +1。"); break;
        case 3: effectText = QStringLiteral("飞刀伤害 +5，数量 +1。"); break;
        case 4: effectText = QStringLiteral("飞刀数量 +1，冷却缩短。"); break;
        case 5: effectText = QStringLiteral("飞刀穿透 +1。"); break;
        case 6: effectText = QStringLiteral("飞刀数量 +1，冷却缩短。"); break;
        case 7: effectText = QStringLiteral("飞刀伤害 +5，数量 +1。"); break;
        case 8: effectText = QStringLiteral("飞刀穿透 +1，冷却缩短。"); break;
        default: break;
        }
    } else if (upgradeId == QStringLiteral("orbit_weapon")) {
        switch (nextLevel) {
        case 1: effectText = QStringLiteral("解锁秘典，按持续时间召唤 1 枚环刃。"); break;
        case 2: effectText = QStringLiteral("环刃数量 +1。"); break;
        case 3: effectText = QStringLiteral("环刃半径扩大，转速提升。"); break;
        case 4: effectText = QStringLiteral("环刃持续时间延长。"); break;
        case 5: effectText = QStringLiteral("环刃伤害提升，数量 +1。"); break;
        case 6: effectText = QStringLiteral("环刃半径扩大，转速提升。"); break;
        case 7: effectText = QStringLiteral("环刃持续时间再次延长。"); break;
        case 8: effectText = QStringLiteral("环刃伤害提升，数量 +1。"); break;
        default: break;
        }
    } else if (upgradeId == QStringLiteral("firewand_weapon")) {
        switch (nextLevel) {
        case 1: effectText = QStringLiteral("解锁火焰魔杖，随机索敌发射高伤火弹。"); break;
        case 2: effectText = QStringLiteral("火焰魔杖伤害 +10。"); break;
        case 3: effectText = QStringLiteral("火焰魔杖弹速提升。"); break;
        case 4: effectText = QStringLiteral("火焰魔杖伤害 +10。"); break;
        case 5: effectText = QStringLiteral("火焰魔杖弹速再次提升。"); break;
        case 6: effectText = QStringLiteral("火焰魔杖伤害 +10。"); break;
        case 7: effectText = QStringLiteral("火焰魔杖弹速提升到更高档。"); break;
        case 8: effectText = QStringLiteral("火焰魔杖伤害 +10。"); break;
        default: break;
        }
    } else if (upgradeId == QStringLiteral("garlic_weapon")) {
        switch (nextLevel) {
        case 1: effectText = QStringLiteral("解锁大蒜，持续灼伤并轻微击退近身敌人。"); break;
        case 2: effectText = QStringLiteral("大蒜范围扩大，伤害 +1。"); break;
        case 3: effectText = QStringLiteral("大蒜伤害 +1，触发更快。"); break;
        case 4: effectText = QStringLiteral("大蒜范围继续扩大。"); break;
        case 5: effectText = QStringLiteral("大蒜伤害 +2。"); break;
        case 6: effectText = QStringLiteral("大蒜范围扩大，触发更快。"); break;
        case 7: effectText = QStringLiteral("大蒜伤害 +1。"); break;
        case 8: effectText = QStringLiteral("大蒜范围再次扩大。"); break;
        default: break;
        }
    } else if (upgradeId == QStringLiteral("cross_weapon")) {
        switch (nextLevel) {
        case 1: effectText = QStringLiteral("解锁十字架，命中后回旋返程。"); break;
        case 2: effectText = QStringLiteral("十字架伤害提升。"); break;
        case 3: effectText = QStringLiteral("十字架体积扩大，飞行速度提升。"); break;
        case 4: effectText = QStringLiteral("十字架数量 +1。"); break;
        case 5: effectText = QStringLiteral("十字架伤害再次提升。"); break;
        case 6: effectText = QStringLiteral("十字架体积扩大，飞行速度提升。"); break;
        case 7: effectText = QStringLiteral("十字架数量 +1。"); break;
        case 8: effectText = QStringLiteral("十字架伤害再次提升。"); break;
        default: break;
        }
    } else if (upgradeId == QStringLiteral("santawater_weapon")) {
        switch (nextLevel) {
        case 1: effectText = QStringLiteral("解锁圣水，在附近生成持续伤害水池。"); break;
        case 2: effectText = QStringLiteral("圣水数量 +1，水池半径扩大。"); break;
        case 3: effectText = QStringLiteral("圣水持续时间延长，伤害提升。"); break;
        case 4: effectText = QStringLiteral("圣水数量 +1，水池半径扩大。"); break;
        case 5: effectText = QStringLiteral("圣水持续时间延长，伤害提升。"); break;
        case 6: effectText = QStringLiteral("圣水数量 +1，水池半径扩大。"); break;
        case 7: effectText = QStringLiteral("圣水持续时间延长，伤害提升。"); break;
        case 8: effectText = QStringLiteral("圣水水池半径扩大，伤害提升。"); break;
        default: break;
        }
    } else if (upgradeId == QStringLiteral("wings_passive")) {
        effectText = QStringLiteral("移动速度 +10%。");
    } else if (upgradeId == QStringLiteral("emptytome_passive")) {
        effectText = QStringLiteral("全部武器冷却 -8%。");
    } else if (upgradeId == QStringLiteral("candelabrador_passive")) {
        effectText = QStringLiteral("范围类武器面积 +10%。");
    } else if (upgradeId == QStringLiteral("attractorb_passive")) {
        effectText = QStringLiteral("显著提升经验吸附范围。");
    } else if (upgradeId == QStringLiteral("hollowheart_passive")) {
        effectText = QStringLiteral("最大生命 +20%。");
    } else if (upgradeId == QStringLiteral("spinach_passive")) {
        effectText = QStringLiteral("全部武器伤害 +10%。");
    }

    return effectText;
}

void SurvivorController::addDamageNumber(const QVector2D &position, int amount, bool elite)
{
    DamageNumber number;
    number.position = position + QVector2D(0.0f,
                                           elite ? -0.05f : -0.035f);
    const qreal horizontalDrift = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.08;
    number.velocity = QVector2D(horizontalDrift, elite ? -0.14f : -0.11f);
    number.amount = qMax(1, amount);
    number.totalLifeMs = elite ? 820 : 680;
    number.lifeMs = number.totalLifeMs;
    number.elite = elite;
    m_damageNumbers.append(number);

    while (m_damageNumbers.size() > 56)
        m_damageNumbers.removeFirst();
}

void SurvivorController::damageEnemy(int enemyIndex, int damage)
{
    if (enemyIndex < 0 || enemyIndex >= m_enemies.size())
        return;

    Enemy &enemy = m_enemies[enemyIndex];
    const int appliedDamage = qMax(1, damage);
    enemy.hp -= appliedDamage;
    enemy.hitFlashMs = enemy.elite ? 120 : 90;
    addDamageNumber(enemy.position, appliedDamage, enemy.elite);
    if (enemy.hp <= 0)
        defeatEnemy(enemyIndex);
}

void SurvivorController::refreshUpgradeSummary()
{
    m_upgradeSummary = QStringLiteral("武器：飞刀 %1/8 · 秘典 %2/8 · %3 %4/8 · 大蒜 %5/8 · 十字架 %6/8 · %7 %8/8\n被动：翅膀 %9/5 · 空白之书 %10/5 · 烛台 %11/5 · 磁力珠 %12/5 · 空心心脏 %13/5 · 菠菜 %14/5")
        .arg(m_bladeWeaponLevel)
        .arg(m_orbitBladeLevel)
        .arg(m_fireWandEvolved ? QStringLiteral("地狱火") : QStringLiteral("火杖"))
        .arg(m_fireWandLevel)
        .arg(m_garlicLevel)
        .arg(m_crossLevel)
        .arg(m_santaWaterEvolved ? QStringLiteral("黑波拉") : QStringLiteral("圣水"))
        .arg(m_santaWaterLevel)
        .arg(m_wingsPassiveLevel)
        .arg(m_emptyTomePassiveLevel)
        .arg(m_candelabradorPassiveLevel)
        .arg(m_attractorbPassiveLevel)
        .arg(m_hollowHeartPassiveLevel)
        .arg(m_spinachPassiveLevel);
}

void SurvivorController::refreshWaveLabel()
{
    const int newWaveIndex = currentWaveIndex();
    if (m_lastWaveIndex == newWaveIndex && !m_waveLabel.isEmpty())
        return;

    m_lastWaveIndex = newWaveIndex;
    const int waveStartSec = newWaveIndex * 60;
    const int waveEndSec = waveStartSec + 59;
    m_waveLabel = QString::fromUtf8(kWaveTemplates[newWaveIndex].label)
        + QStringLiteral(" · %1-%2s")
              .arg(waveStartSec)
              .arg(waveEndSec);
}

void SurvivorController::triggerWaveEvents()
{
    const int sec = survivalTimeSec();
    if (m_triggeredEventSeconds.contains(sec))
        return;

    bool triggered = true;
    switch (sec) {
    case 120:
        spawnBatSwarm(28, 1.10f);
        break;
    case 150:
        spawnBatSwarm(32, 1.14f);
        break;
    case 300:
        spawnFlowerWall(24, 1.00f, 0.060f, 80);
        break;
    case 360:
        spawnBatSwarm(42, 1.22f);
        break;
    case 420:
        spawnFlowerWall(32, 1.15f, 0.072f, 120);
        break;
    case 480:
        spawnBatSwarm(52, 1.28f);
        break;
    case 540:
        spawnBatSwarm(64, 1.36f);
        break;
    case 570:
        spawnFlowerWall(40, 1.28f, 0.082f, 160);
        break;
    default:
        triggered = false;
        break;
    }

    if (triggered)
        m_triggeredEventSeconds.insert(sec);
}

void SurvivorController::spawnBatSwarm(int count, qreal speedMultiplier)
{
    const qreal angle = QRandomGenerator::global()->generateDouble() * 360.0;
    const QVector2D outward = rotatedVector(QVector2D(1.0f, 0.0f), angle);
    const QVector2D tangent(-outward.y(), outward.x());
    const QVector2D basePosition = m_playerPos + outward * 1.65f;

    for (int i = 0; i < count; ++i) {
        spawnEnemy(false, BatEnemy, false);
        Enemy &enemy = m_enemies.last();
        const qreal laneOffset = (static_cast<qreal>(i) - (count - 1) / 2.0) * 0.050f;
        enemy.position = basePosition + tangent * static_cast<float>(laneOffset);
        enemy.speed = qMax(enemy.speed, 0.18f * speedMultiplier);
        enemy.forcedVelocity = -outward * static_cast<float>(enemy.speed * 1.45f);
        enemy.forcedMovementMs = 4200;
        enemy.expReward = 1;
        enemy.knockbackFactor = qMin<qreal>(enemy.knockbackFactor, 0.9f);
    }
}

void SurvivorController::spawnFlowerWall(int count, qreal ringRadius, qreal inwardSpeed, int hpOverride)
{
    for (int i = 0; i < count; ++i) {
        spawnEnemy(false, FlowerEnemy, false);
        Enemy &enemy = m_enemies.last();
        const qreal angle = (360.0 / qMax(1, count)) * i;
        const QVector2D direction = rotatedVector(QVector2D(1.0f, 0.0f), angle);
        enemy.position = m_playerPos + direction * static_cast<float>(ringRadius);
        enemy.maxHp = qMax(enemy.maxHp, hpOverride);
        enemy.hp = enemy.maxHp;
        enemy.speed = qMax(enemy.speed, inwardSpeed);
        enemy.forcedVelocity = -direction * static_cast<float>(inwardSpeed);
        enemy.forcedMovementMs = 2600;
        enemy.knockbackFactor = qMin<qreal>(enemy.knockbackFactor, 0.28f);
        enemy.touchDamage = qMax(enemy.touchDamage, 8);
        enemy.expReward = qMax(enemy.expReward, 2);
    }
}

int SurvivorController::currentSpawnIntervalMs() const
{
    return kWaveTemplates[currentWaveIndex()].spawnIntervalMs;
}

int SurvivorController::currentSpawnBurstCount() const
{
    return kWaveTemplates[currentWaveIndex()].spawnBurst;
}

int SurvivorController::currentEnemyCap() const
{
    return kWaveTemplates[currentWaveIndex()].enemyCap;
}

int SurvivorController::currentEliteSpawnIntervalMs() const
{
    if (survivalTimeSec() < 240)
        return 0;
    return kWaveTemplates[currentWaveIndex()].eliteIntervalMs;
}

int SurvivorController::currentWaveIndex() const
{
    const int waveIndex = survivalTimeSec() / 60;
    return qBound(0, waveIndex, static_cast<int>(std::size(kWaveTemplates)) - 1);
}

int SurvivorController::currentEnemyKind() const
{
    const int sec = survivalTimeSec();
    const int roll = QRandomGenerator::global()->bounded(100);
    if (sec < 60)
        return BatEnemy;
    if (sec < 120)
        return roll < 72 ? ZombieEnemy : BatEnemy;
    if (sec < 180)
        return roll < 82 ? BatEnemy : ZombieEnemy;
    if (sec < 240)
        return roll < 70 ? SkeletonEnemy : (roll < 90 ? BatEnemy : ZombieEnemy);
    if (sec < 300)
        return roll < 65 ? SkeletonEnemy : WerewolfEnemy;
    if (sec < 360)
        return roll < 58 ? FlowerEnemy : (roll < 82 ? ZombieEnemy : SkeletonEnemy);
    if (sec < 420)
        return roll < 60 ? BatEnemy : (roll < 84 ? ZombieEnemy : SkeletonEnemy);
    if (sec < 480)
        return roll < 48 ? WerewolfEnemy : (roll < 76 ? OgreEnemy : SkeletonEnemy);
    if (sec < 540)
        return roll < 42 ? GiantBatEnemy : (roll < 72 ? OgreEnemy : WerewolfEnemy);
    return roll < 45 ? GiantBatEnemy : (roll < 70 ? OgreEnemy : BatEnemy);
}

int SurvivorController::currentEliteKind() const
{
    const int sec = survivalTimeSec();
    const int roll = QRandomGenerator::global()->bounded(100);
    if (sec < 360)
        return roll < 68 ? WerewolfEnemy : FlowerEnemy;
    if (sec < 480)
        return roll < 55 ? FlowerEnemy : OgreEnemy;
    if (sec < 540)
        return roll < 60 ? GiantBatEnemy : OgreEnemy;
    return roll < 45 ? GiantBatEnemy : OgreEnemy;
}

int SurvivorController::currentBossKind() const
{
    const int sec = survivalTimeSec();
    if (sec < 180)
        return GiantBatEnemy;
    if (sec < 300)
        return GiantBatEnemy;
    if (sec < 420)
        return OgreEnemy;
    return sec < 540 ? GiantBatEnemy : OgreEnemy;
}

bool SurvivorController::hasLivingBoss() const
{
    for (const Enemy &enemy : m_enemies) {
        if (enemy.chestCarrier)
            return true;
    }
    return false;
}

void SurvivorController::gainExp(int amount)
{
    m_exp += qMax(0, amount);
    while (m_exp >= m_expToNext) {
        m_exp -= m_expToNext;
        ++m_level;
        m_expToNext = expRequirementForLevel(m_level);
        ++m_pendingLevelUps;
    }

    if (!m_levelUpPending && m_pendingLevelUps > 0) {
        prepareLevelUpChoices();
        m_attackCooldownMs = 0;
    }
}

void SurvivorController::updateStatusText()
{
    if (m_gameOver) {
        m_statusText = QStringLiteral("本局结束，返回房间后可以继续迭代 Survivor MVP。");
        return;
    }

    if (m_chestPending) {
        m_statusText = QStringLiteral("宝箱开启中：确认本次奖励后继续推进。");
        return;
    }

    if (m_levelUpPending) {
        m_statusText = QStringLiteral("升级暂停中：从 3 个强化里选 1 个，然后继续清怪。");
        return;
    }

    if (m_networkPrototype) {
        m_statusText = QStringLiteral("MVP 原型：房间入口已接通，实时联机同步下一步接入。");
        return;
    }

    if (m_garlicLevel > 0
        && m_bladeWeaponLevel == 0
        && m_fireWandLevel == 0
        && m_orbitBladeLevel == 0
        && m_crossLevel == 0
        && m_santaWaterLevel == 0) {
        m_statusText = QStringLiteral("大蒜现在是初始武器，负责清理贴身杂兵。先稳住走位，尽快补出飞刀或远程武器。");
        return;
    }

    if (m_fireWandEvolved || m_santaWaterEvolved) {
        m_statusText = QStringLiteral("进化武器已成型：地狱火负责贯穿清线，黑波拉负责持续压场。");
        return;
    }

    if (m_fireWandLevel > 0 || (m_garlicLevel > 0 && m_bladeWeaponLevel > 0)) {
        m_statusText = QStringLiteral("飞刀负责定向输出，火杖负责远程补刀，大蒜与秘典负责贴身压场。");
        return;
    }

    if (m_orbitBladeLevel > 0 || m_bladeWeaponLevel > 0) {
        m_statusText = QStringLiteral("飞刀按移动朝向发射，秘典负责贴身清场。地图跟随滚动，边走边拉怪更安全。");
        return;
    }

    m_statusText = QStringLiteral("大蒜先保命，后续再补飞刀和范围武器决定 build 走向。");
}
