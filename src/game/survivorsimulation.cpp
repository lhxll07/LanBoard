#include "survivorsimulation.h"

#include <iterator>
#include <QtMath>

namespace {

using LanBoard::Survivor::BossSpawnTemplate;
using LanBoard::Survivor::GiantBatEnemy;
using LanBoard::Survivor::GreenGemPickup;
using LanBoard::Survivor::BlueGemPickup;
using LanBoard::Survivor::FlowerEnemy;
using LanBoard::Survivor::OgreEnemy;
using LanBoard::Survivor::WerewolfEnemy;
using LanBoard::Survivor::PassiveLevelInfo;
using LanBoard::Survivor::RedGemPickup;
using LanBoard::Survivor::SpawnWeight;
using LanBoard::Survivor::WeaponHitProfile;
using LanBoard::Survivor::EvolutionTemplate;
using LanBoard::Survivor::UpgradeTemplate;
using LanBoard::Survivor::WaveEventBatSwarm;
using LanBoard::Survivor::WaveEventFlowerWall;
using LanBoard::Survivor::WaveEventTemplate;
using LanBoard::Survivor::WaveTemplate;

const UpgradeTemplate kWeaponUpgradePool[] = {
    {"knife_weapon", "飞刀", "武器", 8},
    {"orbit_weapon", "秘典", "武器", 8},
    {"firewand_weapon", "火焰魔杖", "武器", 8},
    {"magicwand_weapon", "魔杖", "武器", 8},
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
    {"spinach_passive", "菠菜", "被动", 5},
    {"bracer_passive", "护腕", "被动", 5},
    {"spellbinder_passive", "咒缚", "被动", 5},
    {"pummarola_passive", "番茄", "被动", 5},
    {"clover_passive", "四叶草", "被动", 5}
};

const EvolutionTemplate kEvolutionTemplates[] = {
    {"knife_weapon", "bracer_passive", "千刃",
     "飞刀进化完成，投掷间隔大幅缩短，形成近乎不断流的刀幕。", 8, 1},
    {"orbit_weapon", "spellbinder_passive", "邪恶晚祷",
     "秘典进化完成，环刃进入常驻轮转，几乎不会出现真空期。", 8, 1},
    {"firewand_weapon", "spinach_passive", "地狱火",
     "火杖进化完成，火球穿透全部敌人并造成更高爆发伤害。", 8, 1},
    {"magicwand_weapon", "emptytome_passive", "圣魔杖",
     "魔杖进化完成，持续朝最近敌人喷射高速魔弹。", 8, 1},
    {"garlic_weapon", "pummarola_passive", "噬魂者",
     "大蒜进化完成，光环会随治疗逐步增伤，并有概率在击杀时回心。", 8, 1},
    {"cross_weapon", "clover_passive", "天堂之剑",
     "十字架进化完成，飞得更远更快，并会按幸运打出高额暴击。", 8, 1},
    {"santawater_weapon", "attractorb_passive", "黑波拉",
     "圣水进化完成，生成会向玩家汇聚的持续伤害圣池。", 8, 1}
};

constexpr SpawnWeight kWave0Spawns[] = {
    {LanBoard::Survivor::BatEnemy, 84},
    {LanBoard::Survivor::ZombieEnemy, 16}
};
constexpr SpawnWeight kWave1Spawns[] = {
    {LanBoard::Survivor::ZombieEnemy, 74},
    {LanBoard::Survivor::BatEnemy, 26}
};
constexpr SpawnWeight kWave2Spawns[] = {
    {LanBoard::Survivor::BatEnemy, 58},
    {LanBoard::Survivor::ZombieEnemy, 28},
    {LanBoard::Survivor::SkeletonEnemy, 14}
};
constexpr SpawnWeight kWave3Spawns[] = {
    {LanBoard::Survivor::SkeletonEnemy, 56},
    {LanBoard::Survivor::BatEnemy, 24},
    {LanBoard::Survivor::ZombieEnemy, 20}
};
constexpr SpawnWeight kWave4Spawns[] = {
    {LanBoard::Survivor::WerewolfEnemy, 46},
    {LanBoard::Survivor::SkeletonEnemy, 34},
    {LanBoard::Survivor::FlowerEnemy, 20}
};
constexpr SpawnWeight kWave5Spawns[] = {
    {LanBoard::Survivor::BatEnemy, 46},
    {LanBoard::Survivor::ZombieEnemy, 32},
    {LanBoard::Survivor::SkeletonEnemy, 22}
};
constexpr SpawnWeight kWave6Spawns[] = {
    {LanBoard::Survivor::BatEnemy, 38},
    {LanBoard::Survivor::ZombieEnemy, 34},
    {LanBoard::Survivor::SkeletonEnemy, 28}
};
constexpr SpawnWeight kWave7Spawns[] = {
    {LanBoard::Survivor::WerewolfEnemy, 40},
    {LanBoard::Survivor::FlowerEnemy, 30},
    {LanBoard::Survivor::OgreEnemy, 18},
    {LanBoard::Survivor::SkeletonEnemy, 12}
};
constexpr SpawnWeight kWave8Spawns[] = {
    {LanBoard::Survivor::OgreEnemy, 42},
    {LanBoard::Survivor::WerewolfEnemy, 34},
    {LanBoard::Survivor::GiantBatEnemy, 24}
};
constexpr SpawnWeight kWave9Spawns[] = {
    {LanBoard::Survivor::GiantBatEnemy, 40},
    {LanBoard::Survivor::OgreEnemy, 34},
    {LanBoard::Survivor::BatEnemy, 26}
};
constexpr SpawnWeight kWave10Spawns[] = {
    {LanBoard::Survivor::OgreEnemy, 36},
    {LanBoard::Survivor::SkeletonEnemy, 30},
    {LanBoard::Survivor::FlowerEnemy, 20},
    {LanBoard::Survivor::WerewolfEnemy, 14}
};
constexpr SpawnWeight kWave11Spawns[] = {
    {LanBoard::Survivor::SkeletonEnemy, 34},
    {LanBoard::Survivor::WerewolfEnemy, 28},
    {LanBoard::Survivor::OgreEnemy, 22},
    {LanBoard::Survivor::FlowerEnemy, 16}
};
constexpr SpawnWeight kWave12Spawns[] = {
    {LanBoard::Survivor::WerewolfEnemy, 36},
    {LanBoard::Survivor::FlowerEnemy, 26},
    {LanBoard::Survivor::SkeletonEnemy, 22},
    {LanBoard::Survivor::GiantBatEnemy, 16}
};
constexpr SpawnWeight kWave13Spawns[] = {
    {LanBoard::Survivor::GiantBatEnemy, 30},
    {LanBoard::Survivor::WerewolfEnemy, 28},
    {LanBoard::Survivor::OgreEnemy, 24},
    {LanBoard::Survivor::FlowerEnemy, 18}
};
constexpr SpawnWeight kWave14Spawns[] = {
    {LanBoard::Survivor::GiantBatEnemy, 34},
    {LanBoard::Survivor::OgreEnemy, 28},
    {LanBoard::Survivor::WerewolfEnemy, 22},
    {LanBoard::Survivor::SkeletonEnemy, 16}
};

constexpr SpawnWeight kWave3Elites[] = {
    {LanBoard::Survivor::WerewolfEnemy, 72},
    {LanBoard::Survivor::FlowerEnemy, 28}
};
constexpr SpawnWeight kWave4Elites[] = {
    {LanBoard::Survivor::WerewolfEnemy, 58},
    {LanBoard::Survivor::FlowerEnemy, 42}
};
constexpr SpawnWeight kWave5Elites[] = {
    {LanBoard::Survivor::FlowerEnemy, 56},
    {LanBoard::Survivor::WerewolfEnemy, 44}
};
constexpr SpawnWeight kWave6Elites[] = {
    {LanBoard::Survivor::WerewolfEnemy, 54},
    {LanBoard::Survivor::FlowerEnemy, 46}
};
constexpr SpawnWeight kWave7Elites[] = {
    {LanBoard::Survivor::OgreEnemy, 58},
    {LanBoard::Survivor::FlowerEnemy, 42}
};
constexpr SpawnWeight kWave8Elites[] = {
    {LanBoard::Survivor::GiantBatEnemy, 56},
    {LanBoard::Survivor::OgreEnemy, 44}
};
constexpr SpawnWeight kWave9Elites[] = {
    {LanBoard::Survivor::GiantBatEnemy, 52},
    {LanBoard::Survivor::OgreEnemy, 48}
};
constexpr SpawnWeight kWave10Elites[] = {
    {LanBoard::Survivor::OgreEnemy, 56},
    {LanBoard::Survivor::FlowerEnemy, 44}
};
constexpr SpawnWeight kWave11Elites[] = {
    {LanBoard::Survivor::OgreEnemy, 54},
    {LanBoard::Survivor::WerewolfEnemy, 46}
};
constexpr SpawnWeight kWave12Elites[] = {
    {LanBoard::Survivor::GiantBatEnemy, 46},
    {LanBoard::Survivor::WerewolfEnemy, 34},
    {LanBoard::Survivor::OgreEnemy, 20}
};
constexpr SpawnWeight kWave13Elites[] = {
    {LanBoard::Survivor::GiantBatEnemy, 46},
    {LanBoard::Survivor::OgreEnemy, 32},
    {LanBoard::Survivor::FlowerEnemy, 22}
};
constexpr SpawnWeight kWave14Elites[] = {
    {LanBoard::Survivor::GiantBatEnemy, 42},
    {LanBoard::Survivor::OgreEnemy, 38},
    {LanBoard::Survivor::WerewolfEnemy, 20}
};

const WaveTemplate kWaveTemplates[] = {
    {"夜翼与尸行", 980, 1, 0, 0, 24, kWave0Spawns, static_cast<int>(std::size(kWave0Spawns)), nullptr, 0},
    {"尸群逼近", 860, 1, 0, 0, 34, kWave1Spawns, static_cast<int>(std::size(kWave1Spawns)), nullptr, 0},
    {"骨潮试探", 760, 1, 0, 0, 44, kWave2Spawns, static_cast<int>(std::size(kWave2Spawns)), nullptr, 0},
    {"骨花前奏", 680, 2, 0, 0, 56, kWave3Spawns, static_cast<int>(std::size(kWave3Spawns)), nullptr, 0},
    {"狼藤试探", 600, 2, 0, 0, 68, kWave4Spawns, static_cast<int>(std::size(kWave4Spawns)), nullptr, 0},
    {"猎犬骸潮", 520, 2, 26000, 1, 88, kWave5Spawns, static_cast<int>(std::size(kWave5Spawns)), kWave3Elites, static_cast<int>(std::size(kWave3Elites))},
    {"幽翼盘旋", 470, 3, 20000, 1, 108, kWave6Spawns, static_cast<int>(std::size(kWave6Spawns)), kWave4Elites, static_cast<int>(std::size(kWave4Elites))},
    {"鬼花开场", 420, 3, 16500, 1, 130, kWave7Spawns, static_cast<int>(std::size(kWave7Spawns)), kWave5Elites, static_cast<int>(std::size(kWave5Elites))},
    {"进化节点", 370, 4, 14000, 2, 154, kWave8Spawns, static_cast<int>(std::size(kWave8Spawns)), kWave6Elites, static_cast<int>(std::size(kWave6Elites))},
    {"骨环收束", 330, 4, 11800, 2, 180, kWave9Spawns, static_cast<int>(std::size(kWave9Spawns)), kWave7Elites, static_cast<int>(std::size(kWave7Elites))},
    {"狼鬼混编", 285, 5, 9800, 2, 212, kWave10Spawns, static_cast<int>(std::size(kWave10Spawns)), kWave8Elites, static_cast<int>(std::size(kWave8Elites))},
    {"巨压过渡", 245, 5, 8400, 2, 246, kWave11Spawns, static_cast<int>(std::size(kWave11Spawns)), kWave9Elites, static_cast<int>(std::size(kWave9Elites))},
    {"花墙与棺群", 210, 6, 7200, 3, 286, kWave12Spawns, static_cast<int>(std::size(kWave12Spawns)), kWave12Elites, static_cast<int>(std::size(kWave12Elites))},
    {"墓潮回涌", 180, 7, 6000, 3, 336, kWave13Spawns, static_cast<int>(std::size(kWave13Spawns)), kWave13Elites, static_cast<int>(std::size(kWave13Elites))},
    {"血月终曲", 150, 8, 4800, 4, 392, kWave14Spawns, static_cast<int>(std::size(kWave14Spawns)), kWave14Elites, static_cast<int>(std::size(kWave14Elites))}
};

const BossSpawnTemplate kBossSpawnSchedule[] = {
    {150, WerewolfEnemy},
    {240, GiantBatEnemy},
    {330, OgreEnemy},
    {420, GiantBatEnemy},
    {510, OgreEnemy},
    {600, FlowerEnemy},
    {690, OgreEnemy},
    {780, FlowerEnemy},
    {870, GiantBatEnemy}
};

const WaveEventTemplate kWaveEventSchedule[] = {
    {300, WaveEventBatSwarm, 32, 1.16f, 0.0f, 0},
    {420, WaveEventFlowerWall, 24, 1.04f, 0.062f, 108},
    {540, WaveEventBatSwarm, 56, 1.30f, 0.0f, 0},
    {660, WaveEventFlowerWall, 32, 1.12f, 0.074f, 148},
    {780, WaveEventBatSwarm, 76, 1.40f, 0.0f, 0},
    {840, WaveEventFlowerWall, 38, 1.20f, 0.086f, 196}
};

}  // namespace

