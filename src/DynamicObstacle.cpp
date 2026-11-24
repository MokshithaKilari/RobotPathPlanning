#include "DynamicObstacle.hpp"
#include "Grid.hpp"
#include <algorithm>
#include <cmath>

namespace rpp {

namespace {
bool containsCoord(const std::vector<GridCoord>& list, GridCoord c) {
    return std::find(list.begin(), list.end(), c) != list.end();
}
} // namespace

DynamicObstacle::DynamicObstacle(int id,
                                 GridCoord startPos,
                                 DynamicPattern pattern,
                                 double speedCellsPerSec,
                                 std::vector<GridCoord> route)
    : m_id(id),
      m_spawnCell(startPos),
      m_currentCell(startPos),
      m_previousCell(startPos),
      m_nextCell(startPos),
      m_renderX(static_cast<double>(startPos.x)),
      m_renderY(static_cast<double>(startPos.y)),
      m_pattern(pattern),
      m_speed(std::clamp(speedCellsPerSec, 0.25, 6.0)),
      m_route(std::move(route)),
      m_rngState(static_cast<std::uint32_t>(id * 747796405u + 2891336453u)) {
    if (m_pattern == DynamicPattern::VerticalPatrol) {
        m_headingRad = 3.1415926535 / 2.0;
    }
}

void DynamicObstacle::setSpeed(double cellsPerSec) noexcept {
    m_speed = std::clamp(cellsPerSec, 0.25, 6.0);
}

void DynamicObstacle::setPatrolRoute(std::vector<GridCoord> route) {
    m_route = std::move(route);
    m_routeIndex = 0;
}

void DynamicObstacle::reset() {
    m_currentCell = m_spawnCell;
    m_previousCell = m_spawnCell;
    m_nextCell = m_spawnCell;
    m_isMoving = false;
    m_moveProgress = 0.0;
    m_waitTimer = 0.0;
    m_renderX = static_cast<double>(m_spawnCell.x);
    m_renderY = static_cast<double>(m_spawnCell.y);
    m_direction = 1;
    m_routeIndex = 0;
    m_rngState = static_cast<std::uint32_t>(m_id * 747796405u + 2891336453u);
}

GridCoord DynamicObstacle::chooseNextCell(const Grid& grid,
                                          GridCoord robotCell,
                                          const std::vector<GridCoord>& otherDynamicCells) {
    auto isValidTarget = [&](GridCoord c) {
        if (!grid.isWalkable(c)) return false;
        if (c == robotCell) return false;
        if (containsCoord(otherDynamicCells, c)) return false;
        return true;
    };

    if (m_pattern == DynamicPattern::HorizontalPatrol) {
        GridCoord candidate{m_currentCell.x + m_direction, m_currentCell.y};
        if (isValidTarget(candidate)) {
            return candidate;
        }
        m_direction = -m_direction;
        GridCoord reversed{m_currentCell.x + m_direction, m_currentCell.y};
        if (isValidTarget(reversed)) {
            return reversed;
        }
        return m_currentCell;
    }

    if (m_pattern == DynamicPattern::VerticalPatrol) {
        GridCoord candidate{m_currentCell.x, m_currentCell.y + m_direction};
        if (isValidTarget(candidate)) {
            return candidate;
        }
        m_direction = -m_direction;
        GridCoord reversed{m_currentCell.x, m_currentCell.y + m_direction};
        if (isValidTarget(reversed)) {
            return reversed;
        }
        return m_currentCell;
    }

    if (m_pattern == DynamicPattern::PatrolRoute && !m_route.empty()) {
        if (m_route.size() == 1) {
            return m_currentCell;
        }
        std::size_t nextIdx = (m_routeIndex + 1) % m_route.size();
        GridCoord waypoint = m_route[nextIdx];
        if (waypoint == m_currentCell) {
            m_routeIndex = nextIdx;
            nextIdx = (m_routeIndex + 1) % m_route.size();
            waypoint = m_route[nextIdx];
        }
        // Step one cell toward waypoint along Manhattan axis
        int dx = waypoint.x - m_currentCell.x;
        int dy = waypoint.y - m_currentCell.y;
        GridCoord stepX{m_currentCell.x + (dx > 0 ? 1 : (dx < 0 ? -1 : 0)), m_currentCell.y};
        GridCoord stepY{m_currentCell.x, m_currentCell.y + (dy > 0 ? 1 : (dy < 0 ? -1 : 0))};

        if (std::abs(dx) >= std::abs(dy) && dx != 0 && isValidTarget(stepX)) {
            if (stepX == waypoint) m_routeIndex = nextIdx;
            return stepX;
        }
        if (dy != 0 && isValidTarget(stepY)) {
            if (stepY == waypoint) m_routeIndex = nextIdx;
            return stepY;
        }
        if (dx != 0 && isValidTarget(stepX)) {
            if (stepX == waypoint) m_routeIndex = nextIdx;
            return stepX;
        }
        return m_currentCell;
    }

    // RandomWalk
    static constexpr GridCoord kDirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    std::vector<GridCoord> valid;
    for (const auto& d : kDirs) {
        GridCoord c{m_currentCell.x + d.x, m_currentCell.y + d.y};
        if (isValidTarget(c)) {
            valid.push_back(c);
        }
    }
    if (valid.empty()) {
        return m_currentCell;
    }
    // Prefer continuing away from immediate previous cell if alternatives exist
    if (valid.size() > 1) {
        valid.erase(std::remove(valid.begin(), valid.end(), m_previousCell), valid.end());
    }
    // Simple deterministic LCG per obstacle
    m_rngState = m_rngState * 1664525u + 1013904223u;
    std::size_t pick = static_cast<std::size_t>(m_rngState >> 16) % valid.size();
    return valid[pick];
}

void DynamicObstacle::update(double dt,
                             const Grid& grid,
                             GridCoord robotCell,
                             const std::vector<GridCoord>& otherDynamicCells) {
    if (dt <= 0.0) return;

    if (!m_isMoving) {
        if (m_waitTimer > 0.0) {
            m_waitTimer -= dt;
            return;
        }
        GridCoord next = chooseNextCell(grid, robotCell, otherDynamicCells);
        if (next != m_currentCell) {
            m_nextCell = next;
            m_isMoving = true;
            m_moveProgress = 0.0;
            double dx = static_cast<double>(m_nextCell.x - m_currentCell.x);
            double dy = static_cast<double>(m_nextCell.y - m_currentCell.y);
            m_headingRad = std::atan2(dy, dx);
        } else {
            m_nextCell = m_currentCell;
            m_waitTimer = 0.25;
        }
    }

    if (m_isMoving) {
        // Safety check: if next cell became blocked mid-step, abort back if early or pause
        if (!grid.isWalkable(m_nextCell) || m_nextCell == robotCell) {
            if (m_moveProgress < 0.35) {
                m_isMoving = false;
                m_moveProgress = 0.0;
                m_nextCell = m_currentCell;
                m_renderX = static_cast<double>(m_currentCell.x);
                m_renderY = static_cast<double>(m_currentCell.y);
                m_waitTimer = 0.2;
                return;
            }
        }

        m_moveProgress += dt * m_speed;
        if (m_moveProgress >= 1.0) {
            m_previousCell = m_currentCell;
            m_currentCell = m_nextCell;
            m_renderX = static_cast<double>(m_currentCell.x);
            m_renderY = static_cast<double>(m_currentCell.y);
            m_isMoving = false;
            m_moveProgress = 0.0;
        } else {
            m_renderX = static_cast<double>(m_currentCell.x) +
                        static_cast<double>(m_nextCell.x - m_currentCell.x) * m_moveProgress;
            m_renderY = static_cast<double>(m_currentCell.y) +
                        static_cast<double>(m_nextCell.y - m_currentCell.y) * m_moveProgress;
        }
    }
}

} // namespace rpp
