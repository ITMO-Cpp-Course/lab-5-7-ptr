#include "Document.h"
#include "DocumentBuilder.h"
#include "InvertedIndex.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

using namespace Catch::Matchers;

// ==================== DocumentBuilder::normalize tests ====================

TEST_CASE("normalize converts to lowercase", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize("Hello") == "hello");
    REQUIRE(DocumentBuilder::normalize("WORLD") == "world");
    REQUIRE(DocumentBuilder::normalize("MiXeD") == "mixed");
}

TEST_CASE("normalize trims leading punctuation", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize(".word") == "word");
    REQUIRE(DocumentBuilder::normalize("!hello") == "hello");
    REQUIRE(DocumentBuilder::normalize("\"text\"") == "text");
}

TEST_CASE("normalize trims trailing punctuation", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize("word,") == "word");
    REQUIRE(DocumentBuilder::normalize("word.") == "word");
    REQUIRE(DocumentBuilder::normalize("word!") == "word");
}

TEST_CASE("normalize trims punctuation on both sides", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize(".word.") == "word");
    REQUIRE(DocumentBuilder::normalize("!hi!") == "hi");
}

TEST_CASE("normalize handles empty string", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize("").empty());
}

TEST_CASE("normalize removes strings containing only punctuation", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize("...").empty());
    REQUIRE(DocumentBuilder::normalize("!!!").empty());
    REQUIRE(DocumentBuilder::normalize("---").empty());
}

TEST_CASE("normalize strips digits", "[normalize]")
{
    REQUIRE(DocumentBuilder::normalize("123").empty());
}

// ==================== DocumentBuilder::tokenize tests ====================

TEST_CASE("tokenize splits basic string", "[tokenize]")
{
    auto t = DocumentBuilder::tokenize("hello world");
    REQUIRE(t.size() == 2u);
    CHECK(t[0] == "hello");
    CHECK(t[1] == "world");
}

TEST_CASE("tokenize normalizes case and punctuation", "[tokenize]")
{
    auto t = DocumentBuilder::tokenize("Hello, World!");
    REQUIRE(t.size() == 2u);
    CHECK(t[0] == "hello");
    CHECK(t[1] == "world");
}

TEST_CASE("tokenize skips punctuation-only tokens", "[tokenize]")
{
    auto t = DocumentBuilder::tokenize("a ... b");
    REQUIRE(t.size() == 2u);
    CHECK(t[0] == "a");
    CHECK(t[1] == "b");
}

TEST_CASE("tokenize returns empty vector for empty string", "[tokenize]")
{
    REQUIRE(DocumentBuilder::tokenize("").empty());
}

TEST_CASE("tokenize preserves duplicate words", "[tokenize]")
{
    auto t = DocumentBuilder::tokenize("cat cat cat");
    REQUIRE(t.size() == 3u);
}

// ==================== DocumentBuilder tests ====================

TEST_CASE("DocumentBuilder ID auto-increments", "[builder]")
{
    DocumentBuilder b(10);
    auto d1 = b.build("a", "x");
    auto d2 = b.build("b", "y");
    CHECK(d1.getId() == 10u);
    CHECK(d2.getId() == 11u);
}

TEST_CASE("DocumentBuilder stores name and content", "[builder]")
{
    DocumentBuilder b;
    auto doc = b.build("MyDoc", "some content");
    CHECK(doc.getName() == "MyDoc");
    CHECK(doc.getContent() == "some content");
}

TEST_CASE("DocumentBuilder caches tokens in document", "[builder]")
{
    DocumentBuilder b;
    auto doc = b.build("x", "Hello, World!");
    const auto& tokens = doc.getTokens();
    REQUIRE(tokens.size() == 2u);
    CHECK(tokens[0] == "hello");
    CHECK(tokens[1] == "world");
}

// ==================== Document tests ====================

TEST_CASE("Document stores fields correctly", "[document]")
{
    Document doc(42u, "readme", "hello world", {"hello", "world"});
    CHECK(doc.getId() == 42u);
    CHECK(doc.getName() == "readme");
    CHECK(doc.getContent() == "hello world");
    CHECK(doc.getTokens().size() == 2u);
}

TEST_CASE("Document supports move semantics", "[document]")
{
    Document orig(1u, "orig", "content", {"content"});
    Document moved(std::move(orig));
    CHECK(moved.getId() == 1u);
    CHECK(moved.getName() == "orig");
}

// ==================== InvertedIndex tests ====================

TEST_CASE("InvertedIndex is empty on creation", "[index]")
{
    InvertedIndex idx;
    REQUIRE(idx.size() == 0u);
}

