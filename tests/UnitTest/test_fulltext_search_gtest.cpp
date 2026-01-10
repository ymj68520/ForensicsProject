// test_fulltext_search_gtest.cpp
// GTest-based unit tests for FullTextSearch module
// Tests: FileMetadata, SearchResult, XapianIndexer, XapianSearcher with proper integration

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <cstdlib>

#include "FullTextSearch/FullTextSearch.h"

using namespace forensics;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Not;
using ::testing::IsEmpty;

namespace fs = std::filesystem;

// Helper to generate unique path
static std::string uniquePath(const std::string& prefix) {
    return (fs::temp_directory_path() / (prefix + "_" + std::to_string(std::rand()) + "_" + std::to_string(std::time(nullptr)))).string();
}

// ============================================================================
// FileMetadata Structure Tests
// ============================================================================

class FileMetadataTest : public ::testing::Test {};

TEST_F(FileMetadataTest, DefaultValues) {
    FileMetadata meta;
    EXPECT_TRUE(meta.path.empty());
    EXPECT_TRUE(meta.extension.empty());
    EXPECT_EQ(meta.size, 0);
    EXPECT_EQ(meta.mtime, 0);
}

TEST_F(FileMetadataTest, PopulatedValues) {
    FileMetadata meta;
    meta.path = "/home/user/document.txt";
    meta.extension = "txt";
    meta.size = 12345;
    meta.mtime = 1704067200;
    
    EXPECT_EQ(meta.path, "/home/user/document.txt");
    EXPECT_EQ(meta.extension, "txt");
    EXPECT_EQ(meta.size, 12345);
    EXPECT_EQ(meta.mtime, 1704067200);
}

TEST_F(FileMetadataTest, ExtensionExtraction) {
    FileMetadata meta;
    meta.path = "/path/to/file.pdf";
    meta.extension = fs::path(meta.path).extension().string();
    
    EXPECT_EQ(meta.extension, ".pdf");
}

// ============================================================================
// SearchResult Structure Tests
// ============================================================================

class SearchResultTest : public ::testing::Test {};

TEST_F(SearchResultTest, DefaultValues) {
    SearchResult result{};
    EXPECT_TRUE(result.path.empty());
    EXPECT_EQ(result.score, 0.0);
    EXPECT_TRUE(result.snippet.empty());
    EXPECT_EQ(result.fileSize, 0);
}

TEST_F(SearchResultTest, PopulatedValues) {
    SearchResult result;
    result.path = "/home/user/important.pdf";
    result.score = 0.95;
    result.snippet = "...this is an important document...";
    result.fileSize = 10240;
    result.extension = ".pdf";
    
    EXPECT_EQ(result.path, "/home/user/important.pdf");
    EXPECT_THAT(result.score, Ge(0.0));
    EXPECT_FALSE(result.snippet.empty());
    EXPECT_EQ(result.extension, ".pdf");
}

TEST_F(SearchResultTest, ScoreRange) {
    SearchResult result;
    result.score = 50.0;
    
    // Scores should be 0-100 for percent
    EXPECT_GE(result.score, 0.0);
    EXPECT_LE(result.score, 100.0);
}

// ============================================================================
// ContentCacheEntry Structure Tests
// ============================================================================

class ContentCacheEntryTest : public ::testing::Test {};

TEST_F(ContentCacheEntryTest, DefaultValues) {
    ContentCacheEntry entry{};
    EXPECT_TRUE(entry.content.empty());
    EXPECT_EQ(entry.indexTime, 0);
}

TEST_F(ContentCacheEntryTest, PopulatedValues) {
    ContentCacheEntry entry;
    entry.content = "This is cached content for snippet generation";
    entry.indexTime = 1704067200;
    
    EXPECT_FALSE(entry.content.empty());
    EXPECT_GT(entry.indexTime, 0);
}

// ============================================================================
// XapianIndexer Integration Tests
// ============================================================================

class XapianIndexerTest : public ::testing::Test {
protected:
    std::string testDbPath;
    
    void SetUp() override {
        // Create a unique test database path - DO NOT pre-create the directory!
        // Xapian will create its own directory structure
        testDbPath = uniquePath("xapian_indexer_test");
        // Just ensure it doesn't exist
        fs::remove_all(testDbPath);
    }
    
    void TearDown() override {
        fs::remove_all(testDbPath);
    }
};

TEST_F(XapianIndexerTest, CreateNewIndex) {
    EXPECT_NO_THROW({
        XapianIndexer indexer(testDbPath);
        indexer.addDocument("/test/file.txt", "Test content for indexing");
        indexer.commit();
    });
    
    // Verify database files were created
    EXPECT_TRUE(fs::exists(testDbPath));
}

TEST_F(XapianIndexerTest, AddSingleDocument) {
    XapianIndexer indexer(testDbPath);
    
    indexer.addDocument("/test/document.txt", "Hello world content");
    indexer.commit();
    
    EXPECT_EQ(indexer.getDocumentCount(), 1u);
}

