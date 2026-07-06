#include "survivorcontroller.h"

#include <algorithm>
#include <limits>
#include <QRandomGenerator>
#include <QVariantMap>
#include <QtMath>

#include "survivornetcodec.h"
#include "survivorruntime.h"
#include "survivorsimulation.h"

namespace {

using LanBoard::Survivor::BatEnemy;
using LanBoard::Survivor::ZombieEnemy;
using LanBoard::Survivor::SkeletonEnemy;
using LanBoard::Survivor::WerewolfEnemy;
using LanBoard::Survivor::FlowerEnemy;
using LanBoard::Survivor::OgreEnemy;
using LanBoard::Survivor::GiantBatEnemy;
using LanBoard::Survivor::BlueGemPickup;
using LanBoard::Survivor::GreenGemPickup;
using LanBoard::Survivor::RedGemPickup;
using LanBoard::Survivor::ChestPickup;
using LanBoard::Survivor::UpgradeTemplate;
using LanBoard::Survivor::WaveTemplate;
using LanBoard::Survivor::BossSpawnTemplate;
using LanBoard::Survivor::SpawnDistanceMin;
using LanBoard::Survivor::SpawnDistanceMax;
using LanBoard::Survivor::ProjectileCleanupDistance;
using LanBoard::Survivor::ProjectileCleanupDistanceSquared;
using LanBoard::Survivor::EnemySeparationCellSize;
using LanBoard::Survivor::EvolutionChestStartSec;
using LanBoard::Survivor::normalizedInput;
using LanBoard::Survivor::rotatedVector;
using LanBoard::Survivor::expRequirementForLevel;
using LanBoard::Survivor::spatialCellKey;
using LanBoard::Survivor::pickupKindForExp;
using LanBoard::Survivor::pickupRadiusForKind;
using LanBoard::Survivor::circlesOverlap;
using LanBoard::Survivor::lerpReal;
using LanBoard::Survivor::lerpInt;

const UpgradeTemplate *kWeaponUpgradePool = LanBoard::Survivor::weaponUpgradePool();
const int kWeaponUpgradePoolCount = LanBoard::Survivor::weaponUpgradePoolCount();
const UpgradeTemplate *kPassiveUpgradePool = LanBoard::Survivor::passiveUpgradePool();
const int kPassiveUpgradePoolCount = LanBoard::Survivor::passiveUpgradePoolCount();
const WaveTemplate *kWaveTemplates = LanBoard::Survivor::waveTemplates();
const int kWaveTemplateCount = LanBoard::Survivor::waveTemplateCount();
const BossSpawnTemplate *kBossSpawnSchedule = LanBoard::Survivor::bossSpawnSchedule();
const int kBossSpawnScheduleCount = LanBoard::Survivor::bossSpawnScheduleCount();

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
    const PlayerState *player = hudPlayerState();

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

    if (!player) {
        while (m_cachedWeaponSlots.size() < 6) {
            QVariantMap emptySlot;
            emptySlot[QStringLiteral("title")] = QStringLiteral("空槽");
            emptySlot[QStringLiteral("subtitle")] = QStringLiteral("后续武器位");
            emptySlot[QStringLiteral("filled")] = false;
            emptySlot[QStringLiteral("accent")] = QStringLiteral("#4A655C");
            m_cachedWeaponSlots.append(emptySlot);
        }
        return;
    }

    appendWeapon(QStringLiteral("飞刀"),
                 player->bladeWeaponLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 伤害 %2 / 数量 %3 / 穿透 %4")
                          .arg(player->bladeWeaponLevel)
                          .arg(qRound(player->attackDamage * currentDamageMultiplier(*player)))
                          .arg(player->projectileCount)
                          .arg(player->projectilePierce)
                    : QStringLiteral("未解锁"),
                 player->bladeWeaponLevel > 0,
                 QStringLiteral("#F6D782"));

    appendWeapon(QStringLiteral("秘典"),
                 player->orbitBladeLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 环刃 %2 / 伤害 %3")
                          .arg(player->orbitBladeLevel)
                          .arg(player->orbitBladeCount)
                          .arg(qRound(player->orbitBladeDamage * currentDamageMultiplier(*player)))
                    : QStringLiteral("未解锁"),
                 player->orbitBladeLevel > 0,
                 QStringLiteral("#B4E0D2"));

    appendWeapon(player->fireWandEvolved ? QStringLiteral("地狱火") : QStringLiteral("火杖"),
                 player->fireWandLevel > 0
                    ? QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 冷却 %4s / 弹速 x%5")
                          .arg(player->fireWandEvolved ? QStringLiteral("已进化 · ") : QString())
                          .arg(player->fireWandLevel)
                          .arg(qRound(player->fireWandDamage * currentDamageMultiplier(*player)))
                          .arg(player->fireWandCooldownBaseMs * currentCooldownMultiplier(*player) / 1000.0, 0, 'f', 2)
                          .arg(QString::number(player->fireWandProjectileSpeedMultiplier, 'f', 2))
                    : QStringLiteral("未解锁"),
                 player->fireWandLevel > 0,
                 QStringLiteral("#E98B61"));

    appendWeapon(QStringLiteral("大蒜"),
                 player->garlicLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 伤害 %2 / 半径 %3")
                          .arg(player->garlicLevel)
                          .arg(qRound(player->garlicDamage * currentDamageMultiplier(*player)))
                          .arg(QString::number(m_matchState.worldRuntime.garlicRadius * currentAreaMultiplier(*player), 'f', 2))
                    : QStringLiteral("未解锁"),
                 player->garlicLevel > 0,
                 QStringLiteral("#D8F0B5"));

    appendWeapon(QStringLiteral("十字架"),
                 player->crossLevel > 0
                    ? QStringLiteral("Lv.%1/8 · 伤害 %2 / 数量 %3")
                          .arg(player->crossLevel)
                          .arg(qRound(player->crossDamage * currentDamageMultiplier(*player)))
                          .arg(player->crossAmount)
                    : QStringLiteral("未解锁"),
                 player->crossLevel > 0,
                 QStringLiteral("#EFD7A6"));

