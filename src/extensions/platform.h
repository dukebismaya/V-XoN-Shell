#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
constexpr char PATH_DELIMITER = ';';
inline const char *HOME_DIR = std::getenv("USERPROFILE");
inline const char *USER_NAME = std::getenv("USERNAME");
#else
constexpr char PATH_DELIMITER = ':';
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

inline const char *HOME_DIR = std::getenv("HOME");
inline const char *USER_NAME = std::getenv("USER");
#endif



inline const std::unordered_set<std::string> SHELL_BUILTINS{
    "cd", "complete", "echo", "exit", "jobs", "pwd", "type"};

inline std::unordered_map<std::string, std::string> register_completion;
