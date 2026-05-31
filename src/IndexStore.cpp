#include "IndexStore.h"
#include "UpdateTransaction.h"
#include "DocumentBuilder.h"

// ==================== Публичные методы (немедленные операции) ====================

/**
 * Добавляет документ в основной индекс (без транзакции).
 * Проверяет дубликат id до добавления.
 * При возникновении исключения во время добавления возвращает Unknown.
 */
Result<void> IndexStore::addDocument(Document doc) {
    // Проверка: если документ с таким id уже существует – ошибка дубликата.
    if (currentIndex_->contains(doc.getId())) {
        return std::unexpected(Error::DuplicateDocumentId);
    }
    try {
        // Вызов метода InvertedIndex, который может бросить исключение (например, bad_alloc).
        currentIndex_->addDocument(std::move(doc));
        return {};   // успех (void)
    }
    catch (...) {
        // Любое исключение преобразуем в общую ошибку Unknown.
        return std::unexpected(Error::Unknown);
    }
}

/**
 * Удаляет документ из основного индекса.
 * Если документ не найден – возвращает DocumentNotFound.
 */
Result<void> IndexStore::removeDocument(Document::Id id) {
    if (!currentIndex_->contains(id)) {
        return std::unexpected(Error::DocumentNotFound);
    }
    // removeDocument возвращает bool: true – удалён, false – не найден (но мы уже проверили).
    bool removed = currentIndex_->removeDocument(id);
    return removed ? Result<void>{} : std::unexpected(Error::DocumentNotFound);
}

/**
 * Поиск документов по слову.
 * Сначала нормализует слово (lowercase + обрезка пунктуации).
 * Если после нормализации слово пустое – возвращает InvalidWord.
 * Иначе вызывает метод search у основного индекса, передавая уже нормализованное слово.
 */
Result<std::vector<SearchResult>> IndexStore::search(const std::string& word) const {
    std::string normalized = DocumentBuilder::normalize(word);
    if (normalized.empty()) {
        return std::unexpected(Error::InvalidWord);
    }
    // Передаём нормализованное слово, чтобы избежать повторной нормализации внутри InvertedIndex.
    return currentIndex_->search(normalized);
}

/**
 * Возвращает количество вхождений слова в указанном документе.
 * Аналогично search: нормализует слово, проверяет на пустоту, затем делегирует.
 */
Result<std::size_t> IndexStore::wordCount(Document::Id id, const std::string& word) const {
    std::string normalized = DocumentBuilder::normalize(word);
    if (normalized.empty()) {
        return std::unexpected(Error::InvalidWord);
    }
    return currentIndex_->wordCount(id, normalized);
}

/**
 * Возвращает документ по id (обёрнутый в optional).
 * Если документ не найден – ошибка DocumentNotFound.
 */
Result<std::optional<std::reference_wrapper<const Document>>>
IndexStore::getDocument(Document::Id id) const {
    auto opt = currentIndex_->getDocument(id);
    if (!opt.has_value()) {
        return std::unexpected(Error::DocumentNotFound);
    }
    return opt;
}

// ==================== Транзакционная поддержка ====================

/**
 * Начинает новую транзакцию.
 * Если транзакция уже активна (stagingIndex_ != nullptr), возвращает TransactionAlreadyActive.
 * Иначе создаётся глубокая копия текущего индекса (stagingIndex_) и возвращается
 * RAII-объект UpdateTransaction, который будет управлять этой копией.
 */
Result<UpdateTransaction> IndexStore::beginTransaction() {
    if (stagingIndex_) {
        // Транзакция уже активна – нельзя начать вторую.
        return std::unexpected(Error::TransactionAlreadyActive);
    }
    // Создаём копию текущего индекса для staging.
    stagingIndex_ = std::make_unique<InvertedIndex>(*currentIndex_);
    // Возвращаем транзакцию, которая получит доступ к stagingIndex_ через дружественность.
    return UpdateTransaction(*this);
}

/**
 * Фиксирует изменения: заменяет основной индекс на staging.
 * Вызывается только из UpdateTransaction::commit().
 * После замены stagingIndex_ сбрасывается (уникальный указатель освобождает память).
 */
void IndexStore::commitTransaction() {
    if (stagingIndex_) {
        // Перемещаем владение: текущий индекс заменяется staging.
        currentIndex_ = std::move(stagingIndex_);
        stagingIndex_.reset();   // теперь staging пуст, транзакция завершена
    }
}

/**
 * Откатывает изменения: просто удаляет staging-копию.
 * Вызывается из UpdateTransaction::rollback() или деструктора UpdateTransaction.
 */
void IndexStore::rollbackTransaction() {
    stagingIndex_.reset();   // отбрасываем временную копию
}