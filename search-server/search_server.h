#pragma once

#include "document.h"
#include "log_duration.h"
#include "concurrent_map.h"
#include "string_processing.h"

#include <map>
#include <set>
#include <list>
#include <cmath>
#include <deque>
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <exception>
#include <string_view>



const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAX_RELEVANCE_DIFFERENCE = 1e-6;
const int MAX_COUNTS = 24; // максимальное кол-во потоков выполенения


class SearchServer {
public:
	SearchServer() = default;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    
    explicit SearchServer(const std::string& stop_words_text);
    
    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
        return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
    }

    template <typename DocumentPredicate, typename Policy>
    std::vector<Document> FindTopDocuments(Policy policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    
        const auto& query = ParseQuery(raw_query);

        auto matched_documents = std::move(FindAllDocuments(policy, query, document_predicate));

        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < MAX_RELEVANCE_DIFFERENCE) {
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

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const;

	int GetDocumentCount() const;

	auto begin() const {
		return document_ids_.begin();
	}

	auto end() const {
		return document_ids_.end();
	}

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    
    std::set<std::string, std::less<>> GetStopWords() {
		return stop_words_;
	}

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy& policy, int document_id);
    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);
    
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

private:
	struct DocumentData {
		std::string doc_data;
		int rating;
		DocumentStatus status;
	};
    std::deque <std::string> storage_;
    
	std::map<std::string, std::map<int, double>> word_to_document_freqs_; // {id, {document, freqs}}.
	std::map<int, std::map<std::string_view, double>> words_document_freqs; // {document, {id, freqs}}.
    
    const std::set<std::string, std::less<>> stop_words_;
    
	std::map<int, DocumentData> documents_;
	std::list<int> document_ids_;
    
    bool IsStopWord(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);

	std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;
	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};
    
	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query {
		std::set<std::string_view> plus_words;
		std::set<std::string_view> minus_words;
	};
    
    Query ParseQuery(const std::string_view text) const;

	double ComputeWordInverseDocumentFreq(const std::string& word) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    
	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
    
	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
	std::map<int, double> document_to_relevance;

	for (const std::string_view word_view : query.plus_words) {
		std::string word(word_view);
		if (!word_to_document_freqs_.count(word) || word_to_document_freqs_.at(word).empty()) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			const auto& doc_data = documents_.at(document_id);
			if (document_predicate(document_id, doc_data.status, doc_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}

	for (const std::string_view word_view : query.minus_words) {
		std::string word(word_view);
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	matched_documents.reserve(document_to_relevance.size());
	for (const auto [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
	}

	return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
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
					return word_to_document_freqs_.count(word) && query.minus_words.count(word_view);
				}
			), words.end());

	ConcurrentMap<int, double> document_to_relevance_concurent(MAX_COUNTS);
	std::for_each(
			std::execution::par,
			words.begin(), words.end(),
			[this, document_predicate, &document_to_relevance_concurent] (std::string_view word_view) {
					std::string word(word_view);
					const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
					for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
						const auto& doc_data = documents_.at(document_id);
						if (document_predicate(document_id, doc_data.status, doc_data.rating)) {
							document_to_relevance_concurent[document_id].ref_to_value += term_freq * inverse_document_freq;
						}
					}
				}
			);

	const auto& document_to_relevance_ordinary = document_to_relevance_concurent.BuildOrdinaryMap();
	std::vector<Document> matched_documents(document_to_relevance_ordinary.size());
	transform(
			std::execution::par,
			document_to_relevance_ordinary.begin(), document_to_relevance_ordinary.end(),
			matched_documents.begin(),
			[this](const auto& item) {
					const int document_id = item.first;
					const double relevance = item.second;
					return Document{document_id, relevance, documents_.at(document_id).rating};
				}
			);

	return matched_documents;
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("There are invalid stop words");
    }
}
