#include "dormdefensecontroller.h"

#include <QJsonArray>
#include <QRandomGenerator>
#include <QVariantMap>
#include <QtGlobal>
#include <QtMath>

#include <array>
#include <algorithm>
#include <limits>

#include "src/common/types.h"

namespace {

constexpr int PlayerRoomTop = 3;
constexpr int PlayerRoomLeft = 3;
constexpr int DoorRow = PlayerRoomTop + 6;
constexpr int DoorColumn = PlayerRoomLeft + 5;
constexpr int BedRow = PlayerRoomTop + 4;
constexpr int BedColumn = PlayerRoomLeft + 3;
constexpr int GhostSpawnRow = 24;
constexpr int GhostSpawnColumn = 1;
constexpr int RoomBedOffsetRow = 4;
constexpr int RoomBedOffsetColumn = 3;
constexpr int RoomDoorOffsetRow = 6;
constexpr int RoomDoorOffsetColumn = 5;
constexpr qreal PlayerGhostStepPerMove = 0.09;
constexpr qreal GhostHitboxHalfExtent = 0.34;

int doublingCost(int baseCost, int nextLevel)
{
    return baseCost << qMax(0, nextLevel - 2);
}

int tableValue(const std::array<int, 6> &table, int level)
{
    const int index = qBound(1, level, static_cast<int>(table.size())) - 1;
    return table.at(index);
}

bool ghostWithinTurretRange(qreal ghostCenterRow, qreal ghostCenterColumn,
                            int turretRow, int turretColumn, int range)
{
    const qreal rowDistance = qAbs((turretRow + 0.5) - ghostCenterRow);
    const qreal columnDistance = qAbs((turretColumn + 0.5) - ghostCenterColumn);
    const qreal effectiveRowDistance = qMax<qreal>(0.0, rowDistance - GhostHitboxHalfExtent);
    const qreal effectiveColumnDistance = qMax<qreal>(0.0, columnDistance - GhostHitboxHalfExtent);
    return qMax(effectiveRowDistance, effectiveColumnDistance) <= range + 0.5;
}

bool ghostTouchesCell(qreal ghostCenterRow, qreal ghostCenterColumn,
                      int cellRow, int cellColumn)
{
    const qreal ghostLeft = ghostCenterColumn - GhostHitboxHalfExtent;
    const qreal ghostRight = ghostCenterColumn + GhostHitboxHalfExtent;
    const qreal ghostTop = ghostCenterRow - GhostHitboxHalfExtent;
    const qreal ghostBottom = ghostCenterRow + GhostHitboxHalfExtent;
    return ghostLeft <= cellColumn + 1.0
        && ghostRight >= cellColumn
        && ghostTop <= cellRow + 1.0
        && ghostBottom >= cellRow;
}

QVector<QPair<int, int>> roomTemplate()
{
    QVector<QPair<int, int>> tiles;

    for (int row = 0; row <= 2; ++row) {
        for (int column = 0; column <= 5; ++column)
            tiles.append({row, column});
    }

    for (int row = 3; row <= 5; ++row) {
        for (int column = 3; column <= 5; ++column)
            tiles.append({row, column});
    }

    return tiles;
}

}

DormDefenseController::DormDefenseController(QObject *parent)
    : GameControllerBase(parent)
{
    connect(&m_tickTimer, &QTimer::timeout, this, &DormDefenseController::onTick);
    m_tickTimer.setInterval(1000);
    connect(&m_combatTimer, &QTimer::timeout, this, &DormDefenseController::onCombatTick);
    m_combatTimer.setInterval(CombatTickMs);
    connect(&m_roleSelectionTimer, &QTimer::timeout, this, [this]() {
        if (!m_roleSelectionRequired)
            return;

        if (m_roleSelectionCountdown > 0) {
            --m_roleSelectionCountdown;
            emit roleChanged();
            emit statusChanged();
        }

        if (m_roleSelectionCountdown <= 0)
            finalizeRoleSelection();
    });
    m_roleSelectionTimer.setInterval(1000);
    m_ghostNetworkSyncTimer.setSingleShot(true);
    m_ghostNetworkSyncTimer.setInterval(33);
    connect(&m_ghostNetworkSyncTimer, &QTimer::timeout, this, [this]() {
        if (!m_networked
            || isAuthoritativeInstance()
            || !localControlsGhost()
            || m_gameOver
            || m_roleSelectionRequired
            || m_timeRemaining > 0) {
            return;
        }

        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("move_ghost")},
            {QStringLiteral("row"), m_ghostCenterRow},
            {QStringLiteral("column"), m_ghostCenterColumn}
        });
    });
    reset();
}

void DormDefenseController::startNewGame()
{
    if (m_networked && !isAuthoritativeInstance()) {
        reset();
        return;
    }

    m_roleSelectionRequired = true;
    m_roleSelectionCountdown = RoleSelectionDuration;
    m_pendingRoleSelections.clear();
    m_ghostHumanControlled = false;
    m_playerRoomIndex = -1;
    m_localRole = m_networked && m_sessionLocalPlayerId < 0
        ? LocalRole::Spectator
        : LocalRole::Defender;
    m_playerControlsGhost = false;
    if (m_networked) {
        QHash<int, LocalRole>::iterator it = m_networkRolesByPlayerId.begin();
        while (it != m_networkRolesByPlayerId.end()) {
            it.value() = LocalRole::Defender;
            ++it;
        }
    }
    reset();
    beginRoleSelectionPhase();
}

void DormDefenseController::reset()
{
    m_tickTimer.stop();
    m_combatTimer.stop();
    m_roleSelectionTimer.stop();
    m_ghostNetworkSyncTimer.stop();
    m_playerRoomByPlayerId.clear();
    m_eliminatedDefenderPlayerIds.clear();
    m_playerRoomIndex = -1;
    m_pendingRoleSelections.clear();
    initializeBoard();
    applyRoomOwnershipPresentation();
    m_turretChargeMs = QVector<int>(m_cells.size(), 0);
    m_turretTargetInRange = QVector<bool>(m_cells.size(), false);
    m_humanGhostDoorRoomIndex = -1;
    m_humanGhostDoorChargeMs = 0;
    m_humanGhostDamagedThisSecond = false;
    m_humanGhostAttackedDoorThisSecond = false;
    m_playerControlsGhost = localControlsGhost();
    if (ghostIsHumanControlled()) {
        m_targetRoomIndex = -1;
        m_ghostPath = {qMakePair(GhostSpawnRow, GhostSpawnColumn)};
        m_ghostCenterRow = GhostSpawnRow + 0.5;
        m_ghostCenterColumn = GhostSpawnColumn + 0.5;
    } else {
        chooseRandomTargetRoom(false);
        initializeGhostPath();
        m_ghostCenterRow = GhostSpawnRow + 0.5;
        m_ghostCenterColumn = GhostSpawnColumn + 0.5;
    }
    m_networkGhostDisplayRow = m_ghostCenterRow;
    m_networkGhostDisplayColumn = m_ghostCenterColumn;
    m_gold = 0;
    m_power = 0;
    m_bedLevel = 1;
    m_doorMaxHp = InitialDoorHp;
    m_doorHp = m_doorMaxHp;
    m_wave = 0;
    m_timeRemaining = InitialTimeRemaining;
    m_elapsedTicks = 0;
    m_respawnCountdown = 0;
    m_ticksOutOfCombat = 0;
    m_ghostMode = GhostMode::Assault;
    m_turretVolley.clear();
    m_turretVolleySerial = 0;
    m_lastTurretDamage = 0;
    m_gameOver = false;
    m_winner = 0;
    if (m_roleSelectionRequired) {
        m_localRole = m_networked && m_sessionLocalPlayerId < 0
            ? LocalRole::Spectator
            : LocalRole::Defender;
        m_playerControlsGhost = false;
    }
    spawnGhostForWave();
    if (!m_rooms.isEmpty()) {
        const int playerRoomIndex = currentPlayerRoomIndex();
        if (playerRoomIndex >= 0) {
            m_rooms[playerRoomIndex].doorHp = m_doorHp;
            m_rooms[playerRoomIndex].doorMaxHp = m_doorMaxHp;
        }
        syncMembersFromPrimaryRoomState();
    }
    m_lastActionMessage = m_roleSelectionRequired
        ? QStringLiteral("Choose whether you want to control the defender or the ghost.")
        : (localControlsGhost()
               ? QStringLiteral("You control the ghost. Click a room to choose the next target.")
               : QStringLiteral("Build economy first. Only one ghost appears, but it keeps leveling up."));
    emitAllChanges();
    emit roleChanged();
}

QVariantList DormDefenseController::cells() const
{
    QVariantList list;
    list.reserve(m_cells.size());
    const int localRoomIndex = currentPlayerRoomIndex();

    for (int row = 0; row < RowCount; ++row) {
        for (int column = 0; column < ColumnCount; ++column) {
            const CellData &cell = m_cells.at(cellIndex(row, column));
            const int roomIndex = roomIndexForCell(row, column);
            const bool inLocalRoom = roomIndex >= 0 && roomIndex == localRoomIndex;
            QVariantMap map;
            map[QStringLiteral("row")] = row;
            map[QStringLiteral("column")] = column;
            map[QStringLiteral("tileType")] = tileTypeName(cell.tileKind);
            map[QStringLiteral("buildingType")] = buildingTypeName(cell.buildingKind);
            map[QStringLiteral("level")] = cell.level;
            map[QStringLiteral("localRoom")] = inLocalRoom;
            map[QStringLiteral("buildable")] = cell.tileKind == TileKind::Buildable
                || (inLocalRoom && cell.tileKind == TileKind::OtherRoom);
            map[QStringLiteral("ghostHere")] = ghostOccupiesCell(row, column);
            map[QStringLiteral("roomActive")] = roomIndex < 0 || m_rooms.at(roomIndex).active;

            QString label;
            switch (cell.tileKind) {
            case TileKind::Door:
                label = QStringLiteral("Door\n%1/%2").arg(m_doorHp).arg(m_doorMaxHp);
                break;
            case TileKind::OtherDoor: {
                const int targetRoom = roomIndexForCell(row, column);
                if (targetRoom >= 0) {
                    const RoomState &room = m_rooms.at(targetRoom);
                    label = QStringLiteral("Door\n%1/%2").arg(room.doorHp).arg(room.doorMaxHp);
                } else {
                    label = QStringLiteral("Door");
                }
                break;
            }
            case TileKind::Bed:
                label = QStringLiteral("Bed\nLv.%1").arg(m_bedLevel);
                break;
            case TileKind::OtherBed: {
                const int targetRoom = roomIndexForCell(row, column);
                if (targetRoom >= 0)
                    label = QStringLiteral("Bed\nLv.%1").arg(m_rooms.at(targetRoom).bedLevel);
                else
                    label = QStringLiteral("Bed");
                break;
            }
            case TileKind::Buildable:
                if (cell.buildingKind == BuildingKind::Generator)
                    label = QStringLiteral("Gen\nLv.%1").arg(cell.level);
                else if (cell.buildingKind == BuildingKind::Turret)
                    label = QStringLiteral("Turret\nLv.%1").arg(cell.level);
                else
                    label = QStringLiteral("Empty");
                break;
            case TileKind::Corridor:
                label = ghostOccupiesCell(row, column) ? QStringLiteral("Ghost") : QString();
                break;
            case TileKind::OtherRoom:
                label = QStringLiteral("Room");
                break;
            case TileKind::Wall:
            case TileKind::Empty:
            default:
                label = QString();
                break;
            }

            map[QStringLiteral("label")] = label;
            list.append(map);
        }
    }

    return list;
}

int DormDefenseController::gold() const
{
    const int roomIndex = currentPlayerRoomIndex();
    if (!localControlsGhost() && roomIndex >= 0 && roomIndex < m_rooms.size())
        return m_rooms.at(roomIndex).gold;
    return m_gold;
}

int DormDefenseController::power() const
{
    const int roomIndex = currentPlayerRoomIndex();
    if (!localControlsGhost() && roomIndex >= 0 && roomIndex < m_rooms.size())
        return m_rooms.at(roomIndex).power;
    return m_power;
}

int DormDefenseController::bedLevel() const
{
    const int roomIndex = currentPlayerRoomIndex();
    if (!localControlsGhost() && roomIndex >= 0 && roomIndex < m_rooms.size())
        return m_rooms.at(roomIndex).bedLevel;
    return m_bedLevel;
}

int DormDefenseController::doorHp() const
{
    const int roomIndex = currentPlayerRoomIndex();
    if (!localControlsGhost() && roomIndex >= 0 && roomIndex < m_rooms.size())
        return m_rooms.at(roomIndex).doorHp;
    return m_doorHp;
}

int DormDefenseController::doorMaxHp() const
{
    const int roomIndex = currentPlayerRoomIndex();
    if (!localControlsGhost() && roomIndex >= 0 && roomIndex < m_rooms.size())
        return m_rooms.at(roomIndex).doorMaxHp;
    return m_doorMaxHp;
}

QString DormDefenseController::statusText() const
{
    if (m_roleSelectionRequired)
        return QStringLiteral("Choose your side. Auto assignment starts in %1s.")
            .arg(m_roleSelectionCountdown);

    if (m_gameOver) {
        if (localControlsGhost()) {
            return m_winner == 2
                ? QStringLiteral("The ghost broke into the dorm. You win.")
                : QStringLiteral("The defenders held the line. You lose.");
        }
        return victory()
            ? QStringLiteral("The ghost has been defeated. The dorm is safe.")
            : QStringLiteral("The door was broken. The ghost entered the dorm.");
    }

    if (m_timeRemaining > 0) {
        if (!localControlsGhost() && !playerRoomSelected()) {
            return QStringLiteral("Preparation time: %1s. Choose a room before the ghost arrives.")
                .arg(m_timeRemaining);
        }
        return QStringLiteral("Preparation time: %1s before the ghost starts moving.")
            .arg(m_timeRemaining);
    }

    if (m_ghostMode == GhostMode::Retreat) {
        if (m_ticksOutOfCombat >= GhostRecoveryDelayTicks)
            return QStringLiteral("Ghost is retreating and recovering health.");
        return QStringLiteral("Ghost is retreating from turret fire.");
    }

    if (m_ghostDistance > 0) {
        return QStringLiteral("Ghost Lv.%1 is approaching. %2 tiles away from the door.")
            .arg(m_ghostLevel)
            .arg(m_ghostDistance);
    }

    return QStringLiteral("Ghost Lv.%1 is attacking the door. Repair it or build more turrets.")
        .arg(m_ghostLevel);
}

QString DormDefenseController::helpText() const
{
    if (!localControlsGhost() && !playerRoomSelected() && m_timeRemaining > 0)
        return QStringLiteral("Click any empty dorm room to move in during the preparation phase.");

    if (m_lastActionMessage.isEmpty())
        return localControlsGhost()
            ? QStringLiteral("Click any room to redirect the ghost's next attack target.")
            : QStringLiteral("Click empty slots in the dorm to build and upgrade.");
    return m_lastActionMessage;
}

QString DormDefenseController::debugState() const
{
    const int roomIndex = currentPlayerRoomIndex();
    QString roomSummary = QStringLiteral("room=-1");
    if (roomIndex >= 0 && roomIndex < m_rooms.size()) {
        const RoomState &room = m_rooms.at(roomIndex);
        roomSummary = QStringLiteral("room=%1 active=%2 gold=%3 power=%4 bed=%5 door=%6/%7")
            .arg(roomIndex)
            .arg(room.active ? 1 : 0)
            .arg(room.gold)
            .arg(room.power)
            .arg(room.bedLevel)
            .arg(room.doorHp)
            .arg(room.doorMaxHp);
    }

    const qreal ghostRowValue = ghostCenterRow();
    const qreal ghostColumnValue = ghostCenterColumn();
    const int attackRoomIndex = roomIndexForGhostAttackPosition(ghostRow(), ghostColumn());
    const int turretCoverageCount = turretCoverageCountAtPoint(ghostRowValue, ghostColumnValue);
    const int ghostCellRow = ghostRowValue >= 0 ? static_cast<int>(qFloor(ghostRowValue)) : -1;
    const int ghostCellColumn = ghostColumnValue >= 0 ? static_cast<int>(qFloor(ghostColumnValue)) : -1;

    return QStringLiteral("net=%1 auth=%2 localId=%3 role=%4 map=%5 %6 tick=%7 prep=%8 over=%9 ghostCell=%10,%11 turretCover=%12 doorFront=%13")
        .arg(m_networked ? 1 : 0)
        .arg(isAuthoritativeInstance() ? 1 : 0)
        .arg(m_sessionLocalPlayerId)
        .arg(localRoleName(m_localRole))
        .arg(m_playerRoomByPlayerId.size())
        .arg(roomSummary)
        .arg(m_elapsedTicks)
        .arg(m_timeRemaining)
        .arg(m_gameOver ? 1 : 0)
        .arg(ghostCellRow)
        .arg(ghostCellColumn)
        .arg(turretCoverageCount)
        .arg(attackRoomIndex);
}

QString DormDefenseController::roleSelectionChoice() const
{
    if (m_roleSelectionRequired) {
        const LocalRole choice = m_pendingRoleSelections.value(m_sessionLocalPlayerId,
                                                               LocalRole::Spectator);
        if (choice == LocalRole::Ghost)
            return QStringLiteral("ghost");
        if (choice == LocalRole::Defender)
            return QStringLiteral("defender");
        return QString();
    }

    return localRoleName(m_localRole);
}

bool DormDefenseController::ghostRoleLocked() const
{
    const int ghostPlayerId = selectedGhostPlayerId();
    return ghostPlayerId >= 0 && ghostPlayerId != m_sessionLocalPlayerId;
}

bool DormDefenseController::localPlayerEliminated() const
{
    if (localControlsGhost() || m_roleSelectionRequired)
        return false;

    const int playerId = m_networked ? m_sessionLocalPlayerId : 0;
    return m_eliminatedDefenderPlayerIds.contains(playerId);
}

