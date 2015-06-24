// CSS Preprocessor -- Test Suite
// Copyright (C) 2015  Made to Order Software Corp.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

/** \file
 * \brief Test the csspp.cpp file.
 *
 * This test runs a battery of tests agains the csspp.cpp
 * implementation to ensure full coverage.
 */

#include "catch_tests.h"

#include "csspp/exceptions.h"
#include "csspp/lexer.h"
#include "csspp/unicode_range.h"

#include <sstream>

#include <string.h>

namespace
{


} // no name namespace


TEST_CASE("Version string", "[csspp] [version]")
{
    // we expect the test suite to be compiled with the exact same version
    REQUIRE(csspp::csspp_library_version() == std::string(CSSPP_VERSION));

    // no error left over
    REQUIRE_ERRORS("");
}

TEST_CASE("Safe Boolean", "[csspp] [output]")
{
    {
        bool flag(false);
        REQUIRE(flag == false);
        {
            csspp::safe_bool_t safe(flag);
            REQUIRE(flag == true);
        }
        REQUIRE(flag == false);
    }

    {
        bool flag(false);
        REQUIRE(flag == false);
        {
            csspp::safe_bool_t safe(flag);
            REQUIRE(flag == true);
            flag = false;
            REQUIRE(flag == false);
        }
        REQUIRE(flag == false);
    }

    {
        bool flag(true);
        REQUIRE(flag == true);
        {
            csspp::safe_bool_t safe(flag);
            REQUIRE(flag == true);
            flag = false;
            REQUIRE(flag == false);
        }
        REQUIRE(flag == true);
    }
}

TEST_CASE("Decimal Number Output", "[csspp] [output]")
{
    REQUIRE(csspp::decimal_number_to_string(1.0) == "1");
    REQUIRE(csspp::decimal_number_to_string(1.2521) == "1.252");
    REQUIRE(csspp::decimal_number_to_string(1.2526) == "1.253");
    {
        csspp::safe_precision_t precision(2);
        REQUIRE(csspp::decimal_number_to_string(1.2513) == "1.25");
        REQUIRE(csspp::decimal_number_to_string(1.2561) == "1.26");
    }
    REQUIRE(csspp::decimal_number_to_string(-1.2526) == "-1.253");
    REQUIRE(csspp::decimal_number_to_string(-0.9) == "-0.9");
    REQUIRE(csspp::decimal_number_to_string(-0.0009) == "-0.001");
}

TEST_CASE("Invalid Precision", "[csspp] [invalid]")
{
    // negative not available
    for(int i(-10); i < 0; ++i)
    {
        REQUIRE_THROWS_AS(csspp::set_precision(i), csspp::csspp_exception_overflow);
    }

    // too large not acceptable
    for(int i(11); i <= 20; ++i)
    {
        REQUIRE_THROWS_AS(csspp::set_precision(i), csspp::csspp_exception_overflow);
    }
}

// Local Variables:
// mode: cpp
// indent-tabs-mode: nil
// c-basic-offset: 4
// tab-width: 4
// End:

// vim: ts=4 sw=4 et