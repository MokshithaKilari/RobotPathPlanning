#pragma once

#include "AStar.hpp"
#include "Types.hpp"
#include <string>
#include <vector>

namespace rpp {

struct ReplanNotice {
    bool active = false;
    GridCoord obstacleCoord{};
    std::string reason;
    int newPathLength = 0;
    int nodesExplored = 0;
    int replanCount = 0;
    bool replanSucceeded = true;
};

struct EducationalTopic {
    std::string title;
    std::string summary;
};

class ExplanationEngine {
public:
    static std::string buildStepExplanation(const PathResult& result,
                                            std::size_t stepIndex,
                                            MovementMode mode);

    static std::string buildReplanExplanation(const ReplanNotice& notice);

    static std::string buildNodeHoverTooltip(const AStarStep* step,
                                             GridCoord hovered,
                                             bool isStaticObstacle,
                                             bool isDynamicObstacle);

    static std::vector<EducationalTopic> getEducationalTopics();
};

} // namespace rpp
