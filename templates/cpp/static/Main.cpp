#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <memory>
#include <vector>

#if defined(HX_WINDOWS)
    #include <windows.h>
    #if defined(HXCPP_DEBUG)
        #include <crtdbg.h>
    #endif
#elif defined(HX_MACOS)
    #include <mach-o/dyld.h>
    #include <sys/param.h>
#elif defined(HX_LINUX)
    #include <unistd.h>
    #include <limits.h>
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

extern "C" {
    const char* hxRunLibrary();
    void hxcpp_set_top_of_stack();
    void hxcpp_set_stack_top(unsigned int* top);
    
    // Register primitives for various libraries
    int zlib_register_prims();
    int lime_cairo_register_prims();
    int lime_openal_register_prims();
    
    // Dynamically generated NDLL registrations
    ::foreach ndlls::::if (registerStatics)::
    int ::nameSafe::_register_prims();
    ::end::::end::
}

// ============================================================================
// Platform-Specific Helpers
// ============================================================================

/**
 * Platform-independent logging with timestamps
 */
namespace Logger {
    
    enum class Level {
        INFO,
        WARNING,
        ERROR,
        DEBUG
    };
    
    static void log(Level level, const char* format, ...) {
        va_list args;
        va_start(args, format);
        
        // Get current time for timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        struct tm timeinfo;
        
        #if defined(HX_WINDOWS)
        localtime_s(&timeinfo, &time_t);
        #else
        localtime_r(&time_t, &timeinfo);
        #endif
        
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        // Level string
        const char* levelStr = "";
        switch (level) {
            case Level::INFO:    levelStr = "INFO"; break;
            case Level::WARNING: levelStr = "WARN"; break;
            case Level::ERROR:   levelStr = "ERROR"; break;
            case Level::DEBUG:   levelStr = "DEBUG"; break;
        }
        
        // Print to stdout with timestamp
        fprintf(stdout, "[%s] [%s] ", timestamp, levelStr);
        vfprintf(stdout, format, args);
        fprintf(stdout, "\n");
        fflush(stdout);
        
        va_end(args);
    }
    
    #define LOG_INFO(...)   Logger::log(Logger::Level::INFO, __VA_ARGS__)
    #define LOG_WARN(...)   Logger::log(Logger::Level::WARNING, __VA_ARGS__)
    #define LOG_ERROR(...)  Logger::log(Logger::Level::ERROR, __VA_ARGS__)
    #if defined(DEBUG) || defined(HXCPP_DEBUG)
        #define LOG_DEBUG(...) Logger::log(Logger::Level::DEBUG, __VA_ARGS__)
    #else
        #define LOG_DEBUG(...) ((void)0)
    #endif
}

/**
 * Platform information collector
 */
struct PlatformInfo {
    std::string os;
    std::string arch;
    std::string executablePath;
    std::vector<std::string> arguments;
    
    static PlatformInfo collect(int argc, char* argv[]) {
        PlatformInfo info;
        
        // OS Detection
        #if defined(HX_WINDOWS)
            info.os = "Windows";
            #if defined(_WIN64)
                info.arch = "x86_64";
            #else
                info.arch = "x86";
            #endif
        #elif defined(HX_MACOS)
            info.os = "macOS";
            #if defined(__x86_64__)
                info.arch = "x86_64";
            #elif defined(__aarch64__)
                info.arch = "ARM64";
            #endif
        #elif defined(HX_LINUX)
            info.os = "Linux";
            #if defined(__x86_64__)
                info.arch = "x86_64";
            #elif defined(__i386__)
                info.arch = "x86";
            #elif defined(__aarch64__)
                info.arch = "ARM64";
            #elif defined(__arm__)
                info.arch = "ARM";
            #endif
        #elif defined(HX_ANDROID)
            info.os = "Android";
        #elif defined(HX_IOS)
            info.os = "iOS";
        #else
            info.os = "Unknown";
            info.arch = "Unknown";
        #endif
        
        // Executable path
        info.executablePath = getExecutablePath();
        
        // Arguments
        for (int i = 0; i < argc; i++) {
            if (argv[i]) {
                info.arguments.push_back(argv[i]);
            }
        }
        
        return info;
    }
    
private:
    static std::string getExecutablePath() {
        #if defined(HX_WINDOWS)
            char path[MAX_PATH];
            GetModuleFileNameA(NULL, path, MAX_PATH);
            return std::string(path);
        #elif defined(HX_MACOS)
            char path[PATH_MAX];
            uint32_t size = sizeof(path);
            if (_NSGetExecutablePath(path, &size) == 0) {
                return std::string(path);
            }
        #elif defined(HX_LINUX)
            char path[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
            if (count != -1) {
                return std::string(path, count);
            }
        #endif
        return "Unknown";
    }
};

// ============================================================================
// Error Handler
// ============================================================================

/**
 * Centralized error handler with custom exit codes
 */
class ErrorHandler {
public:
    enum ExitCode {
        SUCCESS = 0,
        INITIALIZATION_FAILED = -1,
        LIBRARY_LOAD_FAILED = -2,
        REGISTRATION_FAILED = -3,
        RUNTIME_ERROR = -4,
        INVALID_ARGUMENTS = -5
    };
    
