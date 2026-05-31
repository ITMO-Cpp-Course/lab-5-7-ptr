#pragma once

#include "Document.h"
#include "Error.h"

class IndexStore;

class UpdateTransaction
{
  public:
    explicit UpdateTransaction(IndexStore& store);

    ~UpdateTransaction();

    UpdateTransaction(const UpdateTransaction&) = delete;
    UpdateTransaction& operator=(const UpdateTransaction&) = delete;

    UpdateTransaction(UpdateTransaction&& other) noexcept;
    UpdateTransaction& operator=(UpdateTransaction&& other) noexcept;

    Result<void> addDocument(Document doc);

    Result<void> removeDocument(Document::Id id);

    Result<void> commit();

    void rollback();

    bool isActive() const
    {
        return active_;
    }

  private:
    IndexStore* store_; ///< Указатель на IndexStore (не владеет, не может быть nullptr)
    bool active_;
    bool committed_;
};