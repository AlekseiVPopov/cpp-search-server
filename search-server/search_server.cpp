#include "search_server.h"


void SearchServer::AddDocument(int document_id, const std::string &document, DocumentStatus status,
                               const std::vector<int> &ratings) {
    using namespace std;
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string &word: words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

//int SearchServer::GetDocumentId(int index) const {
//    return document_ids_.at(index);
//}

std::map<std::string, double> SearchServer::GetWordFrequencies(int document_id) const
{
    if(document_to_word_freqs.count(document_id)!=0)
    {
        return document_to_word_freqs.at(document_id);
    }
    static std::map<std::string, double> empty_word_frequencies{};
    return empty_word_frequencies;
}

std::tuple<std::vector<std::string>, DocumentStatus>
SearchServer::MatchDocument(const std::string &raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string &word: query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string &word: query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const std::string &word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string &word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string &text) const {
    using namespace std;
    vector<string> words;
    for (const std::string &word: SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}


SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string &text) const {
    using namespace std;
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid"s);
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string &text) const {
    Query result;
    for (const std::string &word: SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

std::vector<int>::iterator SearchServer::begin() {
    return std::begin(document_ids_);
}

std::vector<int>::iterator SearchServer::end(){
    return std::end(document_ids_);
}

void SearchServer::RemoveDocument(int document_id)
{
    documents_.erase(document_id);
    document_to_word_freqs.erase(document_id);

    for (auto word :SearchServer::word_to_document_freqs_)
    {
        auto word_freq_map = word.second;
        bool is_document_present = word_freq_map.count(document_id);
        if(is_document_present)
        {
            word_freq_map.erase(document_id);
        }
    }
}

