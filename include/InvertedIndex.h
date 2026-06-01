#pragma once
#include "Document.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct SearchResult
{
    Document::Id docId;
    std::string docName;
    std::size_t occurrences;
};

class InvertedIndex
{
  public:
    InvertedIndex() = default;

    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;
    InvertedIndex(InvertedIndex&&) = default;
    InvertedIndex& operator=(InvertedIndex&&) = default;

    void addDocument(Document doc);

    bool removeDocument(Document::Id id);

    std::vector<SearchResult> search(const std::string& word) const;

    std::size_t wordCount(Document::Id docId, const std::string& word) const;

    std::size_t size() const noexcept
    {
        return documents_.size();
    }

    bool contains(Document::Id id) const;

    std::optional<std::reference_wrapper<const Document>> getDocument(Document::Id id) const;

  private:
    using WordMap = std::unordered_map<Document::Id, std::size_t>;

    std::unordered_map<Document::Id, Document> documents_;
    std::unordered_map<std::string, WordMap> index_;

    void indexDocument(const Document& doc);

    void unindexDocument(const Document& doc);
};
