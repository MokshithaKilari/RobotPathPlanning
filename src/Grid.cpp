#include "Grid.hpp"
#include <algorithm>

namespace rpp {

Grid::Grid(int size) {
    resize(size);
}

void Grid::resize(int newSize) {
    if (newSize < 5) newSize = 5;
    if (newSize > 50) newSize = 50;
    m_size = newSize;
    m_staticMask.assign(static_cast<std::size_t>(m_size * m_size), false);
    m_staticObstacles.clear();

    m_start = {std::clamp(2, 0, m_size - 1), m_size / 2};
    m_goal = {std::clamp(m_size - 3, 0, m_size - 1), m_size / 2};
    if (m_start == m_goal && m_size >= 2) {
        m_goal = {m_size - 1, m_size / 2};
    }
}

bool Grid::inBounds(GridCoord c) const noexcept {
    return c.x >= 0 && c.x < m_size && c.y >= 0 && c.y < m_size;
}

bool Grid::isStaticObstacle(GridCoord c) const noexcept {
    if (!inBounds(c)) return false;
    return m_staticMask[static_cast<std::size_t>(c.y * m_size + c.x)];
}

bool Grid::isWalkable(GridCoord c) const noexcept {
    return inBounds(c) && !isStaticObstacle(c);
}

bool Grid::setStart(GridCoord c) {
    if (!inBounds(c) || isStaticObstacle(c)) {
        return false;
    }
    m_start = c;
    return true;
}

bool Grid::setGoal(GridCoord c) {
    if (!inBounds(c) || isStaticObstacle(c)) {
        return false;
    }
    m_goal = c;
    return true;
}

bool Grid::addStaticObstacle(GridCoord c) {
    if (!inBounds(c) || c == m_start || c == m_goal) {
        return false;
    }
    const std::size_t idx = static_cast<std::size_t>(c.y * m_size + c.x);
    if (m_staticMask[idx]) {
        return false;
    }
    m_staticMask[idx] = true;
    m_staticObstacles.emplace_back(c);
    return true;
}

bool Grid::removeStaticObstacle(GridCoord c) {
    if (!inBounds(c)) {
        return false;
    }
    const std::size_t idx = static_cast<std::size_t>(c.y * m_size + c.x);
    if (!m_staticMask[idx]) {
        return false;
    }
    m_staticMask[idx] = false;
    m_staticObstacles.erase(
        std::remove_if(m_staticObstacles.begin(),
                       m_staticObstacles.end(),
                       [&](const StaticObstacle& obs) { return obs.getPosition() == c; }),
        m_staticObstacles.end());
    return true;
}

void Grid::clearStaticObstacles() {
    std::fill(m_staticMask.begin(), m_staticMask.end(), false);
    m_staticObstacles.clear();
}

std::vector<GridCoord> Grid::getNeighbors(GridCoord c, MovementMode mode) const {
    std::vector<GridCoord> neighbors;
    neighbors.reserve(mode == MovementMode::EightDirectional ? 8 : 4);

    static constexpr GridCoord kCardinal[4] = {
        {0, -1}, // Up
        {1, 0},  // Right
        {0, 1},  // Down
        {-1, 0}  // Left
    };

    for (const auto& d : kCardinal) {
        GridCoord n{c.x + d.x, c.y + d.y};
        if (isWalkable(n)) {
            neighbors.push_back(n);
        }
    }

    if (mode == MovementMode::EightDirectional) {
        static constexpr GridCoord kDiagonal[4] = {
            {1, -1},
            {1, 1},
            {-1, 1},
            {-1, -1}
        };
        for (const auto& d : kDiagonal) {
            GridCoord n{c.x + d.x, c.y + d.y};
            GridCoord adjA{c.x + d.x, c.y};
            GridCoord adjB{c.x, c.y + d.y};
            // Prevent diagonal corner-cutting through two adjacent static obstacles
            if (isWalkable(n) && isWalkable(adjA) && isWalkable(adjB)) {
                neighbors.push_back(n);
            }
        }
    }

    return neighbors;
}

} // namespace rpp
