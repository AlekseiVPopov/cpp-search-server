#pragma once

#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <vector>


#include "document.h"
#include "process_queries.h"
#include "remove_duplicates.h"
#include "search_server.h"
#include "log_duration.h"


template<typename First, typename Second>
std::ostream& operator<<(std::ostream& output, const std::pair<First, Second>& couple) {
	output << couple.first << ": " << couple.second;
	return output;
}

template<typename Elements>
void PrintContainer(std::ostream& output, const Elements& container) {
	bool is_first = true;
	for (const auto& element : container) {
		if (is_first) {
			output << element;
			is_first = false;
		} else {
			output << ", " << element;
		}
	}
}

template<typename Element>
std::ostream& operator<<(std::ostream& output, const std::vector<Element>& container) {
	output << "[";
	PrintContainer(output, container);
	output << "]";
	return output;
}

template<typename Element>
std::ostream& operator<<(std::ostream& output, const std::set<Element>& container) {
	output << "{";
	PrintContainer(output, container);
	output << "}";
	return output;
}

template<typename Key, typename Value>
std::ostream& operator<<(std::ostream& output, const std::map<Key, Value>& container) {
	output << "{";
	PrintContainer(output, container);
	output << "}";
	return output;
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string_view& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view& query);

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str,
		const std::string& file, const std::string& func, unsigned line, const std::string& hint) {
	if (t != u) {
		std::cout << std::boolalpha;
		std::cout << file << "(" << line << "): " << func << ": ";
		std::cout << "ASSERT_EQUAL(" << t_str << ", " << u_str << ") failed: ";
		std::cout << t << " != " << u << ".";
		if (!hint.empty()) {
			std::cout << " Hint: " << hint;
		}
		std::cout << std::endl;
		abort();
	}
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
		unsigned line, const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T& test, const std::string& test_str) {
	test();
	std::cerr << test_str << " OK" << std::endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

void TestAddDocument();
void TestExcludeStopWordsFromAddedDocumentContent();
void TestMinusWordsSearching();
void TestMatchDocumentFromAddedDocumentContent();
void TestMatchDocumentWithExecutionPolicy();
void TestFindTopDocumentsSorting();
void TestComputeAverageRating();
void TestFindTopDocumentsWithPredicate();
void TestFindTopDocumentsWithStatus();
void TestRelevanceCalculating();
void TestBeginEndSearchServer();
void TestGetWordFrequencies();
void TestRemoveDocument();
void TestRemoveDocument2();
void TestRemoveDocumentWithExecutionPolicy();
void TestRemoveDuplicate();
void TestProcessQueries();
void TestProcessQueriesJoined();
void TestFindTopDocumentParallel();

void TestSearchServer();

// --------- Окончание модульных тестов поисковой системы -----------
