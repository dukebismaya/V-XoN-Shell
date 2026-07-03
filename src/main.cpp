#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
constexpr char PATH_DELIMITER = ';';
const char *HOME_DIR = std::getenv("USERPROFILE");
#else
constexpr char PATH_DELIMITER = ':';
#include <sys/wait.h>
#include <unistd.h>
const char *HOME_DIR = std::getenv("HOME");
#endif

namespace fs = std::filesystem;

const std::vector<std::string> shell_builtins{"cd", "echo", "exit", "type",
                                              "pwd"};

auto filter_command(std::string &command, size_t step_trim) -> void {
  size_t curr_idx = 0;
  bool seen_space = true;
  for (size_t i = step_trim; i < command.size(); ++i) {
    if (command[i] == ' ' || command[i] == '\t' || command[i] == '\n') {
      if (!seen_space) {
        command[curr_idx++] = ' ';
        seen_space = true;
      }
    } else {
      command[curr_idx++] = command[i];
      seen_space = false;
    }
  }
  while (curr_idx > 0 && command[curr_idx - 1] == ' ')
    curr_idx--;

  command.resize(curr_idx);
}

auto parse_echo(std::string &command) -> std::string {
  filter_command(command, 4);
  if (command.starts_with("SHELL"))
    return "V-XoN Shell(v0.0.1)\nCopyright(C)2026 By Bismaya All rights "
           "reserved.";
  return command;
}

auto find_executable(const std::string &command) -> std::string {
  const auto path_env = std::getenv("PATH");
  if (!path_env)
    return "";

  std::stringstream raw_path(path_env);
  std::string curr_path{};

  while (std::getline(raw_path, curr_path, PATH_DELIMITER)) {
    auto curr_full_path = static_cast<fs::path>(curr_path) / command;

    if (fs::exists(curr_full_path)) {
      fs::perms p = fs::status(curr_full_path).permissions();
      bool isExecutable = (p & (fs::perms::owner_exec | fs::perms::group_exec |
                                fs::perms::others_exec)) != fs::perms::none;
      if (isExecutable)
        return curr_full_path.string();
    }
  }
  return "";
}

auto parse_type(std::string &command) -> std::string {
  filter_command(command, 4);

  if (std::find(shell_builtins.begin(), shell_builtins.end(), command) !=
      shell_builtins.end())
    return std::format("{} is a shell builtin", command);

  std::string exec_path = find_executable(command);
  if (!exec_path.empty())
    return std::format("{} is {}", command, exec_path);

  return std::format("{}: not found", command);
}

auto pwd(std::string &command) -> bool {
  filter_command(command, 0);
  if (command != "pwd") {
    return false;
  }
  fs::path curr_path = fs::current_path();
  std::cout << std::format("{}\n", curr_path.string());
  return true;
}

auto run_program(std::string &command) -> bool {

  std::stringstream cmd(command);
  std::string curr_cmd;
  cmd >> curr_cmd;

  std::string exec_path = find_executable(curr_cmd);
  if (exec_path.empty())
    return false;

  std::vector<std::string> build_args;
  build_args.push_back(curr_cmd); // first arg -> command/executable name

  std::string curr_arg;
  while (cmd >> curr_arg) {
    build_args.push_back(curr_arg);
  }

  std::vector<char *> c_args;
  for (const auto &arg : build_args) {
    c_args.push_back(const_cast<char *>(arg.c_str()));
  }
  c_args.push_back(nullptr);

#if defined(_WIN32) || defined(_WIN64)
  std::system(command.c_str());
#else
  pid_t pid = fork();
  if (pid < 0) {
    return false;
  } else if (pid == 0) {
    if (execvp(exec_path.c_str(), c_args.data()) == -1) {
      std::exit(1);
    }
  } else {
    int status;
    waitpid(pid, &status, 0);
  }
#endif

  return true;
}

auto parse_cd(std::string &command) -> void {
  filter_command(command, 2);
  if (command.empty()) {
    if (fs::exists(HOME_DIR)) {
      fs::current_path(HOME_DIR);
    }
    return;
  }
  std::stringstream cmd(command);
  std::string curr_arg;

  // we only need 1st arg rest discard

  cmd >> curr_arg;
  auto find_arsenic_pos = curr_arg.find("~");
  if (find_arsenic_pos != std::string::npos) {
    curr_arg.replace(find_arsenic_pos, 1, HOME_DIR);
  }
  if (fs::exists(curr_arg) && fs::is_directory(curr_arg)) {
    fs::current_path(curr_arg);
  } else {
    std::cout << std::format("cd: {}: No such file or directory", curr_arg)
              << std::endl;
  }
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string command{};
  while (true) {
    std::cout << "$ ";
    std::getline(std::cin, command);
    if (command == "echo" || command.starts_with("echo ")) {
      std::cout << parse_echo(command) << std::endl;
      continue;
    }
    if (command.starts_with("type ")) {
      std::cout << parse_type(command) << std::endl;
      continue;
    }
    if (command == "exit" || command.starts_with("exit "))
      std::exit(0);

    if (command == "cd" || command.starts_with("cd ")) {
      parse_cd(command);
      continue;
    }

    if (!run_program(command) && !(pwd(command)))
      std::cout << std::format("{}: command not found\n", command);
  }
}
