#include "IndexStore.h"
#include "DocumentBuilder.h"
#include "UpdateTransaction.h"

Result<void> IndexStore::addDocument(Document doc)
{
    if (currentIndex_->contains(doc.getId()))
    {
        return std::unexpected(Error::DuplicateDocumentId);
    }
    try
    {

        currentIndex_->addDocument(std::move(doc));
        return {}; 
    }
    catch (...)
    {

        return std::unexpected(Error::Unknown);
    }
}



Result<void> IndexStore::removeDocument(Document::Id id)
{
    if (!currentIndex_->contains(id))
    {
        return std::unexpected(Error::DocumentNotFound);
    }

    bool removed = currentIndex_->removeDocument(id);
    return removed ? Result<void>{} : std::unexpected(Error::DocumentNotFound);
}

<SearchResult>> IndexStore::search(const std::string& word) const
{
    std::string normalized = DocumentBuilder::normalize(word);
    if (normalized.empty())
    {
        return std::unexpected(Error::InvalidWord);
    }

    return currentIndex_->search(normalized);
}


Result<std::size_t> IndexStore::wordCount(Document::Id id, const std::string& word) const
{
    std::string normalized = DocumentBuilder::normalize(word);
    if (normalized.empty())
    {
        return std::unexpected(Error::InvalidWord);
    }
    return currentIndex_->wordCount(id, normalized);
}

Result<std::optional<std::reference_wrapper<const Document>>> IndexStore::getDocument(Document::Id id) const
{
    auto opt = currentIndex_->getDocument(id);
    if (!opt.has_value())
    {
        return std::unexpected(Error::DocumentNotFound);
    }
    return opt;
}


Result<UpdateTransaction> IndexStore::beginTransaction()
{
    if (stagingIndex_)
    {
        return std::unexpected(Error::TransactionAlreadyActive);
    }

    stagingIndex_ = std::make_unique<InvertedIndex>(*currentIndex_);

    return UpdateTransaction(*this);
}

void IndexStore::commitTransaction()
{
    if (stagingIndex_)
    {

        currentIndex_ = std::move(stagingIndex_);
        stagingIndex_.reset();
    }
}


void IndexStore::rollbackTransaction()
{
    stagingIndex_.reset();
}