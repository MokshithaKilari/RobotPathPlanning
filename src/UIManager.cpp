#include "UIManager.hpp"
#include "ExplanationEngine.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace rpp {

namespace {

sf::Color toSF(ColorRGBA c, std::uint8_t overrideAlpha = 255) {
    return sf::Color(c.r, c.g, c.b, overrideAlpha == 255 ? c.a : overrideAlpha);
}

std::string fmtFloat(double v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

std::vector<std::string> wrapString(const std::string& input, std::size_t maxCols) {
    std::vector<std::string> lines;
    std::istringstream paraStream(input);
    std::string paragraph;
    while (std::getline(paraStream, paragraph)) {
        if (paragraph.empty()) {
            lines.emplace_back("");
            continue;
        }
        std::istringstream wordStream(paragraph);
        std::string word;
        std::string current;
        // Preserve leading indentation
        std::size_t indent = 0;
        while (indent < paragraph.size() && paragraph[indent] == ' ') ++indent;
        const std::string indentStr(indent, ' ');

        while (wordStream >> word) {
            if (current.empty()) {
                current = indentStr + word;
            } else if (current.size() + 1 + word.size() <= maxCols) {
                current += ' ' + word;
            } else {
                lines.push_back(current);
                current = indentStr + word;
            }
        }
        if (!current.empty()) {
            lines.push_back(current);
        }
    }
    return lines;
}

ColorRGBA severityColor(LogSeverity sev) {
    switch (sev) {
        case LogSeverity::Info:     return Theme::AccentCyan;
        case LogSeverity::Success:  return Theme::AccentGreen;
        case LogSeverity::Warning:  return Theme::AccentOrange;
        case LogSeverity::Critical: return Theme::AccentRed;
    }
    return Theme::TextSecondary;
}

ColorRGBA simStateColor(SimState s) {
    switch (s) {
        case SimState::Idle:       return Theme::AccentCyan;
        case SimState::Planning:   return Theme::AccentVioletHi;
        case SimState::Running:    return Theme::AccentGreen;
        case SimState::Paused:     return Theme::AccentOrange;
        case SimState::Replanning: return Theme::AccentYellow;
        case SimState::Completed:  return Theme::AccentGreen;
        case SimState::Blocked:    return Theme::AccentRed;
        case SimState::Error:      return Theme::AccentRed;
    }
    return Theme::TextSecondary;
}

} // namespace

UIManager::UIManager() = default;

bool UIManager::loadFonts(const std::string& assetBaseDir) {
    namespace fs = std::filesystem;
    const fs::path base(assetBaseDir);

    auto tryLoad = [](sf::Font& font, const std::vector<fs::path>& paths) {
        for (const auto& p : paths) {
            std::error_code ec;
            if (fs::exists(p, ec) && font.openFromFile(p.string())) {
                return true;
            }
        }
        return false;
    };

    const bool okSans = tryLoad(m_sansFont, {
        base / "assets/fonts/DejaVuSans.ttf",
        "assets/fonts/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf"
    });

    const bool okBold = tryLoad(m_sansBoldFont, {
        base / "assets/fonts/DejaVuSans-Bold.ttf",
        "assets/fonts/DejaVuSans-Bold.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf"
    });

    const bool okMono = tryLoad(m_monoFont, {
        base / "assets/fonts/JetBrainsMono-Regular.ttf",
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/lucon.ttf"
    });

    if (!okBold && okSans) m_sansBoldFont = m_sansFont;
    if (!okMono && okSans) m_monoFont = m_sansFont;
    m_fontsLoaded = okSans || okBold || okMono;
    return m_fontsLoaded;
}

void UIManager::addButton(const std::string& id,
                          const std::string& label,
                          const std::string& tooltip,
                          sf::FloatRect rect,
                          bool active,
                          ColorRGBA accent,
                          std::function<void()> cb,
                          bool enabled) {
    m_buttons.push_back(UIButton{id, label, tooltip, rect, active, enabled, accent, std::move(cb)});
}

void UIManager::triggerConfirm(ConfirmAction action, const std::string& prompt) {
    m_confirmAction = action;
    m_confirmMessage = prompt;
}

void UIManager::rebuildLayout(sf::Vector2u winSize, Simulation& sim) {
    m_windowSize = winSize;
    m_buttons.clear();
    m_modalButtons.clear();

    const float W = static_cast<float>(winSize.x);
    const float H = static_cast<float>(winSize.y);

    constexpr float kHeaderH = 68.f;
    constexpr float kPad = 10.f;
    const float leftW = 248.f;
    const float rightW = std::clamp(W * 0.25f, 340.f, 410.f);

    m_headerRect = {{0.f, 0.f}, {W, kHeaderH}};
    m_leftPanelRect = {{kPad, kHeaderH + kPad}, {leftW, H - kHeaderH - 2.f * kPad}};
    m_rightPanelRect = {{W - rightW - kPad, kHeaderH + kPad}, {rightW, H - kHeaderH - 2.f * kPad}};

    const float centerX = m_leftPanelRect.position.x + m_leftPanelRect.size.x + kPad;
    const float centerW = std::max(280.f, m_rightPanelRect.position.x - centerX - kPad);
    m_centerRect = {{centerX, kHeaderH + kPad}, {centerW, H - kHeaderH - 2.f * kPad}};

    const float gridTopPad = 48.f;
    const float gridBottomPad = 46.f;
    const float availW = m_centerRect.size.x - 42.f;
    const float availH = m_centerRect.size.y - gridTopPad - gridBottomPad;
    const float gridSide = std::max(180.f, std::min(availW, availH));
    const float gx = m_centerRect.position.x + (m_centerRect.size.x - gridSide) * 0.5f + 10.f;
    const float gy = m_centerRect.position.y + gridTopPad + (availH - gridSide) * 0.5f;
    m_gridRect = {{gx, gy}, {gridSide, gridSide}};

    // Header navigation & mode buttons
    const float navY = 16.f;
    const float navH = 34.f;
    float nx = 485.f;
    struct ScreenBtn { AppScreen scr; const char* lbl; const char* tip; float w; };
    const ScreenBtn navs[] = {
        {AppScreen::LabWorkspace,   "Lab View",     "Main Robotics Simulation Workspace", 84.f},
        {AppScreen::PathCalculator, "Calculator",   "Simple Path Calculator & Comparison", 92.f},
        {AppScreen::Analytics,      "Analytics",    "Real Recorded Simulation Charts", 86.f},
        {AppScreen::Learning,       "Learn A*",     "Interactive A* Educational Guide", 82.f},
        {AppScreen::Presentation,   "Presentation", "College Viva Demonstration Mode", 108.f}
    };
    for (const auto& nb : navs) {
        addButton("nav_" + std::string(nb.lbl), nb.lbl, nb.tip,
                  {{nx, navY}, {nb.w, navH}}, m_screen == nb.scr, Theme::AccentCyan,
                  [this, scr = nb.scr]() {
                      m_screen = scr;
                      if (scr == AppScreen::PathCalculator) m_rightTab = RightTab::CalculatorAndCompare;
                      if (scr == AppScreen::Learning) m_rightTab = RightTab::Learning;
                  });
        nx += nb.w + 6.f;
    }

    // 4-Dir / 8-Dir toggle in header
    const bool is8Dir = sim.getMovementMode() == MovementMode::EightDirectional;
    addButton("toggle_dir", is8Dir ? "8-Dir (Octile)" : "4-Dir (Manh.)",
              "Toggle 4-direction vs 8-direction movement",
              {{nx + 8.f, navY}, {116.f, navH}}, is8Dir, Theme::AccentVioletHi,
              [&sim]() {
                  sim.setMovementMode(sim.getMovementMode() == MovementMode::FourDirectional
                                          ? MovementMode::EightDirectional
                                          : MovementMode::FourDirectional);
              });

    // Left Control Panel buttons
    float lx = m_leftPanelRect.position.x + 12.f;
    float ly = m_leftPanelRect.position.y + 30.f;
    const float bwFull = m_leftPanelRect.size.x - 24.f;
    const float bwHalf = (bwFull - 6.f) * 0.5f;
    const float bh = 25.f;
    const float gap = 4.f;

    // ENVIRONMENT section
    addButton("env_new", "New Environment", "Reset grid to empty workspace (Ctrl+N)",
              {{lx, ly}, {bwFull, bh}}, false, Theme::AccentCyan,
              [this, &sim]() {
                  if (sim.getGrid().getStaticObstacleCount() > 0 || !sim.getDynamicObstacles().empty()) {
                      triggerConfirm(ConfirmAction::NewEnvironment, "Create a new empty environment and clear current obstacles?");
                  } else {
                      sim.newEnvironment();
                  }
              });
    ly += bh + gap;

    addButton("tool_start", "Set Start", "Click grid to place Start cell (S)",
              {{lx, ly}, {bwHalf, bh}}, m_editTool == EditTool::SetStart, Theme::AccentGreen,
              [this]() { m_editTool = EditTool::SetStart; });
    addButton("tool_goal", "Set Goal", "Click grid to place Goal cell (G)",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, m_editTool == EditTool::SetGoal, Theme::AccentRed,
              [this]() { m_editTool = EditTool::SetGoal; });
    ly += bh + gap;

    addButton("tool_static", "Add Static Obs", "Click/drag cells to add static obstacles (O)",
              {{lx, ly}, {bwHalf, bh}}, m_editTool == EditTool::AddStaticObstacle, Theme::AccentCyan,
              [this]() { m_editTool = EditTool::AddStaticObstacle; });
    addButton("tool_remove", "Remove Obs", "Click/drag or right-click to remove obstacle (X)",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, m_editTool == EditTool::RemoveObstacle, Theme::AccentOrange,
              [this]() { m_editTool = EditTool::RemoveObstacle; });
    ly += bh + gap;

    addButton("tool_dyn", "Add Dynamic Obs", "Click grid cell to spawn moving obstacle (D)",
              {{lx, ly}, {bwFull, bh}}, m_editTool == EditTool::AddDynamicObstacle, Theme::AccentOrange,
              [this]() { m_editTool = EditTool::AddDynamicObstacle; });
    ly += bh + gap;

    // Dynamic pattern selector row (H / V / Rnd / Route)
    const float pW = (bwFull - 9.f) / 4.f;
    struct PatBtn { DynamicPattern pat; const char* lbl; };
    const PatBtn pats[] = {
        {DynamicPattern::HorizontalPatrol, "Horiz"},
        {DynamicPattern::VerticalPatrol,   "Vert"},
        {DynamicPattern::RandomWalk,       "Rand"},
        {DynamicPattern::PatrolRoute,      "Route"}
    };
    for (int i = 0; i < 4; ++i) {
        addButton("pat_" + std::string(pats[i].lbl), pats[i].lbl,
                  "Pattern for newly placed dynamic obstacles",
                  {{lx + i * (pW + 3.f), ly}, {pW, 21.f}},
                  m_selectedDynPattern == pats[i].pat, Theme::AccentOrange,
                  [this, p = pats[i].pat]() { m_selectedDynPattern = p; });
    }
    ly += 25.f;

    addButton("env_clear_obs", "Clear Obstacles", "Remove all static & dynamic obstacles",
              {{lx, ly}, {bwFull, bh}}, false, Theme::AccentRed,
              [this, &sim]() {
                  if (sim.getState() == SimState::Running) {
                      triggerConfirm(ConfirmAction::ClearObstacles, "Simulation is running. Clear all obstacles?");
                  } else {
                      sim.clearObstacles();
                  }
              });
    ly += bh + gap;

    // Grid Size selector (10, 15, 20, 30)
    const int curSize = sim.getGrid().getSize();
    const int sizes[4] = {10, 15, 20, 30};
    for (int i = 0; i < 4; ++i) {
        addButton("size_" + std::to_string(sizes[i]),
                  std::to_string(sizes[i]) + "x" + std::to_string(sizes[i]),
                  "Resize simulation grid",
                  {{lx + i * (pW + 3.f), ly}, {pW, 21.f}},
                  curSize == sizes[i], Theme::AccentCyan,
                  [&sim, sz = sizes[i]]() { sim.resizeGrid(sz); });
    }
    ly += 39.f; // Space for PATH PLANNING header

    // PATH PLANNING section
    addButton("plan_calc", "Calculate A* Path", "Compute shortest path using A* (Space/C)",
              {{lx, ly}, {bwFull, bh}}, false, Theme::AccentVioletHi,
              [&sim]() { sim.calculatePath(false); });
    ly += bh + gap;

    addButton("plan_viz", "Visualize A*", "Animate A* node exploration step-by-step",
              {{lx, ly}, {bwHalf, bh}}, sim.isVizActive(), Theme::AccentCyan,
              [&sim]() { sim.startAStarVisualization(true); });
    addButton("plan_explain", "Explain A*", "Inspect A* G/H/F math step-by-step",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, m_rightTab == RightTab::ExplainAStar, Theme::AccentYellow,
              [this, &sim]() {
                  m_rightTab = RightTab::ExplainAStar;
                  sim.startAStarVisualization(false);
              });
    ly += bh + gap;

    addButton("plan_recalc", "Recalculate", "Re-run A* from current robot cell",
              {{lx, ly}, {bwHalf, bh}}, false, Theme::AccentViolet,
              [&sim]() { sim.recalculatePath(); });
    addButton("plan_clear", "Clear Path", "Clear planned path & search nodes",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, false, Theme::TextSecondary,
              [&sim]() { sim.clearPath(); });
    ly += bh + gap;

    addButton("plan_cmp", "Compare A* vs Dijkstra", "Benchmark A* against Dijkstra on current grid",
              {{lx, ly}, {bwFull, bh}}, false, Theme::AccentTeal,
              [this, &sim]() {
                  sim.runDijkstraComparison();
                  m_rightTab = RightTab::CalculatorAndCompare;
              });
    ly += 42.f; // Space for SIMULATION header

    // SIMULATION section
    const SimState st = sim.getState();
    addButton("sim_run", "Run", "Start autonomous robot navigation (R)",
              {{lx, ly}, {bwHalf, bh}}, st == SimState::Running, Theme::AccentGreen,
              [&sim]() { sim.run(); });
    addButton("sim_pause", "Pause", "Pause simulation (P)",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, st == SimState::Paused, Theme::AccentOrange,
              [&sim]() { sim.pause(); });
    ly += bh + gap;

    addButton("sim_resume", "Resume", "Resume paused simulation",
              {{lx, ly}, {bwHalf, bh}}, false, Theme::AccentGreen,
              [&sim]() { sim.resume(); });
    addButton("sim_stop", "Stop", "Stop and return robot to Start",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, false, Theme::AccentRed,
              [&sim]() { sim.stop(); });
    ly += bh + gap;

    addButton("sim_restart", "Restart", "Restart scenario from beginning",
              {{lx, ly}, {bwHalf, bh}}, false, Theme::AccentCyan,
              [&sim]() { sim.restartSimulation(); });
    addButton("sim_reset", "Reset", "Clear path & reset simulation state",
              {{lx + bwHalf + 6.f, ly}, {bwHalf, bh}}, false, Theme::TextSecondary,
              [this, &sim]() {
                  if (sim.getState() == SimState::Running) {
                      triggerConfirm(ConfirmAction::ResetAll, "Reset simulation while running?");
                  } else {
                      sim.resetAll();
                  }
              });
    ly += bh + gap;

    // Speed buttons (0.25x, 0.5x, 1x, 2x, 4x)
    const double speeds[5] = {0.25, 0.5, 1.0, 2.0, 4.0};
    const char* speedLbls[5] = {"0.25x", "0.5x", "1x", "2x", "4x"};
    const float spW = (bwFull - 12.f) / 5.f;
    for (int i = 0; i < 5; ++i) {
        const bool activeSp = std::abs(sim.getSimSpeedMultiplier() - speeds[i]) < 0.05;
        addButton("spd_" + std::string(speedLbls[i]), speedLbls[i],
                  "Simulation playback speed",
                  {{lx + i * (spW + 3.f), ly}, {spW, 21.f}},
                  activeSp, Theme::AccentGreen,
                  [&sim, sp = speeds[i]]() { sim.setSimSpeedMultiplier(sp); });
    }
    ly += 38.f; // Space for SCENARIOS header

    addButton("scen_mgr", "Scenario Manager", "Browse, preview, and load 10 scenarios",
              {{lx, ly}, {bwFull, bh}}, m_showScenarioModal, Theme::AccentCyan,
              [this, &sim]() {
                  m_showScenarioModal = !m_showScenarioModal;
                  m_showRandomModal = false;
                  if (m_showScenarioModal && !sim.getScenarioManager().getScenarios().empty()) {
                      m_selectedScenarioIndex = std::clamp(m_selectedScenarioIndex, 0,
                                                           static_cast<int>(sim.getScenarioManager().getScenarios().size()) - 1);
                      m_previewScenario = sim.getScenarioManager().getScenarios()[static_cast<std::size_t>(m_selectedScenarioIndex)];
                  }
              });
    ly += bh + gap;

    addButton("scen_rand", "Random Generator", "Generate solvable random obstacle environments",
              {{lx, ly}, {bwFull, bh}}, m_showRandomModal, Theme::AccentVioletHi,
              [this, &sim]() {
                  m_showRandomModal = !m_showRandomModal;
                  m_showScenarioModal = false;
                  if (m_showRandomModal) {
                      m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
                  }
              });

    // Right panel tabs
    const float rx = m_rightPanelRect.position.x + 10.f;
    const float ry = m_rightPanelRect.position.y + 10.f;
    const float rw = m_rightPanelRect.size.x - 20.f;
    const float tabW = (rw - 9.f) / 4.f;
    struct RTab { RightTab t; const char* lbl; };
    const RTab tabs[4] = {
        {RightTab::MetricsAndLog,        "Metrics"},
        {RightTab::ExplainAStar,         "Explain A*"},
        {RightTab::CalculatorAndCompare, "Compare"},
        {RightTab::Learning,             "Learn"}
    };
    for (int i = 0; i < 4; ++i) {
        addButton("rtab_" + std::to_string(i), tabs[i].lbl, "Switch inspector panel tab",
                  {{rx + i * (tabW + 3.f), ry}, {tabW, 26.f}},
                  m_rightTab == tabs[i].t, Theme::AccentCyan,
                  [this, t = tabs[i].t]() { m_rightTab = t; });
    }

    // Tab-specific interactive controls inside Right Panel
    if (m_rightTab == RightTab::ExplainAStar) {
        const float exY = ry + 36.f;
        const float btnW = (rw - 12.f) / 5.f;
        addButton("viz_prev", "Prev", "Previous A* step", {{rx, exY}, {btnW, 25.f}}, false, Theme::AccentCyan,
                  [&sim]() { sim.vizStepBackward(); });
        addButton("viz_next", "Next", "Next A* step", {{rx + (btnW + 3.f), exY}, {btnW, 25.f}}, false, Theme::AccentCyan,
                  [&sim]() { sim.vizStepForward(); });
        addButton("viz_play", "Play", "Auto-play A* search steps", {{rx + 2 * (btnW + 3.f), exY}, {btnW, 25.f}}, sim.isVizPlaying(), Theme::AccentGreen,
                  [&sim]() { sim.vizPlay(); });
        addButton("viz_pause", "Pause", "Pause A* playback", {{rx + 3 * (btnW + 3.f), exY}, {btnW, 25.f}}, false, Theme::AccentOrange,
                  [&sim]() { sim.vizPause(); });
        addButton("viz_rst", "Restart", "Restart A* step inspector", {{rx + 4 * (btnW + 3.f), exY}, {btnW, 25.f}}, false, Theme::AccentViolet,
                  [&sim]() { sim.vizRestart(); });

        const float vsY = exY + 30.f;
        const float vsW = (rw - 9.f) / 4.f;
        const double vSpds[4] = {3.0, 10.0, 22.0, 45.0};
        const char* vLbls[4] = {"3/s", "10/s", "22/s", "45/s"};
        for (int i = 0; i < 4; ++i) {
            addButton("vspd_" + std::to_string(i), vLbls[i], "Visualization steps per second",
                      {{rx + i * (vsW + 3.f), vsY}, {vsW, 21.f}},
                      std::abs(sim.getVizSpeedStepsPerSec() - vSpds[i]) < 1.0, Theme::AccentCyan,
                      [&sim, s = vSpds[i]]() { sim.setVizSpeedStepsPerSec(s); });
        }
    } else if (m_rightTab == RightTab::CalculatorAndCompare) {
        const float cy = ry + 42.f;
        const float cw = (rw - 6.f) / 3.f;
        addButton("calc_s", "1. Set Start", "Select Start placement tool", {{rx, cy}, {cw, 26.f}},
                  m_editTool == EditTool::SetStart, Theme::AccentGreen,
                  [this]() { m_editTool = EditTool::SetStart; });
        addButton("calc_g", "2. Set Goal", "Select Goal placement tool", {{rx + cw + 3.f, cy}, {cw, 26.f}},
                  m_editTool == EditTool::SetGoal, Theme::AccentRed,
                  [this]() { m_editTool = EditTool::SetGoal; });
        addButton("calc_go", "3. Calculate", "Calculate shortest A* path", {{rx + 2 * (cw + 3.f), cy}, {cw, 26.f}},
                  false, Theme::AccentVioletHi,
                  [&sim]() { sim.calculatePath(false); });

        addButton("calc_cmp", "Run A* vs Dijkstra Benchmark", "Execute both planners on current map",
                  {{rx, cy + 178.f}, {rw, 27.f}}, false, Theme::AccentTeal,
                  [&sim]() { sim.runDijkstraComparison(); });
    } else if (m_rightTab == RightTab::Learning) {
        const auto topics = ExplanationEngine::getEducationalTopics();
        float ty = ry + 38.f;
        for (std::size_t i = 0; i < topics.size(); ++i) {
            addButton("topic_" + std::to_string(i), topics[i].title, "View explanation",
                      {{rx, ty}, {rw, 24.f}}, m_selectedTopicIdx == i, Theme::AccentCyan,
                      [this, i]() { m_selectedTopicIdx = i; });
            ty += 28.f;
        }
    }

    // Presentation mode HUD quick-demo buttons
    if (m_screen == AppScreen::Presentation && !m_showScenarioModal && !m_showRandomModal) {
        const float px = m_centerRect.position.x + 14.f;
        const float py = m_centerRect.position.y + m_centerRect.size.y - 44.f;
        addButton("pres_1", "1. Initial Route", "Calculate clean baseline A* route", {{px, py}, {122.f, 28.f}}, false, Theme::AccentCyan,
                  [&sim]() { sim.calculatePath(false); });
        addButton("pres_2", "2. Block Path", "Insert obstacle directly onto planned path", {{px + 128.f, py}, {116.f, 28.f}}, false, Theme::AccentOrange,
                  [&sim]() { sim.injectObstacleOnPlannedPath(); });
        addButton("pres_3", "3. Run / Resume", "Execute autonomous navigation", {{px + 250.f, py}, {122.f, 28.f}}, false, Theme::AccentGreen,
                  [&sim]() {
                      if (sim.getState() == SimState::Paused) sim.resume();
                      else sim.run();
                  });
        addButton("pres_4", "4. Reset", "Reset viva demonstration", {{px + 378.f, py}, {84.f, 28.f}}, false, Theme::TextSecondary,
                  [&sim]() { sim.restartSimulation(); });
    }

    // Modal buttons when Scenario Manager is open
    if (m_showScenarioModal) {
        const float mw = std::min(920.f, W - 80.f);
        const float mh = std::min(620.f, H - 80.f);
        const float mx = (W - mw) * 0.5f;
        const float my = (H - mh) * 0.5f;

        const auto& scens = sim.getScenarioManager().getScenarios();
        float sy = my + 56.f;
        for (int i = 0; i < static_cast<int>(scens.size()); ++i) {
            m_modalButtons.push_back(UIButton{
                "sc_item_" + std::to_string(i),
                std::to_string(scens[static_cast<std::size_t>(i)].id) + ". " + scens[static_cast<std::size_t>(i)].name,
                scens[static_cast<std::size_t>(i)].description,
                {{mx + 18.f, sy}, {300.f, 40.f}},
                m_selectedScenarioIndex == i,
                true,
                Theme::AccentCyan,
                [this, &sim, i]() {
                    m_selectedScenarioIndex = i;
                    m_previewScenario = sim.getScenarioManager().getScenarios()[static_cast<std::size_t>(i)];
                }
            });
            sy += 46.f;
        }

        const float actY = my + mh - 50.f;
        const float actX = mx + 340.f;
        m_modalButtons.push_back(UIButton{"sc_prev", "Preview", "Update preview", {{actX, actY}, {100.f, 32.f}}, false, true, Theme::AccentCyan,
            [this, &sim]() {
                const auto& sc = sim.getScenarioManager().getScenarios();
                if (!sc.empty()) m_previewScenario = sc[static_cast<std::size_t>(m_selectedScenarioIndex)];
            }});
        m_modalButtons.push_back(UIButton{"sc_load", "Load", "Load selected scenario into workspace", {{actX + 110.f, actY}, {100.f, 32.f}}, false, true, Theme::AccentVioletHi,
            [this, &sim]() {
                const auto& sc = sim.getScenarioManager().getScenarios();
                if (!sc.empty()) {
                    sim.loadScenario(sc[static_cast<std::size_t>(m_selectedScenarioIndex)]);
                    m_showScenarioModal = false;
                }
            }});
        m_modalButtons.push_back(UIButton{"sc_run", "Run", "Load and immediately start simulation", {{actX + 220.f, actY}, {100.f, 32.f}}, false, true, Theme::AccentGreen,
            [this, &sim]() {
                const auto& sc = sim.getScenarioManager().getScenarios();
                if (!sc.empty()) {
                    sim.loadScenario(sc[static_cast<std::size_t>(m_selectedScenarioIndex)]);
                    sim.run();
                    m_showScenarioModal = false;
                }
            }});
        m_modalButtons.push_back(UIButton{"sc_rst", "Reset", "Reset current workspace", {{actX + 330.f, actY}, {95.f, 32.f}}, false, true, Theme::AccentOrange,
            [&sim]() { sim.restartSimulation(); }});
        m_modalButtons.push_back(UIButton{"sc_cls", "Close", "Close Scenario Manager", {{mx + mw - 108.f, actY}, {90.f, 32.f}}, false, true, Theme::AccentRed,
            [this]() { m_showScenarioModal = false; }});
    }

    // Modal buttons when Random Environment Generator is open
    if (m_showRandomModal) {
        const float mw = 740.f;
        const float mh = 480.f;
        const float mx = (W - mw) * 0.5f;
        const float my = (H - mh) * 0.5f;
        float ryM = my + 68.f;
        const float cx = mx + 24.f;

        // Grid size buttons
        const int gSizes[4] = {10, 15, 20, 30};
        for (int i = 0; i < 4; ++i) {
            m_modalButtons.push_back(UIButton{"rg_sz_" + std::to_string(i),
                std::to_string(gSizes[i]) + "x" + std::to_string(gSizes[i]), "",
                {{cx + i * 74.f, ryM + 22.f}, {68.f, 26.f}},
                m_randomConfig.gridSize == gSizes[i], true, Theme::AccentCyan,
                [this, &sim, sz = gSizes[i]]() {
                    m_randomConfig.gridSize = sz;
                    m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
                }});
        }
        ryM += 66.f;

        // Density - / +
        m_modalButtons.push_back(UIButton{"rg_den_m", "-", "", {{cx, ryM + 22.f}, {36.f, 26.f}}, false, true, Theme::AccentCyan,
            [this, &sim]() {
                m_randomConfig.obstacleDensity = std::max(0.05, m_randomConfig.obstacleDensity - 0.05);
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        m_modalButtons.push_back(UIButton{"rg_den_p", "+", "", {{cx + 128.f, ryM + 22.f}, {36.f, 26.f}}, false, true, Theme::AccentCyan,
            [this, &sim]() {
                m_randomConfig.obstacleDensity = std::min(0.45, m_randomConfig.obstacleDensity + 0.05);
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        ryM += 66.f;

        // Dynamic obstacle count - / +
        m_modalButtons.push_back(UIButton{"rg_dyn_m", "-", "", {{cx, ryM + 22.f}, {36.f, 26.f}}, false, true, Theme::AccentOrange,
            [this, &sim]() {
                m_randomConfig.dynamicObstacleCount = std::max(0, m_randomConfig.dynamicObstacleCount - 1);
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        m_modalButtons.push_back(UIButton{"rg_dyn_p", "+", "", {{cx + 128.f, ryM + 22.f}, {36.f, 26.f}}, false, true, Theme::AccentOrange,
            [this, &sim]() {
                m_randomConfig.dynamicObstacleCount = std::min(10, m_randomConfig.dynamicObstacleCount + 1);
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        ryM += 66.f;

        // Pattern buttons
        for (int i = 0; i < 4; ++i) {
            m_modalButtons.push_back(UIButton{"rg_pat_" + std::to_string(i), pats[i].lbl, "",
                {{cx + i * 74.f, ryM + 22.f}, {68.f, 26.f}},
                m_randomConfig.dynamicPattern == pats[i].pat, true, Theme::AccentOrange,
                [this, &sim, p = pats[i].pat]() {
                    m_randomConfig.dynamicPattern = p;
                    m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
                }});
        }
        ryM += 66.f;

        // Seed - / + and Solvable toggle
        m_modalButtons.push_back(UIButton{"rg_sd_m", "-1", "", {{cx, ryM + 22.f}, {44.f, 26.f}}, false, true, Theme::AccentCyan,
            [this, &sim]() {
                m_randomConfig.seed = (m_randomConfig.seed > 1) ? m_randomConfig.seed - 1 : 1;
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        m_modalButtons.push_back(UIButton{"rg_sd_p", "+1", "", {{cx + 128.f, ryM + 22.f}, {44.f, 26.f}}, false, true, Theme::AccentCyan,
            [this, &sim]() {
                ++m_randomConfig.seed;
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        m_modalButtons.push_back(UIButton{"rg_solv", m_randomConfig.ensureSolvable ? "Solvable: ON" : "Solvable: OFF", "",
            {{cx + 184.f, ryM + 22.f}, {116.f, 26.f}}, m_randomConfig.ensureSolvable, true, Theme::AccentGreen,
            [this, &sim]() {
                m_randomConfig.ensureSolvable = !m_randomConfig.ensureSolvable;
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});

        const float actY = my + mh - 50.f;
        m_modalButtons.push_back(UIButton{"rg_gen", "Generate", "Generate new random seed layout", {{cx, actY}, {110.f, 32.f}}, false, true, Theme::AccentCyan,
            [this, &sim]() {
                m_randomConfig.seed += 7u;
                m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
            }});
        m_modalButtons.push_back(UIButton{"rg_clr", "Clear", "Clear preview obstacles", {{cx + 120.f, actY}, {95.f, 32.f}}, false, true, Theme::AccentOrange,
            [this]() {
                if (m_generatedPreview) {
                    m_generatedPreview->staticObstacles.clear();
                    m_generatedPreview->dynamicObstacles.clear();
                }
            }});
        m_modalButtons.push_back(UIButton{"rg_app", "Apply", "Apply generated environment to simulation", {{cx + 225.f, actY}, {110.f, 32.f}}, false, true, Theme::AccentGreen,
            [this, &sim]() {
                if (!m_generatedPreview) {
                    m_generatedPreview = sim.getScenarioManager().generateRandomEnvironment(m_randomConfig);
                }
                sim.loadScenario(*m_generatedPreview);
                m_showRandomModal = false;
            }});
        m_modalButtons.push_back(UIButton{"rg_cls", "Close", "Close generator", {{mx + mw - 110.f, actY}, {88.f, 32.f}}, false, true, Theme::AccentRed,
            [this]() { m_showRandomModal = false; }});
    }

    // Confirm modal buttons
    if (m_confirmAction != ConfirmAction::None) {
        const float cw = 440.f;
        const float ch = 175.f;
        const float cx = (W - cw) * 0.5f;
        const float cy = (H - ch) * 0.5f;
        m_modalButtons.push_back(UIButton{"cfm_yes", "Confirm", "", {{cx + cw - 206.f, cy + ch - 46.f}, {90.f, 30.f}}, false, true, Theme::AccentGreen,
            [this, &sim]() {
                const ConfirmAction act = m_confirmAction;
                m_confirmAction = ConfirmAction::None;
                if (act == ConfirmAction::NewEnvironment) sim.newEnvironment();
                else if (act == ConfirmAction::ClearObstacles) sim.clearObstacles();
                else if (act == ConfirmAction::ResetAll) sim.resetAll();
            }});
        m_modalButtons.push_back(UIButton{"cfm_no", "Cancel", "", {{cx + cw - 104.f, cy + ch - 46.f}, {86.f, 30.f}}, false, true, Theme::AccentRed,
            [this]() { m_confirmAction = ConfirmAction::None; }});
    }
}

std::optional<GridCoord> UIManager::screenToGrid(sf::Vector2f mousePos, int gridSize) const {
    if (!m_gridRect.contains(mousePos) || gridSize <= 0) return std::nullopt;
    const float cellSz = m_gridRect.size.x / static_cast<float>(gridSize);
    const int gx = static_cast<int>((mousePos.x - m_gridRect.position.x) / cellSz);
    const int gy = static_cast<int>((mousePos.y - m_gridRect.position.y) / cellSz);
    if (gx >= 0 && gx < gridSize && gy >= 0 && gy < gridSize) {
        return GridCoord{gx, gy};
    }
    return std::nullopt;
}

void UIManager::handleGridClick(sf::Vector2f mousePos, sf::Mouse::Button button, Simulation& sim) {
    const auto cellOpt = screenToGrid(mousePos, sim.getGrid().getSize());
    if (!cellOpt) return;
    const GridCoord cell = *cellOpt;

    if (button == sf::Mouse::Button::Right) {
        sim.removeObstacleAt(cell);
        return;
    }
    if (button != sf::Mouse::Button::Left) return;

    switch (m_editTool) {
        case EditTool::SetStart:
            sim.setStart(cell);
            break;
        case EditTool::SetGoal:
            sim.setGoal(cell);
            break;
        case EditTool::AddStaticObstacle:
            sim.addStaticObstacle(cell);
            break;
        case EditTool::RemoveObstacle:
            sim.removeObstacleAt(cell);
            break;
        case EditTool::AddDynamicObstacle:
            sim.addDynamicObstacle(cell, m_selectedDynPattern, 1.6);
            break;
        case EditTool::None:
            break;
    }
}

void UIManager::handleEvent(const sf::Event& event, sf::RenderWindow& window, Simulation& sim) {
    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        m_mousePos = {static_cast<float>(moved->position.x), static_cast<float>(moved->position.y)};
        m_hoveredCell = screenToGrid(m_mousePos, sim.getGrid().getSize());

        if (m_leftMouseDragging && !m_showScenarioModal && !m_showRandomModal && m_confirmAction == ConfirmAction::None) {
            if (m_editTool == EditTool::AddStaticObstacle || m_editTool == EditTool::RemoveObstacle) {
                if (m_hoveredCell && *m_hoveredCell != m_lastDraggedCell) {
                    m_lastDraggedCell = *m_hoveredCell;
                    handleGridClick(m_mousePos, sf::Mouse::Button::Left, sim);
                }
            }
        }
        return;
    }

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        const sf::Vector2f mp{static_cast<float>(pressed->position.x), static_cast<float>(pressed->position.y)};
        m_mousePos = mp;

        if (pressed->button == sf::Mouse::Button::Left) {
            // Priority 1: Active modal buttons
            if (!m_modalButtons.empty()) {
                for (const auto& btn : m_modalButtons) {
                    if (btn.enabled && btn.bounds.contains(mp) && btn.onClick) {
                        btn.onClick();
                        return;
                    }
                }
                return;
            }

            // Priority 2: Panel buttons
            for (const auto& btn : m_buttons) {
                if (btn.enabled && btn.bounds.contains(mp) && btn.onClick) {
                    btn.onClick();
                    return;
                }
            }

            // Priority 3: Grid interaction
            if (m_screen == AppScreen::LabWorkspace || m_screen == AppScreen::PathCalculator || m_screen == AppScreen::Presentation) {
                m_leftMouseDragging = true;
                m_lastDraggedCell = screenToGrid(mp, sim.getGrid().getSize()).value_or(GridCoord{-1, -1});
                handleGridClick(mp, sf::Mouse::Button::Left, sim);
            }
        } else if (pressed->button == sf::Mouse::Button::Right) {
            if (!m_showScenarioModal && !m_showRandomModal && m_confirmAction == ConfirmAction::None) {
                handleGridClick(mp, sf::Mouse::Button::Right, sim);
            }
        }
        return;
    }

    if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (released->button == sf::Mouse::Button::Left) {
            m_leftMouseDragging = false;
            m_lastDraggedCell = {-1, -1};
        }
        return;
    }

    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (m_rightPanelRect.contains(m_mousePos) && m_rightTab == RightTab::MetricsAndLog) {
            m_logScrollOffset = std::max(0, m_logScrollOffset + (wheel->delta > 0 ? 1 : -1));
        }
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Escape) {
            m_showScenarioModal = false;
            m_showRandomModal = false;
            m_confirmAction = ConfirmAction::None;
        } else if (key->code == sf::Keyboard::Key::Space || key->code == sf::Keyboard::Key::C) {
            sim.calculatePath(false);
        } else if (key->code == sf::Keyboard::Key::R) {
            sim.run();
        } else if (key->code == sf::Keyboard::Key::P) {
            if (sim.getState() == SimState::Paused) sim.resume();
            else sim.pause();
        } else if (key->code == sf::Keyboard::Key::S) {
            m_editTool = EditTool::SetStart;
        } else if (key->code == sf::Keyboard::Key::G) {
            m_editTool = EditTool::SetGoal;
        } else if (key->code == sf::Keyboard::Key::O) {
            m_editTool = EditTool::AddStaticObstacle;
        } else if (key->code == sf::Keyboard::Key::D) {
            m_editTool = EditTool::AddDynamicObstacle;
        } else if (key->code == sf::Keyboard::Key::X) {
            m_editTool = EditTool::RemoveObstacle;
        } else if (key->code == sf::Keyboard::Key::Right && m_rightTab == RightTab::ExplainAStar) {
            sim.vizStepForward();
        } else if (key->code == sf::Keyboard::Key::Left && m_rightTab == RightTab::ExplainAStar) {
            sim.vizStepBackward();
        }
    }
}

void UIManager::update(double dt, const sf::RenderWindow& window, Simulation& sim) {
    m_pulseTime += dt;
    rebuildLayout(window.getSize(), sim);
}

void UIManager::drawPanelBox(sf::RenderWindow& window, sf::FloatRect rect, ColorRGBA fill, ColorRGBA border, float thickness) {
    sf::RectangleShape box(rect.size);
    box.setPosition(rect.position);
    box.setFillColor(toSF(fill));
    box.setOutlineColor(toSF(border));
    box.setOutlineThickness(thickness);
    window.draw(box);
}

void UIManager::drawText(sf::RenderWindow& window, const std::string& str, sf::Vector2f pos, unsigned int size, ColorRGBA color, bool mono, bool bold) {
    if (!m_fontsLoaded) return;
    const sf::Font& f = mono ? m_monoFont : (bold ? m_sansBoldFont : m_sansFont);
    sf::Text text(f, str, size);
    text.setFillColor(toSF(color));
    text.setPosition(pos);
    window.draw(text);
}

void UIManager::drawCenteredText(sf::RenderWindow& window, const std::string& str, sf::FloatRect rect, unsigned int size, ColorRGBA color, bool mono, bool bold) {
    if (!m_fontsLoaded) return;
    const sf::Font& f = mono ? m_monoFont : (bold ? m_sansBoldFont : m_sansFont);
    sf::Text text(f, str, size);
    text.setFillColor(toSF(color));
    const sf::FloatRect b = text.getLocalBounds();
    const float tx = rect.position.x + (rect.size.x - b.size.x) * 0.5f - b.position.x;
    const float ty = rect.position.y + (rect.size.y - b.size.y) * 0.5f - b.position.y;
    text.setPosition({tx, ty});
    window.draw(text);
}

void UIManager::drawButton(sf::RenderWindow& window, const UIButton& btn) {
    const bool hovered = btn.bounds.contains(m_mousePos);
    ColorRGBA bg = Theme::BgCard;
    ColorRGBA border = Theme::BorderSubtle;
    ColorRGBA txt = btn.enabled ? Theme::TextPrimary : Theme::TextMuted;

    if (btn.active) {
        bg = ColorRGBA{
            static_cast<std::uint8_t>((Theme::BgCard.r + btn.accent.r) / 3),
            static_cast<std::uint8_t>((Theme::BgCard.g + btn.accent.g) / 3),
            static_cast<std::uint8_t>((Theme::BgCard.b + btn.accent.b) / 3),
            255
        };
        border = btn.accent;
    } else if (hovered && btn.enabled) {
        bg = Theme::BgCardHover;
        border = Theme::BorderActive;
    }

    drawPanelBox(window, btn.bounds, bg, border, btn.active ? 1.5f : 1.f);
    drawCenteredText(window, btn.label, btn.bounds, 12, txt, false, btn.active);
}

void UIManager::renderHeader(sf::RenderWindow& window, const Simulation& sim, float fps) {
    drawPanelBox(window, m_headerRect, Theme::BgPanel, Theme::BorderSubtle, 1.f);

    // Subtle accent bar at top edge
    sf::RectangleShape topBar({m_headerRect.size.x, 3.f});
    topBar.setPosition({0.f, 0.f});
    topBar.setFillColor(toSF(Theme::AccentViolet));
    window.draw(topBar);

    drawText(window, "INTELLIGENT ROBOT PATH PLANNING", {16.f, 11.f}, 19, Theme::TextPrimary, false, true);
    drawText(window, "A* Navigation  |  Dynamic Obstacle Avoidance  |  Real-Time Replanning",
             {16.f, 38.f}, 12, Theme::AccentCyan, false, false);

    // Status pills on right side of header
    const float rightStart = m_headerRect.size.x - 360.f;
    const ColorRGBA stCol = simStateColor(sim.getState());
    drawPanelBox(window, {{rightStart, 14.f}, {152.f, 38.f}}, Theme::BgCard, stCol, 1.2f);
    drawCenteredText(window, toString(sim.getState()), {{rightStart, 14.f}, {152.f, 38.f}}, 11, stCol, true, true);

    drawPanelBox(window, {{rightStart + 160.f, 14.f}, {116.f, 38.f}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
    drawCenteredText(window, toString(m_editTool), {{rightStart + 160.f, 14.f}, {116.f, 38.f}}, 11, Theme::TextSecondary, false, false);

    drawPanelBox(window, {{rightStart + 284.f, 14.f}, {64.f, 38.f}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
    drawCenteredText(window, std::to_string(static_cast<int>(std::round(fps))) + " FPS",
                     {{rightStart + 284.f, 14.f}, {64.f, 38.f}}, 11, Theme::AccentGreen, true, false);
}

void UIManager::renderLeftPanel(sf::RenderWindow& window) {
    drawPanelBox(window, m_leftPanelRect, Theme::BgPanel, Theme::BorderSubtle, 1.f);

    const float lx = m_leftPanelRect.position.x + 12.f;
    const float top = m_leftPanelRect.position.y;
    drawText(window, "ENVIRONMENT",   {lx, top + 10.f},  11, Theme::AccentCyan,   true, true);
    drawText(window, "PATH PLANNING", {lx, top + 219.f}, 11, Theme::AccentVioletHi, true, true);
    drawText(window, "SIMULATION",    {lx, top + 349.f}, 11, Theme::AccentGreen,  true, true);
    drawText(window, "SCENARIOS",     {lx, top + 477.f}, 11, Theme::AccentOrange, true, true);
}

void UIManager::renderGridViewport(sf::RenderWindow& window, const Simulation& sim) {
    drawPanelBox(window, m_centerRect, Theme::BgPanel, Theme::BorderSubtle, 1.f);

    // Top viewport bar: Scenario title + live status message
    const std::string hdr = "WORKSPACE: " + sim.getActiveScenarioName() +
                            " (" + std::to_string(sim.getGrid().getSize()) + "x" +
                            std::to_string(sim.getGrid().getSize()) + ")";
    drawText(window, hdr, {m_centerRect.position.x + 14.f, m_centerRect.position.y + 10.f},
             13, Theme::TextPrimary, false, true);

    const ColorRGBA msgColor = !sim.getLastError().empty() ? Theme::AccentRed : Theme::AccentCyan;
    drawText(window, sim.getStatusMessage(),
             {m_centerRect.position.x + 14.f, m_centerRect.position.y + 28.f},
             11, msgColor, false, false);

    // Grid outer border
    drawPanelBox(window, m_gridRect, Theme::GridEmpty, Theme::BorderActive, 1.5f);

    const Grid& grid = sim.getGrid();
    const int N = grid.getSize();
    const float cellSz = m_gridRect.size.x / static_cast<float>(N);

    // Build cell state lookup from current A* step / final path
    const AStarStep* step = sim.getCurrentVizStep();
    const PathResult& plan = sim.getLastPlanResult();

    // Draw coordinate axes labels
    const int stepLabel = (N >= 25) ? 5 : (N >= 15 ? 2 : 1);
    for (int i = 0; i < N; i += stepLabel) {
        drawText(window, std::to_string(i),
                 {m_gridRect.position.x + i * cellSz + cellSz * 0.25f, m_gridRect.position.y - 16.f},
                 10, Theme::TextMuted, true, false);
        drawText(window, std::to_string(i),
                 {m_gridRect.position.x - 20.f, m_gridRect.position.y + i * cellSz + cellSz * 0.22f},
                 10, Theme::TextMuted, true, false);
    }

    // 1. Travelled Path cells
    for (const GridCoord& c : sim.getRobot().getTravelledTrail()) {
        sf::RectangleShape cell({cellSz - 2.f, cellSz - 2.f});
        cell.setPosition({m_gridRect.position.x + c.x * cellSz + 1.f, m_gridRect.position.y + c.y * cellSz + 1.f});
        cell.setFillColor(sf::Color(Theme::AccentTeal.r, Theme::AccentTeal.g, Theme::AccentTeal.b, 48));
        window.draw(cell);
    }

    // 2. Closed Set (muted purple) & Open Set (cyan)
    if (step) {
        for (const Node& n : step->closedSnapshot) {
            sf::RectangleShape cell({cellSz - 2.f, cellSz - 2.f});
            cell.setPosition({m_gridRect.position.x + n.coord.x * cellSz + 1.f, m_gridRect.position.y + n.coord.y * cellSz + 1.f});
            cell.setFillColor(toSF(Theme::AccentPurpleDim));
            window.draw(cell);
        }
        for (const Node& n : step->openSnapshot) {
            sf::RectangleShape cell({cellSz - 2.f, cellSz - 2.f});
            cell.setPosition({m_gridRect.position.x + n.coord.x * cellSz + 1.f, m_gridRect.position.y + n.coord.y * cellSz + 1.f});
            cell.setFillColor(sf::Color(Theme::AccentCyan.r, Theme::AccentCyan.g, Theme::AccentCyan.b, 92));
            cell.setOutlineColor(sf::Color(Theme::AccentCyan.r, Theme::AccentCyan.g, Theme::AccentCyan.b, 175));
            cell.setOutlineThickness(1.f);
            window.draw(cell);
        }
        // Current Node (Yellow)
        if (sim.isVizActive()) {
            sf::RectangleShape cur({cellSz - 2.f, cellSz - 2.f});
            cur.setPosition({m_gridRect.position.x + step->current.x * cellSz + 1.f, m_gridRect.position.y + step->current.y * cellSz + 1.f});
            cur.setFillColor(sf::Color(Theme::AccentYellow.r, Theme::AccentYellow.g, Theme::AccentYellow.b, 180));
            window.draw(cur);
        }
    }

    // 3. Planned Path cells & polyline (bright violet)
    const auto& activePath = sim.isVizActive() && step ? step->currentPath : plan.path;
    if (!activePath.empty()) {
        for (const GridCoord& c : activePath) {
            sf::RectangleShape cell({cellSz - 4.f, cellSz - 4.f});
            cell.setPosition({m_gridRect.position.x + c.x * cellSz + 2.f, m_gridRect.position.y + c.y * cellSz + 2.f});
            cell.setFillColor(sf::Color(Theme::AccentViolet.r, Theme::AccentViolet.g, Theme::AccentViolet.b, 95));
            window.draw(cell);
        }
        if (activePath.size() >= 2) {
            sf::VertexArray pathLine(sf::PrimitiveType::LineStrip, activePath.size());
            for (std::size_t i = 0; i < activePath.size(); ++i) {
                const float px = m_gridRect.position.x + (activePath[i].x + 0.5f) * cellSz;
                const float py = m_gridRect.position.y + (activePath[i].y + 0.5f) * cellSz;
                pathLine[i] = sf::Vertex{{px, py}, toSF(Theme::AccentVioletHi)};
            }
            window.draw(pathLine);
        }
    }

    // 4. Grid lines via single VertexArray
    sf::VertexArray lines(sf::PrimitiveType::Lines, static_cast<std::size_t>((N + 1) * 4));
    std::size_t vi = 0;
    const sf::Color lineCol = toSF(Theme::GridLine);
    for (int i = 0; i <= N; ++i) {
        const float x = m_gridRect.position.x + i * cellSz;
        const float y = m_gridRect.position.y + i * cellSz;
        lines[vi++] = sf::Vertex{{x, m_gridRect.position.y}, lineCol};
        lines[vi++] = sf::Vertex{{x, m_gridRect.position.y + m_gridRect.size.y}, lineCol};
        lines[vi++] = sf::Vertex{{m_gridRect.position.x, y}, lineCol};
        lines[vi++] = sf::Vertex{{m_gridRect.position.x + m_gridRect.size.x, y}, lineCol};
    }
    window.draw(lines);

    // 5. Static Obstacles (dark machined blocks)
    for (const auto& obs : grid.getStaticObstacles()) {
        const GridCoord c = obs.getPosition();
        sf::RectangleShape blk({cellSz - 2.f, cellSz - 2.f});
        blk.setPosition({m_gridRect.position.x + c.x * cellSz + 1.f, m_gridRect.position.y + c.y * cellSz + 1.f});
        blk.setFillColor(toSF(Theme::StaticBlock));
        blk.setOutlineColor(toSF(Theme::StaticBlockEdge));
        blk.setOutlineThickness(1.f);
        window.draw(blk);
    }

    // 6. Start marker (blue/green)
    {
        const GridCoord s = grid.getStart();
        const float cx = m_gridRect.position.x + (s.x + 0.5f) * cellSz;
        const float cy = m_gridRect.position.y + (s.y + 0.5f) * cellSz;
        const float rad = cellSz * 0.36f;
        sf::CircleShape ring(rad, 24);
        ring.setOrigin({rad, rad});
        ring.setPosition({cx, cy});
        ring.setFillColor(sf::Color(Theme::AccentTeal.r, Theme::AccentTeal.g, Theme::AccentTeal.b, 80));
        ring.setOutlineColor(toSF(Theme::AccentGreen));
        ring.setOutlineThickness(2.f);
        window.draw(ring);
        if (cellSz >= 18.f) {
            drawCenteredText(window, "S", {{cx - rad, cy - rad}, {rad * 2.f, rad * 2.f}},
                             11, Theme::TextPrimary, true, true);
        }
    }

    // 7. Goal marker (red/cyan concentric target radar)
    {
        const GridCoord g = grid.getGoal();
        const float cx = m_gridRect.position.x + (g.x + 0.5f) * cellSz;
        const float cy = m_gridRect.position.y + (g.y + 0.5f) * cellSz;
        const float pulse = 0.88f + 0.12f * static_cast<float>(std::sin(m_pulseTime * 3.5));

        const float rOuter = cellSz * 0.40f * pulse;
        sf::CircleShape outer(rOuter, 28);
        outer.setOrigin({rOuter, rOuter});
        outer.setPosition({cx, cy});
        outer.setFillColor(sf::Color(Theme::AccentRed.r, Theme::AccentRed.g, Theme::AccentRed.b, 45));
        outer.setOutlineColor(toSF(Theme::AccentCyan));
        outer.setOutlineThickness(1.8f);
        window.draw(outer);

        const float rInner = cellSz * 0.18f;
        sf::CircleShape inner(rInner, 20);
        inner.setOrigin({rInner, rInner});
        inner.setPosition({cx, cy});
        inner.setFillColor(toSF(Theme::AccentRed));
        window.draw(inner);
    }

    // 8. Dynamic Obstacles (animated orange technical modules + predicted cell outline)
    for (const auto& dyn : sim.getDynamicObstacles()) {
        const GridCoord pred = dyn.getPredictedNextCell();
        if (pred != dyn.getCell()) {
            sf::RectangleShape predBox({cellSz - 4.f, cellSz - 4.f});
            predBox.setPosition({m_gridRect.position.x + pred.x * cellSz + 2.f, m_gridRect.position.y + pred.y * cellSz + 2.f});
            predBox.setFillColor(sf::Color(Theme::AccentOrange.r, Theme::AccentOrange.g, Theme::AccentOrange.b, 30));
            predBox.setOutlineColor(sf::Color(Theme::AccentOrange.r, Theme::AccentOrange.g, Theme::AccentOrange.b, 140));
            predBox.setOutlineThickness(1.f);
            window.draw(predBox);
        }

        const float dx = m_gridRect.position.x + static_cast<float>(dyn.getRenderX() + 0.5) * cellSz;
        const float dy = m_gridRect.position.y + static_cast<float>(dyn.getRenderY() + 0.5) * cellSz;
        const float side = cellSz * 0.66f;

        sf::RectangleShape body({side, side});
        body.setOrigin({side * 0.5f, side * 0.5f});
        body.setPosition({dx, dy});
        body.setRotation(sf::degrees(45.f));
        body.setFillColor(toSF(Theme::AccentOrange));
        body.setOutlineColor(toSF(Theme::AccentYellow));
        body.setOutlineThickness(1.5f);
        window.draw(body);
    }

    // 9. Robot Marker (circular chassis, status ring, sensor cone, directional heading)
    {
        const Robot& rob = sim.getRobot();
        const float rx = m_gridRect.position.x + static_cast<float>(rob.getRenderX() + 0.5) * cellSz;
        const float ry = m_gridRect.position.y + static_cast<float>(rob.getRenderY() + 0.5) * cellSz;
        const float rad = cellSz * 0.36f;
        const float heading = static_cast<float>(rob.getHeadingRad());

        // Sensor indicator wedge
        sf::ConvexShape sensorCone(3);
        const float coneLen = cellSz * 0.95f;
        const float spread = 0.42f;
        sensorCone.setPoint(0, {rx, ry});
        sensorCone.setPoint(1, {rx + std::cos(heading - spread) * coneLen, ry + std::sin(heading - spread) * coneLen});
        sensorCone.setPoint(2, {rx + std::cos(heading + spread) * coneLen, ry + std::sin(heading + spread) * coneLen});
        sensorCone.setFillColor(sf::Color(Theme::AccentCyan.r, Theme::AccentCyan.g, Theme::AccentCyan.b, 48));
        window.draw(sensorCone);

        // Status ring
        const ColorRGBA ringCol = simStateColor(sim.getState());
        sf::CircleShape statusRing(rad + 3.f, 28);
        statusRing.setOrigin({rad + 3.f, rad + 3.f});
        statusRing.setPosition({rx, ry});
        statusRing.setFillColor(sf::Color::Transparent);
        statusRing.setOutlineColor(toSF(ringCol));
        statusRing.setOutlineThickness(2.f);
        window.draw(statusRing);

        // Chassis body
        sf::CircleShape chassis(rad, 28);
        chassis.setOrigin({rad, rad});
        chassis.setPosition({rx, ry});
        chassis.setFillColor(sf::Color(24, 38, 68, 255));
        chassis.setOutlineColor(toSF(Theme::TextPrimary));
        chassis.setOutlineThickness(1.8f);
        window.draw(chassis);

        // Directional heading pointer
        sf::ConvexShape arrow(3);
        const float tip = rad * 0.95f;
        arrow.setPoint(0, {rx + std::cos(heading) * tip, ry + std::sin(heading) * tip});
        arrow.setPoint(1, {rx + std::cos(heading + 2.4f) * (rad * 0.6f), ry + std::sin(heading + 2.4f) * (rad * 0.6f)});
        arrow.setPoint(2, {rx + std::cos(heading - 2.4f) * (rad * 0.6f), ry + std::sin(heading - 2.4f) * (rad * 0.6f)});
        arrow.setFillColor(toSF(Theme::AccentCyan));
        window.draw(arrow);
    }

    // 10. Hovered cell highlight
    if (m_hoveredCell) {
        sf::RectangleShape hov({cellSz, cellSz});
        hov.setPosition({m_gridRect.position.x + m_hoveredCell->x * cellSz,
                         m_gridRect.position.y + m_hoveredCell->y * cellSz});
        hov.setFillColor(sf::Color::Transparent);
        hov.setOutlineColor(toSF(Theme::TextPrimary, 180));
        hov.setOutlineThickness(1.5f);
        window.draw(hov);
    }

    // 11. Bottom Grid Telemetry Bar: Robot Position + Legend
    const float botY = m_centerRect.position.y + m_centerRect.size.y - 36.f;
    const GridCoord rPos = sim.getRobot().getCurrentCell();
    drawText(window,
             "Robot Position  X: " + std::to_string(rPos.x) + "   Y: " + std::to_string(rPos.y) +
             "   |   Dir: " + sim.getRobot().getDirectionLabel(),
             {m_centerRect.position.x + 14.f, botY + 8.f}, 12, Theme::TextPrimary, true, true);

    // Compact legend swatches on bottom-right
    if (m_screen != AppScreen::Presentation) {
        struct Leg { ColorRGBA col; const char* lbl; };
        const Leg legs[] = {
            {Theme::AccentCyan, "Open"},
            {Theme::AccentPurpleDim, "Closed"},
            {Theme::AccentVioletHi, "Path"},
            {Theme::AccentOrange, "Dynamic"},
            {Theme::StaticBlockEdge, "Static"}
        };
        float lx = m_centerRect.position.x + m_centerRect.size.x - 325.f;
        for (const auto& l : legs) {
            sf::RectangleShape sw({10.f, 10.f});
            sw.setPosition({lx, botY + 11.f});
            sw.setFillColor(toSF(l.col));
            window.draw(sw);
            drawText(window, l.lbl, {lx + 14.f, botY + 8.f}, 10, Theme::TextSecondary, false, false);
            lx += 63.f;
        }
    }

    // 12. Dynamic Replan Alert Toast Banner
    const ReplanNotice& rep = sim.getReplanNotice();
    if (rep.active) {
        const sf::FloatRect toast{{m_gridRect.position.x + 10.f, m_gridRect.position.y + 10.f}, {330.f, 70.f}};
        drawPanelBox(window, toast,
                     ColorRGBA{26, 20, 44, 238},
                     rep.replanSucceeded ? Theme::AccentYellow : Theme::AccentRed, 1.5f);
        drawText(window, "PATH INTERRUPTION DETECTED", {toast.position.x + 10.f, toast.position.y + 8.f},
                 11, rep.replanSucceeded ? Theme::AccentYellow : Theme::AccentRed, true, true);
        drawText(window, "Obstacle at " + rep.obstacleCoord.toString() + " | Replan #" + std::to_string(rep.replanCount),
                 {toast.position.x + 10.f, toast.position.y + 26.f}, 11, Theme::TextPrimary, true, false);
        drawText(window, rep.replanSucceeded
                     ? ("New A* route: " + std::to_string(rep.newPathLength) + " cells")
                     : "No alternative route available",
                 {toast.position.x + 10.f, toast.position.y + 44.f}, 11,
                 rep.replanSucceeded ? Theme::AccentGreen : Theme::AccentRed, false, false);
    }
}

void UIManager::renderRightPanel(sf::RenderWindow& window, Simulation& sim) {
    drawPanelBox(window, m_rightPanelRect, Theme::BgPanel, Theme::BorderSubtle, 1.f);

    const float rx = m_rightPanelRect.position.x + 12.f;
    const float rw = m_rightPanelRect.size.x - 24.f;
    float y = m_rightPanelRect.position.y + 46.f;

    if (m_rightTab == RightTab::MetricsAndLog) {
        const Statistics& st = sim.getStatistics();
        const Grid& g = sim.getGrid();

        auto drawMetricGroup = [&](const std::string& title, ColorRGBA accent,
                                   const std::vector<std::pair<std::string, std::string>>& rows) {
            const float boxH = 26.f + static_cast<float>(rows.size()) * 19.f;
            drawPanelBox(window, {{rx, y}, {rw, boxH}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
            drawText(window, title, {rx + 10.f, y + 6.f}, 11, accent, true, true);
            float ry = y + 24.f;
            for (const auto& r : rows) {
                drawText(window, r.first, {rx + 10.f, ry}, 11, Theme::TextSecondary, false, false);
                drawText(window, r.second, {rx + rw - 118.f, ry}, 11, Theme::TextPrimary, true, false);
                ry += 19.f;
            }
            y += boxH + 8.f;
        };

        drawMetricGroup("ENVIRONMENT & ROBOT", Theme::AccentCyan, {
            {"Grid Size", std::to_string(g.getSize()) + " x " + std::to_string(g.getSize())},
            {"Static / Dynamic Obs", std::to_string(g.getStaticObstacleCount()) + " / " + std::to_string(sim.getDynamicObstacles().size())},
            {"Robot Position", sim.getRobot().getCurrentCell().toString()},
            {"Distance Travelled", fmtFloat(st.getDistanceTravelled(), 1) + " cells"},
            {"Direction", sim.getRobot().getDirectionLabel()}
        });

        drawMetricGroup("A* & DYNAMIC AVOIDANCE", Theme::AccentVioletHi, {
            {"Path Length", std::to_string(st.getPathLength()) + " cells"},
            {"Nodes Explored", std::to_string(st.getNodesExplored())},
            {"Planning Time", fmtFloat(st.getPlanningTimeMs(), 3) + " ms"},
            {"Replans / Blocked", std::to_string(st.getReplanCount()) + " / " + std::to_string(st.getBlockedPathEvents())},
            {"Success / Failed Runs", std::to_string(st.getSuccessfulNavigations()) + " / " + std::to_string(st.getFailedNavigations())},
            {"Sim Time / Speed", fmtFloat(st.getElapsedTime(), 1) + "s @ " + fmtFloat(sim.getSimSpeedMultiplier(), 2) + "x"}
        });

        // Event Log
        const float logH = std::max(120.f, m_rightPanelRect.position.y + m_rightPanelRect.size.y - y - 10.f);
        drawPanelBox(window, {{rx, y}, {rw, logH}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
        drawText(window, "EVENT LOG", {rx + 10.f, y + 6.f}, 11, Theme::AccentGreen, true, true);

        const auto& entries = sim.getLogger().getEntries();
        const int maxVisible = std::max(1, static_cast<int>((logH - 30.f) / 17.f));
        const int total = static_cast<int>(entries.size());
        const int endIdx = std::max(0, total - std::min(m_logScrollOffset, std::max(0, total - maxVisible)));
        const int startIdx = std::max(0, endIdx - maxVisible);

        float ly = y + 26.f;
        for (int i = startIdx; i < endIdx; ++i) {
            const auto& e = entries[static_cast<std::size_t>(i)];
            std::string line = "[" + e.timestamp + "] " + e.message;
            if (line.size() > 48) line = line.substr(0, 45) + "...";
            drawText(window, line, {rx + 8.f, ly}, 10, severityColor(e.severity), true, false);
            ly += 17.f;
        }
    } else if (m_rightTab == RightTab::ExplainAStar) {
        y += 66.f; // space for Viz step buttons
        const float boxH = m_rightPanelRect.position.y + m_rightPanelRect.size.y - y - 12.f;
        drawPanelBox(window, {{rx, y}, {rw, boxH}}, Theme::BgCard, Theme::BorderSubtle, 1.f);

        const std::string expText = ExplanationEngine::buildStepExplanation(
            sim.getLastPlanResult(), sim.getVizStepIndex(), sim.getMovementMode());
        const auto lines = wrapString(expText, 44);
        float ty = y + 10.f;
        for (const auto& ln : lines) {
            if (ty > y + boxH - 18.f) break;
            ColorRGBA col = Theme::TextPrimary;
            if (ln.find("Current Node:") != std::string::npos || ln.find("Next Selection:") != std::string::npos) {
                col = Theme::AccentYellow;
            } else if (ln.find("GOAL REACHED") != std::string::npos) {
                col = Theme::AccentGreen;
            } else if (ln.find("BLOCKED") != std::string::npos) {
                col = Theme::AccentRed;
            } else if (ln.find("[OPEN]") != std::string::npos || ln.find("[UPDATED]") != std::string::npos) {
                col = Theme::AccentCyan;
            }
            drawText(window, ln, {rx + 10.f, ty}, 11, col, true, false);
            ty += 16.f;
        }

        if (sim.getReplanNotice().active && ty + 90.f < y + boxH) {
            ty += 10.f;
            const auto repLines = wrapString(ExplanationEngine::buildReplanExplanation(sim.getReplanNotice()), 44);
            for (const auto& ln : repLines) {
                if (ty > y + boxH - 18.f) break;
                drawText(window, ln, {rx + 10.f, ty}, 11, Theme::AccentOrange, true, false);
                ty += 16.f;
            }
        }
    } else if (m_rightTab == RightTab::CalculatorAndCompare) {
        drawText(window, "SIMPLE PATH CALCULATOR", {rx, y - 6.f}, 11, Theme::AccentCyan, true, true);
        const PathResult& res = sim.getLastPlanResult();
        const float cardY = y + 40.f;
        drawPanelBox(window, {{rx, cardY}, {rw, 130.f}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
        drawText(window, "Algorithm:     A* (" + toString(sim.getMovementMode()) + ")", {rx + 10.f, cardY + 12.f}, 11, Theme::TextPrimary, true, false);
        drawText(window, "Path Length:   " + std::to_string(res.path.size()) + " cells", {rx + 10.f, cardY + 34.f}, 11, Theme::AccentVioletHi, true, false);
        drawText(window, "Nodes Explored:" + std::to_string(res.nodesExplored), {rx + 10.f, cardY + 56.f}, 11, Theme::AccentCyan, true, false);
        drawText(window, "Planning Time: " + fmtFloat(res.planningTimeMs, 3) + " ms", {rx + 10.f, cardY + 78.f}, 11, Theme::TextPrimary, true, false);
        drawText(window, "Status:        " + toString(res.status), {rx + 10.f, cardY + 100.f}, 11,
                 res.success() ? Theme::AccentGreen : Theme::AccentOrange, true, true);

        const float cmpY = cardY + 178.f;
        const float cmpH = std::max(180.f, m_rightPanelRect.position.y + m_rightPanelRect.size.y - cmpY - 12.f);
        drawPanelBox(window, {{rx, cmpY}, {rw, cmpH}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
        drawText(window, "A* VS DIJKSTRA COMPARISON", {rx + 10.f, cmpY + 10.f}, 11, Theme::AccentTeal, true, true);

        const ComparisonMetrics& cmp = sim.getStatistics().comparison;
        if (!cmp.valid) {
            drawText(window, "Click 'Run A* vs Dijkstra Benchmark' above", {rx + 10.f, cmpY + 42.f}, 11, Theme::TextSecondary, false, false);
            drawText(window, "to execute both algorithms on this grid.", {rx + 10.f, cmpY + 60.f}, 11, Theme::TextSecondary, false, false);
        } else {
            drawText(window, "Metric              A*          Dijkstra", {rx + 10.f, cmpY + 40.f}, 11, Theme::AccentYellow, true, true);
            drawText(window, "Path Length      " + std::to_string(cmp.aStarPathLength) + "           " + std::to_string(cmp.dijkstraPathLength),
                     {rx + 10.f, cmpY + 68.f}, 11, Theme::TextPrimary, true, false);
            drawText(window, "Nodes Explored   " + std::to_string(cmp.aStarNodesExplored) + "           " + std::to_string(cmp.dijkstraNodesExplored),
                     {rx + 10.f, cmpY + 92.f}, 11, Theme::AccentCyan, true, false);
            drawText(window, "Exec Time (ms)   " + fmtFloat(cmp.aStarTimeMs, 3) + "       " + fmtFloat(cmp.dijkstraTimeMs, 3),
                     {rx + 10.f, cmpY + 116.f}, 11, Theme::TextPrimary, true, false);

            if (cmp.dijkstraNodesExplored > 0) {
                const double reduction = 100.0 * (1.0 - static_cast<double>(cmp.aStarNodesExplored) /
                                                            static_cast<double>(cmp.dijkstraNodesExplored));
                drawText(window, "Heuristic Reduction: " + fmtFloat(reduction, 1) + "% fewer nodes",
                         {rx + 10.f, cmpY + 148.f}, 11, Theme::AccentGreen, true, true);
            }
        }
    } else if (m_rightTab == RightTab::Learning) {
        const auto topics = ExplanationEngine::getEducationalTopics();
        const float cardY = y + static_cast<float>(topics.size()) * 28.f + 10.f;
        const float cardH = std::max(140.f, m_rightPanelRect.position.y + m_rightPanelRect.size.y - cardY - 12.f);
        drawPanelBox(window, {{rx, cardY}, {rw, cardH}}, Theme::BgCard, Theme::BorderActive, 1.f);

        if (m_selectedTopicIdx < topics.size()) {
            const auto& t = topics[m_selectedTopicIdx];
            drawText(window, t.title, {rx + 10.f, cardY + 10.f}, 13, Theme::AccentYellow, false, true);
            const auto lines = wrapString(t.summary, 42);
            float ty = cardY + 34.f;
            for (const auto& ln : lines) {
                if (ty > cardY + cardH - 18.f) break;
                drawText(window, ln, {rx + 10.f, ty}, 12, Theme::TextPrimary, false, false);
                ty += 18.f;
            }
        }
    }
}

void UIManager::renderAnalyticsOverlay(sf::RenderWindow& window, const Simulation& sim) {
    drawPanelBox(window, m_centerRect, Theme::BgPanel, Theme::BorderSubtle, 1.f);
    const float x = m_centerRect.position.x + 18.f;
    const float y = m_centerRect.position.y + 14.f;
    const float w = m_centerRect.size.x - 36.f;
    const float h = m_centerRect.size.y - 28.f;

    drawText(window, "SIMULATION ANALYTICS & PERFORMANCE TELEMETRY", {x, y}, 16, Theme::TextPrimary, false, true);
    drawText(window, "All charts below reflect actual recorded simulation runs (zero fabricated data).",
             {x, y + 24.f}, 12, Theme::AccentCyan, false, false);

    const auto& hist = sim.getStatistics().getRunHistory();
    if (hist.empty()) {
        drawPanelBox(window, {{x, y + 60.f}, {w, 160.f}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
        drawCenteredText(window, "No completed runs recorded yet. Run a scenario to populate real telemetry charts.",
                         {{x, y + 60.f}, {w, 160.f}}, 13, Theme::TextSecondary, false, false);
        return;
    }

    const float chartH = (h - 76.f) / 3.f - 10.f;

    auto drawBarChart = [&](float cy, const std::string& title, ColorRGBA barCol,
                            std::function<double(const RunRecord&)> getter,
                            std::function<double(const RunRecord&)> secondaryGetter,
                            ColorRGBA secCol, const std::string& unit) {
        drawPanelBox(window, {{x, cy}, {w, chartH}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
        drawText(window, title, {x + 12.f, cy + 8.f}, 12, Theme::TextPrimary, false, true);

        double maxVal = 1.0;
        for (const auto& r : hist) {
            maxVal = std::max(maxVal, getter(r));
            if (secondaryGetter) maxVal = std::max(maxVal, secondaryGetter(r));
        }

        const float plotX = x + 16.f;
        const float plotY = cy + 34.f;
        const float plotW = w - 32.f;
        const float plotH = chartH - 58.f;
        const float slotW = plotW / static_cast<float>(hist.size());

        for (std::size_t i = 0; i < hist.size(); ++i) {
            const double v1 = getter(hist[i]);
            const float bH1 = static_cast<float>((v1 / maxVal) * plotH);
            const float bW = secondaryGetter ? std::min(22.f, slotW * 0.35f) : std::min(34.f, slotW * 0.55f);
            const float bx = plotX + static_cast<float>(i) * slotW + 6.f;

            sf::RectangleShape b1({bW, std::max(2.f, bH1)});
            b1.setPosition({bx, plotY + plotH - bH1});
            b1.setFillColor(toSF(barCol));
            window.draw(b1);
            drawText(window, fmtFloat(v1, v1 < 10.0 ? 2 : 0) + unit, {bx, plotY + plotH - bH1 - 14.f}, 9, barCol, true, false);

            if (secondaryGetter) {
                const double v2 = secondaryGetter(hist[i]);
                const float bH2 = static_cast<float>((v2 / maxVal) * plotH);
                sf::RectangleShape b2({bW, std::max(2.f, bH2)});
                b2.setPosition({bx + bW + 3.f, plotY + plotH - bH2});
                b2.setFillColor(toSF(secCol));
                window.draw(b2);
            }

            std::string lbl = hist[i].scenarioName;
            if (lbl.size() > 10) lbl = lbl.substr(0, 9) + ".";
            drawText(window, lbl, {bx, plotY + plotH + 4.f}, 9, Theme::TextSecondary, false, false);
        }
    };

    drawBarChart(y + 52.f, "Planning Time (ms) — A* Execution", Theme::AccentCyan,
                 [](const RunRecord& r) { return r.aStarPlanningMs; }, nullptr, Theme::AccentCyan, "ms");

    drawBarChart(y + 52.f + chartH + 10.f, "Nodes Explored — A* (Violet) vs Dijkstra (Orange)", Theme::AccentVioletHi,
                 [](const RunRecord& r) { return static_cast<double>(r.aStarNodesExplored); },
                 [](const RunRecord& r) { return static_cast<double>(r.dijkstraNodesExplored); },
                 Theme::AccentOrange, "");

    drawBarChart(y + 52.f + 2.f * (chartH + 10.f), "Dynamic Replanning Events per Run", Theme::AccentGreen,
                 [](const RunRecord& r) { return static_cast<double>(r.replans); }, nullptr, Theme::AccentGreen, "");
}

void UIManager::renderLearningOverlay(sf::RenderWindow& window) {
    drawPanelBox(window, m_centerRect, Theme::BgPanel, Theme::BorderSubtle, 1.f);
    const float x = m_centerRect.position.x + 20.f;
    float y = m_centerRect.position.y + 16.f;
    const float w = m_centerRect.size.x - 40.f;

    drawText(window, "A* ALGORITHM & DYNAMIC REPLANNING KNOWLEDGE BASE", {x, y}, 16, Theme::TextPrimary, false, true);
    y += 34.f;

    const auto topics = ExplanationEngine::getEducationalTopics();
    for (const auto& t : topics) {
        const auto lines = wrapString(t.summary, 86);
        const float bh = 30.f + static_cast<float>(lines.size()) * 16.f;
        if (y + bh > m_centerRect.position.y + m_centerRect.size.y - 12.f) break;

        drawPanelBox(window, {{x, y}, {w, bh}}, Theme::BgCard, Theme::BorderSubtle, 1.f);
        drawText(window, t.title, {x + 12.f, y + 6.f}, 12, Theme::AccentYellow, false, true);
        float ly = y + 24.f;
        for (const auto& ln : lines) {
            drawText(window, ln, {x + 12.f, ly}, 11, Theme::TextPrimary, false, false);
            ly += 16.f;
        }
        y += bh + 8.f;
    }
}

void UIManager::renderPresentationHUD(sf::RenderWindow& window, Simulation& sim) {
    // Clean viva card overlaid in top-right corner of center viewport
    const float cw = 260.f;
    const float ch = 265.f;
    const float cx = m_centerRect.position.x + m_centerRect.size.x - cw - 14.f;
    const float cy = m_centerRect.position.y + 52.f;

    drawPanelBox(window, {{cx, cy}, {cw, ch}}, ColorRGBA{12, 18, 34, 236}, Theme::AccentVioletHi, 1.5f);
    drawText(window, "PRESENTATION HUD", {cx + 12.f, cy + 10.f}, 12, Theme::AccentYellow, true, true);

    const Statistics& st = sim.getStatistics();
    const Grid& g = sim.getGrid();
    const std::pair<std::string, std::string> rows[] = {
        {"Algorithm", "A*"},
        {"Environment", std::to_string(g.getSize()) + " x " + std::to_string(g.getSize())},
        {"Start", g.getStart().toString()},
        {"Goal", g.getGoal().toString()},
        {"Static Obs", std::to_string(g.getStaticObstacleCount())},
        {"Dynamic Obs", std::to_string(sim.getDynamicObstacles().size())},
        {"Path Length", std::to_string(st.getPathLength())},
        {"Nodes Explored", std::to_string(st.getNodesExplored())},
        {"Replans", std::to_string(st.getReplanCount())},
        {"Status", toString(sim.getState())}
    };

    float ry = cy + 36.f;
    for (const auto& r : rows) {
        drawText(window, r.first + ":", {cx + 12.f, ry}, 11, Theme::TextSecondary, false, false);
        drawText(window, r.second, {cx + 126.f, ry}, 11, Theme::TextPrimary, true, true);
        ry += 21.f;
    }
}

void UIManager::renderScenarioModal(sf::RenderWindow& window, Simulation& sim) {
    const float W = static_cast<float>(m_windowSize.x);
    const float H = static_cast<float>(m_windowSize.y);
    sf::RectangleShape backdrop({W, H});
    backdrop.setFillColor(sf::Color(4, 6, 14, 195));
    window.draw(backdrop);

    const float mw = std::min(920.f, W - 80.f);
    const float mh = std::min(620.f, H - 80.f);
    const float mx = (W - mw) * 0.5f;
    const float my = (H - mh) * 0.5f;

    drawPanelBox(window, {{mx, my}, {mw, mh}}, Theme::BgPanel, Theme::AccentCyan, 1.5f);
    drawText(window, "SCENARIO MANAGER (10 PREDEFINED ROBOTICS ENVIRONMENTS)", {mx + 18.f, my + 16.f}, 15, Theme::TextPrimary, false, true);

    if (m_previewScenario) {
        const ScenarioData& sc = *m_previewScenario;
        const float px = mx + 340.f;
        const float py = my + 56.f;
        const float pw = mw - 358.f;
        drawText(window, sc.name, {px, py}, 15, Theme::AccentCyan, false, true);
        const auto descLines = wrapString(sc.description, 60);
        float dy = py + 24.f;
        for (const auto& l : descLines) {
            drawText(window, l, {px, dy}, 12, Theme::TextSecondary, false, false);
            dy += 17.f;
        }

        // Mini preview grid
        const float pSide = 290.f;
        const float pGridX = px + (pw - pSide) * 0.5f;
        const float pGridY = dy + 14.f;
        drawPanelBox(window, {{pGridX, pGridY}, {pSide, pSide}}, Theme::GridEmpty, Theme::BorderActive, 1.f);

        const float cs = pSide / static_cast<float>(sc.gridSize);
        for (const auto& st : sc.staticObstacles) {
            sf::RectangleShape b({cs - 1.f, cs - 1.f});
            b.setPosition({pGridX + st.x * cs, pGridY + st.y * cs});
            b.setFillColor(toSF(Theme::StaticBlockEdge));
            window.draw(b);
        }
        for (const auto& dn : sc.dynamicObstacles) {
            sf::RectangleShape b({cs - 1.f, cs - 1.f});
            b.setPosition({pGridX + dn.startPos.x * cs, pGridY + dn.startPos.y * cs});
            b.setFillColor(toSF(Theme::AccentOrange));
            window.draw(b);
        }
        sf::RectangleShape sRect({cs, cs});
        sRect.setPosition({pGridX + sc.start.x * cs, pGridY + sc.start.y * cs});
        sRect.setFillColor(toSF(Theme::AccentGreen));
        window.draw(sRect);

        sf::RectangleShape gRect({cs, cs});
        gRect.setPosition({pGridX + sc.goal.x * cs, pGridY + sc.goal.y * cs});
        gRect.setFillColor(toSF(Theme::AccentRed));
        window.draw(gRect);

        drawText(window,
                 "Grid: " + std::to_string(sc.gridSize) + "x" + std::to_string(sc.gridSize) +
                 "  |  Static: " + std::to_string(sc.staticObstacles.size()) +
                 "  |  Dynamic: " + std::to_string(sc.dynamicObstacles.size()),
                 {px, pGridY + pSide + 12.f}, 12, Theme::TextPrimary, true, false);
    }

    for (const auto& btn : m_modalButtons) {
        drawButton(window, btn);
    }
    (void)sim;
}

void UIManager::renderRandomModal(sf::RenderWindow& window, Simulation& sim) {
    const float W = static_cast<float>(m_windowSize.x);
    const float H = static_cast<float>(m_windowSize.y);
    sf::RectangleShape backdrop({W, H});
    backdrop.setFillColor(sf::Color(4, 6, 14, 195));
    window.draw(backdrop);

    const float mw = 740.f;
    const float mh = 480.f;
    const float mx = (W - mw) * 0.5f;
    const float my = (H - mh) * 0.5f;

    drawPanelBox(window, {{mx, my}, {mw, mh}}, Theme::BgPanel, Theme::AccentVioletHi, 1.5f);
    drawText(window, "RANDOM ENVIRONMENT GENERATOR", {mx + 22.f, my + 16.f}, 15, Theme::TextPrimary, false, true);

    const float cx = mx + 24.f;
    float ry = my + 68.f;
    drawText(window, "Grid Size:", {cx, ry}, 12, Theme::TextSecondary, false, true);
    ry += 66.f;
    drawText(window, "Obstacle Density:", {cx, ry}, 12, Theme::TextSecondary, false, true);
    drawText(window, std::to_string(static_cast<int>(std::round(m_randomConfig.obstacleDensity * 100.0))) + "%",
             {cx + 58.f, ry + 26.f}, 13, Theme::AccentCyan, true, true);
    ry += 66.f;
    drawText(window, "Dynamic Obstacles:", {cx, ry}, 12, Theme::TextSecondary, false, true);
    drawText(window, std::to_string(m_randomConfig.dynamicObstacleCount),
             {cx + 76.f, ry + 26.f}, 13, Theme::AccentOrange, true, true);
    ry += 66.f;
    drawText(window, "Dynamic Movement Pattern:", {cx, ry}, 12, Theme::TextSecondary, false, true);
    ry += 66.f;
    drawText(window, "Random Seed & Solvability:", {cx, ry}, 12, Theme::TextSecondary, false, true);
    drawText(window, std::to_string(m_randomConfig.seed),
             {cx + 66.f, ry + 26.f}, 13, Theme::AccentCyan, true, true);

    if (m_generatedPreview) {
        const float pSide = 310.f;
        const float px = mx + mw - pSide - 26.f;
        const float py = my + 68.f;
        drawPanelBox(window, {{px, py}, {pSide, pSide}}, Theme::GridEmpty, Theme::BorderActive, 1.f);
        const float cs = pSide / static_cast<float>(m_generatedPreview->gridSize);
        for (const auto& st : m_generatedPreview->staticObstacles) {
            sf::RectangleShape b({cs - 1.f, cs - 1.f});
            b.setPosition({px + st.x * cs, py + st.y * cs});
            b.setFillColor(toSF(Theme::StaticBlockEdge));
            window.draw(b);
        }
        for (const auto& dn : m_generatedPreview->dynamicObstacles) {
            sf::RectangleShape b({cs - 1.f, cs - 1.f});
            b.setPosition({px + dn.startPos.x * cs, py + dn.startPos.y * cs});
            b.setFillColor(toSF(Theme::AccentOrange));
            window.draw(b);
        }
        sf::RectangleShape sRect({cs, cs});
        sRect.setPosition({px + m_generatedPreview->start.x * cs, py + m_generatedPreview->start.y * cs});
        sRect.setFillColor(toSF(Theme::AccentGreen));
        window.draw(sRect);
        sf::RectangleShape gRect({cs, cs});
        gRect.setPosition({px + m_generatedPreview->goal.x * cs, py + m_generatedPreview->goal.y * cs});
        gRect.setFillColor(toSF(Theme::AccentRed));
        window.draw(gRect);
    }

    for (const auto& btn : m_modalButtons) {
        drawButton(window, btn);
    }
    (void)sim;
}

void UIManager::renderConfirmModal(sf::RenderWindow& window, Simulation& sim) {
    const float W = static_cast<float>(m_windowSize.x);
    const float H = static_cast<float>(m_windowSize.y);
    sf::RectangleShape backdrop({W, H});
    backdrop.setFillColor(sf::Color(4, 6, 14, 180));
    window.draw(backdrop);

    const float cw = 440.f;
    const float ch = 175.f;
    const float cx = (W - cw) * 0.5f;
    const float cy = (H - ch) * 0.5f;

    drawPanelBox(window, {{cx, cy}, {cw, ch}}, Theme::BgPanel, Theme::AccentOrange, 1.5f);
    drawText(window, "CONFIRM ACTION", {cx + 18.f, cy + 16.f}, 14, Theme::AccentOrange, true, true);
    const auto lines = wrapString(m_confirmMessage, 46);
    float ty = cy + 54.f;
    for (const auto& l : lines) {
        drawText(window, l, {cx + 18.f, ty}, 12, Theme::TextPrimary, false, false);
        ty += 18.f;
    }

    for (const auto& btn : m_modalButtons) {
        drawButton(window, btn);
    }
    (void)sim;
}

void UIManager::renderTooltip(sf::RenderWindow& window, const Simulation& sim) {
    std::string tip;
    if (m_hoveredCell && !m_showScenarioModal && !m_showRandomModal && m_confirmAction == ConfirmAction::None) {
        tip = ExplanationEngine::buildNodeHoverTooltip(
            sim.getCurrentVizStep(),
            *m_hoveredCell,
            sim.getGrid().isStaticObstacle(*m_hoveredCell),
            sim.isDynamicObstacleAt(*m_hoveredCell));
    } else {
        for (const auto& btn : m_buttons) {
            if (btn.bounds.contains(m_mousePos) && !btn.tooltip.empty()) {
                tip = btn.tooltip;
                break;
            }
        }
    }

    if (tip.empty()) return;

    const float tw = std::min(430.f, static_cast<float>(tip.size()) * 6.8f + 22.f);
    const float th = 26.f;
    float tx = m_mousePos.x + 14.f;
    float ty = m_mousePos.y + 18.f;
    if (tx + tw > static_cast<float>(m_windowSize.x) - 8.f) tx = m_mousePos.x - tw - 10.f;
    if (ty + th > static_cast<float>(m_windowSize.y) - 8.f) ty = m_mousePos.y - th - 10.f;

    drawPanelBox(window, {{tx, ty}, {tw, th}}, ColorRGBA{8, 12, 24, 240}, Theme::AccentCyan, 1.f);
    drawText(window, tip, {tx + 8.f, ty + 6.f}, 11, Theme::TextPrimary, true, false);
}

void UIManager::render(sf::RenderWindow& window, Simulation& sim, float fps) {
    renderHeader(window, sim, fps);
    renderLeftPanel(window);

    if (m_screen == AppScreen::Analytics) {
        renderAnalyticsOverlay(window, sim);
    } else if (m_screen == AppScreen::Learning) {
        renderLearningOverlay(window);
    } else {
        renderGridViewport(window, sim);
        if (m_screen == AppScreen::Presentation) {
            renderPresentationHUD(window, sim);
        }
    }

    renderRightPanel(window, sim);

    for (const auto& btn : m_buttons) {
        drawButton(window, btn);
    }

    if (m_showScenarioModal) renderScenarioModal(window, sim);
    if (m_showRandomModal) renderRandomModal(window, sim);
    if (m_confirmAction != ConfirmAction::None) renderConfirmModal(window, sim);

    renderTooltip(window, sim);
}

} // namespace rpp
