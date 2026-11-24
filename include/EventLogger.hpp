#pragma once

#include <string>
#include <vector>

namespace rpp {

enum class LogSeverity {
    Info,
    Success,
    Warning,
    Critical
};

struct LogEntry {
    std::string timestamp;
    LogSeverity severity = LogSeverity::Info;
    std::string message;
};

class EventLogger {
public:
    explicit EventLogger(std::size_t maxEntries = 250);

    void log(const std::string& message, LogSeverity severity = LogSeverity::Info);
    void clear();

    const std::vector<LogEntry>& getEntries() const noexcept { return m_entries; }

private:
    static std::string currentTimestamp();

    std::size_t m_maxEntries = 250;
    std::vector<LogEntry> m_entries;
};

} // namespace rpp
