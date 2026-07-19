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

    // Redirection operator: > or >> with optional fd prefix (1>/2>/1>>/2>>)
    else if (c == '>') {
      bool is_append =
          (i + 1 < raw_command.length() && raw_command[i + 1] == '>');

      if (!curr_arg.empty() &&
          (curr_arg.back() == '1' || curr_arg.back() == '2')) {
        char fd_char = curr_arg.back();
        curr_arg.pop_back();
        if (!curr_arg.empty()) {
          args.push_back(std::move(curr_arg));
          curr_arg.clear();
        }
        args.push_back(std::string(1, fd_char) + (is_append ? ">>" : ">"));
      } else {
        // Bare > or >> defaults to stdout redirect (1> / 1>>)
        if (has_chars) {
          args.push_back(std::move(curr_arg));
          curr_arg.clear();
        }
        args.push_back(is_append ? "1>>" : "1>");
      }

      if (is_append)
        i++;
      has_chars = false;
    }

    // Pipe operator: split into separate "|" token
    else if (c == '|') {
      if (has_chars) {
        args.push_back(std::move(curr_arg));
        curr_arg.clear();
        has_chars = false;
      }
      args.push_back("|");
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
