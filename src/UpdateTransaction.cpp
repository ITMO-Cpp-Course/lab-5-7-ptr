#include "UpdateTransaction.h"
#include "IndexStore.h"

// ==================== Конструкторы и деструктор ====================

/**
 * Конструктор транзакции.
 * Вызывается только из IndexStore::beginTransaction().
 * Сохраняет указатель на владельца индекса и устанавливает флаги активного состояния.
 *
 * @param store Ссылка на IndexStore, с которым связана транзакция.
 */
UpdateTransaction::UpdateTransaction(IndexStore& store)
    : store_(&store)      // сохраняем указатель (store живёт дольше транзакции)
    , active_(true)       // транзакция активна
    , committed_(false)   // ещё не закоммичена
{
}

/**
 * Деструктор: автоматический откат при необходимости.
 * Если транзакция активна и не была закоммичена, вызывает rollback().
 * Это ключевой RAII-механизм: даже при исключении изменения не применятся.
 */
UpdateTransaction::~UpdateTransaction() {
    if (active_ && !committed_) {
        rollback();
    }
}

// ==================== Перемещение ====================

/**
 * Конструктор перемещения.
 * Забирает владение у другого объекта, а его оставляет в неактивном состоянии.
 *
 * @param other Объект, который перемещается.
 */
UpdateTransaction::UpdateTransaction(UpdateTransaction&& other) noexcept
    : store_(other.store_)
    , active_(other.active_)
    , committed_(other.committed_)
{
    // Аннулируем other, чтобы он не мог повлиять на индекс
    other.active_ = false;
    other.committed_ = false;
    other.store_ = nullptr;
}

/**
 * Оператор перемещающего присваивания.
 * Освобождает текущие ресурсы (откатывает, если нужно), затем забирает чужие.
 */
UpdateTransaction& UpdateTransaction::operator=(UpdateTransaction&& other) noexcept {
    if (this != &other) {
        // Если текущая транзакция ещё активна и не закоммичена – откатываем её
        if (active_ && !committed_) rollback();

        // Копируем данные из other
        store_ = other.store_;
        active_ = other.active_;
        committed_ = other.committed_;

        // Аннулируем other
        other.active_ = false;
        other.committed_ = false;
        other.store_ = nullptr;
    }
    return *this;
}

// ==================== Операции над staging-индексом ====================

/**
 * Добавляет документ во временный (staging) индекс.
 *
 * @return Success или одна из ошибок:
 *         - TransactionNotActive (если транзакция уже завершена)
 *         - DuplicateDocumentId (если документ с таким id уже есть в staging)
 *         - Unknown (исключение при добавлении)
 */
Result<void> UpdateTransaction::addDocument(Document doc) {
    // Проверка: транзакция должна быть активна и не закоммичена
    if (!active_ || committed_) {
        return std::unexpected(Error::TransactionNotActive);
    }
    // Защита от случая, когда stagingIndex_ почему-то отсутствует (например, после rollback)
    if (!store_->stagingIndex_) {
        return std::unexpected(Error::TransactionNotActive);
    }
    // Дубликат id в рамках этой транзакции
    if (store_->stagingIndex_->contains(doc.getId())) {
        return std::unexpected(Error::DuplicateDocumentId);
    }
    try {
        store_->stagingIndex_->addDocument(std::move(doc));
        return {};
    }
    catch (...) {
        return std::unexpected(Error::Unknown);
    }
}

/**
 * Удаляет документ из временного индекса.
 *
 * @return Success или ошибки:
 *         - TransactionNotActive
 *         - DocumentNotFound (документа нет в staging)
 */
Result<void> UpdateTransaction::removeDocument(Document::Id id) {
    if (!active_ || committed_) {
        return std::unexpected(Error::TransactionNotActive);
    }
    if (!store_->stagingIndex_) {
        return std::unexpected(Error::TransactionNotActive);
    }
    if (!store_->stagingIndex_->contains(id)) {
        return std::unexpected(Error::DocumentNotFound);
    }
    store_->stagingIndex_->removeDocument(id);
    return {};
}

// ==================== Фиксация и откат ====================

/**
 * Фиксирует изменения: заменяет основной индекс на staging.
 * После успешного вызова транзакция становится неактивной.
 */
Result<void> UpdateTransaction::commit() {
    if (!active_ || committed_) {
        return std::unexpected(Error::TransactionNotActive);
    }
    store_->commitTransaction();   // IndexStore выполняет замену
    committed_ = true;             // помечаем как закоммиченную
    active_ = false;               // транзакция завершена
    return {};
}

/**
 * Откатывает изменения вручную (удаляет staging-копию).
 * Обычно не требуется вызывать явно – деструктор сделает это автоматически.
 */
void UpdateTransaction::rollback() {
    if (!active_ || committed_) return;
    store_->rollbackTransaction();  // IndexStore удаляет stagingIndex_
    active_ = false;                // транзакция больше не активна
}