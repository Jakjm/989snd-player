#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace file_util {
inline std::vector<uint8_t> read_binary_file(const fs::path& path) {
  std::error_code ec;
  const auto status = fs::status(path, ec);
  if (ec || !fs::exists(status)) {
    throw std::runtime_error("File " + path.string() + " cannot be opened: does not exist.");
  }
  if (status.type() != fs::file_type::regular && status.type() != fs::file_type::symlink) {
    throw std::runtime_error("File " + path.string() +
                             " cannot be opened: not a regular file or symlink.");
  }

  FILE* fp = std::fopen(path.string().c_str(), "rb");
  if (!fp) {
    throw std::runtime_error("File " + path.string() +
                             " cannot be opened: " + std::string(std::strerror(errno)));
  }

  std::fseek(fp, 0, SEEK_END);
  const long len = std::ftell(fp);
  if (len <= 0) {
    std::fclose(fp);
    return {};
  }
  std::rewind(fp);

  std::vector<uint8_t> data(static_cast<size_t>(len));
  if (std::fread(data.data(), static_cast<size_t>(len), 1, fp) != 1) {
    std::fclose(fp);
    throw std::runtime_error("File " + path.string() + " cannot be read");
  }
  std::fclose(fp);
  return data;
}

inline std::vector<uint8_t> read_binary_file(const std::string& filename) {
  return read_binary_file(fs::path(filename));
}

}  // namespace file_util