int DormDefenseController::playerBedRow() const
{
    const int roomIndex = currentPlayerRoomIndex();
    return roomIndex >= 0 ? m_rooms.at(roomIndex).bedRow : RowCount / 2;
}

int DormDefenseController::playerBedColumn() const
{
    const int roomIndex = currentPlayerRoomIndex();
    return roomIndex >= 0 ? m_rooms.at(roomIndex).bedColumn : ColumnCount / 2;
}

QString DormDefenseController::networkRoleNameForPlayer(int playerId) const
{
    if (!m_networked)
        return playerId == m_sessionLocalPlayerId ? localRoleName(m_localRole) : QStringLiteral("spectator");

    return localRoleName(m_networkRolesByPlayerId.value(playerId, LocalRole::Spectator));
}

int DormDefenseController::ghostRow() const
{
    return qBound(0, static_cast<int>(qFloor(ghostCenterRow())), RowCount - 1);
}

int DormDefenseController::ghostColumn() const
{
    return qBound(0, static_cast<int>(qFloor(ghostCenterColumn())), ColumnCount - 1);
}

qreal DormDefenseController::ghostCenterRow() const
{
    if (m_networked && !m_authoritative)
        return m_networkGhostDisplayRow;
    if (ghostIsHumanControlled())
        return m_ghostCenterRow;
    const QPair<int, int> position = currentGhostPosition();
    return position.first >= 0 ? position.first + 0.5 : -1.0;
}

qreal DormDefenseController::ghostCenterColumn() const
{
    if (m_networked && !m_authoritative)
        return m_networkGhostDisplayColumn;
    if (ghostIsHumanControlled())
        return m_ghostCenterColumn;
    const QPair<int, int> position = currentGhostPosition();
    return position.second >= 0 ? position.second + 0.5 : -1.0;
}

bool DormDefenseController::ghostVisible() const
{
    if (m_timeRemaining > 0) {
        if (ghostIsHumanControlled())
            return m_ghostCenterRow >= 0.0 && m_ghostCenterColumn >= 0.0;
        return false;
    }

    if (m_gameOver || m_ghostHp <= 0)
        return false;

    if (ghostIsHumanControlled())
        return m_ghostCenterRow >= 0.0 && m_ghostCenterColumn >= 0.0;

    return !m_ghostPath.isEmpty();
}

bool DormDefenseController::buildOrUpgradeAt(int row, int column, const QString &buildType)
{
    if (m_gameOver || !localControlsDefender() || !isBuildableCell(row, column))
        return false;
    if (!playerRoomSelected())
        return false;

    if (m_networked && !isAuthoritativeInstance()) {
        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("build_upgrade")},
            {QStringLiteral("row"), row},
            {QStringLiteral("column"), column},
            {QStringLiteral("buildType"), buildType}
        });
        return true;
    }

    CellData &cell = m_cells[cellIndex(row, column)];
    const BuildingKind requestedKind = parseBuildingKind(buildType);
    if (requestedKind == BuildingKind::None && cell.buildingKind == BuildingKind::None) {
        updateStatus(QStringLiteral("Choose generator or turret first."));
        return false;
    }

    const BuildingKind targetKind = cell.buildingKind == BuildingKind::None
        ? requestedKind
        : cell.buildingKind;
    const int nextLevel = cell.level + 1;
    if (nextLevel > 6) {
        updateStatus(QStringLiteral("This building is already at the current max level."));
        return false;
    }

    const int currentDoorLevel = doorLevelForHp(m_doorMaxHp);
    const int requiredDoorLevel = requiredDoorLevelForBuilding(targetKind, nextLevel);
    const int requiredBedLevel = requiredBedLevelForBuilding(targetKind, nextLevel);
    if (currentDoorLevel < requiredDoorLevel) {
        updateStatus(QStringLiteral("Upgrade the door to Lv.%1 first to unlock this building level.")
                         .arg(requiredDoorLevel));
        return false;
    }

    if (m_bedLevel < requiredBedLevel) {
        updateStatus(QStringLiteral("Upgrade the bed to Lv.%1 first to unlock this building level.")
                         .arg(requiredBedLevel));
        return false;
    }

    const int goldCost = buildingGoldCost(targetKind, nextLevel);
    const int powerCost = buildingPowerCost(targetKind, nextLevel);
    if (m_gold < goldCost || m_power < powerCost) {
        updateStatus(QStringLiteral("Not enough resources. Upgrade beds and generators first."));
        return false;
    }

    m_gold -= goldCost;
    m_power -= powerCost;
    cell.buildingKind = targetKind;
    cell.level = nextLevel;
    syncMembersFromPrimaryRoomState();
    m_lastActionMessage = targetKind == BuildingKind::Generator
        ? QStringLiteral("Generator upgraded. Income grows faster now.")
        : QStringLiteral("Turret upgraded. Door defense is stronger now.");
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    return true;
}

QVariantMap DormDefenseController::actionInfo(const QString &type, int row, int column) const
{
    QVariantMap info;
    info[QStringLiteral("type")] = type;
    info[QStringLiteral("goldCost")] = 0;
    info[QStringLiteral("powerCost")] = 0;
    info[QStringLiteral("title")] = QString();
    info[QStringLiteral("verb")] = QStringLiteral("升级");
    info[QStringLiteral("detailText")] = QString();
    info[QStringLiteral("requirementText")] = QStringLiteral("前置：无");
    info[QStringLiteral("requirementMet")] = true;
    info[QStringLiteral("available")] = true;

    const int currentDoorLevel = doorLevelForHp(m_doorMaxHp);

    if (type == QStringLiteral("bed")) {
        const int currentLevel = m_bedLevel;
        const int nextLevel = currentLevel + 1;
        const bool available = nextLevel <= 6;
        const int cappedLevel = qMin(nextLevel, 6);
        const int requiredDoorLevel = requiredDoorLevelForBedLevel(cappedLevel);
        info[QStringLiteral("goldCost")] = available ? doublingCost(25, nextLevel) : 0;
        info[QStringLiteral("title")] = available
            ? QStringLiteral("升级床铺 Lv.%1 -> Lv.%2").arg(currentLevel).arg(nextLevel)
            : QStringLiteral("床铺已满级");
        info[QStringLiteral("verb")] = available ? QStringLiteral("升级") : QStringLiteral("满级");
        info[QStringLiteral("detailText")] = QStringLiteral("生产：%1 金币/s")
            .arg(bedGoldOutputForLevel(cappedLevel));
        info[QStringLiteral("requirementText")] = available
            ? QStringLiteral("前置：铁门 Lv.%1").arg(requiredDoorLevel)
            : QStringLiteral("前置：已达到最高等级");
        info[QStringLiteral("requirementMet")] = available && currentDoorLevel >= requiredDoorLevel;
        info[QStringLiteral("available")] = available;
        return info;
    }

    if (type == QStringLiteral("door")) {
        const int currentLevel = currentDoorLevel;
        const bool available = currentLevel < 6;
        info[QStringLiteral("goldCost")] = available ? m_doorMaxHp * 2 : 0;
        info[QStringLiteral("title")] = available
            ? QStringLiteral("升级铁门 Lv.%1 -> Lv.%2").arg(currentLevel).arg(currentLevel + 1)
            : QStringLiteral("铁门已满级");
        info[QStringLiteral("verb")] = available ? QStringLiteral("升级") : QStringLiteral("满级");
        info[QStringLiteral("detailText")] = available
            ? QStringLiteral("生命：%1 -> %2").arg(m_doorMaxHp).arg(m_doorMaxHp * 2)
            : QStringLiteral("生命：%1").arg(m_doorMaxHp);
        info[QStringLiteral("requirementText")] = available
            ? QStringLiteral("前置：建议与床铺同步升级")
            : QStringLiteral("前置：已达到最高等级");
        info[QStringLiteral("requirementMet")] = available;
        info[QStringLiteral("available")] = available;
        return info;
    }

    const BuildingKind kind = parseBuildingKind(type);
    if (kind == BuildingKind::None)
        return info;

    int currentLevel = 0;
    if (row >= 0 && row < RowCount && column >= 0 && column < ColumnCount) {
        const CellData &cell = m_cells.at(cellIndex(row, column));
        if (cell.buildingKind == kind)
            currentLevel = cell.level;
    }

    const int nextLevel = currentLevel + 1;
    const bool available = nextLevel <= 6;
    const int cappedLevel = qMin(nextLevel, 6);
    const int goldCost = available ? buildingGoldCost(kind, nextLevel) : 0;
    const int powerCost = available ? buildingPowerCost(kind, nextLevel) : 0;
    const int requiredDoorLevel = requiredDoorLevelForBuilding(kind, cappedLevel);
    const int requiredBedLevel = requiredBedLevelForBuilding(kind, cappedLevel);
    const bool requirementMet = available
        && currentDoorLevel >= requiredDoorLevel
        && m_bedLevel >= requiredBedLevel;

    info[QStringLiteral("goldCost")] = goldCost;
    info[QStringLiteral("powerCost")] = powerCost;
    info[QStringLiteral("title")] = currentLevel > 0
        ? (available
               ? QStringLiteral("升级%1 Lv.%2 -> Lv.%3")
                     .arg(kind == BuildingKind::Generator ? QStringLiteral("发电机")
                                                          : QStringLiteral("炮台"))
                     .arg(currentLevel)
                     .arg(nextLevel)
               : QStringLiteral("%1已满级")
                     .arg(kind == BuildingKind::Generator ? QStringLiteral("发电机")
                                                          : QStringLiteral("炮台")))
        : QStringLiteral("建造%1")
              .arg(kind == BuildingKind::Generator ? QStringLiteral("发电机")
                                                   : QStringLiteral("炮台"));
    info[QStringLiteral("verb")] = available
        ? (currentLevel > 0 ? QStringLiteral("升级") : QStringLiteral("建造"))
        : QStringLiteral("满级");
    info[QStringLiteral("detailText")] = kind == BuildingKind::Generator
        ? QStringLiteral("生产：%1 电力/s").arg(generatorPowerOutputForLevel(cappedLevel))
        : QStringLiteral("伤害：%1/次  射程：%2")
              .arg(turretDamageForLevel(cappedLevel))
              .arg(turretRangeForLevel(cappedLevel));
    info[QStringLiteral("requirementText")] = available
        ? QStringLiteral("前置：床铺 Lv.%1，铁门 Lv.%2").arg(requiredBedLevel).arg(requiredDoorLevel)
        : QStringLiteral("前置：已达到最高等级");
    info[QStringLiteral("requirementMet")] = requirementMet;
    info[QStringLiteral("available")] = available;
    return info;
}

void DormDefenseController::prepareForRoleSelection()
{
    m_networked = false;
    m_authoritative = false;
    m_sessionLocalPlayerId = 0;
    m_ghostHumanControlled = false;
    m_playerRoomIndex = -1;
    m_playerRoomByPlayerId.clear();
    m_localRole = LocalRole::Defender;
    m_roleSelectionRequired = true;
    m_roleSelectionCountdown = RoleSelectionDuration;
    m_playerControlsGhost = false;
    m_networkRolesByPlayerId.clear();
    m_pendingRoleSelections.clear();
    reset();
    beginRoleSelectionPhase();
}

void DormDefenseController::setPlayerControlsGhost(bool controlsGhost)
{
    if (m_networked || m_roleSelectionRequired)
        return;

    if (m_playerControlsGhost == controlsGhost && !m_roleSelectionRequired)
        return;

    m_localRole = controlsGhost ? LocalRole::Ghost : LocalRole::Defender;
    m_ghostHumanControlled = controlsGhost;
    m_playerControlsGhost = controlsGhost;
    emit roleChanged();
}

bool DormDefenseController::submitRoleSelection(bool controlsGhost)
{
    if (!m_roleSelectionRequired)
        return false;

    const LocalRole selectedRole = controlsGhost ? LocalRole::Ghost : LocalRole::Defender;

    if (m_networked && !isAuthoritativeInstance()) {
        if (!recordRoleSelectionChoice(m_sessionLocalPlayerId, selectedRole))
            return false;

        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("choose_role")},
            {QStringLiteral("role"), localRoleName(selectedRole)}
        });
        return true;
    }

    const int playerId = m_networked ? m_sessionLocalPlayerId : 0;
    if (!recordRoleSelectionChoice(playerId, selectedRole))
        return false;

    const int expectedChoices = m_networked ? m_networkRolesByPlayerId.size() : 1;
    if (expectedChoices > 0 && m_pendingRoleSelections.size() >= expectedChoices)
        finalizeRoleSelection();
    return true;
}

bool DormDefenseController::chooseDefenderRoom(int row, int column)
{
    if (m_gameOver || localControlsGhost() || m_roleSelectionRequired || m_timeRemaining <= 0)
        return false;

    if (playerRoomSelected())
        return false;

    const int roomIndex = roomIndexForCell(row, column);
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;
    if (m_networked && roomAssignedToAnotherPlayer(roomIndex, m_sessionLocalPlayerId)) {
        updateStatus(QStringLiteral("This room is already occupied by another defender."));
        return false;
    }

    if (m_networked && !isAuthoritativeInstance()) {
        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("choose_room")},
            {QStringLiteral("row"), row},
            {QStringLiteral("column"), column}
        });
        return true;
    }

    return assignPlayerRoom(roomIndex);
}

bool DormDefenseController::moveGhostBy(int deltaRow, int deltaColumn)
{
    if (!localControlsGhost() || m_gameOver || m_roleSelectionRequired || m_timeRemaining > 0)
        return false;

    if ((deltaRow == 0 && deltaColumn == 0)
        || qAbs(deltaRow) > 1 || qAbs(deltaColumn) > 1) {
        return false;
    }

    const bool shouldBroadcastMove = m_networked && !isAuthoritativeInstance();
    auto attackRoomIndexAt = [this](qreal row, qreal column) {
        const int byPosition = roomIndexForGhostAttackPosition(static_cast<int>(qFloor(row)),
                                                               static_cast<int>(qFloor(column)));
        if (byPosition >= 0)
            return byPosition;
        return roomIndexForGhostAttackPoint(row, column);
    };
    const int previousAttackRoomIndex = attackRoomIndexAt(ghostCenterRow(), ghostCenterColumn());

    const qreal currentRow = m_ghostCenterRow;
    const qreal currentColumn = m_ghostCenterColumn;
    if (currentRow < 0 || currentColumn < 0)
        return false;

    const qreal length = qSqrt(static_cast<qreal>(deltaRow * deltaRow + deltaColumn * deltaColumn));
    const qreal stepRow = (deltaRow / length) * PlayerGhostStepPerMove;
    const qreal stepColumn = (deltaColumn / length) * PlayerGhostStepPerMove;
    const qreal nextRow = currentRow + stepRow;
    const qreal nextColumn = currentColumn + stepColumn;
    if (!isCorridorPoint(nextRow, nextColumn))
        return false;

    if (deltaRow != 0 && deltaColumn != 0) {
        if (!isCorridorPoint(currentRow, nextColumn)
            || !isCorridorPoint(nextRow, currentColumn)) {
            return false;
        }
    }

    m_ghostMode = GhostMode::Assault;
    m_ticksOutOfCombat = 0;
    m_ghostCenterRow = nextRow;
    m_ghostCenterColumn = nextColumn;
    m_ghostPath = {qMakePair(ghostRow(), ghostColumn())};
    m_ghostDistance = 0;
    const bool gameFinishedByDoorAttack = isAuthoritativeInstance()
        && processHumanGhostDoorCombat(0);
    const int currentAttackRoomIndex = attackRoomIndexAt(m_ghostCenterRow, m_ghostCenterColumn);
    if (currentAttackRoomIndex >= 0 && currentAttackRoomIndex != previousAttackRoomIndex)
        m_ticksOutOfCombat = 0;
    if (shouldBroadcastMove) {
        queueGhostNetworkPositionSync();
    }
    emit ghostStateChanged();
    if (gameFinishedByDoorAttack)
        return true;
    return true;
}

bool DormDefenseController::moveGhostVector(qreal deltaRow, qreal deltaColumn)
{
    if (!localControlsGhost() || m_gameOver || m_roleSelectionRequired || m_timeRemaining > 0)
        return false;

    if ((qFuzzyIsNull(deltaRow) && qFuzzyIsNull(deltaColumn))
        || qAbs(deltaRow) > 1.0 || qAbs(deltaColumn) > 1.0) {
        return false;
    }

    const bool shouldBroadcastMove = m_networked && !isAuthoritativeInstance();
    auto attackRoomIndexAt = [this](qreal row, qreal column) {
        const int byPosition = roomIndexForGhostAttackPosition(static_cast<int>(qFloor(row)),
                                                               static_cast<int>(qFloor(column)));
        if (byPosition >= 0)
            return byPosition;
        return roomIndexForGhostAttackPoint(row, column);
    };
    const int previousAttackRoomIndex = attackRoomIndexAt(ghostCenterRow(), ghostCenterColumn());

    const qreal currentRow = m_ghostCenterRow;
    const qreal currentColumn = m_ghostCenterColumn;
    if (currentRow < 0 || currentColumn < 0)
        return false;

    const qreal length = qSqrt(deltaRow * deltaRow + deltaColumn * deltaColumn);
    if (length < 0.001)
        return false;

    const qreal stepRow = (deltaRow / length) * PlayerGhostStepPerMove;
    const qreal stepColumn = (deltaColumn / length) * PlayerGhostStepPerMove;

    qreal nextRow = currentRow + stepRow;
    qreal nextColumn = currentColumn + stepColumn;

    auto canMoveTo = [this](qreal row, qreal column) {
        return isCorridorPoint(row, column);
    };

    bool moved = false;
    if (canMoveTo(nextRow, nextColumn)) {
        moved = true;
    } else {
        const qreal rowOnly = currentRow + stepRow;
        const qreal columnOnly = currentColumn + stepColumn;
        const bool canMoveRowOnly = !qFuzzyIsNull(stepRow) && canMoveTo(rowOnly, currentColumn);
        const bool canMoveColumnOnly = !qFuzzyIsNull(stepColumn) && canMoveTo(currentRow, columnOnly);

        if (canMoveRowOnly && canMoveColumnOnly) {
            if (qAbs(deltaRow) >= qAbs(deltaColumn)) {
                nextRow = rowOnly;
                nextColumn = currentColumn;
            } else {
                nextRow = currentRow;
                nextColumn = columnOnly;
            }
            moved = true;
        } else if (canMoveRowOnly) {
            nextRow = rowOnly;
            nextColumn = currentColumn;
            moved = true;
        } else if (canMoveColumnOnly) {
            nextRow = currentRow;
            nextColumn = columnOnly;
            moved = true;
        }
    }

    if (!moved)
        return false;

    m_ghostMode = GhostMode::Assault;
    m_ticksOutOfCombat = 0;
    m_ghostCenterRow = nextRow;
    m_ghostCenterColumn = nextColumn;
    m_ghostPath = {qMakePair(ghostRow(), ghostColumn())};
    m_ghostDistance = 0;
    const bool gameFinishedByDoorAttack = isAuthoritativeInstance()
        && processHumanGhostDoorCombat(0);
    const int currentAttackRoomIndex = attackRoomIndexAt(m_ghostCenterRow, m_ghostCenterColumn);
    if (currentAttackRoomIndex >= 0 && currentAttackRoomIndex != previousAttackRoomIndex)
        m_ticksOutOfCombat = 0;
    if (shouldBroadcastMove) {
        queueGhostNetworkPositionSync();
    }
    emit ghostStateChanged();
    if (gameFinishedByDoorAttack)
        return true;
    return true;
}

