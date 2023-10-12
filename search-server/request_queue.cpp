#include "request_queue.h"

int RequestQueue::GetNoResultRequests() const {
    return minute_in_query;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    auto result = server_.FindTopDocuments(raw_query, status);
    if (result.empty()) {
        ++minute_in_query; // кол-во пустых запросов 
    }
    requests_.push_back({ raw_query,result }); // requests_.size()-кол-во всех запросов 
    while (requests_.size() > min_in_day_) {
        requests_.pop_front();
        --minute_in_query;
    }
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    auto result = server_.FindTopDocuments(raw_query);
    if (result.empty()) {
        ++minute_in_query; // кол-во пустых запросов 
    }
    requests_.push_back({ raw_query,result }); // requests_.size()-кол-во всех запросов 
    while (requests_.size() > min_in_day_) {
        requests_.pop_front();
        --minute_in_query;
    }
    return result;
}
