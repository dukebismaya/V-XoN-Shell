#pragma once

#include "executor.h"
#include "platform.h"
#include <optional>

#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#include <sec_api/stdlib_s.h>

class RawMode {
public:
  RawMode() {}
  void restore() {}
  RawMode(const RawMode &) = delete;
  RawMode &operator=(const RawMode &) = delete;
  ~RawMode() { restore(); }
};

#else
#include <termios.h>

class RawMode {
  struct ::termios saved_;
  bool active_{false};

public:
  RawMode() {
    if (tcgetattr(STDIN_FILENO, &saved_) == 0) {
      struct ::termios raw = saved_;
      raw.c_lflag &= ~(ECHO | ICANON);
      raw.c_cc[VMIN] = 1;
      raw.c_cc[VTIME] = 0;
      if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
        active_ = true;
      }
    }
  }

  void restore() {
    if (active_) {
      if (tcsetattr(STDIN_FILENO, TCSANOW, &saved_) == 0) {
        active_ = false;
      }
    }
  }

  RawMode(const RawMode &) = delete;
  RawMode &operator=(const RawMode &) = delete;

  ~RawMode() { restore(); }
};
#endif
inline auto find_completions(const std::string &prefix)
    -> std::vector<std::string> {
  std::vector<std::string> matches;
  std::unordered_set<std::string> seen;

  for (const auto &builtin : SHELL_BUILTINS) {
    if (builtin.starts_with(prefix)) {
      matches.push_back(builtin);
      seen.insert(builtin);
    }
  }

  const auto *path_env = std::getenv("PATH");
  if (path_env) {
    std::stringstream ss(path_env);
    std::string dir;

    while (std::getline(ss, dir, PATH_DELIMITER)) {
      std::error_code ec;
      for (const auto &entry : fs::directory_iterator(
               dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec)
          continue;
        const auto &name = entry.path().filename().string();
        if (name.starts_with(prefix) && !seen.contains(name)) {
#if defined(_WIN32) || defined(_WIN64)
          std::string lower_name = name;
          std::transform(lower_name.begin(), lower_name.end(),
                         lower_name.begin(), ::tolower);
          bool is_exec =
              lower_name.ends_with(".exe") || lower_name.ends_with(".bat") ||
              lower_name.ends_with(".cmd") || lower_name.ends_with(".com");
#else
          auto perms = entry.status(ec).permissions();
          bool is_exec =
              !ec && (perms & (fs::perms::owner_exec | fs::perms::group_exec |
                               fs::perms::others_exec)) != fs::perms::none;
#endif
          if (is_exec) {
            matches.push_back(name);
            seen.insert(name);
          }
        }
      }
    }
  }

  std::sort(matches.begin(), matches.end());
  return matches;
}

inline auto find_file_completions(const std::string &full_prefix)
    -> std::vector<std::string> {
  std::vector<std::string> matches;
  std::error_code ec;

  std::string dir_path = "";
  std::string search_prefix = full_prefix;
  auto last_slash = full_prefix.find_last_of('/');
  if (last_slash != std::string::npos) {
    dir_path = full_prefix.substr(0, last_slash + 1);
    search_prefix = full_prefix.substr(last_slash + 1);
  }

  fs::path search_dir = fs::current_path();
  if (!dir_path.empty()) {
    search_dir /= dir_path;
  }

  if (fs::exists(search_dir, ec) && fs::is_directory(search_dir, ec)) {
    for (const auto &entry : fs::directory_iterator(
             search_dir, fs::directory_options::skip_permission_denied, ec)) {
      if (ec)
        continue;
      const auto &name = entry.path().filename().string();
      if (name.starts_with(search_prefix)) {
        matches.push_back(dir_path + name);
      }
    }
  }
  std::sort(matches.begin(), matches.end());
  return matches;
}

inline auto run_completer_script(const std::string &script_path,
                                 const std::string &cmd_name,
                                 const std::string &current_word,
                                 const std::string &prev_word,
                                 const std::string &comp_line,
                                 size_t comp_point)
    -> std::vector<std::string> {
  std::vector<std::string> results;
#if defined(_WIN32) || defined(_WIN64)
  std::string command =
      script_path +
      std::format(" \"{}\" \"{}\" \"{}\"", cmd_name, current_word, prev_word);

  _putenv_s("COMP_LINE", comp_line.c_str());
  _putenv_s("COMP_POINT", std::to_string(comp_point).c_str());

  FILE *pipe = _popen(command.c_str(), "r");

  _putenv_s("COMP_LINE", "");
  _putenv_s("COMP_POINT", "");
  if (!pipe)
    return results;
  char buffer[4096];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }
  _pclose(pipe);
#else
  int pipefd[2];
  if (pipe(pipefd) < 0)
    return results;

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return results;
  }
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    setenv("COMP_LINE", comp_line.c_str(), 1);
    setenv("COMP_POINT", std::to_string(comp_point).c_str(), 1);

    execl(script_path.c_str(), script_path.c_str(), cmd_name.c_str(),
          current_word.c_str(), prev_word.c_str(), nullptr);
    std::exit(1);
  }
  close(pipefd[1]);

  std::string output;
  char buffer[4096];
  ssize_t bytes;
  while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
    output.append(buffer, bytes);
  }
  close(pipefd[0]);

  int status;
  waitpid(pid, &status, 0);
