#pragma once

#include "platform.h"
#include <exception>

struct RedirectInfo {
  std::string stdout_file; // target for 1> / 1>>
  std::string stderr_file; // target for 2> / 2>>

  bool has_stdout_redirect() const { return !stdout_file.empty(); }
  bool has_stderr_redirect() const { return !stderr_file.empty(); }
  bool stdout_append_mode = false;
  bool stderr_append_mode = false;
};

#if !defined(_WIN32) && !defined(_WIN64)
class FileDescriptor {
  int fd_ = -1;

public:
  explicit FileDescriptor(const std::string &path, int flags, mode_t mode)
      : fd_(open(path.c_str(), flags, mode)) {}

  ~FileDescriptor() {
    if (fd_ >= 0)
      close(fd_);
  }

  FileDescriptor(const FileDescriptor &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;

  int get() const { return fd_; }

  void apply_redirect(int target_fd) {
    if (fd_ >= 0) {
      dup2(fd_, target_fd);
      close(fd_);
      fd_ = -1;
    }
  }
};
#endif

inline auto redirect_output(const std::string &output,
                            const std::string &filepath,
                            std::ios_base::openmode mode = std::ios_base::trunc)
    -> void {

  auto parent = fs::path(filepath).parent_path();
  if (!parent.empty() && !fs::exists(parent))
    fs::create_directories(parent);

  std::ofstream op_file{filepath, mode};
  if (!op_file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << "\n";
    return;
  }

  try {
    op_file << output;
  } catch (const std::exception &e) {
    std::cerr << "Error writing to file: " << e.what() << "\n";
  }
}

inline auto extract_redirection(std::vector<std::string> &args)
    -> RedirectInfo {
  RedirectInfo info;
  auto write_it = args.begin();

  for (auto it = args.begin(); it != args.end(); ++it) {
    if ((*it == "1>" || *it == "1>>") && std::next(it) != args.end()) {
      if (*it == "1>>") {
        info.stdout_append_mode = true;
      }
      info.stdout_file = *++it;
    } else if ((*it == "2>" || *it == "2>>") && std::next(it) != args.end()) {
      if (*it == "2>>") {
        info.stderr_append_mode = true;
      }
      info.stderr_file = *++it;
    } else {
      if (write_it != it)
        *write_it = std::move(*it);
      ++write_it;
    }
  }
  args.erase(write_it, args.end());
  return info;
}