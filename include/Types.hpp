#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace rpp {

struct GridCoord {
    int x = 0;
    int y = 0;

    bool operator==(const GridCoord& other) const noexcept {
        return x == other.x && y == other.y;
    }
    bool operator!=(const GridCoord& other) const noexcept {
        return !(*this == other);
    }
    bool operator<(const GridCoord& other) const noexcept {
        return (y != other.y) ? (y < other.y) : (x < other.x);
    }

    std::string toString() const {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

struct CoordHash {
    std::size_t operator()(const GridCoord& c) const noexcept {
        return (static_cast<std::size_t>(c.x) * 73856093u) ^
               (static_cast<std::size_t>(c.y) * 19349663u);
    }
};

enum class MovementMode {
    FourDirectional,
    EightDirectional
};

enum class CellState {
    Empty,
    Start,
    Goal,
    StaticObstacle,
    DynamicObstacle,
    OpenNode,
    ClosedNode,
    CurrentNode,
    PlannedPath,
    TravelledPath
};

enum class DynamicPattern {
    HorizontalPatrol,
    VerticalPatrol,
    RandomWalk,
    PatrolRoute
};

enum class EditTool {
    None,
    SetStart,
    SetGoal,
    AddStaticObstacle,
    RemoveObstacle,
    AddDynamicObstacle
};

enum class SimState {
    Idle,
    Planning,
    Running,
    Paused,
    Replanning,
    Completed,
    Blocked,
    Error
};

enum class AppScreen {
    LabWorkspace,
    PathCalculator,
    Analytics,
    Learning,
    Presentation
};

enum class RightTab {
    MetricsAndLog,
    ExplainAStar,
    CalculatorAndCompare,
    Learning
};

struct ColorRGBA {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

namespace Theme {
    inline constexpr ColorRGBA BgDeepNavy      {10, 14, 26, 255};
    inline constexpr ColorRGBA BgPanel         {16, 22, 39, 255};
    inline constexpr ColorRGBA BgCard          {22, 30, 52, 255};
    inline constexpr ColorRGBA BgCardHover     {30, 40, 68, 255};
    inline constexpr ColorRGBA BorderSubtle    {45, 58, 92, 255};
    inline constexpr ColorRGBA BorderActive    {88, 130, 220, 255};

    inline constexpr ColorRGBA TextPrimary     {236, 242, 252, 255};
    inline constexpr ColorRGBA TextSecondary   {156, 172, 204, 255};
    inline constexpr ColorRGBA TextMuted       {104, 118, 148, 255};

    inline constexpr ColorRGBA AccentCyan      {56, 189, 248, 255};
    inline constexpr ColorRGBA AccentViolet    {139, 92, 246, 255};
    inline constexpr ColorRGBA AccentVioletHi  {167, 139, 250, 255};
    inline constexpr ColorRGBA AccentPurpleDim {76, 52, 128, 170};
    inline constexpr ColorRGBA AccentGreen     {34, 197, 94, 255};
    inline constexpr ColorRGBA AccentTeal      {20, 184, 166, 255};
    inline constexpr ColorRGBA AccentOrange    {249, 115, 22, 255};
    inline constexpr ColorRGBA AccentRed       {239, 68, 68, 255};
    inline constexpr ColorRGBA AccentYellow    {250, 204, 21, 255};

    inline constexpr ColorRGBA StaticBlock     {32, 39, 58, 255};
    inline constexpr ColorRGBA StaticBlockEdge {54, 65, 92, 255};
    inline constexpr ColorRGBA GridEmpty       {14, 19, 34, 255};
    inline constexpr ColorRGBA GridLine        {28, 37, 62, 255};
}

inline std::string toString(MovementMode m) {
    return m == MovementMode::FourDirectional ? "4-Direction (Manhattan)" : "8-Direction (Octile)";
}

inline std::string toString(DynamicPattern p) {
    switch (p) {
        case DynamicPattern::HorizontalPatrol: return "Horizontal Patrol";
        case DynamicPattern::VerticalPatrol:   return "Vertical Patrol";
        case DynamicPattern::RandomWalk:       return "Random Movement";
        case DynamicPattern::PatrolRoute:      return "Patrol Route";
    }
    return "Unknown";
}

inline std::string toString(SimState s) {
    switch (s) {
        case SimState::Idle:       return "IDLE";
        case SimState::Planning:   return "PLANNING";
        case SimState::Running:    return "RUNNING";
        case SimState::Paused:     return "PAUSED";
        case SimState::Replanning: return "REPLANNING";
        case SimState::Completed:  return "DESTINATION REACHED";
        case SimState::Blocked:    return "PATH BLOCKED";
        case SimState::Error:      return "ERROR";
    }
    return "UNKNOWN";
}

inline std::string toString(EditTool t) {
    switch (t) {
        case EditTool::None:               return "Inspect";
        case EditTool::SetStart:           return "Set Start";
        case EditTool::SetGoal:            return "Set Goal";
        case EditTool::AddStaticObstacle:  return "Add Static Obstacle";
        case EditTool::RemoveObstacle:     return "Remove Obstacle";
        case EditTool::AddDynamicObstacle: return "Add Dynamic Obstacle";
    }
    return "Inspect";
}

} // namespace rpp
