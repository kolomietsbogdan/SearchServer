#pragma once
#include "string_processing.h"
#include "document.h"


template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator it_begin, Iterator it_end) :
        it_begin_(it_begin), it_end_(it_end) {}


    Iterator begin() {
        return it_begin_;
    }

    Iterator end() {
        return it_end_;
    }

    int size() {
        return distance(it_begin_, it_end_);
    }

private:
    Iterator it_begin_;
    Iterator it_end_;

};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator it_content_begin, Iterator it_content_end, size_t content_size) {
        if (it_content_begin == it_content_end || content_size == 0) return;
        Iterator it_begin = it_content_begin;
        for (Iterator it = it_content_begin; it < it_content_end; ++it) {
            Iterator it_end = next(it_begin, std::min(content_size, static_cast<size_t>(distance(it_begin, it_content_end))));;
            if (it_begin != it_end) {
                IteratorRange page(it_begin, it_end);
                it_begin = it_end;
                content_.push_back(page);
            }
        }
    }


    auto begin() const {
        return content_.begin();
    }

    auto end() const {
        return content_.end();
    }

    auto size() const {
        return content_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> content_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, IteratorRange<Iterator> page) {
    for (auto it = page.begin(); it < page.end(); ++it) {
        output << *it;
    }
    return output;
}