TEST_CASE("InvertedIndex size grows when adding documents", "[index]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "hello"));
    CHECK(idx.size() == 1u);
    idx.addDocument(b.build("b", "world"));
    CHECK(idx.size() == 2u);
}

TEST_CASE("InvertedIndex contains document after adding", "[index]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    auto doc = b.build("x", "text");
    Document::Id id = doc.getId();
    idx.addDocument(std::move(doc));
    REQUIRE(idx.contains(id));
}

TEST_CASE("InvertedIndex does not contain unknown ID", "[index]")
{
    InvertedIndex idx;
    REQUIRE_FALSE(idx.contains(999u));
}

TEST_CASE("Adding duplicate ID throws exception", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(5u, "a", "text", {"text"}));
    REQUIRE_THROWS_AS(idx.addDocument(Document(5u, "b", "other", {"other"})), std::exception);
}

TEST_CASE("Index size unchanged after duplicate ID exception", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(5u, "a", "text", {"text"}));
    try
    {
        idx.addDocument(Document(5u, "b", "other", {"other"}));
    }
    catch (...)
    {
        // expected
    }
    CHECK(idx.size() == 1u);
}

TEST_CASE("Get document by ID returns correct document", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(7u, "seven", "lucky seven", {"lucky", "seven"}));
    auto opt = idx.getDocument(7u);
    REQUIRE(opt.has_value());
    CHECK(opt->get().getName() == "seven");
}

TEST_CASE("Get missing document ID returns nullopt", "[index]")
{
    InvertedIndex idx;
    REQUIRE_FALSE(idx.getDocument(99u).has_value());
}

TEST_CASE("Remove existing document returns true", "[index]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    auto doc = b.build("a", "hello");
    Document::Id id = doc.getId();
    idx.addDocument(std::move(doc));
    REQUIRE(idx.removeDocument(id));
}

TEST_CASE("Remove decreases index size", "[index]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    auto doc = b.build("a", "hello");
    Document::Id id = doc.getId();
    idx.addDocument(std::move(doc));
    idx.removeDocument(id);
    CHECK(idx.size() == 0u);
}

TEST_CASE("Remove missing document returns false", "[index]")
{
    InvertedIndex idx;
    REQUIRE_FALSE(idx.removeDocument(999u));
}

TEST_CASE("Remove clears contains flag", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "a", "hello", {"hello"}));
    idx.removeDocument(1u);
    REQUIRE_FALSE(idx.contains(1u));
}

TEST_CASE("Remove clears word from index", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "a", "unique", {"unique"}));
    idx.removeDocument(1u);
    REQUIRE(idx.search("unique").empty());
}

TEST_CASE("Remove keeps word for other documents", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "a", "common word", {"common", "word"}));
    idx.addDocument(Document(2u, "b", "common thing", {"common", "thing"}));
    idx.removeDocument(1u);
    auto res = idx.search("common");
    REQUIRE(res.size() == 1u);
    CHECK(res[0].docId == 2u);
}

TEST_CASE("Remove document with repeated word occurrences", "[index]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    auto doc = b.build("a", "hello hello hello");
    Document::Id id = doc.getId();
    idx.addDocument(std::move(doc));
    idx.removeDocument(id);
    REQUIRE(idx.search("hello").empty());
}

TEST_CASE("Re-add after remove works correctly", "[index]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "a", "hello", {"hello"}));
    idx.removeDocument(1u);
    idx.addDocument(Document(1u, "b", "world", {"world"}));
    CHECK(idx.contains(1u));
    CHECK(idx.search("world").size() == 1u);
}

// ==================== Search tests ====================

TEST_CASE("Search for missing word returns empty", "[search]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "a", "hello world", {"hello", "world"}));
    REQUIRE(idx.search("absent").empty());
}

TEST_CASE("Search finds correct documents", "[search]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "a", "the quick fox", {"the", "quick", "fox"}));
    idx.addDocument(Document(2u, "b", "the lazy dog", {"the", "lazy", "dog"}));
    idx.addDocument(Document(3u, "c", "completely other", {"completely", "other"}));

    auto res = idx.search("the");
    REQUIRE(res.size() == 2u);

    std::vector<Document::Id> ids;
    for (const auto& r : res)
        ids.push_back(r.docId);

    CHECK(std::find(ids.begin(), ids.end(), 1u) != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), 2u) != ids.end());
}

TEST_CASE("Search is case-insensitive", "[search]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "Hello World"));
    CHECK(idx.search("hello").size() == 1u);
    CHECK(idx.search("HELLO").size() == 1u);
    CHECK(idx.search("HeLLo").size() == 1u);
}

