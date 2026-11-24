#include "Robot.hpp"
#include <algorithm>
#include <cmath>

namespace rpp {

Robot::Robot(GridCoord startPos) {
    reset(startPos);
}

void Robot::reset(GridCoord startPos) {
    m_currentCell = startPos;
    m_renderX = static_cast<double>(startPos.x);
    m_renderY = static_cast<double>(startPos.y);
    m_headingRad = 0.0;
    m_distanceTravelled = 0.0;
    m_activePath.clear();
    m_pathIndex = 0;
    m_segmentProgress = 0.0;
    m_segmentFrom = startPos;
    m_segmentTo = startPos;
    m_movingSegment = false;
    m_travelledTrail.clear();
    m_travelledTrail.push_back(startPos);
}

void Robot::setCellImmediate(GridCoord pos) {
    m_currentCell = pos;
    m_renderX = static_cast<double>(pos.x);
    m_renderY = static_cast<double>(pos.y);
    m_movingSegment = false;
    m_segmentProgress = 0.0;
    if (m_travelledTrail.empty() || m_travelledTrail.back() != pos) {
        m_travelledTrail.push_back(pos);
    }
}

void Robot::setPath(const std::vector<GridCoord>& newPath) {
    m_activePath = newPath;
    m_pathIndex = 0;
    m_movingSegment = false;
    m_segmentProgress = 0.0;

    if (!m_activePath.empty() && m_activePath[0] == m_currentCell && m_activePath.size() > 1) {
        m_pathIndex = 1;
    }
}

void Robot::clearPath() {
    m_activePath.clear();
    m_pathIndex = 0;
    m_movingSegment = false;
    m_segmentProgress = 0.0;
}

bool Robot::hasNextWaypoint() const noexcept {
    return m_movingSegment || (!m_activePath.empty() && m_pathIndex < m_activePath.size());
}

GridCoord Robot::getNextWaypoint() const {
    if (m_movingSegment) {
        return m_segmentTo;
    }
    if (!m_activePath.empty() && m_pathIndex < m_activePath.size()) {
        return m_activePath[m_pathIndex];
    }
    return m_currentCell;
}

bool Robot::hasReachedGoal(GridCoord goal) const noexcept {
    return !m_movingSegment && m_currentCell == goal;
}

void Robot::setSpeedCellsPerSec(double speed) noexcept {
    m_baseSpeed = std::clamp(speed, 0.5, 12.0);
}

std::string Robot::getDirectionLabel() const {
    constexpr double kPi = 3.14159265358979323846;
    double deg = m_headingRad * (180.0 / kPi);
    while (deg < -180.0) deg += 360.0;
    while (deg > 180.0) deg -= 360.0;

    if (deg >= -22.5 && deg < 22.5) return "East (+X)";
    if (deg >= 22.5 && deg < 67.5) return "South-East";
    if (deg >= 67.5 && deg < 112.5) return "South (+Y)";
    if (deg >= 112.5 && deg < 157.5) return "South-West";
    if (deg >= 157.5 || deg < -157.5) return "West (-X)";
    if (deg >= -157.5 && deg < -112.5) return "North-West";
    if (deg >= -112.5 && deg < -67.5) return "North (-Y)";
    return "North-East";
}

bool Robot::stepMovement(double dt, double simSpeedMultiplier) {
    if (dt <= 0.0 || simSpeedMultiplier <= 0.0) return false;
    if (!hasNextWaypoint()) return false;

    if (!m_movingSegment) {
        GridCoord target = m_activePath[m_pathIndex];
        if (target == m_currentCell) {
            ++m_pathIndex;
            if (m_pathIndex >= m_activePath.size()) {
                return true;
            }
            target = m_activePath[m_pathIndex];
        }
        m_segmentFrom = m_currentCell;
        m_segmentTo = target;
        m_segmentProgress = 0.0;
        m_movingSegment = true;

        const double dx = static_cast<double>(m_segmentTo.x - m_segmentFrom.x);
        const double dy = static_cast<double>(m_segmentTo.y - m_segmentFrom.y);
        m_headingRad = std::atan2(dy, dx);
    }

    const double dx = static_cast<double>(m_segmentTo.x - m_segmentFrom.x);
    const double dy = static_cast<double>(m_segmentTo.y - m_segmentFrom.y);
    const double segLen = std::max(1.0, std::hypot(dx, dy));

    const double stepDelta = (dt * m_baseSpeed * simSpeedMultiplier) / segLen;
    m_segmentProgress += stepDelta;

    if (m_segmentProgress >= 1.0) {
        m_currentCell = m_segmentTo;
        m_renderX = static_cast<double>(m_currentCell.x);
        m_renderY = static_cast<double>(m_currentCell.y);
        m_distanceTravelled += segLen;
        m_movingSegment = false;
        m_segmentProgress = 0.0;
        ++m_pathIndex;

        if (m_travelledTrail.empty() || m_travelledTrail.back() != m_currentCell) {
            m_travelledTrail.push_back(m_currentCell);
        }
        return true;
    }

    m_renderX = static_cast<double>(m_segmentFrom.x) + dx * m_segmentProgress;
    m_renderY = static_cast<double>(m_segmentFrom.y) + dy * m_segmentProgress;
    return false;
}

} // namespace rpp