    appendWeapon(player->santaWaterEvolved ? QStringLiteral("黑波拉") : QStringLiteral("圣水"),
                 player->santaWaterLevel > 0
                    ? QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4 / 冷却 %5s")
                          .arg(player->santaWaterEvolved ? QStringLiteral("已进化 · ") : QString())
                          .arg(player->santaWaterLevel)
                          .arg(qRound(player->santaWaterDamage * currentDamageMultiplier(*player)))
                          .arg(player->santaWaterAmount)
                          .arg(player->santaWaterCooldownBaseMs * currentCooldownMultiplier(*player) / 1000.0, 0, 'f', 2)
                    : QStringLiteral("未解锁"),
                 player->santaWaterLevel > 0,
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
    const PlayerState *player = hudPlayerState();

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

    if (!player) {
        while (m_cachedPassiveSlots.size() < 6) {
            QVariantMap emptySlot;
            emptySlot[QStringLiteral("title")] = QStringLiteral("空槽");
            emptySlot[QStringLiteral("subtitle")] = QStringLiteral("后续被动位");
            emptySlot[QStringLiteral("filled")] = false;
            emptySlot[QStringLiteral("accent")] = QStringLiteral("#4A655C");
            m_cachedPassiveSlots.append(emptySlot);
        }
        return;
    }

    if (player->wingsPassiveLevel > 0) {
        appendPassive(QStringLiteral("翅膀"),
                      QStringLiteral("Lv.%1/5 · 移速 %2").arg(player->wingsPassiveLevel).arg(QString::number(currentMoveSpeed(*player), 'f', 2)),
                      QStringLiteral("#D7B7F2"));
    }
    if (player->emptyTomePassiveLevel > 0) {
        appendPassive(QStringLiteral("空白之书"),
                      QStringLiteral("Lv.%1/5 · 冷却 x%2").arg(player->emptyTomePassiveLevel).arg(QString::number(currentCooldownMultiplier(*player), 'f', 2)),
                      QStringLiteral("#E7C96C"));
    }
    if (player->candelabradorPassiveLevel > 0) {
        appendPassive(QStringLiteral("烛台"),
                      QStringLiteral("Lv.%1/5 · 范围 x%2").arg(player->candelabradorPassiveLevel).arg(QString::number(currentAreaMultiplier(*player), 'f', 2)),
                      QStringLiteral("#F3D48A"));
    }
    if (player->attractorbPassiveLevel > 0) {
        appendPassive(QStringLiteral("磁力珠"),
                      QStringLiteral("Lv.%1/5 · 吸附 %2").arg(player->attractorbPassiveLevel).arg(QString::number(currentMagnetRange(*player), 'f', 2)),
                      QStringLiteral("#86D9C7"));
    }
    if (player->hollowHeartPassiveLevel > 0) {
        appendPassive(QStringLiteral("空心心脏"),
                      QStringLiteral("Lv.%1/5 · 生命 %2").arg(player->hollowHeartPassiveLevel).arg(currentMaxHpValue(*player)),
                      QStringLiteral("#E48D81"));
    }
    if (player->spinachPassiveLevel > 0) {
        appendPassive(QStringLiteral("菠菜"),
                      QStringLiteral("Lv.%1/5 · 伤害 x%2").arg(player->spinachPassiveLevel).arg(QString::number(currentDamageMultiplier(*player), 'f', 2)),
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

void SurvivorController::startRun(bool networkSession)
{
    m_networkSession = networkSession;
    resetState();
    m_matchState.running = true;
    updateStatusText();
    m_frameTimer.start();
    m_tickTimer.start();
    refreshFrameCache();
    emitNetworkSyncIfNeeded(true);
    emit runningChanged();
    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::stopRun()
{
    if (!m_matchState.running && !m_matchState.gameOver)
        return;

    m_tickTimer.stop();
    m_matchState.running = false;
    m_matchState.gameOver = false;
    m_matchState.levelUpPending = false;
    m_matchState.chestPending = false;
    m_levelUpChoices.clear();
    m_chestRewardEntries.clear();
    m_matchState.chestTitle.clear();
    m_networkBroadcastAccumulatorMs = 0;
    m_networkHudBroadcastAccumulatorMs = 0;
    m_networkBasePlayers.clear();
    m_networkTargetPlayers.clear();
    m_networkBaseSnapshot = {};
    m_networkTargetSnapshot = {};
    m_lastAppliedFastStateSeq = 0;
    m_networkInterpolationElapsedMs = 0;
    m_hasNetworkInterpolationTarget = false;
    m_localPredictedPosition = QVector2D();
    m_localAuthoritativePosition = QVector2D();
    m_hasLocalPrediction = false;
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
    const QVector2D input = normalizedInput(QVector2D(horizontal, vertical));
    if (PlayerState *player = localPlayerState()) {
        player->moveInput = input;
        if (!input.isNull())
            player->facingDirection = input;
    }

    if (m_networkSession && !m_networkAuthoritative)
        emit localInputChanged(input.x(), input.y());
}

void SurvivorController::chooseLevelUp(const QString &upgradeId)
{
    const QString normalizedUpgradeId = upgradeId.trimmed();
    if (m_networkSession && !m_networkAuthoritative) {
        if (m_matchState.levelUpPending && !normalizedUpgradeId.isEmpty())
            emit levelUpChoiceRequested(normalizedUpgradeId);
        return;
    }
    PlayerState *player = interactionPlayerState();
    if (!player)
        player = hudPlayerState();
    if (!player || player->levelUpChoices.isEmpty() || normalizedUpgradeId.isEmpty())
        return;

    bool found = false;
    for (const UpgradeChoice &choice : std::as_const(player->levelUpChoices)) {
        if (choice.id == normalizedUpgradeId) {
            found = true;
            break;
        }
    }
    if (!found)
        return;

    applyUpgrade(*player, normalizedUpgradeId);
    if (player->pendingLevelUps > 0)
        --player->pendingLevelUps;

    player->levelUpChoices.clear();
    m_matchState.pendingInteractionPlayerId = -1;
    m_matchState.pendingInteractionElapsedMs = 0;
    if (player->pendingLevelUps > 0) {
        prepareLevelUpChoices(*player);
    } else {
        tryOpenQueuedChest();
    }

    syncHudState();
    m_frameTimer.restart();
    refreshFrameCache();
    emitNetworkSyncIfNeeded(true);
    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::closeChestRewards()
{
    if (m_networkSession && !m_networkAuthoritative) {
        if (m_matchState.chestPending)
            emit chestRewardsCloseRequested();
        return;
    }
    PlayerState *player = interactionPlayerState();
    if (!player || player->chestRewardEntries.isEmpty())
        return;

    player->chestRewardEntries.clear();
    player->chestTitle.clear();
    m_matchState.pendingInteractionPlayerId = -1;
    m_matchState.pendingInteractionElapsedMs = 0;

    if (player->pendingLevelUps > 0) {
        prepareLevelUpChoices(*player);
    } else {
        tryOpenQueuedChest();
    }

    syncHudState();
    emitNetworkSyncIfNeeded(true);
    emit stateChanged();
    emit frameChanged();
}

void SurvivorController::resetState()
{
    m_tickTimer.stop();
    LanBoard::Survivor::initializeMatchState(m_matchState);
    initializePlayers();
    m_levelUpChoices.clear();
    m_chestRewardEntries.clear();
    m_networkBroadcastAccumulatorMs = 0;
    m_networkHudBroadcastAccumulatorMs = 0;
    m_renderSnapshot = {};
    m_networkBasePlayers.clear();
    m_networkTargetPlayers.clear();
    m_networkBaseSnapshot = {};
    m_networkTargetSnapshot = {};
    m_lastAppliedFastStateSeq = 0;
    m_networkInterpolationElapsedMs = 0;
    m_hasNetworkInterpolationTarget = false;
    m_localPredictedPosition = QVector2D();
    m_localAuthoritativePosition = QVector2D();
    m_hasLocalPrediction = false;
    m_frameTimer.invalidate();
    refreshDerivedStats();
    refreshWaveLabel();
    if (!m_networkSession || m_networkAuthoritative) {
        for (int i = 0; i < 10; ++i)
            spawnEnemy();
    }
    syncHudState();
    refreshFrameCache();
}

void SurvivorController::initializePlayers()
{
    LanBoard::Survivor::Runtime::initializeSessionPlayers(m_matchState,
                                                          m_sessionActivePlayers,
                                                          m_localPlayerId);
}

void SurvivorController::configureNetworkSession(const QVariantList &activePlayers,
                                                 int localPlayerId,
                                                 bool networkSession,
                                                 bool authoritative)
{
    m_sessionActivePlayers = activePlayers;
    m_localPlayerId = localPlayerId;
    m_networkSession = networkSession;
    m_networkAuthoritative = authoritative;
}

void SurvivorController::setRemoteMoveInput(int playerId, qreal horizontal, qreal vertical)
{
    if (!m_networkAuthoritative)
        return;

    const QVector2D input = normalizedInput(QVector2D(horizontal, vertical));
    for (PlayerState &player : m_matchState.players) {
        if (player.playerId != playerId)
            continue;
        player.moveInput = input;
        if (!input.isNull())
            player.facingDirection = input;
        return;
    }
}

void SurvivorController::applyFastNetworkPacket(const QByteArray &payload)
{
    if (!m_networkSession || m_networkAuthoritative)
        return;

    LanBoard::Survivor::NetCodec::FastNetworkState decoded;
    if (!LanBoard::Survivor::NetCodec::decodeFastNetworkState(payload,
                                                              decoded,
                                                              m_matchState.players,
                                                              m_localPlayerId)) {
        return;
    }
    if (decoded.seq <= m_lastAppliedFastStateSeq)
        return;
    m_lastAppliedFastStateSeq = decoded.seq;

    const bool previousRunning = m_matchState.running;
    const bool previousGameOver = m_matchState.gameOver;
    const int previousHp = hp();
    const int previousMaxHp = maxHp();
    const int previousLevel = m_matchState.level;
    const int previousExp = m_matchState.exp;
    const int previousExpToNext = m_matchState.expToNext;
    const bool previousLevelUpPending = m_matchState.levelUpPending;
    const bool previousChestPending = m_matchState.chestPending;
    const QString previousChestTitle = m_matchState.chestTitle;
    const QString previousWaveLabel = m_matchState.waveLabel;
    const QString previousStatusText = m_statusText;
    const QString previousUpgradeSummary = m_upgradeSummary;

    m_matchState.running = decoded.running;
    m_matchState.gameOver = decoded.gameOver;
    m_matchState.survivalTimeMs = decoded.survivalTimeMs;
    m_matchState.killCount = decoded.killCount;
    m_matchState.waveLabel = decoded.waveLabel;
    m_matchState.pendingInteractionPlayerId = decoded.interactionPlayerId;
    m_networkAuraRadius = decoded.auraRadius;

    if (decoded.hasLocalPlayer) {
        if (PlayerState *local = localPlayerState()) {
            const QVector2D preservedPosition = local->position;
            const QVector2D preservedMoveInput = local->moveInput;
            const QVector2D preservedFacing = local->facingDirection;
            const bool preservedLocalFlag = local->local;
            const int preservedPendingLevelUps = local->pendingLevelUps;
            const int preservedAttackCooldownMs = local->attackCooldownMs;
            const int preservedOrbitBladeCooldownMs = local->orbitBladeCooldownMs;
            const int preservedOrbitBladeActiveMs = local->orbitBladeActiveMs;
            const int preservedFireWandCooldownMs = local->fireWandCooldownMs;
            const int preservedCrossCooldownMs = local->crossCooldownMs;
            const int preservedSantaWaterCooldownMs = local->santaWaterCooldownMs;
            const qreal preservedContactDamageCarry = local->contactDamageCarry;
            const QList<UpgradeChoice> preservedChoices = local->levelUpChoices;
            const QList<ChestReward> preservedRewards = local->chestRewardEntries;
            const QString preservedChestTitle = local->chestTitle;

            *local = decoded.localPlayer;
            local->position = preservedPosition;
            local->moveInput = preservedMoveInput;
            local->facingDirection = preservedFacing;
            local->local = preservedLocalFlag;
            local->pendingLevelUps = preservedPendingLevelUps;
            local->attackCooldownMs = preservedAttackCooldownMs;
            local->orbitBladeCooldownMs = preservedOrbitBladeCooldownMs;
            local->orbitBladeActiveMs = preservedOrbitBladeActiveMs;
            local->fireWandCooldownMs = preservedFireWandCooldownMs;
            local->crossCooldownMs = preservedCrossCooldownMs;
            local->santaWaterCooldownMs = preservedSantaWaterCooldownMs;
            local->contactDamageCarry = preservedContactDamageCarry;
            local->levelUpChoices = preservedChoices;
            local->chestRewardEntries = preservedRewards;
            local->chestTitle = preservedChestTitle;
        }
    }
    m_cachedDamageNumbers.clear();

    const bool immediate = m_renderSnapshot.players.isEmpty();
    adoptRemoteSnapshot(decoded.players, decoded.snapshot, immediate);
    syncHudState();

    if (previousRunning != m_matchState.running)
        emit runningChanged();
    if (previousGameOver != m_matchState.gameOver)
        emit gameOverChanged();
    const bool shouldEmitStateChanged = previousHp != hp()
        || previousMaxHp != maxHp()
        || previousLevel != m_matchState.level
        || previousExp != m_matchState.exp
        || previousExpToNext != m_matchState.expToNext
        || previousLevelUpPending != m_matchState.levelUpPending
        || previousChestPending != m_matchState.chestPending
        || previousChestTitle != m_matchState.chestTitle
        || previousWaveLabel != m_matchState.waveLabel
        || previousStatusText != m_statusText
        || previousUpgradeSummary != m_upgradeSummary;
    if (shouldEmitStateChanged)
        emit stateChanged();
    emit frameChanged();
}

void SurvivorController::applyHudNetworkPacket(const QByteArray &payload)
{
    if (!m_networkSession || m_networkAuthoritative)
        return;

    LanBoard::Survivor::NetCodec::HudNetworkState decoded;
    if (!LanBoard::Survivor::NetCodec::decodeHudNetworkState(payload, decoded))
        return;

    const bool previousLevelUpPending = m_matchState.levelUpPending;
    const bool previousChestPending = m_matchState.chestPending;
    const QString previousChestTitle = m_matchState.chestTitle;
    const QString previousStatusText = m_statusText;
    const QString previousUpgradeSummary = m_upgradeSummary;

    m_matchState.pendingInteractionPlayerId = decoded.interactionPlayerId;
    m_matchState.chestTitle = decoded.chestTitle;

    if (PlayerState *local = localPlayerState()) {
        if (local->playerId == decoded.interactionPlayerId) {
            local->levelUpChoices = decoded.levelUpChoices;
            local->chestRewardEntries = decoded.chestRewards;
            local->chestTitle = decoded.chestTitle;
        } else {
            local->levelUpChoices.clear();
            local->chestRewardEntries.clear();
            local->chestTitle.clear();
        }
    }

    syncHudState();
    const bool shouldEmitStateChanged = previousLevelUpPending != m_matchState.levelUpPending
        || previousChestPending != m_matchState.chestPending
        || previousChestTitle != m_matchState.chestTitle
        || previousStatusText != m_statusText
        || previousUpgradeSummary != m_upgradeSummary;
    if (shouldEmitStateChanged)
        emit stateChanged();
}

void SurvivorController::adoptRemoteSnapshot(const QVector<PlayerState> &players,
                                             const RenderSnapshot &snapshot,
                                             bool immediate)
{
    for (const PlayerState &player : players) {
        if (player.playerId != m_localPlayerId)
            continue;
        m_localAuthoritativePosition = player.position;
        if (immediate || !m_hasLocalPrediction) {
            m_localPredictedPosition = player.position;
            m_hasLocalPrediction = true;
        }
        break;
    }

    if (immediate || m_renderSnapshot.players.isEmpty()) {
        m_matchState.players = players;
        m_renderSnapshot = snapshot;
        m_networkBasePlayers = players;
        m_networkTargetPlayers = players;
        m_networkBaseSnapshot = snapshot;
        m_networkTargetSnapshot = snapshot;
        m_networkInterpolationElapsedMs = 0;
        m_hasNetworkInterpolationTarget = false;
        return;
    }

    m_networkBasePlayers = m_matchState.players;
    m_networkBaseSnapshot = m_renderSnapshot;
    m_networkTargetPlayers = players;
    m_networkTargetSnapshot = snapshot;
    m_networkInterpolationElapsedMs = 0;
    m_hasNetworkInterpolationTarget = true;
}

void SurvivorController::stepRemoteInterpolation(int elapsedMs)
{
    QVector2D preservedLocalInput;
    QVector2D preservedLocalFacing(1.0f, 0.0f);
    bool hasPreservedLocalState = false;
    if (const PlayerState *existingLocal = localPlayerState()) {
        preservedLocalInput = existingLocal->moveInput;
        preservedLocalFacing = existingLocal->facingDirection;
        hasPreservedLocalState = true;
    }

    if (m_hasNetworkInterpolationTarget) {
        m_networkInterpolationElapsedMs += elapsedMs;
        const qreal alpha = qBound<qreal>(0.0,
                                          static_cast<qreal>(m_networkInterpolationElapsedMs)
                                              / qMax(1, m_networkInterpolationDurationMs),
                                          1.0);

        m_matchState.players = m_networkTargetPlayers;
        for (PlayerState &player : m_matchState.players) {
            if (player.playerId == m_localPlayerId)
                continue;
            auto it = std::find_if(m_networkBasePlayers.cbegin(),
                                   m_networkBasePlayers.cend(),
                                   [&player](const PlayerState &candidate) {
                return candidate.playerId == player.playerId;
            });
            if (it == m_networkBasePlayers.cend())
                continue;

            player.position.setX(lerpReal(it->position.x(), player.position.x(), alpha));
            player.position.setY(lerpReal(it->position.y(), player.position.y(), alpha));
        }

        m_renderSnapshot = m_networkTargetSnapshot;
        m_renderSnapshot.players.clear();
        m_renderSnapshot.players.reserve(m_matchState.players.size());
        for (const PlayerState &player : m_matchState.players) {
            m_renderSnapshot.players.append({
                player.position.x(), player.position.y(), player.hp, player.maxHp,
                player.alive, player.local, player.colorIndex
            });
        }

        QHash<int, RenderEnemy> baseEnemies;
        baseEnemies.reserve(m_networkBaseSnapshot.enemies.size());
        for (const RenderEnemy &enemy : std::as_const(m_networkBaseSnapshot.enemies))
            baseEnemies.insert(enemy.id, enemy);
        for (RenderEnemy &to : m_renderSnapshot.enemies) {
            const auto it = baseEnemies.constFind(to.id);
            if (it == baseEnemies.cend())
                continue;
            const RenderEnemy &from = it.value();
            to.x = lerpReal(from.x, to.x, alpha);
            to.y = lerpReal(from.y, to.y, alpha);
            to.radius = lerpReal(from.radius, to.radius, alpha);
        }

        const int orbitalCount = qMin(m_networkBaseSnapshot.orbitals.size(), m_renderSnapshot.orbitals.size());
        for (int i = 0; i < orbitalCount; ++i) {
            const RenderOrbital &from = m_networkBaseSnapshot.orbitals.at(i);
            RenderOrbital &to = m_renderSnapshot.orbitals[i];
            to.x = lerpReal(from.x, to.x, alpha);
            to.y = lerpReal(from.y, to.y, alpha);
            to.radius = lerpReal(from.radius, to.radius, alpha);
        }

        const int projectileCount = qMin(m_networkBaseSnapshot.projectiles.size(), m_renderSnapshot.projectiles.size());
        for (int i = 0; i < projectileCount; ++i) {
            const RenderProjectile &from = m_networkBaseSnapshot.projectiles.at(i);
            RenderProjectile &to = m_renderSnapshot.projectiles[i];
            if (from.kind != to.kind)
                continue;
            to.x = lerpReal(from.x, to.x, alpha);
            to.y = lerpReal(from.y, to.y, alpha);
            to.radius = lerpReal(from.radius, to.radius, alpha);
        }

        const int pickupCount = qMin(m_networkBaseSnapshot.pickups.size(), m_renderSnapshot.pickups.size());
        for (int i = 0; i < pickupCount; ++i) {
            const RenderPickup &from = m_networkBaseSnapshot.pickups.at(i);
            RenderPickup &to = m_renderSnapshot.pickups[i];
            if (from.kind != to.kind || from.exp != to.exp)
                continue;
            to.x = lerpReal(from.x, to.x, alpha);
            to.y = lerpReal(from.y, to.y, alpha);
            to.radius = lerpReal(from.radius, to.radius, alpha);
        }

        const int zoneCount = qMin(m_networkBaseSnapshot.zones.size(), m_renderSnapshot.zones.size());
        for (int i = 0; i < zoneCount; ++i) {
            const RenderZone &from = m_networkBaseSnapshot.zones.at(i);
            RenderZone &to = m_renderSnapshot.zones[i];
            if (from.kind != to.kind)
                continue;
            to.x = lerpReal(from.x, to.x, alpha);
            to.y = lerpReal(from.y, to.y, alpha);
            to.radius = lerpReal(from.radius, to.radius, alpha);
            to.lifeMs = lerpInt(from.lifeMs, to.lifeMs, alpha);
        }

        if (alpha >= 1.0) {
            m_matchState.players = m_networkTargetPlayers;
            m_renderSnapshot = m_networkTargetSnapshot;
            m_networkBasePlayers = m_networkTargetPlayers;
            m_networkBaseSnapshot = m_networkTargetSnapshot;
            m_hasNetworkInterpolationTarget = false;
            m_networkInterpolationElapsedMs = 0;
        }
    }

    if (PlayerState *local = localPlayerState()) {
        if (hasPreservedLocalState) {
            local->moveInput = preservedLocalInput;
            local->facingDirection = preservedLocalFacing;
        }
        if (!m_hasLocalPrediction) {
            m_localPredictedPosition = local->position;
            m_localAuthoritativePosition = local->position;
            m_hasLocalPrediction = true;
        }

        const QVector2D input = normalizedInput(local->moveInput);
        if (local->alive && !input.isNull()) {
            local->facingDirection = input;
            m_localPredictedPosition += input * static_cast<float>(currentMoveSpeed(*local) * (elapsedMs / 1000.0));
        }

        const QVector2D error = m_localAuthoritativePosition - m_localPredictedPosition;
        if (error.lengthSquared() > 0.18f * 0.18f) {
            m_localPredictedPosition = m_localAuthoritativePosition;
        } else {
            m_localPredictedPosition += error * static_cast<float>(qMin<qreal>(1.0, elapsedMs / 1000.0 * 10.0));
        }
        local->position = m_localPredictedPosition;
        for (RenderPlayer &renderPlayer : m_renderSnapshot.players) {
            if (!renderPlayer.local)
                continue;
            renderPlayer.x = local->position.x();
            renderPlayer.y = local->position.y();
            break;
        }
    }
}

SurvivorController::PlayerState *SurvivorController::playerStateById(int playerId)
{
    return LanBoard::Survivor::Runtime::playerStateById(m_matchState, playerId);
}

const SurvivorController::PlayerState *SurvivorController::playerStateById(int playerId) const
{
    return LanBoard::Survivor::Runtime::playerStateById(m_matchState, playerId);
}

SurvivorController::PlayerState *SurvivorController::hudPlayerState()
{
    if (PlayerState *player = localPlayerState())
        return player;
    return m_matchState.players.isEmpty() ? nullptr : &m_matchState.players[0];
}

const SurvivorController::PlayerState *SurvivorController::hudPlayerState() const
{
    if (const PlayerState *player = localPlayerState())
        return player;
    return m_matchState.players.isEmpty() ? nullptr : &m_matchState.players.first();
}

SurvivorController::PlayerState *SurvivorController::interactionPlayerState()
{
    return playerStateById(m_matchState.pendingInteractionPlayerId);
}

const SurvivorController::PlayerState *SurvivorController::interactionPlayerState() const
{
    return playerStateById(m_matchState.pendingInteractionPlayerId);
}

SurvivorController::PlayerState *SurvivorController::localPlayerState()
{
    return LanBoard::Survivor::Runtime::localPlayerState(m_matchState, m_localPlayerId);
}

const SurvivorController::PlayerState *SurvivorController::localPlayerState() const
{
    return LanBoard::Survivor::Runtime::localPlayerState(m_matchState, m_localPlayerId);
}

QList<int> SurvivorController::livingPlayerIndices() const
{
    return LanBoard::Survivor::Runtime::livingPlayerIndices(m_matchState);
}

int SurvivorController::nearestLivingPlayerIndex(const QVector2D &position) const
{
    return LanBoard::Survivor::Runtime::nearestLivingPlayerIndex(m_matchState, position);
}

QVector2D SurvivorController::playerAnchor() const
{
    return LanBoard::Survivor::Runtime::playerAnchor(m_matchState, m_localPlayerId);
}

QVector2D SurvivorController::cameraAnchor() const
{
    return LanBoard::Survivor::Runtime::cameraAnchor(m_matchState, m_localPlayerId);
}

QVector2D SurvivorController::cameraAnchorForPlayer(int playerId) const
{
    if (const PlayerState *player = playerStateById(playerId))
        return player->position;
    return cameraAnchor();
}

SurvivorController::RenderSnapshot SurvivorController::buildNetworkRenderSnapshot(int playerId) const
{
    RenderSnapshot snapshot;
    const QVector2D origin = cameraAnchorForPlayer(playerId);

    auto insideRange = [origin](qreal x, qreal y, qreal radius = 0.0) {
        const qreal dx = x - origin.x();
        const qreal dy = y - origin.y();
        const qreal padded = qMax<qreal>(0.0, radius);
        return dx * dx + dy * dy <= (NetworkCullRadius + padded) * (NetworkCullRadius + padded);
    };

    snapshot.enemies.reserve(m_renderSnapshot.enemies.size());
    for (const RenderEnemy &enemy : m_renderSnapshot.enemies) {
        if (insideRange(enemy.x, enemy.y, enemy.radius))
            snapshot.enemies.append(enemy);
    }

    snapshot.orbitals.reserve(m_renderSnapshot.orbitals.size());
    for (const RenderOrbital &orbital : m_renderSnapshot.orbitals) {
        if (insideRange(orbital.x, orbital.y, orbital.radius))
            snapshot.orbitals.append(orbital);
    }

    snapshot.projectiles.reserve(m_renderSnapshot.projectiles.size());
    for (const RenderProjectile &projectile : m_renderSnapshot.projectiles) {
        if (insideRange(projectile.x, projectile.y, projectile.radius))
            snapshot.projectiles.append(projectile);
    }

    snapshot.pickups.reserve(m_renderSnapshot.pickups.size());
    for (const RenderPickup &pickup : m_renderSnapshot.pickups) {
        if (insideRange(pickup.x, pickup.y, pickup.radius))
            snapshot.pickups.append(pickup);
    }

    snapshot.zones.reserve(m_renderSnapshot.zones.size());
    for (const RenderZone &zone : m_renderSnapshot.zones) {
        if (insideRange(zone.x, zone.y, zone.radius))
            snapshot.zones.append(zone);
    }

    return snapshot;
}

QByteArray SurvivorController::buildFastNetworkPacket(int playerId) const
{
    if (!m_networkSession || !m_networkAuthoritative)
        return {};

    LanBoard::Survivor::NetCodec::FastNetworkState packet;
    packet.running = m_matchState.running;
    packet.gameOver = m_matchState.gameOver;
    packet.survivalTimeMs = m_matchState.survivalTimeMs;
    packet.killCount = m_matchState.killCount;
    packet.interactionPlayerId = m_matchState.pendingInteractionPlayerId;
    packet.waveLabel = m_matchState.waveLabel;
    packet.seq = m_networkStateSequence;
    packet.origin = cameraAnchorForPlayer(playerId);
    packet.players = m_matchState.players;
    packet.snapshot = buildNetworkRenderSnapshot(playerId);
    if (const PlayerState *player = playerStateById(playerId)) {
        packet.hasLocalPlayer = true;
        packet.localPlayer = *player;
        packet.auraRadius = player->garlicLevel > 0
            ? m_matchState.worldRuntime.garlicRadius * currentAreaMultiplier(*player)
            : 0.0;
    }
    return LanBoard::Survivor::NetCodec::encodeFastNetworkState(packet);
}

QByteArray SurvivorController::buildHudNetworkPacket(int playerId) const
{
    if (!m_networkSession || !m_networkAuthoritative)
        return {};

    LanBoard::Survivor::NetCodec::HudNetworkState packet;
    packet.interactionPlayerId = m_matchState.pendingInteractionPlayerId;
    packet.chestTitle = m_matchState.chestTitle;

    if (const PlayerState *player = playerStateById(playerId);
        player && player->playerId == m_matchState.pendingInteractionPlayerId) {
        packet.levelUpChoices = player->levelUpChoices;
        packet.chestRewards = player->chestRewardEntries;
    }

    return LanBoard::Survivor::NetCodec::encodeHudNetworkState(packet);
}

void SurvivorController::syncPlayerMaxHp()
{
    LanBoard::Survivor::Runtime::syncPlayerMaxHp(m_matchState);
    if (const PlayerState *player = hudPlayerState()) {
        if (!m_networkSession || m_networkAuthoritative)
            m_networkAuraRadius = player->garlicLevel > 0
                ? m_matchState.worldRuntime.garlicRadius * currentAreaMultiplier(*player)
                : 0.0;
    } else {
        m_networkAuraRadius = 0.0f;
    }
}

void SurvivorController::syncInteractionState()
{
    const PlayerState *interactionPlayer = interactionPlayerState();
    m_levelUpChoices = interactionPlayer ? interactionPlayer->levelUpChoices : QList<UpgradeChoice>();
    m_chestRewardEntries = interactionPlayer ? interactionPlayer->chestRewardEntries : QList<ChestReward>();
    m_matchState.chestTitle = interactionPlayer ? interactionPlayer->chestTitle : QString();
}

void SurvivorController::syncHudState()
{
    syncInteractionState();

    const PlayerState *player = hudPlayerState();
    if (!player) {
        m_matchState.level = 1;
        m_matchState.exp = 0;
        m_matchState.expToNext = 5;
        m_matchState.levelUpPending = false;
        m_matchState.chestPending = false;
        refreshLevelUpChoiceCache();
        refreshChestRewardCache();
        refreshUpgradeSummary();
        refreshHudSlotCaches();
        updateStatusText();
        return;
    }

    m_matchState.level = player->level;
    m_matchState.exp = player->exp;
    m_matchState.expToNext = player->expToNext;
    const bool localOwnsInteraction = player->playerId == m_matchState.pendingInteractionPlayerId;
    m_matchState.levelUpPending = localOwnsInteraction && !player->levelUpChoices.isEmpty();
    m_matchState.chestPending = localOwnsInteraction && !player->chestRewardEntries.isEmpty();
    if (!m_matchState.chestPending && !localOwnsInteraction)
        m_matchState.chestTitle.clear();
    refreshLevelUpChoiceCache();
    refreshChestRewardCache();
    refreshUpgradeSummary();
    refreshHudSlotCaches();
    updateStatusText();
}

QVariantList SurvivorController::exportDamageNumberVariantList() const
{
    QVariantList numbers;
    numbers.reserve(m_matchState.damageNumbers.size());
    for (const DamageNumber &number : m_matchState.damageNumbers) {
        QVariantMap map;
        map[QStringLiteral("x")] = number.position.x();
        map[QStringLiteral("y")] = number.position.y();
        map[QStringLiteral("amount")] = number.amount;
        map[QStringLiteral("lifeMs")] = number.lifeMs;
        map[QStringLiteral("totalLifeMs")] = number.totalLifeMs;
        map[QStringLiteral("elite")] = number.elite;
        numbers.append(map);
    }
    return numbers;
}

void SurvivorController::emitNetworkSyncIfNeeded(bool force)
{
    if (!m_networkSession || !m_networkAuthoritative)
        return;

    bool includeHudDetails = force || m_matchState.pendingInteractionPlayerId >= 0 || m_matchState.gameOver;
    if (!force) {
        m_networkBroadcastAccumulatorMs += TickIntervalMs;
        if (m_networkBroadcastAccumulatorMs < NetworkSnapshotIntervalMs)
            return;
        m_networkHudBroadcastAccumulatorMs += TickIntervalMs;
        if (m_networkHudBroadcastAccumulatorMs >= NetworkHudSnapshotIntervalMs)
            includeHudDetails = true;
    }

    m_networkBroadcastAccumulatorMs = 0;
    if (includeHudDetails)
        m_networkHudBroadcastAccumulatorMs = 0;
    ++m_networkStateSequence;
    emit networkSyncRequested(includeHudDetails);
}
void SurvivorController::spawnEnemy(bool elite, int forcedKind, bool forceChestCarrier)
{
    Enemy enemy;
    enemy.id = m_matchState.nextEnemyId++;
    const QVector2D anchor = playerAnchor();
    const qreal spawnAngle = QRandomGenerator::global()->generateDouble() * 360.0;
    const qreal spawnDistance = SpawnDistanceMin
        + QRandomGenerator::global()->generateDouble() * (SpawnDistanceMax - SpawnDistanceMin);
    enemy.position = anchor + rotatedVector(QVector2D(spawnDistance, 0.0f), spawnAngle);
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
    m_matchState.enemies.append(enemy);
}

void SurvivorController::tick()
{
    if (!m_matchState.running)
        return;

    if (!m_frameTimer.isValid())
        m_frameTimer.start();

    static constexpr int MaxFrameElapsedMs = 250;
    const int realElapsedMs = qBound(1, static_cast<int>(m_frameTimer.restart()), MaxFrameElapsedMs);

    if (m_networkSession && !m_networkAuthoritative) {
        const int previousHp = hp();
        const int previousMaxHp = maxHp();
        const int previousLevel = m_matchState.level;
        const int previousExp = m_matchState.exp;
        const int previousExpToNext = m_matchState.expToNext;
        const bool previousLevelUpPending = m_matchState.levelUpPending;
        const bool previousChestPending = m_matchState.chestPending;
        const QString previousChestTitle = m_matchState.chestTitle;
        const QString previousWaveLabel = m_matchState.waveLabel;
        const QString previousStatusText = m_statusText;
        const QString previousUpgradeSummary = m_upgradeSummary;
        stepRemoteInterpolation(realElapsedMs);
        syncHudState();
        const bool shouldEmitStateChanged = previousHp != hp()
            || previousMaxHp != maxHp()
            || previousLevel != m_matchState.level
            || previousExp != m_matchState.exp
            || previousExpToNext != m_matchState.expToNext
            || previousLevelUpPending != m_matchState.levelUpPending
            || previousChestPending != m_matchState.chestPending
            || previousChestTitle != m_matchState.chestTitle
            || previousWaveLabel != m_matchState.waveLabel
            || previousStatusText != m_statusText
            || previousUpgradeSummary != m_upgradeSummary;
        if (shouldEmitStateChanged)
            emit stateChanged();
        emit frameChanged();
        return;
    }

    if (m_matchState.pendingInteractionPlayerId >= 0 && m_networkAuthoritative) {
        m_matchState.pendingInteractionElapsedMs += realElapsedMs;
        if (m_matchState.pendingInteractionElapsedMs >= 8000) {
            if (PlayerState *player = interactionPlayerState()) {
                if (!player->levelUpChoices.isEmpty()) {
                    chooseLevelUp(player->levelUpChoices.first().id);
                } else if (!player->chestRewardEntries.isEmpty()) {
                    closeChestRewards();
                }
            }
        }
    }

    if (m_matchState.gameOver)
        return;
    if (m_matchState.pendingInteractionPlayerId >= 0)
        return;

    const int previousHp = hp();
    const int previousMaxHp = maxHp();
    const int previousLevel = m_matchState.level;
    const int previousExp = m_matchState.exp;
    const int previousExpToNext = m_matchState.expToNext;
    const int previousKillCount = m_matchState.killCount;
    const int previousSurvivalSec = survivalTimeSec();
    const bool previousLevelUpPending = m_matchState.levelUpPending;
    const bool previousChestPending = m_matchState.chestPending;
    const QString previousChestTitle = m_matchState.chestTitle;
    const QString previousWaveLabel = m_matchState.waveLabel;
    const QString previousStatusText = m_statusText;
    const QString previousUpgradeSummary = m_upgradeSummary;

    static constexpr int MaxCatchUpSteps = 6;
    m_matchState.tickAccumulatorMs += realElapsedMs;

    int simulatedSteps = 0;
    while (m_matchState.tickAccumulatorMs >= TickIntervalMs
           && simulatedSteps < MaxCatchUpSteps
           && !m_matchState.gameOver) {
        simulateStep(TickIntervalMs);
        m_matchState.tickAccumulatorMs -= TickIntervalMs;
        ++simulatedSteps;
    }

    if (m_matchState.tickAccumulatorMs > TickIntervalMs * 2)
        m_matchState.tickAccumulatorMs = TickIntervalMs * 2;

    if (simulatedSteps <= 0)
        return;

    refreshFrameCache();
    emitNetworkSyncIfNeeded();
    if (m_matchState.gameOver) {
        emit runningChanged();
        emit gameOverChanged();
    }
    const bool shouldEmitStateChanged =
        previousHp != hp()
        || previousMaxHp != maxHp()
        || previousLevel != m_matchState.level
        || previousExp != m_matchState.exp
        || previousExpToNext != m_matchState.expToNext
        || previousKillCount != m_matchState.killCount
        || previousSurvivalSec != survivalTimeSec()
        || previousLevelUpPending != m_matchState.levelUpPending
        || previousChestPending != m_matchState.chestPending
        || previousChestTitle != m_matchState.chestTitle
        || previousWaveLabel != m_matchState.waveLabel
        || previousStatusText != m_statusText
        || previousUpgradeSummary != m_upgradeSummary;
    if (shouldEmitStateChanged)
        emit stateChanged();
    emit frameChanged();
}

void SurvivorController::simulateStep(int elapsedMs)
{
    const qreal deltaSec = elapsedMs / 1000.0;
    m_matchState.survivalTimeMs += elapsedMs;
    m_matchState.spawnAccumulatorMs += elapsedMs;
    m_matchState.eliteSpawnAccumulatorMs += elapsedMs;
    for (PlayerState &player : m_matchState.players) {
        player.attackCooldownMs = qMax(0, player.attackCooldownMs - elapsedMs);
        player.orbitBladeCooldownMs = qMax(0, player.orbitBladeCooldownMs - elapsedMs);
        player.orbitBladeActiveMs = qMax(0, player.orbitBladeActiveMs - elapsedMs);
        player.fireWandCooldownMs = qMax(0, player.fireWandCooldownMs - elapsedMs);
        player.crossCooldownMs = qMax(0, player.crossCooldownMs - elapsedMs);
        player.santaWaterCooldownMs = qMax(0, player.santaWaterCooldownMs - elapsedMs);
    }
    refreshWaveLabel();
    triggerWaveEvents();

    for (int i = m_matchState.damageNumbers.size() - 1; i >= 0; --i) {
        DamageNumber &number = m_matchState.damageNumbers[i];
        number.position += number.velocity * static_cast<float>(deltaSec);
        number.lifeMs -= elapsedMs;
        if (number.lifeMs <= 0)
            m_matchState.damageNumbers.removeAt(i);
    }

    for (PlayerState &player : m_matchState.players) {
        if (!player.alive)
            continue;
        const QVector2D direction = normalizedInput(player.moveInput);
        if (!direction.isNull()) {
            player.facingDirection = direction;
            player.position += direction * static_cast<float>(currentMoveSpeed(player) * deltaSec);
        }
    }

    for (Enemy &enemy : m_matchState.enemies) {
        decayEnemyHitCooldowns(enemy, elapsedMs);
        enemy.hitFlashMs = qMax(0, enemy.hitFlashMs - elapsedMs);
        enemy.speedScale += (1.0f - enemy.speedScale) * qMin<qreal>(1.0, deltaSec * 7.5);
        if (enemy.forcedMovementMs > 0) {
            enemy.forcedMovementMs = qMax(0, enemy.forcedMovementMs - elapsedMs);
            enemy.position += enemy.forcedVelocity * static_cast<float>(deltaSec);
        } else {
            const int targetIndex = nearestLivingPlayerIndex(enemy.position);
            if (targetIndex >= 0) {
                const QVector2D toPlayer = m_matchState.players.at(targetIndex).position - enemy.position;
                if (toPlayer.lengthSquared() > 0.0001f) {
                    enemy.position += toPlayer.normalized()
                        * static_cast<float>(enemy.speed * enemy.speedScale * deltaSec);
                }
            }
        }

        const qreal collisionRadius = enemy.radius + 0.022;
        for (PlayerState &player : m_matchState.players) {
            if (!player.alive)
                continue;
            if (circlesOverlap(enemy.position, player.position, collisionRadius))
                player.contactDamageCarry += enemy.touchDamage * deltaSec;
        }
    }

    bool anyAlive = false;
    for (PlayerState &player : m_matchState.players) {
        const int appliedContactDamage = static_cast<int>(player.contactDamageCarry);
        if (appliedContactDamage > 0) {
            player.contactDamageCarry -= appliedContactDamage;
            player.hp = qMax(0, player.hp - appliedContactDamage);
        }
        player.alive = player.hp > 0;
        anyAlive = anyAlive || player.alive;
    }

    if (!anyAlive) {
        m_matchState.gameOver = true;
        m_matchState.running = false;
        m_tickTimer.stop();
        updateStatusText();
        return;
    }

    syncPlayerMaxHp();

    if (PlayerState *player = localPlayerState()) {
        if (!player->alive && m_networkSession && !m_networkAuthoritative)
            player->moveInput = QVector2D();
    }

    resolveEnemySeparation();

    const int spawnIntervalMs = currentSpawnIntervalMs();
    while (m_matchState.spawnAccumulatorMs >= spawnIntervalMs) {
        m_matchState.spawnAccumulatorMs -= spawnIntervalMs;
        int spawnBurst = currentSpawnBurstCount();
        if (m_matchState.enemies.size() > currentEnemyCap())
            spawnBurst = qMax(1, spawnBurst - 1);
        if (m_matchState.enemies.size() > currentEnemyCap() * 3 / 2
            && QRandomGenerator::global()->bounded(100) < 70) {
            spawnBurst = 0;
        }
        for (int spawnIndex = 0; spawnIndex < spawnBurst; ++spawnIndex)
            spawnEnemy(false);
    }

    const int eliteSpawnIntervalMs = currentEliteSpawnIntervalMs();
    if (eliteSpawnIntervalMs > 0 && m_matchState.eliteSpawnAccumulatorMs >= eliteSpawnIntervalMs) {
        m_matchState.eliteSpawnAccumulatorMs = 0;
        const int eliteCount = m_matchState.enemies.size() > currentEnemyCap() * 4 / 3 ? 1 : currentSpawnBurstCount();
        for (int i = 0; i < eliteCount; ++i)
            spawnEnemy(true, currentEliteKind(), false);
    }

    if (m_matchState.spawnedBossCount < kBossSpawnScheduleCount
        && survivalTimeSec() >= kBossSpawnSchedule[m_matchState.spawnedBossCount].second) {
        spawnEnemy(true, kBossSpawnSchedule[m_matchState.spawnedBossCount].kind, true);
        ++m_matchState.spawnedBossCount;
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
    if (m_matchState.enemies.isEmpty())
        return;

    const QList<int> activePlayers = livingPlayerIndices();
    if (activePlayers.isEmpty())
        return;

    for (int playerIndex : activePlayers) {
        PlayerState &player = m_matchState.players[playerIndex];
        const qreal damageMultiplier = currentDamageMultiplier(player);
        const qreal cooldownMultiplier = currentCooldownMultiplier(player);
        const qreal projectileSpeedMultiplier = currentProjectileSpeedMultiplier();

        if (player.bladeWeaponLevel > 0 && player.attackCooldownMs <= 0) {
            player.attackCooldownMs = qMax(120, qRound(player.attackCooldownBaseMs * cooldownMultiplier));
            const int knifeDamage = qMax(1, qRound(player.attackDamage * damageMultiplier));
            const qreal centerOffset = (player.projectileCount - 1) / 2.0;
            QVector2D fireDirection = player.facingDirection.lengthSquared() > 0.0001f
                ? player.facingDirection.normalized()
                : QVector2D(1.0f, 0.0f);
            for (int i = 0; i < player.projectileCount; ++i) {
                const qreal spreadDegrees = (static_cast<qreal>(i) - centerOffset) * 8.0;
                Projectile projectile;
                projectile.kind = 0;
                projectile.sourceId = m_matchState.nextSourceId++;
                projectile.position = player.position + fireDirection * 0.02f;
                projectile.velocity = rotatedVector(fireDirection, spreadDegrees)
                    * static_cast<float>(m_matchState.worldRuntime.projectileSpeed * projectileSpeedMultiplier);
                projectile.damage = knifeDamage;
                projectile.hitIntervalMs = 1000000;
                projectile.remainingHits = player.projectilePierce;
                projectile.lifeMs = 1680 + (player.projectilePierce - 1) * 180;
                projectile.radius = 0.012f + qMin<qreal>(0.006f, 0.0015f * player.projectilePierce);
                projectile.knockback = 0.040f;
                projectile.damageVariance = 0.18f;
                m_matchState.projectiles.append(projectile);
            }
        }

        if (player.fireWandLevel > 0 && player.fireWandCooldownMs <= 0) {
            player.fireWandCooldownMs = qMax(420, qRound(player.fireWandCooldownBaseMs * cooldownMultiplier));
            int bestIndex = -1;
            qreal bestDistance = std::numeric_limits<qreal>::max();
            for (int enemyIndex = 0; enemyIndex < m_matchState.enemies.size(); ++enemyIndex) {
                const qreal distanceSquared = (m_matchState.enemies.at(enemyIndex).position - player.position).lengthSquared();
                if (distanceSquared >= bestDistance)
                    continue;
                bestDistance = distanceSquared;
                bestIndex = enemyIndex;
            }
            if (bestIndex < 0)
                continue;

            QVector2D fireDirection = m_matchState.enemies.at(bestIndex).position - player.position;
            if (fireDirection.lengthSquared() <= 0.0001f)
                fireDirection = QVector2D(1.0f, 0.0f);
            fireDirection.normalize();

            Projectile projectile;
            projectile.kind = player.fireWandEvolved ? 3 : 1;
            projectile.sourceId = m_matchState.nextSourceId++;
            projectile.position = player.position + fireDirection * 0.024f;
            projectile.velocity = fireDirection * static_cast<float>(m_matchState.worldRuntime.projectileSpeed
                                                                      * (player.fireWandEvolved ? 1.12f : 0.92f)
                                                                      * projectileSpeedMultiplier
                                                                      * player.fireWandProjectileSpeedMultiplier);
            projectile.damage = qMax(1, qRound(player.fireWandDamage * damageMultiplier * (player.fireWandEvolved ? 1.30f : 1.0f)));
            projectile.hitIntervalMs = 1000000;
            projectile.remainingHits = player.fireWandEvolved ? 999 : 1;
            projectile.lifeMs = player.fireWandEvolved ? 3400 : 2400;
            projectile.radius = player.fireWandEvolved ? 0.021f : 0.017f;
            projectile.knockback = player.fireWandEvolved ? 0.072f : 0.048f;
            projectile.damageVariance = player.fireWandEvolved ? 0.10f : 0.16f;
            m_matchState.projectiles.append(projectile);
        }

        if (player.crossLevel > 0 && player.crossCooldownMs <= 0) {
            player.crossCooldownMs = qMax(260, qRound(player.crossCooldownBaseMs * cooldownMultiplier));
            const int crossDamage = qMax(1, qRound(player.crossDamage * damageMultiplier));
            QVector<int> targetIndices;
            QVector<qreal> targetDistances;
            targetIndices.reserve(player.crossAmount);
            targetDistances.reserve(player.crossAmount);
            for (int i = 0; i < m_matchState.enemies.size(); ++i) {
                const qreal distanceSquared = (m_matchState.enemies.at(i).position - player.position).lengthSquared();
                int insertIndex = 0;
                while (insertIndex < targetDistances.size() && targetDistances.at(insertIndex) <= distanceSquared)
                    ++insertIndex;
                if (insertIndex >= player.crossAmount)
                    continue;
                targetDistances.insert(insertIndex, distanceSquared);
                targetIndices.insert(insertIndex, i);
                if (targetIndices.size() > player.crossAmount) {
                    targetIndices.removeLast();
                    targetDistances.removeLast();
                }
            }

            for (int i = 0; i < targetIndices.size(); ++i) {
                QVector2D crossDirection = m_matchState.enemies.at(targetIndices.at(i)).position - player.position;
                if (crossDirection.lengthSquared() <= 0.0001f)
                    crossDirection = rotatedVector(QVector2D(1.0f, 0.0f), i * 18.0);
                crossDirection.normalize();

                Projectile crossProjectile;
                crossProjectile.kind = 2;
                crossProjectile.sourceId = m_matchState.nextSourceId++;
                crossProjectile.position = player.position + crossDirection * 0.024f;
                crossProjectile.velocity = crossDirection * static_cast<float>(m_matchState.worldRuntime.crossSpeed);
                crossProjectile.damage = crossDamage;
                crossProjectile.hitIntervalMs = 1000000;
                crossProjectile.remainingHits = player.crossPierce;
                crossProjectile.lifeMs = 1620;
                crossProjectile.radius = m_matchState.worldRuntime.crossRadius * currentAreaMultiplier(player);
                crossProjectile.returning = false;
                crossProjectile.knockback = 0.060f;
                crossProjectile.damageVariance = 0.12f;
                m_matchState.projectiles.append(crossProjectile);
            }
        }

        if (player.santaWaterLevel > 0 && player.santaWaterCooldownMs <= 0) {
            player.santaWaterCooldownMs = qMax(620, qRound(player.santaWaterCooldownBaseMs * cooldownMultiplier));
            const int zoneDamage = qMax(1, qRound(player.santaWaterDamage * damageMultiplier * (player.santaWaterEvolved ? 1.18f : 1.0f)));
            const qreal zoneRadius = m_matchState.worldRuntime.santaWaterRadius
                * currentAreaMultiplier(player)
                * (player.santaWaterEvolved ? 1.06f : 1.0f);
            for (int i = 0; i < player.santaWaterAmount; ++i) {
                const qreal angle = QRandomGenerator::global()->generateDouble() * 360.0;
                const qreal distance = player.santaWaterEvolved
                    ? 0.32 + QRandomGenerator::global()->generateDouble() * 0.42
                    : 0.16 + QRandomGenerator::global()->generateDouble() * 0.30;
                Zone zone;
                zone.kind = player.santaWaterEvolved ? 1 : 0;
                zone.sourceId = m_matchState.nextSourceId++;
                zone.position = player.position + rotatedVector(QVector2D(distance, 0.0f), angle);
                zone.radius = zoneRadius;
                zone.damage = zoneDamage;
                zone.totalLifeMs = qRound(player.santaWaterDurationMs * currentDurationMultiplier() * (player.santaWaterEvolved ? 1.08f : 1.0f));
                zone.lifeMs = zone.totalLifeMs;
                zone.tickIntervalMs = qMax(170, qRound((player.santaWaterEvolved ? 280 : 320) * cooldownMultiplier));
                zone.tickCooldownMs = 0;
                zone.knockback = player.santaWaterEvolved ? 0.026f : 0.018f;
                zone.damageVariance = player.santaWaterEvolved ? 0.06f : 0.10f;
                m_matchState.zones.append(zone);
            }
        }

        if (player.orbitBladeLevel > 0
            && player.orbitBladeCount > 0
            && player.orbitBladeActiveMs <= 0
            && player.orbitBladeCooldownMs <= 0) {
            player.orbitBladeActiveMs = qRound(player.orbitBladeDurationMs * currentDurationMultiplier());
            player.orbitBladeCooldownMs = qMax(450, qRound(player.orbitBladeCooldownBaseMs * cooldownMultiplier));
        }
    }
}
void SurvivorController::updateGarlicAura()
{
    if (m_matchState.enemies.isEmpty())
        return;

    for (const PlayerState &player : std::as_const(m_matchState.players)) {
        if (!player.alive || player.garlicLevel <= 0)
            continue;
        const qreal auraRadius = m_matchState.worldRuntime.garlicRadius * currentAreaMultiplier(player);
        const int auraDamage = qMax(1, qRound(player.garlicDamage * currentDamageMultiplier(player)));
        const int auraCooldownMs = qMax(650, qRound(player.garlicCooldownBaseMs * currentCooldownMultiplier(player)));
        for (int enemyIndex = m_matchState.enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
            Enemy &enemy = m_matchState.enemies[enemyIndex];
            if (!circlesOverlap(enemy.position, player.position, enemy.radius + auraRadius))
                continue;
            tryApplyHit(enemyIndex,
                        player.position,
                        1000 + player.playerId,
                        auraCooldownMs,
                        auraDamage,
                        0.10f,
                        enemy.elite ? 0.022f : 0.040f,
                        true);
        }
    }
}

void SurvivorController::resolveEnemySeparation()
{
    if (m_matchState.enemies.size() < 2)
        return;

    const int iterationCount = m_matchState.enemies.size() > 180 ? 1 : 2;
    for (int iteration = 0; iteration < iterationCount; ++iteration) {
        QHash<quint64, QVector<int>> buckets;
        QVector<int> cellXs;
        QVector<int> cellYs;
        buckets.reserve(m_matchState.enemies.size() * 2);
        cellXs.resize(m_matchState.enemies.size());
        cellYs.resize(m_matchState.enemies.size());

        for (int i = 0; i < m_matchState.enemies.size(); ++i) {
            const Enemy &enemy = m_matchState.enemies.at(i);
            const int cellX = qFloor(enemy.position.x() / EnemySeparationCellSize);
            const int cellY = qFloor(enemy.position.y() / EnemySeparationCellSize);
            cellXs[i] = cellX;
            cellYs[i] = cellY;
            buckets[spatialCellKey(cellX, cellY)].append(i);
        }

        for (auto bucketIt = buckets.cbegin(); bucketIt != buckets.cend(); ++bucketIt) {
            const QVector<int> &indices = bucketIt.value();
            for (int ii = 0; ii < indices.size(); ++ii) {
                const int i = indices.at(ii);
                Enemy &first = m_matchState.enemies[i];
                const int cellX = cellXs.at(i);
                const int cellY = cellYs.at(i);

                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                        const auto neighborIt = buckets.constFind(spatialCellKey(cellX + offsetX, cellY + offsetY));
                        if (neighborIt == buckets.cend())
                            continue;

                        const QVector<int> &neighborIndices = neighborIt.value();
                        for (int neighborIndex : neighborIndices) {
                            if (neighborIndex <= i)
                                continue;

                            Enemy &second = m_matchState.enemies[neighborIndex];
                            QVector2D delta = second.position - first.position;
                            const qreal minDistance = first.radius + second.radius + 0.004f;
                            const qreal minDistanceSquared = minDistance * minDistance;
                            qreal distanceSquared = delta.lengthSquared();
                            if (distanceSquared >= minDistanceSquared)
                                continue;

                            qreal distance = 0.0;
                            if (distanceSquared <= 0.0001f) {
                                const qreal nudgeAngle = (i * 37 + neighborIndex * 53) % 360;
                                delta = rotatedVector(QVector2D(1.0f, 0.0f), nudgeAngle);
                                distance = 1.0f;
                            } else {
                                distance = qSqrt(distanceSquared);
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
    m_matchState.worldRuntime.orbitAngleDeg += m_matchState.worldRuntime.orbitBladeAngularSpeedDeg * deltaSec;
    while (m_matchState.worldRuntime.orbitAngleDeg >= 360.0f)
        m_matchState.worldRuntime.orbitAngleDeg -= 360.0f;

    Q_UNUSED(elapsedMs)
    for (const PlayerState &player : std::as_const(m_matchState.players)) {
        if (!player.alive
            || player.orbitBladeCount <= 0
            || player.orbitBladeDamage <= 0
            || player.orbitBladeActiveMs <= 0) {
            continue;
        }
        const qreal orbitalRadius = (0.014f + 0.002f * qMin(3, player.orbitBladeLevel))
            * currentAreaMultiplier(player);
        const qreal orbitRadius = m_matchState.worldRuntime.orbitBladeRadius * currentAreaMultiplier(player);
        const qreal angleStep = 360.0 / player.orbitBladeCount;
        const int orbitalDamage = qMax(1, qRound(player.orbitBladeDamage * currentDamageMultiplier(player)));
        for (int orbitalIndex = 0; orbitalIndex < player.orbitBladeCount; ++orbitalIndex) {
            const QVector2D offset = rotatedVector(QVector2D(orbitRadius, 0.0f),
                                                   m_matchState.worldRuntime.orbitAngleDeg + angleStep * orbitalIndex);
            const QVector2D orbitalPos = player.position + offset;
            for (int enemyIndex = m_matchState.enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
                if (!circlesOverlap(m_matchState.enemies.at(enemyIndex).position, orbitalPos,
                                    m_matchState.enemies.at(enemyIndex).radius + orbitalRadius)) {
                    continue;
                }
                tryApplyHit(enemyIndex,
                            orbitalPos,
                            2000 + player.playerId * 100 + orbitalIndex,
                            1700,
                            orbitalDamage,
                            0.08f,
                            0.028f);
            }
        }
    }
}

void SurvivorController::updateProjectiles(qreal deltaSec, int elapsedMs)
{
    for (int i = m_matchState.projectiles.size() - 1; i >= 0; --i) {
        Projectile &projectile = m_matchState.projectiles[i];
        if (projectile.kind == 2) {
            if (!projectile.returning && projectile.lifeMs <= 620) {
                projectile.returning = true;
                projectile.hitEnemyIds.clear();
            }
            if (projectile.returning) {
                const int targetIndex = nearestLivingPlayerIndex(projectile.position);
                if (targetIndex >= 0) {
                    QVector2D toPlayer = m_matchState.players.at(targetIndex).position - projectile.position;
                    if (toPlayer.lengthSquared() > 0.0001f)
                        projectile.velocity = toPlayer.normalized()
                            * static_cast<float>(m_matchState.worldRuntime.crossSpeed * 1.05f);
                }
            }
        }
        projectile.position += projectile.velocity * static_cast<float>(deltaSec);
        projectile.lifeMs -= elapsedMs;

        bool projectileConsumed = false;
        for (int enemyIndex = m_matchState.enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
            Enemy &enemy = m_matchState.enemies[enemyIndex];
            const int enemyId = enemy.id;
            if (!circlesOverlap(enemy.position, projectile.position, enemy.radius + projectile.radius))
                continue;
            if (projectile.hitEnemyIds.contains(enemyId))
                continue;

            if (!tryApplyHit(enemyIndex,
                             projectile.position,
                             projectile.sourceId,
                             projectile.hitIntervalMs,
                             projectile.damage,
                             projectile.damageVariance,
                             projectile.knockback)) {
                continue;
            }

            if (projectile.kind == 2 || projectile.hitIntervalMs >= 1000000)
                projectile.hitEnemyIds.insert(enemyId);
            --projectile.remainingHits;

            if (projectile.remainingHits <= 0) {
                projectileConsumed = true;
                break;
            }
        }

        bool returnedToPlayer = false;
        if (projectile.kind == 2 && projectile.returning) {
            for (const PlayerState &player : std::as_const(m_matchState.players)) {
                if (!player.alive)
                    continue;
                if (circlesOverlap(projectile.position, player.position, 0.04f)) {
                    returnedToPlayer = true;
                    break;
                }
            }
        }

        if (projectileConsumed
            || projectile.lifeMs <= 0
            || returnedToPlayer
            || (projectile.position - playerAnchor()).lengthSquared() > ProjectileCleanupDistanceSquared) {
            m_matchState.projectiles.removeAt(i);
        }
    }
}

void SurvivorController::updateZones(qreal deltaSec, int elapsedMs)
{
    for (int zoneIndex = m_matchState.zones.size() - 1; zoneIndex >= 0; --zoneIndex) {
        Zone &zone = m_matchState.zones[zoneIndex];
        if (zone.kind == 1) {
            QVector2D toPlayer = playerAnchor() - zone.position;
            if (toPlayer.lengthSquared() > 0.0001f)
                zone.position += toPlayer.normalized() * static_cast<float>(0.16f * deltaSec);
        }
        zone.lifeMs -= elapsedMs;
        zone.tickCooldownMs = qMax(0, zone.tickCooldownMs - elapsedMs);

        if (zone.tickCooldownMs == 0) {
            zone.tickCooldownMs = zone.tickIntervalMs;
            for (int enemyIndex = m_matchState.enemies.size() - 1; enemyIndex >= 0; --enemyIndex) {
                if (!circlesOverlap(m_matchState.enemies.at(enemyIndex).position, zone.position,
                                    m_matchState.enemies.at(enemyIndex).radius + zone.radius)) {
                    continue;
                }
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
            m_matchState.zones.removeAt(zoneIndex);
    }
}

void SurvivorController::collectPickups(qreal deltaSec)
{
    const QList<int> activePlayers = livingPlayerIndices();
    if (activePlayers.isEmpty())
        return;

    for (int i = m_matchState.pickups.size() - 1; i >= 0; --i) {
        Pickup &pickup = m_matchState.pickups[i];
        int bestPlayerIndex = -1;
        qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
        for (int playerIndex : activePlayers) {
            const qreal distanceSquared = (m_matchState.players.at(playerIndex).position - pickup.position).lengthSquared();
            if (distanceSquared >= bestDistanceSquared)
                continue;
            bestDistanceSquared = distanceSquared;
            bestPlayerIndex = playerIndex;
        }
        if (bestPlayerIndex < 0)
            continue;

        PlayerState &bestPlayer = m_matchState.players[bestPlayerIndex];
        const qreal magnetRange = currentMagnetRange(bestPlayer);
        const qreal magnetRangeSquared = magnetRange * magnetRange;
        const qreal pullSpeed = 0.40f + magnetRange * 1.8f;
        QVector2D toPlayer = bestPlayer.position - pickup.position;
        const qreal collectRadius = pickup.radius + 0.022f;
        if (bestDistanceSquared <= collectRadius * collectRadius) {
            const Pickup collectedPickup = pickup;
            m_matchState.pickups.removeAt(i);
            if (collectedPickup.kind == ChestPickup) {
                enqueueChest(bestPlayer, collectedPickup);
            } else {
                gainExp(bestPlayer, collectedPickup.exp);
            }
            continue;
        }

        if (bestDistanceSquared > magnetRangeSquared || bestDistanceSquared <= 0.0001f)
            continue;

        pickup.position += toPlayer / qSqrt(bestDistanceSquared) * static_cast<float>(pullSpeed * deltaSec);
    }

    if (m_matchState.pendingInteractionPlayerId < 0)
        tryOpenQueuedChest();
}

void SurvivorController::defeatEnemy(int index)
{
    if (index < 0 || index >= m_matchState.enemies.size())
        return;

    const Enemy enemy = m_matchState.enemies.at(index);
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
        for (int i = m_matchState.pickups.size() - 1; i >= 0; --i) {
            Pickup &existing = m_matchState.pickups[i];
            if (existing.kind == ChestPickup)
                continue;
            if ((existing.position - pickup.position).lengthSquared() > mergeDistance * mergeDistance)
                continue;

            existing.exp += pickup.exp;
            existing.kind = pickupKindForExp(existing.exp);
            existing.radius = pickupRadiusForKind(existing.kind);
            existing.position = (existing.position + pickup.position) * 0.5f;
            m_matchState.enemies.removeAt(index);
            ++m_matchState.killCount;
            return;
        }
    }

    m_matchState.pickups.append(pickup);
    m_matchState.enemies.removeAt(index);
    ++m_matchState.killCount;
}

void SurvivorController::prepareLevelUpChoices(PlayerState &player)
{
    player.levelUpChoices.clear();

    QList<QString> ownedPool;
    QList<QString> newPool;
    auto appendEligible = [this, &player, &ownedPool, &newPool](const UpgradeTemplate *templates, int count) {
        for (int i = 0; i < count; ++i) {
            const QString id = QString::fromLatin1(templates[i].id);
            const int currentLevel = levelForUpgrade(player, id);
            const int maxLevel = maxLevelForUpgrade(id);
            if (currentLevel >= maxLevel)
                continue;
            if (currentLevel > 0)
                ownedPool.append(id);
            else
                newPool.append(id);
        }
    };

    appendEligible(kWeaponUpgradePool, kWeaponUpgradePoolCount);
    appendEligible(kPassiveUpgradePool, kPassiveUpgradePoolCount);

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
        const int currentLevel = levelForUpgrade(player, id);
        player.levelUpChoices.append({
            id,
            titleForUpgrade(id),
            descriptionForUpgrade(id, currentLevel),
            categoryForUpgrade(id),
            currentLevel,
            maxLevelForUpgrade(id)
        });
    }

    if (player.levelUpChoices.isEmpty()) {
        player.pendingLevelUps = 0;
        if (m_matchState.pendingInteractionPlayerId == player.playerId)
            m_matchState.pendingInteractionPlayerId = -1;
        if (m_matchState.pendingInteractionPlayerId < 0)
            m_matchState.pendingInteractionElapsedMs = 0;
        syncHudState();
        emitNetworkSyncIfNeeded(true);
        return;
    }

    m_matchState.pendingInteractionPlayerId = player.playerId;
    m_matchState.pendingInteractionElapsedMs = 0;
    syncHudState();
    emitNetworkSyncIfNeeded(true);
}

void SurvivorController::enqueueChest(PlayerState &player, const Pickup &pickup)
{
    player.queuedChests.append(pickup);
}

void SurvivorController::tryOpenQueuedChest()
{
    if (m_matchState.pendingInteractionPlayerId >= 0)
        return;

    for (PlayerState &player : m_matchState.players) {
        if (player.pendingLevelUps > 0) {
            prepareLevelUpChoices(player);
            return;
        }
    }

    for (PlayerState &player : m_matchState.players) {
        if (player.queuedChests.isEmpty())
            continue;
        openChest(player, player.queuedChests.takeFirst());
        return;
    }
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

QList<QString> SurvivorController::currentChestUpgradeCandidates(const PlayerState &player) const
{
    QList<QString> ids;
    ids.reserve(12);
    auto appendEligible = [this, &player, &ids](const UpgradeTemplate *templates, int count) {
        for (int i = 0; i < count; ++i) {
            const QString id = QString::fromLatin1(templates[i].id);
            const int currentLevel = levelForUpgrade(player, id);
            if (currentLevel <= 0 || currentLevel >= maxLevelForUpgrade(id))
                continue;
            ids.append(id);
        }
    };

    appendEligible(kWeaponUpgradePool, kWeaponUpgradePoolCount);
    appendEligible(kPassiveUpgradePool, kPassiveUpgradePoolCount);
    return ids;
}

bool SurvivorController::canEvolveWeapon(const PlayerState &player, const QString &weaponId) const
{
    if (weaponId == QStringLiteral("firewand_weapon")) {
        return !player.fireWandEvolved
            && player.fireWandLevel >= 8
            && player.spinachPassiveLevel > 0;
    }
    if (weaponId == QStringLiteral("santawater_weapon")) {
        return !player.santaWaterEvolved
            && player.santaWaterLevel >= 8
            && player.attractorbPassiveLevel > 0;
    }
    return false;
}

QList<QString> SurvivorController::currentEvolutionCandidates(const PlayerState &player) const
{
    QList<QString> ids;
    if (canEvolveWeapon(player, QStringLiteral("firewand_weapon")))
        ids.append(QStringLiteral("firewand_weapon"));
    if (canEvolveWeapon(player, QStringLiteral("santawater_weapon")))
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

bool SurvivorController::applyEvolution(PlayerState &player, const QString &weaponId)
{
    if (!canEvolveWeapon(player, weaponId))
        return false;

    if (weaponId == QStringLiteral("firewand_weapon")) {
        player.fireWandEvolved = true;
        player.fireWandDamage = qMax(player.fireWandDamage, 76);
        player.fireWandCooldownBaseMs = qMin(player.fireWandCooldownBaseMs, 1320);
        player.fireWandProjectileSpeedMultiplier = qMax<qreal>(player.fireWandProjectileSpeedMultiplier, 1.85f);
    } else if (weaponId == QStringLiteral("santawater_weapon")) {
        player.santaWaterEvolved = true;
        player.santaWaterDamage = qMax(player.santaWaterDamage, 28);
        player.santaWaterDurationMs = qMax(player.santaWaterDurationMs, 2800);
        m_matchState.worldRuntime.santaWaterRadius = qMax<qreal>(m_matchState.worldRuntime.santaWaterRadius, 0.128f);
        player.santaWaterCooldownBaseMs = qMax(player.santaWaterCooldownBaseMs, 1780);
    } else {
        return false;
    }

    refreshDerivedStats();
    syncHudState();
    return true;
}

void SurvivorController::applyChestReward(PlayerState &player, const QString &upgradeId, bool evolved)
{
    ChestReward reward;
    reward.category = evolved ? QStringLiteral("进化") : QStringLiteral("宝箱");
    reward.evolved = evolved;

    if (evolved) {
        reward.title = evolvedTitleForWeapon(upgradeId);
        reward.description = evolvedDescriptionForWeapon(upgradeId);
        player.chestRewardEntries.append(reward);
        return;
    }

    const int previousLevel = levelForUpgrade(player, upgradeId);
    applyUpgrade(player, upgradeId);
    reward.title = titleForUpgrade(upgradeId);
    reward.description = QStringLiteral("%1 Lv.%2 -> %3")
                             .arg(reward.title)
                             .arg(previousLevel)
                             .arg(previousLevel + 1);
    player.chestRewardEntries.append(reward);
}

void SurvivorController::openChest(PlayerState &player, const Pickup &pickup)
{
    player.chestRewardEntries.clear();

    int rewardCount = qBound(1, pickup.rewardCount, 5);
    if (pickup.canEvolve) {
        const QList<QString> evolutionCandidates = currentEvolutionCandidates(player);
        if (!evolutionCandidates.isEmpty()) {
            const QString evolvedId = evolutionCandidates.at(QRandomGenerator::global()->bounded(evolutionCandidates.size()));
            if (applyEvolution(player, evolvedId)) {
                applyChestReward(player, evolvedId, true);
                --rewardCount;
            }
        }
    }

    for (int i = 0; i < rewardCount; ++i) {
        QList<QString> upgradeCandidates = currentChestUpgradeCandidates(player);
        if (upgradeCandidates.isEmpty()) {
            ChestReward reward;
            reward.category = QStringLiteral("补给");
            reward.title = QStringLiteral("经验结晶");
            reward.description = QStringLiteral("宝箱转化为 25 点经验。");
            player.chestRewardEntries.append(reward);
            gainExp(player, 25);
            continue;
        }

        const QString rewardId = upgradeCandidates.at(QRandomGenerator::global()->bounded(upgradeCandidates.size()));
        applyChestReward(player, rewardId, false);
    }

    player.chestTitle = player.chestRewardEntries.size() >= 5
        ? QStringLiteral("五连宝箱")
        : player.chestRewardEntries.size() >= 3
            ? QStringLiteral("三连宝箱")
            : (player.chestRewardEntries.size() == 1 && player.chestRewardEntries.first().evolved
                   ? QStringLiteral("武器进化")
                   : QStringLiteral("宝箱开启"));
    m_matchState.pendingInteractionPlayerId = player.playerId;
    m_matchState.pendingInteractionElapsedMs = 0;
    syncHudState();
    emitNetworkSyncIfNeeded(true);
}

void SurvivorController::applyUpgrade(PlayerState &player, const QString &upgradeId)
{
    const int currentLevel = levelForUpgrade(player, upgradeId);
    const int maxLevel = maxLevelForUpgrade(upgradeId);
    if (currentLevel >= maxLevel)
        return;

    const int newLevel = currentLevel + 1;
    if (upgradeId == QStringLiteral("knife_weapon")) {
        player.bladeWeaponLevel = newLevel;
        switch (newLevel) {
        case 1:
            player.attackDamage = 7;
            player.projectileCount = 1;
            player.projectilePierce = 1;
            player.attackCooldownBaseMs = 1000;
            m_matchState.worldRuntime.projectileSpeed = 1.00f;
            break;
        case 2:
            player.projectileCount = qMin(6, player.projectileCount + 1);
            break;
        case 3:
            player.attackDamage += 5;
            player.projectileCount = qMin(6, player.projectileCount + 1);
            break;
        case 4:
            player.projectileCount = qMin(6, player.projectileCount + 1);
            player.attackCooldownBaseMs = 930;
            break;
        case 5:
            player.projectilePierce = qMin(3, player.projectilePierce + 1);
            break;
        case 6:
            player.projectileCount = qMin(6, player.projectileCount + 1);
            player.attackCooldownBaseMs = 860;
            break;
        case 7:
            player.attackDamage += 5;
            player.projectileCount = qMin(6, player.projectileCount + 1);
            break;
        case 8:
            player.projectilePierce = qMin(3, player.projectilePierce + 1);
            player.attackCooldownBaseMs = 790;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("orbit_weapon")) {
        player.orbitBladeLevel = newLevel;
        switch (newLevel) {
        case 1:
            player.orbitBladeCount = 1;
            player.orbitBladeDamage = 8;
            m_matchState.worldRuntime.orbitBladeRadius = 0.14f;
            m_matchState.worldRuntime.orbitBladeAngularSpeedDeg = 140.0f;
            player.orbitBladeCooldownBaseMs = 3200;
            player.orbitBladeDurationMs = 3100;
            break;
        case 2:
            player.orbitBladeCount = qMin(4, player.orbitBladeCount + 1);
            break;
        case 3:
            m_matchState.worldRuntime.orbitBladeRadius = qMin<qreal>(0.16f, m_matchState.worldRuntime.orbitBladeRadius + 0.020f);
            m_matchState.worldRuntime.orbitBladeAngularSpeedDeg = qMin<qreal>(180.0f, m_matchState.worldRuntime.orbitBladeAngularSpeedDeg + 20.0f);
            break;
        case 4:
            player.orbitBladeDurationMs += 500;
            break;
        case 5:
            player.orbitBladeDamage += 4;
            player.orbitBladeCount = qMin(4, player.orbitBladeCount + 1);
            break;
        case 6:
            m_matchState.worldRuntime.orbitBladeRadius = qMin<qreal>(0.18f, m_matchState.worldRuntime.orbitBladeRadius + 0.020f);
            m_matchState.worldRuntime.orbitBladeAngularSpeedDeg = qMin<qreal>(210.0f, m_matchState.worldRuntime.orbitBladeAngularSpeedDeg + 24.0f);
            break;
        case 7:
            player.orbitBladeDurationMs += 500;
            break;
        case 8:
            player.orbitBladeDamage += 4;
            player.orbitBladeCount = qMin(4, player.orbitBladeCount + 1);
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("firewand_weapon")) {
        player.fireWandLevel = newLevel;
        switch (newLevel) {
        case 1:
            player.fireWandDamage = 24;
            player.fireWandCooldownBaseMs = 1720;
            player.fireWandProjectileSpeedMultiplier = 1.0f;
            break;
        case 2:
            player.fireWandDamage += 12;
            break;
        case 3:
            player.fireWandProjectileSpeedMultiplier = 1.20f;
            player.fireWandCooldownBaseMs = 1620;
            break;
        case 4:
            player.fireWandDamage += 12;
            break;
        case 5:
            player.fireWandProjectileSpeedMultiplier = 1.40f;
            player.fireWandCooldownBaseMs = 1520;
            break;
        case 6:
            player.fireWandDamage += 12;
            break;
        case 7:
            player.fireWandProjectileSpeedMultiplier = 1.60f;
            player.fireWandCooldownBaseMs = 1420;
            break;
        case 8:
            player.fireWandDamage += 12;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("garlic_weapon")) {
        player.garlicLevel = newLevel;
        switch (newLevel) {
        case 1:
            player.garlicDamage = 4;
            m_matchState.worldRuntime.garlicRadius = 0.10f;
            player.garlicCooldownBaseMs = 1300;
            break;
        case 2:
            m_matchState.worldRuntime.garlicRadius = qMin<qreal>(0.12f, m_matchState.worldRuntime.garlicRadius + 0.020f);
            player.garlicDamage += 1;
            break;
        case 3:
            player.garlicDamage += 1;
            player.garlicCooldownBaseMs = 1200;
            break;
        case 4:
            m_matchState.worldRuntime.garlicRadius = qMin<qreal>(0.14f, m_matchState.worldRuntime.garlicRadius + 0.020f);
            break;
        case 5:
            player.garlicDamage += 2;
            break;
        case 6:
            m_matchState.worldRuntime.garlicRadius = qMin<qreal>(0.16f, m_matchState.worldRuntime.garlicRadius + 0.020f);
            player.garlicCooldownBaseMs = 1080;
            break;
        case 7:
            ++player.garlicDamage;
            break;
        case 8:
            m_matchState.worldRuntime.garlicRadius = qMin<qreal>(0.18f, m_matchState.worldRuntime.garlicRadius + 0.020f);
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("cross_weapon")) {
        player.crossLevel = newLevel;
        switch (newLevel) {
        case 1:
            player.crossDamage = 12;
            player.crossAmount = 1;
            m_matchState.worldRuntime.crossSpeed = 0.82f;
            break;
        case 2:
            player.crossDamage += 8;
            break;
        case 3:
            m_matchState.worldRuntime.crossRadius = qMin<qreal>(0.020f, m_matchState.worldRuntime.crossRadius + 0.002f);
            m_matchState.worldRuntime.crossSpeed = qMin<qreal>(0.92f, m_matchState.worldRuntime.crossSpeed + 0.10f);
            break;
        case 4:
            player.crossAmount = qMin(3, player.crossAmount + 1);
            break;
        case 5:
            player.crossDamage += 8;
            break;
        case 6:
            m_matchState.worldRuntime.crossRadius = qMin<qreal>(0.022f, m_matchState.worldRuntime.crossRadius + 0.002f);
            m_matchState.worldRuntime.crossSpeed = qMin<qreal>(1.04f, m_matchState.worldRuntime.crossSpeed + 0.12f);
            break;
        case 7:
            player.crossAmount = qMin(3, player.crossAmount + 1);
            break;
        case 8:
            player.crossDamage += 8;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("santawater_weapon")) {
        player.santaWaterLevel = newLevel;
        switch (newLevel) {
        case 1:
            player.santaWaterDamage = 10;
            player.santaWaterAmount = 1;
            player.santaWaterDurationMs = 1800;
            player.santaWaterCooldownBaseMs = 1800;
            break;
        case 2:
            player.santaWaterAmount = qMin(3, player.santaWaterAmount + 1);
            m_matchState.worldRuntime.santaWaterRadius = qMin<qreal>(0.090f, m_matchState.worldRuntime.santaWaterRadius + 0.010f);
            break;
        case 3:
            player.santaWaterDurationMs += 350;
            player.santaWaterDamage += 5;
            break;
        case 4:
            player.santaWaterAmount = qMin(4, player.santaWaterAmount + 1);
            m_matchState.worldRuntime.santaWaterRadius = qMin<qreal>(0.100f, m_matchState.worldRuntime.santaWaterRadius + 0.010f);
            break;
        case 5:
            player.santaWaterDurationMs += 250;
            player.santaWaterDamage += 5;
            break;
        case 6:
            player.santaWaterAmount = qMin(4, player.santaWaterAmount + 1);
            m_matchState.worldRuntime.santaWaterRadius = qMin<qreal>(0.112f, m_matchState.worldRuntime.santaWaterRadius + 0.012f);
            break;
        case 7:
            player.santaWaterDurationMs += 250;
            player.santaWaterDamage += 5;
            break;
        case 8:
            m_matchState.worldRuntime.santaWaterRadius = qMin<qreal>(0.122f, m_matchState.worldRuntime.santaWaterRadius + 0.010f);
            player.santaWaterDamage += 5;
            break;
        default:
            break;
        }
    } else if (upgradeId == QStringLiteral("wings_passive")) {
        player.wingsPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("emptytome_passive")) {
        player.emptyTomePassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("candelabrador_passive")) {
        player.candelabradorPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("attractorb_passive")) {
        player.attractorbPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("hollowheart_passive")) {
        player.hollowHeartPassiveLevel = newLevel;
    } else if (upgradeId == QStringLiteral("spinach_passive")) {
        player.spinachPassiveLevel = newLevel;
    }

    refreshDerivedStats();
    syncHudState();
}

void SurvivorController::refreshDerivedStats()
{
    syncPlayerMaxHp();
}

void SurvivorController::refreshFrameCache()
{
    m_renderSnapshot = LanBoard::Survivor::Runtime::buildRenderSnapshot(m_matchState);
    m_cachedDamageNumbers = exportDamageNumberVariantList();
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
    if (enemyIndex < 0 || enemyIndex >= m_matchState.enemies.size())
        return false;

    Enemy &enemy = m_matchState.enemies[enemyIndex];
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
    if (enemy.sourceHitCooldownsMs.isEmpty())
        return;

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

int SurvivorController::levelForUpgrade(const PlayerState &player, const QString &upgradeId) const
{
    if (upgradeId == QStringLiteral("knife_weapon"))
        return player.bladeWeaponLevel;
    if (upgradeId == QStringLiteral("orbit_weapon"))
        return player.orbitBladeLevel;
    if (upgradeId == QStringLiteral("firewand_weapon"))
        return player.fireWandLevel;
    if (upgradeId == QStringLiteral("garlic_weapon"))
        return player.garlicLevel;
    if (upgradeId == QStringLiteral("cross_weapon"))
        return player.crossLevel;
    if (upgradeId == QStringLiteral("santawater_weapon"))
        return player.santaWaterLevel;
    if (upgradeId == QStringLiteral("wings_passive"))
        return player.wingsPassiveLevel;
    if (upgradeId == QStringLiteral("emptytome_passive"))
        return player.emptyTomePassiveLevel;
    if (upgradeId == QStringLiteral("candelabrador_passive"))
        return player.candelabradorPassiveLevel;
    if (upgradeId == QStringLiteral("attractorb_passive"))
        return player.attractorbPassiveLevel;
    if (upgradeId == QStringLiteral("hollowheart_passive"))
        return player.hollowHeartPassiveLevel;
    if (upgradeId == QStringLiteral("spinach_passive"))
        return player.spinachPassiveLevel;
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

qreal SurvivorController::currentDamageMultiplier(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentDamageMultiplier(player);
}

qreal SurvivorController::currentAreaMultiplier(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentAreaMultiplier(player);
}

qreal SurvivorController::currentCooldownMultiplier(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentCooldownMultiplier(player);
}

qreal SurvivorController::currentDurationMultiplier() const
{
    return LanBoard::Survivor::Runtime::currentDurationMultiplier();
}

qreal SurvivorController::currentProjectileSpeedMultiplier() const
{
    return LanBoard::Survivor::Runtime::currentProjectileSpeedMultiplier();
}

qreal SurvivorController::currentMoveSpeed(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentMoveSpeed(player);
}

qreal SurvivorController::currentMagnetRange(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentMagnetRange(player);
}

int SurvivorController::currentMaxHpValue(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentMaxHpValue(player);
}

QString SurvivorController::titleForUpgrade(const QString &upgradeId) const
{
    for (int i = 0; i < kWeaponUpgradePoolCount; ++i) {
        const UpgradeTemplate &entry = kWeaponUpgradePool[i];
        if (upgradeId == QString::fromLatin1(entry.id))
            return QString::fromUtf8(entry.title);
    }
    for (int i = 0; i < kPassiveUpgradePoolCount; ++i) {
        const UpgradeTemplate &entry = kPassiveUpgradePool[i];
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
        case 2: effectText = QStringLiteral("火焰魔杖伤害 +12。"); break;
        case 3: effectText = QStringLiteral("火焰魔杖弹速提升，冷却缩短。"); break;
        case 4: effectText = QStringLiteral("火焰魔杖伤害 +12。"); break;
        case 5: effectText = QStringLiteral("火焰魔杖弹速再次提升，冷却继续缩短。"); break;
        case 6: effectText = QStringLiteral("火焰魔杖伤害 +12。"); break;
        case 7: effectText = QStringLiteral("火焰魔杖弹速提升到更高档，冷却再次缩短。"); break;
        case 8: effectText = QStringLiteral("火焰魔杖伤害 +12。"); break;
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
        case 2: effectText = QStringLiteral("圣水数量 +1，水池半径小幅扩大。"); break;
        case 3: effectText = QStringLiteral("圣水持续时间延长，伤害提升。"); break;
        case 4: effectText = QStringLiteral("圣水数量 +1，水池半径扩大。"); break;
        case 5: effectText = QStringLiteral("圣水持续时间延长，伤害提升。"); break;
        case 6: effectText = QStringLiteral("圣水数量 +1，水池半径扩大。"); break;
        case 7: effectText = QStringLiteral("圣水持续时间延长，伤害提升。"); break;
        case 8: effectText = QStringLiteral("圣水半径小幅扩大，伤害提升。"); break;
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
    const qreal horizontalDrift = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.10;
    number.velocity = QVector2D(horizontalDrift, elite ? -0.16f : -0.12f);
    number.amount = qMax(1, amount);
    number.totalLifeMs = elite ? 920 : 760;
    number.lifeMs = number.totalLifeMs;
    number.elite = elite;
    m_matchState.damageNumbers.append(number);

    while (m_matchState.damageNumbers.size() > 72)
        m_matchState.damageNumbers.removeFirst();
}

void SurvivorController::damageEnemy(int enemyIndex, int damage)
{
    if (enemyIndex < 0 || enemyIndex >= m_matchState.enemies.size())
        return;

    Enemy &enemy = m_matchState.enemies[enemyIndex];
    const int appliedDamage = qMax(1, damage);
    enemy.hp -= appliedDamage;
    enemy.hitFlashMs = enemy.elite ? 120 : 90;
    addDamageNumber(enemy.position, appliedDamage, enemy.elite);
    if (enemy.hp <= 0)
        defeatEnemy(enemyIndex);
}

void SurvivorController::refreshUpgradeSummary()
{
    const PlayerState *player = hudPlayerState();
    if (!player) {
        m_upgradeSummary.clear();
        return;
    }
    m_upgradeSummary = QStringLiteral("武器：飞刀 %1/8 · 秘典 %2/8 · %3 %4/8 · 大蒜 %5/8 · 十字架 %6/8 · %7 %8/8\n被动：翅膀 %9/5 · 空白之书 %10/5 · 烛台 %11/5 · 磁力珠 %12/5 · 空心心脏 %13/5 · 菠菜 %14/5")
        .arg(player->bladeWeaponLevel)
        .arg(player->orbitBladeLevel)
        .arg(player->fireWandEvolved ? QStringLiteral("地狱火") : QStringLiteral("火杖"))
        .arg(player->fireWandLevel)
        .arg(player->garlicLevel)
        .arg(player->crossLevel)
        .arg(player->santaWaterEvolved ? QStringLiteral("黑波拉") : QStringLiteral("圣水"))
        .arg(player->santaWaterLevel)
        .arg(player->wingsPassiveLevel)
        .arg(player->emptyTomePassiveLevel)
        .arg(player->candelabradorPassiveLevel)
        .arg(player->attractorbPassiveLevel)
        .arg(player->hollowHeartPassiveLevel)
        .arg(player->spinachPassiveLevel);
}

void SurvivorController::refreshWaveLabel()
{
    const int newWaveIndex = currentWaveIndex();
    if (m_matchState.lastWaveIndex == newWaveIndex && !m_matchState.waveLabel.isEmpty())
        return;

    m_matchState.lastWaveIndex = newWaveIndex;
    const int waveStartSec = newWaveIndex * 60;
    const int waveEndSec = waveStartSec + 59;
    m_matchState.waveLabel = QString::fromUtf8(kWaveTemplates[newWaveIndex].label)
        + QStringLiteral(" · %1-%2s")
              .arg(waveStartSec)
              .arg(waveEndSec);
}

void SurvivorController::triggerWaveEvents()
{
    const int sec = survivalTimeSec();
    if (m_matchState.triggeredEventSeconds.contains(sec))
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
        m_matchState.triggeredEventSeconds.insert(sec);
}

void SurvivorController::spawnBatSwarm(int count, qreal speedMultiplier)
{
    const qreal angle = QRandomGenerator::global()->generateDouble() * 360.0;
    const QVector2D outward = rotatedVector(QVector2D(1.0f, 0.0f), angle);
    const QVector2D tangent(-outward.y(), outward.x());
    const QVector2D basePosition = playerAnchor() + outward * 1.65f;

    for (int i = 0; i < count; ++i) {
        spawnEnemy(false, BatEnemy, false);
        Enemy &enemy = m_matchState.enemies.last();
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
    const QVector2D anchor = playerAnchor();
    for (int i = 0; i < count; ++i) {
        spawnEnemy(false, FlowerEnemy, false);
        Enemy &enemy = m_matchState.enemies.last();
        const qreal angle = (360.0 / qMax(1, count)) * i;
        const QVector2D direction = rotatedVector(QVector2D(1.0f, 0.0f), angle);
        enemy.position = anchor + direction * static_cast<float>(ringRadius);
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
    return qBound(0, waveIndex, kWaveTemplateCount - 1);
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
    for (const Enemy &enemy : m_matchState.enemies) {
        if (enemy.chestCarrier)
            return true;
    }
    return false;
}

void SurvivorController::gainExp(PlayerState &player, int amount)
{
    player.exp += qMax(0, amount);
    while (player.exp >= player.expToNext) {
        player.exp -= player.expToNext;
        ++player.level;
        player.expToNext = expRequirementForLevel(player.level);
        ++player.pendingLevelUps;
    }

    if (m_matchState.pendingInteractionPlayerId < 0 && player.pendingLevelUps > 0) {
        prepareLevelUpChoices(player);
        player.attackCooldownMs = 0;
    }
    syncHudState();
}

void SurvivorController::updateStatusText()
{
    const PlayerState *player = hudPlayerState();
    if (m_matchState.gameOver) {
        m_statusText = QStringLiteral("本局结束，返回房间后可以继续迭代 Survivor MVP。");
        return;
    }

    if (m_matchState.chestPending) {
        m_statusText = m_networkSession && !m_networkAuthoritative
            ? QStringLiteral("房主正在结算宝箱奖励，稍后会继续推进。")
            : QStringLiteral("宝箱开启中：确认本次奖励后继续推进。");
        return;
    }

    if (m_matchState.pendingInteractionPlayerId >= 0 && !m_matchState.levelUpPending) {
        m_statusText = QStringLiteral("队友正在结算强化或宝箱，战场暂停中。");
        return;
    }

    if (m_matchState.levelUpPending) {
        m_statusText = m_networkSession && !m_networkAuthoritative
            ? QStringLiteral("房主正在选择本轮强化，稍后会继续同步战场。")
            : QStringLiteral("升级暂停中：从 3 个强化里选 1 个，然后继续清怪。");
        return;
    }

    if (m_networkSession) {
        m_statusText = m_networkAuthoritative
            ? QStringLiteral("联机同步已接通：当前由房主模拟战场，并向房间广播实时快照。")
            : QStringLiteral("联机同步已接通：当前正在接收房主战场快照。");
        return;
    }

    if (!player) {
        m_statusText.clear();
        return;
    }

    if (player->garlicLevel > 0
        && player->bladeWeaponLevel == 0
        && player->fireWandLevel == 0
        && player->orbitBladeLevel == 0
        && player->crossLevel == 0
        && player->santaWaterLevel == 0) {
        m_statusText = QStringLiteral("大蒜现在是初始武器，负责清理贴身杂兵。先稳住走位，尽快补出飞刀或远程武器。");
        return;
    }

    if (player->fireWandEvolved || player->santaWaterEvolved) {
        m_statusText = QStringLiteral("进化武器已成型：地狱火负责贯穿清线，黑波拉负责持续压场。");
        return;
    }

    if (player->fireWandLevel > 0 || (player->garlicLevel > 0 && player->bladeWeaponLevel > 0)) {
        m_statusText = QStringLiteral("飞刀负责定向输出，火杖负责远程补刀，大蒜与秘典负责贴身压场。");
        return;
    }

    if (player->orbitBladeLevel > 0 || player->bladeWeaponLevel > 0) {
        m_statusText = QStringLiteral("飞刀按移动朝向发射，秘典负责贴身清场。地图跟随滚动，边走边拉怪更安全。");
        return;
    }

    m_statusText = QStringLiteral("大蒜先保命，后续再补飞刀和范围武器决定 build 走向。");
}