TEST_F(XapianIndexerTest, AddMultipleDocuments) {
    XapianIndexer indexer(testDbPath);
    
    indexer.addDocument("/test/file1.txt", "First document content");
    indexer.addDocument("/test/file2.txt", "Second document content");
    indexer.addDocument("/test/file3.txt", "Third document content");
    indexer.commit();
    
    EXPECT_EQ(indexer.getDocumentCount(), 3u);
}

TEST_F(XapianIndexerTest, AddDocumentWithMetadata) {
    XapianIndexer indexer(testDbPath);
    
    FileMetadata meta;
    meta.path = "/test/document.pdf";
    meta.extension = ".pdf";
    meta.size = 1024;
    meta.mtime = 1704067200;
    
    indexer.addDocument("/test/document.pdf", "PDF content goes here", meta);
    indexer.commit();
    
    EXPECT_EQ(indexer.getDocumentCount(), 1u);
}

TEST_F(XapianIndexerTest, SetStemmerLanguage) {
    XapianIndexer indexer(testDbPath);
    
    // Should not throw for valid languages
    EXPECT_NO_THROW(indexer.setStemmerLanguage("english"));
    EXPECT_NO_THROW(indexer.setStemmerLanguage("german"));
    EXPECT_NO_THROW(indexer.setStemmerLanguage("french"));
}

TEST_F(XapianIndexerTest, EmptyContent) {
    XapianIndexer indexer(testDbPath);
    
    // Empty content should still work
    EXPECT_NO_THROW(indexer.addDocument("/test/empty.txt", ""));
    indexer.commit();
    
    // Document should still be added
    EXPECT_EQ(indexer.getDocumentCount(), 1u);
}

TEST_F(XapianIndexerTest, UpdateExistingDocument) {
    XapianIndexer indexer(testDbPath);
    
    // Add document
    indexer.addDocument("/test/file.txt", "Original content");
    indexer.commit();
    EXPECT_EQ(indexer.getDocumentCount(), 1u);
    
    // Update same document (same path = same ID)
    indexer.addDocument("/test/file.txt", "Updated content");
    indexer.commit();
    
    // Should still be 1 document (updated, not duplicated)
    EXPECT_EQ(indexer.getDocumentCount(), 1u);
}

TEST_F(XapianIndexerTest, LargeContent) {
    XapianIndexer indexer(testDbPath);
    
    // Create large content
    std::string largeContent(100000, 'x');
    
    EXPECT_NO_THROW(indexer.addDocument("/test/large.txt", largeContent));
    indexer.commit();
    
    EXPECT_EQ(indexer.getDocumentCount(), 1u);
}

// ============================================================================
// XapianSearcher Integration Tests
// ============================================================================

class XapianSearcherTest : public ::testing::Test {
protected:
    std::string testDbPath;
    
    void SetUp() override {
        // Create a unique test database path - DO NOT pre-create!
        testDbPath = uniquePath("xapian_search_test");
        fs::remove_all(testDbPath);
        
        // Create and populate index
        XapianIndexer indexer(testDbPath);
        indexer.addDocument("/test/readme.txt", "This is a readme file with important information about the project");
        indexer.addDocument("/test/code.cpp", "C++ source code with functions and classes for programming");
        indexer.addDocument("/test/data.json", "JSON data file containing configuration settings");
        indexer.addDocument("/test/notes.txt", "Personal notes about the project and important details to remember");
        indexer.commit();
    }
    
    void TearDown() override {
        fs::remove_all(testDbPath);
    }
};

TEST_F(XapianSearcherTest, ValidDatabase) {
    XapianSearcher searcher(testDbPath);
    EXPECT_TRUE(searcher.isValid());
}

TEST_F(XapianSearcherTest, TotalDocuments) {
    XapianSearcher searcher(testDbPath);
    EXPECT_EQ(searcher.getTotalDocuments(), 4u);
}

TEST_F(XapianSearcherTest, BasicSearch) {
    XapianSearcher searcher(testDbPath);
    
    auto results = searcher.search("important", 10);
    
    // Should find at least one document with "important"
    EXPECT_GE(results.size(), 1u);
}

TEST_F(XapianSearcherTest, SearchWithLimit) {
    XapianSearcher searcher(testDbPath);
    
    auto results = searcher.search("file", 2);
    
    // Should respect the limit
    EXPECT_LE(results.size(), 2u);
}

TEST_F(XapianSearcherTest, SearchNoResults) {
    XapianSearcher searcher(testDbPath);
    
    auto results = searcher.search("xyznonexistentterm12345", 10);
    
    // Should return empty results for non-matching term
    EXPECT_EQ(results.size(), 0u);
}

TEST_F(XapianSearcherTest, SetSnippetLength) {
    XapianSearcher searcher(testDbPath);
    
    // Should not throw
    EXPECT_NO_THROW(searcher.setSnippetLength(100));
    EXPECT_NO_THROW(searcher.setSnippetLength(500));
}

TEST_F(XapianSearcherTest, ResultsHaveScores) {
    XapianSearcher searcher(testDbPath);
    
    auto results = searcher.search("code", 10);
    
    for (const auto& result : results) {
        EXPECT_GE(result.score, 0.0);
    }
}

