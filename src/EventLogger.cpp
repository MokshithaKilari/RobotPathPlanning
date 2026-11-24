#include "EventLogger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace rpp {

EventLogger::EventLogger(std::size_t maxEntries)
    : m_maxEntries(maxEntries) {}

std::string EventLogger::currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &nowTime);
#else
    localtime_r(&nowTime, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%H:%M:%S");
    return oss.str();
}

void EventLogger::log(const std::string& message, LogSeverity severity) {
    LogEntry entry;
    entry.timestamp = currentTimestamp();
    entry.severity = severity;
    entry.message = message;
    m_entries.push_back(std::move(entry));

    if (m_entries.size() > m_maxEntries) {
        m_entries.erase(m_entries.begin(),
                        m_entries.begin() + static_cast<std::ptrdiff_t>(m_entries.size() - m_maxEntries));
    }
}

void EventLogger::clear() {
    m_entries.clear();
}

} // namespace rpp
