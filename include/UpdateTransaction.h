#pragma once

#include "Document.h"
#include "Error.h"

// Предварительное объявление IndexStore (определение в отдельном файле,
// включать его здесь нельзя из-за циклической зависимости)
class IndexStore;

/**
 * @brief RAII-транзакция для атомарного обновления IndexStore.
 *
 * Схема работы:
 *   1. Вызов IndexStore::beginTransaction() создаёт временную копию индекса.
 *   2. Все изменения (addDocument, removeDocument) применяются к этой копии.
 *   3. При вызове commit() копия замещает основной индекс (атомарно).
 *   4. Если commit() не вызван, деструктор автоматически откатывает изменения.
 *
 * Преимущества:
 *   - Автоматический откат при исключениях или преждевременном выходе из зоны видимости.
 *   - Чёткая семантика: либо все изменения применены, либо ни одного.
 *   - Move-семантика позволяет передавать транзакцию между функциями.
 *
 * Пример использования:
 *   auto tx = store.beginTransaction().value();
 *   tx.addDocument(doc1);
 *   tx.addDocument(doc2);
 *   tx.commit();  // изменения фиксируются
 *
 * Ошибки:
 *   - addDocument / removeDocument могут вернуть DuplicateDocumentId, DocumentNotFound.
 *   - commit() может вернуть TransactionNotActive (если транзакция уже завершена).
 */
class UpdateTransaction
{
  public:
    /**
     * @brief Конструктор, вызывается только из IndexStore::beginTransaction().
     * @param store Ссылка на владельца индекса (хранится указатель).
     *
     * @note Конструктор explicit, чтобы случайно не создать транзакцию без store.
     */
    explicit UpdateTransaction(IndexStore& store);

    /**
     * @brief Деструктор: автоматически откатывает изменения, если не был вызван commit().
     *
     * Реализует RAII: если транзакция активна и не закоммичена, вызывается rollback().
     */
    ~UpdateTransaction();

    // Запрещаем копирование (транзакция владеет ресурсом и не должна дублироваться)
    UpdateTransaction(const UpdateTransaction&) = delete;
    UpdateTransaction& operator=(const UpdateTransaction&) = delete;

    // Разрешаем перемещение (передача владения между объектами)
    UpdateTransaction(UpdateTransaction&& other) noexcept;
    UpdateTransaction& operator=(UpdateTransaction&& other) noexcept;

    /**
     * @brief Добавляет документ во временный индекс (staging).
     * @return Success или ошибка:
     *         - DuplicateDocumentId (если документ с таким id уже есть в staging)
     *         - TransactionNotActive (транзакция уже закоммичена или откачена)
     *         - Unknown (исключение при добавлении)
     *
     * @note Не изменяет основной индекс до commit().
     */
    Result<void> addDocument(Document doc);

    /**
     * @brief Удаляет документ из временного индекса.
     * @return Success или ошибка:
     *         - DocumentNotFound (документа нет в staging)
     *         - TransactionNotActive
     */
    Result<void> removeDocument(Document::Id id);

    /**
     * @brief Фиксирует изменения: заменяет основной индекс на staging.
     * @return Success или ошибка TransactionNotActive (если транзакция уже завершена).
     *
     * После успешного commit() объект транзакции становится неактивным;
     * дальнейшие вызовы addDocument/removeDocument/commit вернут ошибку.
     */
    Result<void> commit();

    /**
     * @brief Откатывает изменения вручную (удаляет staging-копию).
     *
     * После rollback() транзакция становится неактивной.
     * Обычно не требуется вызывать вручную, деструктор сделает это автоматически.
     */
    void rollback();

    /**
     * @brief Проверяет, активна ли транзакция (ещё не закоммичена и не откачена).
     */
    bool isActive() const
    {
        return active_;
    }

  private:
    IndexStore* store_; ///< Указатель на IndexStore (не владеет, не может быть nullptr)
    bool active_;       ///< true, пока транзакция жива и не завершена
    bool committed_;    ///< true, если commit() уже был вызван успешно
};