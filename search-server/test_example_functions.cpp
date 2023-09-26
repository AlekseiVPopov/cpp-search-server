#include "test_example_functions.h"

using namespace std;


void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
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

void PrintMatchDocumentResult(int document_id, const vector<string_view> &words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string_view &word: words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer &search_server, int document_id, const string_view &document, DocumentStatus status,
                 const vector<int> &ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const invalid_argument &e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer &search_server, const string_view &raw_query) {
    LOG_DURATION_STREAM("Operation time", cout);
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document &document: search_server.FindTopDocuments(raw_query)) {
            cout << document << endl;
        }
    } catch (const invalid_argument &e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer &search_server, const string_view &query) {
    LOG_DURATION_STREAM("Operation time", cout);
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (const auto &document_id: search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }

    } catch (const invalid_argument &e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}


// -------- Начало модульных тестов поисковой системы ----------

void TestAddDocument() {
    struct TestContent {
        int id;
        string content;
        DocumentStatus status;
        vector<int> ratings;
    };

    const TestContent doc_0 = {
            10,
            "cat and chicken cat"s,
            DocumentStatus::ACTUAL,
            {1, 2, 3} // mean = 2;
    };

    SearchServer server;
    server.AddDocument(doc_0.id, doc_0.content, doc_0.status, doc_0.ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);

    ASSERT_EQUAL(server.FindTopDocuments("cat"s).size(), 1u);
    ASSERT_EQUAL(server.FindTopDocuments("cat"s)[0].id, doc_0.id);
    ASSERT(server.FindTopDocuments("SomeWord"s).empty());
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto &found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        /*for (const auto word : server.GetStopWords()) {
            cout << "Stop-word:" << word << endl;
        } cout << endl;*/
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestMinusWordsSearching() {
    struct TestContent {
        int id;
        string content;
        DocumentStatus status;
        vector<int> ratings;
    };

    const TestContent doc_0 = {
            10,
            "cat and chicken cat"s,
            DocumentStatus::ACTUAL,
            {1, 2, 3} // mean = 2;
    };
    const TestContent doc_1 = {
            15,
            "dog and chicken"s,
            DocumentStatus::ACTUAL,
            {1, 2, 3} // mean = 2;
    };

    SearchServer server;
    server.AddDocument(doc_0.id, doc_0.content, doc_0.status, doc_0.ratings);
    server.AddDocument(doc_1.id, doc_1.content, doc_1.status, doc_1.ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);

    {
        const auto &found_docs = server.FindTopDocuments("-SomeWord cat dog"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
    }

    {
        const auto &found_docs = server.FindTopDocuments("-cat dog"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_1.id);
    }

    {
        const auto &found_docs = server.FindTopDocuments("-cat -dog"s);
        ASSERT_HINT(found_docs.empty(), "Documents with minus-words shouldn't be found");
    }
}

void TestMatchDocumentFromAddedDocumentContent() {
    struct TestContent {
        int id;
        string content;
        DocumentStatus status;
        vector<int> ratings;
    };

    const TestContent doc_2 = {
            2,
            "cat in the big cat city"s,
            DocumentStatus::BANNED,
            {1, 2, 3} // mean = 2;
    };


    // случай, когда в запросе нет минус-слов
    {
        SearchServer server;
        server.AddDocument(doc_2.id, doc_2.content, DocumentStatus::BANNED, doc_2.ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto &[words, status] = server.MatchDocument("cat big"s, doc_2.id);
        ASSERT_EQUAL(words.size(), 2u);

        vector<string_view> ordered_words = {"big"sv, "cat"sv};
        ASSERT_EQUAL(ordered_words, words);

        ASSERT_EQUAL_HINT(static_cast<int>(status), static_cast<int>(DocumentStatus::BANNED),
                          "Matched document should have changed its document_status."s);
    }

    // случай, когда в запросе есть минус-слово, которое есть в документе
    {
        SearchServer server;
        server.AddDocument(doc_2.id, doc_2.content, DocumentStatus::BANNED, doc_2.ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto &[words, status] = server.MatchDocument("-cat big"s, doc_2.id);
        ASSERT(words.empty());

        ASSERT_EQUAL_HINT(static_cast<int>(status), static_cast<int>(DocumentStatus::BANNED),
                          "Matched document should have changed its document_status."s);
    }
}

void TestMatchDocumentWithExecutionPolicy() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (const string &text: {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
    }
            ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const string query = "curly and funny -not"s;

    {
        const auto [words, status] = search_server.MatchDocument(query, 1);
        ASSERT_EQUAL(words.size(), 1u);
        //cout << words.size() << " words for document 1"s << endl;
        // 1 words for document 1
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
        ASSERT_EQUAL(words.size(), 2u);
        //cout << words.size() << " words for document 2"s << endl;
        // 2 words for document 2
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
        ASSERT_EQUAL(words.size(), 0u);
        //cout << words.size() << " words for document 3"s << endl;
        // 0 words for document 3
    }
}

void TestFindTopDocumentsSorting() {
    struct TestContent {
        int id;
        string content;
        DocumentStatus status;
        vector<int> ratings;
    };

    const TestContent doc_0 = {
            10,
            "cat and cat chicken"s,
            DocumentStatus::ACTUAL,
            {1, 2, 3} // mean = 2;
    };

    const TestContent doc_1 = {
            15,
            "dog and chicken nagets"s,
            DocumentStatus::ACTUAL,
            {1, 2, 3} // mean = 2;
    };

    const TestContent doc_2 = {
            20,
            "murloc ate my cat"s,
            DocumentStatus::ACTUAL,
            {1, 2, 9} // mean = 4;
    };

    SearchServer server;
    server.AddDocument(doc_0.id, doc_0.content, doc_0.status, doc_0.ratings);
    server.AddDocument(doc_1.id, doc_1.content, doc_1.status, doc_1.ratings);
    server.AddDocument(doc_2.id, doc_2.content, doc_2.status, doc_2.ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 3);

    const auto &found_docs = server.FindTopDocuments("cat chicken"s);
    ASSERT_EQUAL(found_docs.size(), 3u);

    const double EPSILON = 1e-6;
    for (int i = 1; i < static_cast<int>(found_docs.size()); ++i) {
        ASSERT_HINT((found_docs[i - 1].relevance - found_docs[i].relevance > EPSILON) || (
                abs(found_docs[i - 1].relevance - found_docs[i].relevance) < EPSILON &&
                found_docs[i - 1].rating >= found_docs[i].rating),
                    "Documents should be descending sorted by relevance primary, by average rating secondly."s
        );
    }
}

void TestComputeAverageRating() {
    const int doc_id = 42;
    const string content = "cat in the city"s;

    // случай, когда результат целочисленный
    {
        const vector<int> ratings = {1, 2, 3}; // mean = 2
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto &found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs[0].rating, 2);
    }

    // случай, когда средний рейтинг нецелочисленный, но вернутся должен целочисленный результат
    {
        const vector<int> ratings = {2, 5}; // mean = 3
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto &found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_docs[0].rating, 3, "Average rating should be counted by integer division."s);
    }

    // случай, когда есть внутри отрицательные рейтинги
    {
        const vector<int> ratings = {-2, -4};
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto &found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs[0].rating, -3);
    }
}

void TestFindTopDocumentsWithPredicate() {
    struct TestContent {
        int id;
        string content;
        DocumentStatus status;
        vector<int> ratings;
    };

    const TestContent doc_0 = {
            10,
            "cat and chicken cat"s,
            DocumentStatus::IRRELEVANT,
            {1, 2, 9} // mean = 4;
    };

    const TestContent doc_1 = {
            15,
            "dog and chicken"s,
            DocumentStatus::BANNED,
            {1, 2, 3} // mean = 2;
    };

    SearchServer server;
    server.AddDocument(doc_0.id, doc_0.content, doc_0.status, doc_0.ratings);
    server.AddDocument(doc_1.id, doc_1.content, doc_1.status, doc_1.ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);

    // Проверка на вывод документов с четным id
    {
        const auto &found_docs = server.FindTopDocuments("chicken"s,
                                                         [](int document_id, [[maybe_unused]] DocumentStatus status,
                                                            [[maybe_unused]] int rating) {
                                                             return document_id % 2 == 0;
                                                         });
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_0.id);
    }

    // Проверка на вывод документов с рейтингом равным 2
    {
        const auto &found_docs = server.FindTopDocuments("chicken"s,
                                                         []([[maybe_unused]] int document_id,
                                                            [[maybe_unused]] DocumentStatus status,
                                                            int rating) { return rating == 2; });
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_1.id);
    }

    // Проверка на вывод документов со статусом BANNED
    {
        const auto &found_docs = server.FindTopDocuments("chicken"s,
                                                         []([[maybe_unused]] int document_id, DocumentStatus status,
                                                            [[maybe_unused]] int rating) {
                                                             return status == DocumentStatus::BANNED;
                                                         });
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_1.id);
    }

    // Проверка на вывод документов со статусом ACTUAL
    {
        const auto &found_docs = server.FindTopDocuments("chicken"s,
                                                         []([[maybe_unused]] int document_id, DocumentStatus status,
                                                            [[maybe_unused]] int rating) {
                                                             return status == DocumentStatus::ACTUAL;
                                                         });
        ASSERT(found_docs.empty());
    }

    // Predicate FALSE
    {
        const auto &found_docs = server.FindTopDocuments("chicken"s,
                                                         []([[maybe_unused]] int document_id,
                                                            [[maybe_unused]] DocumentStatus status,
                                                            [[maybe_unused]] int rating) { return false; });
        ASSERT(found_docs.empty());
    }

    // Predicate TRUE
    {
        const auto &found_docs = server.FindTopDocuments("chicken"s,
                                                         []([[maybe_unused]] int document_id,
                                                            [[maybe_unused]] DocumentStatus status,
                                                            [[maybe_unused]] int rating) { return true; });
        ASSERT(found_docs.size() == 2u);
    }
}

