#include "Statistics.hpp"

namespace rpp {

void Statistics::resetCurrentRun() {
    m_pathLength = 0;
    m_nodesExplored = 0;
    m_planningTimeMs = 0.0;
    m_replanCount = 0;
    m_blockedPathEvents = 0;
    m_elapsedSimTime = 0.0;
    m_distanceTravelled = 0.0;
    comparison = ComparisonMetrics{};
}

void Statistics::recordPlannerResult(int pathLen, int nodesExplored, double timeMs) {
    m_pathLength = pathLen;
    m_nodesExplored = nodesExplored;
    m_planningTimeMs = timeMs;
}

void Statistics::recordReplan(bool success) {
    if (success) {
        ++m_replanCount;
    }
}

void Statistics::recordBlockedPathEvent() {
    ++m_blockedPathEvents;
}

void Statistics::recordNavigationOutcome(bool success) {
    if (success) {
        ++m_successfulNavigations;
    } else {
        ++m_failedNavigations;
    }
}

void Statistics::commitRunRecord(const RunRecord& rec) {
    m_history.push_back(rec);
    if (m_history.size() > 24) {
        m_history.erase(m_history.begin());
    }
}

} // namespace rpp
