#pragma once

#include "Simulation.hpp"
#include "Types.hpp"
#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace rpp {

struct UIButton {
    std::string id;
    std::string label;
    std::string tooltip;
    sf::FloatRect bounds;
    bool active = false;
    bool enabled = true;
    ColorRGBA accent = Theme::AccentCyan;
    std::function<void()> onClick;
};

class UIManager {
public:
    UIManager();

    bool loadFonts(const std::string& assetBaseDir);

    void handleEvent(const sf::Event& event, sf::RenderWindow& window, Simulation& sim);
    void update(double dt, const sf::RenderWindow& window, Simulation& sim);
    void render(sf::RenderWindow& window, Simulation& sim, float fps);

private:
    enum class ConfirmAction {
        None,
        NewEnvironment,
        ClearObstacles,
        ResetAll
    };

    void rebuildLayout(sf::Vector2u winSize, Simulation& sim);
    void addButton(const std::string& id,
                   const std::string& label,
                   const std::string& tooltip,
                   sf::FloatRect rect,
                   bool active,
                   ColorRGBA accent,
                   std::function<void()> cb,
                   bool enabled = true);

    void handleGridClick(sf::Vector2f mousePos, sf::Mouse::Button button, Simulation& sim);
    std::optional<GridCoord> screenToGrid(sf::Vector2f mousePos, int gridSize) const;

    // Rendering helpers
    void drawPanelBox(sf::RenderWindow& window, sf::FloatRect rect, ColorRGBA fill, ColorRGBA border, float thickness = 1.f);
    void drawText(sf::RenderWindow& window, const std::string& str, sf::Vector2f pos, unsigned int size, ColorRGBA color, bool mono = false, bool bold = false);
    void drawCenteredText(sf::RenderWindow& window, const std::string& str, sf::FloatRect rect, unsigned int size, ColorRGBA color, bool mono = false, bool bold = false);
    void drawButton(sf::RenderWindow& window, const UIButton& btn);

    void renderHeader(sf::RenderWindow& window, const Simulation& sim, float fps);
    void renderLeftPanel(sf::RenderWindow& window);
    void renderGridViewport(sf::RenderWindow& window, const Simulation& sim);
    void renderRightPanel(sf::RenderWindow& window, Simulation& sim);
    void renderScenarioModal(sf::RenderWindow& window, Simulation& sim);
    void renderRandomModal(sf::RenderWindow& window, Simulation& sim);
    void renderAnalyticsOverlay(sf::RenderWindow& window, const Simulation& sim);
    void renderLearningOverlay(sf::RenderWindow& window);
    void renderPresentationHUD(sf::RenderWindow& window, Simulation& sim);
    void renderConfirmModal(sf::RenderWindow& window, Simulation& sim);
    void renderTooltip(sf::RenderWindow& window, const Simulation& sim);

    void triggerConfirm(ConfirmAction action, const std::string& prompt);

    sf::Font m_sansFont;
    sf::Font m_sansBoldFont;
    sf::Font m_monoFont;
    bool m_fontsLoaded = false;

    AppScreen m_screen = AppScreen::LabWorkspace;
    RightTab m_rightTab = RightTab::MetricsAndLog;
    EditTool m_editTool = EditTool::AddStaticObstacle;
    DynamicPattern m_selectedDynPattern = DynamicPattern::HorizontalPatrol;

    // Modal overlays
    bool m_showScenarioModal = false;
    bool m_showRandomModal = false;
    int m_selectedScenarioIndex = 0;
    std::optional<ScenarioData> m_previewScenario;
    RandomGenConfig m_randomConfig;
    std::optional<ScenarioData> m_generatedPreview;

    ConfirmAction m_confirmAction = ConfirmAction::None;
    std::string m_confirmMessage;

    // Layout regions
    sf::Vector2u m_windowSize{1600, 960};
    sf::FloatRect m_headerRect;
    sf::FloatRect m_leftPanelRect;
    sf::FloatRect m_centerRect;
    sf::FloatRect m_gridRect;
    sf::FloatRect m_rightPanelRect;

    std::vector<UIButton> m_buttons;
    std::vector<UIButton> m_modalButtons;

    sf::Vector2f m_mousePos{0.f, 0.f};
    std::optional<GridCoord> m_hoveredCell;
    bool m_leftMouseDragging = false;
    GridCoord m_lastDraggedCell{-1, -1};
    double m_pulseTime = 0.0;
    std::size_t m_selectedTopicIdx = 0;
    int m_logScrollOffset = 0;
};

} // namespace rpp
