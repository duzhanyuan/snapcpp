/*
 * Text:
 *      QCassandraSchema.cpp
 *
 * Description:
 *      Database schema metadata.
 *
 * Documentation:
 *      See each function below.
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

#include "QtCassandra/QCassandraSchema.h"

namespace QtCassandra
{


namespace QCassandraSchema
{


class SessionMeta
{
public:
    typedef std::shared_ptr<SessionMeta>  pointer_t;
    typedef std::map<QString,QString>     qstring_map_t;

    SessionMeta( QCassandraSession::pointer_t session );
    ~SessionMeta();

    QCassandraSession::pointer_t    session() const;

    uint32_t   snapshotVersion() const;

    class KeyspaceMeta
    {
    public:
        typedef std::shared_ptr<KeyspaceMeta> pointer_t;
        typedef std::map<QString,pointer_t>   map_t;

        KeyspaceMeta( SessionMeta::pointer_t session_meta );

        QCassandraSession::pointer_t session() const;

        QString         getName() const;
        qstring_map_t   getFields() const;

        class TableMeta
        {
        public:
            typedef std::shared_ptr<TableMeta>  pointer_t;
            typedef std::map<QString,pointer_t> map_t;

            TableMeta( KeyspaceMeta::pointer_t kysp );

            QString     getName() const;

            class ColumnMeta
            {
            public:
                typedef std::shared_ptr<ColumnMeta> pointer_t;
                typedef std::map<QString,pointer_t> map_t;

                typedef enum
                {
                    TypeRegular, TypePartitionKey, TypeClusteringKey, TypeStatic, TypeCompactValue;
                }
                type_t;

                ColumnMeta( TableMeta::pointer_t tbl );

                QString         getName() const;
                qstring_map_t   getFields() const;
                type_t          getType() const;

            private:
                qstring_map_t   f_fields;
            };

        private:
            KeyspaceMeta::pointer_t f_keyspace;
            ColumnMeta::map_t       f_columns;
        };

        const TableMeta::map_t& getTableMetaMap() const;

    private:
        QCassandraSessionMeta::pointer_t    f_session;
        qstring_map_t                       f_fields;
        TableMeta::map_t                    f_tables;
    };

    const KeyspaceMeta::map_t& getKeyspaces();

private:
    QCassandraSession::pointer_t    f_session;
    QString                         f_name;
    qstring_map_t                   f_fields;
    uint32_t                        f_version;
    KeyspaceMeta::map_t             f_keyspaces;
};


}
//namespace QtCassandra


}
//namespace QCassandraSchema


// vim: ts=4 sw=4 et
