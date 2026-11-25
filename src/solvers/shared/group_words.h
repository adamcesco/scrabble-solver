#ifndef SRC_SOLVERS_SHARED_GROUP_WORDS_H
#define SRC_SOLVERS_SHARED_GROUP_WORDS_H

#include "window_masks.h"
#include "word_table.h"

namespace rack_solver {
struct GroupWords {
    const WordTable &dictionary;
    const uint32_t *ids;
    size_t size;
    uint8_t row, start;

    GroupWords(const WordTable &dictionary, const uint32_t *ids, size_t size, uint8_t row, uint8_t start)
        : dictionary(dictionary), ids(ids), size(size), row(row), start(start) {}

    size_t decide(bool accepted) const
    {
        trace_placements(row, start, ids, size, accepted);
        return accepted ? size : 0;
    }

    template <typename Accept>
    bool check_word(size_t i, const char *word, Accept accepts) const
    {
        trace_individual_check();
        const bool accepted = accepts(word);
        trace_placements(row, start, ids + i, 1, accepted);
        return accepted;
    }

    template <typename Accept>
    size_t count_if(Accept accepts, size_t first = 0) const
    {
        size_t count = 0;
        for (size_t i = first; i < size; ++i) {
            count += check_word(i, dictionary.words[ids[i]], accepts);
        }
        return count;
    }

    template <GapEvaluation evaluation = GapEvaluation::skip_unrestricted, typename Layout>
    size_t check_words(const GapChecks<Layout> &checks) const
    {
        return count_if([&](const char *word) {
            return checks.template accepts<evaluation>(word, checks.gap_count);
        });
    }
};
} // namespace rack_solver

#endif
