#ifndef WORD_TABLE_H
#define WORD_TABLE_H

#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <unordered_set>

struct WordTable {
    char **words = NULL;
    size_t count = 0;
    std::unordered_set<std::string_view> word_set;

    static WordTable from_file(const char *file_path);

    // Unsafe to call with a null word pointer.
    int contains(const char *word) const;
    int contains(const char *word, size_t length) const;
    uint64_t fingerprint() const;
    void print() const;
    void destroy();
};

inline int WordTable::contains(const char *word) const
{
    return word_set.find(word) != word_set.end();
}

inline int WordTable::contains(const char *word, size_t length) const
{
    return word_set.find(std::string_view(word, length)) != word_set.end();
}

#endif
