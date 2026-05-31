#pragma once
#include "Document.h"
#include <string>
#include <vector>

class DocumentBuilder
{
  public:
    explicit DocumentBuilder(Document::Id nextId = 0);

    Document build(std::string name, std::string content);

    static std::vector<std::string> tokenize(const std::string& text);

    static std::string normalize(std::string word);

  private:
    Document::Id nextId_;
};