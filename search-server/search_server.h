#pragma once

#include <algorithm>
#include <exception>
#include <execution>
#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <cmath>

#include "concurrent_map.h"
#include "document.h"
#include "log_duration.h"
#include "string_processing.h"


const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:
    SearchServer() = default;

    template<typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words);

    explicit SearchServer(std::string_view stop_words_text);

    explicit SearchServer(const std::string &stop_words_text);

    void
    AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int> &ratings);

    template<typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy &&policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy, std::string_view raw_query) const;

    template<typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy &&policy, std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;


    int GetDocumentCount() const;

    auto begin() const {
        return document_ids_.begin();
    }

    auto end() const {
        return document_ids_.end();
    }

    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy &, int document_id);

    void RemoveDocument(const std::execution::parallel_policy &, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy &,
                                                                            std::string_view raw_query,
                                                                            int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy &,
                                                                            std::string_view raw_query,
                                                                            int document_id) const;

    std::set<std::string, std::less<>> GetStopWords() {
        return stop_words_;
    }

private:
    struct DocumentData {
        std::string document_data;
        int rating;
        DocumentStatus status;
    };
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    bool IsValidWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text, bool remove_duplicates = true) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string &word) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy &, const Query &query,
                                           DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy &, const Query &query,
                                           DocumentPredicate document_predicate) const;
};

template<typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    for (const auto &word: stop_words_) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, [](int document_id, DocumentStatus document_status, int rating) {
        return document_status == DocumentStatus::ACTUAL;
    });
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy, const std::string_view raw_query,
                                                     DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

template<class ExecutionPolicy, class DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy, const std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(static_cast<std::string>(raw_query));
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document &lhs, const Document &rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy &,
                                                     const Query &query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (auto word_view: query.plus_words) {
        if (word_to_document_freqs_.count(word_view) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word_view));
        for (const auto &[document_id, term_freq]: word_to_document_freqs_.at(word_view)) {
            const auto &document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const auto &word_view: query.minus_words) {
        std::string word(word_view);
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance]: document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy &,
                                                     const Query &query, DocumentPredicate document_predicate) const {
    std::vector<std::string_view> words(query.plus_words.size());
    auto words_end = std::copy_if(
            std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            words.begin(),
            [this](std::string_view word_view) {
                std::string word(word_view);
                return word_to_document_freqs_.count(word) && !word_to_document_freqs_.at(word).empty();
            }
    );
    words.erase(words_end, words.end());
    words.erase(std::remove_if(
            std::execution::par,
            words.begin(), words.end(),
            [this, &query](std::string_view word_view) {
                std::string word(word_view);
                return word_to_document_freqs_.count(word) && std::count(query.minus_words.begin(),
                                                                         query.minus_words.end(), word_view);
            }
    ), words.end());

    ConcurrentMap<int, double> document_to_relevance_concurent(101u);
    std::for_each(
            std::execution::par,
            words.begin(), words.end(),
            [this, document_predicate, &document_to_relevance_concurent](std::string_view word_view) {
                std::string word(word_view);
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq]: word_to_document_freqs_.at(word)) {
                    const auto &document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance_concurent[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
    );

    const auto &document_to_relevance_ordinary = document_to_relevance_concurent.BuildOrdinaryMap();
    std::vector<Document> matched_documents(document_to_relevance_ordinary.size());
    transform(
            std::execution::par,
            document_to_relevance_ordinary.begin(), document_to_relevance_ordinary.end(),
            matched_documents.begin(),
            [this](const auto &item) {
                const int document_id = item.first;
                const double relevance = item.second;
                return Document{document_id, relevance, documents_.at(document_id).rating};
            }
    );

    return matched_documents;
}
