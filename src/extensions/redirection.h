#include <exception>
#include <fstream>
#include <iostream>
#include <string>

inline auto redirect_output(const std::string &output, const std::string &filepath,
                            std::ios_base::openmode mode = std::ios_base::trunc) -> void {
  std::ofstream op_file{filepath, mode};
  
  if (!op_file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << "\n";
    return;
  }

  try {
    op_file << output;
  } catch (const std::exception& e) {
    std::cerr << "Error writing to file: " << e.what() << "\n";
  }
}