bool DormDefenseController::selectGhostTargetAt(int row, int column)
{
    if (!localControlsGhost() || m_gameOver)
        return false;

    if (m_networked && !isAuthoritativeInstance()) {
        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("select_target")},
            {QStringLiteral("row"), row},
            {QStringLiteral("column"), column}
        });
        return true;
    }

    const int roomIndex = roomIndexForCell(row, column);
    if (roomIndex < 0 || roomIndex >= m_rooms.size() || !m_rooms.at(roomIndex).active)
        return false;

    const QPair<int, int> currentPosition = currentGhostPosition();
    m_targetRoomIndex = roomIndex;
    initializeGhostPath(currentPosition.first, currentPosition.second);
    m_ghostDistance = qMax(0, m_ghostPath.size() - 1);
    m_ghostMode = GhostMode::Assault;
    m_ticksOutOfCombat = 0;
    m_lastActionMessage = QStringLiteral("Ghost target changed to room %1.").arg(roomIndex + 1);
    emit ghostStateChanged();
    emit statusChanged();
    return true;
}

bool DormDefenseController::upgradeBed()
{
    if (m_gameOver || !localControlsDefender())
        return false;
    if (!playerRoomSelected()) {
        updateStatus(QStringLiteral("Choose a room before upgrading anything."));
        return false;
    }

    if (m_networked && !isAuthoritativeInstance()) {
        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("upgrade_bed")}
        });
        return true;
    }

    const int nextLevel = m_bedLevel + 1;
    if (nextLevel > 6) {
        updateStatus(QStringLiteral("The bed is already at the current max level."));
        return false;
    }

    const int currentDoorLevel = doorLevelForHp(m_doorMaxHp);
    const int requiredDoorLevel = requiredDoorLevelForBedLevel(nextLevel);
    if (currentDoorLevel < requiredDoorLevel) {
        updateStatus(QStringLiteral("Upgrade the door to Lv.%1 first to unlock the next bed level.")
                         .arg(requiredDoorLevel));
        return false;
    }

    const int goldCost = doublingCost(25, nextLevel);
    if (m_gold < goldCost) {
        updateStatus(QStringLiteral("Not enough gold to upgrade the bed."));
        return false;
    }

    m_gold -= goldCost;
    m_bedLevel = nextLevel;
    syncMembersFromPrimaryRoomState();
    updateStatus(QStringLiteral("Bed upgraded. Sleep income is higher now."));
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    return true;
}

bool DormDefenseController::upgradeDoor()
{
    if (m_gameOver || !localControlsDefender())
        return false;
    if (!playerRoomSelected()) {
        updateStatus(QStringLiteral("Choose a room before upgrading anything."));
        return false;
    }
    if (m_networked)
        syncPrimaryRoomStateFromMembers();

    if (m_networked && !isAuthoritativeInstance()) {
        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("upgrade_door")}
        });
        return true;
    }

    if (doorLevelForHp(m_doorMaxHp) >= 6) {
        updateStatus(QStringLiteral("The door is already at the current max level."));
        return false;
    }

    const int goldCost = m_doorMaxHp * 2;
    if (m_gold < goldCost) {
        updateStatus(QStringLiteral("Not enough gold to reinforce the door."));
        return false;
    }

    m_gold -= goldCost;
    m_doorMaxHp *= 2;
    m_doorHp = m_doorMaxHp;
    syncMembersFromPrimaryRoomState();
    updateStatus(QStringLiteral("Door upgraded and fully repaired."));
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    return true;
}

bool DormDefenseController::repairDoor()
{
    if (m_gameOver || !localControlsDefender())
        return false;
    if (!playerRoomSelected()) {
        updateStatus(QStringLiteral("Choose a room before upgrading anything."));
        return false;
    }

    if (m_networked && !isAuthoritativeInstance()) {
        emit networkActionRequested(QJsonObject {
            {QStringLiteral("action"), QStringLiteral("repair_door")}
        });
        return true;
    }

    if (m_doorHp >= m_doorMaxHp) {
        updateStatus(QStringLiteral("The door is already full HP."));
        return false;
    }

    const int goldCost = 18 + m_ghostLevel * 6;
    const int powerCost = 4 + m_ghostLevel;
    if (m_gold < goldCost || m_power < powerCost) {
        updateStatus(QStringLiteral("Not enough gold or power to repair the door."));
        return false;
    }

    m_gold -= goldCost;
    m_power -= powerCost;
    m_doorHp = qMin(m_doorMaxHp, m_doorHp + 28 + m_bedLevel * 4);
    syncMembersFromPrimaryRoomState();
    updateStatus(QStringLiteral("The door has been repaired."));
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    return true;
}

void DormDefenseController::beginRoleSelectionPhase()
{
    m_tickTimer.stop();
    m_combatTimer.stop();
    m_roleSelectionRequired = true;
    m_roleSelectionCountdown = RoleSelectionDuration;
    m_playerControlsGhost = false;
    m_ghostHumanControlled = false;
    m_localRole = m_networked && m_sessionLocalPlayerId < 0
        ? LocalRole::Spectator
        : LocalRole::Defender;
    m_lastActionMessage = QStringLiteral("Select your side within 10 seconds. Unselected players will be assigned automatically.");
    m_roleSelectionTimer.start();
    emitAllChanges();
    emit roleChanged();
}

bool DormDefenseController::recordRoleSelectionChoice(int playerId, LocalRole role)
{
    if (!m_roleSelectionRequired || role == LocalRole::Spectator)
        return false;

    if (m_networked && !m_networkRolesByPlayerId.contains(playerId))
        return false;

    const int ghostPlayerId = selectedGhostPlayerId();
    if (role == LocalRole::Ghost && ghostPlayerId >= 0 && ghostPlayerId != playerId)
        return false;

    if (m_pendingRoleSelections.value(playerId, LocalRole::Spectator) == role)
        return true;

    m_pendingRoleSelections.insert(playerId, role);
    emit roleChanged();
    emit statusChanged();
    return true;
}

int DormDefenseController::selectedGhostPlayerId() const
{
    for (auto it = m_pendingRoleSelections.constBegin(); it != m_pendingRoleSelections.constEnd(); ++it) {
        if (it.value() == LocalRole::Ghost)
            return it.key();
    }
    return -1;
}

void DormDefenseController::applyRoomOwnershipPresentation()
{
    for (int index = 0; index < m_rooms.size(); ++index) {
        RoomState &room = m_rooms[index];
        room.isPlayerRoom = index == m_playerRoomIndex;

        for (int row = room.top; row <= room.top + 5; ++row) {
            for (int column = room.left; column <= room.left + 5; ++column) {
                const int targetRoomIndex = roomIndexForCell(row, column);
                if (targetRoomIndex != index)
                    continue;

                CellData &cell = m_cells[cellIndex(row, column)];
                if (row == room.doorRow && column == room.doorColumn) {
                    cell.tileKind = room.isPlayerRoom ? TileKind::Door : TileKind::OtherDoor;
                } else if (row == room.bedRow && column == room.bedColumn) {
                    cell.tileKind = room.isPlayerRoom ? TileKind::Bed : TileKind::OtherBed;
                } else {
                    cell.tileKind = room.isPlayerRoom ? TileKind::Buildable : TileKind::OtherRoom;
                }
            }
        }
    }
}

bool DormDefenseController::assignPlayerRoom(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;

    m_playerRoomIndex = roomIndex;
    if (m_networked && m_sessionLocalPlayerId >= 0)
        m_playerRoomByPlayerId.insert(m_sessionLocalPlayerId, roomIndex);
    if (!ghostIsHumanControlled() && !m_networked) {
        for (int index = 0; index < m_rooms.size(); ++index)
            m_rooms[index].active = true;
        if (m_targetRoomIndex < 0
            || m_targetRoomIndex >= m_rooms.size()
            || !m_rooms.at(m_targetRoomIndex).active) {
            chooseRandomTargetRoom(false);
        }
    } else if (m_networked) {
        m_rooms[roomIndex].active = true;
        if (!ghostIsHumanControlled()
            && (m_targetRoomIndex < 0
                || m_targetRoomIndex >= m_rooms.size()
                || !m_rooms.at(m_targetRoomIndex).active)) {
            m_targetRoomIndex = roomIndex;
        }
    }
    applyRoomOwnershipPresentation();
    syncMembersFromPrimaryRoomState();
    m_lastActionMessage = QStringLiteral("You moved into room %1. Build your defenses before the ghost arrives.")
        .arg(roomIndex + 1);
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    return true;
}

void DormDefenseController::finalizeRoleSelection()
{
    if (!m_roleSelectionRequired)
        return;

    m_roleSelectionTimer.stop();

    if (m_networked) {
        QVector<int> activePlayerIds = m_networkRolesByPlayerId.keys().toVector();
        std::sort(activePlayerIds.begin(), activePlayerIds.end());

        QVector<int> ghostCandidates;
        for (auto it = m_pendingRoleSelections.constBegin(); it != m_pendingRoleSelections.constEnd(); ++it) {
            if (it.value() == LocalRole::Ghost && m_networkRolesByPlayerId.contains(it.key()))
                ghostCandidates.append(it.key());
        }

        int chosenGhostPlayerId = -1;
        if (!ghostCandidates.isEmpty()) {
            const int index = QRandomGenerator::global()->bounded(ghostCandidates.size());
            chosenGhostPlayerId = ghostCandidates.at(index);
        } else if (activePlayerIds.size() >= 7) {
            QVector<int> unselectedCandidates;
            for (int playerId : activePlayerIds) {
                if (!m_pendingRoleSelections.contains(playerId))
                    unselectedCandidates.append(playerId);
            }
            const QVector<int> &pool = unselectedCandidates.isEmpty() ? activePlayerIds
                                                                       : unselectedCandidates;
            const int index = QRandomGenerator::global()->bounded(pool.size());
            chosenGhostPlayerId = pool.at(index);
        }

        for (int playerId : activePlayerIds)
            m_networkRolesByPlayerId.insert(playerId,
                                            playerId == chosenGhostPlayerId ? LocalRole::Ghost
                                                                            : LocalRole::Defender);
        prunePlayerRoomAssignments();

        m_ghostHumanControlled = chosenGhostPlayerId >= 0;
        m_localRole = m_networkRolesByPlayerId.value(m_sessionLocalPlayerId,
                                                     m_sessionLocalPlayerId >= 0
                                                         ? LocalRole::Defender
                                                         : LocalRole::Spectator);
    } else {
        LocalRole chosenRole = m_pendingRoleSelections.value(m_sessionLocalPlayerId,
                                                             LocalRole::Spectator);
        if (chosenRole == LocalRole::Spectator) {
            chosenRole = QRandomGenerator::global()->bounded(2) == 0
                ? LocalRole::Defender
                : LocalRole::Ghost;
        }

        m_localRole = chosenRole;
        m_ghostHumanControlled = chosenRole == LocalRole::Ghost;
    }

    m_roleSelectionRequired = false;
    m_roleSelectionCountdown = 0;
    m_pendingRoleSelections.clear();
    m_playerControlsGhost = localControlsGhost();

    if (ghostIsHumanControlled()) {
        m_targetRoomIndex = -1;
        m_ghostPath = {qMakePair(GhostSpawnRow, GhostSpawnColumn)};
        m_ghostCenterRow = GhostSpawnRow + 0.5;
        m_ghostCenterColumn = GhostSpawnColumn + 0.5;
    } else {
        chooseRandomTargetRoom(false);
        initializeGhostPath();
        m_ghostCenterRow = GhostSpawnRow + 0.5;
        m_ghostCenterColumn = GhostSpawnColumn + 0.5;
    }

    m_lastActionMessage = localControlsGhost()
        ? QStringLiteral("You control the ghost. Use the movement controls to attack rooms.")
        : QStringLiteral("You control the defender. Build your room before the ghost arrives.");
    m_tickTimer.start();
    m_combatTimer.start();
    emitAllChanges();
    emit roleChanged();
}

void DormDefenseController::configureNetworkSession(const QVariantList &activePlayers,
                                                    int localPlayerId,
                                                    bool networked,
                                                    bool authoritative)
{
    m_networked = networked;
    m_authoritative = authoritative;
    m_sessionLocalPlayerId = localPlayerId;
    if (networked && !authoritative) {
        m_roleSelectionRequired = true;
        m_roleSelectionCountdown = RoleSelectionDuration;
        m_pendingRoleSelections.clear();
    }
    configureRolesFromPlayers(activePlayers, localPlayerId);
    if (!m_roleSelectionRequired) {
        m_localRole = m_networkRolesByPlayerId.value(localPlayerId,
                                                     localPlayerId >= 0
                                                         ? LocalRole::Defender
                                                         : LocalRole::Spectator);
    }
    m_playerControlsGhost = !m_roleSelectionRequired && localControlsGhost();
    emit roleChanged();
}

