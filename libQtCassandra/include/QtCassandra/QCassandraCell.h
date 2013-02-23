/*
 * Header:
 *      QCassandraCell.h
 *
 * Description:
 *      Handling of a cell to access data within the Cassandra database.
 *
 * Documentation:
 *      See the corresponding .cpp file.
 *
 * License:
 *      Copyright (c) 2011-2013 Made to Order Software Corp.
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
#ifndef QCASSANDRA_CELL_H
#define QCASSANDRA_CELL_H

#include "QCassandraValue.h"
#include <QObject>
#include <QMap>
#include <QSharedPointer>


namespace QtCassandra
{

class QCassandraRow;

// Cassandra Cell
class QCassandraCell : public QObject
{
public:
    virtual ~QCassandraCell();

    QString columnName() const;
    const QByteArray& columnKey() const;

    const QCassandraValue& value() const;
    void setValue(const QCassandraValue& value);

    QCassandraCell& operator = (const QCassandraValue& value);
    operator QCassandraValue () const;

    // for counters handling
    void add(int64_t value);
    QCassandraCell& operator += (int64_t value);
    QCassandraCell& operator ++ ();
    QCassandraCell& operator ++ (int);
    QCassandraCell& operator -= (int64_t value);
    QCassandraCell& operator -- ();
    QCassandraCell& operator -- (int);

    void clearCache();

    consistency_level_t consistencyLevel() const;
    void setConsistencyLevel(consistency_level_t level);
    int64_t timestamp() const;
    void setTimestamp(int64_t timestamp);

private:
    QCassandraCell(QCassandraRow *row, const QByteArray& column_key);
    void assignValue(const QCassandraValue& value);
    void unparent();

    friend class QCassandraRow;
    friend class QCassandraTable;

    QCassandraRow *                     f_row;
    QByteArray                          f_key;
    mutable controlled_vars::fbool_t    f_cached;
    QCassandraValue                     f_value;
};

// array of rows
typedef QMap<QByteArray, QSharedPointer<QCassandraCell> > QCassandraCells;



} // namespace QtCassandra
#endif
//#ifndef QCASSANDRA_CELL_H
// vim: ts=4 sw=4 et
