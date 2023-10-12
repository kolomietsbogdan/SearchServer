#include "string_processing.h"

using namespace std;

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

vector<string_view> SplitIntoWordsView(string_view str) {
    vector<string_view> result;
    str.remove_prefix(min(str.size(), str.find_first_not_of(" ")));

    const std::string_view::size_type pos_end = str.npos;

    while (!str.empty()) {
        std::string_view::size_type space = str.find(' ');
        result.push_back(space == pos_end ? str.substr(0, pos_end) : str.substr(0, space));
        str.remove_prefix(min(str.size(), str.find_first_not_of(" ", space)));
    }

    return result;
}
