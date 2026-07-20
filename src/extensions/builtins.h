#pragma once

#include "executor.h"
#include "platform.h"
#include "redirection.h"
#include <ios>

inline auto handle_echo(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);

  std::string output;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0)
      output += " ";
    output += args[i];
  }

  if (redir.has_stdout_redirect()) {
    if (redir.stdout_append_mode) {
      redirect_output(output + "\n", redir.stdout_file, std::ios_base::app);
    } else {
      redirect_output(output + "\n", redir.stdout_file);
    }
  } else
    std::cout << output << "\n";

  // echo produces no stderr, but create/truncate the file if 2>/2>> is
  // specified
  if (redir.has_stderr_redirect()) {
    if (redir.stderr_append_mode) {
      redirect_output("", redir.stderr_file, std::ios_base::app);
    } else {
      redirect_output("", redir.stderr_file);
    }
  }
}

inline auto handle_type(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);

  if (args.empty()) {
    std::cout << "type: missing argument\n";
    return;
  }

  const auto &cmd = args[0];
  std::string output;

  if (SHELL_BUILTINS.contains(cmd)) {
    output = std::format("{} is a shell builtin", cmd);
  } else {
    std::string exec_path = find_executable(cmd);
    if (!exec_path.empty())
      output = std::format("{} is {}", cmd, exec_path);
    else
      output = std::format("{}: not found", cmd);
  }

  if (redir.has_stdout_redirect()) {
    if (redir.stdout_append_mode) {
      redirect_output(output + "\n", redir.stdout_file, std::ios_base::app);
    } else {
      redirect_output(output + "\n", redir.stdout_file);
    }
  } else
    std::cout << output << "\n";

  if (redir.has_stderr_redirect()) {
    if (redir.stderr_append_mode) {
      redirect_output("", redir.stderr_file, std::ios_base::app);
    } else {
      redirect_output("", redir.stderr_file);
    }
  }
}

inline auto handle_cd(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);

  if (args.empty()) {
    if (HOME_DIR && std::filesystem::exists(HOME_DIR))
      std::filesystem::current_path(HOME_DIR);
    return;
  }

  std::string target = args[0];

  if (auto pos = target.find('~'); pos != std::string::npos && HOME_DIR)
    target.replace(pos, 1, HOME_DIR);

  if (std::filesystem::exists(target) &&
      std::filesystem::is_directory(target)) {
    std::filesystem::current_path(target);
  } else {
    std::cout << std::format("cd: {}: No such file or directory\n", target);
  }
}

inline auto handle_pwd(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);
  std::string output = std::filesystem::current_path().string() + "\n";

  if (redir.has_stdout_redirect()) {
    if (redir.stdout_append_mode)
      redirect_output(output, redir.stdout_file, std::ios_base::app);
    else
      redirect_output(output, redir.stdout_file);
  } else
    std::cout << output;

  if (redir.has_stderr_redirect()) {
    if (redir.stderr_append_mode)
      redirect_output("", redir.stderr_file, std::ios_base::app);
    else
      redirect_output("", redir.stderr_file);
  }
}

inline auto handle_complete(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);
  if (args.empty()) {
    std::cout << "complete: missing argument\n";
    return;
  }
  std::string output;
  // -p;
  auto find_p_arg = std::find(args.begin(), args.end(), "-p");
  if (find_p_arg != args.end() && std::next(find_p_arg) != args.end()) {
    if (register_completion.count(*std::next(find_p_arg))) {
      output = "complete -C '" + register_completion[*std::next(find_p_arg)] +
               "' " + *std::next(find_p_arg);
    } else {
      output = "complete: " + *std::next(find_p_arg) +
               ": no completion specification";
    }
  }
  // -C;
  auto find_C_arg = std::find(args.begin(), args.end(), "-C");
  if (find_C_arg != args.end() && std::next(find_C_arg) != args.end() &&
      std::next(std::next(find_C_arg)) != args.end()) {
    register_completion[*std::next(std::next(find_C_arg))] =
        *std::next(find_C_arg);
  }
  // -r;
  auto find_r_arg = std::find(args.begin(), args.end(), "-r");
  if (find_r_arg != args.end() && std::next(find_r_arg) != args.end()) {
    register_completion.erase(*std::next(find_r_arg));
  }

  if (redir.has_stdout_redirect()) {
    std::string to_write = output.empty() ? "" : (output + "\n");
    if (redir.stdout_append_mode) {
      redirect_output(to_write, redir.stdout_file, std::ios_base::app);
    } else {
      redirect_output(to_write, redir.stdout_file);
    }
  } else {
    if (!output.empty()) {
      std::cout << output << "\n";
    }
  }

  if (redir.has_stderr_redirect()) {
    if (redir.stderr_append_mode) {
      redirect_output("", redir.stderr_file, std::ios_base::app);
    } else {
      redirect_output("", redir.stderr_file);
    }
  }
}

