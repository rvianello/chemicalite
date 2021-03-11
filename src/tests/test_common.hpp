#ifndef CHEMICALITE_TEST_COMMON_INCLUDED
#define CHEMICALITE_TEST_COMMON_INCLUDED
#include <string>
#include <sqlite3.h>
#include <catch2/catch.hpp>

void test_db_open(sqlite3 **db);
void test_db_close(sqlite3 *db);

void test_select_value(sqlite3 * db, const std::string & query, double expected);
void test_select_value(sqlite3 * db, const std::string & query, int expected);
void test_select_value(sqlite3 * db, const std::string & query, const std::string expected);

#endif