#endif

  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      results.push_back(line);
    }
  }
  return results;
}

inline auto readline_raw(const std::string &prompt, RawMode & /*raw*/)
    -> std::optional<std::string> {
  std::cout << prompt << std::flush;

  std::string buf;
  bool tab_pressed = false;

  while (true) {
    char c{};
#if defined(_WIN32) || defined(_WIN64)
    int ch = _getch();
    if (ch == 255 || ch == EOF)
      return std::nullopt; // EOF / error
    if (ch == 0 || ch == 224) {
      _getch(); // consume extended key
      continue;
    }
    c = static_cast<char>(ch);
#else
    ssize_t n = ::read(STDIN_FILENO, &c, 1);
    if (n <= 0)
      return std::nullopt; // EOF / error
#endif

    if (c == '\t') {
      std::string prefix;
      bool is_command_completion = false;
      auto last_space = buf.find_last_of(' ');

      if (last_space == std::string::npos) {
        prefix = buf;
        is_command_completion = (prefix.find('/') == std::string::npos);
      } else {
        prefix = buf.substr(last_space + 1);
        is_command_completion = false;
      }

      std::vector<std::string> matches;
      bool used_completer = false;
      if (!is_command_completion) {
        std::vector<std::string> tokens;
        std::string current_token;
        for (char c : buf) {
          if (c == ' ') {
            if (!current_token.empty()) {
              tokens.push_back(current_token);
              current_token.clear();
            }
          } else {
            current_token += c;
          }
        }
        if (!current_token.empty()) {
          tokens.push_back(current_token);
        }

        std::string cmd_name = tokens.empty() ? buf : tokens[0];

        std::string current_word = prefix;
        std::string prev_word = "";

        if (prefix.empty()) {
          if (tokens.size() > 1) {
            prev_word = tokens.back();
          }
        } else {
          if (tokens.size() > 2) {
            prev_word = tokens[tokens.size() - 2];
          }
        }

        auto it = register_completion.find(cmd_name);
        if (it != register_completion.end()) {
          matches = run_completer_script(it->second, cmd_name, current_word,
                                         prev_word, buf, buf.length());
          std::sort(matches.begin(), matches.end());
          used_completer = true;
        }
      }

      if (!used_completer) {
        matches = is_command_completion ? find_completions(prefix)
                                        : find_file_completions(prefix);
      }

      if (matches.empty()) {
        std::cout << '\a' << std::flush;

      } else if (matches.size() == 1) {
        // unique match
        const std::string &completed = matches[0];

        bool is_dir = false;
        if (!is_command_completion && !used_completer) {
          std::error_code ec;
          is_dir = fs::is_directory(fs::current_path() / completed, ec);
        }

        std::string suffix = completed.substr(prefix.size());
        if (is_dir) {
          suffix += "/";
        } else {
          suffix += " ";
        }
        std::cout << suffix << std::flush;
        buf += suffix;
        tab_pressed = false;

      } else {
        // multiple matches — find longest common prefix
        std::string lcp = matches[0];
        for (const auto &m : matches)
          while (!m.starts_with(lcp))
            lcp.pop_back();

        if (lcp.size() > prefix.size()) {
          // Can extend to LCP without ambiguity
          std::string suffix = lcp.substr(prefix.size());
          std::cout << suffix << std::flush;
          buf += suffix;
          tab_pressed = false;
        } else {
          if (!tab_pressed) {
            // First time at this LCP — ring bell
            std::cout << '\a' << std::flush;
            tab_pressed = true;
          } else {
            // (second TAB: print list)
            std::cout << "\n";
            for (const auto &m : matches) {
              std::string display_text = m;
              if (!is_command_completion && !used_completer) {
                auto slash_idx = m.find_last_of('/');
                if (slash_idx != std::string::npos) {
                  display_text = m.substr(slash_idx + 1);
                }
              }

              bool is_dir = false;
              if (!is_command_completion) {
                std::error_code ec;
                is_dir = fs::is_directory(fs::current_path() / m, ec);
              }

              if (is_dir) {
                display_text += "/";
              }
              std::cout << display_text << "  ";
            }
            std::cout << "\n" << prompt << buf << std::flush;
            tab_pressed = false;
          }
        }
      }

    } else if (c == 127 || c == '\b') {
      // Backspace / DEL
      if (!buf.empty()) {
        buf.pop_back();
        // Erase the last character on the terminal
        std::cout << "\b \b" << std::flush;
      }
      tab_pressed = false;

    } else if (c == '\r' || c == '\n') {
      // Enter: accept line
      std::cout << '\n' << std::flush;
      return buf;

    } else if (c == 4) {
      // Ctrl-D on empty input: EOF
      if (buf.empty()) {
        std::cout << '\n' << std::flush;
        return std::nullopt;
      }
      // Ctrl-D mid-line: ignore

    } else if (c >= 32) {
      // Printable character
      buf += c;
      std::cout << c << std::flush;
      tab_pressed = false;
    }
    // Everything else (arrow keys send escape sequences, etc.) is ignored
  }
}
