#include "extensions/builtins.h"
#include "extensions/parser.h"

#include "extensions/completion.h"
#include "extensions/platform.h"
#include <fstream>
#include <string>

static std::string make_prompt() {
  auto cwd = std::filesystem::current_path().string();
  return std::format("┌──({}@V-Xon)-[{}]\n└─$ ", USER_NAME,
                     cwd == HOME_DIR ? "~" : cwd);
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  RawMode raw;

  if (HISTORY_FILE) {
    std::ifstream hist_file{HISTORY_FILE};
    if (hist_file.is_open()) {
      std::string line{};
      while (std::getline(hist_file, line)) {
        if (!line.empty()) {
          command_history.push_back(line);
        }
      }
      track_history_append_index = command_history.size();
    }
  }

  while (true) {
    std::string raw_command;

    std::string auto_reap = format_and_reap_jobs(false);
    if (!auto_reap.empty()) {
      std::cout << auto_reap;
    }

    auto input = readline_raw(make_prompt(), raw);
    if (!input.has_value()) {
      raw.restore();
      break;
    }
    raw_command = std::move(*input);

    auto args = parse_args(raw_command);
    if (args.empty())
      continue;

    command_history.push_back(raw_command);

    bool has_pipe = false;
    for (const auto &a : args) {
      if (a == "|") {
        has_pipe = true;
        break;
      }
    }
    if (has_pipe) {
      run_pipeline(args);
      continue;
    }

    bool run_in_background = false;
    if (args.back() == "&") {
      run_in_background = true;
      args.pop_back();
    }

    if (args.empty())
      continue;

    std::string cmd = std::move(args[0]);
    args.erase(args.begin());

    if (cmd == "exit") {
      raw.restore();
      break;
    }
    if (cmd == "echo") {
      handle_echo(args);
      continue;
    }
    if (cmd == "type") {
      handle_type(args);
      continue;
    }
    if (cmd == "cd") {
      handle_cd(args);
      continue;
    }
    if (cmd == "pwd") {
      handle_pwd(args);
      continue;
    }
    if (cmd == "complete") {
      handle_complete(args);
      continue;
    }
    if (cmd == "jobs") {
      handle_background_jobs(args);
      continue;
    }
    if (cmd == "history") {
      handle_history(args);
      continue;
    }

    if (!run_program(cmd, args, run_in_background))
      std::cout << std::format("{}: command not found\n", cmd);
  }

  if (HISTORY_FILE) {
    std::ofstream out_hist_file{HISTORY_FILE};
    if (out_hist_file.is_open()) {
      for (const auto &line : command_history) {
        out_hist_file << line << "\n";
      }
    }
  }

  return 0;
}
