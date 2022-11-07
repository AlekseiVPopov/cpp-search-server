#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text) {
    vector<string> words;
    string word;
    for (const char c: text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

vector<int> SplitIntoInts(const string &text) {
    vector<string> int_strings;
    vector<int> result;
    string int_string;
    for (const char c: text) {
        if (c == ' ') {
            if (!int_string.empty()) {
                result.push_back(stoi(int_string));
                int_string.clear();
            }
        } else {
            int_string += c;
        }
    }
    if (!int_string.empty()) {
        result.push_back(stoi(int_string));
    }

    return result;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

class SearchServer {
public:
    void SetStopWords(const string &text) {
        if (!text.empty()) {
            for (const string &word: SplitIntoWords(text)) {
                stop_words_.insert(word);
            }
        } else {
            stop_words_ = {};
        }
    }

    void AddDocument(int document_id, const string &document, const vector <int> &ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        int rating = ComputeAverageRating(ratings);
        if (!words.empty()) {
            for (const string &word: words) {
                index_[word][document_id] += 1.0 / words.size();
                documents_rating_.insert({document_id, rating});
            }
            ++document_count_;
        }
    }

    vector<Document> FindTopDocuments(const string &raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        vector<Document> result;
        if (!matched_documents.empty()) {
            for (const auto &[id, relevance]: matched_documents) {
                result.push_back({id, relevance, GetDocumentRating(id)});
            }
        }

        sort(result.begin(), result.end(),
             [](const Document &lhs, const Document &rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return result;
    }

    int ComputeAverageRating(const vector<int> &ratings) const {
        int rating = 0;
        int size = ratings.size();

        if (size) {
            count_if(ratings.begin(), ratings.end(), [&rating](const int &i) {
                rating += i;
                return true;
            });
            rating = round(1.0 * rating / size);
        }
        return rating;
    }

    int GetDocumentRating(const int &id) const {
        return documents_rating_.at(id);
    }


private:

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };


    map<string, map<int, double>> index_;
    map<int, int> documents_rating_;
    set<string> stop_words_;
    int document_count_ = 0;


    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const string &word: SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty

        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    Query ParseQuery(const string &text) const {
        Query query;

        for (const string &word: SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    map<int, double> FindAllDocuments(const Query &query) const {
        map<int, double> matched_documents = {};

        for (const auto &word: query.plus_words) {
            if (index_.count(word)) {

                double idf = log(1.0 * document_count_ / index_.at(word).size());
                for (const auto [id, df]: index_.at(word)) {
                    matched_documents[id] += idf * df;
                }
            }
        }


        if (!query.minus_words.empty() && !matched_documents.empty()) {
            set<int> id_to_remove;
            for (const auto &word: query.minus_words) {
                if (index_.count(word)) {
                    for (const auto &s: index_.at(word)) {
                        id_to_remove.insert(s.first);
                    }
                }
            }
            for (const int &id: id_to_remove) {
                matched_documents.erase(id);
            }
        }

        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    string document, rating_string;
    vector <int> ratings;

    for (int document_id = 0; document_id < document_count; ++document_id) {
        ratings.clear();
        document = ReadLine();
        rating_string = ReadLine();
        if (!rating_string.empty()) {
            ratings = SplitIntoInts(rating_string);
        }
        search_server.AddDocument(document_id, document, ratings);
    }
    return search_server;
}


int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto &[document_id, relevance, rating]: search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", " << "relevance = "s << relevance << ", " << "rating = "s
             << rating << " }"s << endl;
    }
}