#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <numeric>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string &text) {
        for (const string &word: SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document, DocumentStatus status,
                     const vector<int> &ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word: words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename P>
    vector<Document> FindTopDocuments(const string &raw_query, P pred) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, pred);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
                 bool is_almost_equal = abs(lhs.relevance - rhs.relevance) < EPSILON;
                 return is_almost_equal ? lhs.rating > rhs.rating : lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }


    vector<Document> FindTopDocuments(const string &raw_query, const DocumentStatus ds = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [ds](int document_id, DocumentStatus status, int rating) {
            return status == ds;
        });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        set<string> matched_plus_words = {};
        bool skip = false;

        for (const string minus_word: query.minus_words) {
            if (word_to_document_freqs_.count(minus_word) &&
                !word_to_document_freqs_.at(minus_word).empty() &&
                word_to_document_freqs_.at(minus_word).count(document_id)) {
                skip = true;
                break;
            }
        }

        if (!skip) {
            for (const string plus_word: query.plus_words) {
                if (word_to_document_freqs_.count(plus_word) &&
                    !word_to_document_freqs_.at(plus_word).empty() &&
                    word_to_document_freqs_.at(plus_word).count(document_id)) {
                    matched_plus_words.insert(plus_word);
                }
            }
        }
        vector<string> result_words(matched_plus_words.begin(), matched_plus_words.end());
        return tuple(result_words, documents_.at(document_id).status);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int> &ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

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

    // Existence required
    double ComputeWordInverseDocumentFreq(const string &word) const {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    }


    vector<Document> FindAllDocuments(const Query &query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindAllDocuments(query, [status](int document_id, DocumentStatus ds, int rating) {
            return status == ds;
        });
    }

    template<typename P>
    vector<Document> FindAllDocuments(const Query &query, P pred) const {
        map<int, double> document_to_relevance;
        for (const string &word: query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq]: word_to_document_freqs_.at(word)) {
                if (pred(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word: query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance]: document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};


void PrintMatchDocumentResult(int document_id, const vector<string> &words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string &word: words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template<typename T>
ostream &operator<<(ostream &os, const vector<T> &v) {
    if (v.empty()) {
        os << "[]"s;
    } else {
        bool first = true;
        for (const auto &e: v) {
            if (first) {
                os << "["s << e;
                first = false;
                continue;
            }
            os << ", "s << e;
        }
        os << "]"s;
    }
    return os;
}

template<typename T>
ostream &operator<<(ostream &os, const set<T> &s) {
    if (s.empty()) {
        os << "{}"s;
    } else {
        bool first = true;
        for (const auto &e: s) {
            if (first) {
                os << "{"s << e;
                first = false;
                continue;
            }
            os << ", "s << e;
        }
        os << "}"s;
    }
    return os;
}


template<typename K, typename V>
ostream &operator<<(ostream &os, const map<K, V> &m) {
    if (m.empty()) {
        os << "{}"s;
    } else {
        bool first = true;
        for (const auto &[key, value]: m) {
            if (first) {
                os << "{"s << key << ": " << value;
                first = false;
                continue;
            }
            os << ", "s << key << ": " << value;
        }
        os << "}"s;
    }
    return os;
}

template<typename T, typename U>
void ASSERTEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

template<typename T, typename U>
void
ASSERTEqualImpl(const vector<T> &vt, const vector<U> &vu, const string &t_str, const string &u_str, const string &file,
                const string &func, unsigned line, const string &hint) {
    bool test_failed = false;
    if (vt.size() != vu.size()) {
        test_failed = true;
    }

    if (!test_failed) {
        for (int i = 0; i < vt.size(); i++) {
            if (vt[i] != vu[i]) {
                cout << boolalpha;
                cout << file << "("s << line << "): "s << func << ": "s;
                cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
                cout << vt << " != "s << vu << "."s;
                if (!hint.empty()) {
                    cout << " Hint: "s << hint;
                }
                cout << endl;
                abort();
            }
        }
    }
}

#define ASSERT_EQUAL(a, b) ASSERTEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) ASSERTEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void ASSERTImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) ASSERTImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) ASSERTImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template<typename F>
void RunTestImpl(F func, const string &s) {
    func();
    cerr << s << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

void TestAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
        ASSERT(doc0.rating == 2);
    }

    {
        SearchServer server;
        server.SetStopWords(""s);
        server.AddDocument(doc_id, "", DocumentStatus::ACTUAL, {});
        ASSERT(server.FindTopDocuments("in"s).empty());
    }

}

void TestStopWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("cat in the city"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, content + " left", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 10, content + "right", DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat"s).size() == 3);
    }

}

void TestMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, content + " left", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 10, content + "right", DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat -left"s).size() == 2);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, content + " left", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 10, content + " right", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -left -right"s);
        ASSERT(found_docs.size() == 1);
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, content + " left", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 10, content + "right", DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat - "s).size() == 3);
    }
}

void TestRating() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.SetStopWords("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id + 1, content + " left", DocumentStatus::ACTUAL, {});
    server.AddDocument(doc_id + 10, content + "right", DocumentStatus::ACTUAL, {-4, 0, 0, 4});
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT(found_docs.size() == 3);
    const Document &doc0 = found_docs[0];
    const Document &doc1 = found_docs[1];
    const Document &doc2 = found_docs[2];
    ASSERT(doc0.rating == 2);
    ASSERT(doc1.rating == 0);
    ASSERT(doc2.rating == 0);
}

void TestSortAndRelevanceAndSearch() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
    search_server.AddDocument(4, "пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT, {2, 3, 5, 1});
    search_server.AddDocument(5, "кот"s, DocumentStatus::REMOVED, {3, 2, 1, 8, -7});
    search_server.AddDocument(6, "пушистый кот"s, DocumentStatus::ACTUAL, {6, 5, 2});
    search_server.AddDocument(7, "ухоженный кот"s, DocumentStatus::ACTUAL, {0});
    {
        const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT(found_docs.size() == MAX_RESULT_DOCUMENT_COUNT); //Check limit
        const Document &doc0 = found_docs[0];
        const Document &doc1 = found_docs[1];
        const Document &doc2 = found_docs[2];
        const Document &doc3 = found_docs[3];
        const Document &doc4 = found_docs[4];
        /* check found id are right */
        ASSERT(doc0.id == 6);
        ASSERT(doc1.id == 1);
        ASSERT(doc2.id == 7);
        ASSERT(doc3.id == 2);
        ASSERT(doc4.id == 0);
        /* check found relevance is right */
        ASSERT(doc0.relevance - 0.634255 <= EPSILON);
        ASSERT(doc1.relevance - 0.562335 <= EPSILON);
        ASSERT(doc2.relevance - 0.490414 <= EPSILON);
        ASSERT(doc3.relevance - 0.173286 <= EPSILON);
        ASSERT(doc4.relevance - 0.071920 <= EPSILON);
        /* check sort order is right */
        ASSERT(doc0.relevance >= doc1.relevance);
        ASSERT(doc1.relevance >= doc2.relevance);
        ASSERT(doc2.relevance >= doc3.relevance);
        ASSERT(doc3.relevance >= doc4.relevance);
        /* check found rating is right */
        ASSERT(doc0.rating == 4);
        ASSERT(doc1.rating == 5);
        ASSERT(doc2.rating == 0);
        ASSERT(doc3.rating == -1);
        ASSERT(doc4.rating == 2);
    }
    /* check status filter work*/
    {

        const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == 3);
    }
    {
        const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == 4);
    }
    {
        const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == 5);
    }
    {
        const auto ds = DocumentStatus::IRRELEVANT;
        const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [ds](int document_id, DocumentStatus status, int rating) {
            return status == ds;
        });
        const Document &doc0 = found_docs[0];
        ASSERT(doc0.id == 4);
    }
}
/*
Разместите код остальных тестов здесь
*/

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestRating);
    RUN_TEST(TestSortAndRelevanceAndSearch);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}