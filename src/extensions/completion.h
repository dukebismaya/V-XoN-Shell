#pragma once

#include "executor.h"
#include "platform.h"

#include <optional>

#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>

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
      auto matches = find_completions(buf);

      if (matches.empty()) {
        std::cout << '\a' << std::flush;

      } else if (matches.size() == 1) {
        // unique match — match + " ";
        const std::string &completed = matches[0];
        std::string suffix = completed.substr(buf.size()) + " ";
        std::cout << suffix << std::flush;
        buf = completed + " ";
        tab_pressed = false;

      } else {
        // multiple matches — find longest common prefix
        std::string lcp = matches[0];
        for (const auto &m : matches)
          while (!m.starts_with(lcp))
            lcp.pop_back();

        if (lcp.size() > buf.size()) {
          // Can extend to LCP without ambiguity
          std::string suffix = lcp.substr(buf.size());
          std::cout << suffix << std::flush;
          buf = lcp;
          tab_pressed = false;
        } else {
          if (!tab_pressed) {
            // First time at this LCP — ring bell
            std::cout << '\a' << std::flush;
            tab_pressed = true;
          } else {
            // (second TAB: print list)
            std::cout << "\n";
            for (const auto &m : matches)
              std::cout << m << "  ";
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
