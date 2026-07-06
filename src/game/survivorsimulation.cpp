#include "survivorsimulation.h"

#include <iterator>
#include <QtMath>

namespace {

using LanBoard::Survivor::BossSpawnTemplate;
using LanBoard::Survivor::GiantBatEnemy;
using LanBoard::Survivor::GreenGemPickup;
using LanBoard::Survivor::BlueGemPickup;
using LanBoard::Survivor::OgreEnemy;
using LanBoard::Survivor::RedGemPickup;
using LanBoard::Survivor::UpgradeTemplate;
using LanBoard::Survivor::WaveTemplate;

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

}  // namespace

namespace LanBoard::Survivor {

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

bool circlesOverlap(const QVector2D &lhs, const QVector2D &rhs, qreal combinedRadius)
{
    return (lhs - rhs).lengthSquared() <= combinedRadius * combinedRadius;
}

qreal lerpReal(qreal from, qreal to, qreal alpha)
{
    return from + (to - from) * alpha;
}

int lerpInt(int from, int to, qreal alpha)
{
    return qRound(lerpReal(from, to, alpha));
}

void initializeMatchState(MatchState &state)
{
    state = {};
    initializeWorldRuntimeState(state.worldRuntime);
}

void initializeWorldRuntimeState(WorldRuntimeState &state)
{
    state.projectileSpeed = 1.00f;
    state.orbitBladeRadius = 0.14f;
    state.orbitBladeAngularSpeedDeg = 140.0f;
    state.orbitAngleDeg = 0.0f;
    state.garlicRadius = 0.10f;
    state.crossSpeed = 0.82f;
    state.crossRadius = 0.018f;
    state.santaWaterRadius = 0.08f;
}

void initializePlayerProgression(PlayerState &player)
{
    player.hp = 100;
    player.maxHp = 100;
    player.contactDamageCarry = 0.0f;
    player.alive = true;
    player.level = 1;
    player.exp = 0;
    player.expToNext = expRequirementForLevel(player.level);
    player.pendingLevelUps = 0;
    player.attackCooldownMs = 0;
    player.attackCooldownBaseMs = 1000;
    player.orbitBladeCooldownMs = 0;
    player.orbitBladeCooldownBaseMs = 3200;
    player.orbitBladeActiveMs = 0;
    player.orbitBladeDurationMs = 3100;
    player.fireWandCooldownMs = 0;
    player.fireWandCooldownBaseMs = 1720;
    player.garlicCooldownBaseMs = 1300;
    player.crossCooldownMs = 0;
    player.crossCooldownBaseMs = 1080;
    player.santaWaterCooldownMs = 0;
    player.santaWaterCooldownBaseMs = 1800;
    player.attackDamage = 7;
    player.bladeWeaponLevel = 0;
    player.projectileCount = 1;
    player.projectilePierce = 1;
    player.orbitBladeLevel = 0;
    player.orbitBladeCount = 0;
    player.orbitBladeDamage = 0;
    player.fireWandLevel = 0;
    player.fireWandDamage = 24;
    player.fireWandProjectileSpeedMultiplier = 1.0f;
    player.garlicLevel = 1;
    player.garlicDamage = 4;
    player.crossLevel = 0;
    player.crossDamage = 12;
    player.crossAmount = 1;
    player.crossPierce = 1000;
    player.santaWaterLevel = 0;
    player.santaWaterDamage = 10;
    player.santaWaterAmount = 1;
    player.santaWaterDurationMs = 1800;
    player.wingsPassiveLevel = 0;
    player.emptyTomePassiveLevel = 0;
    player.candelabradorPassiveLevel = 0;
    player.attractorbPassiveLevel = 0;
    player.hollowHeartPassiveLevel = 0;
    player.spinachPassiveLevel = 0;
    player.fireWandEvolved = false;
    player.santaWaterEvolved = false;
    player.levelUpChoices.clear();
    player.chestRewardEntries.clear();
    player.queuedChests.clear();
    player.chestTitle.clear();
}

const UpgradeTemplate *weaponUpgradePool()
{
    return kWeaponUpgradePool;
}

int weaponUpgradePoolCount()
{
    return static_cast<int>(std::size(kWeaponUpgradePool));
}

const UpgradeTemplate *passiveUpgradePool()
{
    return kPassiveUpgradePool;
}

int passiveUpgradePoolCount()
{
    return static_cast<int>(std::size(kPassiveUpgradePool));
}

const WaveTemplate *waveTemplates()
{
    return kWaveTemplates;
}

int waveTemplateCount()
{
    return static_cast<int>(std::size(kWaveTemplates));
}

const BossSpawnTemplate *bossSpawnSchedule()
{
    return kBossSpawnSchedule;
}

int bossSpawnScheduleCount()
{
    return static_cast<int>(std::size(kBossSpawnSchedule));
}

}  // namespace LanBoard::Survivor
