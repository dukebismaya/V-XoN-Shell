#include "extensions/builtins.h"
#include "extensions/parser.h"

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string raw_command;

  while (true) {
    std::cout << std::format("┌──({}@V-Xon)-", USER_NAME) << "["
              << fs::current_path().string() << "]\n└─$ ";

    if (!std::getline(std::cin, raw_command))
      break;

    auto args = parse_args(raw_command);
    if (args.empty())
      continue;

    std::string cmd = std::move(args[0]);
    args.erase(args.begin());

    if (cmd == "exit")
      std::exit(0);
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
}
