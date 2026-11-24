#pragma once

#include "AStar.hpp"
#include "Grid.hpp"
#include "Types.hpp"
#include <vector>

namespace rpp {

class Dijkstra {
public:
    PathResult findPath(const Grid& grid,
                        GridCoord start,
                        GridCoord goal,
                        MovementMode mode,
                        const std::vector<GridCoord>& blockedDynamicCells = {}) const;
};

} // namespace rpp
