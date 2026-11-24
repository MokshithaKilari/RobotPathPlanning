#include "Dijkstra.hpp"
#include "AStar.hpp"
#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace rpp {

PathResult Dijkstra::findPath(const Grid& grid,
                              GridCoord start,
                              GridCoord goal,
                              MovementMode mode,
                              const std::vector<GridCoord>& blockedDynamicCells) const {
    using Clock = std::chrono::high_resolution_clock;
    const auto tStart = Clock::now();

    PathResult result;
    std::unordered_set<GridCoord, CoordHash> dynBlocked(blockedDynamicCells.begin(), blockedDynamicCells.end());

    if (!grid.isWalkable(start) || dynBlocked.count(start) > 0) {
        result.status = PlanStatus::StartBlocked;
        result.message = "Start position is blocked.";
        result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
        return result;
    }

    if (!grid.isWalkable(goal) || dynBlocked.count(goal) > 0) {
        result.status = PlanStatus::GoalBlocked;
        result.message = "Goal position is blocked.";
        result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
        return result;
    }

    if (start == goal) {
        result.status = PlanStatus::StartEqualsGoal;
        result.path = {start};
        result.nodesExplored = 1;
        result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
        return result;
    }

    // Dijkstra is uniform-cost search (h(n) = 0, f(n) = g(n))
    std::priority_queue<Node, std::vector<Node>, NodeCompare> openQueue;
    std::unordered_map<GridCoord, Node, CoordHash> nodeMap;
    nodeMap.reserve(static_cast<std::size_t>(grid.getSize() * grid.getSize()));

    Node startNode;
    startNode.coord = start;
    startNode.gCost = 0.0;
    startNode.hCost = 0.0;
    startNode.fCost = 0.0;
    startNode.inOpenSet = true;
    nodeMap[start] = startNode;
    openQueue.push(startNode);

    while (!openQueue.empty()) {
        const Node popped = openQueue.top();
        openQueue.pop();

        auto curIt = nodeMap.find(popped.coord);
        if (curIt == nodeMap.end() || curIt->second.inClosedSet) continue;
        if (popped.gCost > curIt->second.gCost + 1e-9) continue;

        curIt->second.inOpenSet = false;
        curIt->second.inClosedSet = true;
        const Node current = curIt->second;
        ++result.nodesExplored;

        if (current.coord == goal) {
            std::vector<GridCoord> path;
            GridCoord cur = goal;
            while (true) {
                path.push_back(cur);
                if (cur == start) break;
                auto it = nodeMap.find(cur);
                if (it == nodeMap.end() || !it->second.hasParent) break;
                cur = it->second.parent;
            }
            std::reverse(path.begin(), path.end());
            result.status = PlanStatus::PathFound;
            result.path = std::move(path);
            result.totalCost = current.gCost;
            result.message = "Dijkstra path found.";
            result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
            return result;
        }

        for (const GridCoord& nextCoord : grid.getNeighbors(current.coord, mode)) {
            if (dynBlocked.count(nextCoord) > 0) continue;
            auto nextIt = nodeMap.find(nextCoord);
            if (nextIt != nodeMap.end() && nextIt->second.inClosedSet) continue;

            const double tentativeG = current.gCost + AStar::movementStepCost(current.coord, nextCoord);
            if (nextIt == nodeMap.end() || tentativeG + 1e-9 < nextIt->second.gCost) {
                Node nextNode;
                nextNode.coord = nextCoord;
                nextNode.gCost = tentativeG;
                nextNode.hCost = 0.0;
                nextNode.fCost = tentativeG;
                nextNode.parent = current.coord;
                nextNode.hasParent = true;
                nextNode.inOpenSet = true;
                nodeMap[nextCoord] = nextNode;
                openQueue.push(nextNode);
            }
        }
    }

    result.status = PlanStatus::NoPathFound;
    result.message = "No valid path found.";
    result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
    return result;
}

} // namespace rpp
