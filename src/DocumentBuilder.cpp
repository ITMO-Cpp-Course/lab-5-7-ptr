#include "DocumentBuilder.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

DocumentBuilder::DocumentBuilder(Document::Id nextId) : nextId_(nextId) {}

Document DocumentBuilder::build(std::string name, std::string content) {
  auto tokens = tokenize(content);
  return Document(nextId_++, std::move(name), std::move(content),
                  std::move(tokens));
}

std::string DocumentBuilder::normalize(std::string word) {
  std::transform(word.begin(), word.end(), word.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });

  auto first = std::find_if(word.begin(), word.end(), [](char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
  });
  word.erase(word.begin(), first);

  if (word.empty())
    return word;

  auto last = std::find_if(word.rbegin(), word.rend(), [](char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
  });
  word.erase(last.base(), word.end());

  return word;
}

std::vector<std::string> DocumentBuilder::tokenize(const std::string &text) {
  std::vector<std::string> tokens;
  std::istringstream stream(text);
  std::string raw;

  while (stream >> raw) {
    std::string word = normalize(raw);
    if (!word.empty()) {
      tokens.push_back(std::move(word));
    }
  }

  return tokens;
}