    static void fatal(ExitCode code, const char* message, const char* detail = nullptr) {
        LOG_ERROR("Fatal Error [%d]: %s", static_cast<int>(code), message);
        if (detail) {
            LOG_ERROR("Detail: %s", detail);
        }
        
        #if defined(HX_WINDOWS) && defined(HXCPP_DEBUG)
        // Show message box in debug mode
        std::string fullMsg = message;
        if (detail) {
            fullMsg += "\n\n";
            fullMsg += detail;
        }
        MessageBoxA(NULL, fullMsg.c_str(), "Fatal Error", MB_ICONERROR | MB_OK);
        #endif
        
        exit(static_cast<int>(code));
    }
};

// ============================================================================
// Primitive Registry Manager
// ============================================================================

/**
 * Manages registration of CFFI primitives
 */
class PrimitiveRegistry {
public:
    struct PrimitiveInfo {
        const char* name;
        int (*registerFunc)();
        bool required;
    };
    
    static bool registerAll() {
        LOG_DEBUG("Registering CFFI primitives...");
        
        std::vector<PrimitiveInfo> primitives = {
            {"zlib", zlib_register_prims, true},
            {"cairo", lime_cairo_register_prims, true},
            {"openal", lime_openal_register_prims, true},
            ::foreach ndlls::::if (registerStatics)::
            {"::nameSafe::", ::nameSafe::_register_prims, true},
            ::end::::end::
        };
        
        bool allSucceeded = true;
        int successCount = 0;
        int failCount = 0;
        
        for (const auto& prim : primitives) {
            LOG_DEBUG("Registering %s primitives...", prim.name);
            
            if (prim.registerFunc) {
                try {
                    int result = prim.registerFunc();
                    if (result == 0) {
                        LOG_DEBUG("✓ %s registered successfully", prim.name);
                        successCount++;
                    } else {
                        LOG_WARN("⚠ %s registration returned %d", prim.name, result);
                        successCount++; // Still consider it successful if it returns non-zero?
                    }
                } catch (...) {
                    LOG_ERROR("✗ Exception during %s registration", prim.name);
                    if (prim.required) {
                        allSucceeded = false;
                        failCount++;
                    }
                }
            } else {
                LOG_ERROR("✗ %s register function is null", prim.name);
                if (prim.required) {
                    allSucceeded = false;
                    failCount++;
                }
            }
        }
        
        LOG_INFO("Primitive registration complete: %d succeeded, %d failed", 
                 successCount, failCount);
        
        return allSucceeded;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================

/**
 * Application entry point with platform-specific handling
 */
#if defined(HX_WINDOWS) && !defined(HXCPP_DEBUG)
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Convert command line to argc/argv format
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    // Convert wide strings to UTF-8
    std::vector<std::string> argvStrings;
    std::vector<char*> argv;
    
    for (int i = 0; i < argc; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        std::string str(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &str[0], len, NULL, NULL);
        argvStrings.push_back(str);
        argv.push_back(&argvStrings[i][0]);
    }
    
    LocalFree(argvW);
    
    return mainImpl(argc, argv.data());
}
#else
int main(int argc, char* argv[]) {
    return mainImpl(argc, argv);
}
#endif

/**
 * Shared implementation for all platforms
 */
static int mainImpl(int argc, char* argv[]) {
    // Set up stack top for garbage collection
    hxcpp_set_top_of_stack();
    
    // Alternative: set explicit stack top
    unsigned int stackTop = 0;
    hxcpp_set_stack_top(&stackTop);
    
    // Collect platform information
    PlatformInfo platformInfo = PlatformInfo::collect(argc, argv);
    
    // Log startup information
    LOG_INFO("=========================================");
    LOG_INFO("Application Starting");
    LOG_INFO("=========================================");
    LOG_INFO("OS: %s (%s)", platformInfo.os.c_str(), platformInfo.arch.c_str());
    LOG_INFO("Executable: %s", platformInfo.executablePath.c_str());
    
    if (argc > 1) {
        LOG_INFO("Arguments:");
        for (size_t i = 1; i < platformInfo.arguments.size(); i++) {
            LOG_INFO("  [%zu] %s", i, platformInfo.arguments[i].c_str());
        }
    }
    
    #if defined(DEBUG) || defined(HXCPP_DEBUG)
    LOG_INFO("Build: Debug");
    #else
    LOG_INFO("Build: Release");
    #endif
    
    // Register all CFFI primitives
    if (!PrimitiveRegistry::registerAll()) {
        ErrorHandler::fatal(
            ErrorHandler::REGISTRATION_FAILED,
            "Failed to register required CFFI primitives"
        );
    }
    
    // Run the Haxe library
    LOG_INFO("Starting Haxe runtime...");
    
    const char* err = nullptr;
    
    try {
        err = hxRunLibrary();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception caught: %s", e.what());
        ErrorHandler::fatal(
            ErrorHandler::RUNTIME_ERROR,
            "C++ exception during hxRunLibrary",
            e.what()
        );
    } catch (...) {
        ErrorHandler::fatal(
            ErrorHandler::RUNTIME_ERROR,
            "Unknown exception during hxRunLibrary"
        );
    }
    
    // Handle initialization errors
    if (err) {
        LOG_ERROR("Haxe runtime error: %s", err);
        ErrorHandler::fatal(
            ErrorHandler::LIBRARY_LOAD_FAILED,
            "Failed to initialize Haxe library",
            err
        );
    }
    
    LOG_INFO("Application completed successfully");
    return ErrorHandler::SUCCESS;
}
