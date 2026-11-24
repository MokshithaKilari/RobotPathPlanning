#include "AStar.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <queue>
#include <unordered_set>

namespace rpp {

namespace {

std::vector<GridCoord> reconstructPath(
    const std::unordered_map<GridCoord, Node, CoordHash>& nodes,
    GridCoord start,
    GridCoord target) {
    std::vector<GridCoord> path;
    GridCoord cur = target;
    std::size_t guard = 0;
    const std::size_t maxGuard = nodes.size() + 4;
    while (guard++ < maxGuard) {
        path.push_back(cur);
        if (cur == start) break;
        auto it = nodes.find(cur);
        if (it == nodes.end() || !it->second.hasParent) break;
        cur = it->second.parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace

double AStar::computeHeuristic(GridCoord a, GridCoord b, MovementMode mode) noexcept {
    const double dx = static_cast<double>(std::abs(a.x - b.x));
    const double dy = static_cast<double>(std::abs(a.y - b.y));
    if (mode == MovementMode::FourDirectional) {
        // Admissible Manhattan heuristic for 4-way unit-cost grid
        return dx + dy;
    }
    // Admissible Octile heuristic for 8-way grid (cardinal = 1.0, diagonal = sqrt(2))
    constexpr double kDiag = 1.4142135623730950488;
    const double dMin = std::min(dx, dy);
    const double dMax = std::max(dx, dy);
    return (dMax - dMin) + kDiag * dMin;
}

double AStar::movementStepCost(GridCoord a, GridCoord b) noexcept {
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    return (dx + dy == 2) ? 1.4142135623730950488 : 1.0;
}

PathResult AStar::findPath(const Grid& grid,
                           GridCoord start,
                           GridCoord goal,
                           MovementMode mode,
                           const std::vector<GridCoord>& blockedDynamicCells,
                           bool recordSteps) const {
    using Clock = std::chrono::high_resolution_clock;
    const auto tStart = Clock::now();

    PathResult result;
    std::unordered_set<GridCoord, CoordHash> dynBlocked(blockedDynamicCells.begin(), blockedDynamicCells.end());

    if (!grid.isWalkable(start) || dynBlocked.count(start) > 0) {
        result.status = PlanStatus::StartBlocked;
        result.message = "Start position " + start.toString() + " is inside an obstacle or out of bounds.";
        result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
        return result;
    }

    if (!grid.isWalkable(goal) || dynBlocked.count(goal) > 0) {
        result.status = PlanStatus::GoalBlocked;
        result.message = "Goal position " + goal.toString() + " is inside an obstacle or out of bounds.";
        result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
        return result;
    }

    if (start == goal) {
        result.status = PlanStatus::StartEqualsGoal;
        result.path = {start};
        result.nodesExplored = 1;
        result.totalCost = 0.0;
        result.message = "Start position equals Goal position " + start.toString() + ".";
        result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
        return result;
    }

    std::priority_queue<Node, std::vector<Node>, NodeCompare> openQueue;
    std::unordered_map<GridCoord, Node, CoordHash> nodeMap;
    nodeMap.reserve(static_cast<std::size_t>(grid.getSize() * grid.getSize()));

    Node startNode;
    startNode.coord = start;
    startNode.gCost = 0.0;
    startNode.hCost = computeHeuristic(start, goal, mode);
    startNode.recomputeF();
    startNode.inOpenSet = true;

    nodeMap[start] = startNode;
    openQueue.push(startNode);

    int stepCounter = 0;

    while (!openQueue.empty()) {
        const Node popped = openQueue.top();
        openQueue.pop();

        auto curIt = nodeMap.find(popped.coord);
        if (curIt == nodeMap.end() || curIt->second.inClosedSet) {
            continue;
        }

        // Skip stale queue entries whose G cost was superseded
        if (popped.gCost > curIt->second.gCost + 1e-9) {
            continue;
        }

        curIt->second.inOpenSet = false;
        curIt->second.inClosedSet = true;
        const Node current = curIt->second;
        ++result.nodesExplored;

        AStarStep step;
        if (recordSteps) {
            step.stepIndex = stepCounter++;
            step.current = current.coord;
            step.currentG = current.gCost;
            step.currentH = current.hCost;
            step.currentF = current.fCost;
        }

        if (current.coord == goal) {
            result.status = PlanStatus::PathFound;
            result.path = reconstructPath(nodeMap, start, goal);
            result.totalCost = current.gCost;
            result.message = "Optimal path found: " + std::to_string(result.path.size()) + " cells.";
            if (recordSteps) {
                step.goalReached = true;
                step.currentPath = result.path;
                for (const auto& kv : nodeMap) {
                    if (kv.second.inOpenSet) step.openSnapshot.push_back(kv.second);
                    if (kv.second.inClosedSet) step.closedSnapshot.push_back(kv.second);
                }
                result.steps.push_back(std::move(step));
            }
            result.finalNodes = std::move(nodeMap);
            result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
            return result;
        }

        // Expand walkable neighbors (including checking dynamic obstacle exclusions)
        const std::vector<GridCoord> rawNeighbors = grid.getNeighbors(current.coord, mode);
        for (const GridCoord& nextCoord : rawNeighbors) {
            const bool isDynBlocked = dynBlocked.count(nextCoord) > 0;
            if (isDynBlocked) {
                if (recordSteps) {
                    NeighborEvaluation eval;
                    eval.coord = nextCoord;
                    eval.blocked = true;
                    eval.note = "Blocked by dynamic obstacle";
                    step.neighbors.push_back(std::move(eval));
                }
                continue;
            }

            auto nextIt = nodeMap.find(nextCoord);
            if (nextIt != nodeMap.end() && nextIt->second.inClosedSet) {
                if (recordSteps) {
                    NeighborEvaluation eval;
                    eval.coord = nextCoord;
                    eval.gCost = nextIt->second.gCost;
                    eval.hCost = nextIt->second.hCost;
                    eval.fCost = nextIt->second.fCost;
                    eval.alreadyClosed = true;
                    eval.note = "Already evaluated in Closed Set";
                    step.neighbors.push_back(std::move(eval));
                }
                continue;
            }

            const double tentativeG = current.gCost + movementStepCost(current.coord, nextCoord);
            const double h = computeHeuristic(nextCoord, goal, mode);
            const double f = tentativeG + h;

            bool shouldInsert = false;
            bool isUpdate = false;

            if (nextIt == nodeMap.end()) {
                shouldInsert = true;
            } else if (tentativeG + 1e-9 < nextIt->second.gCost) {
                shouldInsert = true;
                isUpdate = true;
            }

            if (shouldInsert) {
                Node nextNode;
                nextNode.coord = nextCoord;
                nextNode.gCost = tentativeG;
                nextNode.hCost = h;
                nextNode.fCost = f;
                nextNode.parent = current.coord;
                nextNode.hasParent = true;
                nextNode.inOpenSet = true;
                nextNode.inClosedSet = false;

                nodeMap[nextCoord] = nextNode;
                openQueue.push(nextNode);
            }

            if (recordSteps) {
                NeighborEvaluation eval;
                eval.coord = nextCoord;
                eval.gCost = tentativeG;
                eval.hCost = h;
                eval.fCost = f;
                eval.acceptedToOpen = shouldInsert;
                eval.updatedBetterPath = isUpdate;
                if (isUpdate) {
                    eval.note = "Lower G cost found; updated parent & pushed to Open Set";
                } else if (shouldInsert) {
                    eval.note = "Added to Open Set";
                } else {
                    eval.note = "Existing Open Set path is equal or better";
                }
                step.neighbors.push_back(std::move(eval));
            }
        }

        if (recordSteps) {
            step.currentPath = reconstructPath(nodeMap, start, current.coord);
            const Node* bestNext = nullptr;
            NodeCompare worseThan;
            for (const auto& kv : nodeMap) {
                if (kv.second.inOpenSet) {
                    step.openSnapshot.push_back(kv.second);
                    if (!bestNext || worseThan(*bestNext, kv.second)) {
                        bestNext = &kv.second;
                    }
                } else if (kv.second.inClosedSet) {
                    step.closedSnapshot.push_back(kv.second);
                }
            }
            if (bestNext) {
                step.hasNextCandidate = true;
                step.nextCandidate = bestNext->coord;
                step.nextCandidateF = bestNext->fCost;
                step.nextCandidateG = bestNext->gCost;
                step.nextCandidateH = bestNext->hCost;
            }
            result.steps.push_back(std::move(step));
        }
    }

    result.status = PlanStatus::NoPathFound;
    result.message = "No valid path exists between " + start.toString() + " and " + goal.toString() + ".";
    result.finalNodes = std::move(nodeMap);
    result.planningTimeMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
    return result;
}

} // namespace rpp
