#include "ScenarioManager.hpp"
#include "AStar.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace rpp {

namespace {

DynamicPattern parsePattern(const std::string& token) {
    if (token == "VERTICAL") return DynamicPattern::VerticalPatrol;
    if (token == "RANDOM")   return DynamicPattern::RandomWalk;
    if (token == "ROUTE")    return DynamicPattern::PatrolRoute;
    return DynamicPattern::HorizontalPatrol;
}

std::string patternToken(DynamicPattern p) {
    switch (p) {
        case DynamicPattern::HorizontalPatrol: return "HORIZONTAL";
        case DynamicPattern::VerticalPatrol:   return "VERTICAL";
        case DynamicPattern::RandomWalk:       return "RANDOM";
        case DynamicPattern::PatrolRoute:      return "ROUTE";
    }
    return "HORIZONTAL";
}

} // namespace

ScenarioManager::ScenarioManager() {
    buildBuiltinScenarios();
}

void ScenarioManager::buildBuiltinScenarios() {
    m_scenarios.clear();

    // 1. Open Environment
    {
        ScenarioData s;
        s.id = 1;
        s.name = "Open Environment";
        s.description = "Obstacle-free 20x20 workspace demonstrating baseline A* heuristic efficiency.";
        s.gridSize = 20;
        s.start = {2, 10};
        s.goal = {17, 10};
        m_scenarios.push_back(s);
    }

    // 2. Static Obstacle Navigation
    {
        ScenarioData s;
        s.id = 2;
        s.name = "Static Obstacle Navigation";
        s.description = "Multi-barrier static layout testing A* detour routing around solid walls.";
        s.gridSize = 20;
        s.start = {2, 10};
        s.goal = {17, 10};
        for (int y = 4; y <= 14; ++y) s.staticObstacles.push_back({7, y});
        for (int y = 1; y <= 11; ++y) s.staticObstacles.push_back({12, y});
        for (int x = 8; x <= 11; ++x) s.staticObstacles.push_back({x, 14});
        m_scenarios.push_back(s);
    }

    // 3. Maze
    {
        ScenarioData s;
        s.id = 3;
        s.name = "Maze";
        s.description = "Structured labyrinth requiring backtracking and deep heuristic search.";
        s.gridSize = 20;
        s.start = {1, 1};
        s.goal = {18, 18};
        for (int y = 0; y < 17; ++y) s.staticObstacles.push_back({3, y});
        for (int y = 3; y < 20; ++y) s.staticObstacles.push_back({6, y});
        for (int y = 0; y < 16; ++y) s.staticObstacles.push_back({9, y});
        for (int y = 4; y < 20; ++y) s.staticObstacles.push_back({12, y});
        for (int y = 0; y < 16; ++y) s.staticObstacles.push_back({15, y});
        s.staticObstacles.push_back({11, 7});
        s.staticObstacles.push_back({13, 11});
        s.staticObstacles.push_back({16, 15});
        s.staticObstacles.push_back({17, 15});
        m_scenarios.push_back(s);
    }

    // 4. Dynamic Obstacle Crossing
    {
        ScenarioData s;
        s.id = 4;
        s.name = "Dynamic Obstacle Crossing";
        s.description = "Single patrolling obstacle crossing the primary corridor, triggering live replanning.";
        s.gridSize = 20;
        s.start = {2, 10};
        s.goal = {17, 10};
        for (int y = 0; y <= 7; ++y) s.staticObstacles.push_back({10, y});
        for (int y = 13; y <= 19; ++y) s.staticObstacles.push_back({10, y});
        s.dynamicObstacles.push_back({{10, 8}, DynamicPattern::VerticalPatrol, 1.8, {}});
        m_scenarios.push_back(s);
    }

    // 5. Multiple Dynamic Obstacles
    {
        ScenarioData s;
        s.id = 5;
        s.name = "Multiple Dynamic Obstacles";
        s.description = "Four autonomous moving obstacles with mixed patrol patterns.";
        s.gridSize = 20;
        s.start = {1, 10};
        s.goal = {18, 10};
        for (int y = 4; y <= 7; ++y) s.staticObstacles.push_back({6, y});
        for (int y = 12; y <= 15; ++y) s.staticObstacles.push_back({13, y});
        s.dynamicObstacles.push_back({{6, 10}, DynamicPattern::VerticalPatrol, 1.6, {}});
        s.dynamicObstacles.push_back({{10, 6}, DynamicPattern::HorizontalPatrol, 1.9, {}});
        s.dynamicObstacles.push_back({{13, 9}, DynamicPattern::VerticalPatrol, 1.7, {}});
        s.dynamicObstacles.push_back({{15, 14}, DynamicPattern::RandomWalk, 1.4, {}});
        m_scenarios.push_back(s);
    }

    // 6. Blocked Optimal Route
    {
        ScenarioData s;
        s.id = 6;
        s.name = "Blocked Optimal Route";
        s.description = "Direct central gate starts open then closes behind a patrolling block, forcing detour.";
        s.gridSize = 20;
        s.start = {2, 10};
        s.goal = {17, 10};
        for (int y = 3; y <= 16; ++y) {
            if (y != 10) s.staticObstacles.push_back({10, y});
        }
        s.staticObstacles.push_back({9, 8});
        s.staticObstacles.push_back({9, 12});
        s.dynamicObstacles.push_back({{10, 10}, DynamicPattern::HorizontalPatrol, 1.2, {}});
        m_scenarios.push_back(s);
    }

    // 7. Narrow Corridor
    {
        ScenarioData s;
        s.id = 7;
        s.name = "Narrow Corridor";
        s.description = "Tight 1-cell bottleneck passage with alternate outer bypass loop.";
        s.gridSize = 20;
        s.start = {2, 10};
        s.goal = {17, 10};
        for (int x = 5; x <= 14; ++x) {
            s.staticObstacles.push_back({x, 8});
            s.staticObstacles.push_back({x, 12});
        }
        for (int y = 2; y <= 7; ++y) s.staticObstacles.push_back({9, y});
        for (int y = 13; y <= 17; ++y) s.staticObstacles.push_back({9, y});
        s.dynamicObstacles.push_back({{12, 10}, DynamicPattern::HorizontalPatrol, 1.3, {}});
        m_scenarios.push_back(s);
    }

    // 8. No Path Available
    {
        ScenarioData s;
        s.id = 8;
        s.name = "No Path Available";
        s.description = "Completely sealed enclosure testing graceful A* failure handling.";
        s.gridSize = 20;
        s.start = {3, 10};
        s.goal = {16, 10};
        for (int y = 0; y < 20; ++y) {
            s.staticObstacles.push_back({10, y});
        }
        m_scenarios.push_back(s);
    }

    // 9. Random Environment
    {
        RandomGenConfig cfg;
        cfg.gridSize = 20;
        cfg.obstacleDensity = 0.22;
        cfg.dynamicObstacleCount = 2;
        cfg.dynamicPattern = DynamicPattern::RandomWalk;
        cfg.seed = 2026u;
        cfg.ensureSolvable = true;
        ScenarioData s = generateRandomEnvironment(cfg);
        s.id = 9;
        s.name = "Random Environment";
        s.description = "Procedurally generated solvable 20x20 obstacle field (Seed 2026).";
        m_scenarios.push_back(s);
    }

    // 10. Complex Dynamic Environment
    {
        ScenarioData s;
        s.id = 10;
        s.name = "Complex Dynamic Environment";
        s.description = "Capstone stress test: static chambers plus 5 coordinated dynamic patrols.";
        s.gridSize = 20;
        s.start = {1, 2};
        s.goal = {18, 17};
        for (int y = 0; y <= 11; ++y) s.staticObstacles.push_back({5, y});
        for (int y = 7; y <= 19; ++y) s.staticObstacles.push_back({10, y});
        for (int y = 0; y <= 12; ++y) s.staticObstacles.push_back({15, y});
        for (int x = 6; x <= 9; ++x) s.staticObstacles.push_back({x, 14});
        for (int x = 11; x <= 14; ++x) s.staticObstacles.push_back({x, 5});

        s.dynamicObstacles.push_back({{3, 14}, DynamicPattern::HorizontalPatrol, 1.5, {}});
        s.dynamicObstacles.push_back({{8, 9}, DynamicPattern::VerticalPatrol, 1.8, {}});
        s.dynamicObstacles.push_back({{12, 11}, DynamicPattern::HorizontalPatrol, 1.6, {}});
        s.dynamicObstacles.push_back({{17, 6}, DynamicPattern::VerticalPatrol, 1.7, {}});
        s.dynamicObstacles.push_back({{14, 15}, DynamicPattern::PatrolRoute, 1.6, {{14, 15}, {18, 15}, {18, 18}, {14, 18}}});
        m_scenarios.push_back(s);
    }
}

