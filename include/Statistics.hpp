#pragma once

#include <string>
#include <vector>

namespace rpp {

struct RunRecord {
    std::string scenarioName;
    int gridSize = 20;
    int staticObstacles = 0;
    int dynamicObstacles = 0;
    int pathLength = 0;
    int aStarNodesExplored = 0;
    double aStarPlanningMs = 0.0;
    int dijkstraNodesExplored = 0;
    double dijkstraPlanningMs = 0.0;
    int replans = 0;
    double distanceTravelled = 0.0;
    double elapsedSimSec = 0.0;
    bool succeeded = false;
};

struct ComparisonMetrics {
    bool valid = false;
    int aStarPathLength = 0;
    int aStarNodesExplored = 0;
    double aStarTimeMs = 0.0;
    std::string aStarStatus;

    int dijkstraPathLength = 0;
    int dijkstraNodesExplored = 0;
    double dijkstraTimeMs = 0.0;
    std::string dijkstraStatus;
};

class Statistics {
public:
    void resetCurrentRun();
    void recordPlannerResult(int pathLen, int nodesExplored, double timeMs);
    void recordReplan(bool success);
    void recordBlockedPathEvent();
    void recordNavigationOutcome(bool success);
    void setElapsedTime(double sec) noexcept { m_elapsedSimTime = sec; }
    void setDistanceTravelled(double dist) noexcept { m_distanceTravelled = dist; }

    void commitRunRecord(const RunRecord& rec);
    const std::vector<RunRecord>& getRunHistory() const noexcept { return m_history; }

    int getPathLength() const noexcept { return m_pathLength; }
    int getNodesExplored() const noexcept { return m_nodesExplored; }
    double getPlanningTimeMs() const noexcept { return m_planningTimeMs; }
    int getReplanCount() const noexcept { return m_replanCount; }
    int getBlockedPathEvents() const noexcept { return m_blockedPathEvents; }
    int getSuccessfulNavigations() const noexcept { return m_successfulNavigations; }
    int getFailedNavigations() const noexcept { return m_failedNavigations; }
    double getElapsedTime() const noexcept { return m_elapsedSimTime; }
    double getDistanceTravelled() const noexcept { return m_distanceTravelled; }

    ComparisonMetrics comparison;

private:
    int m_pathLength = 0;
    int m_nodesExplored = 0;
    double m_planningTimeMs = 0.0;
    int m_replanCount = 0;
    int m_blockedPathEvents = 0;
    int m_successfulNavigations = 0;
    int m_failedNavigations = 0;
    double m_elapsedSimTime = 0.0;
    double m_distanceTravelled = 0.0;

    std::vector<RunRecord> m_history;
};

} // namespace rpp
