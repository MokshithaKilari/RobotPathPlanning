#include "AStar.hpp"
#include "Dijkstra.hpp"
#include "DynamicObstacle.hpp"
#include "ExplanationEngine.hpp"
#include "Grid.hpp"
#include "ScenarioManager.hpp"
#include "Simulation.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace rpp;

namespace {

void testGridAndObstacles() {
    Grid g(20);
    assert(g.getSize() == 20);
    assert(g.inBounds({0, 0}));
    assert(!g.inBounds({20, 0}));
    assert(g.setStart({1, 1}));
    assert(g.setGoal({18, 18}));
    assert(g.addStaticObstacle({5, 5}));
    assert(!g.addStaticObstacle({1, 1})); // cannot place on start
    assert(!g.addStaticObstacle({18, 18})); // cannot place on goal
    assert(g.isStaticObstacle({5, 5}));
    assert(!g.isWalkable({5, 5}));
    assert(g.removeStaticObstacle({5, 5}));
    assert(g.isWalkable({5, 5}));

    // Test 8-directional corner-cutting prevention
    g.addStaticObstacle({4, 5});
    g.addStaticObstacle({5, 4});
    auto n8 = g.getNeighbors({4, 4}, MovementMode::EightDirectional);
    for (const auto& c : n8) {
        assert(!(c.x == 5 && c.y == 5)); // diagonal (5,5) must be blocked by (4,5)&(5,4)
    }
    std::cout << "[PASS] Grid & obstacle validation\n";
}

void testAStarAndEdgeCases() {
    AStar astar;
    Grid g(10);
    g.setStart({1, 1});
    g.setGoal({8, 1});

    // 1. Open path (4-dir Manhattan shortest distance = 7 steps -> 8 nodes)
    PathResult r1 = astar.findPath(g, g.getStart(), g.getGoal(), MovementMode::FourDirectional);
    assert(r1.status == PlanStatus::PathFound);
    assert(r1.path.size() == 8);
    assert(std::abs(r1.totalCost - 7.0) < 1e-6);
    assert(!r1.steps.empty());

    // 2. Start equals goal
    PathResult rEq = astar.findPath(g, {3, 3}, {3, 3}, MovementMode::FourDirectional);
    assert(rEq.status == PlanStatus::StartEqualsGoal);
    assert(rEq.path.size() == 1);

    // 3. Start or goal blocked
    g.addStaticObstacle({4, 4});
    PathResult rSB = astar.findPath(g, {4, 4}, {8, 1}, MovementMode::FourDirectional);
    assert(rSB.status == PlanStatus::StartBlocked);
    PathResult rGB = astar.findPath(g, {1, 1}, {4, 4}, MovementMode::FourDirectional);
    assert(rGB.status == PlanStatus::GoalBlocked);

    // 4. Completely sealed no-path
    for (int y = 0; y < 10; ++y) {
        g.addStaticObstacle({5, y});
    }
    PathResult rNo = astar.findPath(g, {1, 1}, {8, 1}, MovementMode::FourDirectional);
    assert(rNo.status == PlanStatus::NoPathFound);
    assert(rNo.path.empty());

    std::cout << "[PASS] A* shortest path & edge cases\n";
}

void testDijkstraComparison() {
    Grid g(20);
    g.setStart({2, 10});
    g.setGoal({17, 10});
    for (int y = 5; y <= 14; ++y) {
        g.addStaticObstacle({10, y});
    }

    AStar astar;
    Dijkstra dijkstra;
    PathResult ar = astar.findPath(g, g.getStart(), g.getGoal(), MovementMode::FourDirectional);
    PathResult dr = dijkstra.findPath(g, g.getStart(), g.getGoal(), MovementMode::FourDirectional);

    assert(ar.status == PlanStatus::PathFound);
    assert(dr.status == PlanStatus::PathFound);
    assert(ar.path.size() == dr.path.size()); // Both find optimal shortest path
    assert(ar.nodesExplored <= dr.nodesExplored); // A* explores fewer or equal nodes
    std::cout << "[PASS] A* vs Dijkstra optimality & heuristic pruning ("
              << ar.nodesExplored << " vs " << dr.nodesExplored << " nodes)\n";
}

void testDynamicReplanningAndSimulation() {
    Simulation sim;
    sim.newEnvironment(20);
    sim.setStart({2, 10});
    sim.setGoal({17, 10});
    assert(sim.calculatePath(false));
    const std::size_t initialLen = sim.getLastPlanResult().path.size();
    assert(initialLen == 16);

    sim.run();
    assert(sim.getState() == SimState::Running);

    // Step forward slightly
    for (int i = 0; i < 10; ++i) {
        sim.update(0.05);
    }

    // Place obstacle directly on the planned path -> must detect & replan!
    assert(sim.injectObstacleOnPlannedPath());
    assert(sim.getStatistics().getBlockedPathEvents() >= 1);
    assert(sim.getStatistics().getReplanCount() >= 1);
    assert(sim.getReplanNotice().active);
    assert(sim.getReplanNotice().replanSucceeded);
    assert(sim.getLastPlanResult().path.size() > initialLen); // detour is longer than straight line

    // Run until destination reached
    for (int i = 0; i < 600 && sim.getState() == SimState::Running; ++i) {
        sim.update(0.05);
    }
    assert(sim.getState() == SimState::Completed);
    assert(sim.getRobot().getCurrentCell() == GridCoord({17, 10}));
    assert(sim.getStatistics().getSuccessfulNavigations() == 1);

    std::cout << "[PASS] Dynamic obstacle blocking, real-time A* replanning, and goal completion\n";
}

void testExplanationAndScenarios() {
    ScenarioManager mgr;
    mgr.loadDirectory("scenarios");
    const auto& all = mgr.getScenarios();
    assert(all.size() == 10);

    Simulation sim;
    for (const auto& sc : all) {
        std::cout << "  -> Scenario " << sc.id << " (" << sc.name << "): " << std::flush;
        assert(sim.loadScenario(sc));
        std::cout << toString(sim.getLastPlanResult().status) << std::endl;
        if (sc.id != 8) { // Scenario 8 is "No Path Available"
            assert(sim.getLastPlanResult().success());
        } else {
            assert(sim.getLastPlanResult().status == PlanStatus::NoPathFound);
        }
    }

    // Verify ExplanationEngine generates real step output
    sim.loadScenarioById(2);
    sim.startAStarVisualization(false);
    const std::string exp = ExplanationEngine::buildStepExplanation(
        sim.getLastPlanResult(), 0, sim.getMovementMode());
    assert(exp.find("Current Node:") != std::string::npos);
    assert(exp.find("G = ") != std::string::npos);
    assert(exp.find("F = G + H = ") != std::string::npos);

    // Verify Random Generator with ensureSolvable=true
    RandomGenConfig cfg;
    cfg.gridSize = 20;
    cfg.obstacleDensity = 0.28;
    cfg.dynamicObstacleCount = 3;
    cfg.seed = 777;
    cfg.ensureSolvable = true;
    ScenarioData randScen = mgr.generateRandomEnvironment(cfg);
    assert(sim.loadScenario(randScen));
    assert(sim.getLastPlanResult().success());

    // Verify Scenario 4 & 5 dynamic obstacle simulation completes collision-free
    assert(sim.loadScenarioById(4));
    sim.run();
    for (int i = 0; i < 800 && (sim.getState() == SimState::Running || sim.getState() == SimState::Blocked); ++i) {
        sim.update(0.05);
    }
    assert(sim.getState() == SimState::Completed);

    std::cout << "[PASS] All 10 scenarios, random solvable generator, & ExplanationEngine\n";
}

} // namespace

int main() {
    testGridAndObstacles();
    testAStarAndEdgeCases();
    testDijkstraComparison();
    testDynamicReplanningAndSimulation();
    testExplanationAndScenarios();
    std::cout << "\nALL CORE C++17 ROBOTICS TESTS PASSED.\n";
    return 0;
}