void ScenarioManager::loadDirectory(const std::string& scenariosDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(scenariosDir, ec) || !fs::is_directory(scenariosDir, ec)) {
        return;
    }

    std::vector<ScenarioData> loaded;
    for (const auto& entry : fs::directory_iterator(scenariosDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".scen") continue;
        ScenarioData parsed;
        if (parseScenarioFile(entry.path().string(), parsed)) {
            loaded.push_back(std::move(parsed));
        }
    }

    if (!loaded.empty()) {
        std::sort(loaded.begin(), loaded.end(), [](const ScenarioData& a, const ScenarioData& b) {
            return a.id < b.id;
        });
        m_scenarios = std::move(loaded);
    }
}

const ScenarioData* ScenarioManager::getScenarioById(int id) const noexcept {
    for (const auto& s : m_scenarios) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

bool ScenarioManager::parseScenarioFile(const std::string& path, ScenarioData& outData) const {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    outData = ScenarioData{};
    outData.filePath = path;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key == "ID") {
            iss >> outData.id;
        } else if (key == "NAME") {
            std::getline(iss >> std::ws, outData.name);
        } else if (key == "DESC") {
            std::getline(iss >> std::ws, outData.description);
        } else if (key == "GRID_SIZE") {
            iss >> outData.gridSize;
        } else if (key == "START") {
            iss >> outData.start.x >> outData.start.y;
        } else if (key == "GOAL") {
            iss >> outData.goal.x >> outData.goal.y;
        } else if (key == "MOVEMENT") {
            int dirs = 4;
            iss >> dirs;
            outData.recommendedMovement = (dirs == 8) ? MovementMode::EightDirectional : MovementMode::FourDirectional;
        } else if (key == "STATIC") {
            GridCoord c;
            if (iss >> c.x >> c.y) {
                outData.staticObstacles.push_back(c);
            }
        } else if (key == "DYNAMIC") {
            DynamicObstacleSpec spec;
            std::string pat;
            if (iss >> spec.startPos.x >> spec.startPos.y >> pat >> spec.speed) {
                spec.pattern = parsePattern(pat);
                int rx = 0, ry = 0;
                while (iss >> rx >> ry) {
                    spec.route.push_back({rx, ry});
                }
                outData.dynamicObstacles.push_back(std::move(spec));
            }
        }
    }

    return !outData.name.empty() && outData.gridSize >= 5;
}