QJsonObject DormDefenseController::buildNetworkState() const
{
    QJsonArray cellsArray;
    for (const CellData &cell : m_cells) {
        QJsonObject cellObject;
        cellObject[QStringLiteral("buildingType")] = buildingTypeName(cell.buildingKind);
        cellObject[QStringLiteral("level")] = cell.level;
        cellsArray.append(cellObject);
    }

    QJsonArray roomsArray;
    for (const RoomState &room : m_rooms) {
        QJsonObject roomObject;
        roomObject[QStringLiteral("top")] = room.top;
        roomObject[QStringLiteral("left")] = room.left;
        roomObject[QStringLiteral("doorRow")] = room.doorRow;
        roomObject[QStringLiteral("doorColumn")] = room.doorColumn;
        roomObject[QStringLiteral("bedRow")] = room.bedRow;
        roomObject[QStringLiteral("bedColumn")] = room.bedColumn;
        roomObject[QStringLiteral("doorHp")] = room.doorHp;
        roomObject[QStringLiteral("doorMaxHp")] = room.doorMaxHp;
        roomObject[QStringLiteral("gold")] = room.gold;
        roomObject[QStringLiteral("power")] = room.power;
        roomObject[QStringLiteral("bedLevel")] = room.bedLevel;
        roomObject[QStringLiteral("active")] = room.active;
        roomObject[QStringLiteral("isPlayerRoom")] = room.isPlayerRoom;
        roomsArray.append(roomObject);
    }

    QJsonArray pathArray;
    for (const auto &point : m_ghostPath) {
        QJsonObject pointObject;
        pointObject[QStringLiteral("row")] = point.first;
        pointObject[QStringLiteral("column")] = point.second;
        pathArray.append(pointObject);
    }

    QJsonArray volleyArray;
    for (const QVariant &shotVariant : m_turretVolley) {
        const QVariantMap shotMap = shotVariant.toMap();
        QJsonObject shotObject;
        shotObject[QStringLiteral("row")] = shotMap.value(QStringLiteral("row")).toInt();
        shotObject[QStringLiteral("column")] = shotMap.value(QStringLiteral("column")).toInt();
        shotObject[QStringLiteral("damage")] = shotMap.value(QStringLiteral("damage")).toInt();
        shotObject[QStringLiteral("level")] = shotMap.value(QStringLiteral("level")).toInt();
        volleyArray.append(shotObject);
    }

    QJsonArray roleArray;
    for (auto it = m_networkRolesByPlayerId.constBegin(); it != m_networkRolesByPlayerId.constEnd(); ++it) {
        QJsonObject roleObject;
        roleObject[QStringLiteral("playerId")] = it.key();
        roleObject[QStringLiteral("role")] = localRoleName(it.value());
        roleArray.append(roleObject);
    }

    QJsonArray pendingRoleArray;
    for (auto it = m_pendingRoleSelections.constBegin(); it != m_pendingRoleSelections.constEnd(); ++it) {
        QJsonObject roleObject;
        roleObject[QStringLiteral("playerId")] = it.key();
        roleObject[QStringLiteral("role")] = localRoleName(it.value());
        pendingRoleArray.append(roleObject);
    }

    QJsonArray playerRoomArray;
    for (auto it = m_playerRoomByPlayerId.constBegin(); it != m_playerRoomByPlayerId.constEnd(); ++it) {
        QJsonObject playerRoomObject;
        playerRoomObject[QStringLiteral("playerId")] = it.key();
        playerRoomObject[QStringLiteral("roomIndex")] = it.value();
        playerRoomArray.append(playerRoomObject);
    }

    QJsonArray eliminatedDefenderArray;
    for (int playerId : m_eliminatedDefenderPlayerIds)
        eliminatedDefenderArray.append(playerId);

    const QPair<int, int> pathPosition = currentGhostPosition();
    const qreal ghostRowCenter = ghostIsHumanControlled()
        ? m_ghostCenterRow
        : (pathPosition.first >= 0 ? pathPosition.first + 0.5 : m_ghostCenterRow);
    const qreal ghostColumnCenter = ghostIsHumanControlled()
        ? m_ghostCenterColumn
        : (pathPosition.second >= 0 ? pathPosition.second + 0.5 : m_ghostCenterColumn);

    QJsonObject state;
    state[QStringLiteral("cells")] = cellsArray;
    state[QStringLiteral("rooms")] = roomsArray;
    state[QStringLiteral("ghostPath")] = pathArray;
    state[QStringLiteral("turretVolley")] = volleyArray;
    state[QStringLiteral("roles")] = roleArray;
    state[QStringLiteral("pendingRoleSelections")] = pendingRoleArray;
    state[QStringLiteral("playerRooms")] = playerRoomArray;
    state[QStringLiteral("eliminatedDefenders")] = eliminatedDefenderArray;
    state[QStringLiteral("gold")] = m_gold;
    state[QStringLiteral("power")] = m_power;
    state[QStringLiteral("bedLevel")] = m_bedLevel;
    state[QStringLiteral("doorHp")] = m_doorHp;
    state[QStringLiteral("doorMaxHp")] = m_doorMaxHp;
    state[QStringLiteral("wave")] = m_wave;
    state[QStringLiteral("timeRemaining")] = m_timeRemaining;
    state[QStringLiteral("ghostHp")] = m_ghostHp;
    state[QStringLiteral("ghostMaxHp")] = m_ghostMaxHp;
    state[QStringLiteral("ghostLevel")] = m_ghostLevel;
    state[QStringLiteral("ghostAttack")] = m_ghostAttack;
    state[QStringLiteral("ghostDistance")] = m_ghostDistance;
    state[QStringLiteral("ghostCenterRow")] = ghostRowCenter;
    state[QStringLiteral("ghostCenterColumn")] = ghostColumnCenter;
    state[QStringLiteral("elapsedTicks")] = m_elapsedTicks;
    state[QStringLiteral("ghostDoorHitCount")] = m_ghostDoorHitCount;
    state[QStringLiteral("respawnCountdown")] = m_respawnCountdown;
    state[QStringLiteral("targetRoomIndex")] = m_targetRoomIndex;
    state[QStringLiteral("ticksOutOfCombat")] = m_ticksOutOfCombat;
    state[QStringLiteral("turretVolleySerial")] = m_turretVolleySerial;
    state[QStringLiteral("lastTurretDamage")] = m_lastTurretDamage;
    state[QStringLiteral("gameOver")] = m_gameOver;
    state[QStringLiteral("winner")] = m_winner;
    state[QStringLiteral("roleSelectionRequired")] = m_roleSelectionRequired;
    state[QStringLiteral("roleSelectionCountdown")] = m_roleSelectionCountdown;
    state[QStringLiteral("ghostHumanControlled")] = m_ghostHumanControlled;
    state[QStringLiteral("ghostMode")] = m_ghostMode == GhostMode::Retreat
        ? QStringLiteral("retreat")
        : QStringLiteral("assault");
    state[QStringLiteral("lastActionMessage")] = m_lastActionMessage;
    return state;
}

QJsonObject DormDefenseController::buildNetworkStateForPlayer(int playerId) const
{
    QJsonObject state = buildNetworkState();
    const LocalRole role = m_networkRolesByPlayerId.value(playerId,
                                                          playerId >= 0
                                                              ? LocalRole::Defender
                                                              : LocalRole::Spectator);
    const int roomIndex = playerRoomIndexForPlayer(playerId);

    state[QStringLiteral("localRole")] = localRoleName(role);
    state[QStringLiteral("localPlayerRoomIndex")] = roomIndex;
    state[QStringLiteral("localPlayerEliminated")] = m_eliminatedDefenderPlayerIds.contains(playerId);

    if (role == LocalRole::Defender && roomIndex >= 0 && roomIndex < m_rooms.size()) {
        const RoomState &room = m_rooms.at(roomIndex);
        state[QStringLiteral("gold")] = room.gold;
        state[QStringLiteral("power")] = room.power;
        state[QStringLiteral("bedLevel")] = room.bedLevel;
        state[QStringLiteral("doorHp")] = room.doorHp;
        state[QStringLiteral("doorMaxHp")] = room.doorMaxHp;
    } else {
        state[QStringLiteral("gold")] = 0;
        state[QStringLiteral("power")] = 0;
        state[QStringLiteral("bedLevel")] = 1;
        state[QStringLiteral("doorHp")] = InitialDoorHp;
        state[QStringLiteral("doorMaxHp")] = InitialDoorHp;
    }

    return state;
}

void DormDefenseController::applyNetworkState(const QJsonObject &state)
{
    m_networked = true;
    m_authoritative = false;
    m_tickTimer.stop();
    m_combatTimer.stop();
    m_roleSelectionTimer.stop();
    const QVector<CellData> previousCells = m_cells;
    const QVector<RoomState> previousRooms = m_rooms;
    const int previousPlayerRoomIndex = m_playerRoomIndex;
    const int previousResolvedRoomIndex = currentPlayerRoomIndex();
    const QVector<QPair<int, int>> previousGhostPath = m_ghostPath;
    const QVariantList previousTurretVolley = m_turretVolley;
    const int previousGold = m_gold;
    const int previousPower = m_power;
    const int previousBedLevel = m_bedLevel;
    const int previousDoorHp = m_doorHp;
    const int previousDoorMaxHp = m_doorMaxHp;
    const int previousWave = m_wave;
    const int previousTimeRemaining = m_timeRemaining;
    const int previousGhostHp = m_ghostHp;
    const int previousGhostMaxHp = m_ghostMaxHp;
    const int previousGhostLevel = m_ghostLevel;
    const int previousGhostAttack = m_ghostAttack;
    const int previousGhostDistance = m_ghostDistance;
    const qreal previousGhostCenterRowValue = m_ghostCenterRow;
    const qreal previousGhostCenterColumnValue = m_ghostCenterColumn;
    const int previousElapsedTicks = m_elapsedTicks;
    const int previousGhostDoorHitCount = m_ghostDoorHitCount;
    const int previousRespawnCountdown = m_respawnCountdown;
    const int previousTargetRoomIndex = m_targetRoomIndex;
    const int previousTicksOutOfCombat = m_ticksOutOfCombat;
    const int previousTurretVolleySerial = m_turretVolleySerial;
    const int previousLastTurretDamage = m_lastTurretDamage;
    const bool previousGameOver = m_gameOver;
    const int previousWinner = m_winner;
    const LocalRole previousLocalRole = m_localRole;
    const bool previousPlayerControlsGhost = m_playerControlsGhost;
    const bool previousRoleSelectionRequired = m_roleSelectionRequired;
    const int previousRoleSelectionCountdown = m_roleSelectionCountdown;
    const bool previousGhostHumanControlled = m_ghostHumanControlled;
    const GhostMode previousGhostMode = m_ghostMode;
    const QString previousLastActionMessage = m_lastActionMessage;

    int previousResolvedGold = previousGold;
    int previousResolvedPower = previousPower;
    int previousResolvedBedLevel = previousBedLevel;
    int previousResolvedDoorHp = previousDoorHp;
    int previousResolvedDoorMaxHp = previousDoorMaxHp;
    bool previousResolvedRoomActive = true;
    if (previousResolvedRoomIndex >= 0 && previousResolvedRoomIndex < previousRooms.size()) {
        const RoomState &previousLocalRoom = previousRooms.at(previousResolvedRoomIndex);
        previousResolvedGold = previousLocalRoom.gold;
        previousResolvedPower = previousLocalRoom.power;
        previousResolvedBedLevel = previousLocalRoom.bedLevel;
        previousResolvedDoorHp = previousLocalRoom.doorHp;
        previousResolvedDoorMaxHp = previousLocalRoom.doorMaxHp;
        previousResolvedRoomActive = previousLocalRoom.active;
    }

    const QJsonArray cellsArray = state.value(QStringLiteral("cells")).toArray();
    if (cellsArray.size() == m_cells.size()) {
        for (int index = 0; index < cellsArray.size(); ++index) {
            const QJsonObject cellObject = cellsArray.at(index).toObject();
            m_cells[index].buildingKind = parseBuildingKind(
                cellObject.value(QStringLiteral("buildingType")).toString());
            m_cells[index].level = cellObject.value(QStringLiteral("level")).toInt();
        }
    }

    const QJsonArray roomsArray = state.value(QStringLiteral("rooms")).toArray();
    for (int index = 0; index < roomsArray.size() && index < m_rooms.size(); ++index) {
        const QJsonObject roomObject = roomsArray.at(index).toObject();
        RoomState &room = m_rooms[index];
        room.top = roomObject.value(QStringLiteral("top")).toInt(room.top);
        room.left = roomObject.value(QStringLiteral("left")).toInt(room.left);
        room.doorRow = roomObject.value(QStringLiteral("doorRow")).toInt(room.doorRow);
        room.doorColumn = roomObject.value(QStringLiteral("doorColumn")).toInt(room.doorColumn);
        room.bedRow = roomObject.value(QStringLiteral("bedRow")).toInt(room.bedRow);
        room.bedColumn = roomObject.value(QStringLiteral("bedColumn")).toInt(room.bedColumn);
        room.doorHp = roomObject.value(QStringLiteral("doorHp")).toInt(room.doorHp);
        room.doorMaxHp = roomObject.value(QStringLiteral("doorMaxHp")).toInt(room.doorMaxHp);
        room.gold = roomObject.value(QStringLiteral("gold")).toInt(room.gold);
        room.power = roomObject.value(QStringLiteral("power")).toInt(room.power);
        room.bedLevel = roomObject.value(QStringLiteral("bedLevel")).toInt(room.bedLevel);
        room.active = roomObject.value(QStringLiteral("active")).toBool(room.active);
        room.isPlayerRoom = false;
    }

    m_playerRoomByPlayerId.clear();
    const QJsonArray playerRoomArray = state.value(QStringLiteral("playerRooms")).toArray();
    for (const QJsonValue &playerRoomValue : playerRoomArray) {
        const QJsonObject playerRoomObject = playerRoomValue.toObject();
        const int playerId = playerRoomObject.value(QStringLiteral("playerId")).toInt(-1);
        const int roomIndex = playerRoomObject.value(QStringLiteral("roomIndex")).toInt(-1);
        if (playerId >= 0 && roomIndex >= 0 && roomIndex < m_rooms.size())
            m_playerRoomByPlayerId.insert(playerId, roomIndex);
    }

    m_eliminatedDefenderPlayerIds.clear();
    const QJsonArray eliminatedDefenderArray = state.value(QStringLiteral("eliminatedDefenders")).toArray();
    for (const QJsonValue &playerIdValue : eliminatedDefenderArray) {
        const int playerId = playerIdValue.toInt(-1);
        if (playerId >= 0)
            m_eliminatedDefenderPlayerIds.insert(playerId);
    }
    if (state.value(QStringLiteral("localPlayerEliminated")).toBool(false)
        && m_sessionLocalPlayerId >= 0) {
        m_eliminatedDefenderPlayerIds.insert(m_sessionLocalPlayerId);
    }

    m_ghostPath.clear();
    const QJsonArray pathArray = state.value(QStringLiteral("ghostPath")).toArray();
    m_ghostPath.reserve(pathArray.size());
    for (const QJsonValue &pointValue : pathArray) {
        const QJsonObject pointObject = pointValue.toObject();
        m_ghostPath.append(qMakePair(pointObject.value(QStringLiteral("row")).toInt(),
                                     pointObject.value(QStringLiteral("column")).toInt()));
    }

    m_turretVolley.clear();
    const QJsonArray volleyArray = state.value(QStringLiteral("turretVolley")).toArray();
    m_turretVolley.reserve(volleyArray.size());
    for (const QJsonValue &shotValue : volleyArray) {
        const QJsonObject shotObject = shotValue.toObject();
        QVariantMap shotMap;
        shotMap[QStringLiteral("row")] = shotObject.value(QStringLiteral("row")).toInt();
        shotMap[QStringLiteral("column")] = shotObject.value(QStringLiteral("column")).toInt();
        shotMap[QStringLiteral("damage")] = shotObject.value(QStringLiteral("damage")).toInt();
        shotMap[QStringLiteral("level")] = shotObject.value(QStringLiteral("level")).toInt();
        m_turretVolley.append(shotMap);
    }

    m_networkRolesByPlayerId.clear();
    const QJsonArray roleArray = state.value(QStringLiteral("roles")).toArray();
    for (const QJsonValue &roleValue : roleArray) {
        const QJsonObject roleObject = roleValue.toObject();
        m_networkRolesByPlayerId.insert(roleObject.value(QStringLiteral("playerId")).toInt(-1),
                                        localRoleFromName(
                                            roleObject.value(QStringLiteral("role")).toString()));
    }

    m_pendingRoleSelections.clear();
    const QJsonArray pendingRoleArray = state.value(QStringLiteral("pendingRoleSelections")).toArray();
    for (const QJsonValue &roleValue : pendingRoleArray) {
        const QJsonObject roleObject = roleValue.toObject();
        m_pendingRoleSelections.insert(roleObject.value(QStringLiteral("playerId")).toInt(-1),
                                       localRoleFromName(
                                           roleObject.value(QStringLiteral("role")).toString()));
    }

    m_gold = state.value(QStringLiteral("gold")).toInt(m_gold);
    m_power = state.value(QStringLiteral("power")).toInt(m_power);
    m_bedLevel = state.value(QStringLiteral("bedLevel")).toInt(m_bedLevel);
    m_doorHp = state.value(QStringLiteral("doorHp")).toInt(m_doorHp);
    m_doorMaxHp = state.value(QStringLiteral("doorMaxHp")).toInt(m_doorMaxHp);
    m_wave = state.value(QStringLiteral("wave")).toInt(m_wave);
    m_timeRemaining = state.value(QStringLiteral("timeRemaining")).toInt(m_timeRemaining);
    m_ghostHp = state.value(QStringLiteral("ghostHp")).toInt(m_ghostHp);
    m_ghostMaxHp = state.value(QStringLiteral("ghostMaxHp")).toInt(m_ghostMaxHp);
    m_ghostLevel = state.value(QStringLiteral("ghostLevel")).toInt(m_ghostLevel);
    m_ghostAttack = state.value(QStringLiteral("ghostAttack")).toInt(m_ghostAttack);
    m_ghostDistance = state.value(QStringLiteral("ghostDistance")).toInt(m_ghostDistance);
    m_ghostCenterRow = state.value(QStringLiteral("ghostCenterRow")).toDouble(m_ghostCenterRow);
    m_ghostCenterColumn = state.value(QStringLiteral("ghostCenterColumn")).toDouble(m_ghostCenterColumn);
    m_networkGhostDisplayRow = m_ghostCenterRow;
    m_networkGhostDisplayColumn = m_ghostCenterColumn;
    m_elapsedTicks = state.value(QStringLiteral("elapsedTicks")).toInt(m_elapsedTicks);
    m_ghostDoorHitCount = state.value(QStringLiteral("ghostDoorHitCount")).toInt(m_ghostDoorHitCount);
    m_respawnCountdown = state.value(QStringLiteral("respawnCountdown")).toInt(m_respawnCountdown);
    m_targetRoomIndex = state.value(QStringLiteral("targetRoomIndex")).toInt(m_targetRoomIndex);
    m_ticksOutOfCombat = state.value(QStringLiteral("ticksOutOfCombat")).toInt(m_ticksOutOfCombat);
    m_turretVolleySerial = state.value(QStringLiteral("turretVolleySerial")).toInt(m_turretVolleySerial);
    m_lastTurretDamage = state.value(QStringLiteral("lastTurretDamage")).toInt(m_lastTurretDamage);
    m_gameOver = state.value(QStringLiteral("gameOver")).toBool(m_gameOver);
    m_winner = state.value(QStringLiteral("winner")).toInt(m_winner);
    m_roleSelectionRequired = state.value(QStringLiteral("roleSelectionRequired")).toBool(false);
    m_roleSelectionCountdown = state.value(QStringLiteral("roleSelectionCountdown")).toInt(0);
    m_ghostHumanControlled = state.value(QStringLiteral("ghostHumanControlled")).toBool(m_ghostHumanControlled);
    m_ghostMode = state.value(QStringLiteral("ghostMode")).toString() == QStringLiteral("retreat")
        ? GhostMode::Retreat
        : GhostMode::Assault;
    m_lastActionMessage = state.value(QStringLiteral("lastActionMessage")).toString(m_lastActionMessage);

    const QString fallbackRole = networkRoleNameForPlayer(m_sessionLocalPlayerId);
    const LocalRole nextRole = localRoleFromName(
        state.value(QStringLiteral("localRole")).toString(fallbackRole));
    const int incomingGold = state.value(QStringLiteral("gold")).toInt(m_gold);
    const int incomingPower = state.value(QStringLiteral("power")).toInt(m_power);
    const int incomingBedLevel = state.value(QStringLiteral("bedLevel")).toInt(m_bedLevel);
    const int incomingDoorHp = state.value(QStringLiteral("doorHp")).toInt(m_doorHp);
    const int incomingDoorMaxHp = state.value(QStringLiteral("doorMaxHp")).toInt(m_doorMaxHp);
    const int incomingLocalRoomIndex = state.value(QStringLiteral("localPlayerRoomIndex")).toInt(-1);
    const int mappedLocalRoomIndex = playerRoomIndexForPlayer(m_sessionLocalPlayerId);
    if (incomingLocalRoomIndex >= 0) {
        m_playerRoomIndex = incomingLocalRoomIndex;
    } else if (mappedLocalRoomIndex >= 0) {
        m_playerRoomIndex = mappedLocalRoomIndex;
    } else {
        m_playerRoomIndex = -1;
    }
    applyRoomOwnershipPresentation();
    if (nextRole == LocalRole::Defender && m_playerRoomIndex >= 0 && m_playerRoomIndex < m_rooms.size()) {
        RoomState &localRoom = m_rooms[m_playerRoomIndex];
        localRoom.gold = incomingGold;
        localRoom.power = incomingPower;
        localRoom.bedLevel = incomingBedLevel;
        localRoom.doorHp = incomingDoorHp;
        localRoom.doorMaxHp = incomingDoorMaxHp;
        syncPrimaryRoomStateFromMembers();
    }
    const bool localGhostClient = nextRole == LocalRole::Ghost && !m_roleSelectionRequired;
    if (localGhostClient) {
        if (!previousGhostPath.isEmpty()) {
            m_ghostPath = previousGhostPath;
            m_targetRoomIndex = previousTargetRoomIndex;
        }
        if (!previousRoleSelectionRequired) {
            m_ghostCenterRow = previousGhostCenterRowValue;
            m_ghostCenterColumn = previousGhostCenterColumnValue;
        }
    }
    m_localRole = nextRole;
    m_playerControlsGhost = !m_roleSelectionRequired && localControlsGhost();
    const bool roleWasChanged = nextRole != previousLocalRole
        || m_playerControlsGhost != previousPlayerControlsGhost
        || m_roleSelectionRequired != previousRoleSelectionRequired
        || m_roleSelectionCountdown != previousRoleSelectionCountdown
        || m_ghostHumanControlled != previousGhostHumanControlled;
    bool cellsWereChanged = m_cells.size() != previousCells.size();
    if (!cellsWereChanged) {
        for (int index = 0; index < m_cells.size(); ++index) {
            const CellData &current = m_cells.at(index);
            const CellData &previous = previousCells.at(index);
            if (current.tileKind != previous.tileKind
                || current.buildingKind != previous.buildingKind
                || current.level != previous.level) {
                cellsWereChanged = true;
                break;
            }
        }
    }

    bool roomsWereChanged = m_rooms.size() != previousRooms.size();
    if (!roomsWereChanged) {
        for (int index = 0; index < m_rooms.size(); ++index) {
            const RoomState &current = m_rooms.at(index);
            const RoomState &previous = previousRooms.at(index);
            if (current.top != previous.top
                || current.left != previous.left
                || current.doorRow != previous.doorRow
                || current.doorColumn != previous.doorColumn
                || current.bedRow != previous.bedRow
                || current.bedColumn != previous.bedColumn
                || current.doorHp != previous.doorHp
                || current.doorMaxHp != previous.doorMaxHp
                || current.gold != previous.gold
                || current.power != previous.power
                || current.bedLevel != previous.bedLevel
                || current.active != previous.active
                || current.isPlayerRoom != previous.isPlayerRoom) {
                roomsWereChanged = true;
                break;
            }
        }
    }

    const bool boardWasChanged = cellsWereChanged
        || roomsWereChanged
        || m_playerRoomIndex != previousPlayerRoomIndex
        || m_doorHp != previousDoorHp
        || m_doorMaxHp != previousDoorMaxHp
        || m_bedLevel != previousBedLevel;
    const int currentResolvedRoomIndex = currentPlayerRoomIndex();
    int currentResolvedGold = m_gold;
    int currentResolvedPower = m_power;
    int currentResolvedBedLevel = m_bedLevel;
    int currentResolvedDoorHp = m_doorHp;
    int currentResolvedDoorMaxHp = m_doorMaxHp;
    bool currentResolvedRoomActive = true;
    if (currentResolvedRoomIndex >= 0 && currentResolvedRoomIndex < m_rooms.size()) {
        const RoomState &currentLocalRoom = m_rooms.at(currentResolvedRoomIndex);
        currentResolvedGold = currentLocalRoom.gold;
        currentResolvedPower = currentLocalRoom.power;
        currentResolvedBedLevel = currentLocalRoom.bedLevel;
        currentResolvedDoorHp = currentLocalRoom.doorHp;
        currentResolvedDoorMaxHp = currentLocalRoom.doorMaxHp;
        currentResolvedRoomActive = currentLocalRoom.active;
    }
    const bool localRoomResourcesWereChanged = currentResolvedRoomIndex != previousResolvedRoomIndex
        || currentResolvedGold != previousResolvedGold
        || currentResolvedPower != previousResolvedPower
        || currentResolvedBedLevel != previousResolvedBedLevel
        || currentResolvedDoorHp != previousResolvedDoorHp
        || currentResolvedDoorMaxHp != previousResolvedDoorMaxHp
        || currentResolvedRoomActive != previousResolvedRoomActive;
    const bool resourcesWereChanged = m_gold != previousGold
        || m_power != previousPower
        || m_bedLevel != previousBedLevel
        || localRoomResourcesWereChanged;
    const bool statusWasChanged = m_wave != previousWave
        || m_timeRemaining != previousTimeRemaining
        || m_ghostHp != previousGhostHp
        || m_ghostMaxHp != previousGhostMaxHp
        || m_ghostLevel != previousGhostLevel
        || m_ghostAttack != previousGhostAttack
        || m_ghostDistance != previousGhostDistance
        || m_elapsedTicks != previousElapsedTicks
        || m_ghostDoorHitCount != previousGhostDoorHitCount
        || m_respawnCountdown != previousRespawnCountdown
        || m_ticksOutOfCombat != previousTicksOutOfCombat
        || m_gameOver != previousGameOver
        || m_winner != previousWinner
        || m_roleSelectionRequired != previousRoleSelectionRequired
        || m_roleSelectionCountdown != previousRoleSelectionCountdown
        || m_ghostHumanControlled != previousGhostHumanControlled
        || m_ghostMode != previousGhostMode
        || m_lastActionMessage != previousLastActionMessage;
    const bool ghostStateWasChanged = m_ghostCenterRow != previousGhostCenterRowValue
        || m_ghostCenterColumn != previousGhostCenterColumnValue
        || m_ghostPath != previousGhostPath
        || m_targetRoomIndex != previousTargetRoomIndex
        || m_ghostDistance != previousGhostDistance
        || m_ghostMode != previousGhostMode;
    const bool turretVolleyWasChanged = m_turretVolley != previousTurretVolley
        || m_turretVolleySerial != previousTurretVolleySerial
        || m_lastTurretDamage != previousLastTurretDamage;
    const bool gameOverWasChanged = m_gameOver != previousGameOver
        || m_winner != previousWinner;

    if (boardWasChanged)
        emit boardChanged();
    if (resourcesWereChanged || m_networked)
        emit resourcesChanged();
    if (statusWasChanged)
        emit statusChanged();
    if (ghostStateWasChanged)
        emit ghostStateChanged();
    if (turretVolleyWasChanged)
        emit turretVolleyChanged();
    if (roleWasChanged)
        emit roleChanged();
    if (gameOverWasChanged)
        emit gameOverChanged();

    const int localRoomIndex = currentPlayerRoomIndex();
    if (!m_gameOver
        && !m_roleSelectionRequired
        && !localControlsGhost()
        && !hasAnyActiveRoom()) {
        finalizeGameOver(2);
    }
}

