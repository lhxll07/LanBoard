#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QVariantList>
#include <QVector2D>

#include "src/common/gamecontrollerbase.h"
#include "survivorsimulation.h"
#include "survivorworld.h"

class SurvivorController : public GameControllerBase
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool gameOver READ isGameOver NOTIFY gameOverChanged)
    Q_PROPERTY(bool networkSession READ isNetworkSession NOTIFY runningChanged)
    Q_PROPERTY(int hp READ hp NOTIFY stateChanged)
    Q_PROPERTY(int maxHp READ maxHp NOTIFY stateChanged)
    Q_PROPERTY(int level READ level NOTIFY stateChanged)
    Q_PROPERTY(int exp READ exp NOTIFY stateChanged)
    Q_PROPERTY(int expToNext READ expToNext NOTIFY stateChanged)
    Q_PROPERTY(int killCount READ killCount NOTIFY stateChanged)
    Q_PROPERTY(int localKillCount READ localKillCount NOTIFY stateChanged)
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
    Q_PROPERTY(QVariantList damageNumbers READ damageNumbers NOTIFY frameChanged)
    Q_PROPERTY(QString waveLabel READ waveLabel NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString upgradeSummary READ upgradeSummary NOTIFY stateChanged)
    Q_PROPERTY(QVariantList leaderboard READ leaderboard NOTIFY stateChanged)
    Q_PROPERTY(bool waitingForOtherPlayer READ waitingForOtherPlayer NOTIFY stateChanged)

public:
    using Enemy = LanBoard::Survivor::Enemy;
    using Pickup = LanBoard::Survivor::Pickup;
    using Projectile = LanBoard::Survivor::Projectile;
    using Zone = LanBoard::Survivor::Zone;
    using UpgradeChoice = LanBoard::Survivor::UpgradeChoice;
    using ChestReward = LanBoard::Survivor::ChestReward;
    using PlayerState = LanBoard::Survivor::PlayerState;
    using DamageNumber = LanBoard::Survivor::DamageNumber;
    using RenderPlayer = LanBoard::Survivor::RenderPlayer;
    using RenderEnemy = LanBoard::Survivor::RenderEnemy;
    using RenderOrbital = LanBoard::Survivor::RenderOrbital;
    using RenderProjectile = LanBoard::Survivor::RenderProjectile;
    using RenderPickup = LanBoard::Survivor::RenderPickup;
    using RenderZone = LanBoard::Survivor::RenderZone;
    using RenderDamageNumber = LanBoard::Survivor::RenderDamageNumber;
    using RenderSnapshot = LanBoard::Survivor::RenderSnapshot;

    explicit SurvivorController(QObject *parent = nullptr);

    bool isRunning() const { return m_matchState.running; }
    bool isGameOver() const override { return m_matchState.gameOver; }
    int winner() const override { return m_matchWinner; }
    bool isNetworkSession() const { return m_networkSession; }
    int hp() const { const PlayerState *player = hudPlayerState(); return player ? player->hp : 0; }
    int maxHp() const { const PlayerState *player = hudPlayerState(); return player ? player->maxHp : 0; }
    int level() const { return m_matchState.level; }
    int exp() const { return m_matchState.exp; }
    int expToNext() const { return m_matchState.expToNext; }
    int killCount() const { return m_matchState.killCount; }
    int localKillCount() const { const PlayerState *player = localPlayerState(); return player ? player->killCount : 0; }
    int survivalTimeSec() const { return m_matchState.survivalTimeMs / 1000; }
    qreal playerX() const { return cameraAnchor().x(); }
    qreal playerY() const { return cameraAnchor().y(); }
    qreal auraRadius() const {
        if (m_networkSession && !m_networkAuthoritative)
            return m_networkAuraRadius;
        const PlayerState *player = localPlayerState();
        return player ? playerAuraRadius(*player) : 0.0;
    }
    qreal radarRange() const { return 2.6; }
    bool isLevelUpPending() const { return m_matchState.levelUpPending; }
    QVariantList levelUpChoices() const;
    bool isChestPending() const { return m_matchState.chestPending; }
    QVariantList chestRewards() const;
    QString chestTitle() const { return m_matchState.chestTitle; }
    int interactionPlayerId() const { return m_matchState.pendingInteractionPlayerId; }
    QVariantList weaponSlots() const;
    QVariantList passiveSlots() const;
    QVariantList damageNumbers() const { return m_cachedDamageNumbers; }
    QString waveLabel() const { return m_matchState.waveLabel; }
    QString statusText() const { return m_statusText; }
    QString upgradeSummary() const { return m_upgradeSummary; }
    QVariantList leaderboard() const { return m_cachedLeaderboard; }
    bool garlicAuraEvolved() const {
        const PlayerState *player = hudPlayerState();
        return player && player->garlicEvolved;
    }
    bool waitingForOtherPlayer() const {
        return m_matchState.pendingInteractionPlayerId >= 0
            && m_matchState.pendingInteractionPlayerId != m_localPlayerId
            && !m_matchState.levelUpPending
            && !m_matchState.chestPending;
    }
    const RenderSnapshot &renderSnapshot() const { return m_renderSnapshot; }

    // ---- GameControllerBase ----
    void startNewGame() override;
    void reset() override;

    Q_INVOKABLE void startRun(bool networkSession = false);
    Q_INVOKABLE void stopRun();
    Q_INVOKABLE void setMoveInput(qreal horizontal, qreal vertical);
    Q_INVOKABLE void chooseLevelUp(const QString &upgradeId);
    Q_INVOKABLE void closeChestRewards();
    void configureNetworkSession(const QVariantList &activePlayers,
                                 int localPlayerId,
                                 bool networkSession,
                                 bool authoritative);
    QByteArray buildFastNetworkPacket(int playerId) const;
    QByteArray buildHudNetworkPacket(int playerId) const;
    void applyFastNetworkPacket(const QByteArray &payload);
    void applyHudNetworkPacket(const QByteArray &payload);
    void setRemoteMoveInput(int playerId, qreal horizontal, qreal vertical);
    void finalizeGameOver(int winner = 0);

