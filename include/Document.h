#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Document
{
  public:
    using Id = uint32_t;

    Document(Id id, std::string name, std::string content, std::vector<std::string> tokens);

    Document(Document&&) noexcept = default;
    Document& operator=(Document&&) noexcept = default;
    Document(const Document&) = default;
    Document& operator=(const Document&) = default;

    Id getId() const noexcept
    {
        return id_;
    }
    const std::string& getName() const noexcept
    {
        return name_;
    }
    const std::string& getContent() const noexcept
    {
        return content_;
    }
    const std::vector<std::string>& getTokens() const noexcept
    {
        return tokens_;
    }

  private:
    Id id_;
    std::string name_;
    std::string content_;
    std::vector<std::string> tokens_;
};