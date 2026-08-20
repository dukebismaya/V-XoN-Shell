<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++23" />
  <img src="https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-0078D4?style=for-the-badge&logo=windows-terminal&logoColor=white" alt="Platform" />
  <img src="https://img.shields.io/badge/License-MIT-10B981?style=for-the-badge" alt="License" />
  <img src="https://img.shields.io/github/last-commit/dukebismaya/V-XoN-Shell?style=for-the-badge&color=8B5CF6" alt="Last Commit" />
</p>

<br/>

<h1 align="center">
  <code>V-XoN Shell</code>
</h1>

<p align="center">
  <strong>A lightweight, cross-platform Unix-style shell built from scratch in modern C++23.</strong>
</p>

<p align="center">
  <sub>No dependencies. No frameworks. Pure standard library.</sub>
</p>

<br/>

---

<br/>

## Overview

**V-XoN Shell** is a custom command-line shell that implements core Unix shell semantics — command parsing, I/O redirection, pipelines, tab completion, background job control, shell variables, and persistent history — entirely in C++23 with zero external dependencies.

Built as a cross-platform binary targeting both **Linux** and **Windows**, it maps familiar Unix commands (`ls`, `cat`, `clear`, `rm`) to their native Windows equivalents when running on Windows, while providing a fully native POSIX experience on Linux.

<br/>

## Features

| Feature | Description |
|:--|:--|
| **Built-in Commands** | `cd`, `pwd`, `echo`, `type`, `exit`, `declare`, `complete`, `jobs`, `history` |
| **External Program Execution** | `PATH`-based lookup and `fork/exec` on Linux, `system()` bridge on Windows |
| **I/O Redirection** | `>`, `>>`, `2>`, `2>>` — stdout and stderr, truncate and append modes |
| **Pipelines** | Multi-stage `cmd1 \| cmd2 \| cmd3` with recursive pipe chaining |
| **Tab Completion** | Context-aware: commands via `PATH` scan, files via directory walk, or custom completer scripts via `complete -C` |
| **Command History** | Arrow-key navigation, `HISTFILE` persistence, `history -r/-w/-a` for read/write/append |
| **Background Jobs** | `command &` with `jobs` listing, automatic reaping, `+`/`-` markers |
| **Shell Variables** | `declare name=value`, `$var` / `${var}` expansion in arguments |
| **Quoting & Escaping** | Single quotes (literal), double quotes (with `\\`, `$`, `` ` `` escaping), bare backslash escaping |
| **Raw Mode Line Editing** | Custom `readline` with cursor movement, insert-at-position, backspace handling — no `libreadline` dependency |
| **Cross-Platform** | Unified codebase with `#if` platform splits — `termios` raw mode on Linux, `_getch()` on Windows; Linux command aliases on Windows (`ls` → `dir`, `clear` → `cls`, `rm` → `del`) |

<br/>

## Architecture

```
src/
├── main.cpp                    # Shell REPL — prompt, dispatch, history persistence
└── extensions/
    ├── platform.h              # Platform detection, environment, global state
    ├── parser.h                # Tokenizer — quoting, escaping, variable expansion, redirection & pipe operators
    ├── executor.h              # fork/exec, PATH resolution, pipeline orchestration, background jobs
    ├── builtins.h              # Built-in command handlers with redirection support
    ├── completion.h            # Tab completion engine, raw mode terminal, custom readline
    └── redirection.h           # Redirection extraction, file descriptor management, output routing
```

<br/>

## Build

**Requirements:** A C++23-compliant compiler (`g++ 13+`, `clang++ 17+`, or `MSVC 19.38+`).

```bash
# Linux / macOS
g++ -std=c++23 -O2 -o vxon src/main.cpp

# Windows (MSVC Developer Command Prompt)
cl /std:c++latest /EHsc /Fe:vxon.exe src\main.cpp
```

<br/>

## Usage

```bash
# Launch the shell
./vxon
```

```
┌──(user@V-Xon)-[~]
└─$ echo hello world
hello world

┌──(user@V-Xon)-[~]
└─$ ls | grep src
src

┌──(user@V-Xon)-[~]
└─$ echo output > file.txt

┌──(user@V-Xon)-[~]
└─$ sleep 10 &
[1] 42069

┌──(user@V-Xon)-[~]
└─$ jobs
[1]+  Running                 sleep 10

┌──(user@V-Xon)-[~]
└─$ declare greeting=hello
┌──(user@V-Xon)-[~]
└─$ echo $greeting
hello

┌──(user@V-Xon)-[~]
└─$ exit
```

<br/>

## License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for details.

<br/>

---

<br/>

<p align="center">
  <strong>Developed by Bismaya Jyoti Dalei</strong>
</p>

<p align="center">
  <a href="https://bismaya.xyz">
    <img src="https://img.shields.io/badge/Website-bismaya.xyz-0A0A0A?style=for-the-badge&logo=googlechrome&logoColor=white" alt="Website" />
  </a>
  &nbsp;
  <a href="https://linkedin.com/in/bismaya-dalei">
    <img src="https://img.shields.io/badge/LinkedIn-bismaya--dalei-0A66C2?style=for-the-badge&logo=linkedin&logoColor=white" alt="LinkedIn" />
  </a>
  &nbsp;
  <a href="https://x.com/bismaya_dev">
    <img src="https://img.shields.io/badge/X-@bismaya__dev-000000?style=for-the-badge&logo=x&logoColor=white" alt="X" />
  </a>
  &nbsp;
  <a href="https://github.com/dukebismaya">
    <img src="https://img.shields.io/badge/GitHub-dukebismaya-181717?style=for-the-badge&logo=github&logoColor=white" alt="GitHub" />
  </a>
</p>