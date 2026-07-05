#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVector2D>

class SurvivorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool gameOver READ isGameOver NOTIFY gameOverChanged)
    Q_PROPERTY(bool networkPrototype READ isNetworkPrototype NOTIFY runningChanged)
    Q_PROPERTY(int hp READ hp NOTIFY stateChanged)
    Q_PROPERTY(int maxHp READ maxHp NOTIFY stateChanged)
    Q_PROPERTY(int level READ level NOTIFY stateChanged)
    Q_PROPERTY(int exp READ exp NOTIFY stateChanged)
    Q_PROPERTY(int expToNext READ expToNext NOTIFY stateChanged)
    Q_PROPERTY(int killCount READ killCount NOTIFY stateChanged)
    Q_PROPERTY(int survivalTimeSec READ survivalTimeSec NOTIFY stateChanged)
    Q_PROPERTY(qreal playerX READ playerX NOTIFY frameChanged)
    Q_PROPERTY(qreal playerY READ playerY NOTIFY frameChanged)
    Q_PROPERTY(qreal auraRadius READ auraRadius NOTIFY frameChanged)
    Q_PROPERTY(qreal radarRange READ radarRange CONSTANT)
    Q_PROPERTY(bool levelUpPending READ isLevelUpPending NOTIFY stateChanged)
    Q_PROPERTY(QVariantList levelUpChoices READ levelUpChoices NOTIFY stateChanged)
    Q_PROPERTY(bool chestPending READ isChestPending NOTIFY stateChanged)
    Q_PROPERTY(QVariantList chestRewards READ chestRewards NOTIFY stateChanged)
    Q_PROPERTY(QString chestTitle READ chestTitle NOTIFY stateChanged)
    Q_PROPERTY(QVariantList weaponSlots READ weaponSlots NOTIFY stateChanged)
    Q_PROPERTY(QVariantList passiveSlots READ passiveSlots NOTIFY stateChanged)
    Q_PROPERTY(QString waveLabel READ waveLabel NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString upgradeSummary READ upgradeSummary NOTIFY stateChanged)

public:
    explicit SurvivorController(QObject *parent = nullptr);

    bool isRunning() const { return m_running; }
    bool isGameOver() const { return m_gameOver; }
    bool isNetworkPrototype() const { return m_networkPrototype; }
    int hp() const { return m_hp; }
    int maxHp() const { return m_maxHp; }
    int level() const { return m_level; }
    int exp() const { return m_exp; }
    int expToNext() const { return m_expToNext; }
    int killCount() const { return m_killCount; }
    int survivalTimeSec() const { return m_survivalTimeMs / 1000; }
    qreal playerX() const { return m_playerPos.x(); }
    qreal playerY() const { return m_playerPos.y(); }
    qreal auraRadius() const { return m_garlicLevel > 0 ? m_garlicRadius * currentAreaMultiplier() : 0.0; }
    qreal radarRange() const { return 2.6; }
    bool isLevelUpPending() const { return m_levelUpPending; }
    QVariantList levelUpChoices() const;
    bool isChestPending() const { return m_chestPending; }
    QVariantList chestRewards() const;
    QString chestTitle() const { return m_chestTitle; }
    QVariantList weaponSlots() const;
    QVariantList passiveSlots() const;
    QString waveLabel() const { return m_waveLabel; }
    QString statusText() const { return m_statusText; }
    QString upgradeSummary() const { return m_upgradeSummary; }

    Q_INVOKABLE void startRun(bool networkPrototype = false);
    Q_INVOKABLE void stopRun();
    Q_INVOKABLE void setMoveInput(qreal horizontal, qreal vertical);
    Q_INVOKABLE void chooseLevelUp(const QString &upgradeId);
    Q_INVOKABLE void closeChestRewards();

signals:
    void stateChanged();
    void frameChanged();
    void runningChanged();
    void gameOverChanged();

