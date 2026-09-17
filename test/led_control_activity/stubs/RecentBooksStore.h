#pragma once

#include <string>
#include <vector>

struct RecentBook {
  std::string path;
  std::string title;
  std::string author;
  std::string coverBmpPath;
};

struct TestRecentBooksStore {
  std::vector<RecentBook> books;
  const std::vector<RecentBook>& getBooks() const { return books; }
  void updateBook(const std::string&, const std::string&, const std::string&, const std::string&) {}
};
struct RecentBooksStore {
  static bool isMissing(const RecentBook&) { return false; }
};
inline TestRecentBooksStore testRecentBooks;
#define RECENT_BOOKS testRecentBooks
