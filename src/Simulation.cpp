#include "Simulation.hpp"
#include <algorithm>
#include <sstream>

namespace rpp {

Simulation::Simulation()
    : m_grid(20),
      m_robot({2, 10}) {
    m_logger.log("Environment initialized (20x20)", LogSeverity::Info);
    m_logger.log("Start position: " + m_grid.getStart().toString() +
                 ", Goal position: " + m_grid.getGoal().toString(), LogSeverity::Info);
}

void Simulation::newEnvironment(int gridSize) {
    const int size = (gridSize > 0) ? gridSize : m_grid.getSize();
    m_grid.resize(size);
    m_dynamicObstacles.clear();
    m_nextDynId = 1;
    m_robot.reset(m_grid.getStart());
    m_lastPlan = PathResult{};
    m_replanNotice = ReplanNotice{};
    m_stats.resetCurrentRun();
    m_vizActive = false;
    m_vizPlaying = false;
    m_vizStepIndex = 0;
    m_elapsedSimSec = 0.0;
    m_runRecorded = false;
    m_state = SimState::Idle;
    m_activeScenarioName = "Custom Environment";
    m_lastError.clear();
    m_statusMessage = "New " + std::to_string(size) + "x" + std::to_string(size) + " environment initialized.";
    m_logger.log("New environment created (" + std::to_string(size) + "x" + std::to_string(size) + ")", LogSeverity::Info);
}

void Simulation::resizeGrid(int newSize) {
    newEnvironment(newSize);
}

bool Simulation::setStart(GridCoord c) {
    if (!m_grid.inBounds(c)) {
        m_lastError = "Start coordinate " + c.toString() + " is out of bounds.";
        m_statusMessage = m_lastError;
        return false;
    }
    if (m_grid.isStaticObstacle(c) || isDynamicObstacleAt(c)) {
        m_lastError = "Cannot place Start inside an obstacle at " + c.toString() + ".";
        m_statusMessage = m_lastError;
        m_logger.log(m_lastError, LogSeverity::Warning);
        return false;
    }
    m_grid.setStart(c);
    if (m_state == SimState::Idle || m_state == SimState::Completed || m_state == SimState::Blocked || m_state == SimState::Error) {
        m_robot.reset(c);
    }
    m_lastError.clear();
    m_statusMessage = "Start position set to " + c.toString() + ".";
    m_logger.log(m_statusMessage, LogSeverity::Info);
    if (m_grid.getStart() == m_grid.getGoal()) {
        m_statusMessage = "Notice: Start equals Goal at " + c.toString() + ".";
        m_logger.log(m_statusMessage, LogSeverity::Warning);
    }
    return true;
}

bool Simulation::setGoal(GridCoord c) {
    if (!m_grid.inBounds(c)) {
        m_lastError = "Goal coordinate " + c.toString() + " is out of bounds.";
        m_statusMessage = m_lastError;
        return false;
    }
    if (m_grid.isStaticObstacle(c) || isDynamicObstacleAt(c)) {
        m_lastError = "Cannot place Goal inside an obstacle at " + c.toString() + ".";
        m_statusMessage = m_lastError;
        m_logger.log(m_lastError, LogSeverity::Warning);
        return false;
    }
    m_grid.setGoal(c);
    m_lastError.clear();
    m_statusMessage = "Goal position set to " + c.toString() + ".";
    m_logger.log(m_statusMessage, LogSeverity::Info);
    if (m_grid.getStart() == m_grid.getGoal()) {
        m_statusMessage = "Notice: Start equals Goal at " + c.toString() + ".";
        m_logger.log(m_statusMessage, LogSeverity::Warning);
    }
    return true;
}

bool Simulation::addStaticObstacle(GridCoord c) {
    if (!m_grid.inBounds(c)) {
        m_lastError = "Invalid obstacle placement: out of bounds.";
        return false;
    }
    if (c == m_grid.getStart() || c == m_grid.getGoal() || c == m_robot.getCurrentCell()) {
        m_lastError = "Cannot place obstacle on Start, Goal, or Robot cell " + c.toString() + ".";
        m_statusMessage = m_lastError;
        m_logger.log(m_lastError, LogSeverity::Warning);
        return false;
    }
    if (isDynamicObstacleAt(c)) {
        m_lastError = "Cell " + c.toString() + " is occupied by a dynamic obstacle.";
        m_statusMessage = m_lastError;
        return false;
    }
    if (!m_grid.addStaticObstacle(c)) {
        return false;
    }

    m_lastError.clear();
    m_logger.log("Static obstacle added at " + c.toString(), LogSeverity::Info);
    m_statusMessage = "Static obstacle placed at " + c.toString() + ".";

    // Check if placed obstacle blocks the currently planned path
    if (!m_lastPlan.path.empty()) {
        auto it = std::find(m_lastPlan.path.begin(), m_lastPlan.path.end(), c);
        if (it != m_lastPlan.path.end()) {
            m_stats.recordBlockedPathEvent();
            if (m_state == SimState::Running || m_state == SimState::Paused || m_state == SimState::Planning || m_state == SimState::Idle) {
                performDynamicReplan(c, "Static obstacle inserted on planned path");
            }
        }
    }
    return true;
}

bool Simulation::removeObstacleAt(GridCoord c) {
    if (m_grid.removeStaticObstacle(c)) {
        m_logger.log("Static obstacle removed at " + c.toString(), LogSeverity::Info);
        m_statusMessage = "Removed static obstacle at " + c.toString() + ".";
        return true;
    }

    auto it = std::find_if(m_dynamicObstacles.begin(),
                           m_dynamicObstacles.end(),
                           [&](const DynamicObstacle& d) { return d.getCell() == c; });
    if (it != m_dynamicObstacles.end()) {
        m_dynamicObstacles.erase(it);
        m_logger.log("Dynamic obstacle removed at " + c.toString(), LogSeverity::Info);
        m_statusMessage = "Removed dynamic obstacle at " + c.toString() + ".";
        return true;
    }
    return false;
}

bool Simulation::addDynamicObstacle(GridCoord c, DynamicPattern pattern, double speed) {
    if (!m_grid.inBounds(c) || !m_grid.isWalkable(c)) {
        m_lastError = "Cannot place dynamic obstacle on static obstacle or out of bounds.";
        m_statusMessage = m_lastError;
        return false;
    }
    if (c == m_grid.getStart() || c == m_grid.getGoal() || c == m_robot.getCurrentCell()) {
        m_lastError = "Cannot spawn dynamic obstacle on Start, Goal, or Robot cell.";
        m_statusMessage = m_lastError;
        return false;
    }
    if (isDynamicObstacleAt(c)) {
        m_lastError = "A dynamic obstacle already exists at " + c.toString() + ".";
        m_statusMessage = m_lastError;
        return false;
    }

    std::vector<GridCoord> route;
    if (pattern == DynamicPattern::PatrolRoute) {
        const int maxC = m_grid.getSize() - 1;
        route = {
            c,
            {std::clamp(c.x + 3, 0, maxC), c.y},
            {std::clamp(c.x + 3, 0, maxC), std::clamp(c.y + 3, 0, maxC)},
            {c.x, std::clamp(c.y + 3, 0, maxC)}
        };
    }

    m_dynamicObstacles.emplace_back(m_nextDynId++, c, pattern, speed, std::move(route));
    m_logger.log("Dynamic obstacle (" + toString(pattern) + ") added at " + c.toString(), LogSeverity::Info);
    m_statusMessage = "Dynamic obstacle added at " + c.toString() + ".";

    if (!m_lastPlan.path.empty()) {
        if (std::find(m_lastPlan.path.begin(), m_lastPlan.path.end(), c) != m_lastPlan.path.end()) {
            m_stats.recordBlockedPathEvent();
            performDynamicReplan(c, "Dynamic obstacle spawned on planned path");
        }
    }
    return true;
}

void Simulation::clearObstacles() {
    m_grid.clearStaticObstacles();
    m_dynamicObstacles.clear();
    m_replanNotice = ReplanNotice{};
    m_logger.log("All static and dynamic obstacles cleared", LogSeverity::Info);
    m_statusMessage = "All obstacles cleared.";
}

void Simulation::setMovementMode(MovementMode mode) {
    m_movementMode = mode;
    m_logger.log("Movement mode set to " + toString(mode), LogSeverity::Info);
    m_statusMessage = "Movement mode: " + toString(mode);
}

bool Simulation::calculatePath(bool fromRobotPosition) {
    const GridCoord origin = fromRobotPosition ? m_robot.getCurrentCell() : m_grid.getStart();
    if (!fromRobotPosition) {
        m_robot.reset(m_grid.getStart());
        m_elapsedSimSec = 0.0;
        m_runRecorded = false;
    }

    m_logger.log("A* planning started from " + origin.toString() + " to " + m_grid.getGoal().toString(), LogSeverity::Info);

    const auto dynCells = getOccupiedDynamicCells();
    m_lastPlan = m_astar.findPath(m_grid, origin, m_grid.getGoal(), m_movementMode, dynCells, true);

    if (m_lastPlan.success()) {
        m_robot.setPath(m_lastPlan.path);
        m_stats.recordPlannerResult(static_cast<int>(m_lastPlan.path.size()),
                                    m_lastPlan.nodesExplored,
                                    m_lastPlan.planningTimeMs);
        m_lastError.clear();
        m_statusMessage = m_lastPlan.message;
        m_logger.log("Path generated: " + std::to_string(m_lastPlan.path.size()) +
                     " cells, " + std::to_string(m_lastPlan.nodesExplored) + " nodes explored",
                     LogSeverity::Success);
        return true;
    }

    m_robot.clearPath();
    m_stats.recordPlannerResult(0, m_lastPlan.nodesExplored, m_lastPlan.planningTimeMs);
    m_lastError = m_lastPlan.message;
    m_statusMessage = m_lastPlan.message;
    m_logger.log("Planning failed: " + m_lastPlan.message, LogSeverity::Critical);
    return false;
}

void Simulation::clearPath() {
    m_lastPlan = PathResult{};
    m_robot.clearPath();
    m_vizActive = false;
    m_vizPlaying = false;
    m_vizStepIndex = 0;
    m_replanNotice = ReplanNotice{};
    m_statusMessage = "Planned path cleared.";
    m_logger.log("Path visualization cleared", LogSeverity::Info);
}

bool Simulation::recalculatePath() {
    const bool fromRobot = (m_state == SimState::Running || m_state == SimState::Paused || m_state == SimState::Blocked);
    return calculatePath(fromRobot);
}

void Simulation::runDijkstraComparison() {
    const auto dynCells = getOccupiedDynamicCells();
    const PathResult aRes = m_astar.findPath(m_grid, m_grid.getStart(), m_grid.getGoal(), m_movementMode, dynCells, false);
    const PathResult dRes = m_dijkstra.findPath(m_grid, m_grid.getStart(), m_grid.getGoal(), m_movementMode, dynCells);

    ComparisonMetrics& cmp = m_stats.comparison;
    cmp.valid = true;
    cmp.aStarPathLength = static_cast<int>(aRes.path.size());
    cmp.aStarNodesExplored = aRes.nodesExplored;
    cmp.aStarTimeMs = aRes.planningTimeMs;
    cmp.aStarStatus = toString(aRes.status);

    cmp.dijkstraPathLength = static_cast<int>(dRes.path.size());
    cmp.dijkstraNodesExplored = dRes.nodesExplored;
    cmp.dijkstraTimeMs = dRes.planningTimeMs;
    cmp.dijkstraStatus = toString(dRes.status);

    m_logger.log("Algorithm comparison: A* explored " + std::to_string(aRes.nodesExplored) +
                 " nodes vs Dijkstra " + std::to_string(dRes.nodesExplored) + " nodes",
                 LogSeverity::Info);
    m_statusMessage = "A* vs Dijkstra benchmark completed.";
}

void Simulation::startAStarVisualization(bool autoPlay) {
    if (m_lastPlan.steps.empty()) {
        calculatePath(false);
    }
    if (m_lastPlan.steps.empty()) {
        m_vizActive = false;
        m_vizPlaying = false;
        return;
    }
    m_vizActive = true;
    m_vizPlaying = autoPlay;
    m_vizStepIndex = 0;
    m_vizAccumulator = 0.0;
    m_statusMessage = autoPlay ? "Visualizing A* search..." : "Explain A* step-by-step mode ready.";
}

void Simulation::vizPlay() {
    if (!m_vizActive) startAStarVisualization(true);
    else m_vizPlaying = true;
}

void Simulation::vizPause() {
    m_vizPlaying = false;
}

void Simulation::vizStepForward() {
    if (!m_vizActive) startAStarVisualization(false);
    m_vizPlaying = false;
    if (!m_lastPlan.steps.empty() && m_vizStepIndex + 1 < m_lastPlan.steps.size()) {
        ++m_vizStepIndex;
    }
}

void Simulation::vizStepBackward() {
    if (!m_vizActive) startAStarVisualization(false);
    m_vizPlaying = false;
    if (m_vizStepIndex > 0) {
        --m_vizStepIndex;
    }
}

void Simulation::vizRestart() {
    if (!m_vizActive) startAStarVisualization(false);
    m_vizStepIndex = 0;
    m_vizAccumulator = 0.0;
    m_vizPlaying = false;
}

void Simulation::setVizSpeedStepsPerSec(double stepsPerSec) noexcept {
    m_vizStepsPerSec = std::clamp(stepsPerSec, 1.0, 60.0);
}

const AStarStep* Simulation::getCurrentVizStep() const noexcept {
    if (m_lastPlan.steps.empty()) return nullptr;
    if (m_vizActive) {
        const std::size_t idx = std::min(m_vizStepIndex, m_lastPlan.steps.size() - 1);
        return &m_lastPlan.steps[idx];
    }
    return &m_lastPlan.steps.back();
}

void Simulation::run() {
    if (m_state == SimState::Completed) {
        restartSimulation();
    }

    if (m_robot.getActivePath().empty() || m_lastPlan.status != PlanStatus::PathFound) {
        if (!calculatePath(true)) {
            if (m_lastPlan.status == PlanStatus::StartEqualsGoal) {
                m_state = SimState::Completed;
                m_statusMessage = "Robot is already at Goal.";
                return;
            }
            m_state = SimState::Error;
            return;
        }
    }

    m_vizActive = false;
    m_vizPlaying = false;
    m_state = SimState::Running;
    m_statusMessage = "Simulation running.";
    m_logger.log("Simulation started", LogSeverity::Info);
}

void Simulation::pause() {
    if (m_state == SimState::Running || m_state == SimState::Replanning) {
        m_state = SimState::Paused;
        m_statusMessage = "Simulation paused.";
        m_logger.log("Simulation paused", LogSeverity::Warning);
    }
}

void Simulation::resume() {
    if (m_state == SimState::Paused || m_state == SimState::Blocked) {
        if (m_robot.getActivePath().empty()) {
            if (!calculatePath(true)) {
                m_state = SimState::Blocked;
                return;
            }
        }
        m_state = SimState::Running;
        m_statusMessage = "Simulation resumed.";
        m_logger.log("Simulation resumed", LogSeverity::Info);
    }
}

void Simulation::stop() {
    if (m_state == SimState::Running || m_state == SimState::Paused || m_state == SimState::Replanning) {
        m_state = SimState::Idle;
        m_robot.reset(m_grid.getStart());
        m_elapsedSimSec = 0.0;
        m_stats.setElapsedTime(0.0);
        m_stats.setDistanceTravelled(0.0);
        if (!m_lastPlan.path.empty()) {
            m_robot.setPath(m_lastPlan.path);
        }
        m_statusMessage = "Simulation stopped. Robot returned to Start.";
        m_logger.log("Simulation stopped", LogSeverity::Info);
    }
}

void Simulation::restartSimulation() {
    m_robot.reset(m_grid.getStart());
    for (auto& dyn : m_dynamicObstacles) {
        dyn.reset();
    }
    m_elapsedSimSec = 0.0;
    m_runRecorded = false;
    m_replanNotice = ReplanNotice{};
    m_stats.resetCurrentRun();
    if (calculatePath(false)) {
        m_state = SimState::Idle;
        m_statusMessage = "Simulation restarted. Press Run to begin.";
    } else {
        m_state = SimState::Error;
    }
}

void Simulation::resetAll() {
    m_robot.reset(m_grid.getStart());
    for (auto& dyn : m_dynamicObstacles) {
        dyn.reset();
    }
    m_lastPlan = PathResult{};
    m_replanNotice = ReplanNotice{};
    m_stats.resetCurrentRun();
    m_vizActive = false;
    m_vizPlaying = false;
    m_vizStepIndex = 0;
    m_elapsedSimSec = 0.0;
    m_runRecorded = false;
    m_state = SimState::Idle;
    m_lastError.clear();
    m_statusMessage = "Simulation reset.";
    m_logger.log("Simulation state reset", LogSeverity::Info);
}

void Simulation::setSimSpeedMultiplier(double mult) noexcept {
    m_simSpeed = std::clamp(mult, 0.25, 4.0);
}

bool Simulation::isDynamicObstacleAt(GridCoord c) const noexcept {
    return isCellOccupiedByAnyDynamic(c);
}

bool Simulation::isCellOccupiedByAnyDynamic(GridCoord c) const noexcept {
    for (const auto& d : m_dynamicObstacles) {
        if (d.getCell() == c) return true;
    }
    return false;
}

bool Simulation::willDynamicEnterCell(GridCoord c) const noexcept {
    for (const auto& d : m_dynamicObstacles) {
        if (d.getCell() == c || d.getPredictedNextCell() == c) {
            return true;
        }
    }
    return false;
}

std::vector<GridCoord> Simulation::getOccupiedDynamicCells() const {
    std::vector<GridCoord> out;
    out.reserve(m_dynamicObstacles.size() * 2);
    for (const auto& d : m_dynamicObstacles) {
        out.push_back(d.getCell());
        if (d.getPredictedNextCell() != d.getCell()) {
            out.push_back(d.getPredictedNextCell());
        }
    }
    return out;
}

bool Simulation::checkPlannedPathBlocked(GridCoord& outBlockedCoord, std::string& outReason) const {
    const auto& path = m_robot.getActivePath();
    if (path.empty()) return false;

    const std::size_t startIdx = std::min(m_robot.getPathCursor(), path.size());
    for (std::size_t i = startIdx; i < path.size(); ++i) {
        const GridCoord cell = path[i];
        if (m_grid.isStaticObstacle(cell)) {
            outBlockedCoord = cell;
            outReason = "Static obstacle occupies planned cell " + cell.toString();
            return true;
        }
        // Immediate next 2 waypoints check both current and predicted dynamic occupancy
        if (i <= startIdx + 2) {
            if (willDynamicEnterCell(cell)) {
                outBlockedCoord = cell;
                outReason = "Dynamic obstacle collision risk at " + cell.toString();
                return true;
            }
        } else if (isCellOccupiedByAnyDynamic(cell)) {
            outBlockedCoord = cell;
            outReason = "Dynamic obstacle blocks upcoming path at " + cell.toString();
            return true;
        }
    }
    return false;
}

bool Simulation::performDynamicReplan(GridCoord triggerCoord, const std::string& reason) {
    const SimState prevState = m_state;
    m_state = SimState::Replanning;
    m_logger.log("Obstacle detected at " + triggerCoord.toString() + " — invalidating route", LogSeverity::Warning);
    m_logger.log("Replanning using A* from " + m_robot.getCurrentCell().toString(), LogSeverity::Info);

    const auto dynCells = getOccupiedDynamicCells();
    PathResult replanRes = m_astar.findPath(m_grid,
                                            m_robot.getCurrentCell(),
                                            m_grid.getGoal(),
                                            m_movementMode,
                                            dynCells,
                                            true);

    m_replanNotice.active = true;
    m_replanNotice.obstacleCoord = triggerCoord;
    m_replanNotice.reason = reason;
    m_replanNotice.nodesExplored = replanRes.nodesExplored;

    if (replanRes.success()) {
        m_lastPlan = std::move(replanRes);
        m_robot.setPath(m_lastPlan.path);
        m_stats.recordPlannerResult(static_cast<int>(m_lastPlan.path.size()),
                                    m_lastPlan.nodesExplored,
                                    m_lastPlan.planningTimeMs);
        m_stats.recordReplan(true);

        m_replanNotice.replanSucceeded = true;
        m_replanNotice.newPathLength = static_cast<int>(m_lastPlan.path.size());
        m_replanNotice.replanCount = m_stats.getReplanCount();

        m_lastError.clear();
        m_statusMessage = "Dynamic replanning succeeded: new route (" +
                          std::to_string(m_lastPlan.path.size()) + " cells).";
        m_logger.log("New path generated: " + std::to_string(m_lastPlan.path.size()) +
                     " cells (Replan #" + std::to_string(m_stats.getReplanCount()) + ")",
                     LogSeverity::Success);

        m_state = (prevState == SimState::Running || prevState == SimState::Replanning)
                      ? SimState::Running
                      : prevState;
        return true;
    }

    m_robot.clearPath();
    m_stats.recordReplan(false);
    m_replanNotice.replanSucceeded = false;
    m_replanNotice.newPathLength = 0;
    m_replanNotice.replanCount = m_stats.getReplanCount();

    m_lastError = "Replanning failed: " + replanRes.message;
    m_statusMessage = m_lastError;
    m_logger.log("Replanning failed — path completely blocked", LogSeverity::Critical);
    m_state = SimState::Blocked;
    m_replanCooldown = 0.75;
    return false;
}

void Simulation::finalizeRunStats(bool succeeded) {
    if (m_runRecorded) return;
    m_runRecorded = true;
    m_stats.recordNavigationOutcome(succeeded);

    // Also run Dijkstra benchmark on current environment for real Analytics comparison
    const PathResult dRes = m_dijkstra.findPath(m_grid, m_grid.getStart(), m_grid.getGoal(), m_movementMode, {});
    RunRecord rec;
    rec.scenarioName = m_activeScenarioName;
    rec.gridSize = m_grid.getSize();
    rec.staticObstacles = m_grid.getStaticObstacleCount();
    rec.dynamicObstacles = static_cast<int>(m_dynamicObstacles.size());
    rec.pathLength = m_stats.getPathLength();
    rec.aStarNodesExplored = m_stats.getNodesExplored();
    rec.aStarPlanningMs = m_stats.getPlanningTimeMs();
    rec.dijkstraNodesExplored = dRes.nodesExplored;
    rec.dijkstraPlanningMs = dRes.planningTimeMs;
    rec.replans = m_stats.getReplanCount();
    rec.distanceTravelled = m_robot.getDistanceTravelled();
    rec.elapsedSimSec = m_elapsedSimSec;
    rec.succeeded = succeeded;
    m_stats.commitRunRecord(rec);
}

void Simulation::update(double dt) {
    if (dt <= 0.0) return;

    // 1. Handle A* step visualization playback
    if (m_vizActive && m_vizPlaying && !m_lastPlan.steps.empty()) {
        m_vizAccumulator += dt * m_vizStepsPerSec;
        while (m_vizAccumulator >= 1.0) {
            m_vizAccumulator -= 1.0;
            if (m_vizStepIndex + 1 < m_lastPlan.steps.size()) {
                ++m_vizStepIndex;
            } else {
                m_vizPlaying = false;
                m_statusMessage = "A* search visualization complete.";
                break;
            }
        }
    }

    if (m_replanCooldown > 0.0) {
        m_replanCooldown = std::max(0.0, m_replanCooldown - dt);
    }

    if (m_state != SimState::Running && m_state != SimState::Blocked) {
        return;
    }

    const double scaledDt = dt * m_simSpeed;
    m_elapsedSimSec += scaledDt;
    m_stats.setElapsedTime(m_elapsedSimSec);

    // 2. Update all dynamic obstacles with collision awareness
    for (std::size_t i = 0; i < m_dynamicObstacles.size(); ++i) {
        std::vector<GridCoord> others;
        others.reserve(m_dynamicObstacles.size());
        for (std::size_t j = 0; j < m_dynamicObstacles.size(); ++j) {
            if (i != j) {
                others.push_back(m_dynamicObstacles[j].getCell());
                others.push_back(m_dynamicObstacles[j].getPredictedNextCell());
            }
        }
        m_dynamicObstacles[i].update(scaledDt, m_grid, m_robot.getCurrentCell(), others);
    }

    // 3. If blocked by a dynamic obstacle that moved away, try recovering automatically
    if (m_state == SimState::Blocked) {
        if (m_replanCooldown <= 0.0) {
            const auto dynCells = getOccupiedDynamicCells();
            PathResult retry = m_astar.findPath(m_grid,
                                                m_robot.getCurrentCell(),
                                                m_grid.getGoal(),
                                                m_movementMode,
                                                dynCells,
                                                true);
            if (retry.success()) {
                m_lastPlan = std::move(retry);
                m_robot.setPath(m_lastPlan.path);
                m_stats.recordReplan(true);
                m_stats.recordPlannerResult(static_cast<int>(m_lastPlan.path.size()),
                                            m_lastPlan.nodesExplored,
                                            m_lastPlan.planningTimeMs);
                m_state = SimState::Running;
                m_lastError.clear();
                m_statusMessage = "Corridor cleared — dynamic replanning resumed navigation.";
                m_logger.log("Route cleared; navigation resumed", LogSeverity::Success);
            } else {
                m_replanCooldown = 0.5;
            }
        }
        return;
    }

    // 4. Continuous look-ahead safety check before robot steps into next cell
    GridCoord blockedAt{};
    std::string blockReason;
    if (checkPlannedPathBlocked(blockedAt, blockReason)) {
        m_stats.recordBlockedPathEvent();
        if (!performDynamicReplan(blockedAt, blockReason)) {
            return;
        }
    }

    // Double-check immediate next cell safety right before stepping
    if (m_robot.hasNextWaypoint()) {
        const GridCoord nextCell = m_robot.getNextWaypoint();
        if (!m_grid.isWalkable(nextCell) || willDynamicEnterCell(nextCell)) {
            m_stats.recordBlockedPathEvent();
            if (!performDynamicReplan(nextCell, "Immediate next cell unsafe")) {
                return;
            }
        }
    }

    // 5. Advance smooth robot movement
    m_robot.stepMovement(dt, m_simSpeed);
    m_stats.setDistanceTravelled(m_robot.getDistanceTravelled());

    // 6. Check goal completion
    if (m_robot.hasReachedGoal(m_grid.getGoal())) {
        m_state = SimState::Completed;
        m_statusMessage = "Destination reached at " + m_grid.getGoal().toString() + "!";
        m_logger.log("Destination reached! Distance: " +
                     std::to_string(static_cast<int>(std::round(m_robot.getDistanceTravelled()))) +
                     " cells, Replans: " + std::to_string(m_stats.getReplanCount()),
                     LogSeverity::Success);
        finalizeRunStats(true);
    }
}

bool Simulation::loadScenario(const ScenarioData& scenario) {
    if (scenario.gridSize < 5 || !Grid(scenario.gridSize).inBounds(scenario.start) ||
        !Grid(scenario.gridSize).inBounds(scenario.goal)) {
        m_lastError = "Invalid scenario configuration: out-of-bounds coordinates.";
        m_statusMessage = m_lastError;
        m_logger.log(m_lastError, LogSeverity::Critical);
        return false;
    }

    m_grid.resize(scenario.gridSize);
    m_dynamicObstacles.clear();
    m_nextDynId = 1;
    m_movementMode = scenario.recommendedMovement;
    m_activeScenarioName = scenario.name;

    m_grid.setStart(scenario.start);
    m_grid.setGoal(scenario.goal);

    for (const auto& obs : scenario.staticObstacles) {
        m_grid.addStaticObstacle(obs);
    }
    for (const auto& dSpec : scenario.dynamicObstacles) {
        if (m_grid.isWalkable(dSpec.startPos) && dSpec.startPos != scenario.start && dSpec.startPos != scenario.goal) {
            m_dynamicObstacles.emplace_back(m_nextDynId++, dSpec.startPos, dSpec.pattern, dSpec.speed, dSpec.route);
        }
    }

    m_robot.reset(scenario.start);
    m_lastPlan = PathResult{};
    m_replanNotice = ReplanNotice{};
    m_stats.resetCurrentRun();
    m_vizActive = false;
    m_vizPlaying = false;
    m_vizStepIndex = 0;
    m_elapsedSimSec = 0.0;
    m_runRecorded = false;
    m_state = SimState::Idle;
    m_lastError.clear();

    m_logger.log("Loaded scenario: " + scenario.name, LogSeverity::Info);
    calculatePath(false);
    return true;
}

bool Simulation::loadScenarioById(int id) {
    const ScenarioData* s = m_scenarioMgr.getScenarioById(id);
    if (!s) {
        m_lastError = "Scenario #" + std::to_string(id) + " not found.";
        return false;
    }
    return loadScenario(*s);
}

bool Simulation::injectObstacleOnPlannedPath() {
    if (m_lastPlan.path.empty()) {
        calculatePath(true);
    }
    const auto& path = m_robot.getActivePath();
    if (path.size() < 3) {
        m_lastError = "No active multi-cell path available to block.";
        m_statusMessage = m_lastError;
        return false;
    }

    const std::size_t cursor = std::min(m_robot.getPathCursor(), path.size() - 1);
    const std::size_t targetIdx = std::min(cursor + 2, path.size() - 2);
    for (std::size_t i = targetIdx; i < path.size() - 1; ++i) {
        const GridCoord candidate = path[i];
        if (candidate != m_grid.getStart() && candidate != m_grid.getGoal() && candidate != m_robot.getCurrentCell()) {
            return addStaticObstacle(candidate);
        }
    }
    return false;
}

} // namespace rpp