void DormDefenseController::applyNetworkGhostPosition(qreal row, qreal column)
{
    if (!m_networked || !qIsFinite(row) || !qIsFinite(column))
        return;

    m_networkGhostDisplayRow = row;
    m_networkGhostDisplayColumn = column;
    if (!localControlsGhost()) {
        m_ghostCenterRow = row;
        m_ghostCenterColumn = column;
    }
    emit ghostStateChanged();
}

bool DormDefenseController::applyNetworkAction(int playerId, const QJsonObject &action)
{
    const QString actionType = action.value(QStringLiteral("action")).toString();
    if (actionType.isEmpty())
        return false;

    if (actionType == QStringLiteral("choose_role")) {
        const QString requestedRole = action.value(QStringLiteral("role"))
                                          .toString(QStringLiteral("defender"))
                                          .trimmed()
                                          .toLower();
        const LocalRole nextRole = requestedRole == QStringLiteral("ghost")
            ? LocalRole::Ghost
            : LocalRole::Defender;
        const bool ok = recordRoleSelectionChoice(playerId, nextRole);
        const int expectedChoices = m_networkRolesByPlayerId.size();
        if (ok && expectedChoices > 0 && m_pendingRoleSelections.size() >= expectedChoices)
            finalizeRoleSelection();
        return ok;
    }

    if (m_roleSelectionRequired)
        return false;

    const LocalRole originalRole = m_localRole;
    const bool originalPlayerControlsGhost = m_playerControlsGhost;
    const int originalSessionLocalPlayerId = m_sessionLocalPlayerId;
    const int originalPlayerRoomIndex = m_playerRoomIndex;
    const int originalGold = m_gold;
    const int originalPower = m_power;
    const int originalBedLevel = m_bedLevel;
    const int originalDoorHp = m_doorHp;
    const int originalDoorMaxHp = m_doorMaxHp;

    auto restoreRole = [&]() {
        m_localRole = originalRole;
        m_playerControlsGhost = originalPlayerControlsGhost;
        m_sessionLocalPlayerId = originalSessionLocalPlayerId;
        m_playerRoomIndex = originalPlayerRoomIndex;
        const int restoredRoomIndex = currentPlayerRoomIndex();
        if (restoredRoomIndex >= 0) {
            m_playerRoomIndex = restoredRoomIndex;
            syncPrimaryRoomStateFromMembers();
        } else {
            m_gold = originalGold;
            m_power = originalPower;
            m_bedLevel = originalBedLevel;
            m_doorHp = originalDoorHp;
            m_doorMaxHp = originalDoorMaxHp;
        }
        applyRoomOwnershipPresentation();
    };

    const bool playerIsGhost = m_networkRolesByPlayerId.value(playerId, LocalRole::Defender)
        == LocalRole::Ghost;
    m_sessionLocalPlayerId = playerId;
    m_localRole = playerIsGhost ? LocalRole::Ghost : LocalRole::Defender;
    m_playerControlsGhost = localControlsGhost();
    if (!playerIsGhost) {
        m_playerRoomIndex = playerRoomIndexForPlayer(playerId);
        if (m_playerRoomIndex >= 0)
            syncPrimaryRoomStateFromMembers();
    }

    bool ok = false;
    if (actionType == QStringLiteral("move_ghost") && playerIsGhost) {
        if (action.contains(QStringLiteral("row"))
            && action.contains(QStringLiteral("column"))) {
            ok = moveGhostToNetworkPosition(action.value(QStringLiteral("row")).toDouble(),
                                            action.value(QStringLiteral("column")).toDouble());
        } else {
            ok = moveGhostVector(action.value(QStringLiteral("deltaRow")).toDouble(),
                                 action.value(QStringLiteral("deltaColumn")).toDouble());
        }
    } else if (actionType == QStringLiteral("select_target") && playerIsGhost) {
        ok = selectGhostTargetAt(action.value(QStringLiteral("row")).toInt(-1),
                                 action.value(QStringLiteral("column")).toInt(-1));
    } else if (actionType == QStringLiteral("build_upgrade") && !playerIsGhost) {
        ok = buildOrUpgradeAt(action.value(QStringLiteral("row")).toInt(-1),
                              action.value(QStringLiteral("column")).toInt(-1),
                              action.value(QStringLiteral("buildType")).toString());
    } else if (actionType == QStringLiteral("choose_room") && !playerIsGhost) {
        ok = chooseDefenderRoom(action.value(QStringLiteral("row")).toInt(-1),
                                action.value(QStringLiteral("column")).toInt(-1));
    } else if (actionType == QStringLiteral("upgrade_bed") && !playerIsGhost) {
        ok = upgradeBed();
    } else if (actionType == QStringLiteral("upgrade_door") && !playerIsGhost) {
        ok = upgradeDoor();
    } else if (actionType == QStringLiteral("repair_door") && !playerIsGhost) {
        ok = repairDoor();
    }

    if (ok && !playerIsGhost) {
        if (m_playerRoomIndex >= 0)
            m_playerRoomByPlayerId.insert(playerId, m_playerRoomIndex);
        syncMembersFromPrimaryRoomState();
    }

    restoreRole();
    return ok;
}

void DormDefenseController::initializeBoard()
{
    m_cells = QVector<CellData>(RowCount * ColumnCount);
    m_rooms.clear();

    auto setTile = [this](int row, int column, TileKind kind) {
        CellData &cell = m_cells[cellIndex(row, column)];
        cell.tileKind = kind;
        cell.buildingKind = BuildingKind::None;
        cell.level = 0;
    };

    for (int row = 0; row < RowCount; ++row) {
        for (int column = 0; column < ColumnCount; ++column)
            setTile(row, column, TileKind::Corridor);
    }

    const QVector<QPair<int, int>> templateTiles = roomTemplate();
    const QVector<QPair<int, int>> roomOrigins = {
        {3, 3},
        {3, 13},
        {3, 23},
        {14, 3},
        {14, 13},
        {14, 23}
    };

    auto containsTemplateTile = [&](int relativeRow, int relativeColumn) {
        for (const auto &tile : templateTiles) {
            if (tile.first == relativeRow && tile.second == relativeColumn)
                return true;
        }
        return false;
    };

    auto paintRoom = [&](int top, int left) {
        QVector<QPair<int, int>> absoluteTiles;
        absoluteTiles.reserve(templateTiles.size());

        for (const auto &tile : templateTiles) {
            const int row = top + tile.first;
            const int column = left + tile.second;
            absoluteTiles.append({row, column});
            setTile(row, column, TileKind::OtherRoom);
        }

        static const QPair<int, int> directions[] = {
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}
        };

        const int roomDoorRow = top + RoomDoorOffsetRow;
        const int roomDoorColumn = left + RoomDoorOffsetColumn;
        const int roomBedRow = top + RoomBedOffsetRow;
        const int roomBedColumn = left + RoomBedOffsetColumn;
        RoomState room;
        room.top = top;
        room.left = left;
        room.doorRow = roomDoorRow;
        room.doorColumn = roomDoorColumn;
        room.bedRow = roomBedRow;
        room.bedColumn = roomBedColumn;
        room.doorHp = InitialDoorHp;
        room.doorMaxHp = InitialDoorHp;
        room.active = true;
        room.isPlayerRoom = false;

        for (const auto &tile : absoluteTiles) {
            for (const auto &direction : directions) {
                const int neighborRow = tile.first + direction.first;
                const int neighborColumn = tile.second + direction.second;
                const int relativeRow = neighborRow - top;
                const int relativeColumn = neighborColumn - left;

                if (neighborRow < 0 || neighborRow >= RowCount
                    || neighborColumn < 0 || neighborColumn >= ColumnCount) {
                    continue;
                }

                if (containsTemplateTile(relativeRow, relativeColumn))
                    continue;

                if (neighborRow == roomDoorRow && neighborColumn == roomDoorColumn)
                    continue;

                CellData &neighbor = m_cells[cellIndex(neighborRow, neighborColumn)];
                neighbor.tileKind = TileKind::Wall;
                neighbor.buildingKind = BuildingKind::None;
                neighbor.level = 0;
            }
        }

        setTile(roomBedRow, roomBedColumn, TileKind::OtherBed);
        setTile(roomDoorRow, roomDoorColumn, TileKind::OtherDoor);

        m_rooms.append(room);
    };

    for (const auto &origin : roomOrigins)
        paintRoom(origin.first, origin.second);
}

void DormDefenseController::initializeGhostPath(int startRow, int startColumn)
{
    const int targetRoomIndex = currentTargetRoomIndex();
    if (targetRoomIndex < 0 || targetRoomIndex >= m_rooms.size()) {
        m_ghostPath.clear();
        return;
    }

    const RoomState &targetRoom = m_rooms.at(targetRoomIndex);
    const QPair<int, int> goal = {targetRoom.doorRow + 1, targetRoom.doorColumn};
    m_ghostPath = buildPath(startRow, startColumn, goal.first, goal.second);
}

QVector<QPair<int, int>> DormDefenseController::buildPath(int startRow,
                                                          int startColumn,
                                                          int goalRow,
                                                          int goalColumn) const
{
    static constexpr std::array<QPair<int, int>, 8> directions = {{
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    }};

    auto isCorridor = [this](int row, int column) {
        if (row < 0 || row >= RowCount || column < 0 || column >= ColumnCount)
            return false;
        return m_cells.at(cellIndex(row, column)).tileKind == TileKind::Corridor;
    };

    const QPair<int, int> start = isCorridor(startRow, startColumn)
        ? qMakePair(startRow, startColumn)
        : qMakePair(GhostSpawnRow, GhostSpawnColumn);
    const QPair<int, int> goal = qMakePair(goalRow, goalColumn);
    if (!isCorridor(start.first, start.second) || !isCorridor(goal.first, goal.second))
        return {};

    QVector<int> parents(RowCount * ColumnCount, -1);
    QVector<bool> visited(RowCount * ColumnCount, false);
    QVector<int> queue;
    queue.reserve(RowCount * ColumnCount);

    const int startIndex = cellIndex(start.first, start.second);
    const int goalIndex = cellIndex(goal.first, goal.second);
    visited[startIndex] = true;
    queue.append(startIndex);

    for (int head = 0; head < queue.size(); ++head) {
        const int currentIndex = queue.at(head);
        if (currentIndex == goalIndex)
            break;

        const int row = currentIndex / ColumnCount;
        const int column = currentIndex % ColumnCount;

        for (const auto &direction : directions) {
            const int nextRow = row + direction.first;
            const int nextColumn = column + direction.second;
            if (!isCorridor(nextRow, nextColumn))
                continue;

            if (direction.first != 0 && direction.second != 0) {
                if (!isCorridor(row, nextColumn) || !isCorridor(nextRow, column))
                    continue;
            }

            const int nextIndex = cellIndex(nextRow, nextColumn);
            if (visited.at(nextIndex))
                continue;

            visited[nextIndex] = true;
            parents[nextIndex] = currentIndex;
            queue.append(nextIndex);
        }
    }

    if (!visited.at(goalIndex))
        return {};

    QVector<QPair<int, int>> reversedPath;
    for (int index = goalIndex; index != -1; index = parents.at(index))
        reversedPath.append({index / ColumnCount, index % ColumnCount});

    QVector<QPair<int, int>> path;
    path.reserve(reversedPath.size());
    for (int index = reversedPath.size() - 1; index >= 0; --index)
        path.append(reversedPath.at(index));

    return path;
}

