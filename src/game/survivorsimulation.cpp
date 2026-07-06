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

// ---- 武器升级数据表 ----
// 每武器 8 级，index 0 = level 1。字段含义：
// damage/cooldownMs/count/pierce/durationMs = level 1 为绝对值，>1 为增量
// radiusMult/speedMult/angularSpeedDeg = 绝对值
namespace {

using LanBoard::Survivor::WeaponLevelInfo;
using LanBoard::Survivor::WeaponCount;

constexpr int kMaxWeaponLevel = 8;

const WeaponLevelInfo kKnifeLevels[8] = {
    {0}, // level 1 description unused; the data struct stat fields are placeholder
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀伤害 +5，数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀数量 +1，冷却缩短。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀穿透 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀数量 +1，冷却缩短。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀伤害 +5，数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "飞刀穿透 +1，冷却缩短。"},
};

const WeaponLevelInfo kOrbitLevels[8] = {
    {0, 0, 0, 0, 0, 0, 0, 0, "解锁秘典，按持续时间召唤 1 枚环刃。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃半径扩大，转速提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃持续时间延长。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃伤害提升，数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃半径扩大，转速提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃持续时间再次延长。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "环刃伤害提升，数量 +1。"},
};

const WeaponLevelInfo kFireWandLevels[8] = {
    {0, 0, 0, 0, 0, 0, 0, 0, "解锁火焰魔杖，随机索敌发射高伤火弹。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖伤害 +12。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖弹速提升，冷却缩短。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖伤害 +12。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖弹速再次提升，冷却继续缩短。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖伤害 +12。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖弹速提升到更高档，冷却再次缩短。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "火焰魔杖伤害 +12。"},
};

const WeaponLevelInfo kGarlicLevels[8] = {
    {0, 0, 0, 0, 0, 0, 0, 0, "解锁大蒜，持续灼伤并轻微击退近身敌人。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜范围扩大，伤害 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜伤害 +1，触发更快。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜范围继续扩大。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜伤害 +2。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜范围扩大，触发更快。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜伤害 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "大蒜范围再次扩大。"},
};

const WeaponLevelInfo kCrossLevels[8] = {
    {0, 0, 0, 0, 0, 0, 0, 0, "解锁十字架，命中后回旋返程。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架伤害提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架体积扩大，飞行速度提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架伤害再次提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架体积扩大，飞行速度提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架数量 +1。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "十字架伤害再次提升。"},
};

const WeaponLevelInfo kSantaWaterLevels[8] = {
    {0, 0, 0, 0, 0, 0, 0, 0, "解锁圣水，在附近生成持续伤害水池。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水数量 +1，水池半径小幅扩大。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水持续时间延长，伤害提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水数量 +1，水池半径扩大。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水持续时间延长，伤害提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水数量 +1，水池半径扩大。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水持续时间延长，伤害提升。"},
    {0, 0, 0, 0, 0, 0, 0, 0, "圣水半径小幅扩大，伤害提升。"},
};

const WeaponLevelInfo *kWeaponTables[WeaponCount] = {
    kKnifeLevels, kOrbitLevels, kFireWandLevels,
    kGarlicLevels, kCrossLevels, kSantaWaterLevels
};

}  // namespace

namespace LanBoard::Survivor {

const WeaponLevelInfo *weaponLevelTable(WeaponType type)
{
    if (type < 0 || type >= WeaponCount)
        return nullptr;
    return kWeaponTables[type];
}

int weaponIndexForId(const QString &upgradeId)
{
    for (int i = 0; i < WeaponCount; ++i) {
        if (upgradeId == QString::fromLatin1(weaponUpgradePool()[i].id))
            return i;
    }
    return -1;
}

int passiveIndexForId(const QString &upgradeId)
{
    for (int i = 0; i < PassiveCount; ++i) {
        if (upgradeId == QString::fromLatin1(passiveUpgradePool()[i].id))
            return i;
    }
    return -1;
}

}  // namespace LanBoard::Survivor

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