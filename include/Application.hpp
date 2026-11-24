#pragma once

#include "Simulation.hpp"
#include "UIManager.hpp"
#include <SFML/Graphics.hpp>
#include <string>

namespace rpp {

class Application {
public:
    explicit Application(const std::string& executableDir = ".");
    int run();

private:
    std::string resolveResourceBase() const;

    std::string m_execDir;
    Simulation m_simulation;
    UIManager m_ui;
};

} // namespace rpp
