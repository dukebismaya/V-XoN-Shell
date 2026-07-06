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
#include <fcntl.h>
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

    else if (c == '>' && !curr_arg.empty() &&
             (curr_arg.back() == '1' || curr_arg.back() == '2') &&
             i + 1 < raw_command.length()) {

      auto curr_arg_back = curr_arg.back();
      curr_arg.pop_back();
      if (!curr_arg.empty()) {
        args.push_back(curr_arg);
      }
      curr_arg.clear();
      hasChars = false;
      args.push_back(std::string(1, curr_arg_back) + ">");
    }

    else if (c == '>' || i + 1 < raw_command.length()) {
      if (hasChars) {
        args.push_back(curr_arg);
        curr_arg.clear();
        hasChars = false;
      }
      args.push_back("1>");
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
  std::string err_output;
  bool allow_redirection_err, allow_redirection_out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "1>") {
      allow_redirection_out = true;
      if (i + 1 < args.size()) {

        output = args[i + 1];
      }
      i++;
      continue;
    } else if (args[i] == "2>") {
      allow_redirection_err = true;
      if (i + 1 < args.size()) {
        err_output = args[i + 1];
      }
      i++;
      continue;
    }

    output += args[i];
    if (i + 1 < args.size() && args[i + 1] != "1>" && args[i + 1] != "2>")
      output += " ";

    if (allow_redirection_out) {
      redirect_output(output + "\n", output);
      return "";
    } else if (allow_redirection_err) {
      redirect_output("", err_output);
    }
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

auto parse_type(std::vector<std::string> &args) -> std::string {
  if (args.empty())
    return "type: missing argument\n";

  std::string cmd = args[0]; // Only 1st argument is needed
  std::string output{std::format("{}: not found", cmd)};

  if (std::find(shell_builtins.begin(), shell_builtins.end(), cmd) !=
      shell_builtins.end()) {
    output = std::format("{} is a shell builtin", cmd);
  }

  else {
    std::string exec_path = find_executable(cmd);
    if (!exec_path.empty()) {
      output = std::format("{} is {}", cmd, exec_path);
    }
  }

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == ">") {
      if (i + 1 < args.size()) // the redirected file dump exists or not
        redirect_output(output + "\n", args[i + 1]);
      return ""; // nothing just pass to the next prompt
    }
  }
  return output + "\n";
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

  auto pos_redirect_override_out = std::find(args.begin(), args.end(), "1>");
  auto pos_redirect_override_err = std::find(args.begin(), args.end(), "2>");
  bool allow_redirection_out = pos_redirect_override_out != args.end();
  bool allow_redirection_err = pos_redirect_override_err != args.end();

#if defined(_WIN32) || defined(_WIN64)
  if (cmd_name == "cat") {

    std::string output{};
    std::string err_output{};
    for (int i = 0; i < args.size(); ++i) {
      if (args[i] == "1>" || args[i] == "2") {
        i++;
        continue;
      }
      auto filepath = args[i];
      std::ifstream file(filepath);
      if (!file.is_open()) {
        auto err_msg = "cat: " + filepath + ": No such file or directory\n";
        if (allow_redirection_err) {
          err_output += err_msg;
        } else {
          std::cerr << err_msg;
        }
        continue;
      }
      std::string line;
      while (std::getline(file, line)) {
        if (allow_redirection_out) {
          output += line + "\n";
        } else {
          std::cout << line << "\n";
        }
      }
    }

    if (allow_redirection_out) {
      if (std::next(pos_redirect_override_out) != args.end())
        redirect_output(output, *std::next(pos_redirect_override_out));
    } else if (allow_redirection_err) {
      if (std::next(pos_redirect_override_err) != args.end())
        redirect_output(err_output, *std::next(pos_redirect_override_err));
    }
    return true;
  }
  if (cmd_name == "clear") {
    cmd_name = "cls";
  } else if (cmd_name == "ls") {
    cmd_name = "dir";
  } else if (cmd_name == "rm") {
    cmd_name = "del";
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

  std::string redirect_file_out{};
  std::string redirect_file_err{};
  if (allow_redirection_out &&
      std::next(pos_redirect_override_out) != args.end()) {
    redirect_file_out = *std::next(pos_redirect_override_out);
  }
  if (allow_redirection_err &&
      std::next(pos_redirect_override_err) != args.end()) {
    redirect_file_err = *std::next(pos_redirect_override_err);
  }

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

  std::vector<char *> c_args_no_redirect;
  if (allow_redirection_out || allow_redirection_err) {
    c_args_no_redirect.push_back(c_args[0]);
    for (size_t k = 1; k < c_args.size() - 1; ++k) {
      if (std::string(c_args[k]) == "1>" || std::string(c_args[k]) == "2>") {
        k++;
        continue;
      }
      c_args_no_redirect.push_back(c_args[k]);
    }
    c_args_no_redirect.push_back(nullptr);
  }

  pid_t pid = fork();
  if (pid < 0) {
    return false;
  } else if (pid == 0) {
    if (allow_redirection_out && !redirect_file_out.empty()) {
      int fd = open(redirect_file_out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        close(fd);
      }
    }
    if (allow_redirection_err && !redirect_file_err.empty()) {
      int fd = open(redirect_file_err.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0) {
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
    }

    char **argv_to_use = (allow_redirection_out || allow_redirection_err)
                             ? c_args_no_redirect.data()
                             : c_args.data();

    if (execvp(exec_path.c_str(), argv_to_use) == -1) {
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
      std::cout << parse_type(args);
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
