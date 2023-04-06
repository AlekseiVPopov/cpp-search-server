#pragma once
#include <iostream>
#include <numeric>
#include <vector>
#include <execution>
#include <list>

#include "search_server.h"


std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);