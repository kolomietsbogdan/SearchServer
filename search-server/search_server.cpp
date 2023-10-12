#include "search_server.h"


SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(
	SplitIntoWords(stop_words_text))
{}

SearchServer::SearchServer(const std::string_view stop_words_text) : SearchServer(
	SplitIntoWordsView(stop_words_text))
{}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw std::invalid_argument("ID document is invalid"); // проверка на id < 0 и на повторяющийся id
	}

	document_ids_.push_back(document_id);

	const auto& [document_id_to_data, _] = documents_.emplace(document_id, SearchServer::DocumentData{ std::string(document), ComputeAverageRating(ratings), status });
	const auto words = SplitIntoWordsNoStop(document_id_to_data->second.doc_data);

	const double inv_word_count = 1.0 / words.size();
	for (const std::string_view word : words) {
		word_to_document_freqs_[std::string(word)][document_id] += inv_word_count;
		words_document_freqs[document_id][word] += inv_word_count;
	}

}
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(std::execution::seq, raw_query, status);
}
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
	return FindTopDocuments(std::execution::seq, raw_query);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const {
	return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(std::execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const {
	return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
	return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const {
	if (!count(document_ids_.begin(), document_ids_.end(), document_id)) {
		throw std::out_of_range("Document ID is out of range: " + document_id);
	}
	auto& status = documents_.at(document_id).status;
	const auto query = ParseQuery(raw_query);

	std::vector<std::string_view> matched_words;

	for (const std::string_view word_view : query.minus_words) {
		std::string word(word_view);
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			return make_tuple(matched_words, status);
		}
	}
	for (const std::string_view word_view : query.plus_words) {
		std::string word(word_view);
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(word).count(document_id)) {
			matched_words.push_back(word_view);
		}
	}
	return make_tuple(matched_words, status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const {
	if (!count(document_ids_.begin(), document_ids_.end(), document_id)) {
		throw std::out_of_range("Document ID is out of range: " + document_id);
	}
	auto& status = documents_.at(document_id).status;
	const auto query = ParseQuery(raw_query);

	std::vector<std::string_view> matched_words;

	const auto word_checker = [this, document_id](std::string_view word_view) {
		std::string word(word_view);
		const auto& item = word_to_document_freqs_.find(word);
		return item != word_to_document_freqs_.end() && item->second.count(document_id);
		};

	if (any_of(query.minus_words.begin(), query.minus_words.end(), word_checker)) {
		return make_tuple(matched_words, status);
	}

	matched_words.resize(query.plus_words.size());
	auto matched_words_end = copy_if(
		std::execution::par,
		query.plus_words.begin(), query.plus_words.end(),
		matched_words.begin(),
		word_checker
	);
	matched_words.erase(matched_words_end, matched_words.end());

	return make_tuple(matched_words, status);
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

bool SearchServer::IsStopWord(const std::string_view word) const {
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
	return std::none_of(word.begin(), word.end(), [](char c) { // Валидное слово не должно содержать спецсимволов
		return c >= '\0' && c < ' ';
		});
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
	std::vector<std::string_view> words;
	for (const std::string_view word : SplitIntoWordsView(text)) {
		if (!IsValidWord(word)) {
			throw std::invalid_argument(std::string("The word ") + std::string(word) + std::string(" is invalid"));
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
	int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view text) const {
	if (text.empty()) {
		throw std::invalid_argument("Your query is empty");
	}
	std::string_view word = text;
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw std::invalid_argument("This query word is invalid");
	}

	return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
	SearchServer::Query result;
	for (std::string_view word : SplitIntoWordsView(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.emplace(query_word.data);
			}
			else {
				result.plus_words.emplace(query_word.data);
			}
		}
	}
	return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
	static const std::map<std::string_view, double> empty_map;

	if (!words_document_freqs.count(document_id)) {
		return empty_map;
	}

	return words_document_freqs.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
	SearchServer::RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
	// если пытаемся удалить ID, который не добавляли на сервер
	if (!count(document_ids_.begin(), document_ids_.end(), document_id)) {
		throw std::invalid_argument(std::string{ "ID is invalid" });
	}

	for (auto& [word, freqs] : words_document_freqs.at(document_id)) {
		auto& document_freqs = word_to_document_freqs_.at(std::string(word));
		document_freqs.erase(document_id);
	}

	words_document_freqs.erase(document_id);
	documents_.erase(document_id);
	document_ids_.remove(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
	// если пытаемся удалить ID, который не добавляли на сервер
	if (!count(document_ids_.begin(), document_ids_.end(), document_id)) {
		throw std::invalid_argument(std::string{ "ID is invalid" });
	}

	const auto& word_freqs = words_document_freqs.at(document_id);
	std::vector<std::string_view> words(word_freqs.size());
	transform(
		std::execution::par,
		word_freqs.begin(), word_freqs.end(),
		words.begin(),
		[](const auto& item) { return item.first; }
	);

	for_each(std::execution::par, words.begin(), words.end(), [this, document_id](std::string_view word)
		{ word_to_document_freqs_.at(std::string(word)).erase(document_id);
		});

	words_document_freqs.erase(document_id);
	documents_.erase(document_id);
	document_ids_.remove(document_id);
}
