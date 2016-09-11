/*
 * Text:
 *      QCassandraConsistencyLevel.cpp
 *
 * Description:
 *      Handling of the cassandra::CassandraClient and corresponding transports,
 *      protocols, sockets, etc.
 *
 * Documentation:
 *      See each definition below.
 *
 * License:
 *      Copyright (c) 2011-2016 Made to Order Software Corp.
 *
 *      http://snapwebsites.org/
 *      contact@m2osw.com
 *
 *      Permission is hereby granted, free of charge, to any person obtaining a
 *      copy of this software and associated documentation files (the
 *      "Software"), to deal in the Software without restriction, including
 *      without limitation the rights to use, copy, modify, merge, publish,
 *      distribute, sublicense, and/or sell copies of the Software, and to
 *      permit persons to whom the Software is furnished to do so, subject to
 *      the following conditions:
 *
 *      The above copyright notice and this permission notice shall be included
 *      in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *      OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "QtCassandra/QCassandraConsistencyLevel.h"

namespace QtCassandra
{

struct ConsistencyLevel
{
    enum type
    {
        ONE          = 1,
        QUORUM       = 2,
        LOCAL_QUORUM = 3,
        EACH_QUORUM  = 4,
        ALL          = 5,
        ANY          = 6,
        TWO          = 7,
        THREE        = 8,
        SERIAL       = 9,
        LOCAL_SERIAL = 10
    };
};


/** \brief Ask the system to use the default consistency level.
 *
 * It is possible to setup a consistency level in your QCassandra object.
 * That is viewed as the default consistency level. To help you avoid
 * having to define the consistency level in each object used to access
 * the database, you can leave the consistency level to the default and
 * then the QCassandra object consistency level will be used.
 *
 * Details http://www.datastax.com/docs/1.0/dml/data_consistency
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_DEFAULT = -1;

/** \brief Read/Write to one computer.
 *
 * Read/Write the data to the one computer we're connected to. Do not
 * duplicate the data.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_ONE =
    ConsistencyLevel::ONE;

/** \brief Read/Write to a quorum of computers.
 *
 * Read/Write the data to the total number of all the computers divided
 * by 2 plus one. This ensures data safety.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_QUORUM =
    ConsistencyLevel::QUORUM;

/** \brief Read/Write to a quorum of local computers.
 *
 * Read/Write the data to the total number of local computers divided by 2
 * plus one. This ensures data safety.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_LOCAL_QUORUM =
    ConsistencyLevel::LOCAL_QUORUM;

/** \brief Read/Write to a quorum of computers in each data center.
 *
 * Read/Write the data to the total number of computers divided by 2
 * plus one in each data center. This ensures data safety in all
 * the data centers instead of just the entire set of computers.
 *
 * This is generally the best value if you have multiple centers
 * and want to have safe data.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_EACH_QUORUM =
    ConsistencyLevel::EACH_QUORUM;

/** \brief Read/Write to all computers.
 *
 * Read/Write the data to all the computers.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_ALL =
    ConsistencyLevel::ALL;

/** \brief Read/Write to any computer.
 *
 * Read/Write the data to any computer. If the computer we are connected
 * to is too slow or filled up, then another may be selected for
 * this data.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_ANY =
    ConsistencyLevel::ANY;

/** \brief Read/Write to two computers.
 *
 * Read/Write the data to two computers.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_TWO =
    ConsistencyLevel::TWO;

/** \brief Read/Write to three computers.
 *
 * Read/Write the data to three computers.
 */
const cassandra_consistency_level_t CONSISTENCY_LEVEL_THREE =
    ConsistencyLevel::THREE;

} // namespace QtCassandra
// vim: ts=4 sw=4 et
