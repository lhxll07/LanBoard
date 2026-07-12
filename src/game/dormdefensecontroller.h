#pragma once

#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "src/common/gamecontrollerbase.h"

class DormDefenseController : public GameControllerBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList cells READ cells NOTIFY boardChanged)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int gold READ gold NOTIFY resourcesChanged)
    Q_PROPERTY(int power READ power NOTIFY resourcesChanged)
    Q_PROPERTY(int bedLevel READ bedLevel NOTIFY resourcesChanged)
    Q_PROPERTY(int doorHp READ doorHp NOTIFY boardChanged)
    Q_PROPERTY(int doorMaxHp READ doorMaxHp NOTIFY boardChanged)
    Q_PROPERTY(int wave READ wave NOTIFY statusChanged)
    Q_PROPERTY(int timeRemaining READ timeRemaining NOTIFY statusChanged)
    Q_PROPERTY(int ghostHp READ ghostHp NOTIFY statusChanged)
    Q_PROPERTY(int ghostMaxHp READ ghostMaxHp NOTIFY statusChanged)
    Q_PROPERTY(int ghostLevel READ ghostLevel NOTIFY statusChanged)
    Q_PROPERTY(int ghostDistance READ ghostDistance NOTIFY statusChanged)
    Q_PROPERTY(int ghostRow READ ghostRow NOTIFY ghostStateChanged)
    Q_PROPERTY(int ghostColumn READ ghostColumn NOTIFY ghostStateChanged)
    Q_PROPERTY(qreal ghostCenterRow READ ghostCenterRow NOTIFY ghostStateChanged)
    Q_PROPERTY(qreal ghostCenterColumn READ ghostCenterColumn NOTIFY ghostStateChanged)
    Q_PROPERTY(bool ghostVisible READ ghostVisible NOTIFY ghostStateChanged)
    Q_PROPERTY(QVariantList turretVolley READ turretVolley NOTIFY turretVolleyChanged)
    Q_PROPERTY(int turretVolleySerial READ turretVolleySerial NOTIFY turretVolleyChanged)
    Q_PROPERTY(int lastTurretDamage READ lastTurretDamage NOTIFY turretVolleyChanged)
    Q_PROPERTY(bool ghostRespawning READ ghostRespawning NOTIFY statusChanged)
    Q_PROPERTY(int respawnCountdown READ respawnCountdown NOTIFY statusChanged)
    Q_PROPERTY(bool gameOver READ isGameOver NOTIFY gameOverChanged)
    Q_PROPERTY(int winner READ winner NOTIFY gameOverChanged)
    Q_PROPERTY(bool victory READ victory NOTIFY gameOverChanged)
    Q_PROPERTY(bool playerControlsGhost READ playerControlsGhost NOTIFY roleChanged)
    Q_PROPERTY(bool humanGhostControlled READ humanGhostControlled NOTIFY roleChanged)
    Q_PROPERTY(bool roleSelectionRequired READ roleSelectionRequired NOTIFY roleChanged)
    Q_PROPERTY(int roleSelectionCountdown READ roleSelectionCountdown NOTIFY roleChanged)
    Q_PROPERTY(QString roleSelectionChoice READ roleSelectionChoice NOTIFY roleChanged)
    Q_PROPERTY(bool ghostRoleLocked READ ghostRoleLocked NOTIFY roleChanged)
    Q_PROPERTY(bool playerRoomSelected READ playerRoomSelected NOTIFY boardChanged)
    Q_PROPERTY(bool localPlayerEliminated READ localPlayerEliminated NOTIFY boardChanged)
    Q_PROPERTY(int playerBedRow READ playerBedRow NOTIFY boardChanged)
    Q_PROPERTY(int playerBedColumn READ playerBedColumn NOTIFY boardChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString helpText READ helpText NOTIFY statusChanged)
    Q_PROPERTY(QString debugState READ debugState NOTIFY statusChanged)

