#include "document.h"
#include "paginator.h"
#include <iostream>

using namespace std;

ostream& operator<<(ostream& output, Document document) {
    output << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;
    return output;
}
