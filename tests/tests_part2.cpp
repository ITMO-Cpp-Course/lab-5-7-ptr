#include "DocumentBuilder.h"
#include "IndexStore.h"
#include "UpdateTransaction.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <optional>

using namespace Catch::Matchers;

// ==================== Тесты немедленных операций с ошибками ====================

TEST_CASE("addDocument returns DuplicateDocumentId when adding duplicate ID", "[transaction][error]")
{
    IndexStore store;
    DocumentBuilder b;
    auto doc = b.build("a", "hello");
    auto id = doc.getId();

    auto r1 = store.addDocument(std::move(doc));
    REQUIRE(r1.has_value());

    // Попытка добавить документ с тем же id
    auto r2 = store.addDocument(Document(id, "dup", "content", {"dup"}));
    REQUIRE_FALSE(r2.has_value());
    CHECK(r2.error() == Error::DuplicateDocumentId);
    CHECK(store.size() == 1u);
}

TEST_CASE("removeDocument returns DocumentNotFound for non-existent ID", "[transaction][error]")
{
    IndexStore store;
    auto r = store.removeDocument(999);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == Error::DocumentNotFound);
}

TEST_CASE("search returns InvalidWord when word consists only of digits or punctuation", "[transaction][error]")
{
    IndexStore store;
    auto r = store.search("123");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == Error::InvalidWord);
}

TEST_CASE("wordCount returns InvalidWord for invalid word", "[transaction][error]")
{
    IndexStore store;
    auto r = store.wordCount(1, "!!!");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == Error::InvalidWord);
}

TEST_CASE("getDocument returns DocumentNotFound for non-existent ID", "[transaction][error]")
{
    IndexStore store;
    auto r = store.getDocument(42);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == Error::DocumentNotFound);
}

// ==================== Транзакции: успешный коммит ====================

TEST_CASE("Transaction commit applies changes to main index", "[transaction][commit]")
{
    IndexStore store;
    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    DocumentBuilder b;
    auto doc = b.build("test", "hello world");
    auto id = doc.getId();

    auto addRes = trans.addDocument(std::move(doc));
    REQUIRE(addRes.has_value());

    auto commitRes = trans.commit();
    REQUIRE(commitRes.has_value());

    // Проверяем, что документ появился в основном индексе
    CHECK(store.size() == 1u);
    CHECK(store.contains(id));
    auto searchRes = store.search("hello");
    REQUIRE(searchRes.has_value());
    CHECK(searchRes->size() == 1u);
}

TEST_CASE("Transaction remove committed successfully", "[transaction][commit]")
{
    IndexStore store;
    DocumentBuilder b;
    auto doc = b.build("doc", "data");
    auto id = doc.getId();
    store.addDocument(std::move(doc));

    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());
    auto remRes = trans.removeDocument(id);
    REQUIRE(remRes.has_value());
    auto commitRes = trans.commit();
    REQUIRE(commitRes.has_value());

    // Проверяем, что документ удалён
    CHECK(store.size() == 0u);
    CHECK_FALSE(store.contains(id));
}

// ==================== Транзакции: автоматический и явный откат ====================

TEST_CASE("Transaction automatically rolls back on destroy without commit", "[transaction][rollback]")
{
    IndexStore store;
    {
        auto transRes = store.beginTransaction();
        REQUIRE(transRes.has_value());
        auto trans = std::move(transRes.value());
        DocumentBuilder b;
        auto doc = b.build("tmp", "content");
        auto addRes = trans.addDocument(std::move(doc));
        REQUIRE(addRes.has_value());
        // Здесь trans уничтожается, commit() не вызывался → откат
    }
    CHECK(store.size() == 0u);
}

TEST_CASE("Transaction explicit rollback prevents further operations", "[transaction][rollback]")
{
    IndexStore store;
    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    DocumentBuilder b;
    auto doc = b.build("tmp", "content");
    auto addRes = trans.addDocument(std::move(doc));
    REQUIRE(addRes.has_value());
    trans.rollback();

    CHECK(store.size() == 0u);

    // Попытка добавить документ в уже откаченную транзакцию должна вернуть ошибку
    auto addAgain = trans.addDocument(b.build("again", "x"));
    REQUIRE_FALSE(addAgain.has_value());
    CHECK(addAgain.error() == Error::TransactionNotActive);
}

// ==================== Транзакции: ошибки внутри транзакции ====================

TEST_CASE("Transaction adding duplicate ID returns error and rolls back", "[transaction][error]")
{
    IndexStore store;
    DocumentBuilder b;
    auto doc1 = b.build("doc1", "content");
    auto id = doc1.getId();
    store.addDocument(std::move(doc1));

    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    // Пытаемся добавить документ с тем же id в транзакцию
    auto doc2 = Document(id, "dup", "dup content", {"dup"});
    auto addRes = trans.addDocument(std::move(doc2));
    REQUIRE_FALSE(addRes.has_value());
    CHECK(addRes.error() == Error::DuplicateDocumentId);

    // Состояние основного индекса не изменилось
    CHECK(store.size() == 1u);
    CHECK(store.contains(id));
}

