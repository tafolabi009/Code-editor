/**
 * @file profiler.cpp
 * @brief Performance profiling utilities implementation
 */

#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace utils {

// ============================================================================
// Timer Implementation
// ============================================================================

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;
    
    void start() {
        m_startTime = Clock::now();
        m_running = true;
    }
    
    void stop() {
        if (m_running) {
            m_endTime = Clock::now();
            m_running = false;
        }
    }
    
    double elapsedNanoseconds() const {
        TimePoint end = m_running ? Clock::now() : m_endTime;
        return std::chrono::duration<double, std::nano>(end - m_startTime).count();
    }
    
    double elapsedMicroseconds() const {
        return elapsedNanoseconds() / 1000.0;
    }
    
    double elapsedMilliseconds() const {
        return elapsedNanoseconds() / 1000000.0;
    }
    
    double elapsedSeconds() const {
        return elapsedNanoseconds() / 1000000000.0;
    }
    
    bool isRunning() const { return m_running; }
    
private:
    TimePoint m_startTime;
    TimePoint m_endTime;
    bool m_running = false;
};

// ============================================================================
// ProfileData Implementation
// ============================================================================

struct ProfileData {
    std::string name;
    uint64_t callCount = 0;
    double totalTimeNs = 0;
    double minTimeNs = std::numeric_limits<double>::max();
    double maxTimeNs = 0;
    std::vector<double> samples;  // For percentile calculations
    
    static constexpr size_t MAX_SAMPLES = 10000;
    
    void addSample(double timeNs) {
        ++callCount;
        totalTimeNs += timeNs;
        minTimeNs = std::min(minTimeNs, timeNs);
        maxTimeNs = std::max(maxTimeNs, timeNs);
        
        if (samples.size() < MAX_SAMPLES) {
            samples.push_back(timeNs);
        }
    }
    
    double averageTimeNs() const {
        return callCount > 0 ? totalTimeNs / callCount : 0;
    }
    
    double percentile(double p) const {
        if (samples.empty()) return 0;
        
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        
        size_t index = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[index];
    }
    
    double stdDev() const {
        if (samples.size() < 2) return 0;
        
        double mean = averageTimeNs();
        double sumSquares = 0;
        
        for (double sample : samples) {
            double diff = sample - mean;
            sumSquares += diff * diff;
        }
        
        return std::sqrt(sumSquares / (samples.size() - 1));
    }
};

// ============================================================================
// Profiler Implementation
// ============================================================================

class Profiler {
public:
    static Profiler& instance() {
        static Profiler profiler;
        return profiler;
    }
    
    void setEnabled(bool enabled) {
        m_enabled = enabled;
    }
    
    bool isEnabled() const {
        return m_enabled;
    }
    
    void beginSection(const std::string& name) {
        if (!m_enabled) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTimers[name].start();
    }
    
    void endSection(const std::string& name) {
        if (!m_enabled) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_activeTimers.find(name);
        if (it != m_activeTimers.end()) {
            it->second.stop();
            double elapsed = it->second.elapsedNanoseconds();
            m_data[name].name = name;
            m_data[name].addSample(elapsed);
            m_activeTimers.erase(it);
        }
    }
    
    void recordSample(const std::string& name, double valueNs) {
        if (!m_enabled) return;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data[name].name = name;
        m_data[name].addSample(valueNs);
    }
    
    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.clear();
        m_activeTimers.clear();
    }
    
    ProfileData getProfileData(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_data.find(name);
        if (it != m_data.end()) {
            return it->second;
        }
        return ProfileData{};
    }
    
    std::vector<ProfileData> getAllProfileData() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<ProfileData> result;
        result.reserve(m_data.size());
        
        for (const auto& [name, data] : m_data) {
            result.push_back(data);
        }
        
        // Sort by total time (descending)
        std::sort(result.begin(), result.end(),
                  [](const ProfileData& a, const ProfileData& b) {
                      return a.totalTimeNs > b.totalTimeNs;
                  });
        
        return result;
    }
    
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream ss;
        
        ss << "\n=== Performance Profile Report ===\n\n";
        ss << std::left << std::setw(40) << "Section"
           << std::right << std::setw(12) << "Calls"
           << std::setw(15) << "Total (ms)"
           << std::setw(15) << "Avg (µs)"
           << std::setw(15) << "Min (µs)"
           << std::setw(15) << "Max (µs)"
           << std::setw(15) << "P95 (µs)"
           << "\n";
        
        ss << std::string(127, '-') << "\n";
        
        // Sort by total time
        std::vector<std::pair<std::string, ProfileData>> sorted(m_data.begin(), m_data.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) {
                      return a.second.totalTimeNs > b.second.totalTimeNs;
                  });
        
        for (const auto& [name, data] : sorted) {
            ss << std::left << std::setw(40) << name
               << std::right << std::setw(12) << data.callCount
               << std::setw(15) << std::fixed << std::setprecision(3)
                                << (data.totalTimeNs / 1000000.0)
               << std::setw(15) << std::setprecision(2)
                                << (data.averageTimeNs() / 1000.0)
               << std::setw(15) << (data.minTimeNs / 1000.0)
               << std::setw(15) << (data.maxTimeNs / 1000.0)
               << std::setw(15) << (data.percentile(0.95) / 1000.0)
               << "\n";
        }
        
        ss << "\n";
        return ss.str();
    }
    
    void printReport() const {
        std::cout << generateReport();
    }
    
