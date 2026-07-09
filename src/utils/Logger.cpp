#include "Logger.h"

#include <iostream>

void Logger::Info(const std::string& message) {
    std::cout << "[INFO] " << message << "\n";
}

void Logger::Warning(const std::string& message) {
    std::cout << "[WARNING] " << message << "\n";
}

void Logger::Error(const std::string& message) {
    std::cerr << "[ERROR] " << message << "\n";
}