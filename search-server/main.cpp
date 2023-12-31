#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = numeric_limits<double>::epsilon();

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
            for (string letter : stop_words) {
                if (!IsValidWord(letter)) {
                    throw invalid_argument("недопустимые символы в стоп-словах"s);
                }
            }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
    }

    void AddDocument(int document_id, const string& document, 
                     DocumentStatus status, const vector<int>& ratings) {
        if(document_id<0) {
            throw invalid_argument("отрицательный id документа"s);
        }
        
        if(count(check_id.begin(), check_id.end(), document_id)!=0) {
            throw invalid_argument("документ с таким id уже существует"s);
        }
        
        check_id.push_back(document_id);
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }
    
    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }
    
    int GetDocumentId(int index) const {
        if ( index < 0 || index > static_cast<int>(check_id.size()) ) {
            throw out_of_range("индекс документа выходит за пределы допустимого диапазона"s);
        }
        
        return check_id.at(index);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto result = FindAllDocuments(query, document_predicate);
        
        sort(result.begin(), result.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 }
                 return lhs.relevance > rhs.relevance;
             }
            );
        
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        
        return result;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, 
                                [status](int document_id, 
                                         DocumentStatus document_status, 
                                         int rating) {
                                    return document_status == status;
                                }
                               );
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }


    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
       
        if(document_id < 0) {
            throw invalid_argument("отрицательное id документа"s);
        }
        
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        
        return tuple(matched_words, documents_.at(document_id).status);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> check_id;

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        if (!IsValidWord(text) || IsStopWord(text)) {
            throw invalid_argument("недопустимые символы в документе"s);
        }
        
        vector<string> words;
        
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word) && IsValidWord(text)) {
                words.push_back(word);
            }
        }
        
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string word) const {
        if (!IsValidWord(word)) {
            throw invalid_argument("неправильное слово"s);
        }
        
        for (int i = 0; i < word.size(); ++i) {
            if (word[i] == '-' && word[i + 1] == '-') {
                throw invalid_argument("неправильное слово"s);
            }
            
            if (word[i] == '-' && i + 1 == word.size()) {
                throw invalid_argument("неправильное слово"s);
            }
            
            if (word[i] == '-' && word.size() == 1) {
                throw invalid_argument("неправильное слово"s);
            }
        }
        
        bool is_minus = false;
        // Word shouldn't be empty
        if (word[0] == '-') {
            is_minus = true;
            word = word.substr(1);
        }
        
        return {word, is_minus, IsStopWord(word)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            
            if(IsValidWord(query_word.data)) {
                if (!query_word.is_stop) {
                    if (query_word.is_minus) {
                        query.minus_words.insert(query_word.data);
                    } else {
                        query.plus_words.insert(query_word.data);
                    }
                }
            } else {
                query.minus_words.clear();
                query.plus_words.clear();
            }
        }
        
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating}
            );
        }
        
        return matched_documents;
    }
    
};
/*
// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SearchServer search_server("и в на"s);
    // Явно игнорируем результат метода AddDocument, чтобы избежать предупреждения
    // о неиспользуемом результате его вызова
    (void) search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    if (!search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2})) {
        cout << "Документ не был добавлен, так как его id совпадает с уже имеющимся"s << endl;
    }
    if (!search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2})) {
        cout << "Документ не был добавлен, так как его id отрицательный"s << endl;
    }
    if (!search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2})) {
        cout << "Документ не был добавлен, так как содержит спецсимволы"s << endl;
    }
    if (const auto documents = search_server.FindTopDocuments("--пушистый"s)) {
        for (const Document& document : *documents) {
            PrintDocument(document);
        }
    } else {
        cout << "Ошибка в поисковом запросе"s << endl;
    }
}
*/