private:
    Profiler() = default;
    
    mutable std::mutex m_mutex;
    std::map<std::string, ProfileData> m_data;
    std::map<std::string, Timer> m_activeTimers;
    bool m_enabled = false;
};

// ============================================================================
// ScopedProfile Implementation
// ============================================================================

class ScopedProfile {
public:
    explicit ScopedProfile(const std::string& name)
        : m_name(name) {
        Profiler::instance().beginSection(name);
    }
    
    ~ScopedProfile() {
        Profiler::instance().endSection(m_name);
    }
    
    // Non-copyable
    ScopedProfile(const ScopedProfile&) = delete;
    ScopedProfile& operator=(const ScopedProfile&) = delete;
    
private:
    std::string m_name;
};

// ============================================================================
// FrameCounter Implementation
// ============================================================================

class FrameCounter {
public:
    void beginFrame() {
        m_frameStart = std::chrono::high_resolution_clock::now();
    }
    
    void endFrame() {
        auto now = std::chrono::high_resolution_clock::now();
        double frameTimeMs = std::chrono::duration<double, std::milli>(now - m_frameStart).count();
        
        m_frameTimes[m_frameIndex] = frameTimeMs;
        m_frameIndex = (m_frameIndex + 1) % FRAME_HISTORY;
        ++m_totalFrames;
        
        // Update FPS every second
        double elapsed = std::chrono::duration<double>(now - m_lastFpsUpdate).count();
        if (elapsed >= 1.0) {
            m_currentFps = m_framesSinceUpdate / elapsed;
            m_framesSinceUpdate = 0;
            m_lastFpsUpdate = now;
        }
        ++m_framesSinceUpdate;
    }
    
    double getCurrentFps() const { return m_currentFps; }
    
    double getAverageFrameTimeMs() const {
        double sum = 0;
        size_t count = std::min(m_totalFrames, static_cast<uint64_t>(FRAME_HISTORY));
        for (size_t i = 0; i < count; ++i) {
            sum += m_frameTimes[i];
        }
        return count > 0 ? sum / count : 0;
    }
    
    double getMinFrameTimeMs() const {
        size_t count = std::min(m_totalFrames, static_cast<uint64_t>(FRAME_HISTORY));
        double minTime = std::numeric_limits<double>::max();
        for (size_t i = 0; i < count; ++i) {
            minTime = std::min(minTime, m_frameTimes[i]);
        }
        return count > 0 ? minTime : 0;
    }
    
    double getMaxFrameTimeMs() const {
        size_t count = std::min(m_totalFrames, static_cast<uint64_t>(FRAME_HISTORY));
        double maxTime = 0;
        for (size_t i = 0; i < count; ++i) {
            maxTime = std::max(maxTime, m_frameTimes[i]);
        }
        return maxTime;
    }
    
    uint64_t getTotalFrames() const { return m_totalFrames; }
    
private:
    static constexpr size_t FRAME_HISTORY = 120;
    
    std::chrono::high_resolution_clock::time_point m_frameStart;
    std::chrono::high_resolution_clock::time_point m_lastFpsUpdate;
    
    double m_frameTimes[FRAME_HISTORY] = {};
    size_t m_frameIndex = 0;
    uint64_t m_totalFrames = 0;
    
