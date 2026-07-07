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
using LanBoard::Survivor::SpawnWeight;
using LanBoard::Survivor::UpgradeTemplate;
using LanBoard::Survivor::EvolutionTemplate;
using LanBoard::Survivor::WaveTemplate;
using LanBoard::Survivor::BossSpawnTemplate;
using LanBoard::Survivor::WaveEventTemplate;
using LanBoard::Survivor::WaveEventBatSwarm;
using LanBoard::Survivor::WaveEventFlowerWall;
using LanBoard::Survivor::SpawnDistanceMin;
using LanBoard::Survivor::SpawnDistanceMax;
using LanBoard::Survivor::ProjectileCleanupDistance;
using LanBoard::Survivor::ProjectileCleanupDistanceSquared;
using LanBoard::Survivor::EnemySeparationCellSize;
using LanBoard::Survivor::SurvivorRunDurationSec;
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

void applyRemoteProgressionSnapshot(LanBoard::Survivor::PlayerState &target,
                                    const LanBoard::Survivor::PlayerState &source)
{
    target.hp = source.hp;
    target.maxHp = source.maxHp;
    target.soulEaterHealedHp = source.soulEaterHealedHp;
    target.soulEaterBonusDamage = source.soulEaterBonusDamage;
    target.alive = source.alive;
    target.colorIndex = source.colorIndex;
    target.level = source.level;
    target.exp = source.exp;
    target.expToNext = source.expToNext;
    target.attackDamage = source.attackDamage;
    target.bladeWeaponLevel = source.bladeWeaponLevel;
    target.projectileCount = source.projectileCount;
    target.projectilePierce = source.projectilePierce;
    target.orbitBladeLevel = source.orbitBladeLevel;
    target.orbitBladeCount = source.orbitBladeCount;
    target.orbitBladeDamage = source.orbitBladeDamage;
    target.orbitBladeEvolved = source.orbitBladeEvolved;
    target.fireWandLevel = source.fireWandLevel;
    target.fireWandDamage = source.fireWandDamage;
    target.fireWandAmount = source.fireWandAmount;
    target.fireWandCooldownBaseMs = source.fireWandCooldownBaseMs;
    target.fireWandProjectileSpeedMultiplier = source.fireWandProjectileSpeedMultiplier;
    target.magicWandLevel = source.magicWandLevel;
    target.magicWandDamage = source.magicWandDamage;
    target.magicWandAmount = source.magicWandAmount;
    target.magicWandCooldownBaseMs = source.magicWandCooldownBaseMs;
    target.magicWandEvolved = source.magicWandEvolved;
    target.garlicLevel = source.garlicLevel;
    target.garlicDamage = source.garlicDamage;
    target.garlicCooldownBaseMs = source.garlicCooldownBaseMs;
    target.garlicEvolved = source.garlicEvolved;
    target.crossLevel = source.crossLevel;
    target.crossDamage = source.crossDamage;
    target.crossAmount = source.crossAmount;
    target.crossPierce = source.crossPierce;
    target.crossEvolved = source.crossEvolved;
    target.santaWaterLevel = source.santaWaterLevel;
    target.santaWaterDamage = source.santaWaterDamage;
    target.santaWaterAmount = source.santaWaterAmount;
    target.santaWaterDurationMs = source.santaWaterDurationMs;
    target.santaWaterCooldownBaseMs = source.santaWaterCooldownBaseMs;
    target.wingsPassiveLevel = source.wingsPassiveLevel;
    target.emptyTomePassiveLevel = source.emptyTomePassiveLevel;
    target.candelabradorPassiveLevel = source.candelabradorPassiveLevel;
    target.attractorbPassiveLevel = source.attractorbPassiveLevel;
    target.hollowHeartPassiveLevel = source.hollowHeartPassiveLevel;
    target.spinachPassiveLevel = source.spinachPassiveLevel;
    target.bracerPassiveLevel = source.bracerPassiveLevel;
    target.spellbinderPassiveLevel = source.spellbinderPassiveLevel;
    target.pummarolaPassiveLevel = source.pummarolaPassiveLevel;
    target.cloverPassiveLevel = source.cloverPassiveLevel;
    target.bladeWeaponEvolved = source.bladeWeaponEvolved;
    target.fireWandEvolved = source.fireWandEvolved;
    target.santaWaterEvolved = source.santaWaterEvolved;
    target.orbitBladeDurationMs = source.orbitBladeDurationMs;
    target.orbitBladeCooldownBaseMs = source.orbitBladeCooldownBaseMs;
}

int weaponLevelValue(const LanBoard::Survivor::PlayerState &player,
                     LanBoard::Survivor::WeaponType type)
{
    switch (type) {
    case LanBoard::Survivor::WeaponKnife:
        return player.bladeWeaponLevel;
    case LanBoard::Survivor::WeaponOrbitBlade:
        return player.orbitBladeLevel;
    case LanBoard::Survivor::WeaponFireWand:
        return player.fireWandLevel;
    case LanBoard::Survivor::WeaponMagicWand:
        return player.magicWandLevel;
    case LanBoard::Survivor::WeaponGarlic:
        return player.garlicLevel;
    case LanBoard::Survivor::WeaponCross:
        return player.crossLevel;
    case LanBoard::Survivor::WeaponSantaWater:
        return player.santaWaterLevel;
    case LanBoard::Survivor::WeaponCount:
        break;
    }
    return 0;
}

int passiveLevelValue(const LanBoard::Survivor::PlayerState &player,
                      LanBoard::Survivor::PassiveType type)
{
    switch (type) {
    case LanBoard::Survivor::PassiveWings:
        return player.wingsPassiveLevel;
    case LanBoard::Survivor::PassiveEmptyTome:
        return player.emptyTomePassiveLevel;
    case LanBoard::Survivor::PassiveCandelabrador:
        return player.candelabradorPassiveLevel;
    case LanBoard::Survivor::PassiveAttractorb:
        return player.attractorbPassiveLevel;
    case LanBoard::Survivor::PassiveHollowHeart:
        return player.hollowHeartPassiveLevel;
    case LanBoard::Survivor::PassiveSpinach:
        return player.spinachPassiveLevel;
    case LanBoard::Survivor::PassiveBracer:
        return player.bracerPassiveLevel;
    case LanBoard::Survivor::PassiveSpellbinder:
        return player.spellbinderPassiveLevel;
    case LanBoard::Survivor::PassivePummarola:
        return player.pummarolaPassiveLevel;
    case LanBoard::Survivor::PassiveClover:
        return player.cloverPassiveLevel;
    case LanBoard::Survivor::PassiveCount:
        break;
    }
    return 0;
}

