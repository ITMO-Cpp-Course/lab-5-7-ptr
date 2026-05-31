#pragma once

#include "Error.h"
#include "InvertedIndex.h"

#include <memory>

#include <optional>

class UpdateTransaction;

class IndexStore
{
  public:
    IndexStore() = default;

    ~IndexStore() = default;

    IndexStore(const IndexStore&) = delete;
    IndexStore& operator=(const IndexStore&) = delete;

    IndexStore(IndexStore&& other) noexcept
        : currentIndex_(std::move(other.currentIndex_)), stagingIndex_(std::move(other.stagingIndex_))
    {
        other.currentIndex_ = std::make_unique<InvertedIndex>();
    }

    IndexStore& operator=(IndexStore&& other) noexcept
    {
        if (this != &other)
        {
            currentIndex_ = std::move(other.currentIndex_);
            stagingIndex_ = std::move(other.stagingIndex_);
            other.currentIndex_ = std::make_unique<InvertedIndex>();
        }
        return *this;
    }

    Result<void> addDocument(Document doc);

    Result<void> removeDocument(Document::Id id);

    Result<std::vector<SearchResult>> search(const std::string& word) const;

    Result<std::size_t> wordCount(Document::Id id, const std::string& word) const;

    Result<std::optional<std::reference_wrapper<const Document>>> getDocument(Document::Id id) const;

    std::size_t size() const noexcept
    {
        return currentIndex_ ? currentIndex_->size() : 0;
    }

    bool contains(Document::Id id) const
    {
        return currentIndex_ ? currentIndex_->contains(id) : false;
    }

    Result<UpdateTransaction> beginTransaction();

  private:
    friend class UpdateTransaction;

    std::unique_ptr<InvertedIndex> currentIndex_ = std::make_unique<InvertedIndex>();

    std::unique_ptr<InvertedIndex> stagingIndex_;

    void commitTransaction();

    void rollbackTransaction();
};