    double m_currentFps = 0;
    size_t m_framesSinceUpdate = 0;
};

// ============================================================================
// MemoryProfiler Implementation
// ============================================================================

class MemoryProfiler {
public:
    static MemoryProfiler& instance() {
        static MemoryProfiler profiler;
        return profiler;
    }
    
    void recordAllocation(const std::string& tag, size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto& stats = m_stats[tag];
        stats.currentBytes += bytes;
        stats.totalAllocated += bytes;
        ++stats.allocationCount;
        stats.peakBytes = std::max(stats.peakBytes, stats.currentBytes);
    }
    
    void recordDeallocation(const std::string& tag, size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto& stats = m_stats[tag];
        stats.currentBytes = (bytes <= stats.currentBytes) ? stats.currentBytes - bytes : 0;
        ++stats.deallocationCount;
    }
    
    struct MemoryStats {
        size_t currentBytes = 0;
        size_t peakBytes = 0;
        size_t totalAllocated = 0;
        uint64_t allocationCount = 0;
        uint64_t deallocationCount = 0;
    };
    
    MemoryStats getStats(const std::string& tag) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_stats.find(tag);
        if (it != m_stats.end()) {
            return it->second;
        }
        return MemoryStats{};
    }
    
    size_t getTotalCurrentBytes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        size_t total = 0;
        for (const auto& [tag, stats] : m_stats) {
            total += stats.currentBytes;
        }
        return total;
    }
    
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream ss;
        
        ss << "\n=== Memory Profile Report ===\n\n";
        ss << std::left << std::setw(30) << "Tag"
           << std::right << std::setw(15) << "Current"
           << std::setw(15) << "Peak"
           << std::setw(15) << "Total Alloc"
           << std::setw(12) << "Allocs"
           << std::setw(12) << "Deallocs"
           << "\n";
        
        ss << std::string(99, '-') << "\n";
        
        auto formatBytes = [](size_t bytes) -> std::string {
            std::ostringstream oss;
            if (bytes >= 1024 * 1024 * 1024) {
                oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
            } else if (bytes >= 1024 * 1024) {
                oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
            } else if (bytes >= 1024) {
                oss << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB";
            } else {
                oss << bytes << " B";
            }
            return oss.str();
        };
        
        for (const auto& [tag, stats] : m_stats) {
            ss << std::left << std::setw(30) << tag
               << std::right << std::setw(15) << formatBytes(stats.currentBytes)
               << std::setw(15) << formatBytes(stats.peakBytes)
               << std::setw(15) << formatBytes(stats.totalAllocated)
               << std::setw(12) << stats.allocationCount
               << std::setw(12) << stats.deallocationCount
               << "\n";
        }
        
        ss << "\n";
        return ss.str();
    }
    
    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.clear();
    }
    
private:
    MemoryProfiler() = default;
    
    mutable std::mutex m_mutex;
    std::map<std::string, MemoryStats> m_stats;
};

// ============================================================================
// Global Functions
// ============================================================================

void enableProfiling(bool enable) {
    Profiler::instance().setEnabled(enable);
}

bool isProfilingEnabled() {
    return Profiler::instance().isEnabled();
}

void resetProfiler() {
    Profiler::instance().reset();
}

void printProfileReport() {
    Profiler::instance().printReport();
}

// ============================================================================
// Convenience Macros (defined in header)
// ============================================================================

/*
 * Usage:
 * 
 * // Manual profiling
 * PROFILE_BEGIN("MyOperation");
 * // ... do work ...
 * PROFILE_END("MyOperation");
 * 
 * // Scoped profiling (automatically ends when scope exits)
 * {
 *     PROFILE_SCOPE("MyFunction");
 *     // ... do work ...
 * }
 * 
 * // Function profiling
 * void myFunction() {
 *     PROFILE_FUNCTION();
 *     // ... do work ...
 * }
 */

} // namespace utils

// Macro definitions (would be in header)
#define PROFILE_BEGIN(name) ::utils::Profiler::instance().beginSection(name)
#define PROFILE_END(name)   ::utils::Profiler::instance().endSection(name)
#define PROFILE_SCOPE(name) ::utils::ScopedProfile __profiler##__LINE__(name)
#define PROFILE_FUNCTION()  PROFILE_SCOPE(__PRETTY_FUNCTION__)