public:
    explicit DormDefenseController(QObject *parent = nullptr);

    Q_INVOKABLE bool buildOrUpgradeAt(int row, int column, const QString &buildType);
    Q_INVOKABLE QVariantMap actionInfo(const QString &type, int row = -1, int column = -1) const;
    Q_INVOKABLE void prepareForRoleSelection();
    Q_INVOKABLE void setPlayerControlsGhost(bool controlsGhost);
    Q_INVOKABLE bool submitRoleSelection(bool controlsGhost);
    Q_INVOKABLE bool chooseDefenderRoom(int row, int column);
    Q_INVOKABLE bool moveGhostBy(int deltaRow, int deltaColumn);
    Q_INVOKABLE bool moveGhostVector(qreal rowDelta, qreal columnDelta);
    Q_INVOKABLE bool selectGhostTargetAt(int row, int column);
    Q_INVOKABLE bool upgradeBed();
    Q_INVOKABLE bool upgradeDoor();
    Q_INVOKABLE bool repairDoor();
    Q_INVOKABLE void finalizeGameOver(int winner);
    Q_INVOKABLE void configureNetworkSession(const QVariantList &activePlayers,
                                             int localPlayerId,
                                             bool networked,
                                             bool authoritative);
    Q_INVOKABLE QJsonObject buildNetworkState() const;
    QJsonObject buildNetworkStateForPlayer(int playerId) const;
    Q_INVOKABLE void applyNetworkState(const QJsonObject &state);
    void applyNetworkGhostPosition(qreal row, qreal column);
    bool applyNetworkAction(int playerId, const QJsonObject &action);
    Q_INVOKABLE void startNewGame() override;
    Q_INVOKABLE void reset() override;

    bool isGameOver() const override { return m_gameOver; }
    int winner() const override { return m_winner; }

    QVariantList cells() const;
    int rows() const { return RowCount; }
    int columns() const { return ColumnCount; }
    int gold() const;
    int power() const;
    int bedLevel() const;
    int doorHp() const;
    int doorMaxHp() const;
    int wave() const { return m_wave; }
    int timeRemaining() const { return m_timeRemaining; }
    int ghostHp() const { return m_ghostHp; }
    int ghostMaxHp() const { return m_ghostMaxHp; }
    int ghostLevel() const { return m_ghostLevel; }
    int ghostDistance() const { return m_ghostDistance; }
    int ghostRow() const;
    int ghostColumn() const;
    qreal ghostCenterRow() const;
    qreal ghostCenterColumn() const;
    bool ghostVisible() const;
    QVariantList turretVolley() const { return m_turretVolley; }
    int turretVolleySerial() const { return m_turretVolleySerial; }
    int lastTurretDamage() const { return m_lastTurretDamage; }
    bool ghostRespawning() const { return m_respawnCountdown > 0; }
    int respawnCountdown() const { return m_respawnCountdown; }
    bool victory() const { return m_gameOver && m_winner == 1; }
    bool playerControlsGhost() const { return m_playerControlsGhost; }
    bool humanGhostControlled() const { return m_ghostHumanControlled; }
    bool roleSelectionRequired() const { return m_roleSelectionRequired; }
    int roleSelectionCountdown() const { return m_roleSelectionCountdown; }
    QString roleSelectionChoice() const;
    bool ghostRoleLocked() const;
    bool playerRoomSelected() const { return currentPlayerRoomIndex() >= 0; }
    bool localPlayerEliminated() const;
    int playerBedRow() const;
    int playerBedColumn() const;
    QString statusText() const;
    QString helpText() const;
    QString debugState() const;
    QString networkRoleNameForPlayer(int playerId) const;

signals:
    void boardChanged();
    void resourcesChanged();
    void statusChanged();
    void ghostStateChanged();
    void turretVolleyChanged();
    void roleChanged();
    void networkActionRequested(QJsonObject action);

