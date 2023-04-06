#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <set>

std::vector<std::string> SplitIntoWords(const std::string &text);

std::vector<std::string_view> SplitIntoWordsView(const std::string_view str);

template<typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer &container) {
    std::set<std::string, std::less<>> non_empty_words;
    for (const auto &word: container) {
        if (!word.empty()) {
            non_empty_words.insert(std::string(word));
        }
    }
    return non_empty_words;
}