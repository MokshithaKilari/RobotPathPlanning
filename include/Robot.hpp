#pragma once

#include "Types.hpp"
#include <vector>

namespace rpp {

class Robot {
public:
    explicit Robot(GridCoord startPos = {2, 10});

    void reset(GridCoord startPos);
    void setCellImmediate(GridCoord pos);

    void setPath(const std::vector<GridCoord>& newPath);
    void clearPath();

    GridCoord getCurrentCell() const noexcept { return m_currentCell; }
    GridCoord getNextWaypoint() const;
    bool hasNextWaypoint() const noexcept;
    bool hasReachedGoal(GridCoord goal) const noexcept;

    double getRenderX() const noexcept { return m_renderX; }
    double getRenderY() const noexcept { return m_renderY; }
    double getHeadingRad() const noexcept { return m_headingRad; }
    std::string getDirectionLabel() const;

    double getSpeedCellsPerSec() const noexcept { return m_baseSpeed; }
    void setSpeedCellsPerSec(double speed) noexcept;

    double getDistanceTravelled() const noexcept { return m_distanceTravelled; }
    const std::vector<GridCoord>& getTravelledTrail() const noexcept { return m_travelledTrail; }
    const std::vector<GridCoord>& getActivePath() const noexcept { return m_activePath; }
    std::size_t getPathCursor() const noexcept { return m_pathIndex; }

    // Advances smooth interpolation towards the next cell in activePath.
    // Returns true if the robot arrived at a cell boundary this frame.
    bool stepMovement(double dt, double simSpeedMultiplier);

private:
    GridCoord m_currentCell{};
    double m_renderX = 0.0;
    double m_renderY = 0.0;
    double m_headingRad = 0.0;
    double m_baseSpeed = 3.2; // cells per second at 1.0x
    double m_distanceTravelled = 0.0;

    std::vector<GridCoord> m_activePath;
    std::size_t m_pathIndex = 0; // index of current cell or target waypoint
    double m_segmentProgress = 0.0;
    GridCoord m_segmentFrom{};
    GridCoord m_segmentTo{};
    bool m_movingSegment = false;

    std::vector<GridCoord> m_travelledTrail;
};

} // namespace rpp