signals:
    void stateChanged();
    void frameChanged();
    void runningChanged();
    void gameOverChanged();
    void localInputChanged(qreal horizontal, qreal vertical);
    void levelUpChoiceRequested(QString upgradeId);
    void chestRewardsCloseRequested();
    void networkSyncRequested(bool includeHudDetails);

private:
    void resetState();
    void initializePlayers();
    void spawnEnemy(bool elite = false, int forcedKind = -1, bool forceChestCarrier = false);
    void tick();
    void simulateStep(int elapsedMs);
    void applyAutoAttack();
    void updateGarlicAura();
    void trimEnemyPopulation();
    void resolveEnemySeparation();
    void updateOrbitals(qreal deltaSec, int elapsedMs);
    void updateProjectiles(qreal deltaSec, int elapsedMs);
    void updateZones(qreal deltaSec, int elapsedMs);
    void collectPickups(qreal deltaSec);
    void defeatEnemy(int index, int ownerPlayerId = -1);
    void gainExp(PlayerState &player, int amount);
    void prepareLevelUpChoices(PlayerState &player);
    void applyUpgrade(PlayerState &player, const QString &upgradeId);
    void applyWeaponUpgradeLevel(PlayerState &player,
                                 LanBoard::Survivor::WeaponType type,
                                 int newLevel);
    void applyPassiveUpgradeLevel(PlayerState &player,
                                  LanBoard::Survivor::PassiveType type,
                                  int newLevel);
    void addDamageNumber(const QVector2D &position, int amount, bool elite);
    void damageEnemy(int enemyIndex, int damage, int ownerPlayerId = -1);
    int healPlayer(PlayerState &player, int amount);
    void refreshDerivedStats();
    void refreshFrameCache();
    void syncHudState();
    void syncInteractionState();
    void refreshLevelUpChoiceCache();
    void refreshChestRewardCache();
    void refreshWeaponSlotCache();
    void refreshPassiveSlotCache();
    void refreshHudSlotCaches();
    void refreshLeaderboardCache();
    void openChest(PlayerState &player, const Pickup &pickup);
    void enqueueChest(PlayerState &player, const Pickup &pickup);
    void tryOpenQueuedChest();
    int rollChestRewardCount(const PlayerState *player = nullptr) const;
    QList<QString> currentChestUpgradeCandidates(const PlayerState &player) const;
    QList<QString> currentEvolutionCandidates(const PlayerState &player) const;
    bool canEvolveWeapon(const PlayerState &player, const QString &weaponId) const;
    void applyChestReward(PlayerState &player, const QString &upgradeId, bool evolved);
    bool applyEvolution(PlayerState &player, const QString &weaponId);
    QString evolvedTitleForWeapon(const QString &weaponId) const;
    QString evolvedDescriptionForWeapon(const QString &weaponId) const;
    bool tryApplyHit(int enemyIndex,
                     const QVector2D &sourcePosition,
                     int sourceId,
                     int ownerPlayerId,
                     int hitIntervalMs,
                     int baseDamage,
                     qreal damageVariance,
                     qreal knockback,
                     bool amplifyKnockback = false);
    void applyKnockbackToEnemy(Enemy &enemy, const QVector2D &sourcePosition, qreal knockback);
    void decayEnemyHitCooldowns(Enemy &enemy, int elapsedMs);
    int rollDamage(int baseDamage, qreal damageVariance) const;
    int levelForUpgrade(const PlayerState &player, const QString &upgradeId) const;
    int maxLevelForUpgrade(const QString &upgradeId) const;
    bool isWeaponUpgrade(const QString &upgradeId) const;
    qreal currentDamageMultiplier(const PlayerState &player) const;
    qreal currentAreaMultiplier(const PlayerState &player) const;
    qreal playerAuraRadius(const PlayerState &player) const;
    qreal currentCooldownMultiplier(const PlayerState &player) const;
    qreal currentDurationMultiplier(const PlayerState &player) const;
    qreal currentProjectileSpeedMultiplier(const PlayerState &player) const;
    qreal currentMoveSpeed(const PlayerState &player) const;
    qreal currentMagnetRange(const PlayerState &player) const;
    int currentMaxHpValue(const PlayerState &player) const;
    qreal currentRecoveryPerSecond(const PlayerState &player) const;
    qreal currentLuckMultiplier(const PlayerState &player) const;
    bool isWeaponEvolved(const PlayerState &player, LanBoard::Survivor::WeaponType type) const;
    qreal heavenSwordCritChance(const PlayerState &player) const;
    QString titleForUpgrade(const QString &upgradeId) const;
    QString categoryForUpgrade(const QString &upgradeId) const;
    QString descriptionForUpgrade(const QString &upgradeId, int currentLevel) const;
    void refreshUpgradeSummary();
    void refreshWaveLabel();
    void updateStatusText();
    const LanBoard::Survivor::WaveTemplate &currentWaveTemplate() const;
    int rollSpawnKind(const LanBoard::Survivor::SpawnWeight *weights, int count) const;
    void processWaveEvents();
    void spawnBatSwarm(int count, qreal speedMultiplier);
    void spawnFlowerWall(int count, qreal ringRadius, qreal inwardSpeed, int hpOverride);
    int currentSpawnIntervalMs() const;
    int currentSpawnBurstCount() const;
    int currentEnemyCap() const;
    int currentEliteSpawnIntervalMs() const;
    int currentEliteSpawnBurstCount() const;
    int scaledEnemyCount(int baseCount, qreal extraPerPlayer) const;
    int currentWaveIndex() const;
    int currentEnemyKind() const;
    int currentEliteKind() const;
    bool hasLivingBoss() const;
    QVector2D playerAnchor() const;
    QVector2D cameraAnchor() const;
    QVector2D cameraAnchorForPlayer(int playerId) const;
    QVector2D combatAnchorForPosition(const QVector2D &position) const;
    QVector2D nextSpawnAnchor();
    PlayerState *playerStateById(int playerId);
    const PlayerState *playerStateById(int playerId) const;
    PlayerState *hudPlayerState();
    const PlayerState *hudPlayerState() const;
    PlayerState *interactionPlayerState();
    const PlayerState *interactionPlayerState() const;
    PlayerState *localPlayerState();
    const PlayerState *localPlayerState() const;
    int nearestLivingPlayerIndex(const QVector2D &position) const;
    QList<int> livingPlayerIndices() const;
    void syncPlayerMaxHp();
    void emitNetworkSyncIfNeeded(bool force = false);
    RenderSnapshot buildNetworkRenderSnapshot(int playerId) const;
    QVariantList exportDamageNumberVariantList() const;
    void adoptRemoteSnapshot(const QVector<PlayerState> &players,
                             const RenderSnapshot &snapshot,
                             bool immediate);
    void stepRemoteInterpolation(int elapsedMs);

    static constexpr int TickIntervalMs = 16;
    static constexpr qreal NetworkCullRadius = 3.4;

    QTimer m_tickTimer;
    QElapsedTimer m_frameTimer;
    LanBoard::Survivor::MatchState m_matchState;
    QList<UpgradeChoice> m_levelUpChoices;
    bool m_networkSession = false;
    bool m_networkAuthoritative = true;
    int m_spawnAnchorCursor = 0;
    qreal m_networkAuraRadius = 0.0f;
    int m_networkBroadcastAccumulatorMs = 0;
    int m_networkHudBroadcastAccumulatorMs = 0;
    int m_localPlayerId = 0;
    int m_matchWinner = 0;
    quint64 m_networkStateSequence = 0;
    QVariantList m_sessionActivePlayers;
    RenderSnapshot m_renderSnapshot;
    QVector<PlayerState> m_networkBasePlayers;
    QVector<PlayerState> m_networkTargetPlayers;
    RenderSnapshot m_networkBaseSnapshot;
    RenderSnapshot m_networkTargetSnapshot;
    quint64 m_lastAppliedFastStateSeq = 0;
    quint64 m_lastAppliedHudStateSeq = 0;
    int m_networkInterpolationElapsedMs = 0;
    int m_networkInterpolationDurationMs = 33;
    bool m_hasNetworkInterpolationTarget = false;
    QVector2D m_localPredictedPosition;
    QVector2D m_localAuthoritativePosition;
    bool m_hasLocalPrediction = false;
    QVariantList m_cachedLevelUpChoices;
    QVariantList m_cachedChestRewards;
    QVariantList m_cachedWeaponSlots;
    QVariantList m_cachedPassiveSlots;
    QVariantList m_cachedDamageNumbers;
    QVariantList m_cachedLeaderboard;
    QList<ChestReward> m_chestRewardEntries;
    QString m_statusText;
    QString m_upgradeSummary;
    bool m_networkHudDirty = false;

    static constexpr int NetworkSnapshotIntervalMs = 25;
    static constexpr int NetworkHudSnapshotIntervalMs = 200;
};