const UpgradeTemplate *kWeaponUpgradePool = LanBoard::Survivor::weaponUpgradePool();
const int kWeaponUpgradePoolCount = LanBoard::Survivor::weaponUpgradePoolCount();
const UpgradeTemplate *kPassiveUpgradePool = LanBoard::Survivor::passiveUpgradePool();
const int kPassiveUpgradePoolCount = LanBoard::Survivor::passiveUpgradePoolCount();
const WaveTemplate *kWaveTemplates = LanBoard::Survivor::waveTemplates();
const int kWaveTemplateCount = LanBoard::Survivor::waveTemplateCount();
const BossSpawnTemplate *kBossSpawnSchedule = LanBoard::Survivor::bossSpawnSchedule();
const int kBossSpawnScheduleCount = LanBoard::Survivor::bossSpawnScheduleCount();
const WaveEventTemplate *kWaveEventSchedule = LanBoard::Survivor::waveEventSchedule();
const int kWaveEventScheduleCount = LanBoard::Survivor::waveEventScheduleCount();

}

SurvivorController::SurvivorController(QObject *parent)
    : GameControllerBase(parent)
{
    m_tickTimer.setInterval(TickIntervalMs);
    m_tickTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &SurvivorController::tick);
    resetState();
}

void SurvivorController::startNewGame()
{
    stopRun();
    resetState();
}

