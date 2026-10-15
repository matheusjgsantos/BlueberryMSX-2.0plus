/* spdlog backend for the Pi port logging facility. */

extern "C" {
#include "Log.h"
}

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

std::string findConfigPath()
{
    const char *env = std::getenv("BLUEMSX_INI");
    if (env && *env) {
        return std::string(env);
    }
    if (FILE *f = std::fopen("bluemsx.ini", "r")) {
        std::fclose(f);
        return std::string("bluemsx.ini");
    }
    const char *home = std::getenv("HOME");
    if (home && *home) {
        std::string p = std::string(home) + "/.config/blueMSX/bluemsx.ini";
        if (FILE *f = std::fopen(p.c_str(), "r")) {
            std::fclose(f);
            return p;
        }
    }
    return std::string();
}

std::string levelFromConfig(const std::string &path)
{
    FILE *f = std::fopen(path.c_str(), "r");
    if (!f) {
        return std::string();
    }
    char line[256];
    std::string value;
    while (std::fgets(line, (int)sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        static const char *keys[] = { "settings.logLevel=", "logLevel=" };
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
            size_t k = std::strlen(keys[i]);
            if (std::strncmp(p, keys[i], k) == 0) {
                value = std::string(p + k);
                size_t end = value.find_first_of("\r\n ;#");
                if (end != std::string::npos) {
                    value.erase(end);
                }
                break;
            }
        }
        if (!value.empty()) {
            break;
        }
    }
    std::fclose(f);
    return value;
}

} // namespace

static void mapLevel(const char *name)
{
    std::string n = name;
    for (size_t i = 0; i < n.size(); ++i) {
        n[i] = (char)std::tolower((unsigned char)n[i]);
    }
    if (n == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (n == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (n == "info") {
        spdlog::set_level(spdlog::level::info);
    } else if (n == "warn" || n == "warning") {
        spdlog::set_level(spdlog::level::warn);
    } else if (n == "error") {
        spdlog::set_level(spdlog::level::err);
    } else if (n == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else if (n == "off") {
        spdlog::set_level(spdlog::level::off);
    } else {
        spdlog::set_level(spdlog::level::warn);
        spdlog::warn("unknown log level '{}', defaulting to warn", name);
    }
}

extern "C" void logSetLevelByName(const char *name)
{
    mapLevel(name);
}

extern "C" void logWrite(int level, const char *fmt, ...)
{
    static const spdlog::level::level_enum lv[] = {
        spdlog::level::trace, spdlog::level::debug, spdlog::level::info,
        spdlog::level::warn, spdlog::level::err, spdlog::level::critical
    };
    if (level < 0 || level > 5) {
        level = 3;
    }
    if (!spdlog::should_log(lv[level])) {
        return;
    }
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, (int)sizeof(buf), fmt, ap);
    va_end(ap);
    spdlog::log(lv[level], "{}", buf);
}

extern "C" void logInit(void)
{
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("bluemsx", sink);
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    std::string level;
    const char *env = std::getenv("BLUEMSX_LOG_LEVEL");
    if (env && *env) {
        level = env;
    }
    if (level.empty()) {
        std::string path = findConfigPath();
        if (!path.empty()) {
            level = levelFromConfig(path);
        }
    }
    if (level.empty()) {
        level = "warn";
    }
    mapLevel(level.c_str());
}