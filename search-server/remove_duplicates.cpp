#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    std::map<std::string, double> freq_map;
    std::vector<std::pair<int, std::vector<std::string>>> all_document_struct;

    for (const int document_id: search_server) {
        std::vector<std::string> key_words;  // только слова документа без id
        std::map<std::string, double> id_freq = search_server.GetWordFrequencies(document_id);
        for (std::map<std::string, double>::iterator it = id_freq.begin(); it != id_freq.end(); ++it) {
            key_words.push_back(it->first);
        }
        all_document_struct.push_back(
                std::make_pair(document_id, key_words));
    }

    for (auto i = 0; i < (int) all_document_struct.size() - 1; ++i) {
        for (auto j = i + 1; j < (int) all_document_struct.size(); ++j) {
            if (all_document_struct[i].second == all_document_struct[j].second) {
                std::cout << "Found duplicate document id " << all_document_struct[j].first << std::endl;
                search_server.RemoveDocument(all_document_struct[j].first);
                all_document_struct.erase(all_document_struct.begin() + j);
                j--;
            }
        }
    }
}