void SurvivorController::reset()
{
    stopRun();
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

    if (player->bladeWeaponLevel > 0) {
        appendWeapon(player->bladeWeaponEvolved ? QStringLiteral("千刃") : QStringLiteral("飞刀"),
                     QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4 / 穿透 %5")
                         .arg(player->bladeWeaponEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->bladeWeaponLevel)
                         .arg(qRound(player->attackDamage * currentDamageMultiplier(*player)))
                         .arg(player->projectileCount)
                         .arg(player->projectilePierce),
                     true,
                     QStringLiteral("#F6D782"));
    }

    if (player->orbitBladeLevel > 0) {
        appendWeapon(player->orbitBladeEvolved ? QStringLiteral("邪恶晚祷") : QStringLiteral("秘典"),
                     QStringLiteral("%1Lv.%2/8 · 环刃 %3 / 伤害 %4")
                         .arg(player->orbitBladeEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->orbitBladeLevel)
                         .arg(player->orbitBladeCount)
                         .arg(qRound(player->orbitBladeDamage * currentDamageMultiplier(*player))),
                     true,
                     QStringLiteral("#B4E0D2"));
    }

    if (player->fireWandLevel > 0) {
        appendWeapon(player->fireWandEvolved ? QStringLiteral("地狱火") : QStringLiteral("火杖"),
                     QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4 / 冷却 %5s / 弹速 x%6")
                         .arg(player->fireWandEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->fireWandLevel)
                         .arg(qRound(player->fireWandDamage * currentDamageMultiplier(*player)))
                         .arg(player->fireWandAmount)
                         .arg(player->fireWandCooldownBaseMs * currentCooldownMultiplier(*player) / 1000.0, 0, 'f', 2)
                         .arg(QString::number(player->fireWandProjectileSpeedMultiplier, 'f', 2)),
                     true,
                     QStringLiteral("#E98B61"));
    }

    if (player->magicWandLevel > 0) {
        appendWeapon(player->magicWandEvolved ? QStringLiteral("圣魔杖") : QStringLiteral("魔杖"),
                     QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4 / 冷却 %5s")
                         .arg(player->magicWandEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->magicWandLevel)
                         .arg(qRound(player->magicWandDamage * currentDamageMultiplier(*player)))
                         .arg(player->magicWandAmount)
                         .arg(player->magicWandCooldownBaseMs * currentCooldownMultiplier(*player) / 1000.0, 0, 'f', 2),
                     true,
                     QStringLiteral("#92A8F8"));
    }

    if (player->garlicLevel > 0) {
        appendWeapon(player->garlicEvolved ? QStringLiteral("噬魂者") : QStringLiteral("大蒜"),
                     QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 半径 %4%5")
                         .arg(player->garlicEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->garlicLevel)
                         .arg(qRound(player->garlicDamage * currentDamageMultiplier(*player)))
                         .arg(QString::number(m_matchState.worldRuntime.garlicRadius * currentAreaMultiplier(*player), 'f', 2))
                         .arg(player->garlicEvolved && player->soulEaterBonusDamage > 0
                                  ? QStringLiteral(" / 成长 +%1").arg(player->soulEaterBonusDamage)
                                  : QString()),
                     true,
                     QStringLiteral("#D8F0B5"));
    }

    if (player->crossLevel > 0) {
        appendWeapon(player->crossEvolved ? QStringLiteral("天堂之剑") : QStringLiteral("十字架"),
                     QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4%5")
                         .arg(player->crossEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->crossLevel)
                         .arg(qRound(player->crossDamage * currentDamageMultiplier(*player)))
                         .arg(player->crossAmount)
                         .arg(player->crossEvolved
                                  ? QStringLiteral(" / 暴击 %1%").arg(qRound(heavenSwordCritChance(*player) * 100.0))
                                  : QString()),
                     true,
                     QStringLiteral("#EFD7A6"));
    }

    if (player->santaWaterLevel > 0) {
        appendWeapon(player->santaWaterEvolved ? QStringLiteral("黑波拉") : QStringLiteral("圣水"),
                     QStringLiteral("%1Lv.%2/8 · 伤害 %3 / 数量 %4 / 冷却 %5s")
                         .arg(player->santaWaterEvolved ? QStringLiteral("已进化 · ") : QString())
                         .arg(player->santaWaterLevel)
                         .arg(qRound(player->santaWaterDamage * currentDamageMultiplier(*player)))
                         .arg(player->santaWaterAmount)
                         .arg(player->santaWaterCooldownBaseMs * currentCooldownMultiplier(*player) / 1000.0, 0, 'f', 2),
                     true,
                     QStringLiteral("#86AAF6"));
    }

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
    if (player->bracerPassiveLevel > 0) {
        appendPassive(QStringLiteral("护腕"),
                      QStringLiteral("Lv.%1/5 · 弹速 x%2").arg(player->bracerPassiveLevel).arg(QString::number(currentProjectileSpeedMultiplier(*player), 'f', 2)),
                      QStringLiteral("#E2C58D"));
    }
    if (player->spellbinderPassiveLevel > 0) {
        appendPassive(QStringLiteral("咒缚"),
                      QStringLiteral("Lv.%1/5 · 持续 x%2").arg(player->spellbinderPassiveLevel).arg(QString::number(currentDurationMultiplier(*player), 'f', 2)),
                      QStringLiteral("#A5CBE7"));
    }
    if (player->pummarolaPassiveLevel > 0) {
        appendPassive(QStringLiteral("番茄"),
                      QStringLiteral("Lv.%1/5 · 回复 %2/s").arg(player->pummarolaPassiveLevel).arg(QString::number(currentRecoveryPerSecond(*player), 'f', 1)),
                      QStringLiteral("#D9836B"));
    }
    if (player->cloverPassiveLevel > 0) {
        appendPassive(QStringLiteral("四叶草"),
                      QStringLiteral("Lv.%1/5 · 幸运 x%2").arg(player->cloverPassiveLevel).arg(QString::number(currentLuckMultiplier(*player), 'f', 2)),
                      QStringLiteral("#A7D37A"));
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
    m_networkHudDirty = true;
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

    m_networkHudDirty = true;
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

    m_networkHudDirty = true;
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
    m_networkHudDirty = true;
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

    QList<UpgradeChoice> preservedChoices;
    QList<ChestReward> preservedRewards;
    QString preservedChestTitle;

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
            const int preservedMagicWandCooldownMs = local->magicWandCooldownMs;
            const int preservedCrossCooldownMs = local->crossCooldownMs;
            const int preservedSantaWaterCooldownMs = local->santaWaterCooldownMs;
            const qreal preservedContactDamageCarry = local->contactDamageCarry;
            preservedChoices = local->levelUpChoices;
            preservedRewards = local->chestRewardEntries;
            preservedChestTitle = local->chestTitle;

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
            local->magicWandCooldownMs = preservedMagicWandCooldownMs;
            local->crossCooldownMs = preservedCrossCooldownMs;
            local->santaWaterCooldownMs = preservedSantaWaterCooldownMs;
            local->contactDamageCarry = preservedContactDamageCarry;
            local->levelUpChoices = preservedChoices;
            local->chestRewardEntries = preservedRewards;
            local->chestTitle = preservedChestTitle;
        }

        for (PlayerState &player : decoded.players) {
            if (player.playerId != m_localPlayerId)
                continue;
            applyRemoteProgressionSnapshot(player, decoded.localPlayer);
            player.levelUpChoices = preservedChoices;
            player.chestRewardEntries = preservedRewards;
            player.chestTitle = preservedChestTitle;
            player.local = true;
            break;
        }
    }
    m_cachedDamageNumbers.clear();

    const bool immediate = m_renderSnapshot.players.isEmpty();
    adoptRemoteSnapshot(decoded.players, decoded.snapshot, immediate);
    syncHudState();
    m_cachedDamageNumbers = exportDamageNumberVariantList();

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

    m_cachedDamageNumbers = exportDamageNumberVariantList();

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

    snapshot.damageNumbers.reserve(m_renderSnapshot.damageNumbers.size());
    for (const RenderDamageNumber &number : m_renderSnapshot.damageNumbers) {
        if (insideRange(number.x, number.y, 0.04))
            snapshot.damageNumbers.append(number);
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
    if (m_networkSession && !m_networkAuthoritative) {
        numbers.reserve(m_renderSnapshot.damageNumbers.size());
        for (const RenderDamageNumber &number : m_renderSnapshot.damageNumbers) {
            QVariantMap map;
            map[QStringLiteral("x")] = number.x;
            map[QStringLiteral("y")] = number.y;
            map[QStringLiteral("amount")] = number.amount;
            map[QStringLiteral("lifeMs")] = number.lifeMs;
            map[QStringLiteral("totalLifeMs")] = number.totalLifeMs;
            map[QStringLiteral("elite")] = number.elite;
            numbers.append(map);
        }
        return numbers;
    }

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

    bool includeHudDetails = force || m_networkHudDirty || m_matchState.gameOver;
    if (!force) {
        m_networkBroadcastAccumulatorMs += TickIntervalMs;
        if (m_networkBroadcastAccumulatorMs < NetworkSnapshotIntervalMs)
            return;
        m_networkHudBroadcastAccumulatorMs += TickIntervalMs;
        if (m_networkHudBroadcastAccumulatorMs >= NetworkHudSnapshotIntervalMs)
            includeHudDetails = true;
    }

    m_networkBroadcastAccumulatorMs = 0;
    if (includeHudDetails) {
        m_networkHudBroadcastAccumulatorMs = 0;
        m_networkHudDirty = false;
    }
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
    if (m_matchState.pendingInteractionPlayerId >= 0) {
        refreshFrameCache();
        emitNetworkSyncIfNeeded();
        emit frameChanged();
        return;
    }

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
        player.magicWandCooldownMs = qMax(0, player.magicWandCooldownMs - elapsedMs);
        player.crossCooldownMs = qMax(0, player.crossCooldownMs - elapsedMs);
        player.santaWaterCooldownMs = qMax(0, player.santaWaterCooldownMs - elapsedMs);
    }
    refreshWaveLabel();
    processWaveEvents();

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
        if (player.hp > 0 && player.hp < player.maxHp) {
            player.recoveryCarry += currentRecoveryPerSecond(player) * deltaSec;
            const int recoveredHp = static_cast<int>(player.recoveryCarry);
            if (recoveredHp > 0) {
                player.recoveryCarry -= recoveredHp;
                healPlayer(player, recoveredHp);
            }
        } else {
            player.recoveryCarry = 0.0f;
        }
        player.alive = player.hp > 0;
        anyAlive = anyAlive || player.alive;
    }

    if (!anyAlive) {
        m_matchState.gameOver = true;
        m_matchWinner = 0;
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
        const int eliteCount = m_matchState.enemies.size() > currentEnemyCap() * 4 / 3 ? 1 : currentEliteSpawnBurstCount();
        for (int i = 0; i < eliteCount; ++i)
            spawnEnemy(true, currentEliteKind(), false);
    }

    while (m_matchState.nextBossSpawnIndex < kBossSpawnScheduleCount
           && survivalTimeSec() >= kBossSpawnSchedule[m_matchState.nextBossSpawnIndex].second
           && !hasLivingBoss()) {
        spawnEnemy(true, kBossSpawnSchedule[m_matchState.nextBossSpawnIndex].kind, true);
        ++m_matchState.nextBossSpawnIndex;
    }

    applyAutoAttack();
    updateGarlicAura();
    updateOrbitals(deltaSec, elapsedMs);
    updateProjectiles(deltaSec, elapsedMs);
    updateZones(deltaSec, elapsedMs);
    collectPickups(deltaSec);

    if (survivalTimeSec() >= SurvivorRunDurationSec) {
        m_matchState.gameOver = true;
        m_matchWinner = 1;
        m_matchState.running = false;
        m_tickTimer.stop();
        updateStatusText();
    }
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
        const qreal projectileSpeedMultiplier = currentProjectileSpeedMultiplier(player);
        const auto &knifeProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileKnife);
        const auto &fireWandProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileFireWand);
        const auto &hellfireProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileHellfire);
        const auto &magicWandProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileMagicWand);
        const auto &holyWandProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileHolyWand);
        const auto &crossProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileCross);
        const auto &santaWaterProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileSantaWater);
        const auto &laBorraProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileLaBorra);

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
                projectile.lifeMs = knifeProfile.lifeMs + (player.projectilePierce - 1) * 180;
                projectile.radius = knifeProfile.radius + qMin<qreal>(0.006f, 0.0015f * player.projectilePierce);
                projectile.knockback = knifeProfile.knockback;
                projectile.damageVariance = knifeProfile.damageVariance;
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

            const auto &profile = player.fireWandEvolved ? hellfireProfile : fireWandProfile;
            const qreal centerOffset = (player.fireWandAmount - 1) / 2.0;
            for (int i = 0; i < player.fireWandAmount; ++i) {
                const qreal spreadDegrees = (static_cast<qreal>(i) - centerOffset) * 6.5;
                Projectile projectile;
                projectile.kind = player.fireWandEvolved ? 3 : 1;
                projectile.sourceId = m_matchState.nextSourceId++;
                projectile.position = player.position + fireDirection * 0.024f;
                projectile.velocity = rotatedVector(fireDirection, spreadDegrees)
                    * static_cast<float>(m_matchState.worldRuntime.projectileSpeed
                                         * profile.speedMultiplier
                                         * projectileSpeedMultiplier
                                         * player.fireWandProjectileSpeedMultiplier);
                projectile.damage = qMax(1, qRound(player.fireWandDamage * damageMultiplier * profile.damageMultiplier));
                projectile.hitIntervalMs = 1000000;
                projectile.remainingHits = profile.remainingHits;
                projectile.lifeMs = profile.lifeMs;
                projectile.radius = profile.radius;
                projectile.knockback = profile.knockback;
                projectile.damageVariance = profile.damageVariance;
                m_matchState.projectiles.append(projectile);
            }
        }

        if (player.magicWandLevel > 0 && player.magicWandCooldownMs <= 0) {
            player.magicWandCooldownMs = qMax(120, qRound(player.magicWandCooldownBaseMs * cooldownMultiplier));
            const auto &profile = player.magicWandEvolved ? holyWandProfile : magicWandProfile;
            const int wandDamage = qMax(1, qRound(player.magicWandDamage * damageMultiplier * profile.damageMultiplier));

            QVector<int> targetIndices;
            QVector<qreal> targetDistances;
            targetIndices.reserve(player.magicWandAmount);
            targetDistances.reserve(player.magicWandAmount);
            for (int i = 0; i < m_matchState.enemies.size(); ++i) {
                const qreal distanceSquared = (m_matchState.enemies.at(i).position - player.position).lengthSquared();
                int insertIndex = 0;
                while (insertIndex < targetDistances.size() && targetDistances.at(insertIndex) <= distanceSquared)
                    ++insertIndex;
                if (insertIndex >= player.magicWandAmount)
                    continue;
                targetDistances.insert(insertIndex, distanceSquared);
                targetIndices.insert(insertIndex, i);
                if (targetIndices.size() > player.magicWandAmount) {
                    targetIndices.removeLast();
                    targetDistances.removeLast();
                }
            }

            for (int i = 0; i < targetIndices.size(); ++i) {
                QVector2D castDirection = m_matchState.enemies.at(targetIndices.at(i)).position - player.position;
                if (castDirection.lengthSquared() <= 0.0001f)
                    castDirection = rotatedVector(QVector2D(1.0f, 0.0f), i * 18.0);
                castDirection.normalize();

                Projectile projectile;
                projectile.kind = player.magicWandEvolved ? 5 : 4;
                projectile.sourceId = m_matchState.nextSourceId++;
                projectile.position = player.position + castDirection * 0.022f;
                projectile.velocity = rotatedVector(castDirection, (i - (player.magicWandAmount - 1) / 2.0) * 4.0)
                    * static_cast<float>(m_matchState.worldRuntime.projectileSpeed
                                         * projectileSpeedMultiplier
                                         * profile.speedMultiplier);
                projectile.damage = wandDamage;
                projectile.hitIntervalMs = 1000000;
                projectile.remainingHits = profile.remainingHits;
                projectile.lifeMs = profile.lifeMs;
                projectile.radius = profile.radius;
                projectile.knockback = profile.knockback;
                projectile.damageVariance = profile.damageVariance;
                m_matchState.projectiles.append(projectile);
            }
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
                const bool critical = player.crossEvolved
                    && QRandomGenerator::global()->generateDouble() < heavenSwordCritChance(player);
                crossProjectile.velocity = crossDirection
                    * static_cast<float>(m_matchState.worldRuntime.crossSpeed * (player.crossEvolved ? 1.10f : 1.0f));
                crossProjectile.damage = critical ? qMax(1, qRound(crossDamage * 2.5)) : crossDamage;
                crossProjectile.hitIntervalMs = 1000000;
                crossProjectile.remainingHits = player.crossPierce;
                crossProjectile.lifeMs = player.crossEvolved ? crossProfile.lifeMs + 960 : crossProfile.lifeMs;
                crossProjectile.radius = m_matchState.worldRuntime.crossRadius * currentAreaMultiplier(player);
                crossProjectile.returning = false;
                crossProjectile.knockback = crossProfile.knockback;
                crossProjectile.damageVariance = crossProfile.damageVariance;
                m_matchState.projectiles.append(crossProjectile);
            }
        }

        if (player.santaWaterLevel > 0 && player.santaWaterCooldownMs <= 0) {
            player.santaWaterCooldownMs = qMax(620, qRound(player.santaWaterCooldownBaseMs * cooldownMultiplier));
            const auto &profile = player.santaWaterEvolved ? laBorraProfile : santaWaterProfile;
            const int zoneDamage = qMax(1, qRound(player.santaWaterDamage * damageMultiplier * profile.damageMultiplier));
            const qreal zoneRadius = m_matchState.worldRuntime.santaWaterRadius
                * currentAreaMultiplier(player)
                * profile.areaMultiplier;
            QVector2D targetCenter = player.position;
            if (!player.santaWaterEvolved) {
                int nearestIndex = -1;
                qreal nearestDistance = std::numeric_limits<qreal>::max();
                for (int enemyIndex = 0; enemyIndex < m_matchState.enemies.size(); ++enemyIndex) {
                    const qreal distanceSquared =
                        (m_matchState.enemies.at(enemyIndex).position - player.position).lengthSquared();
                    if (distanceSquared >= nearestDistance)
                        continue;
                    nearestDistance = distanceSquared;
                    nearestIndex = enemyIndex;
                }
                if (nearestIndex >= 0)
                    targetCenter = m_matchState.enemies.at(nearestIndex).position;
            }
            for (int i = 0; i < player.santaWaterAmount; ++i) {
                const qreal angle = player.santaWaterEvolved
                    ? (360.0 / qMax(1, player.santaWaterAmount)) * i
                    : QRandomGenerator::global()->generateDouble() * 360.0;
                const qreal distance = player.santaWaterEvolved
                    ? 0.30 + 0.09 * (i % 2)
                    : 0.06 + QRandomGenerator::global()->generateDouble() * 0.16;
                Zone zone;
                zone.kind = player.santaWaterEvolved ? 1 : 0;
                zone.sourceId = m_matchState.nextSourceId++;
                zone.position = targetCenter + rotatedVector(QVector2D(distance, 0.0f), angle);
                zone.radius = zoneRadius;
                zone.damage = zoneDamage;
                zone.totalLifeMs = qRound(player.santaWaterDurationMs * currentDurationMultiplier(player) * profile.durationMultiplier);
                zone.lifeMs = zone.totalLifeMs;
                zone.tickIntervalMs = qMax(170, qRound(profile.tickIntervalMs * cooldownMultiplier));
                zone.tickCooldownMs = 0;
                zone.knockback = profile.knockback;
                zone.damageVariance = profile.damageVariance;
                m_matchState.zones.append(zone);
            }
        }

        if (player.orbitBladeLevel > 0
            && player.orbitBladeCount > 0
            && player.orbitBladeActiveMs <= 0
            && player.orbitBladeCooldownMs <= 0) {
            player.orbitBladeActiveMs = qRound(player.orbitBladeDurationMs * currentDurationMultiplier(player));
            player.orbitBladeCooldownMs = qMax(450, qRound(player.orbitBladeCooldownBaseMs * cooldownMultiplier));
        }
    }
}
void SurvivorController::updateGarlicAura()
{
    if (m_matchState.enemies.isEmpty())
        return;

    const auto &garlicProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileGarlic);
    for (const PlayerState &player : std::as_const(m_matchState.players)) {
        if (!player.alive || player.garlicLevel <= 0)
            continue;
        const qreal auraRadius = m_matchState.worldRuntime.garlicRadius * currentAreaMultiplier(player);
        const int auraDamage = qMax(1, qRound((player.garlicDamage + player.soulEaterBonusDamage)
                                              * currentDamageMultiplier(player)));
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
                        garlicProfile.damageVariance,
                        enemy.elite ? 0.022f : garlicProfile.knockback,
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
    const auto &orbitProfile = LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileOrbitBlade);
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
                            orbitProfile.tickIntervalMs,
                            orbitalDamage,
                            orbitProfile.damageVariance,
                            orbitProfile.knockback);
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
                            * static_cast<float>(m_matchState.worldRuntime.crossSpeed
                                                 * LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileCross).returnSpeedMultiplier);
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
                zone.position += toPlayer.normalized()
                    * static_cast<float>(LanBoard::Survivor::weaponHitProfile(LanBoard::Survivor::AttackProfileLaBorra).driftSpeed * deltaSec);
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
        const int nearestPlayerIndex = nearestLivingPlayerIndex(enemy.position);
        const PlayerState *rewardPlayer = nearestPlayerIndex >= 0
            ? &m_matchState.players.at(nearestPlayerIndex)
            : nullptr;
        pickup.kind = ChestPickup;
        pickup.exp = 0;
        pickup.radius = 0.028f;
        pickup.rewardCount = rollChestRewardCount(rewardPlayer);
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
            if (currentLevel <= 0) {
                int usedSlots = 0;
                if (isWeaponUpgrade(id)) {
                    for (int weaponIndex = 0; weaponIndex < LanBoard::Survivor::WeaponCount; ++weaponIndex) {
                        if (weaponLevelValue(player, static_cast<LanBoard::Survivor::WeaponType>(weaponIndex)) > 0)
                            ++usedSlots;
                    }
                } else {
                    for (int passiveIndex = 0; passiveIndex < LanBoard::Survivor::PassiveCount; ++passiveIndex) {
                        if (passiveLevelValue(player, static_cast<LanBoard::Survivor::PassiveType>(passiveIndex)) > 0)
                            ++usedSlots;
                    }
                }
                if (usedSlots >= 6)
                    continue;
            }
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
        m_networkHudDirty = true;
        syncHudState();
        emitNetworkSyncIfNeeded(true);
        return;
    }

    m_matchState.pendingInteractionPlayerId = player.playerId;
    m_matchState.pendingInteractionElapsedMs = 0;
    m_networkHudDirty = true;
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

