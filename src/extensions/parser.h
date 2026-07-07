#pragma once

#include "platform.h"
#include <unordered_set>

// escapable characters
inline const std::unordered_set<char> ESCAPABLE_CHARS{'"', '\\', '\n', '$',
                                                      '`'};
inline auto parse_args(const std::string &raw_command)
    -> std::vector<std::string> {
  std::vector<std::string> args;
  std::string curr_arg;
  bool in_single_quote = false;
  bool in_double_quote = false;
  bool has_chars = false;

  for (size_t i = 0; i < raw_command.length(); ++i) {
    char c = raw_command[i];

    if (c == '\\') {
      if (in_double_quote) {
        if (i + 1 < raw_command.length() &&
            ESCAPABLE_CHARS.contains(raw_command[i + 1])) {
          curr_arg += raw_command[++i];
        } else {
          curr_arg += c;
        }
      } else if (in_single_quote) {
        // Inside single quotes: backslash is literal
        curr_arg += c;
      } else if (i + 1 < raw_command.length()) {
        // Outside quotes: escape next character
        curr_arg += raw_command[++i];
      }
      has_chars = true;
    }

    else if (c == '"' && !in_single_quote) {
      in_double_quote = !in_double_quote;
      has_chars = true;
    }

    else if (c == '\'' && !in_double_quote) {
      in_single_quote = !in_single_quote;
      has_chars = true;
    }

    // Inside any quotes: all chars are literal
    else if (in_single_quote || in_double_quote) {
      curr_arg += c;
      has_chars = true;
    }

    // Whitespace: flush current argument
    else if (c == ' ' || c == '\t' || c == '\n') {
      if (has_chars) {
        args.push_back(std::move(curr_arg));
        curr_arg.clear();
        has_chars = false;
      }
    }

    // Redirection operator: > with optional fd prefix (1> or 2>)
    else if (c == '>') {
      if (!curr_arg.empty() &&
          (curr_arg.back() == '1' || curr_arg.back() == '2')) {
        char fd_char = curr_arg.back();
        curr_arg.pop_back();
        if (!curr_arg.empty()) {
          args.push_back(std::move(curr_arg));
          curr_arg.clear();
        }
        if (raw_command[i + 1] == '>' && i + 2 < raw_command.length()) {
          args.push_back(std::string(1, fd_char) + ">>");
          i++;
        } else {
          args.push_back(std::string(1, fd_char) + ">");
        }
      } else {
        // Bare > defaults to stdout redirect (1>)
        if (has_chars) {
          args.push_back(std::move(curr_arg));
          curr_arg.clear();
        }
        if (raw_command[i + 1] == '>' && i + 2 < raw_command.length()) {
          args.push_back("1>>");
        } else {
          args.push_back("1>");
        }
      }
      has_chars = false;
    }

    else {
      curr_arg += c;
      has_chars = true;
    }
  }

  if (has_chars)
    args.push_back(std::move(curr_arg));

  return args;
}
