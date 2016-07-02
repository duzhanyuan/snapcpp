// Copyright (C) 2015-2016  Made to Order Software Corp.
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
//
#pragma once

#include <QtCassandra/QueryModel.h>

#include <snapwebsites/dbutils.h>

#include <QModelIndex>

class DomainModel
    : public QtCassandra::QueryModel
{
    Q_OBJECT

public:
    DomainModel();

    void doQuery();

    // Read only access
    //
    virtual bool     fetchFilter( const QByteArray& key ) override;
    virtual QVariant data( QModelIndex const & index, int role = Qt::DisplayRole ) const override;
    virtual void     fetchCustomData( QtCassandra::QCassandraQuery::pointer_t q ) override;

private:
    typedef std::map<QString,QByteArray> sort_map_t;
    sort_map_t                      f_sortMap;
    std::shared_ptr<snap::dbutils>  f_dbutils;
};

// vim: ts=4 sw=4 et