int SurvivorController::rollChestRewardCount(const PlayerState *player) const
{
    const qreal luck = player ? currentLuckMultiplier(*player) : 1.0;
    const int roll = QRandomGenerator::global()->bounded(100);
    if (roll < qRound(5 * luck))
        return 5;
    if (roll < qRound(25 * luck))
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
    const EvolutionTemplate *templateInfo = LanBoard::Survivor::evolutionTemplateForWeaponId(weaponId);
    if (!templateInfo)
        return false;

    const int weaponIndex = LanBoard::Survivor::weaponIndexForId(weaponId);
    const int passiveIndex = LanBoard::Survivor::passiveIndexForId(QString::fromLatin1(templateInfo->requiredPassiveId));
    if (weaponIndex < 0 || passiveIndex < 0)
        return false;

    const auto weaponType = static_cast<LanBoard::Survivor::WeaponType>(weaponIndex);
    const auto passiveType = static_cast<LanBoard::Survivor::PassiveType>(passiveIndex);
    return !isWeaponEvolved(player, weaponType)
        && weaponLevelValue(player, weaponType) >= templateInfo->requiredWeaponLevel
        && passiveLevelValue(player, passiveType) >= templateInfo->requiredPassiveLevel;
}

QList<QString> SurvivorController::currentEvolutionCandidates(const PlayerState &player) const
{
    QList<QString> ids;
    for (int weaponIndex = 0; weaponIndex < LanBoard::Survivor::WeaponCount; ++weaponIndex) {
        const UpgradeTemplate &entry = kWeaponUpgradePool[weaponIndex];
        const QString weaponId = QString::fromLatin1(entry.id);
        if (canEvolveWeapon(player, weaponId))
            ids.append(weaponId);
    }
    return ids;
}