inline auto format_and_reap_jobs(bool print_all) -> std::string {
  std::string output;

#if !defined(_WIN32) && !defined(_WIN64)
  for (auto &job : background_jobs) {
    if (job.status == "Running") {
      int status;
      pid_t result = waitpid(job.pid, &status, WNOHANG);
      if (result == job.pid) {
        if (WIFEXITED(status)) {
          job.status = "Done";
          if (job.command.length() >= 2 &&
              job.command.substr(job.command.length() - 2) == " &") {
            job.command = job.command.substr(0, job.command.length() - 2);
          }
        }
      }
    }
  }
#endif

  for (size_t i = 0; i < background_jobs.size(); ++i) {
    const auto &job = background_jobs[i];
    if (!print_all && job.status != "Done")
      continue;

    std::string status_padded = job.status;
    if (status_padded.length() < 24) {
      status_padded.append(24 - status_padded.length(), ' ');
    }
    char marker = ' ';
    if (i == background_jobs.size() - 1) {
      marker = '+';
    } else if (background_jobs.size() >= 2 && i == background_jobs.size() - 2) {
      marker = '-';
    }
    output += std::format("[{}]{}  {}{}\n", job.id, marker, status_padded,
                          job.command);
  }

  background_jobs.erase(
      std::remove_if(background_jobs.begin(), background_jobs.end(),
                     [](const BackgroundJob &j) { return j.status == "Done"; }),
      background_jobs.end());

  return output;
}

inline auto handle_background_jobs(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);
  std::string output = format_and_reap_jobs(true);

  if (redir.has_stdout_redirect()) {
    if (redir.stdout_append_mode)
      redirect_output(output, redir.stdout_file, std::ios_base::app);
    else
      redirect_output(output, redir.stdout_file);
  } else
    std::cout << output;

  if (redir.has_stderr_redirect()) {
    if (redir.stderr_append_mode)
      redirect_output("", redir.stderr_file, std::ios_base::app);
    else
      redirect_output("", redir.stderr_file);
  }
}

inline auto handle_history(std::vector<std::string> &args) -> void {
  auto redir = extract_redirection(args);

  std::string output;
  for (size_t i = 0; i < command_history.size(); ++i) {
    output += std::format("{:>5}  {}\n", i + 1, command_history[i]);
  }

  if (redir.has_stdout_redirect()) {
    if (redir.stdout_append_mode)
      redirect_output(output, redir.stdout_file, std::ios_base::app);
    else
      redirect_output(output, redir.stdout_file);
  } else
    std::cout << output;

  if (redir.has_stderr_redirect()) {
    if (redir.stderr_append_mode)
      redirect_output("", redir.stderr_file, std::ios_base::app);
    else
      redirect_output("", redir.stderr_file);
  }
}

inline void exec_builtin_for_pipeline(const std::string &cmd,
                                      std::vector<std::string> &args) {

  if (cmd == "echo")
    handle_echo(args);
  else if (cmd == "type")
    handle_type(args);
  else if (cmd == "pwd")
    handle_pwd(args);
  else if (cmd == "cd")
    handle_cd(args);
  else if (cmd == "history")
    handle_history(args);
}
