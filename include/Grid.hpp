#pragma once

#include "StaticObstacle.hpp"
#include "Types.hpp"
#include <vector>

namespace rpp {

class Grid {
public:
    explicit Grid(int size = 20);

    void resize(int newSize);
    int getSize() const noexcept { return m_size; }

    bool inBounds(GridCoord c) const noexcept;
    bool isStaticObstacle(GridCoord c) const noexcept;
    bool isWalkable(GridCoord c) const noexcept;

    bool setStart(GridCoord c);
    bool setGoal(GridCoord c);
    GridCoord getStart() const noexcept { return m_start; }
    GridCoord getGoal() const noexcept { return m_goal; }

    bool addStaticObstacle(GridCoord c);
    bool removeStaticObstacle(GridCoord c);
    void clearStaticObstacles();

    int getStaticObstacleCount() const noexcept { return static_cast<int>(m_staticObstacles.size()); }
    const std::vector<StaticObstacle>& getStaticObstacles() const noexcept { return m_staticObstacles; }

    std::vector<GridCoord> getNeighbors(GridCoord c, MovementMode mode) const;

private:
    int m_size = 20;
    GridCoord m_start{2, 10};
    GridCoord m_goal{17, 10};
    std::vector<bool> m_staticMask;
    std::vector<StaticObstacle> m_staticObstacles;
};

} // namespace rpp