QString SurvivorController::evolvedTitleForWeapon(const QString &weaponId) const
{
    if (const EvolutionTemplate *templateInfo = LanBoard::Survivor::evolutionTemplateForWeaponId(weaponId))
        return QString::fromUtf8(templateInfo->evolvedTitle);
    return titleForUpgrade(weaponId);
}

QString SurvivorController::evolvedDescriptionForWeapon(const QString &weaponId) const
{
    if (const EvolutionTemplate *templateInfo = LanBoard::Survivor::evolutionTemplateForWeaponId(weaponId))
        return QString::fromUtf8(templateInfo->evolvedDescription);
    return QStringLiteral("武器进化完成。");
}

bool SurvivorController::applyEvolution(PlayerState &player, const QString &weaponId)
{
    if (!canEvolveWeapon(player, weaponId))
        return false;

    if (weaponId == QStringLiteral("knife_weapon")) {
        player.bladeWeaponEvolved = true;
        player.attackDamage = qMax(player.attackDamage, 22);
        player.attackCooldownBaseMs = qMin(player.attackCooldownBaseMs, 240);
        player.projectileCount = qMax(player.projectileCount, 5);
        player.projectilePierce = qMax(player.projectilePierce, 3);
        m_matchState.worldRuntime.projectileSpeed = qMax<qreal>(m_matchState.worldRuntime.projectileSpeed, 1.45f);
    } else if (weaponId == QStringLiteral("orbit_weapon")) {
        player.orbitBladeEvolved = true;
        player.orbitBladeDamage = qMax(player.orbitBladeDamage, 28);
        player.orbitBladeCount = qMax(player.orbitBladeCount, 4);
        player.orbitBladeDurationMs = qMax(player.orbitBladeDurationMs, 3000);
        player.orbitBladeCooldownBaseMs = qMin(player.orbitBladeCooldownBaseMs, 3000);
        m_matchState.worldRuntime.orbitBladeRadius = qMax<qreal>(m_matchState.worldRuntime.orbitBladeRadius, 0.20f);
        m_matchState.worldRuntime.orbitBladeAngularSpeedDeg = qMax<qreal>(m_matchState.worldRuntime.orbitBladeAngularSpeedDeg, 210.0f);
    } else if (weaponId == QStringLiteral("firewand_weapon")) {
        player.fireWandEvolved = true;
        player.fireWandDamage = qMax(player.fireWandDamage, 90);
        player.fireWandCooldownBaseMs = qMin(player.fireWandCooldownBaseMs, 3000);
        player.fireWandAmount = qMax(player.fireWandAmount, 3);
        player.fireWandProjectileSpeedMultiplier = qMax<qreal>(player.fireWandProjectileSpeedMultiplier, 1.30f);
    } else if (weaponId == QStringLiteral("magicwand_weapon")) {
        player.magicWandEvolved = true;
        player.magicWandDamage = qMax(player.magicWandDamage, 24);
        player.magicWandAmount = qMax(player.magicWandAmount, 4);
        player.magicWandCooldownBaseMs = qMin(player.magicWandCooldownBaseMs, 180);
    } else if (weaponId == QStringLiteral("garlic_weapon")) {
        player.garlicEvolved = true;
        player.garlicDamage = qMax(player.garlicDamage, 18);
        player.garlicCooldownBaseMs = qMin(player.garlicCooldownBaseMs, 680);
        m_matchState.worldRuntime.garlicRadius = qMax<qreal>(m_matchState.worldRuntime.garlicRadius, 0.22f);
    } else if (weaponId == QStringLiteral("cross_weapon")) {
        player.crossEvolved = true;
        player.crossDamage = qMax(player.crossDamage, 42);
        player.crossAmount = qMax(player.crossAmount, 3);
        player.crossCooldownBaseMs = qMin(player.crossCooldownBaseMs, 1700);
        m_matchState.worldRuntime.crossRadius = qMax<qreal>(m_matchState.worldRuntime.crossRadius, 0.024f);
        m_matchState.worldRuntime.crossSpeed = qMax<qreal>(m_matchState.worldRuntime.crossSpeed, 1.46f);
    } else if (weaponId == QStringLiteral("santawater_weapon")) {
        player.santaWaterEvolved = true;
        player.santaWaterDamage = qMax(player.santaWaterDamage, 50);
        player.santaWaterDurationMs = qMax(player.santaWaterDurationMs, 3500);
        m_matchState.worldRuntime.santaWaterRadius = qMax<qreal>(m_matchState.worldRuntime.santaWaterRadius, 0.132f);
        player.santaWaterCooldownBaseMs = qMin(player.santaWaterCooldownBaseMs, 4500);
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
    m_networkHudDirty = true;
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
    const int weaponIndex = LanBoard::Survivor::weaponIndexForId(upgradeId);
    if (weaponIndex >= 0) {
        applyWeaponUpgradeLevel(player,
                                static_cast<LanBoard::Survivor::WeaponType>(weaponIndex),
                                newLevel);
    } else {
        const int passiveIndex = LanBoard::Survivor::passiveIndexForId(upgradeId);
        if (passiveIndex >= 0) {
            applyPassiveUpgradeLevel(player,
                                     static_cast<LanBoard::Survivor::PassiveType>(passiveIndex),
                                     newLevel);
        }
    }

    refreshDerivedStats();
    syncHudState();
}

void SurvivorController::applyWeaponUpgradeLevel(PlayerState &player,
                                                 LanBoard::Survivor::WeaponType type,
                                                 int newLevel)
{
    using namespace LanBoard::Survivor;

    const WeaponLevelInfo *table = weaponLevelTable(type);
    if (!table || newLevel < 1 || newLevel > 8)
        return;

    const WeaponLevelInfo &info = table[newLevel - 1];
    switch (type) {
    case WeaponKnife:
        player.bladeWeaponLevel = newLevel;
        if (newLevel == 1) {
            player.attackDamage = info.damage;
            player.projectileCount = info.count;
            player.projectilePierce = info.pierce;
        } else {
            player.attackDamage += info.damage;
            player.projectileCount = qMin(6, player.projectileCount + info.count);
            player.projectilePierce = qMin(3, player.projectilePierce + info.pierce);
        }
        if (info.cooldownMs > 0)
            player.attackCooldownBaseMs = info.cooldownMs;
        if (info.speed > 0.0f)
            m_matchState.worldRuntime.projectileSpeed = info.speed;
        break;
    case WeaponOrbitBlade:
        player.orbitBladeLevel = newLevel;
        if (newLevel == 1) {
            player.orbitBladeCount = info.count;
            player.orbitBladeDamage = info.damage;
            player.orbitBladeDurationMs = info.durationMs;
        } else {
            player.orbitBladeCount = qMin(4, player.orbitBladeCount + info.count);
            player.orbitBladeDamage += info.damage;
            player.orbitBladeDurationMs += info.durationMs;
        }
        if (info.cooldownMs > 0)
            player.orbitBladeCooldownBaseMs = info.cooldownMs;
        if (info.radius > 0.0f)
            m_matchState.worldRuntime.orbitBladeRadius = info.radius;
        if (info.angularSpeedDeg > 0.0f)
            m_matchState.worldRuntime.orbitBladeAngularSpeedDeg = info.angularSpeedDeg;
        break;
    case WeaponFireWand:
        player.fireWandLevel = newLevel;
        if (newLevel == 1) {
            player.fireWandDamage = info.damage;
            player.fireWandAmount = info.count;
        } else {
            player.fireWandDamage += info.damage;
        }
        if (info.cooldownMs > 0)
            player.fireWandCooldownBaseMs = info.cooldownMs;
        if (info.speed > 0.0f)
            player.fireWandProjectileSpeedMultiplier = info.speed;
        break;
    case WeaponMagicWand:
        player.magicWandLevel = newLevel;
        if (newLevel == 1) {
            player.magicWandDamage = info.damage;
            player.magicWandAmount = info.count;
        } else {
            player.magicWandDamage += info.damage;
            player.magicWandAmount = qMin(4, player.magicWandAmount + info.count);
        }
        if (info.cooldownMs > 0)
            player.magicWandCooldownBaseMs = info.cooldownMs;
        break;
    case WeaponGarlic:
        player.garlicLevel = newLevel;
        if (newLevel == 1)
            player.garlicDamage = info.damage;
        else
            player.garlicDamage += info.damage;
        if (info.cooldownMs > 0)
            player.garlicCooldownBaseMs = info.cooldownMs;
        if (info.radius > 0.0f)
            m_matchState.worldRuntime.garlicRadius = info.radius;
        break;
    case WeaponCross:
        player.crossLevel = newLevel;
        if (newLevel == 1) {
            player.crossDamage = info.damage;
            player.crossAmount = info.count;
        } else {
            player.crossDamage += info.damage;
            player.crossAmount = qMin(3, player.crossAmount + info.count);
        }
        if (info.cooldownMs > 0)
            player.crossCooldownBaseMs = info.cooldownMs;
        if (info.radius > 0.0f)
            m_matchState.worldRuntime.crossRadius = info.radius;
        if (info.speed > 0.0f)
            m_matchState.worldRuntime.crossSpeed = info.speed;
        break;
    case WeaponSantaWater:
        player.santaWaterLevel = newLevel;
        if (newLevel == 1) {
            player.santaWaterDamage = info.damage;
            player.santaWaterAmount = info.count;
            player.santaWaterDurationMs = info.durationMs;
        } else {
            player.santaWaterDamage += info.damage;
            player.santaWaterAmount = qMin(4, player.santaWaterAmount + info.count);
            player.santaWaterDurationMs += info.durationMs;
        }
        if (info.cooldownMs > 0)
            player.santaWaterCooldownBaseMs = info.cooldownMs;
        if (info.radius > 0.0f)
            m_matchState.worldRuntime.santaWaterRadius = info.radius;
        break;
    case WeaponCount:
        break;
    }
}

void SurvivorController::applyPassiveUpgradeLevel(PlayerState &player,
                                                  LanBoard::Survivor::PassiveType type,
                                                  int newLevel)
{
    using namespace LanBoard::Survivor;

    switch (type) {
    case PassiveWings:
        player.wingsPassiveLevel = newLevel;
        break;
    case PassiveEmptyTome:
        player.emptyTomePassiveLevel = newLevel;
        break;
    case PassiveCandelabrador:
        player.candelabradorPassiveLevel = newLevel;
        break;
    case PassiveAttractorb:
        player.attractorbPassiveLevel = newLevel;
        break;
    case PassiveHollowHeart:
        player.hollowHeartPassiveLevel = newLevel;
        break;
    case PassiveSpinach:
        player.spinachPassiveLevel = newLevel;
        break;
    case PassiveBracer:
        player.bracerPassiveLevel = newLevel;
        break;
    case PassiveSpellbinder:
        player.spellbinderPassiveLevel = newLevel;
        break;
    case PassivePummarola:
        player.pummarolaPassiveLevel = newLevel;
        break;
    case PassiveClover:
        player.cloverPassiveLevel = newLevel;
        break;
    case PassiveCount:
        break;
    }
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
    const int appliedDamage = rollDamage(baseDamage, damageVariance);
    const bool defeatedByThisHit = enemy.hp <= appliedDamage;
    damageEnemy(enemyIndex, appliedDamage);

    if (defeatedByThisHit && sourceId >= 1000 && sourceId < 2000) {
        if (PlayerState *owner = playerStateById(sourceId - 1000)) {
            if (owner->garlicEvolved) {
                const qreal restoreChance = qMin<qreal>(0.65, appliedDamage / 140.0);
                if (QRandomGenerator::global()->generateDouble() < restoreChance)
                    healPlayer(*owner, 1);
            }
        }
    }
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
    const int weaponIndex = LanBoard::Survivor::weaponIndexForId(upgradeId);
    if (weaponIndex >= 0) {
        return weaponLevelValue(player,
                                static_cast<LanBoard::Survivor::WeaponType>(weaponIndex));
    }

    const int passiveIndex = LanBoard::Survivor::passiveIndexForId(upgradeId);
    if (passiveIndex >= 0) {
        return passiveLevelValue(player,
                                 static_cast<LanBoard::Survivor::PassiveType>(passiveIndex));
    }

    return 0;
}

int SurvivorController::maxLevelForUpgrade(const QString &upgradeId) const
{
    if (const UpgradeTemplate *templateInfo = LanBoard::Survivor::upgradeTemplateForId(upgradeId))
        return templateInfo->maxLevel;
    return {};
}

bool SurvivorController::isWeaponUpgrade(const QString &upgradeId) const
{
    return LanBoard::Survivor::isWeaponUpgradeId(upgradeId);
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

qreal SurvivorController::currentDurationMultiplier(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentDurationMultiplier(player);
}

qreal SurvivorController::currentProjectileSpeedMultiplier(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentProjectileSpeedMultiplier(player);
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

qreal SurvivorController::currentRecoveryPerSecond(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentRecoveryPerSecond(player);
}

qreal SurvivorController::currentLuckMultiplier(const PlayerState &player) const
{
    return LanBoard::Survivor::Runtime::currentLuckMultiplier(player);
}

bool SurvivorController::isWeaponEvolved(const PlayerState &player,
                                         LanBoard::Survivor::WeaponType type) const
{
    switch (type) {
    case LanBoard::Survivor::WeaponKnife:
        return player.bladeWeaponEvolved;
    case LanBoard::Survivor::WeaponOrbitBlade:
        return player.orbitBladeEvolved;
    case LanBoard::Survivor::WeaponFireWand:
        return player.fireWandEvolved;
    case LanBoard::Survivor::WeaponMagicWand:
        return player.magicWandEvolved;
    case LanBoard::Survivor::WeaponGarlic:
        return player.garlicEvolved;
    case LanBoard::Survivor::WeaponCross:
        return player.crossEvolved;
    case LanBoard::Survivor::WeaponSantaWater:
        return player.santaWaterEvolved;
    case LanBoard::Survivor::WeaponCount:
        break;
    }
    return false;
}

qreal SurvivorController::heavenSwordCritChance(const PlayerState &player) const
{
    return qMin<qreal>(0.45, 0.10 * currentLuckMultiplier(player));
}

QString SurvivorController::titleForUpgrade(const QString &upgradeId) const
{
    if (const UpgradeTemplate *templateInfo = LanBoard::Survivor::upgradeTemplateForId(upgradeId))
        return QString::fromUtf8(templateInfo->title);
    return QStringLiteral("未知强化");
}

QString SurvivorController::categoryForUpgrade(const QString &upgradeId) const
{
    if (const UpgradeTemplate *templateInfo = LanBoard::Survivor::upgradeTemplateForId(upgradeId))
        return QString::fromUtf8(templateInfo->category);
    return QStringLiteral("强化");
}

QString SurvivorController::descriptionForUpgrade(const QString &upgradeId, int currentLevel) const
{
    using namespace LanBoard::Survivor;
    const int weaponIdx = weaponIndexForId(upgradeId);
    if (weaponIdx >= 0) {
        const int nextLevel = currentLevel + 1;
        if (nextLevel < 1 || nextLevel > 8)
            return {};
        const WeaponLevelInfo *table = weaponLevelTable(static_cast<WeaponType>(weaponIdx));
        if (table)
            return QString::fromUtf8(table[nextLevel - 1].description);
    }

    const int passiveIdx = passiveIndexForId(upgradeId);
    if (passiveIdx >= 0) {
        const int nextLevel = currentLevel + 1;
        if (nextLevel < 1 || nextLevel > 5)
            return {};
        const PassiveLevelInfo *table = passiveLevelTable(static_cast<PassiveType>(passiveIdx));
        if (table)
            return QString::fromUtf8(table[nextLevel - 1].description);
    }

    return {};
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

int SurvivorController::healPlayer(PlayerState &player, int amount)
{
    if (amount <= 0 || player.hp <= 0 || player.hp >= player.maxHp)
        return 0;

    const int healed = qMin(amount, player.maxHp - player.hp);
    player.hp += healed;
    player.soulEaterHealedHp += healed;
    player.soulEaterBonusDamage = qMin(60, player.soulEaterHealedHp / 60);
    return healed;
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

    QStringList weaponParts;
    auto appendWeapon = [&weaponParts](int level,
                                       const QString &title,
                                       bool evolved = false) {
        if (level <= 0)
            return;
        weaponParts.append(QStringLiteral("%1%2 %3/8")
                               .arg(title)
                               .arg(evolved ? QStringLiteral("·超武") : QString())
                               .arg(level));
    };
    appendWeapon(player->garlicLevel,
                 player->garlicEvolved ? QStringLiteral("噬魂者") : QStringLiteral("大蒜"),
                 player->garlicEvolved);
    appendWeapon(player->bladeWeaponLevel,
                 player->bladeWeaponEvolved ? QStringLiteral("千刃") : QStringLiteral("飞刀"),
                 player->bladeWeaponEvolved);
    appendWeapon(player->orbitBladeLevel,
                 player->orbitBladeEvolved ? QStringLiteral("邪恶晚祷") : QStringLiteral("秘典"),
                 player->orbitBladeEvolved);
    appendWeapon(player->fireWandLevel,
                 player->fireWandEvolved ? QStringLiteral("地狱火") : QStringLiteral("火杖"),
                 player->fireWandEvolved);
    appendWeapon(player->magicWandLevel,
                 player->magicWandEvolved ? QStringLiteral("圣魔杖") : QStringLiteral("魔杖"),
                 player->magicWandEvolved);
    appendWeapon(player->crossLevel,
                 player->crossEvolved ? QStringLiteral("天堂之剑") : QStringLiteral("十字架"),
                 player->crossEvolved);
    appendWeapon(player->santaWaterLevel,
                 player->santaWaterEvolved ? QStringLiteral("黑波拉") : QStringLiteral("圣水"),
                 player->santaWaterEvolved);

    QStringList passiveParts;
    auto appendPassive = [&passiveParts](int level, const QString &title) {
        if (level <= 0)
            return;
        passiveParts.append(QStringLiteral("%1 %2/5").arg(title).arg(level));
    };
    appendPassive(player->wingsPassiveLevel, QStringLiteral("翅膀"));
    appendPassive(player->emptyTomePassiveLevel, QStringLiteral("空书"));
    appendPassive(player->candelabradorPassiveLevel, QStringLiteral("烛台"));
    appendPassive(player->attractorbPassiveLevel, QStringLiteral("磁力珠"));
    appendPassive(player->hollowHeartPassiveLevel, QStringLiteral("空心心脏"));
    appendPassive(player->spinachPassiveLevel, QStringLiteral("菠菜"));
    appendPassive(player->bracerPassiveLevel, QStringLiteral("护腕"));
    appendPassive(player->spellbinderPassiveLevel, QStringLiteral("咒缚"));
    appendPassive(player->pummarolaPassiveLevel, QStringLiteral("番茄"));
    appendPassive(player->cloverPassiveLevel, QStringLiteral("四叶草"));

    m_upgradeSummary = QStringLiteral("武器：%1\n被动：%2")
        .arg(weaponParts.isEmpty() ? QStringLiteral("当前仅大蒜开局") : weaponParts.join(QStringLiteral(" · ")))
        .arg(passiveParts.isEmpty() ? QStringLiteral("暂未持有") : passiveParts.join(QStringLiteral(" · ")));
}

void SurvivorController::refreshWaveLabel()
{
    const int newWaveIndex = currentWaveIndex();
    if (m_matchState.lastWaveIndex == newWaveIndex && !m_matchState.waveLabel.isEmpty())
        return;

    m_matchState.lastWaveIndex = newWaveIndex;
    const int waveStartSec = newWaveIndex * 60;
    const int waveEndSec = waveStartSec + 59;
    m_matchState.waveLabel = QString::fromUtf8(currentWaveTemplate().label)
        + QStringLiteral(" · %1-%2s")
              .arg(waveStartSec)
              .arg(waveEndSec);
}

const WaveTemplate &SurvivorController::currentWaveTemplate() const
{
    return kWaveTemplates[currentWaveIndex()];
}

int SurvivorController::rollSpawnKind(const SpawnWeight *weights, int count) const
{
    if (!weights || count <= 0)
        return BatEnemy;

    int totalWeight = 0;
    for (int i = 0; i < count; ++i)
        totalWeight += qMax(0, weights[i].weight);
    if (totalWeight <= 0)
        return weights[0].kind;

    int roll = QRandomGenerator::global()->bounded(totalWeight);
    for (int i = 0; i < count; ++i) {
        roll -= qMax(0, weights[i].weight);
        if (roll < 0)
            return weights[i].kind;
    }
    return weights[count - 1].kind;
}

void SurvivorController::processWaveEvents()
{
    while (m_matchState.nextWaveEventIndex < kWaveEventScheduleCount) {
        const WaveEventTemplate &event = kWaveEventSchedule[m_matchState.nextWaveEventIndex];
        if (survivalTimeSec() < event.second)
            break;

        switch (event.type) {
        case WaveEventBatSwarm:
            spawnBatSwarm(event.count, event.primaryValue);
            break;
        case WaveEventFlowerWall:
            spawnFlowerWall(event.count,
                            event.primaryValue,
                            event.secondaryValue,
                            event.tertiaryValue);
            break;
        default:
            break;
        }

        ++m_matchState.nextWaveEventIndex;
    }
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
    return currentWaveTemplate().spawnIntervalMs;
}

int SurvivorController::currentSpawnBurstCount() const
{
    return currentWaveTemplate().spawnBurst;
}

int SurvivorController::currentEnemyCap() const
{
    return currentWaveTemplate().enemyCap;
}

int SurvivorController::currentEliteSpawnIntervalMs() const
{
    return currentWaveTemplate().eliteIntervalMs;
}

int SurvivorController::currentEliteSpawnBurstCount() const
{
    return currentWaveTemplate().eliteBurst;
}

int SurvivorController::currentWaveIndex() const
{
    const int waveIndex = survivalTimeSec() / 60;
    return qBound(0, waveIndex, kWaveTemplateCount - 1);
}

int SurvivorController::currentEnemyKind() const
{
    return rollSpawnKind(currentWaveTemplate().spawnWeights,
                         currentWaveTemplate().spawnWeightCount);
}

int SurvivorController::currentEliteKind() const
{
    return rollSpawnKind(currentWaveTemplate().eliteWeights,
                         currentWaveTemplate().eliteWeightCount);
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
        m_statusText = survivalTimeSec() >= SurvivorRunDurationSec
            ? QStringLiteral("你撑到了 15:00。按原版前段节奏改造的这局已经收束，返回房间后可以继续再开一把。")
            : QStringLiteral("本局结束，返回房间后可以继续迭代 Survivor MVP。");
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
