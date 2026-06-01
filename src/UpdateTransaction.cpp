#include "UpdateTransaction.h"
#include "IndexStore.h"

UpdateTransaction::UpdateTransaction(IndexStore& store) : store_(&store), active_(true), committed_(false) {}

UpdateTransaction::~UpdateTransaction()
{
    if (active_ && !committed_)
    {
        rollback();
    }
}

UpdateTransaction::UpdateTransaction(UpdateTransaction&& other) noexcept
    : store_(other.store_), active_(other.active_), committed_(other.committed_)
{
    other.active_ = false;
    other.committed_ = false;
    other.store_ = nullptr;
}

UpdateTransaction& UpdateTransaction::operator=(UpdateTransaction&& other) noexcept
{
    if (this != &other)
    {
        if (active_ && !committed_)
            rollback();

        store_ = other.store_;
        active_ = other.active_;
        committed_ = other.committed_;

        other.active_ = false;
        other.committed_ = false;
        other.store_ = nullptr;
    }
    return *this;
}

Result<void> UpdateTransaction::addDocument(Document doc)
{
    if (!active_ || committed_)
    {
        return std::unexpected(Error::TransactionNotActive);
    }
    if (!store_->stagingIndex_)
    {
        return std::unexpected(Error::TransactionNotActive);
    }
    if (store_->stagingIndex_->contains(doc.getId()))
    {
        return std::unexpected(Error::DuplicateDocumentId);
    }
    try
    {
        store_->stagingIndex_->addDocument(std::move(doc));
        return {};
    }
    catch (...)
    {
        return std::unexpected(Error::Unknown);
    }
}

Result<void> UpdateTransaction::removeDocument(Document::Id id)
{
    if (!active_ || committed_)
    {
        return std::unexpected(Error::TransactionNotActive);
    }
    if (!store_->stagingIndex_)
    {
        return std::unexpected(Error::TransactionNotActive);
    }
    if (!store_->stagingIndex_->contains(id))
    {
        return std::unexpected(Error::DocumentNotFound);
    }
    store_->stagingIndex_->removeDocument(id);
    return {};
}

Result<void> UpdateTransaction::commit()
{
    if (!active_ || committed_)
    {
        return std::unexpected(Error::TransactionNotActive);
    }
    store_->commitTransaction();
    committed_ = true;
    active_ = false;
    return {};
}

void UpdateTransaction::rollback()
{
    if (!active_ || committed_)
        return;
    store_->rollbackTransaction();
    active_ = false;
}