TEST_CASE("Transaction removing non-existent document returns error without changes", "[transaction][error]")
{
    IndexStore store;
    store.addDocument(DocumentBuilder().build("only", "text"));

    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());
    auto remRes = trans.removeDocument(999);
    REQUIRE_FALSE(remRes.has_value());
    CHECK(remRes.error() == Error::DocumentNotFound);

    // Основной индекс не изменился
    CHECK(store.size() == 1u);
}

// ==================== Последовательные транзакции ====================

TEST_CASE("Multiple sequential transactions work correctly", "[transaction][sequence]")
{
    IndexStore store;
    DocumentBuilder b;

    // Первая транзакция: добавляем документ
    auto t1Res = store.beginTransaction();
    REQUIRE(t1Res.has_value());
    auto t1 = std::move(t1Res.value());
    t1.addDocument(b.build("first", "alpha"));
    t1.commit();
    CHECK(store.size() == 1u);

    // Вторая транзакция: удаляем тот же документ
    auto t2Res = store.beginTransaction();
    REQUIRE(t2Res.has_value());
    auto t2 = std::move(t2Res.value());
    t2.removeDocument(0);
    t2.commit();
    CHECK(store.size() == 0u);
}

TEST_CASE("beginTransaction returns TransactionAlreadyActive when transaction is active", "[transaction][error]")
{
    IndexStore store;
    auto t1Res = store.beginTransaction();
    REQUIRE(t1Res.has_value());

    auto t2Res = store.beginTransaction();
    REQUIRE_FALSE(t2Res.has_value());
    CHECK(t2Res.error() == Error::TransactionAlreadyActive);
}

// ==================== Дополнительные тесты для покрытия крайних случаев ====================

TEST_CASE("Transaction with multiple operations commits all changes", "[transaction][commit]")
{
    IndexStore store;
    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    DocumentBuilder b;
    auto doc1 = b.build("doc1", "hello world");
    auto doc2 = b.build("doc2", "goodbye world");
    auto id1 = doc1.getId();
    auto id2 = doc2.getId();

    trans.addDocument(std::move(doc1));
    trans.addDocument(std::move(doc2));
    trans.commit();

    CHECK(store.size() == 2u);
    CHECK(store.contains(id1));
    CHECK(store.contains(id2));
}

TEST_CASE("Transaction with mixed operations (add and remove) commits correctly", "[transaction][commit]")
{
    IndexStore store;
    DocumentBuilder b;
    auto initialDoc = b.build("initial", "test");
    auto initialId = initialDoc.getId();
    store.addDocument(std::move(initialDoc));

    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    auto newDoc = b.build("new", "content");
    auto newId = newDoc.getId();
    trans.addDocument(std::move(newDoc));
    trans.removeDocument(initialId);
    trans.commit();

    CHECK(store.size() == 1u);
    CHECK_FALSE(store.contains(initialId));
    CHECK(store.contains(newId));
}

TEST_CASE("Rollback after commit does nothing", "[transaction][rollback]")
{
    IndexStore store;
    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    DocumentBuilder b;
    auto doc = b.build("test", "content");
    trans.addDocument(std::move(doc));
    trans.commit();
    trans.rollback(); // Должно быть no-op или ошибка

    CHECK(store.size() == 1u);
}

TEST_CASE("Operations on inactive transaction return TransactionNotActive", "[transaction][error]")
{
    IndexStore store;
    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());
    trans.commit();

    DocumentBuilder b;
    auto addRes = trans.addDocument(b.build("test", "content"));
    REQUIRE_FALSE(addRes.has_value());
    CHECK(addRes.error() == Error::TransactionNotActive);

    auto remRes = trans.removeDocument(0);
    REQUIRE_FALSE(remRes.has_value());
    CHECK(remRes.error() == Error::TransactionNotActive);
}

TEST_CASE("search with empty string after normalization returns InvalidWord", "[transaction][error]")
{
    IndexStore store;
    auto r = store.search("");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == Error::InvalidWord);
}

TEST_CASE("wordCount with empty string after normalization returns InvalidWord", "[transaction][error]")
{
    IndexStore store;
    auto r = store.wordCount(0, "");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == Error::InvalidWord);
}

TEST_CASE("Transaction rollback restores original state after failed operations", "[transaction][rollback]")
{
    IndexStore store;
    DocumentBuilder b;
    auto originalDoc = b.build("original", "important data");
    auto originalId = originalDoc.getId();
    store.addDocument(std::move(originalDoc));

    auto transRes = store.beginTransaction();
    REQUIRE(transRes.has_value());
    auto trans = std::move(transRes.value());

    // Пытаемся выполнить несколько операций
    auto newDoc = b.build("new", "new data");
    trans.addDocument(std::move(newDoc));

    // Эта операция должна провалиться
    auto failedRemoval = trans.removeDocument(999);
    REQUIRE_FALSE(failedRemoval.has_value());

    // Откатываем транзакцию
    trans.rollback();

    // Состояние должно вернуться к исходному
    CHECK(store.size() == 1u);
    CHECK(store.contains(originalId));
    CHECK_FALSE(store.contains(100)); // ID нового документа (предположительно 1)
}

// Запуск тестов не требуется - Catch2 автоматически генерирует main,
// но если нужно, можно определить CATCH_CONFIG_MAIN в одном файле