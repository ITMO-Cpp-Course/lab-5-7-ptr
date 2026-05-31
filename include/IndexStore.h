#pragma once

// Подключаем заголовки:
// - InvertedIndex.h — прямой и инвертированный индекс (основная логика)
// - Error.h — перечисление Error и шаблон Result = std::expected<T, Error>
#include "InvertedIndex.h"
#include "Error.h"

// Для std::unique_ptr и управления владением
#include <memory>

// Для std::optional (используется в getDocument)
#include <optional>

// Предварительное объявление класса UpdateTransaction,
// чтобы избежать циклической зависимости (определение в отдельном файле)
class UpdateTransaction;

/**
 * @brief Безопасный фасад над инвертированным индексом.
 *
 * Предоставляет:
 *   - методы, возвращающие Result<T> (явная обработка ошибок);
 *   - транзакционные обновления через RAII-объект UpdateTransaction;
 *   - корректное поведение после перемещения.
 *
 * Внутри хранит два unique_ptr<InvertedIndex>:
 *   - currentIndex_ — основная версия индекса (всегда валиден);
 *   - stagingIndex_ — временная копия для активной транзакции (nullptr, если транзакции нет).
 */
class IndexStore {
public:
    // Конструктор по умолчанию: создаёт пустой индекс через std::make_unique.
    IndexStore() = default;

    // Деструктор по умолчанию автоматически удалит unique_ptr.
    ~IndexStore() = default;

    // Индекс не копируется (дорого и не нужно для API).
    IndexStore(const IndexStore&) = delete;
    IndexStore& operator=(const IndexStore&) = delete;

    // --- Перемещение с сохранением валидного состояния moved-from объекта ---

    /// Конструктор перемещения: забирает владение индексами у other.
    IndexStore(IndexStore&& other) noexcept
        : currentIndex_(std::move(other.currentIndex_))   // currentIndex_ other становится nullptr
        , stagingIndex_(std::move(other.stagingIndex_))
    {
        // После перемещения other должен оставаться в рабочем состоянии,
        // чтобы вызов size() или contains() не приводил к UB.
        // Создаём новый пустой индекс для other.currentIndex_.
        other.currentIndex_ = std::make_unique<InvertedIndex>();
    }

    /// Оператор перемещающего присваивания.
    IndexStore& operator=(IndexStore&& other) noexcept {
        if (this != &other) {                // защита от самоприсваивания
            // Освобождаем текущие ресурсы и забираем чужие
            currentIndex_ = std::move(other.currentIndex_);
            stagingIndex_ = std::move(other.stagingIndex_);
            // Возвращаем other в валидное пустое состояние
            other.currentIndex_ = std::make_unique<InvertedIndex>();
        }
        return *this;
    }

    // --- Немедленные операции (без транзакции) ---
    // Все возвращают Result, чтобы явно сигнализировать об ошибках.

    /// Добавляет документ в индекс. Ошибки: DuplicateDocumentId, Unknown.
    Result<void> addDocument(Document doc);

    /// Удаляет документ по id. Ошибка: DocumentNotFound.
    Result<void> removeDocument(Document::Id id);

    /// Ищет документы по слову (регистронезависимо, с обрезкой пунктуации).
    /// Ошибка: InvalidWord (если после нормализации слово пустое).
    Result<std::vector<SearchResult>> search(const std::string& word) const;

    /// Возвращает количество вхождений слова в документе. Ошибка: InvalidWord.
    Result<std::size_t> wordCount(Document::Id id, const std::string& word) const;

    /// Возвращает optional-ссылку на документ. Ошибка: DocumentNotFound.
    Result<std::optional<std::reference_wrapper<const Document>>> getDocument(Document::Id id) const;

    // --- Простые наблюдатели (noexcept, защищены от nullptr) ---

    /// Общее количество документов в индексе.
    std::size_t size() const noexcept {
        // currentIndex_ может быть nullptr только после перемещения,
        // но мы гарантируем, что moved-from объект имеет валидный пустой индекс.
        return currentIndex_ ? currentIndex_->size() : 0;
    }

    /// Проверяет, существует ли документ с данным id.
    bool contains(Document::Id id) const {
        return currentIndex_ ? currentIndex_->contains(id) : false;
    }

    // --- Транзакционная поддержка ---

    /**
     * @brief Начинает новую транзакцию.
     * @return UpdateTransaction (RAII) или ошибка TransactionAlreadyActive,
     *         если предыдущая транзакция ещё не завершена.
     */
    Result<UpdateTransaction> beginTransaction();

private:
    // Дружественный класс UpdateTransaction получает доступ к приватным методам
    // commitTransaction() и rollbackTransaction(), а также к полю stagingIndex_.
    friend class UpdateTransaction;

    // Основной индекс (всегда не-null, кроме короткого промежутка в move-операциях).
    std::unique_ptr<InvertedIndex> currentIndex_ = std::make_unique<InvertedIndex>();

    // Временный индекс для текущей транзакции (копия currentIndex_).
    // Если транзакция активна, stagingIndex_ != nullptr.
    std::unique_ptr<InvertedIndex> stagingIndex_;

    /// Применяет изменения: заменяет currentIndex_ на stagingIndex_ и очищает staging.
    void commitTransaction();

    /// Откатывает изменения: просто удаляет stagingIndex_.
    void rollbackTransaction();
};