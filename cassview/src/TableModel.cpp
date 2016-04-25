//===============================================================================
// Copyright (c) 2005-2016 by Made to Order Software Corporation
// 
// All Rights Reserved.
// 
// The source code in this file ("Source Code") is provided by Made to Order Software Corporation
// to you under the terms of the GNU General Public License, version 2.0
// ("GPL").  Terms of the GPL can be found in doc/GPL-license.txt in this distribution.
// 
// By copying, modifying or distributing this software, you acknowledge
// that you have read and understood your obligations described above,
// and agree to abide by those obligations.
// 
// ALL SOURCE CODE IN THIS DISTRIBUTION IS PROVIDED "AS IS." THE AUTHOR MAKES NO
// WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
// COMPLETENESS OR PERFORMANCE.
//===============================================================================

#include "TableModel.h"
#include <snapwebsites/dbutils.h>
#include <snapwebsites/log.h>
#include <snapwebsites/not_used.h>

#include <QtCassandra/QCassandraQuery.h>
#include <QtCassandra/QCassandraSession.h>

#include <QSettings>
#include <QTimer>
#include <QVariant>

#include <iostream>
#include <exception>

#include "poison.h"


using QtCassandra;
using QCassandraSchema;


TableModel::TableModel()
    //: f_tableName(name)
    //, f_rowCount(row_count)
    //, f_rowsRemaining(0) -- auto-init
    //, f_pos(0)           -- auto-init
{
}


void TableModel::setSession
    ( QCassandraSession::pointer_t session
    , const QString& keyspace_name
    , const QString& table_name
    , const int32_t row_count
    )
{
    f_session      = session;
    f_keyspaceName = keyspace_name;
    f_tableName    = table_name;
    f_rowCount     = row_count;

    f_query = std::make_shared<QCassandraQuery>(f_session);
    f_query->query(
        QString("SELECT key FROM %1.%2")
            .arg(f_keyspaceName)
            .arg(f_tableName)
            );
    f_query->setPagingSize( f_rowCount );
    f_query->start( false /*don't block*/ );

    fireTimer();
}


/** \brief Single shot timer.
 */
void TableModel::fireTimer()
{
    QTimer::singleShot( 500, this, SLOT(TableModel::onTimer()));
}


void TableModel::onTimer()
{
    if( !f_query->isReady() )
    {
        fireTimer();
        return;
    }

    const int start_pos = static_cast<int>(f_rows.size()-1);
    beginInsertRows
        ( QModelIndex()
        , start_pos
        , start_pos + f_rowCount
        );
    //
    while( f_query->nextRow() )
    {
        f_rows.push_back( f_query->getByteArrayColumn("key") );
    }
    //
    endInsertRows();

    if( f_query->nextPage( false /*block*/ ) )
    {
        fireTimer();
    }

    // Otherwise, done.
}


#if 0
bool TableModel::canFetchMore(QModelIndex const & model_index) const
{
    return f_rows.find( model_index.row() ) == f_rows.end();
}


void TableModel::fetchMore(QModelIndex const & model_index)
{
    if( !f_query )
    {
        f_query = std::make_shared<QCassandraQuery>(f_sessionMeta
    }

    NOTUSED(model_index);

    if( !f_table )
    {
        return;
    }

    try
    {
        //f_table->clearCache();
        f_rowsRemaining = f_table->readRows( f_rowp );

        int32_t const itemsToFetch( qMin(static_cast<int32_t>(f_rowCount), static_cast<int32_t>(f_rowsRemaining)) );

        beginInsertRows( QModelIndex(), f_pos, f_pos + itemsToFetch - 1 );
        endInsertRows();

        f_pos += itemsToFetch;
    }
    catch( std::exception const & x )
    {
        SNAP_LOG_ERROR() << "Exception caught! [" << x.what() << "]";
    }
}
#endif


Qt::ItemFlags TableModel::flags( QModelIndex const & idx ) const
{
    NOTUSED(idx);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


QVariant TableModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if( role != Qt::DisplayRole )
    {
        return QVariant();
    }

#if 0
    try
    {
        // the rows array is used immediately, so we can keep a temporary
        // reference here
        if( f_rows.size() <= section )
        {
            return QVariant();
        }

        if( orientation == Qt::Horizontal )
        {
            auto const & row( *(f_rows.begin()) );
            Q_ASSERT(row);

            auto const & cell( *(f_row->cells().begin() + section) );
            return cell->columnName();
        }
    }
    catch( std::exception const & x )
    {
        SNAP_LOG_ERROR() << "Exception caught! [" << x.what() << "]";
    }
#endif

    return QVariant("Table Name");
}


QVariant TableModel::data( QModelIndex const & idx, int role ) const
{
    if( f_rows.size() <= idx.row() )
    {
        return QVariant();
    }

    QSettings settings;
    const QString snap_keyspace( settings.value("snap_keyspace","snap_websites") );
    try
    {
        if( role == Qt::DisplayRole || role == Qt::EditRole )
        {
            QString ret_name;
            if( f_keyspaceName == snap_keyspace )
            {
                snap::dbutils du( f_tableName, "" );
                ret_name = du.get_row_name( f_rows[idx.row()] );
            }
            else
            {
                ret_name = QString::fromUtf8( f_rows[idx.row()].data() );
            }
            //
            return ret_name;
        }
    }
    catch( std::exception const & x )
    {
        SNAP_LOG_ERROR() << "Exception caught! [" << x.what() << "]";
    }

    return QVariant();
}


int TableModel::rowCount( QModelIndex const & prnt ) const
{
    if( prnt.isValid() )
    {
        return 1;
    }

    try
    {
        return f_rows.size();
    }
    catch( std::exception const & x )
    {
        SNAP_LOG_ERROR() << "Exception caught! [" << x.what() << "]";
    }

    return 0;
}


// vim: ts=4 sw=4 et