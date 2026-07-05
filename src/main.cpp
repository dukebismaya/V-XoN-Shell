#include "extensions/redirection.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
constexpr char PATH_DELIMITER = ';';
const char *HOME_DIR = std::getenv("USERPROFILE");
const char *USER_NAME = std::getenv("USERNAME");
#else
constexpr char PATH_DELIMITER = ':';
#include <sys/wait.h>
#include <unistd.h>
const char *HOME_DIR = std::getenv("HOME");
const char *USER_NAME = std::getenv("USER");
#endif

namespace fs = std::filesystem;

const std::vector<std::string> shell_builtins{"cd", "echo", "exit", "type",
                                              "pwd"};
const std::unordered_map<char, bool> check_escape{
    {'"', true}, {'\\', true}, {'\n', true}, {'$', true}, {'`', true}};

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

auto parse_args(std::string &raw_command) -> std::vector<std::string> {
  std::vector<std::string> args;
  std::string curr_arg;
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool hasChars = false;
  for (size_t i = 0; i < raw_command.length(); ++i) {
    char c = raw_command[i];

    if (c == '\\') {

      if (in_double_quote) {

        if (i + 1 < raw_command.length() &&
            check_escape.count(raw_command[i + 1])) {
          curr_arg += raw_command[++i];
        } else {
          curr_arg += c;
        }
      }

      else {
        if (in_single_quote) {
          curr_arg += c;
        } else if (i + 1 < raw_command.length()) {
          curr_arg += raw_command[++i];
        }
      }

      hasChars = true;
    }

    else if (c == '\"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      hasChars = true;
    }

    else if (c == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      hasChars = true;
    }

    else if (in_single_quote || in_double_quote) {
      curr_arg += c;
      hasChars = true;
    }

    else if (c == ' ' || c == '\t' || c == '\n') {
      if (hasChars) {
        args.push_back(curr_arg);
        curr_arg.clear();
        hasChars = false;
      }
    }

    else if (c == '>' || (c == '1' && i + 1 < raw_command.length() &&
                          raw_command[i + 1] == '>')) {
      if (hasChars) {
        args.push_back(curr_arg);
        curr_arg.clear();
        if (c == '1') {
          i++;
        }
        args.push_back(">");
      }
    }

    else {
      curr_arg += c;
      hasChars = true;
    }
  }
  if (hasChars)
    args.push_back(curr_arg);
  return args;
}

auto parse_echo(std::vector<std::string> &args) -> std::string {
  std::string output;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == ">") {
      if (i + 1 < args.size()) {
        redirect_output(output, args[i + 1]);
      }
      return "\n";
    }
    output += args[i];
    if (i + 1 < args.size() && args[i + 1] != ">")
      output += " ";
  }
  return output + "\n";
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

auto pwd() -> bool {
  fs::path curr_path = fs::current_path();
  std::cout << std::format("{}\n", curr_path.string());
  return true;
}

auto run_program(std::string &cmd_name, std::vector<std::string> &args)
    -> bool {

  if (cmd_name == "pwd")
    return pwd();

#if defined(_WIN32) || defined(_WIN64)
  if (cmd_name == "cat") {
    for (const auto &filepath : args) {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        std::cerr << "cat: " << filepath << ": No such file or directory\n";
        continue;
      }
      std::string line;
      while (std::getline(file, line)) {
        std::cout << line << "\n";
      }
    }
    return true;
  }
  if (cmd_name == "clear") {
    cmd_name = "cls";
  }

  std::string win_cmd = cmd_name;
  for (auto &arg : args) {
    std::string fixed_arg = arg;
    std::replace(fixed_arg.begin(), fixed_arg.end(), '/', '\\');
    win_cmd += " ";
    if (fixed_arg.find(' ') != std::string::npos)
      win_cmd += "\"" + fixed_arg + "\"";
    else
      win_cmd += fixed_arg;
  }
  std::system(win_cmd.c_str());
#else
  std::string exec_path = find_executable(cmd_name);
  if (exec_path.empty())
    return false;

  std::vector<std::string> build_args;
  build_args.push_back(cmd_name);
  for (auto &arg : args) {
    build_args.push_back(std::move(arg));
  }

  std::vector<char *> c_args;
  for (const auto &arg : build_args) {
    c_args.push_back(const_cast<char *>(arg.c_str()));
  }
  c_args.push_back(nullptr);

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

  // only need 1st arg rest discard

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
  std::string raw_command{};

  while (true) {
    std::cout << std::format("┌──({}@V-Xon)-", USER_NAME) << "["
              << fs::current_path().string() << "]\n└─$ ";
    std::getline(std::cin, raw_command);
    auto args = parse_args(raw_command);
    if (args.empty())
      continue;
    std::string get_type = args[0];
    args.erase(args.begin());

    if (get_type == "echo") {
      std::cout << parse_echo(args);
      continue;
    }
    if (get_type == "type") {
      std::cout << parse_type(raw_command);
      continue;
    }
    if (get_type == "exit")
      std::exit(0);

    if (get_type == "cd") {
      parse_cd(raw_command);
      continue;
    }

    if (!run_program(get_type, args))
      std::cout << std::format("{}: command not found\n", get_type);
  }
}