TEST_CASE("Search strips punctuation from query", "[search]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "hello world"));
    CHECK(idx.search("hello,").size() == 1u);
    CHECK(idx.search(".hello.").size() == 1u);
}

TEST_CASE("Search returns occurrence count", "[search]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "hello hello world hello"));
    auto res = idx.search("hello");
    REQUIRE(res.size() == 1u);
    CHECK(res[0].occurrences == 3u);
}

TEST_CASE("Search results sorted by occurrences descending", "[search]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("three", "cat cat cat"));
    idx.addDocument(b.build("one", "cat"));
    idx.addDocument(b.build("two", "cat cat"));

    auto res = idx.search("cat");
    REQUIRE(res.size() == 3u);
    CHECK(res[0].occurrences >= res[1].occurrences);
    CHECK(res[1].occurrences >= res[2].occurrences);
}

TEST_CASE("Search tie-breaker by document ID when occurrences equal", "[search]")
{
    InvertedIndex idx;
    idx.addDocument(Document(10u, "b", "cat", {"cat"}));
    idx.addDocument(Document(1u, "a", "cat", {"cat"}));

    auto res = idx.search("cat");
    REQUIRE(res.size() == 2u);
    CHECK(res[0].docId == 1u);
    CHECK(res[1].docId == 10u);
}

TEST_CASE("Search result contains document name", "[search]")
{
    InvertedIndex idx;
    idx.addDocument(Document(1u, "MyGreatDoc", "keyword found here", {"keyword", "found", "here"}));
    auto res = idx.search("keyword");
    REQUIRE(res.size() == 1u);
    CHECK(res[0].docName == "MyGreatDoc");
}

TEST_CASE("Search single-word document", "[search]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("x", "only"));
    CHECK(idx.search("only").size() == 1u);
    CHECK(idx.search("absent").empty());
}

// ==================== wordCount tests ====================

TEST_CASE("wordCount returns zero for unknown word", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "hello"));
    CHECK(idx.wordCount(0u, "absent") == 0u);
}

TEST_CASE("wordCount returns zero for unknown document", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "hello"));
    CHECK(idx.wordCount(999u, "hello") == 0u);
}

TEST_CASE("wordCount returns correct count for single occurrence", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "hello world"));
    CHECK(idx.wordCount(0u, "hello") == 1u);
    CHECK(idx.wordCount(0u, "world") == 1u);
}

TEST_CASE("wordCount returns correct count for multiple occurrences", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "to be or not to be"));
    CHECK(idx.wordCount(0u, "to") == 2u);
    CHECK(idx.wordCount(0u, "be") == 2u);
    CHECK(idx.wordCount(0u, "or") == 1u);
    CHECK(idx.wordCount(0u, "not") == 1u);
}

TEST_CASE("wordCount is case-insensitive", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "Cat CAT cat"));
    CHECK(idx.wordCount(0u, "cat") == 3u);
    CHECK(idx.wordCount(0u, "CAT") == 3u);
    CHECK(idx.wordCount(0u, "Cat") == 3u);
}

TEST_CASE("wordCount returns zero after document removal", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    auto doc = b.build("a", "hello hello");
    Document::Id id = doc.getId();
    idx.addDocument(std::move(doc));
    CHECK(idx.wordCount(id, "hello") == 2u);
    idx.removeDocument(id);
    CHECK(idx.wordCount(id, "hello") == 0u);
}

TEST_CASE("wordCount is independent per document", "[wordcount]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("doc1", "cat cat cat"));
    idx.addDocument(b.build("doc2", "cat"));
    CHECK(idx.wordCount(0u, "cat") == 3u);
    CHECK(idx.wordCount(1u, "cat") == 1u);
}

// ==================== Integration tests ====================

TEST_CASE("Integration: add, search, remove, search", "[integration]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("C++ Intro", "C++ is a powerful language"));
    idx.addDocument(b.build("Python Intro", "Python is a simple language"));
    idx.addDocument(b.build("Misc Notes", "Random notes about cooking"));

    CHECK(idx.search("language").size() == 2u);

    auto pow = idx.search("powerful");
    REQUIRE(pow.size() == 1u);
    CHECK(pow[0].docName == "C++ Intro");

    idx.removeDocument(0u);
    CHECK(idx.size() == 2u);
    CHECK(idx.search("powerful").empty());

    auto lang = idx.search("language");
    REQUIRE(lang.size() == 1u);
    CHECK(lang[0].docName == "Python Intro");

    CHECK(idx.search("cooking").size() == 1u);
}

