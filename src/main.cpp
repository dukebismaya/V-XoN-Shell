#include "extensions/builtins.h"
#include "extensions/parser.h"

#include "extensions/completion.h"

static std::string make_prompt() {
  return std::format("┌──({}@V-Xon)-[{}]\n└─$ ", USER_NAME,
                     fs::current_path().string());
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  RawMode raw;

  while (true) {
    std::string raw_command;

    auto input = readline_raw(make_prompt(), raw);
    if (!input.has_value()) {
      raw.restore();
      break;
    }
    raw_command = std::move(*input);

    auto args = parse_args(raw_command);
    if (args.empty())
      continue;

    std::string cmd = std::move(args[0]);
    args.erase(args.begin());

    if (cmd == "exit") {
      raw.restore();
      std::exit(0);
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

    if (!run_program(cmd, args))
      std::cout << std::format("{}: command not found\n", cmd);
  }

  return 0;
}
