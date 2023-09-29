#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

//разбиваем строку на слова и заносим в вектор
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
    int id;
    double relevance;
};

class SearchServer {
public:
    //устанавливаем стоп слова
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    //добавляем в базу документ, его id и TF
    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        double size = 1.0 / words.size();
        for (const auto& word : words) {
            documents_freqs_[word][document_id] += size;
        }
        ++document_count_;
    }

    //возвращает вектор из топ 5 документов по релевантности
    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, map<int, double>> documents_freqs_;
    set<string> stop_words_;
    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    //разбивает строку запроса на слова и возвращает вектор этих слов без стоп-слов
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    //проверяет является ли слово минус- или плюс-словом
    bool ParseQueryWord(string& word) const {
        return word[0] == '-';
    }

    double GetIDF(const string& word) const {
        return log(static_cast<double>(document_count_) / static_cast<double>(documents_freqs_.at(word).size()));
    }

    //добавляет минус- и плюс-слова, на основе вводимой пользователем строки
    Query ParseQuery(const string& text) const {
        Query query_words;
        for (string word : SplitIntoWordsNoStop(text)){
            if(ParseQueryWord(word)) {
                word = word.substr(1);
                if (!IsStopWord(word)){
                    query_words.minus_words.insert(word);
                }
            } else {
                query_words.plus_words.insert(word);
            } 
        }
        return query_words;
    }

    //возвращает вектор, состоящий из документов удовлетворяющих запросу
    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;
        map<int, double> document_to_relevance;
        
        //проходясь по плюс-словам, расчитываем и устанавливаем релевантность документов ранжированием TF-IDF
        for (const auto& word : query_words.plus_words) {
            double idf = 0.0;
            if (documents_freqs_.count(word)) {
                idf = GetIDF(word);
            } else {
                continue;
            }
 
            for (const auto& [id, tf] : documents_freqs_.at(word)) {
                document_to_relevance[id] += idf * tf;
            }
        }
        
        //удаляем из сохраненных все документы, содержащие хотя бы одно минус-слово
        for (const auto& word : query_words.minus_words) {
            if (documents_freqs_.count(word)) {
                for (const auto& [id, idf] : documents_freqs_.at(word)) {
                    document_to_relevance.erase(id);
                }
            }
        }
        
        //заносим ранее полученные данные в вектор
        for (auto& [id, relevance] : document_to_relevance) {
            matched_documents.push_back({id, relevance});
        }
        
        return matched_documents;
    }
};

/* функция с алгоритмом создания поискового сервера - считывает и устанавливает стоп-слова, 
считывает желаемое кол-во документов, считывает и заносит это кол-во документов в базу данных */
SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer(); //создается сервер с данными

    const string query = ReadLine(); //считывается запрос
    //выводит в cout топ 5 по релевантности документов, хранящихся на сервере search_server
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}