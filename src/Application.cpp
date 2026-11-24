#include "Application.hpp"
#include <algorithm>
#include <filesystem>

namespace rpp {

Application::Application(const std::string& executableDir)
    : m_execDir(executableDir) {}

std::string Application::resolveResourceBase() const {
    namespace fs = std::filesystem;
    const fs::path exePath(m_execDir);
    const std::vector<fs::path> candidates = {
        exePath,
        exePath.parent_path(),
        exePath.parent_path().parent_path(),
        fs::current_path(),
        fs::current_path().parent_path()
    };

    for (const auto& base : candidates) {
        if (base.empty()) continue;
        std::error_code ec;
        if (fs::exists(base / "assets", ec) || fs::exists(base / "scenarios", ec)) {
            return base.string();
        }
    }
    return ".";
}

int Application::run() {
    const std::string resourceBase = resolveResourceBase();
    m_simulation.getScenarioManager().loadDirectory((std::filesystem::path(resourceBase) / "scenarios").string());
    m_ui.loadFonts(resourceBase);

    sf::RenderWindow window(sf::VideoMode({1600u, 960u}),
                            "Intelligent Robot Path Planning & Dynamic Obstacle Avoidance");
    window.setFramerateLimit(60);

    sf::Clock frameClock;
    float smoothedFps = 60.f;

    while (window.isOpen()) {
        const float rawDt = frameClock.restart().asSeconds();
        const double dt = std::min(static_cast<double>(rawDt), 0.05);
        if (rawDt > 0.0001f) {
            smoothedFps = smoothedFps * 0.9f + (1.f / rawDt) * 0.1f;
        }

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                const float w = static_cast<float>(std::max(1024u, resized->size.x));
                const float h = static_cast<float>(std::max(680u, resized->size.y));
                sf::View view(sf::FloatRect({0.f, 0.f}, {w, h}));
                window.setView(view);
            }
            m_ui.handleEvent(*event, window, m_simulation);
        }

        m_simulation.update(dt);
        m_ui.update(dt, window, m_simulation);

        window.clear(sf::Color(Theme::BgDeepNavy.r, Theme::BgDeepNavy.g, Theme::BgDeepNavy.b, 255));
        m_ui.render(window, m_simulation, smoothedFps);
        window.display();
    }

    return 0;
}

} // namespace rpp