TEST_CASE("Integration: word shared across many documents", "[integration]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("d1", "common"));
    idx.addDocument(b.build("d2", "common common"));
    idx.addDocument(b.build("d3", "common common common"));
    idx.addDocument(b.build("d4", "other words here"));
    idx.addDocument(b.build("d5", "common common common common"));

    auto res = idx.search("common");
    REQUIRE(res.size() == 4u);
    CHECK(res[0].occurrences >= res[1].occurrences);
    CHECK(res[1].occurrences >= res[2].occurrences);
    CHECK(res[2].occurrences >= res[3].occurrences);

    for (const auto& r : res)
    {
        CHECK(r.docName != "d4");
    }
}

TEST_CASE("Integration: large document word count", "[integration]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    std::string text;
    for (int i = 0; i < 100; ++i)
        text += "word ";
    auto doc = b.build("big", text);
    Document::Id id = doc.getId();
    idx.addDocument(std::move(doc));
    CHECK(idx.wordCount(id, "word") == 100u);
}

TEST_CASE("Integration: multiple removes maintain correct index", "[integration]")
{
    InvertedIndex idx;
    DocumentBuilder b;
    idx.addDocument(b.build("a", "alpha beta gamma"));
    idx.addDocument(b.build("b", "beta gamma delta"));
    idx.addDocument(b.build("c", "gamma delta epsilon"));

    CHECK(idx.search("gamma").size() == 3u);
    idx.removeDocument(0u);
    CHECK(idx.search("gamma").size() == 2u);
    CHECK(idx.search("alpha").empty());
    idx.removeDocument(1u);
    CHECK(idx.search("gamma").size() == 1u);
    CHECK(idx.search("beta").empty());
}

// ==================== Additional tests ====================

TEST_CASE("Search multiple documents with same word", "[search]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("doc1", "apple banana"));
    idx.addDocument(b.build("doc2", "apple"));

    auto res = idx.search("apple");
    REQUIRE(res.size() == 2u);

    bool has_doc1 = false, has_doc2 = false;
    for (const auto& r : res)
    {
        if (r.docName == "doc1")
            has_doc1 = true;
        if (r.docName == "doc2")
            has_doc2 = true;
    }
    CHECK(has_doc1);
    CHECK(has_doc2);
}

TEST_CASE("Remove document removes it from search results", "[search]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    auto d = b.build("doc1", "test word");
    auto id = d.getId();

    idx.addDocument(std::move(d));
    idx.removeDocument(id);

    auto res = idx.search("test");
    CHECK(res.empty());
}

TEST_CASE("Search word not present in any document returns empty", "[search]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("doc1", "hello world"));
    idx.addDocument(b.build("doc2", "foo bar"));

    CHECK(idx.search("absent").empty());
    CHECK(idx.search("xyz").empty());
}

TEST_CASE("Search handles case-insensitive variants correctly", "[search]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("doc", "Hello World"));

    CHECK(idx.search("hello").size() == 1u);
    CHECK(idx.search("HELLO").size() == 1u);
    CHECK(idx.search("Hello").size() == 1u);
    CHECK(idx.search("WORLD").size() == 1u);
}

TEST_CASE("Empty document can be added without error", "[index]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("empty_doc", ""));

    CHECK(idx.size() == 1u);
    CHECK(idx.search("anything").empty());
}

TEST_CASE("Punctuation-only document can be added", "[index]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("punct", "... !!! ---"));

    CHECK(idx.size() == 1u);
    CHECK(idx.search("anything").empty());
}

TEST_CASE("Sequential removes correctly update search results", "[search]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("a", "common"));
    idx.addDocument(b.build("b", "common"));
    idx.addDocument(b.build("c", "common"));

    CHECK(idx.search("common").size() == 3u);

    idx.removeDocument(0u);
    CHECK(idx.search("common").size() == 2u);

    idx.removeDocument(1u);
    CHECK(idx.search("common").size() == 1u);

    idx.removeDocument(2u);
    CHECK(idx.search("common").empty());
}

TEST_CASE("wordCount updates correctly after partial remove", "[wordcount]")
{
    DocumentBuilder b;
    InvertedIndex idx;

    idx.addDocument(b.build("a", "test test test"));
    idx.addDocument(b.build("b", "test"));

    CHECK(idx.wordCount(0u, "test") == 3u);
    CHECK(idx.wordCount(1u, "test") == 1u);

    idx.removeDocument(0u);

    CHECK(idx.wordCount(0u, "test") == 0u);
    CHECK(idx.wordCount(1u, "test") == 1u);
    CHECK(idx.search("test").size() == 1u);
}