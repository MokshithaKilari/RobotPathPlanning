#include "Node.hpp"
#include <cmath>

namespace rpp {

bool NodeCompare::operator()(const Node& a, const Node& b) const noexcept {
    constexpr double kEps = 1e-9;
    if (std::abs(a.fCost - b.fCost) > kEps) {
        return a.fCost > b.fCost;
    }
    if (std::abs(a.hCost - b.hCost) > kEps) {
        return a.hCost > b.hCost;
    }
    return a.coord < b.coord;
}

} // namespace rpp
