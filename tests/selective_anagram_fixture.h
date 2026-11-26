#pragma once

#include <string>

// For A.., all 325 rack subsets drawn from A through Y occur.
// BC has two words in dictionary order. Every subset containing Z is absent.
inline std::string selective_anagram_dictionary()
{
    std::string text = "AA\nACB\n";
    for (char first = 'A'; first < 'Z'; ++first) {
        for (char second = first; second < 'Z'; ++second) {
            text += std::string{'A', first, second, '\n'};
        }
    }
    return text;
}
