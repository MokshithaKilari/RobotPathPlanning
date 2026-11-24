#pragma once

#include "Types.hpp"
#include <cstdint>
#include <vector>

namespace rpp {

class Grid;

class DynamicObstacle {
public:
    DynamicObstacle(int id,
                    GridCoord startPos,
                    DynamicPattern pattern,
                    double speedCellsPerSec = 1.5,
                    std::vector<GridCoord> route = {});

    int getId() const noexcept { return m_id; }
    GridCoord getCell() const noexcept { return m_currentCell; }
    GridCoord getPreviousCell() const noexcept { return m_previousCell; }
    GridCoord getPredictedNextCell() const noexcept { return m_nextCell; }

    double getRenderX() const noexcept { return m_renderX; }
    double getRenderY() const noexcept { return m_renderY; }
    double getHeadingRad() const noexcept { return m_headingRad; }

    DynamicPattern getPattern() const noexcept { return m_pattern; }
    double getSpeed() const noexcept { return m_speed; }
    void setSpeed(double cellsPerSec) noexcept;

    const std::vector<GridCoord>& getPatrolRoute() const noexcept { return m_route; }
    void setPatrolRoute(std::vector<GridCoord> route);

    void update(double dt, const Grid& grid, GridCoord robotCell, const std::vector<GridCoord>& otherDynamicCells);
    void reset();

private:
    GridCoord chooseNextCell(const Grid& grid, GridCoord robotCell, const std::vector<GridCoord>& otherDynamicCells);

    int m_id = 0;
    GridCoord m_spawnCell{};
    GridCoord m_currentCell{};
    GridCoord m_previousCell{};
    GridCoord m_nextCell{};
    bool m_isMoving = false;
    double m_moveProgress = 0.0;
    double m_waitTimer = 0.0;

    double m_renderX = 0.0;
    double m_renderY = 0.0;
    double m_headingRad = 0.0;

    DynamicPattern m_pattern = DynamicPattern::HorizontalPatrol;
    double m_speed = 1.5;
    int m_direction = 1;

    std::vector<GridCoord> m_route;
    std::size_t m_routeIndex = 0;
    std::uint32_t m_rngState = 1337u;
};

} // namespace rpp
