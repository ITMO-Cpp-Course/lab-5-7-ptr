#include "InvertedIndex.h"
#include "DocumentBuilder.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

void InvertedIndex::addDocument(Document doc) {
  if (documents_.count(doc.getId())) {
    throw std::invalid_argument(
        "Document with id=" + std::to_string(doc.getId()) + " already exists");
  }

  auto [it, ok] = documents_.emplace(doc.getId(), std::move(doc));

  indexDocument(it->second);
}

bool InvertedIndex::removeDocument(Document::Id id) {
  auto it = documents_.find(id);
  if (it == documents_.end())
    return false;

  unindexDocument(it->second);
  documents_.erase(it);
  return true;
}

std::vector<SearchResult> InvertedIndex::search(const std::string &word) const {
  const std::string normalized = DocumentBuilder::normalize(word);

  auto wit = index_.find(normalized);
  if (wit == index_.end())
    return {};

  std::vector<SearchResult> results;
  results.reserve(wit->second.size());

  for (const auto &[docId, count] : wit->second) {
    auto dit = documents_.find(docId);
    if (dit != documents_.end()) {
      results.push_back({docId, dit->second.getName(), count});
    }
  }

  std::sort(results.begin(), results.end(),
            [](const SearchResult &a, const SearchResult &b) {
              if (a.occurrences != b.occurrences)
                return a.occurrences > b.occurrences;
              return a.docId < b.docId;
            });

  return results;
}

std::size_t InvertedIndex::wordCount(Document::Id docId,
                                     const std::string &word) const {
  const std::string normalized = DocumentBuilder::normalize(word);

  auto wit = index_.find(normalized);
  if (wit == index_.end())
    return 0;

  auto dit = wit->second.find(docId);
  if (dit == wit->second.end())
    return 0;

  return dit->second;
}

bool InvertedIndex::contains(Document::Id id) const {
  return documents_.count(id) > 0;
}

std::optional<std::reference_wrapper<const Document>>
InvertedIndex::getDocument(Document::Id id) const {
  auto it = documents_.find(id);
  if (it == documents_.end())
    return std::nullopt;
  return std::cref(it->second);
}

void InvertedIndex::indexDocument(const Document &doc) {
  for (const auto &word : doc.getTokens()) {
    index_[word][doc.getId()]++;
  }
}

void InvertedIndex::unindexDocument(const Document &doc) {
  const auto &tokens = doc.getTokens();

  std::unordered_set<std::string> uniqueWords(tokens.begin(), tokens.end());

  for (const auto &word : uniqueWords) {
    auto wit = index_.find(word);
    if (wit == index_.end())
      continue;

    wit->second.erase(doc.getId());

    if (wit->second.empty()) {
      index_.erase(wit);
    }
  }
}