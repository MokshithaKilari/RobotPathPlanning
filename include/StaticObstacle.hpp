#pragma once

#include "Types.hpp"

namespace rpp {

class StaticObstacle {
public:
    explicit StaticObstacle(GridCoord pos = {0, 0});

    GridCoord getPosition() const noexcept { return m_position; }
    void setPosition(GridCoord pos) noexcept { m_position = pos; }

private:
    GridCoord m_position;
};

} // namespace rpp
