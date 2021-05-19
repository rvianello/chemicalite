#include <catch2/catch.hpp>

#include <sqlite3ext.h>
extern const sqlite3_api_routines *sqlite3_api;

#include "../utils.hpp"

TEST_CASE("trim whitespace", "[utils]")
{
  REQUIRE(trim("") == "");

  REQUIRE(trim(" ") == "");

  REQUIRE(trim("    ") == "");

  REQUIRE(trim("  dflsdkjf") == "dflsdkjf");

  REQUIRE(trim("  dflsdkjf  ") == "dflsdkjf");

  REQUIRE(trim("dflsdkjf  ") == "dflsdkjf");

  REQUIRE(trim("  dfls  dkjf  ") == "dfls  dkjf");
}