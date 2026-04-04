#ifndef FRABGINE_CORE_UTILS_LOGGER_HPP
#define FRABGINE_CORE_UTILS_LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace frabgine {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

class Logger {
private:
    static Logger* instance_;
    static std::mutex mutex_;
    
    std::ofstream fileStream_;
    LogLevel minLevel_ = LogLevel::Info;
    bool logToConsole_ = true;
    
    const char* levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::Trace:   return "TRACE";
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Fatal:   return "FATAL";
            default:                return "UNKNOWN";
        }
    }
    
    std::string getTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
public:
    static Logger& getInstance() {
        if (!instance_) {
            instance_ = new Logger();
        }
        return *instance_;
    }
    
    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
        fileStream_.open(filename, std::ios::app);
    }
    
    void setMinLevel(LogLevel level) { minLevel_ = level; }
    void setLogToConsole(bool enable) { logToConsole_ = enable; }
    
    void log(LogLevel level, const std::string& message) {
        if (level < minLevel_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string formatted = "[" + getTimestamp() + "] [" + 
                                levelToString(level) + "] " + message;
        
        if (logToConsole_) {
            if (level >= LogLevel::Error) {
                std::cerr << formatted << std::endl;
            } else {
                std::cout << formatted << std::endl;
            }
        }
        
        if (fileStream_.is_open()) {
            fileStream_ << formatted << std::endl;
            fileStream_.flush();
        }
    }
    
    template<typename... Args>
    void trace(const char* format, Args... args) {
        std::stringstream ss;
        (ss << ... << args);
        log(LogLevel::Trace, ss.str());
    }
    
    template<typename... Args>
    void debug(const char* format, Args... args) {
        std::stringstream ss;
        (ss << ... << args);
        log(LogLevel::Debug, ss.str());
    }
    
    template<typename... Args>
    void info(const char* format, Args... args) {
        std::stringstream ss;
        (ss << ... << args);
        log(LogLevel::Info, ss.str());
    }
    
    template<typename... Args>
    void warning(const char* format, Args... args) {
        std::stringstream ss;
        (ss << ... << args);
        log(LogLevel::Warning, ss.str());
    }
    
    template<typename... Args>
    void error(const char* format, Args... args) {
        std::stringstream ss;
        (ss << ... << args);
        log(LogLevel::Error, ss.str());
    }
    
    template<typename... Args>
    void fatal(const char* format, Args... args) {
        std::stringstream ss;
        (ss << ... << args);
        log(LogLevel::Fatal, ss.str());
    }
};

// Макросы для удобного логирования
#define LOG_TRACE(...) frabgine::Logger::getInstance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) frabgine::Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...) frabgine::Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARNING(...) frabgine::Logger::getInstance().warning(__VA_ARGS__)
#define LOG_ERROR(...) frabgine::Logger::getInstance().error(__VA_ARGS__)
#define LOG_FATAL(...) frabgine::Logger::getInstance().fatal(__VA_ARGS__)

} // namespace frabgine

#endif // FRABGINE_CORE_UTILS_LOGGER_HPP