void DormDefenseController::spawnGhostForWave()
{
    m_ghostLevel = 1;
    m_ghostMaxHp = 60;
    m_ghostHp = m_ghostMaxHp;
    m_ghostAttack = 1;
    m_ghostDistance = qMax(0, m_ghostPath.size() - 1);
    m_ghostDoorHitCount = 0;
    m_respawnCountdown = 0;
    m_ticksOutOfCombat = 0;
    m_ghostMode = GhostMode::Assault;
    setTurretVolley({}, 0);
    emit ghostStateChanged();
}

void DormDefenseController::onTick()
{
    if (m_gameOver || m_roleSelectionRequired)
        return;

    ++m_elapsedTicks;
    const int localRoomIndex = currentPlayerRoomIndex();
    if (isAuthoritativeInstance()) {
        for (int roomIndex = 0; roomIndex < m_rooms.size(); ++roomIndex) {
            RoomState &room = m_rooms[roomIndex];
            room.gold += bedGoldOutputForLevel(room.bedLevel);
            room.power += 1 + roomGeneratorPowerOutput(roomIndex);
        }
    }
    if (!localControlsGhost()
        && localRoomIndex >= 0
        && localRoomIndex < m_rooms.size()) {
        syncPrimaryRoomStateFromMembers();
    }

    const bool aiBoardChanged = processAiRooms();
    if (ghostIsHumanControlled())
        syncPrimaryRoomStateFromMembers();

    if (!ghostIsHumanControlled() && m_timeRemaining <= 0) {
        if (currentTargetRoomIndex() < 0) {
            if (localRoomIndex >= 0
                && localRoomIndex < m_rooms.size()
                && m_rooms.at(localRoomIndex).active) {
                m_targetRoomIndex = localRoomIndex;
            } else {
                chooseRandomTargetRoom(false);
            }
        }
        if (m_ghostPath.isEmpty()) {
            initializeGhostPath(GhostSpawnRow, GhostSpawnColumn);
            m_ghostDistance = qMax(0, m_ghostPath.size() - 1);
        }
    }

    if (m_timeRemaining > 0) {
        --m_timeRemaining;
        if (m_timeRemaining == 0) {
            if (m_networked && isAuthoritativeInstance()) {
                assignMissingDefenderRooms();
            } else if (!localControlsGhost() && !playerRoomSelected()) {
                assignPlayerRoom(QRandomGenerator::global()->bounded(m_rooms.size()));
            }
        }
        setTurretVolley({}, 0);
        if (aiBoardChanged)
            emit boardChanged();
        emit resourcesChanged();
        emit statusChanged();
        emit ghostStateChanged();
        return;
    }

    if (ghostIsHumanControlled()) {
        if (!m_humanGhostAttackedDoorThisSecond && !m_humanGhostDamagedThisSecond) {
            ++m_ticksOutOfCombat;
        } else {
            m_ticksOutOfCombat = 0;
        }

        if (!m_humanGhostAttackedDoorThisSecond
            && !m_humanGhostDamagedThisSecond
            && m_ticksOutOfCombat >= GhostRecoveryDelayTicks
            && m_ghostHp < m_ghostMaxHp) {
            const int healAmount = qMax(GhostRecoveryMinHpPerTick, m_ghostMaxHp / 15);
            m_ghostHp = qMin(m_ghostMaxHp, m_ghostHp + healAmount);
            emit ghostStateChanged();
        }
        m_humanGhostDamagedThisSecond = false;
        m_humanGhostAttackedDoorThisSecond = false;
    } else if (m_ghostMode == GhostMode::Retreat) {
        QVariantList volley;
        int damage = 0;
        const QPair<int, int> ghostPosition = currentGhostPosition();
        if (ghostPosition.first >= 0 && ghostPosition.second >= 0) {
            volley = firingTurretsAtPoint(ghostCenterRow(), ghostCenterColumn());
            for (const QVariant &shotVariant : volley)
                damage += shotVariant.toMap().value(QStringLiteral("damage")).toInt();
        }
        setTurretVolley(volley, damage);

        if (damage > 0)
            m_ghostHp = qMax(0, m_ghostHp - damage);

        if (m_ghostMode == GhostMode::Retreat && damage == 0)
            ++m_ticksOutOfCombat;
        else
            m_ticksOutOfCombat = 0;

        if (m_ghostHp <= 0) {
            m_ghostHp = 0;
            updateStatus(QStringLiteral("The ghost has been defeated."));
            setGameFinished(true);
            emit resourcesChanged();
            emit statusChanged();
            return;
        }
        if (m_ghostDistance > 0) {
            --m_ghostDistance;
            emit ghostStateChanged();
        } else {
            if (m_ticksOutOfCombat >= GhostRecoveryDelayTicks && m_ghostHp < m_ghostMaxHp) {
                const int healAmount = qMax(GhostRecoveryMinHpPerTick, m_ghostMaxHp / 12);
                m_ghostHp = qMin(m_ghostMaxHp, m_ghostHp + healAmount);
                emit ghostStateChanged();
            }

            if (m_ghostHp >= m_ghostMaxHp) {
                m_ghostMode = GhostMode::Assault;
                m_ticksOutOfCombat = 0;
                chooseRandomTargetRoom(true);
                updateStatus(QStringLiteral("Ghost recovered and is attacking again."));
                emit ghostStateChanged();
            }
        }
    } else if (shouldGhostRetreat()) {
        startGhostRetreat();
    } else if (m_ghostDistance > 0) {
        QVariantList volley;
        int damage = 0;
        const QPair<int, int> ghostPosition = currentGhostPosition();
        if (ghostPosition.first >= 0 && ghostPosition.second >= 0) {
            volley = firingTurretsAtPoint(ghostCenterRow(), ghostCenterColumn());
            for (const QVariant &shotVariant : volley)
                damage += shotVariant.toMap().value(QStringLiteral("damage")).toInt();
        }
        setTurretVolley(volley, damage);

        if (damage > 0)
            m_ghostHp = qMax(0, m_ghostHp - damage);

        if (m_ghostHp <= 0) {
            m_ghostHp = 0;
            updateStatus(QStringLiteral("The ghost has been defeated."));
            setGameFinished(true);
            emit resourcesChanged();
            emit statusChanged();
            return;
        }
        --m_ghostDistance;
        emit ghostStateChanged();
    } else {
        QVariantList volley;
        int damage = 0;
        const QPair<int, int> ghostPosition = currentGhostPosition();
        if (ghostPosition.first >= 0 && ghostPosition.second >= 0) {
            volley = firingTurretsAtPoint(ghostCenterRow(), ghostCenterColumn());
            for (const QVariant &shotVariant : volley)
                damage += shotVariant.toMap().value(QStringLiteral("damage")).toInt();
        }
        setTurretVolley(volley, damage);

        if (damage > 0)
            m_ghostHp = qMax(0, m_ghostHp - damage);

        if (m_ghostHp <= 0) {
            m_ghostHp = 0;
            updateStatus(QStringLiteral("The ghost has been defeated."));
            setGameFinished(true);
            emit resourcesChanged();
            emit statusChanged();
            return;
        }
        const int targetRoomIndex = currentTargetRoomIndex();
        if (targetRoomIndex < 0 || targetRoomIndex >= m_rooms.size()) {
            chooseRandomTargetRoom(false);
            emit ghostStateChanged();
            if (aiBoardChanged)
                emit boardChanged();
            emit resourcesChanged();
            emit statusChanged();
            return;
        }

        RoomState &targetRoom = m_rooms[targetRoomIndex];
        targetRoom.doorHp = qMax(0, targetRoom.doorHp - m_ghostAttack);
        if (targetRoom.isPlayerRoom) {
            m_doorHp = targetRoom.doorHp;
        }

        if (targetRoom.doorHp <= 0) {
            clearRoomAfterDoorDestroyed(targetRoomIndex);
            if (!hasAnyActiveRoom()) {
                setGameFinished(false);
                return;
            }
            chooseRandomTargetRoom(true);
            emit boardChanged();
            emit ghostStateChanged();
            emit statusChanged();
            return;
        }

        ++m_ghostDoorHitCount;
        if (m_ghostDoorHitCount >= DoorHitsPerGhostLevel) {
            m_ghostDoorHitCount = 0;
            levelUpGhost(1, false);
        }
        emit boardChanged();
    }

    if (aiBoardChanged)
        emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
}

void DormDefenseController::scheduleGhostRespawn()
{
    m_respawnCountdown = 0;
}

void DormDefenseController::onCombatTick()
{
    if (!isAuthoritativeInstance()
        || !ghostIsHumanControlled()
        || m_gameOver
        || m_roleSelectionRequired
        || m_timeRemaining > 0) {
        return;
    }

    const qreal row = ghostCenterRow();
    const qreal column = ghostCenterColumn();
    if (row < 0 || column < 0 || !ghostVisible()) {
        m_humanGhostDoorRoomIndex = -1;
        m_humanGhostDoorChargeMs = 0;
        std::fill(m_turretChargeMs.begin(), m_turretChargeMs.end(), 0);
        std::fill(m_turretTargetInRange.begin(), m_turretTargetInRange.end(), false);
        setTurretVolley({}, 0);
        return;
    }

    if (processHumanGhostDoorCombat(CombatTickMs))
        return;

    QVariantList volley;
    int totalDamage = 0;

    for (int index = 0; index < m_cells.size(); ++index) {
        const CellData &cell = m_cells.at(index);
        if (cell.buildingKind != BuildingKind::Turret || cell.level <= 0)
            continue;

        const int turretRow = index / ColumnCount;
        const int turretColumn = index % ColumnCount;
        const int range = turretRangeForLevel(cell.level);
        const bool inRange = ghostWithinTurretRange(row, column, turretRow, turretColumn, range);

        if (index >= m_turretChargeMs.size() || index >= m_turretTargetInRange.size())
            continue;

        if (!inRange) {
            m_turretChargeMs[index] = 0;
            m_turretTargetInRange[index] = false;
            continue;
        }

        bool shouldFire = !m_turretTargetInRange.at(index);
        m_turretTargetInRange[index] = true;
        if (shouldFire) {
            m_turretChargeMs[index] = 0;
        } else {
            m_turretChargeMs[index] += CombatTickMs;
            if (m_turretChargeMs[index] >= TurretAttackIntervalMs) {
                m_turretChargeMs[index] -= TurretAttackIntervalMs;
                shouldFire = true;
            }
        }
        if (!shouldFire)
            continue;

        const int shotDamage = turretDamageForLevel(cell.level);
        QVariantMap shot;
        shot[QStringLiteral("row")] = turretRow;
        shot[QStringLiteral("column")] = turretColumn;
        shot[QStringLiteral("damage")] = shotDamage;
        shot[QStringLiteral("level")] = cell.level;
        volley.append(shot);
        totalDamage += shotDamage;
    }

    setTurretVolley(volley, totalDamage);
    if (totalDamage > 0) {
        m_ghostHp = qMax(0, m_ghostHp - totalDamage);
        m_humanGhostDamagedThisSecond = true;
        m_ticksOutOfCombat = 0;
        emit ghostStateChanged();
        emit statusChanged();
        if (m_ghostHp <= 0) {
            updateStatus(QStringLiteral("The ghost has been defeated."));
            setGameFinished(true);
            return;
        }
    }

}

