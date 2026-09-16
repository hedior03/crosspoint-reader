#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

constexpr int O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2, O_CREAT = 4, O_APPEND = 8, O_EXCL = 16;
namespace fake {
inline std::map<std::string, std::vector<uint8_t>> files;
inline std::set<std::string> dirs;
inline std::string fault;
inline unsigned skip = 0;
inline bool fails(const char* operation) {
  if (fault != operation) return false;
  if (skip) {
    --skip;
    return false;
  }
  fault.clear();
  return true;
}
inline void reset() {
  files.clear();
  dirs.clear();
  fault.clear();
  skip = 0;
}
}  // namespace fake
class HalFile {
 public:
  HalFile() = default;
  HalFile(std::string name, bool append) : path(std::move(name)), pos(append ? fake::files[path].size() : 0) {}
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;
  HalFile(HalFile&& other) noexcept : path(std::exchange(other.path, {})), pos(other.pos) {}
  HalFile& operator=(HalFile&& other) noexcept {
    path = std::exchange(other.path, {});
    pos = other.pos;
    return *this;
  }
  explicit operator bool() const { return !path.empty(); }
  size_t write(const uint8_t* bytes, size_t count) {
    if (fake::fails("write")) count /= 2;
    auto& data = fake::files.at(path);
    data.resize(std::max(data.size(), pos + count));
    std::memcpy(data.data() + pos, bytes, count);
    pos += count;
    return count;
  }
  bool seek(size_t offset) {
    if (fake::fails("seek")) return false;
    pos = offset;
    return true;
  }
  void flush() {}
  bool close() {
    path.clear();
    return !fake::fails("close");
  }

 private:
  std::string path;
  size_t pos = 0;
};
class HalStorage {
 public:
  bool ready() const { return true; }
  bool ensureDirectoryExists(const char* path) {
    fake::dirs.insert(path);
    return true;
  }
  bool exists(const char* path) { return fake::files.count(path) || fake::dirs.count(path); }
  bool mkdir(const char* path) { return !fake::fails("mkdir") && fake::dirs.insert(path).second; }
  HalFile open(const char* path, int flags) {
    if (fake::fails("open") || ((flags & O_EXCL) && exists(path))) return {};
    fake::files.try_emplace(path);
    return HalFile(path, flags & O_APPEND);
  }
  bool rename(const char* from, const char* to) {
    if (fake::fails("rename") || exists(to)) return false;
    auto node = fake::files.extract(from);
    if (node.empty()) return false;
    node.key() = to;
    fake::files.insert(std::move(node));
    return true;
  }
};
inline HalStorage testStorage;
#define Storage testStorage
