#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    std::vector<int> duplicates_ids;
    std::set<std::set<std::string_view>> all_document_struct;

    for (const int document_id: search_server) {
        std::set<std::string_view> key_words;
        std::map<std::string_view, double> id_freq = search_server.GetWordFrequencies(document_id);
        for (auto & it : id_freq) {
            key_words.insert(it.first);
        }
        if (all_document_struct.count(key_words) == 0) {
            all_document_struct.insert(key_words);
        } else {
            std::cout << "Found duplicate document id " << document_id << std::endl;
            duplicates_ids.push_back(document_id);
        }
    }

    for (auto document_id : duplicates_ids) {
        search_server.RemoveDocument(document_id);
    }
}