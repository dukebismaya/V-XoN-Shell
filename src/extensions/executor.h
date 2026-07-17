#pragma once

#include "platform.h"
#include "redirection.h"

inline auto find_executable(const std::string &command) -> std::string {
  const auto *path_env = std::getenv("PATH");
  if (!path_env)
    return "";

  std::stringstream raw_path(path_env);
  std::string curr_path;

  while (std::getline(raw_path, curr_path, PATH_DELIMITER)) {
    auto full_path = fs::path(curr_path) / command;

    if (fs::exists(full_path)) {
      auto perms = fs::status(full_path).permissions();
      bool is_executable =
          (perms & (fs::perms::owner_exec | fs::perms::group_exec |
                    fs::perms::others_exec)) != fs::perms::none;
      if (is_executable)
        return full_path.string();
    }
  }
  return "";
}

inline auto run_program(const std::string &cmd_name,
                        std::vector<std::string> &args,
                        bool run_in_background = false) -> bool {
  auto redir = extract_redirection(args);

#if defined(_WIN32) || defined(_WIN64)
  // -> custom cat emulation (Windows lacks native cat)
  if (cmd_name == "cat") {
    std::string output;
    std::string err_output;
    for (const auto &filepath : args) {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        auto err_msg =
            std::format("cat: {}: No such file or directory\n", filepath);
        if (redir.has_stderr_redirect())
          err_output += err_msg;
        else
          std::cerr << err_msg;
        continue;
      }
      std::string line;
      while (std::getline(file, line)) {
        if (redir.has_stdout_redirect())
          output += line + "\n";
        else
          std::cout << line << "\n";
      }
    }

    if (redir.has_stdout_redirect()) {
      auto mode =
          redir.stdout_append_mode ? std::ios_base::app : std::ios_base::trunc;
      redirect_output(output, redir.stdout_file, mode);
    }
    if (redir.has_stderr_redirect()) {
      auto mode =
          redir.stderr_append_mode ? std::ios_base::app : std::ios_base::trunc;
      redirect_output(err_output, redir.stderr_file, mode);
    }
    return true;
  }

  // linux style command name mapping
  std::string actual_cmd = cmd_name;
  if (actual_cmd == "clear")
    actual_cmd = "cls";
  else if (actual_cmd == "ls")
    actual_cmd = "dir";
  else if (actual_cmd == "rm")
    actual_cmd = "del";

  std::string win_cmd = actual_cmd;
  for (const auto &arg : args) {
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

  std::vector<std::string> argv_strs;
  argv_strs.reserve(args.size() + 1);
  argv_strs.push_back(cmd_name);
  for (auto &arg : args)
    argv_strs.push_back(std::move(arg));

  std::vector<char *> argv;
  argv.reserve(argv_strs.size() + 1);
  for (auto &s : argv_strs)
    argv.push_back(s.data());
  argv.push_back(nullptr);

  pid_t pid = fork();
  if (pid < 0) {
    return false;
  } else if (pid == 0) {
    if (redir.has_stdout_redirect()) {
      int flags =
          O_WRONLY | O_CREAT | (redir.stdout_append_mode ? O_APPEND : O_TRUNC);
      FileDescriptor fd(redir.stdout_file, flags, 0644);
      fd.apply_redirect(STDOUT_FILENO);
    }
    if (redir.has_stderr_redirect()) {
      int flags =
          O_WRONLY | O_CREAT | (redir.stderr_append_mode ? O_APPEND : O_TRUNC);
      FileDescriptor fd(redir.stderr_file, flags, 0644);
      fd.apply_redirect(STDERR_FILENO);
    }

    execvp(exec_path.c_str(), argv.data());
    std::exit(1); // -> execvp fails
  } else {
    if (run_in_background) {
      static int next_job_id = 1;
      int job_id = next_job_id++;
      std::cout << std::format("[{}] {}\n", job_id, pid);
    } else {
      int status;
      waitpid(pid, &status, 0);
    }
  }
#endif
  return true;
}
