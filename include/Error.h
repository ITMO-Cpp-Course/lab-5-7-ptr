#pragma once
#include <string>
#include <expected>

enum class Error {
    DuplicateDocumentId,
    DocumentNotFound,
    InvalidWord,
    TransactionNotActive,
    TransactionAlreadyActive,   
    Unknown
};

inline std::string errorToString(Error e) {
    switch (e) {
    case Error::DuplicateDocumentId:  return "Duplicate document ID";
    case Error::DocumentNotFound:     return "Document not found";
    case Error::InvalidWord:          return "Invalid or empty word";
    case Error::TransactionNotActive: return "No active transaction";
    case Error::TransactionAlreadyActive: return "Transaction already active";
    default:                          return "Unknown error";
    }
}

template<typename T>
using Result = std::expected<T, Error>;