private:
    enum class GhostMode {
        Assault,
        Retreat
    };

    enum class LocalRole {
        Defender,
        Ghost,
        Spectator
    };

    enum class TileKind {
        Empty,
        Wall,
        Corridor,
        Door,
        OtherDoor,
        Bed,
        OtherBed,
        Buildable,
        OtherRoom
    };

    enum class BuildingKind {
        None,
        Generator,
        Turret
    };

    static constexpr int RowCount = 25;
    static constexpr int ColumnCount = 32;
    static constexpr int InitialDoorHp = 16;
    static constexpr int InitialTimeRemaining = 30;
    static constexpr int DoorHitsPerGhostLevel = 8;
    static constexpr int GhostRecoveryDelayTicks = 5;
    static constexpr int GhostRecoveryMinHpPerTick = 6;
    static constexpr int GhostRetreatSafetyBuffer = 2;
    static constexpr int RoleSelectionDuration = 10;
    static constexpr int CombatTickMs = 250;
    static constexpr int TurretAttackIntervalMs = 1000;
    static constexpr int GhostDoorAttackIntervalMs = 1000;

    struct CellData {
        TileKind tileKind = TileKind::Wall;
        BuildingKind buildingKind = BuildingKind::None;
        int level = 0;
    };

    struct RoomState {
        int top = 0;
        int left = 0;
        int doorRow = 0;
        int doorColumn = 0;
        int bedRow = 0;
        int bedColumn = 0;
        int doorHp = InitialDoorHp;
        int doorMaxHp = InitialDoorHp;
        int gold = 0;
        int power = 0;
        int bedLevel = 1;
        bool active = true;
        bool isPlayerRoom = false;
    };

    void initializeBoard();
    void initializeGhostPath(int startRow = RowCount - 1, int startColumn = 1);
    QVector<QPair<int, int>> buildPath(int startRow, int startColumn, int goalRow, int goalColumn) const;
    void spawnGhostForWave();
    void onTick();
    void onCombatTick();
    void scheduleGhostRespawn();
    bool processAiRooms();
    void chooseRandomTargetRoom(bool preferDifferentRoom);
    void startGhostRetreat();
    void setGameFinished(bool playerWon);
    void levelUpGhost(int levelDelta, bool restoreHealth);
    void updateStatus(const QString &message = QString());
    void emitAllChanges();
    void beginRoleSelectionPhase();
    void finalizeRoleSelection();
    bool recordRoleSelectionChoice(int playerId, LocalRole role);
    int selectedGhostPlayerId() const;
    void applyRoomOwnershipPresentation();
    bool assignPlayerRoom(int roomIndex);
    void syncPrimaryRoomStateFromMembers();
    void syncMembersFromPrimaryRoomState();
    int playerRoomIndexForPlayer(int playerId) const;
    bool roomAssignedToAnotherPlayer(int roomIndex, int playerId) const;
    bool roomHasHumanDefender(int roomIndex) const;
    void assignMissingDefenderRooms();
    void prunePlayerRoomAssignments();
    bool shouldGhostRetreat() const;
    void setTurretVolley(const QVariantList &volley, int totalDamage);
    int cellIndex(int row, int column) const;
    bool isBuildableCell(int row, int column) const;
    int roomIndexForCell(int row, int column) const;
    int currentTargetRoomIndex() const;
    int currentPlayerRoomIndex() const;
    QPair<int, int> currentGhostPosition() const;
    bool ghostOccupiesCell(int row, int column) const;
    bool isCorridorCell(int row, int column) const;
    bool isCorridorPoint(qreal row, qreal column) const;
    int roomIndexForGhostAttackPosition(int row, int column) const;
    int roomIndexForGhostAttackPoint(qreal row, qreal column) const;
    int turretCoverageCountAtPoint(qreal row, qreal column) const;
    void clearRoomAfterDoorDestroyed(int roomIndex);
    bool applyHumanGhostDoorDamage(int roomIndex, int damage);
    bool damageHumanGhostDoor(int roomIndex);
    bool processHumanGhostDoorCombat(int elapsedMs);
    bool moveGhostToNetworkPosition(qreal row, qreal column);
    void queueGhostNetworkPositionSync();
    bool hasAnyActiveRoom() const;
    QVector<QPair<int, int>> roomBuildSlots(int roomIndex) const;
    int buildingCount(int roomIndex, BuildingKind buildingKind) const;
    int doorLevelForHp(int doorMaxHp) const;
    int maxBedLevelForDoorLevel(int doorLevel) const;
    int requiredDoorLevelForBedLevel(int bedLevel) const;
    int requiredBedLevelForBuilding(BuildingKind buildingKind, int nextLevel) const;
    int requiredDoorLevelForBuilding(BuildingKind buildingKind, int nextLevel) const;
    int requiredDoorLevelForRoom(int roomIndex) const;
    bool tryAiRepairDoor(int roomIndex);
    bool tryAiUpgradeDoor(int roomIndex);
    bool tryAiUpgradeBed(int roomIndex);
    bool tryAiBuildOrUpgrade(int roomIndex, BuildingKind buildingKind);
    int roomGeneratorCount(int roomIndex) const;
    int roomTurretCount(int roomIndex) const;
    int roomGeneratorGoldOutput(int roomIndex) const;
    int roomGeneratorPowerOutput(int roomIndex) const;
    QVariantList firingTurretsAt(int row, int column) const;
    QVariantList firingTurretsAtPoint(qreal row, qreal column) const;
    int generatorGoldOutput() const;
    int generatorPowerOutput() const;
    int turretDamageAt(int row, int column) const;
    int turretDamageAtPoint(qreal row, qreal column) const;
    int turretDamageOutput() const;
    int projectedRetreatDamage(const QVector<QPair<int, int>> &path) const;
    int buildingGoldCost(BuildingKind buildingKind, int nextLevel) const;
    int buildingPowerCost(BuildingKind buildingKind, int nextLevel) const;
    int bedGoldOutputForLevel(int level) const;
    int generatorPowerOutputForLevel(int level) const;
    int turretDamageForLevel(int level) const;
    int turretRangeForLevel(int level) const;
    QString tileTypeName(TileKind tileKind) const;
    QString buildingTypeName(BuildingKind buildingKind) const;
    BuildingKind parseBuildingKind(const QString &buildType) const;
    QString localRoleName(LocalRole role) const;
    LocalRole localRoleFromName(const QString &roleName) const;
    bool localControlsDefender() const;
    bool localControlsGhost() const;
    bool ghostIsHumanControlled() const;
    bool isAuthoritativeInstance() const;
    void configureRolesFromPlayers(const QVariantList &activePlayers, int localPlayerId);

    QVector<CellData> m_cells;
    QVector<RoomState> m_rooms;
    QVector<QPair<int, int>> m_ghostPath;
    QTimer m_tickTimer;
    QTimer m_combatTimer;
    QTimer m_roleSelectionTimer;
    QTimer m_ghostNetworkSyncTimer;
    int m_gold = 0;
    int m_power = 0;
    int m_bedLevel = 1;
    int m_doorHp = InitialDoorHp;
    int m_doorMaxHp = InitialDoorHp;
    int m_wave = 1;
    int m_timeRemaining = InitialTimeRemaining;
    int m_ghostHp = 0;
    int m_ghostMaxHp = 0;
    int m_ghostLevel = 1;
    int m_ghostAttack = 0;
    int m_ghostDistance = 0;
    int m_elapsedTicks = 0;
    int m_ghostDoorHitCount = 0;
    int m_respawnCountdown = 0;
    int m_targetRoomIndex = -1;
    int m_ticksOutOfCombat = 0;
    QVariantList m_turretVolley;
    int m_turretVolleySerial = 0;
    int m_lastTurretDamage = 0;
    bool m_gameOver = false;
    int m_winner = 0;
    bool m_playerControlsGhost = false;
    bool m_roleSelectionRequired = true;
    int m_roleSelectionCountdown = 0;
    bool m_networked = false;
    bool m_authoritative = false;
    bool m_ghostHumanControlled = false;
    LocalRole m_localRole = LocalRole::Defender;
    int m_sessionLocalPlayerId = 0;
    int m_playerRoomIndex = -1;
    QHash<int, LocalRole> m_networkRolesByPlayerId;
    QHash<int, int> m_playerRoomByPlayerId;
    QSet<int> m_eliminatedDefenderPlayerIds;
    QHash<int, LocalRole> m_pendingRoleSelections;
    QVector<int> m_turretChargeMs;
    QVector<bool> m_turretTargetInRange;
    int m_humanGhostDoorRoomIndex = -1;
    int m_humanGhostDoorChargeMs = 0;
    qreal m_ghostCenterRow = 0.5;
    qreal m_ghostCenterColumn = 0.5;
    qreal m_networkGhostDisplayRow = 0.5;
    qreal m_networkGhostDisplayColumn = 0.5;
    bool m_humanGhostDamagedThisSecond = false;
    bool m_humanGhostAttackedDoorThisSecond = false;
    GhostMode m_ghostMode = GhostMode::Assault;
    QString m_lastActionMessage;
};