TEST_F(XapianSearcherTest, ResultsHavePaths) {
    XapianSearcher searcher(testDbPath);
    
    auto results = searcher.search("project", 10);
    
    for (const auto& result : results) {
        EXPECT_FALSE(result.path.empty());
    }
}

TEST_F(XapianSearcherTest, NonExistentDatabase) {
    XapianSearcher searcher("/nonexistent/path/to/db_12345");
    EXPECT_FALSE(searcher.isValid());
}

TEST_F(XapianSearcherTest, SearchWithOffset) {
    XapianSearcher searcher(testDbPath);
    
    // First get all results
    auto allResults = searcher.search("the", 10, 0);
    
    if (allResults.size() >= 2) {
        // Get results with offset
        auto offsetResults = searcher.search("the", 10, 1);
        
        // Should have one less result
        EXPECT_EQ(offsetResults.size(), allResults.size() - 1);
    }
}

// ============================================================================
// Full Integration Tests (Index -> Search)
// ============================================================================

class FullTextSearchIntegrationTest : public ::testing::Test {
protected:
    std::string testDbPath;
    
    void SetUp() override {
        testDbPath = uniquePath("fts_integration");
        fs::remove_all(testDbPath);
    }
    
    void TearDown() override {
        fs::remove_all(testDbPath);
    }
};

TEST_F(FullTextSearchIntegrationTest, IndexAndSearch) {
    // Index
    {
        XapianIndexer indexer(testDbPath);
        indexer.addDocument("/evidence/email.eml", "Subject: Important Meeting\nPlease attend the meeting tomorrow at 9am.");
        indexer.addDocument("/evidence/document.docx", "This document contains sensitive financial information.");
        indexer.commit();
    }
    
    // Search
    {
        XapianSearcher searcher(testDbPath);
        EXPECT_TRUE(searcher.isValid());
        EXPECT_EQ(searcher.getTotalDocuments(), 2u);
        
        auto meetingResults = searcher.search("meeting", 10);
        EXPECT_GE(meetingResults.size(), 1u);
        
        auto sensitiveResults = searcher.search("sensitive", 10);
        EXPECT_GE(sensitiveResults.size(), 1u);
    }
}

TEST_F(FullTextSearchIntegrationTest, IndexWithMetadataAndSearch) {
    // Index with metadata
    {
        XapianIndexer indexer(testDbPath);
        
        FileMetadata meta1;
        meta1.path = "/evidence/report.pdf";
        meta1.extension = ".pdf";
        meta1.size = 102400;
        
        FileMetadata meta2;
        meta2.path = "/evidence/logs.txt";
        meta2.extension = ".txt";
        meta2.size = 50000;
        
        indexer.addDocument(meta1.path, "Forensic analysis report for case #12345 with detailed findings", meta1);
        indexer.addDocument(meta2.path, "System log entries from the target machine showing suspicious activity", meta2);
        indexer.commit();
    }
    
    // Search
    {
        XapianSearcher searcher(testDbPath);
        EXPECT_EQ(searcher.getTotalDocuments(), 2u);
        
        auto results = searcher.search("forensic", 10);
        EXPECT_GE(results.size(), 1u);
        
        if (!results.empty()) {
            EXPECT_FALSE(results[0].path.empty());
        }
    }
}

TEST_F(FullTextSearchIntegrationTest, MultiTermSearch) {
    {
        XapianIndexer indexer(testDbPath);
        indexer.addDocument("/doc1.txt", "The quick brown fox jumps over the lazy dog");
        indexer.addDocument("/doc2.txt", "A brown cat sleeps on the mat");
        indexer.addDocument("/doc3.txt", "The fox is red and very clever");
        indexer.commit();
    }
    
    {
        XapianSearcher searcher(testDbPath);
        
        // Search for "brown fox" should find doc1
        auto results = searcher.search("brown fox", 10);
        EXPECT_GE(results.size(), 1u);
        
        // Search for "cat" should find doc2
        auto catResults = searcher.search("cat", 10);
        EXPECT_EQ(catResults.size(), 1u);
    }
}

TEST_F(FullTextSearchIntegrationTest, CaseSensitivity) {
    {
        XapianIndexer indexer(testDbPath);
        indexer.addDocument("/test.txt", "UPPERCASE and lowercase text");
        indexer.commit();
    }
    
    {
        XapianSearcher searcher(testDbPath);
        
        // Search should be case-insensitive
        auto upper = searcher.search("UPPERCASE", 10);
        auto lower = searcher.search("uppercase", 10);
        
        EXPECT_EQ(upper.size(), lower.size());
        EXPECT_GE(upper.size(), 1u);
    }
}

TEST_F(FullTextSearchIntegrationTest, StemmingTest) {
    {
        XapianIndexer indexer(testDbPath);
        indexer.addDocument("/test.txt", "running runners run");
        indexer.commit();
    }
    
    {
        XapianSearcher searcher(testDbPath);
        
        // Stemming should match different forms
        auto results = searcher.search("run", 10);
        EXPECT_GE(results.size(), 1u);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));  // Seed for unique temp paths
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
