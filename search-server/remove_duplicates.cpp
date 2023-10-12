#include <set>
#include <vector>
#include <iostream>

#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<std::set<std::string, std::less<>>> unique_docs;
    std::vector<int> duplicates;
    for (const int id : search_server) {
        const std::map<std::string, double> word_freqs = search_server.GetWordFrequencies(id);
        std::set<std::string, std::less<>> words;
        transform(word_freqs.begin(), word_freqs.end(), inserter(words, words.begin()), [](const std::pair<std::string, double> word) {
            return word.first;
        });

        if (unique_docs.count(words) == 0) {
            unique_docs.insert(words);
        } else {
            duplicates.push_back(id);
        }
    }

    for (const int& document_id : duplicates)
    {
        std::cout << "Found duplicate document id " << document_id << '\n';
        search_server.RemoveDocument(document_id);
    }
}
