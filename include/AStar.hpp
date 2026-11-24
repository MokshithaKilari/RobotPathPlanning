#pragma once

#include "Grid.hpp"
#include "Node.hpp"
#include "Types.hpp"
#include <unordered_map>
#include <vector>

namespace rpp {

enum class PlanStatus {
    None,
    PathFound,
    StartEqualsGoal,
    StartBlocked,
    GoalBlocked,
    NoPathFound
};

struct NeighborEvaluation {
    GridCoord coord{};
    double gCost = 0.0;
    double hCost = 0.0;
    double fCost = 0.0;
    bool blocked = false;
    bool alreadyClosed = false;
    bool acceptedToOpen = false;
    bool updatedBetterPath = false;
    std::string note;
};

struct AStarStep {
    int stepIndex = 0;
    GridCoord current{};
    double currentG = 0.0;
    double currentH = 0.0;
    double currentF = 0.0;
    bool goalReached = false;

    std::vector<NeighborEvaluation> neighbors;
    std::vector<Node> openSnapshot;
    std::vector<Node> closedSnapshot;
    std::vector<GridCoord> currentPath;

    bool hasNextCandidate = false;
    GridCoord nextCandidate{};
    double nextCandidateF = 0.0;
    double nextCandidateG = 0.0;
    double nextCandidateH = 0.0;
};

struct PathResult {
    PlanStatus status = PlanStatus::None;
    std::vector<GridCoord> path;
    int nodesExplored = 0;
    double planningTimeMs = 0.0;
    double totalCost = 0.0;
    std::vector<AStarStep> steps;
    std::unordered_map<GridCoord, Node, CoordHash> finalNodes;
    std::string message;

    bool success() const noexcept {
        return status == PlanStatus::PathFound || status == PlanStatus::StartEqualsGoal;
    }
};

class AStar {
public:
    static double computeHeuristic(GridCoord a, GridCoord b, MovementMode mode) noexcept;
    static double movementStepCost(GridCoord a, GridCoord b) noexcept;

    PathResult findPath(const Grid& grid,
                        GridCoord start,
                        GridCoord goal,
                        MovementMode mode,
                        const std::vector<GridCoord>& blockedDynamicCells = {},
                        bool recordSteps = true) const;
};

inline std::string toString(PlanStatus status) {
    switch (status) {
        case PlanStatus::None:            return "Not Planned";
        case PlanStatus::PathFound:       return "Path Found";
        case PlanStatus::StartEqualsGoal: return "Start Equals Goal";
        case PlanStatus::StartBlocked:    return "Start Blocked";
        case PlanStatus::GoalBlocked:     return "Goal Blocked";
        case PlanStatus::NoPathFound:     return "No Path Available";
    }
    return "Unknown";
}

} // namespace rpp
