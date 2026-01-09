#include "FullTextSearch.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace forensics {

// --- XapianIndexer ---

XapianIndexer::XapianIndexer(const std::string& dbPath) : dbPath_(dbPath) {
    try {
        // Create or open database for writing
        db_ = std::make_unique<Xapian::WritableDatabase>(dbPath, Xapian::DB_CREATE_OR_OPEN);
        termGenerator_.set_stemmer(Xapian::Stem("english"));
    } catch (const Xapian::Error& e) {
        std::cerr << "Xapian Indexer Error: " << e.get_msg() << std::endl;
        throw;
    }
}

XapianIndexer::~XapianIndexer() {
    try {
        if (db_) {
            db_->commit();
        }
    } catch (const Xapian::Error& e) {
        std::cerr << "Xapian Indexer Destructor Error: " << e.get_msg() << std::endl;
    }
}

void XapianIndexer::addDocument(const std::string& filePath, const std::string& content) {
    if (!db_) return;

    try {
        Xapian::Document doc;
        termGenerator_.set_document(doc);

        // Index specific fields
        termGenerator_.index_text(filePath, 1, "P"); // Prefix 'P' for path, weight 1
        termGenerator_.index_text(content); // General content

        // Store data for retrieval
        doc.set_data(filePath);

        // Add a value for sorting if needed (e.g., file size, date - not implemented here)
        
        // Add unique ID based on path to avoid duplicates
        std::string idterm = "Q" + filePath;
        doc.add_boolean_term(idterm);
        
        db_->replace_document(idterm, doc);
    } catch (const Xapian::Error& e) {
        std::cerr << "Xapian Indexer AddDocument Error: " << e.get_msg() << std::endl;
    }
}

void XapianIndexer::commit() {
    if (db_) {
        db_->commit();
    }
}

// --- XapianSearcher ---

XapianSearcher::XapianSearcher(const std::string& dbPath) : dbPath_(dbPath) {
    try {
        db_ = std::make_unique<Xapian::Database>(dbPath);
    } catch (const Xapian::Error& e) {
        std::cerr << "Xapian Searcher Error: " << e.get_msg() << std::endl;
        // Don't throw here if DB doesn't exist, just leaving db_ invalid or empty handled by checks
    }
}

XapianSearcher::~XapianSearcher() = default;

std::vector<SearchResult> XapianSearcher::search(const std::string& queryStr, size_t limit, size_t offset) {
    std::vector<SearchResult> results;
    if (!db_) return results;

    try {
        Xapian::Enquire enquire(*db_);
        Xapian::QueryParser parser;
        Xapian::Stem stemmer("english");
        parser.set_stemmer(stemmer);
        parser.set_database(*db_);
        
        // Enable prefix matching for paths if user types "path:/home/..."
        parser.add_prefix("path", "P");

        Xapian::Query query = parser.parse_query(queryStr, 
            Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_WILDCARD);

        std::cout << "Parsed Query: " << query.get_description() << std::endl;

        enquire.set_query(query);

        Xapian::MSet mset = enquire.get_mset(offset, limit);

        for (Xapian::MSetIterator i = mset.begin(); i != mset.end(); ++i) {
            SearchResult res;
            res.path = i.get_document().get_data();
            res.score = i.get_percent();
            
            // Generate snippet
            // NOTE: Snippet generation requires storing content or re-reading file. 
            // Xapian's built-in snippet needs term positions. 
            // Simple approach: we didn't store content in DB to save space, so snippet is empty 
            // or we could re-read snippet from file if path is accessible.
            // For MVP, just returning path and score.
            res.snippet = "(Snippet generation requires accessing file content)";
            
            results.push_back(res);
        }
    } catch (const Xapian::Error& e) {
        std::cerr << "Xapian Searcher Search Error: " << e.get_msg() << std::endl;
    }

    return results;
}

}
