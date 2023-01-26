#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <deque>

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer &search_server);

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template<typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate) {
        auto result = this->sserv_->FindTopDocuments(raw_query, document_predicate);
        if (result.empty()) {
            ++this->empty_num_;
        }
        if (requests_.size() == min_in_day_) {
            if (requests_.front().is_empty) {
                --this->empty_num_;
            }
            requests_.pop_front();
        }
        requests_.push_back({raw_query, result, result.empty()});
        return result;
        // напишите реализацию
    }

    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string &raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::string raw_query;
        std::vector<Document> result;
        bool is_empty = true;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer *sserv_;
    int empty_num_ = 0;
};