void TestFindTopDocumentsWithStatus() {
    struct TestContent {
        int id;
        string content;
        DocumentStatus status;
        vector<int> ratings;
    };

    const TestContent doc_0 = {
            0,
            "cat in the big cat city"s,
            DocumentStatus::ACTUAL,
            {1, 2, 3} // mean = 2;
    };
    const TestContent doc_1 = {
            1,
            "cat in the big cat city"s,
            DocumentStatus::IRRELEVANT,
            {1, 2, 3} // mean = 2;
    };
    const TestContent doc_2 = {
            2,
            "cat in the big cat city"s,
            DocumentStatus::BANNED,
            {1, 2, 3} // mean = 2;
    };
    const TestContent doc_3 = {
            3,
            "cat in the big cat city"s,
            DocumentStatus::REMOVED,
            {1, 2, 3} // mean = 2;
    };

    {
        SearchServer server;
        server.AddDocument(doc_1.id, doc_1.content, doc_1.status, doc_1.ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);

        ASSERT(server.FindTopDocuments("big cat"s).empty());

        ASSERT(server.FindTopDocuments("big cat"s, DocumentStatus::ACTUAL).empty());
        ASSERT(server.FindTopDocuments("big cat"s, DocumentStatus::BANNED).empty());
        ASSERT(server.FindTopDocuments("big cat"s, DocumentStatus::REMOVED).empty());

        ASSERT_EQUAL(server.FindTopDocuments("big cat"s, DocumentStatus::IRRELEVANT).size(), 1u);
        ASSERT_EQUAL(server.FindTopDocuments("big cat"s, DocumentStatus::IRRELEVANT)[0].id, doc_1.id);
    }

    SearchServer server;
    server.AddDocument(doc_0.id, doc_0.content, doc_0.status, doc_0.ratings);
    server.AddDocument(doc_1.id, doc_1.content, doc_1.status, doc_1.ratings);
    server.AddDocument(doc_2.id, doc_2.content, doc_2.status, doc_2.ratings);
    server.AddDocument(doc_3.id, doc_3.content, doc_3.status, doc_3.ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 4);

    {
        const auto &found_docs = server.FindTopDocuments("big cat"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_0.id);
    }

    {
        const auto &found_docs = server.FindTopDocuments("big cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_0.id);
    }

    {
        const auto &found_docs = server.FindTopDocuments("big cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_1.id);
    }

    {
        const auto &found_docs = server.FindTopDocuments("big cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_2.id);
    }

    {
        const auto &found_docs = server.FindTopDocuments("big cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_3.id);
    }
}

void TestRelevanceCalculating() {
    SearchServer server("и в на"s);

    string raw_query = "пушистый ухоженный кот"s;
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});
    ASSERT_EQUAL(server.GetDocumentCount(), 3);
    vector<double> relevances = {.650672, .274653, .101366};

    const auto &found_docs = server.FindTopDocuments(raw_query);
    const double EPSILON = 1e-6;

    for (int i = 0; i < static_cast<int>(found_docs.size()); i++) {
        ASSERT(abs(found_docs[i].relevance - relevances[i]) < EPSILON);
    }
}

void TestBeginEndSearchServer() {
    SearchServer server;

    server.AddDocument(16, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(3, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});

    vector<int> document_ids = {1, 3, 16};
    vector<int> result;

    for (const auto id: server) {
        result.push_back(id);
    }

    ASSERT_EQUAL(result, document_ids);
}

void TestGetWordFrequencies() {
    SearchServer server;

    server.AddDocument(16, "белый кот модный кот"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});

    map<string_view, double> empty_map;
    map<string_view, double> doc_id_16 = {{"белый"sv,  0.25},
                                          {"кот"sv,    0.5},
                                          {"модный"sv, 0.25}};
    map<string_view, double> doc_id_1 = {{"кот"sv,      0.25},
                                         {"пушистый"sv, 0.5},
                                         {"хвост"sv,    0.25}};

    ASSERT_EQUAL(server.GetWordFrequencies(16), doc_id_16);
    ASSERT_EQUAL(server.GetWordFrequencies(1), doc_id_1);
    ASSERT_EQUAL_HINT(server.GetWordFrequencies(3), empty_map, "Should return empty map for non-existent ID");
}

void TestRemoveDocument() {
    SearchServer server;

    server.AddDocument(16, "белый кот модный кот"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(3, "ухоженный кот выразительные глаза"s, DocumentStatus::ACTUAL, {0});

    server.RemoveDocument(1);

    {
        string raw_query = "пушистый";
        const auto &found_docs = server.FindTopDocuments(raw_query);

        ASSERT_EQUAL(found_docs.size(), 0u);
    }

    {
        string raw_query = "пушистый кот";
        const auto &found_docs = server.FindTopDocuments(raw_query);

        ASSERT_EQUAL(found_docs.size(), 2u);
    }
}

// Проверка на равенство чисел
bool InTheVicinity(const double d1, const double d2, const double delta) {
    return abs(d1 - d2) < delta;
}

// Проверка метода удаления документа
void TestRemoveDocument2() {
    SearchServer server("and with as"s);

    AddDocument(server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(server, 4, "kind dog bite fat rat"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(server, 6, "fluffy snake or cat"s, DocumentStatus::ACTUAL, {1, 2});

    AddDocument(server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    // nasty tf = 1/4
    AddDocument(server, 3, "angry rat with black hat"s, DocumentStatus::ACTUAL, {1, 2});
    // black tf = 1/4
    AddDocument(server, 5, "fat fat cat"s, DocumentStatus::ACTUAL, {1, 2});
    // cat tf = 1/3
    AddDocument(server, 7, "sharp as hedgehog"s, DocumentStatus::ACTUAL, {1, 2});
    // sharp tf = 1/2

    // kind - doesn't occur
    // nasty - log(4)
    // black - log(4)
    // cat - log(4)
    // sharp - log(4)

    // 7 - 1/2 * log(4) = 0.6931471805599453
    // 5 - 1/3 * log(4) = 0.46209812037329684
    // 1 - 1/4 * log(4) = 0.34657359027997264
    // 3 - 1/4 * log(4) = 0.34657359027997264

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 7, "Nothing has been removed, yet!"s);
    server.RemoveDocument(0);
    server.RemoveDocument(8);

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 7, "Nothing has been removed, yet!"s);
    {
        const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
        for (const auto doc: docs) {
            cerr << doc.id << " ";
        }
        cerr << endl;
        ASSERT_EQUAL_HINT(docs.size(), 5u, "All documents must be found"s); // 5 потому что MAX_DOCUMENT_COUNT = 5
    }

    server.RemoveDocument(2);
    ASSERT_EQUAL(server.GetDocumentCount(), 6);
    {
        const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
        for (const auto doc: docs) {
            cerr << doc.id << " ";
        }
        cerr << endl;
        ASSERT_EQUAL_HINT(docs.size(), 5u, "All documents must be found"s);
    }

    server.RemoveDocument(4);
    ASSERT_EQUAL(server.GetDocumentCount(), 5);
    {
        const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
        for (const auto doc: docs) {
            cerr << doc.id << " ";
        }
        cerr << endl;
        ASSERT_EQUAL_HINT(docs.size(), 5u, "All documents must be found"s);
    }

    server.RemoveDocument(6);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 4, "3 documents have been removed"s);

    // Check document_data_
    ASSERT_HINT(server.GetWordFrequencies(2).empty(), "Server doesn't has id = 2, result must be empty"s);
    ASSERT_HINT(server.GetWordFrequencies(4).empty(), "Server doesn't has id = 4, result must be empty"s);
    ASSERT_HINT(server.GetWordFrequencies(6).empty(), "Server doesn't has id = 6, result must be empty"s);

    // Check document_ids_
    for (int id: server) {
        ASSERT_HINT(id % 2 == 1, "Only odd ids has been left"s);
    }

    // Check word_to_document_freqs_
    const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
    for (const auto doc: docs) {
        cerr << doc.id << " ";
    }
    cerr << endl;
    ASSERT_EQUAL_HINT(docs.size(), 4u, "All documents must be found"s);

    const double delta = 1e-6;
    ASSERT_EQUAL_HINT(docs.at(0).id, 7, "Max relevance has doc with id 7"s);
    ASSERT_HINT(InTheVicinity(docs.at(0).relevance, 0.6931471805599453, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(1).id, 5, "Second relevance has doc with id 5"s);
    ASSERT_HINT(InTheVicinity(docs.at(1).relevance, 0.46209812037329684, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(2).id, 1, "Third relevance has doc with id 1"s);
    ASSERT_HINT(InTheVicinity(docs.at(2).relevance, 0.34657359027997264, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(3).id, 3, "Forth relevance has doc with id 3"s);
    ASSERT_HINT(InTheVicinity(docs.at(3).relevance, 0.34657359027997264, delta), "Wrong relevance"s);
}

void TestRemoveDocumentWithExecutionPolicy() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (

        const string &text: {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
    }
            ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const string query = "curly and funny"s;

    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 4u);
    // однопоточная версия
    search_server.RemoveDocument(5);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 4);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 3u);
    // однопоточная версия
    search_server.RemoveDocument(execution::seq, 1);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 3);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 2u);
    // многопоточная версия
    search_server.RemoveDocument(execution::par, 2);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 2);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 1u);
}