bool DormDefenseController::processAiRooms()
{
    bool boardChanged = false;
    const bool preparationPhase = m_timeRemaining > 0;
    const bool authoritativeNetworkView = m_networked && isAuthoritativeInstance();

    for (int roomIndex = 0; roomIndex < m_rooms.size(); ++roomIndex) {
        RoomState &room = m_rooms[roomIndex];
        if (!room.active) {
            continue;
        }

        const bool humanDefenderRoom = ghostIsHumanControlled() && roomHasHumanDefender(roomIndex);
        const bool offlineHumanDefenderRoom = !m_networked && !ghostIsHumanControlled() && room.isPlayerRoom;

        if (authoritativeNetworkView && !ghostIsHumanControlled() && roomHasHumanDefender(roomIndex))
            continue;

        if (humanDefenderRoom || offlineHumanDefenderRoom)
            continue;

        const bool underDirectAttack = currentTargetRoomIndex() == roomIndex && m_ghostMode == GhostMode::Assault;
        int actionsRemaining = preparationPhase ? 3 : (underDirectAttack ? 3 : 2);

        while (actionsRemaining-- > 0) {
            const int generatorCount = roomGeneratorCount(roomIndex);
            const int turretCount = roomTurretCount(roomIndex);
            const int nextDoorGold = room.doorMaxHp * 2;
            const int nextBedGold = doublingCost(25, room.bedLevel + 1);
            const int currentDoorLevel = doorLevelForHp(room.doorMaxHp);
            const int requiredDoorLevel = requiredDoorLevelForRoom(roomIndex);
            const bool doorInDanger = room.doorHp <= room.doorMaxHp * (underDirectAttack ? 7 : 4) / 10;
            const bool doorBelowRequired = currentDoorLevel < requiredDoorLevel;
            bool acted = false;

            if (doorBelowRequired) {
                if (room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex)) {
                    boardChanged = true;
                    continue;
                }
                break;
            }

            if (preparationPhase) {
                if (room.bedLevel < 2 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (room.bedLevel >= currentDoorLevel + 1 && room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (turretCount < 1 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
                else if (room.bedLevel < 3 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (currentDoorLevel < 2 && room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (generatorCount < 1 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Generator))
                    acted = true;
                else if (room.bedLevel < 4 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (room.gold >= nextDoorGold && currentDoorLevel < room.bedLevel && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
            } else if (underDirectAttack) {
                if (doorInDanger && room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (room.bedLevel > currentDoorLevel && room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (turretCount < 2 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
                else if (room.bedLevel < 4 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (generatorCount < 1 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Generator))
                    acted = true;
                else if (tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
            } else {
                if (room.bedLevel < 2 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (room.bedLevel > currentDoorLevel && room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (turretCount < 1 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
                else if (room.bedLevel < 3 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (currentDoorLevel < 2 && room.gold >= nextDoorGold && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (generatorCount < 1 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Generator))
                    acted = true;
                else if (room.bedLevel < 4 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (room.gold >= nextDoorGold && currentDoorLevel < room.bedLevel && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (generatorCount < 2 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Generator))
                    acted = true;
                else if (turretCount < 2 && tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
                else if (room.bedLevel < 6 && room.gold >= nextBedGold && tryAiUpgradeBed(roomIndex))
                    acted = true;
                else if (room.gold >= nextDoorGold && currentDoorLevel < room.bedLevel && tryAiUpgradeDoor(roomIndex))
                    acted = true;
                else if (tryAiBuildOrUpgrade(roomIndex, BuildingKind::Turret))
                    acted = true;
            }

            if (!acted)
                break;

            boardChanged = true;
        }
    }

    return boardChanged;
}

void DormDefenseController::startGhostRetreat()
{
    const QPair<int, int> currentPosition = currentGhostPosition();
    const QVector<QPair<int, int>> retreatPath = buildPath(currentPosition.first,
                                                           currentPosition.second,
                                                           GhostSpawnRow,
                                                           GhostSpawnColumn);
    if (retreatPath.isEmpty())
        return;

    m_ghostMode = GhostMode::Retreat;
    m_ghostPath = retreatPath;
    m_ghostDistance = qMax(0, m_ghostPath.size() - 1);
    m_ticksOutOfCombat = 0;
    updateStatus(QStringLiteral("Ghost is retreating before the turrets finish it off."));
    emit ghostStateChanged();
}

void DormDefenseController::chooseRandomTargetRoom(bool preferDifferentRoom)
{
    const QPair<int, int> previousPosition = currentGhostPosition();
    QVector<int> candidates;
    candidates.reserve(m_rooms.size());

    for (int index = 0; index < m_rooms.size(); ++index) {
        if (!m_rooms.at(index).active)
            continue;
        candidates.append(index);
    }

    if (candidates.isEmpty()) {
        m_targetRoomIndex = -1;
        m_ghostPath.clear();
        return;
    }

    if (preferDifferentRoom && candidates.size() > 1)
        candidates.removeAll(m_targetRoomIndex);

    m_targetRoomIndex = candidates.at(QRandomGenerator::global()->bounded(candidates.size()));
    initializeGhostPath(previousPosition.first, previousPosition.second);
    m_ghostDistance = qMax(0, m_ghostPath.size() - 1);
    m_ghostMode = GhostMode::Assault;
}

void DormDefenseController::levelUpGhost(int levelDelta, bool restoreHealth)
{
    if (levelDelta <= 0 || m_gameOver || m_ghostHp <= 0)
        return;

    m_ghostLevel += levelDelta;
    const int gainedMaxHp = 8 * levelDelta;
    m_ghostMaxHp += gainedMaxHp;
    m_ghostAttack += levelDelta;

    if (restoreHealth)
        m_ghostHp = m_ghostMaxHp;

    m_lastActionMessage = QStringLiteral("Ghost reached Lv.%1. HP and attack increased.")
        .arg(m_ghostLevel);
    emit ghostStateChanged();
    emit statusChanged();
}

void DormDefenseController::setGameFinished(bool playerWon)
{
    if (m_gameOver)
        return;

    m_tickTimer.stop();
    m_combatTimer.stop();
    m_gameOver = true;
    m_winner = playerWon ? 1 : 2;
    if (localControlsGhost()) {
        m_lastActionMessage = playerWon
            ? QStringLiteral("The defenders survived. The ghost has been repelled.")
            : QStringLiteral("The ghost broke the door and won the match.");
    } else {
        m_lastActionMessage = playerWon
            ? QStringLiteral("The ghost was driven away. You win.")
            : QStringLiteral("The door broke. Build defenses earlier next round.");
    }
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    emit ghostStateChanged();
    emit gameOverChanged();
}

void DormDefenseController::finalizeGameOver(int winner)
{
    if (winner != 1 && winner != 2)
        return;

    if (m_gameOver && m_winner == winner)
        return;

    m_tickTimer.stop();
    m_combatTimer.stop();
    m_gameOver = true;
    m_winner = winner;
    const bool defendersWon = winner == 1;
    if (localControlsGhost()) {
        m_lastActionMessage = defendersWon
            ? QStringLiteral("The defenders held the line. The ghost has been defeated.")
            : QStringLiteral("The ghost broke the door and won the match.");
    } else {
        m_lastActionMessage = defendersWon
            ? QStringLiteral("The ghost has been defeated. You win.")
            : QStringLiteral("The dorm fell. The ghost won the match.");
    }
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    emit ghostStateChanged();
    emit gameOverChanged();
}

void DormDefenseController::updateStatus(const QString &message)
{
    if (!message.isEmpty())
        m_lastActionMessage = message;
    emit statusChanged();
}

void DormDefenseController::emitAllChanges()
{
    emit boardChanged();
    emit resourcesChanged();
    emit statusChanged();
    emit ghostStateChanged();
    emit turretVolleyChanged();
    emit gameOverChanged();
}

void DormDefenseController::syncPrimaryRoomStateFromMembers()
{
    const int roomIndex = currentPlayerRoomIndex();
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return;

    const RoomState &room = m_rooms.at(roomIndex);
    m_gold = room.gold;
    m_power = room.power;
    m_bedLevel = room.bedLevel;
    m_doorHp = room.doorHp;
    m_doorMaxHp = room.doorMaxHp;
}

void DormDefenseController::syncMembersFromPrimaryRoomState()
{
    const int roomIndex = currentPlayerRoomIndex();
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return;

    RoomState &room = m_rooms[roomIndex];
    room.gold = m_gold;
    room.power = m_power;
    room.bedLevel = m_bedLevel;
    room.doorHp = m_doorHp;
    room.doorMaxHp = m_doorMaxHp;
}

void DormDefenseController::clearRoomAfterDoorDestroyed(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return;

    RoomState &room = m_rooms[roomIndex];
    room.active = false;
    room.gold = 0;
    room.power = 0;

    if (m_playerRoomIndex == roomIndex) {
        const int localPlayerId = m_networked ? m_sessionLocalPlayerId : 0;
        if (localPlayerId >= 0 && !localControlsGhost())
            m_eliminatedDefenderPlayerIds.insert(localPlayerId);
        m_playerRoomIndex = -1;
    }

    for (auto it = m_playerRoomByPlayerId.begin(); it != m_playerRoomByPlayerId.end();) {
        if (it.value() == roomIndex) {
            if (m_networkRolesByPlayerId.value(it.key(), LocalRole::Defender) == LocalRole::Defender)
                m_eliminatedDefenderPlayerIds.insert(it.key());
            it = m_playerRoomByPlayerId.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto &slot : roomBuildSlots(roomIndex)) {
        CellData &cell = m_cells[cellIndex(slot.first, slot.second)];
        cell.buildingKind = BuildingKind::None;
        cell.level = 0;
    }

    applyRoomOwnershipPresentation();
}

bool DormDefenseController::applyHumanGhostDoorDamage(int roomIndex, int damage)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size() || damage <= 0)
        return false;

    RoomState &targetRoom = m_rooms[roomIndex];
    targetRoom.doorHp = qMax(0, targetRoom.doorHp - damage);
    if (targetRoom.isPlayerRoom) {
        m_doorHp = targetRoom.doorHp;
    }

    if (targetRoom.doorHp <= 0) {
        clearRoomAfterDoorDestroyed(roomIndex);
        if (!hasAnyActiveRoom()) {
            setGameFinished(false);
            return true;
        }
    }

    emit boardChanged();
    return false;
}

bool DormDefenseController::damageHumanGhostDoor(int roomIndex)
{
    if (applyHumanGhostDoorDamage(roomIndex, qMax(1, m_ghostAttack)))
        return true;

    ++m_ghostDoorHitCount;
    if (m_ghostDoorHitCount >= DoorHitsPerGhostLevel) {
        m_ghostDoorHitCount = 0;
        levelUpGhost(1, false);
    }
    return false;
}

bool DormDefenseController::processHumanGhostDoorCombat(int elapsedMs)
{
    const int roomIndex = roomIndexForGhostAttackPoint(m_ghostCenterRow, m_ghostCenterColumn);
    if (roomIndex < 0) {
        m_humanGhostDoorRoomIndex = -1;
        m_humanGhostDoorChargeMs = 0;
        return false;
    }

    m_humanGhostAttackedDoorThisSecond = true;
    m_ticksOutOfCombat = 0;
    if (roomIndex != m_humanGhostDoorRoomIndex) {
        m_humanGhostDoorRoomIndex = roomIndex;
        m_humanGhostDoorChargeMs = 0;
        return damageHumanGhostDoor(roomIndex);
    }

    m_humanGhostDoorChargeMs += qMax(0, elapsedMs);
    while (m_humanGhostDoorChargeMs >= GhostDoorAttackIntervalMs) {
        m_humanGhostDoorChargeMs -= GhostDoorAttackIntervalMs;
        if (damageHumanGhostDoor(roomIndex))
            return true;
    }
    return false;
}

bool DormDefenseController::moveGhostToNetworkPosition(qreal row, qreal column)
{
    if (!localControlsGhost() || m_gameOver || m_roleSelectionRequired || m_timeRemaining > 0)
        return false;
    if (!qIsFinite(row) || !qIsFinite(column) || !isCorridorPoint(row, column))
        return false;

    m_ghostMode = GhostMode::Assault;
    m_ticksOutOfCombat = 0;
    m_ghostCenterRow = row;
    m_ghostCenterColumn = column;
    m_ghostPath = {qMakePair(ghostRow(), ghostColumn())};
    m_ghostDistance = 0;
    processHumanGhostDoorCombat(0);
    emit ghostStateChanged();
    return true;
}

void DormDefenseController::queueGhostNetworkPositionSync()
{
    if (!m_ghostNetworkSyncTimer.isActive())
        m_ghostNetworkSyncTimer.start();
}

bool DormDefenseController::hasAnyActiveRoom() const
{
    for (const RoomState &room : m_rooms) {
        if (room.active)
            return true;
    }
    return false;
}

int DormDefenseController::playerRoomIndexForPlayer(int playerId) const
{
    const int roomIndex = m_playerRoomByPlayerId.value(playerId, -1);
    return roomIndex >= 0 && roomIndex < m_rooms.size() ? roomIndex : -1;
}

bool DormDefenseController::roomAssignedToAnotherPlayer(int roomIndex, int playerId) const
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;

    for (auto it = m_playerRoomByPlayerId.constBegin(); it != m_playerRoomByPlayerId.constEnd(); ++it) {
        if (it.key() == playerId)
            continue;
        if (it.value() != roomIndex)
            continue;
        if (m_networkRolesByPlayerId.value(it.key(), LocalRole::Defender) == LocalRole::Defender)
            return true;
    }

    return false;
}

bool DormDefenseController::roomHasHumanDefender(int roomIndex) const
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;

    for (auto it = m_playerRoomByPlayerId.constBegin(); it != m_playerRoomByPlayerId.constEnd(); ++it) {
        if (it.value() != roomIndex)
            continue;
        if (m_networkRolesByPlayerId.value(it.key(), LocalRole::Defender) == LocalRole::Defender)
            return true;
    }

    return false;
}

void DormDefenseController::assignMissingDefenderRooms()
{
    if (!m_networked)
        return;

    const int previousLocalRoomIndex = currentPlayerRoomIndex();
    QVector<int> freeRooms;
    for (int roomIndex = 0; roomIndex < m_rooms.size(); ++roomIndex) {
        if (roomAssignedToAnotherPlayer(roomIndex, -1))
            continue;
        freeRooms.append(roomIndex);
    }

    QVector<int> playerIds = m_networkRolesByPlayerId.keys().toVector();
    std::sort(playerIds.begin(), playerIds.end());
    for (int playerId : playerIds) {
        if (m_networkRolesByPlayerId.value(playerId, LocalRole::Spectator) != LocalRole::Defender)
            continue;
        if (playerRoomIndexForPlayer(playerId) >= 0 || freeRooms.isEmpty())
            continue;

        const int selectedIndex = QRandomGenerator::global()->bounded(freeRooms.size());
        const int roomIndex = freeRooms.takeAt(selectedIndex);
        m_playerRoomByPlayerId.insert(playerId, roomIndex);
    }

    for (RoomState &room : m_rooms)
        room.active = true;

    if (m_targetRoomIndex < 0
        || m_targetRoomIndex >= m_rooms.size()
        || !m_rooms.at(m_targetRoomIndex).active) {
        chooseRandomTargetRoom(false);
    }

    if (!localControlsGhost()) {
        m_playerRoomIndex = playerRoomIndexForPlayer(m_sessionLocalPlayerId);
        applyRoomOwnershipPresentation();
        syncPrimaryRoomStateFromMembers();
        if (m_playerRoomIndex != previousLocalRoomIndex) {
            emit boardChanged();
            emit resourcesChanged();
        }
    }
}

void DormDefenseController::prunePlayerRoomAssignments()
{
    QHash<int, int>::iterator it = m_playerRoomByPlayerId.begin();
    while (it != m_playerRoomByPlayerId.end()) {
        if (it.value() < 0 || it.value() >= m_rooms.size()
            || m_networkRolesByPlayerId.value(it.key(), LocalRole::Spectator) != LocalRole::Defender) {
            it = m_playerRoomByPlayerId.erase(it);
            continue;
        }
        ++it;
    }

    QVector<int> playerIds = m_playerRoomByPlayerId.keys().toVector();
    std::sort(playerIds.begin(), playerIds.end());
    QVector<int> usedRooms;
    for (int playerId : playerIds) {
        const int roomIndex = m_playerRoomByPlayerId.value(playerId, -1);
        if (usedRooms.contains(roomIndex)) {
            m_playerRoomByPlayerId.remove(playerId);
            continue;
        }
        usedRooms.append(roomIndex);
    }
}

void DormDefenseController::setTurretVolley(const QVariantList &volley, int totalDamage)
{
    m_turretVolley = volley;
    m_lastTurretDamage = totalDamage;
    if (totalDamage > 0)
        ++m_turretVolleySerial;
    emit turretVolleyChanged();
}

int DormDefenseController::cellIndex(int row, int column) const
{
    return row * ColumnCount + column;
}

bool DormDefenseController::isBuildableCell(int row, int column) const
{
    if (row < 0 || row >= RowCount || column < 0 || column >= ColumnCount)
        return false;
    const CellData &cell = m_cells.at(cellIndex(row, column));
    if (cell.tileKind == TileKind::Buildable)
        return true;

    return cell.tileKind == TileKind::OtherRoom
        && roomIndexForCell(row, column) == currentPlayerRoomIndex();
}

int DormDefenseController::roomIndexForCell(int row, int column) const
{
    for (int index = 0; index < m_rooms.size(); ++index) {
        const RoomState &room = m_rooms.at(index);
        const int relativeRow = row - room.top;
        const int relativeColumn = column - room.left;
        const bool inMainArea = relativeRow >= 0 && relativeRow <= 2
            && relativeColumn >= 0 && relativeColumn <= 5;
        const bool inLowerArea = relativeRow >= 3 && relativeRow <= 5
            && relativeColumn >= 3 && relativeColumn <= 5;
        const bool isDoor = row == room.doorRow && column == room.doorColumn;
        const bool isBed = row == room.bedRow && column == room.bedColumn;
        if (inMainArea || inLowerArea || isDoor || isBed)
            return index;
    }

    return -1;
}

int DormDefenseController::currentTargetRoomIndex() const
{
    if (m_targetRoomIndex >= 0 && m_targetRoomIndex < m_rooms.size()
        && m_rooms.at(m_targetRoomIndex).active) {
        return m_targetRoomIndex;
    }

    return -1;
}

int DormDefenseController::currentPlayerRoomIndex() const
{
    if (m_playerRoomIndex >= 0 && m_playerRoomIndex < m_rooms.size())
        return m_playerRoomIndex;

    const int mappedRoomIndex = playerRoomIndexForPlayer(m_sessionLocalPlayerId);
    return mappedRoomIndex >= 0 ? mappedRoomIndex : -1;
}

QPair<int, int> DormDefenseController::currentGhostPosition() const
{
    if (!ghostVisible())
        return qMakePair(-1, -1);

    if (ghostIsHumanControlled()) {
        return qMakePair(qBound(0, static_cast<int>(qFloor(m_ghostCenterRow)), RowCount - 1),
                         qBound(0, static_cast<int>(qFloor(m_ghostCenterColumn)), ColumnCount - 1));
    }

    const int pathIndex = qBound(0,
                                 m_ghostPath.size() - 1 - m_ghostDistance,
                                 m_ghostPath.size() - 1);
    return m_ghostPath.at(pathIndex);
}

bool DormDefenseController::ghostOccupiesCell(int row, int column) const
{
    const QPair<int, int> position = currentGhostPosition();
    if (position.first < 0 || position.second < 0)
        return false;
    return position.first == row && position.second == column;
}

bool DormDefenseController::isCorridorCell(int row, int column) const
{
    if (row < 0 || row >= RowCount || column < 0 || column >= ColumnCount)
        return false;
    return m_cells.at(cellIndex(row, column)).tileKind == TileKind::Corridor;
}

bool DormDefenseController::isCorridorPoint(qreal row, qreal column) const
{
    const int cellRow = static_cast<int>(qFloor(row));
    const int cellColumn = static_cast<int>(qFloor(column));
    if (isCorridorCell(cellRow, cellColumn))
        return true;

    constexpr qreal tolerance = 0.045;
    const qreal localRow = row - cellRow;
    const qreal localColumn = column - cellColumn;

    if (localRow <= tolerance && isCorridorCell(cellRow - 1, cellColumn))
        return true;
    if (localRow >= 1.0 - tolerance && isCorridorCell(cellRow + 1, cellColumn))
        return true;
    if (localColumn <= tolerance && isCorridorCell(cellRow, cellColumn - 1))
        return true;
    if (localColumn >= 1.0 - tolerance && isCorridorCell(cellRow, cellColumn + 1))
        return true;

    return false;
}

int DormDefenseController::roomIndexForGhostAttackPosition(int row, int column) const
{
    return roomIndexForGhostAttackPoint(row + 0.5, column + 0.5);
}

int DormDefenseController::roomIndexForGhostAttackPoint(qreal row, qreal column) const
{
    for (int index = 0; index < m_rooms.size(); ++index) {
        const RoomState &room = m_rooms.at(index);
        if (!room.active)
            continue;

        const int attackRow = room.doorRow + 1;
        const int attackColumn = room.doorColumn;
        if (ghostTouchesCell(row, column, attackRow, attackColumn)) {
            return index;
        }
    }
    return -1;
}

int DormDefenseController::turretCoverageCountAtPoint(qreal row, qreal column) const
{
    int count = 0;
    for (int index = 0; index < m_cells.size(); ++index) {
        const CellData &cell = m_cells.at(index);
        if (cell.buildingKind != BuildingKind::Turret || cell.level <= 0)
            continue;

        const int turretRow = index / ColumnCount;
        const int turretColumn = index % ColumnCount;
        const int range = turretRangeForLevel(cell.level);
        if (ghostWithinTurretRange(row, column, turretRow, turretColumn, range))
            ++count;
    }
    return count;
}

QVector<QPair<int, int>> DormDefenseController::roomBuildSlots(int roomIndex) const
{
    QVector<QPair<int, int>> buildSlots;
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return buildSlots;

    for (int row = 0; row < RowCount; ++row) {
        for (int column = 0; column < ColumnCount; ++column) {
            if (roomIndexForCell(row, column) != roomIndex)
                continue;

            const CellData &cell = m_cells.at(cellIndex(row, column));
            if (cell.tileKind == TileKind::Wall
                || cell.tileKind == TileKind::Corridor
                || cell.tileKind == TileKind::Door
                || cell.tileKind == TileKind::OtherDoor
                || cell.tileKind == TileKind::Bed
                || cell.tileKind == TileKind::OtherBed) {
                continue;
            }

            buildSlots.append({row, column});
        }
    }

    return buildSlots;
}

int DormDefenseController::buildingCount(int roomIndex, BuildingKind buildingKind) const
{
    int total = 0;
    for (const auto &slot : roomBuildSlots(roomIndex)) {
        if (m_cells.at(cellIndex(slot.first, slot.second)).buildingKind == buildingKind)
            ++total;
    }
    return total;
}

int DormDefenseController::doorLevelForHp(int doorMaxHp) const
{
    int level = 1;
    int hp = InitialDoorHp;
    while (hp < doorMaxHp) {
        hp *= 2;
        ++level;
    }
    return level;
}

int DormDefenseController::maxBedLevelForDoorLevel(int doorLevel) const
{
    static constexpr std::array<int, 6> table = {2, 3, 4, 5, 6, 6};
    return tableValue(table, doorLevel);
}

int DormDefenseController::requiredDoorLevelForBedLevel(int bedLevel) const
{
    static constexpr std::array<int, 6> table = {1, 1, 2, 3, 4, 5};
    return tableValue(table, bedLevel);
}

int DormDefenseController::requiredBedLevelForBuilding(BuildingKind buildingKind, int nextLevel) const
{
    switch (buildingKind) {
    case BuildingKind::Generator: {
        static constexpr std::array<int, 6> table = {3, 4, 5, 6, 6, 6};
        return tableValue(table, nextLevel);
    }
    case BuildingKind::Turret: {
        static constexpr std::array<int, 6> table = {1, 2, 3, 4, 5, 6};
        return tableValue(table, nextLevel);
    }
    case BuildingKind::None:
    default:
        return 1;
    }
}

int DormDefenseController::requiredDoorLevelForBuilding(BuildingKind buildingKind, int nextLevel) const
{
    switch (buildingKind) {
    case BuildingKind::Generator: {
        static constexpr std::array<int, 6> table = {2, 2, 3, 4, 5, 6};
        return tableValue(table, nextLevel);
    }
    case BuildingKind::Turret: {
        static constexpr std::array<int, 6> table = {1, 1, 2, 3, 4, 5};
        return tableValue(table, nextLevel);
    }
    case BuildingKind::None:
    default:
        return 1;
    }
}

int DormDefenseController::requiredDoorLevelForRoom(int roomIndex) const
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return 1;

    const RoomState &room = m_rooms.at(roomIndex);
    int requiredLevel = requiredDoorLevelForBedLevel(room.bedLevel);

    for (const auto &slot : roomBuildSlots(roomIndex)) {
        const CellData &cell = m_cells.at(cellIndex(slot.first, slot.second));
        if (cell.buildingKind == BuildingKind::None || cell.level <= 0)
            continue;
        requiredLevel = qMax(requiredLevel,
                             requiredDoorLevelForBuilding(cell.buildingKind, cell.level));
    }

    return requiredLevel;
}

bool DormDefenseController::tryAiRepairDoor(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;

    RoomState &room = m_rooms[roomIndex];
    if (!room.active || room.doorHp >= room.doorMaxHp)
        return false;

    const int goldCost = 18 + m_ghostLevel * 6;
    const int powerCost = 4 + m_ghostLevel;
    if (room.gold < goldCost || room.power < powerCost)
        return false;

    room.gold -= goldCost;
    room.power -= powerCost;
    room.doorHp = qMin(room.doorMaxHp, room.doorHp + 28 + room.bedLevel * 4);
    return true;
}

bool DormDefenseController::tryAiUpgradeDoor(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;

    RoomState &room = m_rooms[roomIndex];
    if (!room.active)
        return false;

    if (doorLevelForHp(room.doorMaxHp) >= 6)
        return false;

    const int goldCost = room.doorMaxHp * 2;
    if (room.gold < goldCost)
        return false;

    room.gold -= goldCost;
    room.doorMaxHp *= 2;
    room.doorHp = room.doorMaxHp;
    return true;
}

bool DormDefenseController::tryAiUpgradeBed(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size())
        return false;

    RoomState &room = m_rooms[roomIndex];
    if (!room.active)
        return false;

    const int nextLevel = room.bedLevel + 1;
    if (nextLevel > 6)
        return false;

    if (doorLevelForHp(room.doorMaxHp) < requiredDoorLevelForBedLevel(nextLevel))
        return false;

    const int goldCost = doublingCost(25, nextLevel);
    if (room.gold < goldCost)
        return false;

    room.gold -= goldCost;
    room.bedLevel = nextLevel;
    return true;
}

bool DormDefenseController::tryAiBuildOrUpgrade(int roomIndex, BuildingKind buildingKind)
{
    if (roomIndex < 0 || roomIndex >= m_rooms.size() || buildingKind == BuildingKind::None)
        return false;

    RoomState &room = m_rooms[roomIndex];
    if (!room.active)
        return false;

    const QVector<QPair<int, int>> buildSlots = roomBuildSlots(roomIndex);
    const int currentDoorLevel = doorLevelForHp(room.doorMaxHp);

    for (const auto &slot : buildSlots) {
        CellData &cell = m_cells[cellIndex(slot.first, slot.second)];
        if (cell.buildingKind != BuildingKind::None)
            continue;

        const int nextLevel = 1;
        if (currentDoorLevel < requiredDoorLevelForBuilding(buildingKind, nextLevel))
            continue;
        if (room.bedLevel < requiredBedLevelForBuilding(buildingKind, nextLevel))
            continue;

        const int goldCost = buildingGoldCost(buildingKind, nextLevel);
        const int powerCost = buildingPowerCost(buildingKind, nextLevel);
        if (room.gold < goldCost || room.power < powerCost)
            continue;

        room.gold -= goldCost;
        room.power -= powerCost;
        cell.buildingKind = buildingKind;
        cell.level = nextLevel;
        return true;
    }

    int bestSlotIndex = -1;
    int bestLevel = std::numeric_limits<int>::max();

    for (int slotIndex = 0; slotIndex < buildSlots.size(); ++slotIndex) {
        const auto &slot = buildSlots.at(slotIndex);
        CellData &cell = m_cells[cellIndex(slot.first, slot.second)];
        if (cell.buildingKind != buildingKind)
            continue;

        const int nextLevel = cell.level + 1;
        if (nextLevel > 6)
            continue;

        if (currentDoorLevel < requiredDoorLevelForBuilding(buildingKind, nextLevel))
            continue;
        if (room.bedLevel < requiredBedLevelForBuilding(buildingKind, nextLevel))
            continue;

        const int goldCost = buildingGoldCost(buildingKind, nextLevel);
        const int powerCost = buildingPowerCost(buildingKind, nextLevel);
        if (room.gold < goldCost || room.power < powerCost)
            continue;

        if (cell.level < bestLevel) {
            bestLevel = cell.level;
            bestSlotIndex = slotIndex;
        }
    }

    if (bestSlotIndex >= 0) {
        const auto &slot = buildSlots.at(bestSlotIndex);
        CellData &cell = m_cells[cellIndex(slot.first, slot.second)];
        const int nextLevel = cell.level + 1;
        room.gold -= buildingGoldCost(buildingKind, nextLevel);
        room.power -= buildingPowerCost(buildingKind, nextLevel);
        cell.level = nextLevel;
        return true;
    }

    return false;
}

int DormDefenseController::roomGeneratorCount(int roomIndex) const
{
    return buildingCount(roomIndex, BuildingKind::Generator);
}

int DormDefenseController::roomTurretCount(int roomIndex) const
{
    return buildingCount(roomIndex, BuildingKind::Turret);
}

int DormDefenseController::roomGeneratorGoldOutput(int roomIndex) const
{
    int total = 0;
    for (const auto &slot : roomBuildSlots(roomIndex)) {
        const CellData &cell = m_cells.at(cellIndex(slot.first, slot.second));
        if (cell.buildingKind == BuildingKind::Generator)
            total += 0;
    }
    return total;
}

int DormDefenseController::roomGeneratorPowerOutput(int roomIndex) const
{
    int total = 0;
    for (const auto &slot : roomBuildSlots(roomIndex)) {
        const CellData &cell = m_cells.at(cellIndex(slot.first, slot.second));
        if (cell.buildingKind == BuildingKind::Generator)
            total += 1 << qMax(0, cell.level - 1);
    }
    return total;
}

QVariantList DormDefenseController::firingTurretsAt(int row, int column) const
{
    return firingTurretsAtPoint(row + 0.5, column + 0.5);
}

QVariantList DormDefenseController::firingTurretsAtPoint(qreal row, qreal column) const
{
    QVariantList shots;

    for (int index = 0; index < m_cells.size(); ++index) {
        const CellData &cell = m_cells.at(index);
        if (cell.buildingKind != BuildingKind::Turret || cell.level <= 0)
            continue;

        const int turretRow = index / ColumnCount;
        const int turretColumn = index % ColumnCount;
        const int range = turretRangeForLevel(cell.level);
        if (!ghostWithinTurretRange(row, column, turretRow, turretColumn, range))
            continue;

        QVariantMap shot;
        shot[QStringLiteral("row")] = turretRow;
        shot[QStringLiteral("column")] = turretColumn;
        shot[QStringLiteral("damage")] = turretDamageForLevel(cell.level);
        shot[QStringLiteral("level")] = cell.level;
        shots.append(shot);
    }

    return shots;
}

int DormDefenseController::generatorGoldOutput() const
{
    int total = 0;
    for (int index = 0; index < m_cells.size(); ++index) {
        const CellData &cell = m_cells.at(index);
        if (cell.buildingKind != BuildingKind::Generator)
            continue;

        const int ownerRoomIndex = roomIndexForCell(index / ColumnCount, index % ColumnCount);
        if (ownerRoomIndex < 0 || !m_rooms.at(ownerRoomIndex).isPlayerRoom)
            continue;

            total += 0;
    }
    return total;
}

int DormDefenseController::generatorPowerOutput() const
{
    int total = 0;
    for (int index = 0; index < m_cells.size(); ++index) {
        const CellData &cell = m_cells.at(index);
        if (cell.buildingKind != BuildingKind::Generator)
            continue;

        const int ownerRoomIndex = roomIndexForCell(index / ColumnCount, index % ColumnCount);
        if (ownerRoomIndex < 0 || !m_rooms.at(ownerRoomIndex).isPlayerRoom)
            continue;

            total += 1 << qMax(0, cell.level - 1);
    }
    return total;
}

int DormDefenseController::turretDamageAt(int row, int column) const
{
    return turretDamageAtPoint(row + 0.5, column + 0.5);
}

int DormDefenseController::turretDamageAtPoint(qreal row, qreal column) const
{
    int total = 0;
    for (int index = 0; index < m_cells.size(); ++index) {
        const CellData &cell = m_cells.at(index);
        if (cell.buildingKind != BuildingKind::Turret || cell.level <= 0)
            continue;

        const int turretRow = index / ColumnCount;
        const int turretColumn = index % ColumnCount;
        const int range = turretRangeForLevel(cell.level);
        if (ghostWithinTurretRange(row, column, turretRow, turretColumn, range))
            total += turretDamageForLevel(cell.level);
    }
    return total;
}

int DormDefenseController::turretDamageOutput() const
{
    const qreal row = ghostCenterRow();
    const qreal column = ghostCenterColumn();
    if (row < 0 || column < 0)
        return 0;
    int total = 0;
    for (const QVariant &shotVariant : firingTurretsAtPoint(row, column))
        total += shotVariant.toMap().value(QStringLiteral("damage")).toInt();
    return total;
}

int DormDefenseController::projectedRetreatDamage(const QVector<QPair<int, int>> &path) const
{
    int total = 0;
    for (int index = 1; index < path.size(); ++index)
        total += turretDamageAt(path.at(index).first, path.at(index).second);
    return total;
}

bool DormDefenseController::shouldGhostRetreat() const
{
    const QPair<int, int> position = currentGhostPosition();
    if (position.first < 0 || position.second < 0)
        return false;

    // Travelling to a room, including immediately after breaking a door,
    // must never be replaced with a path back to the spawn point.
    if (m_ghostDistance > 0)
        return false;

    const QVector<QPair<int, int>> retreatPath = buildPath(position.first,
                                                           position.second,
                                                           GhostSpawnRow,
                                                           GhostSpawnColumn);
    if (retreatPath.isEmpty())
        return false;

    const int retreatDamage = projectedRetreatDamage(retreatPath);
    const int safetyHp = qMax(GhostRetreatSafetyBuffer, m_ghostMaxHp / 14);
    if (m_ghostHp <= retreatDamage + safetyHp)
        return true;

    if (m_ghostDistance == 0) {
        const int targetRoomIndex = currentTargetRoomIndex();
        if (targetRoomIndex < 0 || targetRoomIndex >= m_rooms.size())
            return false;

        const RoomState &targetRoom = m_rooms.at(targetRoomIndex);
        const int hitsToBreakDoor = qMax(1, (targetRoom.doorHp + m_ghostAttack - 1) / m_ghostAttack);
        const int projectedFightDamage = hitsToBreakDoor * turretDamageAt(position.first, position.second);
        if (projectedFightDamage + retreatDamage + safetyHp >= m_ghostHp
            && targetRoom.doorHp > m_ghostAttack) {
            return true;
        }
    }

    return false;
}

int DormDefenseController::buildingGoldCost(BuildingKind buildingKind, int nextLevel) const
{
    switch (buildingKind) {
    case BuildingKind::Generator:
        return doublingCost(200, nextLevel);
    case BuildingKind::Turret:
        return doublingCost(8, nextLevel);
    case BuildingKind::None:
    default:
        return 0;
    }
}

int DormDefenseController::buildingPowerCost(BuildingKind buildingKind, int nextLevel) const
{
    switch (buildingKind) {
    case BuildingKind::Generator:
        return 0;
    case BuildingKind::Turret:
        return 1 << qMax(0, nextLevel - 1);
    case BuildingKind::None:
    default:
        return 0;
    }
}

int DormDefenseController::bedGoldOutputForLevel(int level) const
{
    return 1 << qMax(0, level - 1);
}

int DormDefenseController::generatorPowerOutputForLevel(int level) const
{
    return 1 << qMax(0, level - 1);
}

int DormDefenseController::turretDamageForLevel(int level) const
{
    static constexpr std::array<int, 6> table = {1, 1, 2, 2, 3, 4};
    return tableValue(table, level);
}

int DormDefenseController::turretRangeForLevel(int level) const
{
    return 2 + qMax(0, level);
}

QString DormDefenseController::tileTypeName(TileKind tileKind) const
{
    switch (tileKind) {
    case TileKind::Empty:
        return QStringLiteral("empty");
    case TileKind::Corridor:
        return QStringLiteral("corridor");
    case TileKind::Door:
        return QStringLiteral("door");
    case TileKind::OtherDoor:
        return QStringLiteral("otherDoor");
    case TileKind::Bed:
        return QStringLiteral("bed");
    case TileKind::OtherBed:
        return QStringLiteral("otherBed");
    case TileKind::Buildable:
        return QStringLiteral("buildable");
    case TileKind::OtherRoom:
        return QStringLiteral("otherRoom");
    case TileKind::Wall:
    default:
        return QStringLiteral("wall");
    }
}

QString DormDefenseController::buildingTypeName(BuildingKind buildingKind) const
{
    switch (buildingKind) {
    case BuildingKind::Generator:
        return QStringLiteral("generator");
    case BuildingKind::Turret:
        return QStringLiteral("turret");
    case BuildingKind::None:
    default:
        return QStringLiteral("none");
    }
}

QString DormDefenseController::localRoleName(LocalRole role) const
{
    switch (role) {
    case LocalRole::Ghost:
        return QStringLiteral("ghost");
    case LocalRole::Spectator:
        return QStringLiteral("spectator");
    case LocalRole::Defender:
    default:
        return QStringLiteral("defender");
    }
}

DormDefenseController::LocalRole DormDefenseController::localRoleFromName(const QString &roleName) const
{
    if (roleName == QStringLiteral("ghost"))
        return LocalRole::Ghost;
    if (roleName == QStringLiteral("spectator"))
        return LocalRole::Spectator;
    return LocalRole::Defender;
}

bool DormDefenseController::localControlsDefender() const
{
    if (m_networked && m_sessionLocalPlayerId >= 0 && !m_roleSelectionRequired) {
        return m_networkRolesByPlayerId.value(m_sessionLocalPlayerId, m_localRole)
            == LocalRole::Defender;
    }
    return m_localRole == LocalRole::Defender;
}

bool DormDefenseController::localControlsGhost() const
{
    if (m_networked && m_sessionLocalPlayerId >= 0 && !m_roleSelectionRequired) {
        return m_networkRolesByPlayerId.value(m_sessionLocalPlayerId, m_localRole)
            == LocalRole::Ghost;
    }
    return m_localRole == LocalRole::Ghost;
}

bool DormDefenseController::ghostIsHumanControlled() const
{
    return m_ghostHumanControlled;
}

bool DormDefenseController::isAuthoritativeInstance() const
{
    return !m_networked || m_authoritative;
}

void DormDefenseController::configureRolesFromPlayers(const QVariantList &activePlayers, int localPlayerId)
{
    const QHash<int, LocalRole> previousRoles = m_networkRolesByPlayerId;
    const QHash<int, LocalRole> previousSelections = m_pendingRoleSelections;
    m_localRole = localPlayerId >= 0 ? LocalRole::Defender : LocalRole::Spectator;
    m_networkRolesByPlayerId.clear();
    m_pendingRoleSelections.clear();
    for (const QVariant &playerVariant : activePlayers) {
        const QVariantMap player = playerVariant.toMap();
        const int playerId = player.value(QStringLiteral("playerId")).toInt();
        const QString snapshotRoleName = player.value(QStringLiteral("dormDefenseRole")).toString();
        const LocalRole role = snapshotRoleName.isEmpty()
            ? previousRoles.value(playerId, LocalRole::Defender)
            : localRoleFromName(snapshotRoleName);
        m_networkRolesByPlayerId.insert(playerId, role);
        if (previousSelections.contains(playerId))
            m_pendingRoleSelections.insert(playerId, previousSelections.value(playerId));
        if (playerId == localPlayerId)
            m_localRole = role;
    }
    prunePlayerRoomAssignments();
    m_ghostHumanControlled = std::any_of(m_networkRolesByPlayerId.constBegin(),
                                         m_networkRolesByPlayerId.constEnd(),
                                         [](LocalRole role) { return role == LocalRole::Ghost; });
}

DormDefenseController::BuildingKind DormDefenseController::parseBuildingKind(const QString &buildType) const
{
    if (buildType == QStringLiteral("generator"))
        return BuildingKind::Generator;
    if (buildType == QStringLiteral("turret"))
        return BuildingKind::Turret;
    return BuildingKind::None;
}
