#pragma once

#include "Types.hpp"

namespace rpp {

struct Node {
    GridCoord coord{};
    double gCost = 0.0;
    double hCost = 0.0;
    double fCost = 0.0;
    GridCoord parent{-1, -1};
    bool hasParent = false;
    bool inOpenSet = false;
    bool inClosedSet = false;

    void recomputeF() noexcept {
        fCost = gCost + hCost;
    }
};

struct NodeCompare {
    bool operator()(const Node& a, const Node& b) const noexcept;
};

} // namespace rpp
