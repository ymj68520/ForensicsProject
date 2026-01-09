#pragma once

#include <string>
#include <vector>
#include <memory>
#include <xapian.h>

namespace forensics {

struct SearchResult {
    std::string path;
    double score;
    std::string snippet;
};

class XapianIndexer {
public:
    explicit XapianIndexer(const std::string& dbPath);
    ~XapianIndexer();

    void addDocument(const std::string& filePath, const std::string& content);
    void commit();

private:
    std::string dbPath_;
    std::unique_ptr<Xapian::WritableDatabase> db_;
    Xapian::TermGenerator termGenerator_;
};

class XapianSearcher {
public:
    explicit XapianSearcher(const std::string& dbPath);
    ~XapianSearcher();

    std::vector<SearchResult> search(const std::string& queryStr, size_t limit = 10, size_t offset = 0);

private:
    std::string dbPath_;
    std::unique_ptr<Xapian::Database> db_;
};

}
