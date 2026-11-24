#pragma once

#include "AStar.hpp"
#include "Dijkstra.hpp"
#include "DynamicObstacle.hpp"
#include "EventLogger.hpp"
#include "ExplanationEngine.hpp"
#include "Grid.hpp"
#include "Robot.hpp"
#include "ScenarioManager.hpp"
#include "Statistics.hpp"
#include "Types.hpp"
#include <string>
#include <vector>

namespace rpp {

class Simulation {
public:
    Simulation();

    // Environment operations
    void newEnvironment(int gridSize = -1);
    void resizeGrid(int newSize);
    bool setStart(GridCoord c);
    bool setGoal(GridCoord c);
    bool addStaticObstacle(GridCoord c);
    bool removeObstacleAt(GridCoord c);
    bool addDynamicObstacle(GridCoord c, DynamicPattern pattern, double speed = 1.6);
    void clearObstacles();

    // Movement & planner settings
    void setMovementMode(MovementMode mode);
    MovementMode getMovementMode() const noexcept { return m_movementMode; }

    // Path planning & comparison
    bool calculatePath(bool fromRobotPosition = false);
    void clearPath();
    bool recalculatePath();
    void runDijkstraComparison();

    // A* Visualization & Explain A* step controller
    void startAStarVisualization(bool autoPlay);
    void vizPlay();
    void vizPause();
    void vizStepForward();
    void vizStepBackward();
    void vizRestart();
    void setVizSpeedStepsPerSec(double stepsPerSec) noexcept;
    double getVizSpeedStepsPerSec() const noexcept { return m_vizStepsPerSec; }
    bool isVizActive() const noexcept { return m_vizActive; }
    bool isVizPlaying() const noexcept { return m_vizPlaying; }
    std::size_t getVizStepIndex() const noexcept { return m_vizStepIndex; }
    const AStarStep* getCurrentVizStep() const noexcept;

    // Simulation execution
    void run();
    void pause();
    void resume();
    void stop();
    void restartSimulation();
    void resetAll();

    void setSimSpeedMultiplier(double mult) noexcept;
    double getSimSpeedMultiplier() const noexcept { return m_simSpeed; }

    void update(double dt);

    // Scenarios & Random Environment
    ScenarioManager& getScenarioManager() noexcept { return m_scenarioMgr; }
    const ScenarioManager& getScenarioManager() const noexcept { return m_scenarioMgr; }
    bool loadScenario(const ScenarioData& scenario);
    bool loadScenarioById(int id);
    const std::string& getActiveScenarioName() const noexcept { return m_activeScenarioName; }

    // Presentation helper
    bool injectObstacleOnPlannedPath();

    // Accessors for UI & tests
    const Grid& getGrid() const noexcept { return m_grid; }
    const Robot& getRobot() const noexcept { return m_robot; }
    const std::vector<DynamicObstacle>& getDynamicObstacles() const noexcept { return m_dynamicObstacles; }
    const PathResult& getLastPlanResult() const noexcept { return m_lastPlan; }
    const Statistics& getStatistics() const noexcept { return m_stats; }
    Statistics& getStatisticsMutable() noexcept { return m_stats; }
    const EventLogger& getLogger() const noexcept { return m_logger; }
    EventLogger& getLoggerMutable() noexcept { return m_logger; }
    const ReplanNotice& getReplanNotice() const noexcept { return m_replanNotice; }
    SimState getState() const noexcept { return m_state; }
    const std::string& getStatusMessage() const noexcept { return m_statusMessage; }
    const std::string& getLastError() const noexcept { return m_lastError; }
    bool isDynamicObstacleAt(GridCoord c) const noexcept;
    std::vector<GridCoord> getOccupiedDynamicCells() const;

private:
    bool isCellOccupiedByAnyDynamic(GridCoord c) const noexcept;
    bool willDynamicEnterCell(GridCoord c) const noexcept;
    bool checkPlannedPathBlocked(GridCoord& outBlockedCoord, std::string& outReason) const;
    bool performDynamicReplan(GridCoord triggerCoord, const std::string& reason);
    void finalizeRunStats(bool succeeded);

    Grid m_grid;
    Robot m_robot;
    AStar m_astar;
    Dijkstra m_dijkstra;
    std::vector<DynamicObstacle> m_dynamicObstacles;
    ScenarioManager m_scenarioMgr;
    Statistics m_stats;
    EventLogger m_logger;

    MovementMode m_movementMode = MovementMode::FourDirectional;
    SimState m_state = SimState::Idle;
    PathResult m_lastPlan;
    ReplanNotice m_replanNotice;

    // A* step visualization state
    bool m_vizActive = false;
    bool m_vizPlaying = false;
    std::size_t m_vizStepIndex = 0;
    double m_vizStepsPerSec = 10.0;
    double m_vizAccumulator = 0.0;

    // Simulation timing
    double m_simSpeed = 1.0;
    double m_elapsedSimSec = 0.0;
    double m_replanCooldown = 0.0;
    bool m_runRecorded = false;
    int m_nextDynId = 1;

    std::string m_activeScenarioName = "Custom Environment";
    std::string m_statusMessage = "Ready. Configure environment or calculate A* path.";
    std::string m_lastError;
};

} // namespace rpp
