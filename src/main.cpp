#include "Application.hpp"
#include <filesystem>

int main(int argc, char* argv[]) {
    std::string execDir = ".";
    if (argc > 0 && argv[0] != nullptr) {
        std::error_code ec;
        const auto p = std::filesystem::weakly_canonical(argv[0], ec);
        if (!ec && p.has_parent_path()) {
            execDir = p.parent_path().string();
        }
    }

    rpp::Application app(execDir);
    return app.run();
}
