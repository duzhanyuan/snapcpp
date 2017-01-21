/* test_addr_ipv6.cpp
 * Copyright (C) 2011-2017  Made to Order Software Corporation
 *
 * Project: http://snapwebsites.org/project/libaddr
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and
 * associated documentation files (the "Software"), to
 * deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 * EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "test_addr_main.h"
#include "libaddr/addr.h"

// C++ lib
//
#include <sstream>
#include <fstream>

// C lib
//
#include <string.h>
//#include <math.h>
//#include <time.h>
#include <netdb.h>
#include <unistd.h>
#include <limits.h>




TEST_CASE( "ipv6::invalid_input", "ipv6" )
{
    GIVEN("addr()")
    {
        addr::addr a;

        SECTION("set IPv6 with an invalid family")
        {
            struct sockaddr_in6 in6 = sockaddr_in6();
            do
            {
                in6.sin6_family = rand();
            }
            while(in6.sin6_family == AF_INET6);
            in6.sin6_port = rand();
            for(int idx(0); idx < 8; ++idx)
            {
                in6.sin6_addr.s6_addr16[idx] = rand();
            }
            REQUIRE_THROWS_AS(a.set_ipv6(in6), addr::addr_invalid_argument_exception);
            REQUIRE_THROWS_AS(addr::addr b(in6), addr::addr_invalid_argument_exception);
        }
    }

    GIVEN("addr_parser() with IPv6 addresses")
    {
        SECTION("bad address")
        {
            addr::addr_parser p;
            addr::addr_range::vector_t ips(p.parse("[{bad-ip}]"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "Invalid address in \"{bad-ip}\" error -2 -- Name or service not known (errno: 2 -- No such file or directory).\n");
            REQUIRE(ips.size() == 0);
        }

        SECTION("missing ']'")
        {
            addr::addr_parser p;
            addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "IPv6 is missing the ']' ([1:2:3:4:5:6:7).\n");
            REQUIRE(ips.size() == 0);
        }

        SECTION("required address")
        {
            addr::addr_parser p;
            p.set_protocol(IPPROTO_TCP);
            p.set_allow(addr::addr_parser::flag_t::REQUIRED_ADDRESS, true);
            addr::addr_range::vector_t ips(p.parse("[]"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "Required address is missing.\n");
            REQUIRE(ips.size() == 0);
        }
    }

    GIVEN("addr_parser() with IPv4 ports")
    {
        SECTION("required port")
        {
            // optional + required -> required
            {
                addr::addr_parser p;
                p.set_protocol(IPPROTO_TCP);
                p.set_allow(addr::addr_parser::flag_t::REQUIRED_PORT, true);
                addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]"));
                REQUIRE(p.has_errors());
                REQUIRE(p.error_count() == 1);
                REQUIRE(p.error_messages() == "Required port is missing.\n");
                REQUIRE(ips.size() == 0);
            }

            // only required -> required just the same
            {
                addr::addr_parser p;
                p.set_protocol(IPPROTO_TCP);
                p.set_allow(addr::addr_parser::flag_t::PORT, false);
                p.set_allow(addr::addr_parser::flag_t::REQUIRED_PORT, true);
                addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]"));
                REQUIRE(p.has_errors());
                REQUIRE(p.error_count() == 1);
                REQUIRE(p.error_messages() == "Required port is missing.\n");
                REQUIRE(ips.size() == 0);
            }
        }

        SECTION("port not allowed")
        {
            addr::addr_parser p;
            p.set_protocol(IPPROTO_TCP);
            p.set_allow(addr::addr_parser::flag_t::PORT, false);
            addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]:123"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "Port not allowed ([1:2:3:4:5:6:7:8]:123).\n");
            REQUIRE(ips.size() == 0);
        }
    }

    GIVEN("addr_parser() with invalid masks")
    {
        SECTION("ipv6 mask is limited between 0 and 128")
        {
            for(int idx(0); idx < 5; ++idx)
            {
                int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                int const port(rand() & 0xFFFF);
                int const mask((rand() & 0xFF) + 129);
                addr::addr_parser p;
                p.set_protocol(proto);
                p.set_allow(addr::addr_parser::flag_t::MASK, true);
                addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]:" + std::to_string(port) + "/" + std::to_string(mask)));
                REQUIRE(p.has_errors());
                REQUIRE(p.error_count() == 1);
                REQUIRE(p.error_messages() == "Unsupported mask size (" + std::to_string(mask) + ", expected 128 at the most for an IPv6).\n");
                REQUIRE(ips.size() == 0);
            }
        }

        SECTION("ipv6 mask cannot use name")
        {
            for(int idx(0); idx < 5; ++idx)
            {
                int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                int const port(rand() & 0xFFFF);
                addr::addr_parser p;
                p.set_protocol(proto);
                p.set_allow(addr::addr_parser::flag_t::MASK, true);
                addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]:" + std::to_string(port) + "/[localhost]"));
                REQUIRE(p.has_errors());
                REQUIRE(p.error_count() == 1);
                REQUIRE(p.error_messages() == "Invalid mask in \"/[localhost]\", error -2 -- Name or service not known (errno: 0 -- Success).\n");
                REQUIRE(ips.size() == 0);
            }
        }

        SECTION("ipv6 mask must be between '[...]'")
        {
            int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
            int const port(rand() & 0xFFFF);
            addr::addr_parser p;
            p.set_protocol(proto);
            p.set_allow(addr::addr_parser::flag_t::MASK, true);
            addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]:" + std::to_string(port) + "/::3"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "The address uses the IPv6 syntax, the mask cannot use IPv4.\n");
            REQUIRE(ips.size() == 0);
        }

        SECTION("ipv6 mask missing the ']'")
        {
            int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
            int const port(rand() & 0xFFFF);
            addr::addr_parser p;
            p.set_protocol(proto);
            p.set_allow(addr::addr_parser::flag_t::MASK, true);
            addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]:" + std::to_string(port) + "/[::3"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "The IPv6 mask is missing the ']' ([::3).\n");
            REQUIRE(ips.size() == 0);
        }

        SECTION("ipv6 mask with an ipv4 in the '[...]'")
        {
            int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
            int const port(rand() & 0xFFFF);
            addr::addr_parser p;
            p.set_protocol(proto);
            p.set_allow(addr::addr_parser::flag_t::MASK, true);
            addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]:" + std::to_string(port) + "/[1.2.3.4]"));
            REQUIRE(p.has_errors());
            REQUIRE(p.error_count() == 1);
            REQUIRE(p.error_messages() == "Incompatible address between the address and mask address (first was an IPv6 second an IPv4).\n");
            REQUIRE(ips.size() == 0);
        }
    }
}


TEST_CASE( "ipv6::address", "ipv6" )
{
    GIVEN("addr() with an IPv6")
    {
        addr::addr a;

        SECTION("set_ipv6() / get_ipv6()")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr16[0] = rand();
                in6.sin6_addr.s6_addr16[1] = rand();
                in6.sin6_addr.s6_addr16[2] = rand();
                in6.sin6_addr.s6_addr16[3] = rand();
                in6.sin6_addr.s6_addr16[4] = rand();
                in6.sin6_addr.s6_addr16[5] = rand();
                in6.sin6_addr.s6_addr16[6] = rand();
                in6.sin6_addr.s6_addr16[7] = rand();

                // verify network type
                //
                a.set_ipv6(in6);

                // test constructor
                //
                addr::addr b(in6);
                struct sockaddr_in6 out6;
                b.get_ipv6(out6);
                REQUIRE(memcmp(&out6, &in6, sizeof(struct sockaddr_in)) == 0);

                // test set
                //
                a.set_ipv6(in6);
                a.get_ipv6(out6);
                REQUIRE(memcmp(&out6, &in6, sizeof(struct sockaddr_in)) == 0);
            }
        }

        SECTION("set_ipv6() check to_ipv6_string()")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                for(int j(0); j < 8; ++j)
                {
                    // avoid any zeroes so that way we do not have
                    // to handle the "::" syntax
                    do
                    {
                        in6.sin6_addr.s6_addr16[j] = rand();
                    }
                    while(in6.sin6_addr.s6_addr16[j] == 0);
                }

                std::stringstream ip_buf;
                ip_buf << std::hex
                       << ntohs(in6.sin6_addr.s6_addr16[0])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[1])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[2])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[3])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[4])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[5])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[6])
                       << ":"
                       << ntohs(in6.sin6_addr.s6_addr16[7]);
                std::string const ip(ip_buf.str());

                std::string port_str(std::to_string(static_cast<int>(htons(in6.sin6_port))));

                // check IPv6 as a string
                //
                a.set_ipv6(in6);
                REQUIRE(a.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ONLY)          == ip);
                REQUIRE(a.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS)      == "[" + ip + "]");
                REQUIRE(a.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_PORT)          == "[" + ip + "]:" + port_str);
                REQUIRE(a.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_MASK)          == ip + "/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // will change to 128 at some point
                REQUIRE(a.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS_MASK) == "[" + ip + "]/[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
                REQUIRE(a.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL)           == "[" + ip + "]:" + port_str + "/[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
            }
        }

        SECTION("name of various IPs")
        {
            struct sockaddr_in6 in6 = sockaddr_in6();
            in6.sin6_family = AF_INET6;
            in6.sin6_port = htons(rand());

            // verify network type
            //
            a.set_ipv6(in6);
            REQUIRE(a.get_name() == std::string()); // no name for "any" (TCP)

            a.set_protocol(IPPROTO_UDP);
            REQUIRE(a.get_name() == std::string()); // no name for "any" (UDP)

            in6 = sockaddr_in6();
            in6.sin6_family = AF_INET6;
            in6.sin6_port = htons(rand());
            in6.sin6_addr.s6_addr16[7] = htons(1);
            a.set_ipv6(in6);
            char hostname[HOST_NAME_MAX + 1];
            hostname[HOST_NAME_MAX] = '\0';
            REQUIRE(gethostname(hostname, sizeof(hostname)) == 0);
            REQUIRE(hostname[0] != '\0');
            REQUIRE(a.get_name() == hostname);
            REQUIRE(a.is_computer_interface_address() == addr::addr::computer_interface_address_t::COMPUTER_INTERFACE_ADDRESS_TRUE);
        }
    }

    GIVEN("addr_parser() with IPv6 addresses")
    {
        SECTION("verify basics")
        {
            addr::addr_parser p;
            p.set_protocol(IPPROTO_TCP);
            addr::addr_range::vector_t ips(p.parse("[1:2:3:4:5:6:7:8]"));
            REQUIRE_FALSE(p.has_errors());
            REQUIRE(ips.size() == 1);
            addr::addr_range const & r(ips[0]);
            REQUIRE(r.has_from());
            REQUIRE_FALSE(r.has_to());
            REQUIRE_FALSE(r.is_range());
            REQUIRE_FALSE(r.is_empty());
            addr::addr f(r.get_from());
            REQUIRE_FALSE(f.is_ipv4());
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "1:2:3:4:5:6:7:8");
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS) == "[1:2:3:4:5:6:7:8]");
            REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "1:2:3:4:5:6:7:8");
            REQUIRE(f.get_port() == 0);
            REQUIRE(f.get_protocol() == IPPROTO_TCP);
            REQUIRE(f.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_PUBLIC);
            uint8_t mask[16] = {};
            f.get_mask(mask);
            for(int idx(0); idx < 16; ++idx)
            {
                REQUIRE(mask[idx] == 255);
            }
        }

        SECTION("default address")
        {
            addr::addr_parser p;
            p.set_protocol(IPPROTO_TCP);
            p.set_default_address("5:5:5:5:5:5:5:5");
            addr::addr_range::vector_t ips(p.parse(""));
            REQUIRE_FALSE(p.has_errors());
            REQUIRE(ips.size() == 1);
            addr::addr_range const & r(ips[0]);
            REQUIRE(r.has_from());
            REQUIRE_FALSE(r.has_to());
            REQUIRE_FALSE(r.is_range());
            REQUIRE_FALSE(r.is_empty());
            addr::addr f(r.get_from());
            REQUIRE_FALSE(f.is_ipv4());
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "5:5:5:5:5:5:5:5");
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS) == "[5:5:5:5:5:5:5:5]");
            REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "5:5:5:5:5:5:5:5");
            REQUIRE(f.get_port() == 0);
            REQUIRE(f.get_protocol() == IPPROTO_TCP);
            REQUIRE(f.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_PUBLIC);
        }

        SECTION("address, not port allowed")
        {
            // specific address with a default
            {
                addr::addr_parser p;
                p.set_allow(addr::addr_parser::flag_t::PORT, false);
                p.set_protocol(IPPROTO_TCP);
                p.set_default_address("8:7:6:5:4:3:2:1");
                addr::addr_range::vector_t ips(p.parse("[9:9:9:9:4:3:2:1]"));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                REQUIRE(r.has_from());
                REQUIRE_FALSE(r.has_to());
                REQUIRE_FALSE(r.is_range());
                REQUIRE_FALSE(r.is_empty());
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "9:9:9:9:4:3:2:1");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS) == "[9:9:9:9:4:3:2:1]");
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "9:9:9:9:4:3:2:1");
                REQUIRE(f.get_port() == 0);
                REQUIRE(f.get_protocol() == IPPROTO_TCP);
                REQUIRE(f.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_PUBLIC);
            }

            // only a default address
            {
                addr::addr_parser p;
                p.set_allow(addr::addr_parser::flag_t::PORT, false);
                p.set_protocol(IPPROTO_TCP);
                p.set_default_address("5:1:6:2:7:3:8:4");
                addr::addr_range::vector_t ips(p.parse(""));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                REQUIRE(r.has_from());
                REQUIRE_FALSE(r.has_to());
                REQUIRE_FALSE(r.is_range());
                REQUIRE_FALSE(r.is_empty());
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "5:1:6:2:7:3:8:4");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS) == "[5:1:6:2:7:3:8:4]");
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "5:1:6:2:7:3:8:4");
                REQUIRE(f.get_port() == 0);
                REQUIRE(f.get_protocol() == IPPROTO_TCP);
                REQUIRE(f.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_PUBLIC);
            }
        }
    }
}


TEST_CASE( "ipv6::ports", "ipv6" )
{
    // by default addr() is an IPv6 address so we test the basic port
    // functions here, although it could be in a common place instead...
    //
    GIVEN("addr()")
    {
        addr::addr a;

        SECTION("default port")
        {
            REQUIRE(a.get_port() == 0);
        }

        SECTION("set_port()")
        {
            // setup a random port to start with
            //
            int const start_port(rand() & 0xFFFF);
            a.set_port(start_port);

            // test 100 invalid ports
            //
            for(int idx(0); idx < 100; ++idx)
            {
                // first try a negative port
                int port_too_small;
                do
                {
                    port_too_small = -(rand() & 0xFFFF);
                }
                while(port_too_small == 0);
                REQUIRE_THROWS_AS(a.set_port(port_too_small), addr::addr_invalid_argument_exception);

                // first try a negative port
                int const port_too_large = (rand() & 0xFFFF) + 65536;
                REQUIRE_THROWS_AS(a.set_port(port_too_large), addr::addr_invalid_argument_exception);

                // make sure port does not get modified on errors
                REQUIRE(a.get_port() == start_port);
            }

            // test all ports
            //
            for(int port(0); port < 65536; ++port)
            {
                a.set_port(port);

                REQUIRE(a.get_port() == port);
            }
        }

        SECTION("known ports to test get_service()")
        {
            a.set_port(80);
            REQUIRE(a.get_service() == "http");

            a.set_port(443);
            REQUIRE(a.get_service() == "https");

            // again with UDP
            // 
            a.set_protocol(IPPROTO_UDP);

            a.set_port(80);
            REQUIRE(a.get_service() == "http");

            a.set_port(443);
            REQUIRE(a.get_service() == "https");
        }
    }

    GIVEN("addr_parser() with IPv6 addresses and port")
    {
        SECTION("verify port")
        {
            for(int port(0); port < 65536; ++port)
            {
                int proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                addr::addr_parser p;
                p.set_protocol(proto);
                addr::addr_range::vector_t ips(p.parse("[ff01:2f3:f041:e301:f:10:11:12]:" + std::to_string(port)));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ONLY) == "ff01:2f3:f041:e301:f:10:11:12");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_BRACKETS) == "[ff01:2f3:f041:e301:f:10:11:12]");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_PORT) == "[ff01:2f3:f041:e301:f:10:11:12]:" + std::to_string(port));
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_PORT) == "[ff01:2f3:f041:e301:f:10:11:12]:" + std::to_string(port));
                REQUIRE(f.get_port() == port);
                REQUIRE(f.get_protocol() == proto);
                REQUIRE(f.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LOOPBACK);
            }
        }

        SECTION("default address with various port")
        {
            for(int idx(0); idx < 100; ++idx)
            {
                uint16_t const port(rand());
                addr::addr_parser p;
                p.set_protocol(IPPROTO_TCP);
                p.set_default_address("ff02:23:f41:e31:20:30:40:50");
                addr::addr_range::vector_t ips(p.parse(":" + std::to_string(static_cast<int>(port))));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                REQUIRE(r.has_from());
                REQUIRE_FALSE(r.has_to());
                REQUIRE_FALSE(r.is_range());
                REQUIRE_FALSE(r.is_empty());
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_PORT) == "[ff02:23:f41:e31:20:30:40:50]:" + std::to_string(static_cast<int>(port)));
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_PORT) == "[ff02:23:f41:e31:20:30:40:50]:" + std::to_string(static_cast<int>(port)));
                REQUIRE(f.get_port() == port);
                REQUIRE(f.get_protocol() == IPPROTO_TCP);
                REQUIRE(f.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LINK_LOCAL);
            }
        }
    }
}


TEST_CASE( "ipv6::masks", "ipv6" )
{
    GIVEN("addr()")
    {
        // technically, a default addr object represents and IPv6 so the
        // dealing with the mask without an IPv4 is done by IPv6 tests
        //
        addr::addr a;

        SECTION("default mask")
        {
            uint8_t mask[16] = {};
            a.get_mask(mask);
            for(int idx(0); idx < 16; ++idx)
            {
                REQUIRE(mask[idx] == 255);
            }
        }

        SECTION("set_mask()")
        {
            uint8_t mask[16], verify_mask[16];
            for(int idx(0); idx < 5; ++idx)
            {
                for(int j(0); j < 16; ++j)
                {
                    mask[j] = rand();
                }
                a.set_mask(mask);
                a.get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(mask[j] == verify_mask[j]);
                }

                // verify that a copy does copy the mask as expected
                //
                addr::addr b(a);
                b.get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(mask[j] == verify_mask[j]);
                }
            }
        }

        SECTION("set_mask()")
        {
            uint8_t mask[16];
            uint8_t verify_mask[16];
            for(int idx(0); idx < 5; ++idx)
            {
                for(int j(0); j < 16; ++j)
                {
                    mask[j] = rand();
                }
                a.set_mask(mask);
                a.get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(mask[j] == verify_mask[j]);
                    verify_mask[j] = rand();
                }

                // verify that a copy does copy the mask as expected
                //
                addr::addr b(a);
                b.get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(mask[j] == verify_mask[j]);
                    verify_mask[j] = rand();
                }

                // verify that copying inside a range works too
                //
                addr::addr_range r;
                r.set_from(a);
                r.get_from().get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(mask[j] == verify_mask[j]);
                    verify_mask[j] = rand();
                }

                // then that a range copy works as expected
                //
                addr::addr_range c(r);
                c.get_from().get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(mask[j] == verify_mask[j]);
                    verify_mask[j] = rand();
                }
            }
        }
    }

    GIVEN("addr_parser() of address:port/mask")
    {
        SECTION("mask allowed, but no mask")
        {
            int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
            int const port(rand() & 0xFFFF);
            addr::addr_parser p;
            p.set_protocol(proto);
            p.set_allow(addr::addr_parser::flag_t::MASK, true);
            addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port)));
            REQUIRE_FALSE(p.has_errors());
            REQUIRE(ips.size() == 1);
            addr::addr_range const & r(ips[0]);
            addr::addr f(r.get_from());
            REQUIRE_FALSE(f.is_ipv4());
            std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
            REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
            REQUIRE(f.get_port() == port);
            REQUIRE(f.get_protocol() == proto);
        }

        SECTION("empty mask")
        {
            int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
            int const port(rand() & 0xFFFF);
            addr::addr_parser p;
            p.set_protocol(proto);
            p.set_allow(addr::addr_parser::flag_t::MASK, true);
            addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/"));
            REQUIRE_FALSE(p.has_errors());
            REQUIRE(ips.size() == 1);
            addr::addr_range const & r(ips[0]);
            addr::addr f(r.get_from());
            REQUIRE_FALSE(f.is_ipv4());
            std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
            REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
            REQUIRE(f.get_port() == port);
            REQUIRE(f.get_protocol() == proto);
        }

        SECTION("empty mask including the '[]'")
        {
            int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
            int const port(rand() & 0xFFFF);
            addr::addr_parser p;
            p.set_protocol(proto);
            p.set_allow(addr::addr_parser::flag_t::MASK, true);
            addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[]"));
            REQUIRE_FALSE(p.has_errors());
            REQUIRE(ips.size() == 1);
            addr::addr_range const & r(ips[0]);
            addr::addr f(r.get_from());
            REQUIRE_FALSE(f.is_ipv4());
            std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]");
            REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
            REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
            REQUIRE(f.get_port() == port);
            REQUIRE(f.get_protocol() == proto);
        }

        SECTION("one number masks")
        {
            for(int idx(0); idx <= 128; ++idx)
            {
                int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                int const port(rand() & 0xFFFF);
                addr::addr_parser p;
                p.set_protocol(proto);
                p.set_allow(addr::addr_parser::flag_t::MASK, true);
                addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/" + std::to_string(idx)));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                uint8_t mask[16] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
                int j(15);
                int m(128 - idx);
                for(; m > 8; m -= 8, --j)
                {
                    mask[j] = 0;
                }
                if(j < 0)
                {
                    throw std::logic_error("invalid j here");
                }
                mask[j] = 255 << m;
                char buf[1024]; // really large buffer to make sure it does not get truncated
                if(inet_ntop(AF_INET6, mask, buf, sizeof(buf)) == nullptr)
                {
                    throw std::logic_error("somehow we could not convert our mask to an IPv6 address.");
                }
                std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[" + buf + "]");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.get_port() == port);
                REQUIRE(f.get_protocol() == proto);
            }
        }

        SECTION("address like mask")
        {
            for(int idx(0); idx < 25; ++idx)
            {
                int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                int const port(rand() & 0xFFFF);
                addr::addr_parser p;
                p.set_protocol(proto);
                p.set_allow(addr::addr_parser::flag_t::MASK, true);
                // when specified as an IP, the mask can be absolutely anything
                uint8_t mask[16];
                for(int j(0); j < 16; ++j)
                {
                    mask[j] = rand();
                }
                std::stringstream smask;
                smask << std::hex
                      << htons((mask[ 1] << 8) | mask[ 0])
                      << ":"                            
                      << htons((mask[ 3] << 8) | mask[ 2])
                      << ":"                            
                      << htons((mask[ 5] << 8) | mask[ 4])
                      << ":"                            
                      << htons((mask[ 7] << 8) | mask[ 6])
                      << ":"                            
                      << htons((mask[ 9] << 8) | mask[ 8])
                      << ":"                            
                      << htons((mask[11] << 8) | mask[10])
                      << ":"                            
                      << htons((mask[13] << 8) | mask[12])
                      << ":"                            
                      << htons((mask[15] << 8) | mask[14]);
                char buf[1024]; // really large buffer to make sure it does not get truncated
                if(inet_ntop(AF_INET6, mask, buf, sizeof(buf)) == nullptr)
                {
                    throw std::logic_error("somehow we could not convert our mask to an IPv6 address.");
                }
                addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[" + smask.str() + "]"));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[" + buf + "]");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.get_port() == port);
                REQUIRE(f.get_protocol() == proto);
            }
        }

        SECTION("address like default mask")
        {
            for(int idx(0); idx < 25; ++idx)
            {
                int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                int const port(rand() & 0xFFFF);
                addr::addr_parser p;
                p.set_protocol(proto);
                p.set_allow(addr::addr_parser::flag_t::MASK, true);
                // when specified as an IP, the mask can be absolutely anything
                // (here the mask is a string an it will be parsed by the
                // parser if required)
                //
                uint8_t mask[16];
                for(int j(0); j < 16; ++j)
                {
                    mask[j] = rand();
                }
                std::stringstream smask;
                smask << std::hex
                      << "["
                      << htons((mask[ 1] << 8) | mask[ 0])
                      << ":"                            
                      << htons((mask[ 3] << 8) | mask[ 2])
                      << ":"                            
                      << htons((mask[ 5] << 8) | mask[ 4])
                      << ":"                            
                      << htons((mask[ 7] << 8) | mask[ 6])
                      << ":"                            
                      << htons((mask[ 9] << 8) | mask[ 8])
                      << ":"                            
                      << htons((mask[11] << 8) | mask[10])
                      << ":"                            
                      << htons((mask[13] << 8) | mask[12])
                      << ":"                            
                      << htons((mask[15] << 8) | mask[14])
                      << "]";
                char buf[1024]; // really large buffer to make sure it does not get truncated
                if(inet_ntop(AF_INET6, mask, buf, sizeof(buf)) == nullptr)
                {
                    throw std::logic_error("somehow we could not convert our mask to an IPv6 address.");
                }
                p.set_default_mask(smask.str());
                addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port)));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[" + buf + "]");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.get_port() == port);
                REQUIRE(f.get_protocol() == proto);
                uint8_t verify_mask[16];
                f.get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(verify_mask[j] == mask[j]);
                }
            }
        }

        SECTION("address like mask with a default")
        {
            for(int idx(0); idx < 25; ++idx)
            {
                int const proto(rand() & 1 ? IPPROTO_TCP : IPPROTO_UDP);
                int const port(rand() & 0xFFFF);
                addr::addr_parser p;
                p.set_protocol(proto);
                p.set_allow(addr::addr_parser::flag_t::MASK, true);

                // here we want a default and an IP with a specific mask
                // to make sure that the specific mask has priority
                //
                uint8_t mask[16];
                for(int j(0); j < 16; ++j)
                {
                    mask[j] = rand();
                }
                std::stringstream smask;
                smask << std::hex
                      << "["
                      << htons((mask[ 1] << 8) | mask[ 0])
                      << ":"                            
                      << htons((mask[ 3] << 8) | mask[ 2])
                      << ":"                            
                      << htons((mask[ 5] << 8) | mask[ 4])
                      << ":"                            
                      << htons((mask[ 7] << 8) | mask[ 6])
                      << ":"                            
                      << htons((mask[ 9] << 8) | mask[ 8])
                      << ":"                            
                      << htons((mask[11] << 8) | mask[10])
                      << ":"                            
                      << htons((mask[13] << 8) | mask[12])
                      << ":"                            
                      << htons((mask[15] << 8) | mask[14])
                      << "]";
                char buf[1024]; // really large buffer to make sure it does not get truncated
                if(inet_ntop(AF_INET6, mask, buf, sizeof(buf)) == nullptr)
                {
                    throw std::logic_error("somehow we could not convert our mask to an IPv6 address.");
                }

                uint8_t default_mask[16];
                for(int j(0); j < 16; ++j)
                {
                    default_mask[j] = rand();
                }
                //std::stringstream default_smask;
                //default_smask << std::hex
                //      << "["
                //      << htons((default_mask[ 1] << 8) | default_mask[ 0])
                //      << ":"                            
                //      << htons((default_mask[ 3] << 8) | default_mask[ 2])
                //      << ":"                            
                //      << htons((default_mask[ 5] << 8) | default_mask[ 4])
                //      << ":"                            
                //      << htons((default_mask[ 7] << 8) | default_mask[ 6])
                //      << ":"                            
                //      << htons((default_mask[ 9] << 8) | default_mask[ 8])
                //      << ":"                            
                //      << htons((default_mask[11] << 8) | default_mask[10])
                //      << ":"                            
                //      << htons((default_mask[13] << 8) | default_mask[12])
                //      << ":"                            
                //      << htons((default_mask[15] << 8) | default_mask[14])
                //      << "]";
                char default_buf[1024]; // really large buffer to make sure it does not get truncated
                if(inet_ntop(AF_INET6, default_mask, default_buf, sizeof(buf)) == nullptr)
                {
                    throw std::logic_error("somehow we could not convert our mask to an IPv6 address.");
                }
                p.set_default_mask(default_buf);

                addr::addr_range::vector_t ips(p.parse("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/" + smask.str()));
                REQUIRE_FALSE(p.has_errors());
                REQUIRE(ips.size() == 1);
                addr::addr_range const & r(ips[0]);
                addr::addr f(r.get_from());
                REQUIRE_FALSE(f.is_ipv4());
                std::string result("[55:33:22:11:0:cc:bb:aa]:" + std::to_string(port) + "/[" + buf + "]");
                REQUIRE(f.to_ipv6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_ALL) == result);
                REQUIRE(f.get_port() == port);
                REQUIRE(f.get_protocol() == proto);
                uint8_t verify_mask[16];
                f.get_mask(verify_mask);
                for(int j(0); j < 16; ++j)
                {
                    REQUIRE(verify_mask[j] == mask[j]);
                }
            }
        }
    }
}


TEST_CASE( "ipv6::network_type", "ipv6" )
{
    GIVEN("addr()")
    {
        addr::addr a;

        SECTION("any (::)")
        {
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr32[0] = 0;
                in6.sin6_addr.s6_addr32[1] = 0;
                in6.sin6_addr.s6_addr32[2] = 0;
                in6.sin6_addr.s6_addr32[3] = 0;

                // verify network type
                //
                a.set_ipv6(in6);

                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_ANY);
                REQUIRE(a.get_network_type_string() == "Any");
            }

            // make sure that if any byte is set to non-zero it is not
            // viewed as the ANY address
            //
            for(int idx(0); idx < 16; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr32[0] = 0;
                in6.sin6_addr.s6_addr32[1] = 0;
                in6.sin6_addr.s6_addr32[2] = 0;
                in6.sin6_addr.s6_addr32[3] = 0;

                // change one byte only
                //
                do
                {
                    in6.sin6_addr.s6_addr[idx] = rand();
                }
                while(in6.sin6_addr.s6_addr[idx] == 0);

                // verify network type
                //
                a.set_ipv6(in6);

                REQUIRE(a.get_network_type() != addr::addr::network_type_t::NETWORK_TYPE_ANY);
                REQUIRE(a.get_network_type_string() != "Any");
            }
        }

        SECTION("private address fd00::/8")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr16[0] = htons(0xFD00 | (rand() & 255));
                in6.sin6_addr.s6_addr16[1] = rand();
                in6.sin6_addr.s6_addr16[2] = rand();
                in6.sin6_addr.s6_addr16[3] = rand();
                in6.sin6_addr.s6_addr16[4] = rand();
                in6.sin6_addr.s6_addr16[5] = rand();
                in6.sin6_addr.s6_addr16[6] = rand();
                in6.sin6_addr.s6_addr16[7] = rand();

                // verify network type
                //
                a.set_ipv6(in6);
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_PRIVATE);
                REQUIRE(a.get_network_type_string() == "Private");
            }
        }

        SECTION("private address fe80::/10")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr16[0] = htons(0xFE80 | (rand() & 63));
                in6.sin6_addr.s6_addr16[1] = rand();
                in6.sin6_addr.s6_addr16[2] = rand();
                in6.sin6_addr.s6_addr16[3] = rand();
                in6.sin6_addr.s6_addr16[4] = rand();
                in6.sin6_addr.s6_addr16[5] = rand();
                in6.sin6_addr.s6_addr16[6] = rand();
                in6.sin6_addr.s6_addr16[7] = rand();

                // verify network type
                //
                a.set_ipv6(in6);
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LINK_LOCAL);
                REQUIRE(a.get_network_type_string() == "Local Link");
            }
        }

        SECTION("private address ff02::/16")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr16[0] = htons(0xFF02);
                in6.sin6_addr.s6_addr16[1] = rand();
                in6.sin6_addr.s6_addr16[2] = rand();
                in6.sin6_addr.s6_addr16[3] = rand();
                in6.sin6_addr.s6_addr16[4] = rand();
                in6.sin6_addr.s6_addr16[5] = rand();
                in6.sin6_addr.s6_addr16[6] = rand();
                in6.sin6_addr.s6_addr16[7] = rand();

                // verify network type
                //
                a.set_ipv6(in6);
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LINK_LOCAL);
                REQUIRE(a.get_network_type_string() == "Local Link");
            }
        }

        SECTION("private address ff00::/8")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                do
                {
                    in6.sin6_addr.s6_addr16[0] = htons(0xFF00 | (rand() & 255));
                }
                while((in6.sin6_addr.s6_addr16[0] & htons(0xFF0F)) == htons(0xFF01)       // ffx1::/16
                   || (in6.sin6_addr.s6_addr16[0] & htons(0xFF0F)) == htons(0xFF02)       // ffx2::/16
                   || (in6.sin6_addr.s6_addr16[0] & htons(0xFFC0)) == htons(0xFE80)       // fe80::/10
                   || (in6.sin6_addr.s6_addr16[0] & htons(0xFF00)) == htons(0xFD00));     // fd00::/8
                in6.sin6_addr.s6_addr16[1] = rand();
                in6.sin6_addr.s6_addr16[2] = rand();
                in6.sin6_addr.s6_addr16[3] = rand();
                in6.sin6_addr.s6_addr16[4] = rand();
                in6.sin6_addr.s6_addr16[5] = rand();
                in6.sin6_addr.s6_addr16[6] = rand();
                in6.sin6_addr.s6_addr16[7] = rand();

                // verify network type
                //
                a.set_ipv6(in6);
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_MULTICAST);
                REQUIRE(a.get_network_type_string() == "Multicast");
            }
        }

        SECTION("private address ffx1::/8")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr16[0] = htons(0xFF01 | ((rand() & 15) << 4));
                in6.sin6_addr.s6_addr16[1] = rand();
                in6.sin6_addr.s6_addr16[2] = rand();
                in6.sin6_addr.s6_addr16[3] = rand();
                in6.sin6_addr.s6_addr16[4] = rand();
                in6.sin6_addr.s6_addr16[5] = rand();
                in6.sin6_addr.s6_addr16[6] = rand();
                in6.sin6_addr.s6_addr16[7] = rand();

                // verify network type
                //
                a.set_ipv6(in6);
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LOOPBACK);
                REQUIRE(a.get_network_type_string() == "Loopback");
            }
        }

        SECTION("private address ::1")
        {
            for(int idx(0); idx < 10; ++idx)
            {
                struct sockaddr_in6 in6 = sockaddr_in6();
                in6.sin6_family = AF_INET6;
                in6.sin6_port = htons(rand());
                in6.sin6_addr.s6_addr16[0] = 0;
                in6.sin6_addr.s6_addr16[1] = 0;
                in6.sin6_addr.s6_addr16[2] = 0;
                in6.sin6_addr.s6_addr16[3] = 0;
                in6.sin6_addr.s6_addr16[4] = 0;
                in6.sin6_addr.s6_addr16[5] = 0;
                in6.sin6_addr.s6_addr16[6] = 0;
                in6.sin6_addr.s6_addr16[7] = htons(1);

                // verify network type
                //
                a.set_ipv6(in6);
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LOOPBACK);
                REQUIRE(a.get_network_type_string() == "Loopback");

                // try again from a string to confirm
                //
                struct addrinfo * addrlist(nullptr);
                int const port(rand() & 65535);
                int const r(getaddrinfo("::1", std::to_string(port).c_str(), nullptr, &addrlist));
                REQUIRE(r == 0);
                REQUIRE(addrlist != nullptr);
                REQUIRE(addrlist->ai_family == AF_INET6);
                REQUIRE(addrlist->ai_addrlen == sizeof(struct sockaddr_in6));
                a.set_ipv6(*reinterpret_cast<sockaddr_in6 *>(addrlist->ai_addr));
                REQUIRE(a.get_network_type() == addr::addr::network_type_t::NETWORK_TYPE_LOOPBACK);
                REQUIRE(a.get_network_type_string() == "Loopback");
            }
        }
    }
}


// vim: ts=4 sw=4 et
