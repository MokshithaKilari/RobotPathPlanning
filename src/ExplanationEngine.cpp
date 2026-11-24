#include "ExplanationEngine.hpp"
#include <iomanip>
#include <sstream>

namespace rpp {

namespace {
std::string fmtNum(double val) {
    std::ostringstream oss;
    if (std::abs(val - std::round(val)) < 1e-4) {
        oss << static_cast<long long>(std::llround(val));
    } else {
        oss << std::fixed << std::setprecision(2) << val;
    }
    return oss.str();
}
} // namespace

std::string ExplanationEngine::buildStepExplanation(const PathResult& result,
                                                    std::size_t stepIndex,
                                                    MovementMode mode) {
    if (result.steps.empty()) {
        if (result.status == PlanStatus::None) {
            return "A* Explanation\n\nClick 'Calculate A* Path' or 'Visualize A*' to generate step-by-step search calculations.";
        }
        return "A* Explanation\n\n" + result.message;
    }

    const std::size_t idx = std::min(stepIndex, result.steps.size() - 1);
    const AStarStep& step = result.steps[idx];

    std::ostringstream oss;
    oss << "A* Step " << (idx + 1) << " / " << result.steps.size() << "\n";
    oss << "Heuristic: " << (mode == MovementMode::FourDirectional ? "Manhattan" : "Octile") << "\n\n";
    oss << "Current Node: " << step.current.toString() << "\n";
    oss << "G = " << fmtNum(step.currentG)
        << "   H = " << fmtNum(step.currentH)
        << "   F = G + H = " << fmtNum(step.currentF) << "\n\n";

    if (step.goalReached) {
        oss << "GOAL REACHED!\n";
        oss << "The current node matches the Goal.\n";
        oss << "Parent pointers are traced back to construct the optimal path of "
            << result.path.size() << " cells (total cost " << fmtNum(result.totalCost) << ").";
        return oss.str();
    }

    oss << "Neighbor Evaluation:\n";
    if (step.neighbors.empty()) {
        oss << "  No walkable neighbors from this cell.\n";
    } else {
        for (const auto& nb : step.neighbors) {
            oss << "  " << nb.coord.toString() << " -> ";
            if (nb.blocked) {
                oss << "BLOCKED (" << nb.note << ")\n";
            } else if (nb.alreadyClosed) {
                oss << "CLOSED (skip)\n";
            } else {
                oss << "G=" << fmtNum(nb.gCost)
                    << " H=" << fmtNum(nb.hCost)
                    << " F=" << fmtNum(nb.fCost);
                if (nb.updatedBetterPath) {
                    oss << " [UPDATED]";
                } else if (nb.acceptedToOpen) {
                    oss << " [OPEN]";
                }
                oss << "\n";
            }
        }
    }

    oss << "\nOpen Set: " << step.openSnapshot.size()
        << " | Closed Set: " << step.closedSnapshot.size() << "\n\n";

    if (step.hasNextCandidate) {
        oss << "Next Selection:\n";
        oss << "Node " << step.nextCandidate.toString()
            << " has the lowest F cost (" << fmtNum(step.nextCandidateF)
            << " = " << fmtNum(step.nextCandidateG) << " + " << fmtNum(step.nextCandidateH)
            << "), so it is selected next from the priority queue.";
    } else {
        oss << "Open Set is empty — no remaining candidates to explore.";
    }

    return oss.str();
}

std::string ExplanationEngine::buildReplanExplanation(const ReplanNotice& notice) {
    if (!notice.active) {
        return "";
    }
    std::ostringstream oss;
    oss << "PATH INTERRUPTION DETECTED\n\n";
    oss << "Obstacle detected at: " << notice.obstacleCoord.toString() << "\n";
    oss << "Cause: " << notice.reason << "\n\n";
    oss << "Replanning sequence:\n";
    oss << "1. Robot movement paused safely.\n";
    oss << "2. Invalidated blocked path.\n";
    oss << "3. Invoked A* from current robot cell.\n";
    if (notice.replanSucceeded) {
        oss << "4. New route found (" << notice.newPathLength << " cells, "
            << notice.nodesExplored << " nodes explored).\n";
        oss << "5. Navigation resumed.\n";
    } else {
        oss << "4. WARNING: No alternative route available!\n";
    }
    oss << "\nReplanning count: " << notice.replanCount;
    return oss.str();
}

std::string ExplanationEngine::buildNodeHoverTooltip(const AStarStep* step,
                                                     GridCoord hovered,
                                                     bool isStaticObstacle,
                                                     bool isDynamicObstacle) {
    std::ostringstream oss;
    oss << "Cell " << hovered.toString();
    if (isStaticObstacle) {
        oss << " | Static Obstacle (Non-traversable)";
        return oss.str();
    }
    if (isDynamicObstacle) {
        oss << " | Dynamic Obstacle (Moving hazard)";
        return oss.str();
    }
    if (!step) {
        oss << " | Walkable Cell";
        return oss.str();
    }
    if (hovered == step->current) {
        oss << " | CURRENT NODE | G=" << fmtNum(step->currentG)
            << " H=" << fmtNum(step->currentH) << " F=" << fmtNum(step->currentF);
        return oss.str();
    }
    for (const auto& n : step->openSnapshot) {
        if (n.coord == hovered) {
            oss << " | OPEN SET | G=" << fmtNum(n.gCost)
                << " H=" << fmtNum(n.hCost) << " F=" << fmtNum(n.fCost);
            return oss.str();
        }
    }
    for (const auto& n : step->closedSnapshot) {
        if (n.coord == hovered) {
            oss << " | CLOSED SET | G=" << fmtNum(n.gCost)
                << " H=" << fmtNum(n.hCost) << " F=" << fmtNum(n.fCost);
            return oss.str();
        }
    }
    oss << " | Unexplored Walkable Cell";
    return oss.str();
}

std::vector<EducationalTopic> ExplanationEngine::getEducationalTopics() {
    return {
        {"What is A*?",
         "A* (A-Star) is an informed best-first graph search algorithm that finds the shortest valid path from a start node to a goal node by combining exact path cost with a heuristic estimate."},
        {"Why is A* useful?",
         "Unlike Dijkstra, which expands uniformly in all directions, A* focuses exploration toward the goal. This yields the exact shortest path while exploring significantly fewer cells."},
        {"What is the heuristic h(n)?",
         "The heuristic estimates the remaining distance from node n to the goal. For 4-directional movement we use Manhattan distance |dx| + |dy|; for 8-directional movement we use Octile distance."},
        {"What is G cost?",
         "g(n) is the exact cumulative movement cost from the Start cell to node n along the current best known path (1.0 per cardinal step, 1.414 per diagonal step)."},
        {"What is H cost?",
         "h(n) is the admissible heuristic estimate from node n to the Goal. Because it never overestimates true distance, A* guarantees an optimal shortest path."},
        {"What is F cost?",
         "f(n) = g(n) + h(n) represents the estimated total path cost passing through node n. At every step, A* pops the Open Set node with the smallest f(n)."},
        {"Why was this node selected?",
         "At each iteration, A* inspects the priority queue and selects the node with minimum F cost (breaking ties by lower H cost). This balances shortest distance already travelled with proximity to the target."},
        {"Why did the robot replan?",
         "Before stepping into the next waypoint, the simulation checks if any static obstacle or moving dynamic obstacle blocks or will enter the planned path. On detection, the robot halts, invalidates the stale route, and invokes A* from its current cell."},
        {"Why can the robot not move through this cell?",
         "Obstacle cells (static blocks or cells occupied/predicted by dynamic obstacles) are excluded during neighbor expansion and collision checks so the robot maintains collision-free navigation."}
    };
}

} // namespace rpp
