#include "Document.h"
#include <utility>

Document::Document(Id id, std::string name, std::string content, std::vector<std::string> tokens)
    : id_(id), name_(std::move(name)), content_(std::move(content)), tokens_(std::move(tokens))
{
}