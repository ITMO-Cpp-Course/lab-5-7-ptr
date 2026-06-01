#include "Document.h"
#include <utility>
using namespace std;
Document::Document(Id id, string name, string content, vector<string> tokens)
    : id_(id), name_(move(name)), content_(move(content)), tokens_(move(tokens))
{
}