bool ScenarioManager::saveScenarioFile(const std::string& path, const ScenarioData& data) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "# RobotPathPlanning Scenario File\n";
    out << "ID " << data.id << "\n";
    out << "NAME " << data.name << "\n";
    out << "DESC " << data.description << "\n";
    out << "GRID_SIZE " << data.gridSize << "\n";
    out << "START " << data.start.x << " " << data.start.y << "\n";
    out << "GOAL " << data.goal.x << " " << data.goal.y << "\n";
    out << "MOVEMENT " << (data.recommendedMovement == MovementMode::EightDirectional ? 8 : 4) << "\n";
    for (const auto& s : data.staticObstacles) {
        out << "STATIC " << s.x << " " << s.y << "\n";
    }
    for (const auto& d : data.dynamicObstacles) {
        out << "DYNAMIC " << d.startPos.x << " " << d.startPos.y << " "
            << patternToken(d.pattern) << " " << d.speed;
        for (const auto& rp : d.route) {
            out << " " << rp.x << " " << rp.y;
        }
        out << "\n";
    }
    return true;
}

ScenarioData ScenarioManager::generateRandomEnvironment(const RandomGenConfig& cfg) const {
    ScenarioData s;
    s.id = 900;
    s.name = "Random (" + std::to_string(cfg.gridSize) + "x" + std::to_string(cfg.gridSize) +
             ", Seed " + std::to_string(cfg.seed) + ")";
    s.description = "Randomly generated environment with density " +
                    std::to_string(static_cast<int>(cfg.obstacleDensity * 100.0)) + "%.";
    s.gridSize = std::clamp(cfg.gridSize, 10, 30);
    s.start = {1, s.gridSize / 2};
    s.goal = {s.gridSize - 2, s.gridSize / 2};

    std::mt19937 rng(cfg.seed);
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    AStar astar;

    const int maxAttempts = cfg.ensureSolvable ? 25 : 1;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        Grid testGrid(s.gridSize);
        testGrid.setStart(s.start);
        testGrid.setGoal(s.goal);
        std::vector<GridCoord> candidateStatics;

        for (int y = 0; y < s.gridSize; ++y) {
            for (int x = 0; x < s.gridSize; ++x) {
                GridCoord c{x, y};
                if (c == s.start || c == s.goal) continue;
                // Keep immediate 1-cell neighborhood of start/goal sparse for clean start
                if (std::abs(x - s.start.x) + std::abs(y - s.start.y) <= 1) continue;
                if (std::abs(x - s.goal.x) + std::abs(y - s.goal.y) <= 1) continue;
                if (prob(rng) < cfg.obstacleDensity) {
                    testGrid.addStaticObstacle(c);
                    candidateStatics.push_back(c);
                }
            }
        }

        if (!cfg.ensureSolvable ||
            astar.findPath(testGrid, s.start, s.goal, MovementMode::FourDirectional, {}, false).success()) {
            s.staticObstacles = std::move(candidateStatics);
            break;
        }
        if (attempt == maxAttempts - 1) {
            // Fallback: keep last generated layout but clear a corridor if needed
            s.staticObstacles = std::move(candidateStatics);
        }
    }

    // Place dynamic obstacles on free cells away from start/goal
    Grid finalGrid(s.gridSize);
    finalGrid.setStart(s.start);
    finalGrid.setGoal(s.goal);
    for (const auto& st : s.staticObstacles) {
        finalGrid.addStaticObstacle(st);
    }

    std::vector<GridCoord> freeCells;
    for (int y = 0; y < s.gridSize; ++y) {
        for (int x = 0; x < s.gridSize; ++x) {
            GridCoord c{x, y};
            if (!finalGrid.isWalkable(c)) continue;
            if (c == s.start || c == s.goal) continue;
            if (std::abs(x - s.start.x) + std::abs(y - s.start.y) <= 2) continue;
            freeCells.push_back(c);
        }
    }
    std::shuffle(freeCells.begin(), freeCells.end(), rng);

    const int dynCount = std::min(cfg.dynamicObstacleCount, static_cast<int>(freeCells.size()));
    for (int i = 0; i < dynCount; ++i) {
        DynamicObstacleSpec spec;
        spec.startPos = freeCells[static_cast<std::size_t>(i)];
        spec.pattern = cfg.dynamicPattern;
        spec.speed = 1.5;
        if (spec.pattern == DynamicPattern::PatrolRoute) {
            spec.route = {
                spec.startPos,
                {std::clamp(spec.startPos.x + 2, 0, s.gridSize - 1), spec.startPos.y},
                {std::clamp(spec.startPos.x + 2, 0, s.gridSize - 1), std::clamp(spec.startPos.y + 2, 0, s.gridSize - 1)},
                {spec.startPos.x, std::clamp(spec.startPos.y + 2, 0, s.gridSize - 1)}
            };
        }
        s.dynamicObstacles.push_back(std::move(spec));
    }

    return s;
}

} // namespace rpp