// ---- 武器升级数据表 ----
// 每武器 8 级，index 0 = level 1。字段含义：
// damage/cooldownMs/count/pierce/durationMs = level 1 为绝对值，>1 为增量
// radius/speed/angularSpeedDeg = 绝对值
namespace {

using LanBoard::Survivor::WeaponLevelInfo;
using LanBoard::Survivor::WeaponCount;

constexpr int kMaxWeaponLevel = 8;

const WeaponLevelInfo kKnifeLevels[8] = {
    {9, 960, 1, 1, 0, 0.0f, 1.0f, 0.0f, "解锁飞刀，按移动朝向投掷 1 枚飞刀。"},
    {3, 900, 1, 0, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +3，数量 +1，投掷间隔更紧。"},
    {4, 860, 1, 0, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +4，数量 +1。"},
    {3, 820, 1, 0, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +3，数量 +1，投掷间隔更紧。"},
    {5, 820, 0, 1, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +5，穿透 +1。"},
    {4, 760, 1, 0, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +4，数量 +1，投掷间隔更紧。"},
    {6, 720, 1, 0, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +6，数量 +1。"},
    {6, 680, 0, 1, 0, 0.0f, 0.0f, 0.0f, "飞刀伤害 +6，穿透 +1，投掷间隔更紧。"},
};

const WeaponLevelInfo kOrbitLevels[8] = {
    {8, 3200, 1, 0, 3100, 0.14f, 0.0f, 140.0f, "解锁秘典，按持续时间召唤 1 枚环刃。"},
    {0, 0, 1, 0, 0, 0.0f, 0.0f, 0.0f, "环刃数量 +1。"},
    {0, 0, 0, 0, 0, 0.16f, 0.0f, 180.0f, "环刃半径扩大，转速提升。"},
    {0, 0, 0, 0, 500, 0.0f, 0.0f, 0.0f, "环刃持续时间延长。"},
    {4, 0, 1, 0, 0, 0.0f, 0.0f, 0.0f, "环刃伤害提升，数量 +1。"},
    {0, 0, 0, 0, 0, 0.18f, 0.0f, 210.0f, "环刃半径扩大，转速提升。"},
    {0, 0, 0, 0, 500, 0.0f, 0.0f, 0.0f, "环刃持续时间再次延长。"},
    {4, 0, 1, 0, 0, 0.0f, 0.0f, 0.0f, "环刃伤害提升，数量 +1。"},
};

const WeaponLevelInfo kFireWandLevels[8] = {
    {24, 2800, 3, 0, 0, 0.0f, 0.78f, 0.0f, "解锁火焰魔杖，自动朝最近敌人发射 3 枚火弹。"},
    {8, 2550, 0, 0, 0, 0.0f, 0.82f, 0.0f, "火焰魔杖伤害 +8，冷却缩短。"},
    {10, 2350, 1, 0, 0, 0.0f, 0.92f, 0.0f, "火焰魔杖伤害 +10，数量 +1。"},
    {10, 2150, 0, 0, 0, 0.0f, 1.02f, 0.0f, "火焰魔杖伤害 +10，冷却缩短。"},
    {12, 1950, 1, 0, 0, 0.0f, 1.12f, 0.0f, "火焰魔杖伤害 +12，数量 +1。"},
    {12, 1800, 0, 0, 0, 0.0f, 1.18f, 0.0f, "火焰魔杖伤害 +12，冷却缩短。"},
    {14, 1650, 1, 0, 0, 0.0f, 1.26f, 0.0f, "火焰魔杖伤害 +14，数量 +1。"},
    {16, 1500, 0, 0, 0, 0.0f, 1.34f, 0.0f, "火焰魔杖伤害 +16，冷却进一步缩短。"},
};

const WeaponLevelInfo kMagicWandLevels[8] = {
    {10, 1200, 1, 0, 0, 0.0f, 1.10f, 0.0f, "解锁魔杖，自动朝最近敌人发射追敌魔弹。"},
    {2, 1100, 0, 0, 0, 0.0f, 1.12f, 0.0f, "魔杖伤害提升，冷却略缩短。"},
    {0, 1020, 1, 0, 0, 0.0f, 1.15f, 0.0f, "魔杖数量 +1。"},
    {3, 980, 0, 0, 0, 0.0f, 1.18f, 0.0f, "魔杖伤害提升。"},
    {0, 920, 1, 0, 0, 0.0f, 1.22f, 0.0f, "魔杖数量 +1，冷却缩短。"},
    {3, 860, 0, 0, 0, 0.0f, 1.28f, 0.0f, "魔杖伤害提升，弹速提高。"},
    {0, 820, 1, 0, 0, 0.0f, 1.34f, 0.0f, "魔杖数量 +1。"},
    {4, 760, 0, 0, 0, 0.0f, 1.40f, 0.0f, "魔杖伤害提升，冷却缩短。"},
};

const WeaponLevelInfo kGarlicLevels[8] = {
    {5, 1300, 0, 0, 0, 0.10f, 0.0f, 0.0f, "解锁大蒜，持续灼伤并轻微击退近身敌人。"},
    {2, 1300, 0, 0, 0, 0.12f, 0.0f, 0.0f, "大蒜伤害提升，范围扩大。"},
    {0, 1100, 0, 0, 0, 0.12f, 0.0f, 0.0f, "大蒜触发更快。"},
    {0, 1100, 0, 0, 0, 0.14f, 0.0f, 0.0f, "大蒜范围继续扩大。"},
    {2, 1100, 0, 0, 0, 0.14f, 0.0f, 0.0f, "大蒜伤害提升。"},
    {0, 920, 0, 0, 0, 0.16f, 0.0f, 0.0f, "大蒜范围扩大，触发更快。"},
    {1, 920, 0, 0, 0, 0.16f, 0.0f, 0.0f, "大蒜伤害提升。"},
    {0, 920, 0, 0, 0, 0.18f, 0.0f, 0.0f, "大蒜范围再次扩大。"},
};

const WeaponLevelInfo kCrossLevels[8] = {
    {9, 1800, 1, 0, 0, 0.020f, 0.90f, 0.0f, "解锁十字架，命中后回旋返程。"},
    {8, 1700, 0, 0, 0, 0.020f, 0.96f, 0.0f, "十字架伤害提升，冷却缩短。"},
    {4, 1600, 0, 0, 0, 0.022f, 1.08f, 0.0f, "十字架伤害提升，体积扩大，飞行速度提升。"},
    {6, 1500, 1, 0, 0, 0.022f, 1.12f, 0.0f, "十字架伤害提升，数量 +1。"},
    {10, 1450, 0, 0, 0, 0.022f, 1.18f, 0.0f, "十字架伤害再次提升。"},
    {6, 1350, 0, 0, 0, 0.024f, 1.30f, 0.0f, "十字架伤害提升，体积扩大，飞行速度提升。"},
    {8, 1250, 1, 0, 0, 0.024f, 1.34f, 0.0f, "十字架伤害提升，数量 +1。"},
    {12, 1150, 0, 0, 0, 0.024f, 1.40f, 0.0f, "十字架伤害大幅提升，冷却缩短。"},
};

const WeaponLevelInfo kSantaWaterLevels[8] = {
    {10, 4500, 1, 0, 2000, 0.08f, 0.0f, 0.0f, "解锁圣水，在最近敌人周围投下持续伤害圣池。"},
    {0, 4500, 1, 0, 0, 0.08f, 0.0f, 0.0f, "圣水数量 +1。"},
    {10, 4500, 0, 0, 500, 0.08f, 0.0f, 0.0f, "圣水伤害提升，持续时间延长。"},
    {0, 4500, 1, 0, 0, 0.10f, 0.0f, 0.0f, "圣水数量 +1，半径扩大。"},
    {10, 4500, 0, 0, 500, 0.10f, 0.0f, 0.0f, "圣水伤害提升，持续时间延长。"},
    {0, 4500, 1, 0, 0, 0.112f, 0.0f, 0.0f, "圣水数量 +1，半径扩大。"},
    {10, 4500, 0, 0, 500, 0.112f, 0.0f, 0.0f, "圣水伤害提升，持续时间延长。"},
    {10, 4500, 0, 0, 0, 0.124f, 0.0f, 0.0f, "圣水伤害提升，半径扩大。"},
};

const WeaponLevelInfo *kWeaponTables[WeaponCount] = {
    kKnifeLevels, kOrbitLevels, kFireWandLevels, kMagicWandLevels,
    kGarlicLevels, kCrossLevels, kSantaWaterLevels
};

const PassiveLevelInfo kWingsLevels[5] = {
    {"移速 +10%。"},
    {"移速 +10%。"},
    {"移速 +10%。"},
    {"移速 +10%。"},
    {"移速 +10%。"},
};

const PassiveLevelInfo kEmptyTomeLevels[5] = {
    {"冷却 -8%。"},
    {"冷却 -8%。"},
    {"冷却 -8%。"},
    {"冷却 -8%。"},
    {"冷却 -8%。"},
};

const PassiveLevelInfo kCandelabradorLevels[5] = {
    {"范围 +10%。"},
    {"范围 +10%。"},
    {"范围 +10%。"},
    {"范围 +10%。"},
    {"范围 +10%。"},
};

const PassiveLevelInfo kAttractorbLevels[5] = {
    {"拾取范围提升到 1.50x。"},
    {"拾取范围提升到 2.00x。"},
    {"拾取范围提升到 2.49x。"},
    {"拾取范围提升到 2.99x。"},
    {"拾取范围提升到 3.98x。"},
};

const PassiveLevelInfo kHollowHeartLevels[5] = {
    {"最大生命乘算提升到 1.20x。"},
    {"最大生命乘算提升到 1.44x。"},
    {"最大生命乘算提升到 1.73x。"},
    {"最大生命乘算提升到 2.07x。"},
    {"最大生命乘算提升到 2.49x。"},
};

const PassiveLevelInfo kSpinachLevels[5] = {
    {"伤害 +10%。"},
    {"伤害 +10%。"},
    {"伤害 +10%。"},
    {"伤害 +10%。"},
    {"伤害 +10%。"},
};

const PassiveLevelInfo kBracerLevels[5] = {
    {"投射物速度 +10%。"},
    {"投射物速度 +10%。"},
    {"投射物速度 +10%。"},
    {"投射物速度 +10%。"},
    {"投射物速度 +10%。"},
};

const PassiveLevelInfo kSpellbinderLevels[5] = {
    {"持续时间 +10%。"},
    {"持续时间 +10%。"},
    {"持续时间 +10%。"},
    {"持续时间 +10%。"},
    {"持续时间 +10%。"},
};

const PassiveLevelInfo kPummarolaLevels[5] = {
    {"每秒恢复 0.2 HP。"},
    {"每秒恢复 0.2 HP。"},
    {"每秒恢复 0.2 HP。"},
    {"每秒恢复 0.2 HP。"},
    {"每秒恢复 0.2 HP。"},
};

const PassiveLevelInfo kCloverLevels[5] = {
    {"幸运 +10%。"},
    {"幸运 +10%。"},
    {"幸运 +10%。"},
    {"幸运 +10%。"},
    {"幸运 +10%。"},
};

const PassiveLevelInfo *kPassiveTables[LanBoard::Survivor::PassiveCount] = {
    kWingsLevels, kEmptyTomeLevels, kCandelabradorLevels,
    kAttractorbLevels, kHollowHeartLevels, kSpinachLevels,
    kBracerLevels, kSpellbinderLevels, kPummarolaLevels, kCloverLevels
};

const WeaponHitProfile kAttackProfiles[LanBoard::Survivor::AttackProfileCount] = {
    {0.012f, 1.0f, 0.040f, 0.18f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1680, 0, 1},
    {0.017f, 1.0f, 0.048f, 0.16f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 2600, 0, 1},
    {0.021f, 1.24f, 0.072f, 0.10f, 1.0f, 1.40f, 1.0f, 1.0f, 0.0f, 4200, 0, 999},
    {0.014f, 1.0f, 0.020f, 0.08f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 2600, 0, 1},
    {0.016f, 1.22f, 0.024f, 0.06f, 1.0f, 1.20f, 1.0f, 1.0f, 0.0f, 3200, 0, 2},
    {0.018f, 1.0f, 0.060f, 0.12f, 1.0f, 1.0f, 1.0f, 1.05f, 0.0f, 1620, 0, 1000},
    {0.0f, 1.0f, 0.018f, 0.10f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0, 500, 1},
    {0.0f, 1.0f, 0.026f, 0.06f, 1.12f, 1.30f, 1.20f, 1.0f, 0.16f, 0, 360, 1},
    {0.0f, 1.0f, 0.040f, 0.10f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0, 0, 1},
    {0.0f, 1.0f, 0.028f, 0.08f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0, 1700, 1}
};

}  // namespace

namespace LanBoard::Survivor {

const WeaponLevelInfo *weaponLevelTable(WeaponType type)
{
    if (type < 0 || type >= WeaponCount)
        return nullptr;
    return kWeaponTables[type];
}

const PassiveLevelInfo *passiveLevelTable(PassiveType type)
{
    if (type < 0 || type >= PassiveCount)
        return nullptr;
    return kPassiveTables[type];
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

bool isWeaponUpgradeId(const QString &upgradeId)
{
    return weaponIndexForId(upgradeId) >= 0;
}

const UpgradeTemplate *upgradeTemplateForId(const QString &upgradeId)
{
    for (const UpgradeTemplate &entry : kWeaponUpgradePool) {
        if (upgradeId == QString::fromLatin1(entry.id))
            return &entry;
    }
    for (const UpgradeTemplate &entry : kPassiveUpgradePool) {
        if (upgradeId == QString::fromLatin1(entry.id))
            return &entry;
    }
    return nullptr;
}

const EvolutionTemplate *evolutionTemplateForWeaponId(const QString &weaponId)
{
    for (const EvolutionTemplate &entry : kEvolutionTemplates) {
        if (weaponId == QString::fromLatin1(entry.weaponId))
            return &entry;
    }
    return nullptr;
}

const WeaponHitProfile &weaponHitProfile(AttackProfileType type)
{
    return kAttackProfiles[qBound(0, static_cast<int>(type), static_cast<int>(AttackProfileCount - 1))];
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
    state.projectileSpeed = weaponLevelTable(WeaponKnife)[0].speed;
    state.orbitBladeRadius = weaponLevelTable(WeaponOrbitBlade)[0].radius;
    state.orbitBladeAngularSpeedDeg = weaponLevelTable(WeaponOrbitBlade)[0].angularSpeedDeg;
    state.orbitAngleDeg = 0.0f;
    state.garlicRadius = weaponLevelTable(WeaponGarlic)[0].radius;
    state.crossSpeed = weaponLevelTable(WeaponCross)[0].speed;
    state.crossRadius = weaponLevelTable(WeaponCross)[0].radius;
    state.santaWaterRadius = weaponLevelTable(WeaponSantaWater)[0].radius;
}

void initializePlayerProgression(PlayerState &player)
{
    player.hp = BasePlayerMaxHp;
    player.maxHp = BasePlayerMaxHp;
    player.contactDamageCarry = 0.0f;
    player.recoveryCarry = 0.0f;
    player.soulEaterHealedHp = 0;
    player.soulEaterBonusDamage = 0;
    player.alive = true;
    player.level = 1;
    player.exp = 0;
    player.expToNext = expRequirementForLevel(player.level);
    player.pendingLevelUps = 0;
    player.attackCooldownMs = 0;
    player.attackCooldownBaseMs = weaponLevelTable(WeaponKnife)[0].cooldownMs;
    player.orbitBladeCooldownMs = 0;
    player.orbitBladeCooldownBaseMs = weaponLevelTable(WeaponOrbitBlade)[0].cooldownMs;
    player.orbitBladeActiveMs = 0;
    player.orbitBladeDurationMs = weaponLevelTable(WeaponOrbitBlade)[0].durationMs;
    player.fireWandCooldownMs = 0;
    player.fireWandCooldownBaseMs = weaponLevelTable(WeaponFireWand)[0].cooldownMs;
    player.magicWandCooldownMs = 0;
    player.magicWandCooldownBaseMs = weaponLevelTable(WeaponMagicWand)[0].cooldownMs;
    player.garlicCooldownBaseMs = weaponLevelTable(WeaponGarlic)[0].cooldownMs;
    player.crossCooldownMs = 0;
    player.crossCooldownBaseMs = weaponLevelTable(WeaponCross)[0].cooldownMs;
    player.santaWaterCooldownMs = 0;
    player.santaWaterCooldownBaseMs = weaponLevelTable(WeaponSantaWater)[0].cooldownMs;
    player.attackDamage = weaponLevelTable(WeaponKnife)[0].damage;
    player.bladeWeaponLevel = 0;
    player.projectileCount = weaponLevelTable(WeaponKnife)[0].count;
    player.projectilePierce = weaponLevelTable(WeaponKnife)[0].pierce;
    player.orbitBladeLevel = 0;
    player.orbitBladeCount = 0;
    player.orbitBladeDamage = weaponLevelTable(WeaponOrbitBlade)[0].damage;
    player.fireWandLevel = 0;
    player.fireWandDamage = weaponLevelTable(WeaponFireWand)[0].damage;
    player.fireWandAmount = weaponLevelTable(WeaponFireWand)[0].count;
    player.fireWandProjectileSpeedMultiplier = weaponLevelTable(WeaponFireWand)[0].speed;
    player.magicWandLevel = 0;
    player.magicWandDamage = weaponLevelTable(WeaponMagicWand)[0].damage;
    player.magicWandAmount = weaponLevelTable(WeaponMagicWand)[0].count;
    player.garlicLevel = 1;
    player.garlicDamage = weaponLevelTable(WeaponGarlic)[0].damage;
    player.crossLevel = 0;
    player.crossDamage = weaponLevelTable(WeaponCross)[0].damage;
    player.crossAmount = weaponLevelTable(WeaponCross)[0].count;
    player.crossPierce = 1000;
    player.santaWaterLevel = 0;
    player.santaWaterDamage = weaponLevelTable(WeaponSantaWater)[0].damage;
    player.santaWaterAmount = weaponLevelTable(WeaponSantaWater)[0].count;
    player.santaWaterDurationMs = weaponLevelTable(WeaponSantaWater)[0].durationMs;
    player.wingsPassiveLevel = 0;
    player.emptyTomePassiveLevel = 0;
    player.candelabradorPassiveLevel = 0;
    player.attractorbPassiveLevel = 0;
    player.hollowHeartPassiveLevel = 0;
    player.spinachPassiveLevel = 0;
    player.bracerPassiveLevel = 0;
    player.spellbinderPassiveLevel = 0;
    player.pummarolaPassiveLevel = 0;
    player.cloverPassiveLevel = 0;
    player.bladeWeaponEvolved = false;
    player.orbitBladeEvolved = false;
    player.fireWandEvolved = false;
    player.magicWandEvolved = false;
    player.garlicEvolved = false;
    player.crossEvolved = false;
    player.santaWaterEvolved = false;
    player.levelUpChoices.clear();
    player.chestRewardEntries.clear();
    player.queuedChests.clear();
    player.chestTitle.clear();
}

qreal damageMultiplierForLevel(int spinachLevel)
{
    return 1.0 + qBound(0, spinachLevel, 5) * 0.10;
}

qreal areaMultiplierForLevel(int candelabradorLevel)
{
    return 1.0 + qBound(0, candelabradorLevel, 5) * 0.10;
}

qreal cooldownMultiplierForLevel(int emptyTomeLevel)
{
    return qMax<qreal>(0.60, 1.0 - qBound(0, emptyTomeLevel, 5) * 0.08);
}

qreal moveSpeedMultiplierForLevel(int wingsLevel)
{
    return 1.0 + qBound(0, wingsLevel, 5) * 0.10;
}

qreal magnetRangeForLevel(int attractorbLevel)
{
    static const qreal kMagnetMultipliers[] = {1.0f, 1.5f, 1.995f, 2.49375f, 2.9925f, 3.980025f};
    return BasePickupMagnetRange * kMagnetMultipliers[qBound(0, attractorbLevel, 5)];
}

int maxHpValueForLevel(int hollowHeartLevel)
{
    qreal hp = BasePlayerMaxHp;
    for (int i = 0; i < qBound(0, hollowHeartLevel, 5); ++i)
        hp *= 1.20;
    return qRound(hp);
}

qreal projectileSpeedMultiplierForLevel(int bracerLevel)
{
    return 1.0 + qBound(0, bracerLevel, 5) * 0.10;
}

qreal durationMultiplierForLevel(int spellbinderLevel)
{
    return 1.0 + qBound(0, spellbinderLevel, 5) * 0.10;
}

qreal recoveryPerSecondForLevel(int pummarolaLevel)
{
    return qBound(0, pummarolaLevel, 5) * 0.2;
}

qreal luckMultiplierForLevel(int cloverLevel)
{
    return 1.0 + qBound(0, cloverLevel, 5) * 0.10;
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

const WaveEventTemplate *waveEventSchedule()
{
    return kWaveEventSchedule;
}

int waveEventScheduleCount()
{
    return static_cast<int>(std::size(kWaveEventSchedule));
}

}  // namespace LanBoard::Survivor
