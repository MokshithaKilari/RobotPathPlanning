#pragma once

#include "DynamicObstacle.hpp"
#include "Grid.hpp"
#include "Types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace rpp {

struct DynamicObstacleSpec {
    GridCoord startPos{};
    DynamicPattern pattern = DynamicPattern::HorizontalPatrol;
    double speed = 1.5;
    std::vector<GridCoord> route;
};

struct ScenarioData {
    int id = 0;
    std::string name;
    std::string description;
    int gridSize = 20;
    GridCoord start{2, 10};
    GridCoord goal{17, 10};
    MovementMode recommendedMovement = MovementMode::FourDirectional;
    std::vector<GridCoord> staticObstacles;
    std::vector<DynamicObstacleSpec> dynamicObstacles;
    std::string filePath;
};

struct RandomGenConfig {
    int gridSize = 20;
    double obstacleDensity = 0.22; // 0.0 .. 0.45
    int dynamicObstacleCount = 2;
    DynamicPattern dynamicPattern = DynamicPattern::HorizontalPatrol;
    std::uint32_t seed = 42u;
    bool ensureSolvable = true;
};

class ScenarioManager {
public:
    ScenarioManager();

    void loadDirectory(const std::string& scenariosDir);
    const std::vector<ScenarioData>& getScenarios() const noexcept { return m_scenarios; }
    const ScenarioData* getScenarioById(int id) const noexcept;

    bool parseScenarioFile(const std::string& path, ScenarioData& outData) const;
    bool saveScenarioFile(const std::string& path, const ScenarioData& data) const;

    ScenarioData generateRandomEnvironment(const RandomGenConfig& cfg) const;

private:
    void buildBuiltinScenarios();
    std::vector<ScenarioData> m_scenarios;
};

} // namespace rpp