void TestRemoveDuplicate() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    ASSERT_EQUAL(search_server.GetDocumentCount(), 9);

    cerr << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
    RemoveDuplicates(search_server);
    cerr << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;

    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);

    RemoveDuplicates(search_server);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);
}

void TestProcessQueries() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (const string &text: {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
    }
            ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
    };

    id = 0;
    vector<size_t> result{3, 5, 2};
    for (
        const auto &documents: ProcessQueries(search_server, queries)
            ) {
        //cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
        ASSERT_EQUAL(documents.size(), result[id++]);
    }
}

void TestProcessQueriesJoined() {
    SearchServer search_server("and with"s);
    int id = 0;
    for (const string &text: {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
    }) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
    };
    vector<Document> result{
            {1, 0.183492,  0},
            {5, 0.183492,  0},
            {4, 0.167358,  0},
            {3, 0.743945,  0},
            {1, 0.311199,  0},
            {2, 0.183492,  0},
            {5, 0.127706,  0},
            {4, 0.0557859, 0},
            {2, 0.458145,  0},
            {5, 0.458145,  0}
    };
    const double EPSILON = 1e-6;
    id = 0;
    for (const Document &document: ProcessQueriesJoined(search_server, queries)) {
        //cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
        ASSERT_EQUAL(document.id, result[id].id);
        ASSERT(abs(document.relevance - result[id].relevance) < EPSILON);
        ++id;
    }
}

