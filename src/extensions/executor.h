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
    auto full_path = std::filesystem::path(curr_path) / command;

    if (std::filesystem::exists(full_path)) {
      auto perms = std::filesystem::status(full_path).permissions();
      bool is_executable = (perms & (std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_exec)) !=
                           std::filesystem::perms::none;
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
      int job_id{1};
      if (!background_jobs.empty()) {
        int max_id = 0;
        for (const auto &job : background_jobs) {
          max_id = std::max(max_id, job.id);
        }
        job_id = max_id + 1;
      }
      std::cout << std::format("[{}] {}\n", job_id, pid);
      std::string full_cmd = argv_strs[0];
      for (size_t i = 1; i < argv_strs.size(); ++i) {
        full_cmd += " " + argv_strs[i];
      }
      full_cmd += " &";
      background_jobs.push_back({job_id, pid, full_cmd, "Running"});
    } else {
      int status;
      waitpid(pid, &status, 0);
    }
  }
#endif
  return true;
}

// Split parsed args at the first "|" token into two command arg lists
inline auto split_pipeline(const std::vector<std::string> &args)
    -> std::pair<std::vector<std::string>, std::vector<std::string>> {
  std::vector<std::string> left, right;
  bool found_pipe = false;
  for (const auto &tok : args) {
    if (!found_pipe && tok == "|") {
      found_pipe = true;
      continue;
    }
    if (found_pipe)
      right.push_back(tok);
    else
      left.push_back(tok);
  }
  return {left, right};
}

inline auto run_pipeline(const std::vector<std::string> &args) -> bool {
#if !defined(_WIN32) && !defined(_WIN64)
  auto [left_args, right_args] = split_pipeline(args);

  if (left_args.empty() || right_args.empty())
    return false;

  auto left_redir = extract_redirection(left_args);
  auto right_redir = extract_redirection(right_args);

  std::string left_cmd = left_args[0];
  std::string right_cmd = right_args[0];

  std::string left_exec = find_executable(left_cmd);
  std::string right_exec = find_executable(right_cmd);
  if (left_exec.empty() || right_exec.empty())
    return false;

  std::vector<char *> left_argv, right_argv;
  for (auto &s : left_args)
    left_argv.push_back(const_cast<char *>(s.c_str()));
  left_argv.push_back(nullptr);

  for (auto &s : right_args)
    right_argv.push_back(const_cast<char *>(s.c_str()));
  right_argv.push_back(nullptr);

  int pipefd[2];
  if (pipe(pipefd) < 0)
    return false;

  pid_t pid1 = fork();
  if (pid1 < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }
  if (pid1 == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    if (left_redir.has_stderr_redirect()) {
      int flags = O_WRONLY | O_CREAT |
                  (left_redir.stderr_append_mode ? O_APPEND : O_TRUNC);
      FileDescriptor fd(left_redir.stderr_file, flags, 0644);
      fd.apply_redirect(STDERR_FILENO);
    }

    execvp(left_exec.c_str(), left_argv.data());
    std::exit(1);
  }

  pid_t pid2 = fork();
  if (pid2 < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, nullptr, 0);
    return false;
  }
  if (pid2 == 0) {
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    if (right_redir.has_stdout_redirect()) {
      int flags = O_WRONLY | O_CREAT |
                  (right_redir.stdout_append_mode ? O_APPEND : O_TRUNC);
      FileDescriptor fd(right_redir.stdout_file, flags, 0644);
      fd.apply_redirect(STDOUT_FILENO);
    }
    if (right_redir.has_stderr_redirect()) {
      int flags = O_WRONLY | O_CREAT |
                  (right_redir.stderr_append_mode ? O_APPEND : O_TRUNC);
      FileDescriptor fd(right_redir.stderr_file, flags, 0644);
      fd.apply_redirect(STDERR_FILENO);
    }

    execvp(right_exec.c_str(), right_argv.data());
    std::exit(1);
  }

  close(pipefd[0]);
  close(pipefd[1]);

  int status;
  waitpid(pid1, &status, 0);
  waitpid(pid2, &status, 0);
#endif
  return true;
}