private:
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

    struct Pickup {
        QVector2D position;
        qreal radius = 0.014f;
        int exp = 1;
        int kind = 0;
        int rewardCount = 1;
        bool canEvolve = false;
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

    struct DamageNumber {
        QVector2D position;
        QVector2D velocity;
        int amount = 0;
        int lifeMs = 0;
        int totalLifeMs = 0;
        bool elite = false;
    };

public:
    struct RenderEnemy {
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
        QList<RenderEnemy> enemies;
        QList<RenderOrbital> orbitals;
        QList<RenderProjectile> projectiles;
        QList<RenderPickup> pickups;
        QList<RenderZone> zones;
        QList<RenderDamageNumber> damageNumbers;
    };

    const RenderSnapshot &renderSnapshot() const { return m_renderSnapshot; }

private:
    void resetState();
    void spawnEnemy(bool elite = false, int forcedKind = -1, bool forceChestCarrier = false);
    void tick();
    void simulateStep(int elapsedMs);
    void applyAutoAttack();
    void updateGarlicAura();
    void resolveEnemySeparation();
    void updateOrbitals(qreal deltaSec, int elapsedMs);
    void updateProjectiles(qreal deltaSec, int elapsedMs);
    void updateZones(qreal deltaSec, int elapsedMs);
    void collectPickups(qreal deltaSec);
    void defeatEnemy(int index);
    void gainExp(int amount);
    void prepareLevelUpChoices();
    void applyUpgrade(const QString &upgradeId);
    void addDamageNumber(const QVector2D &position, int amount, bool elite);
    void damageEnemy(int enemyIndex, int damage);
    void refreshDerivedStats();
    void refreshFrameCache();
    void refreshLevelUpChoiceCache();
    void refreshChestRewardCache();
    void refreshWeaponSlotCache();
    void refreshPassiveSlotCache();
    void refreshHudSlotCaches();
    void openChest(const Pickup &pickup);
    void enqueueChest(const Pickup &pickup);
    void tryOpenQueuedChest();
    int rollChestRewardCount() const;
    QList<QString> currentChestUpgradeCandidates() const;
    QList<QString> currentEvolutionCandidates() const;
    bool canEvolveWeapon(const QString &weaponId) const;
    void applyChestReward(const QString &upgradeId, bool evolved);
    bool applyEvolution(const QString &weaponId);
    QString evolvedTitleForWeapon(const QString &weaponId) const;
    QString evolvedDescriptionForWeapon(const QString &weaponId) const;
    bool tryApplyHit(int enemyIndex,
                     const QVector2D &sourcePosition,
                     int sourceId,
                     int hitIntervalMs,
                     int baseDamage,
                     qreal damageVariance,
                     qreal knockback,
                     bool amplifyKnockback = false);
    void applyKnockbackToEnemy(Enemy &enemy, const QVector2D &sourcePosition, qreal knockback);
    void decayEnemyHitCooldowns(Enemy &enemy, int elapsedMs);
    int rollDamage(int baseDamage, qreal damageVariance) const;
    int levelForUpgrade(const QString &upgradeId) const;
    int maxLevelForUpgrade(const QString &upgradeId) const;
    bool isWeaponUpgrade(const QString &upgradeId) const;
    qreal currentDamageMultiplier() const;
    qreal currentAreaMultiplier() const;
    qreal currentCooldownMultiplier() const;
    qreal currentDurationMultiplier() const;
    qreal currentProjectileSpeedMultiplier() const;
    qreal currentMoveSpeed() const;
    qreal currentMagnetRange() const;
    int currentMaxHpValue() const;
    QString titleForUpgrade(const QString &upgradeId) const;
    QString categoryForUpgrade(const QString &upgradeId) const;
    QString descriptionForUpgrade(const QString &upgradeId, int currentLevel) const;
    void refreshUpgradeSummary();
    void refreshWaveLabel();
    void updateStatusText();
    void triggerWaveEvents();
    void spawnBatSwarm(int count, qreal speedMultiplier);
    void spawnFlowerWall(int count, qreal ringRadius, qreal inwardSpeed, int hpOverride);
    int currentSpawnIntervalMs() const;
    int currentSpawnBurstCount() const;
    int currentEnemyCap() const;
    int currentEliteSpawnIntervalMs() const;
    int currentWaveIndex() const;
    int currentEnemyKind() const;
    int currentEliteKind() const;
    int currentBossKind() const;
    bool hasLivingBoss() const;

    static constexpr int TickIntervalMs = 16;

    QTimer m_tickTimer;
    QElapsedTimer m_frameTimer;
    QVector2D m_playerPos {0.5f, 0.5f};
    QVector2D m_moveInput {0.0f, 0.0f};
    QVector2D m_facingDirection {1.0f, 0.0f};
    QList<Enemy> m_enemies;
    QList<Projectile> m_projectiles;
    QList<Pickup> m_pickups;
    QList<Zone> m_zones;
    QList<DamageNumber> m_damageNumbers;
    QList<UpgradeChoice> m_levelUpChoices;
    bool m_running = false;
    bool m_gameOver = false;
    bool m_networkPrototype = false;
    bool m_levelUpPending = false;
    bool m_chestPending = false;
    int m_hp = 100;
    int m_maxHp = 100;
    int m_level = 1;
    int m_exp = 0;
    int m_expToNext = 5;
    int m_killCount = 0;
    int m_pendingLevelUps = 0;
    int m_survivalTimeMs = 0;
    int m_tickAccumulatorMs = 0;
    int m_spawnAccumulatorMs = 0;
    int m_eliteSpawnAccumulatorMs = 0;
    int m_spawnedBossCount = 0;
    int m_nextEnemyId = 1;
    int m_nextSourceId = 1;
    qreal m_contactDamageCarry = 0.0;
    int m_attackCooldownMs = 0;
    int m_attackCooldownBaseMs = 520;
    int m_orbitBladeCooldownMs = 0;
    int m_orbitBladeCooldownBaseMs = 3200;
    int m_orbitBladeActiveMs = 0;
    int m_orbitBladeDurationMs = 3100;
    int m_fireWandCooldownMs = 0;
    int m_fireWandCooldownBaseMs = 1500;
    int m_garlicCooldownBaseMs = 1300;
    int m_crossCooldownMs = 0;
    int m_crossCooldownBaseMs = 1080;
    int m_santaWaterCooldownMs = 0;
    int m_santaWaterCooldownBaseMs = 1260;
    int m_attackDamage = 1;
    int m_bladeWeaponLevel = 1;
    int m_projectileCount = 1;
    int m_projectilePierce = 1;
    int m_orbitBladeLevel = 0;
    int m_orbitBladeCount = 0;
    int m_orbitBladeDamage = 0;
    int m_fireWandLevel = 0;
    int m_fireWandDamage = 3;
    qreal m_fireWandProjectileSpeedMultiplier = 1.0f;
    int m_garlicLevel = 0;
    int m_garlicDamage = 1;
    int m_crossLevel = 0;
    int m_crossDamage = 2;
    int m_crossAmount = 1;
    int m_crossPierce = 1000;
    int m_santaWaterLevel = 0;
    int m_santaWaterDamage = 2;
    int m_santaWaterAmount = 1;
    int m_santaWaterDurationMs = 1400;
    int m_wingsPassiveLevel = 0;
    int m_emptyTomePassiveLevel = 0;
    int m_candelabradorPassiveLevel = 0;
    int m_attractorbPassiveLevel = 0;
    int m_hollowHeartPassiveLevel = 0;
    int m_spinachPassiveLevel = 0;
    bool m_fireWandEvolved = false;
    bool m_santaWaterEvolved = false;
    int m_lastWaveIndex = -1;
    QSet<int> m_triggeredEventSeconds;
    qreal m_moveSpeed = 0.28f;
    qreal m_pickupRange = 0.11f;
    qreal m_projectileSpeed = 0.74f;
    qreal m_orbitBladeRadius = 0.12f;
    qreal m_orbitBladeAngularSpeedDeg = 140.0f;
    qreal m_orbitAngleDeg = 0.0f;
    qreal m_fireWandRange = 0.74f;
    qreal m_garlicRadius = 0.12f;
    qreal m_crossSpeed = 0.74f;
    qreal m_crossRadius = 0.018f;
    qreal m_santaWaterRadius = 0.08f;
    RenderSnapshot m_renderSnapshot;
    QVariantList m_cachedLevelUpChoices;
    QVariantList m_cachedChestRewards;
    QVariantList m_cachedWeaponSlots;
    QVariantList m_cachedPassiveSlots;
    QList<ChestReward> m_chestRewardEntries;
    QList<Pickup> m_queuedChests;
    QString m_chestTitle;
    QString m_statusText;
    QString m_upgradeSummary;
    QString m_waveLabel;
};