void TestFindTopDocumentParallel() {
    SearchServer search_server("and with"s);
    const auto text = {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
    };
    int id = 0;
    for (const auto &str: text) {
        search_server.AddDocument(++id, str, DocumentStatus::ACTUAL, {1, 2});
    }

    const double EPSILON = 1e-6;
    // only query
    {
        vector<Document> result{
                {2, 0.866434, 1},
                {4, 0.231049, 1},
                {1, 0.173287, 1},
                {3, 0.173287, 1}
        };
        int i = 0;
        for (const Document &document: search_server.FindTopDocuments(execution::par, "curly nasty cat"s)) {
            ASSERT_EQUAL(document.id, result[i].id);
            ASSERT(abs(document.relevance - result[i].relevance) < EPSILON);
            ASSERT_EQUAL(document.rating, result[i].rating);
            ++i;
        }
    }

    // query and status
    {
        vector<Document> result;
        ASSERT(search_server.FindTopDocuments(execution::par, "curly nasty cat"s, DocumentStatus::BANNED).empty());
    }

    // query, status, predicate
    {
        vector<Document> result{
                {2, 0.866434, 1},
                {4, 0.231049, 1}
        };
        int i = 0;
        for (const Document &document: search_server.FindTopDocuments(execution::par, "curly nasty cat"s,
                                                                      [](int document_id, DocumentStatus status,
                                                                         int rating) {
                                                                          return document_id % 2 == 0;
                                                                      })) {
            ASSERT_EQUAL(document.id, result[i].id);
            ASSERT(abs(document.relevance - result[i].relevance) < EPSILON);
            ASSERT_EQUAL(document.rating, result[i].rating);
            ++i;
        }
    }
}

void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWordsSearching);
    RUN_TEST(TestMatchDocumentFromAddedDocumentContent);
    RUN_TEST(TestMatchDocumentWithExecutionPolicy);
    RUN_TEST(TestFindTopDocumentsSorting);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestFindTopDocumentsWithPredicate);
    RUN_TEST(TestFindTopDocumentsWithStatus);
    RUN_TEST(TestRelevanceCalculating);
    RUN_TEST(TestBeginEndSearchServer);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestRemoveDocument2);
    RUN_TEST(TestRemoveDocumentWithExecutionPolicy);
    RUN_TEST(TestRemoveDuplicate);
    RUN_TEST(TestProcessQueries);
    RUN_TEST(TestProcessQueriesJoined);
    RUN_TEST(TestFindTopDocumentParallel);
}

// --------- Окончание модульных тестов поисковой системы -----------
