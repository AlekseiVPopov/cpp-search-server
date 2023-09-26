#include "search_server.h"
#include "process_queries.h"
#include "test_example_functions.h"

#include <iostream>

using namespace std;

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
        TestSearchServer();

    return 0;
}