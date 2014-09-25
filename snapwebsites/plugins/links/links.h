// Snap Websites Server -- links
// Copyright (C) 2012-2014  Made to Order Software Corp.
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
#pragma once

#include "snapwebsites.h"


namespace snap
{
namespace links
{

enum name_t
{
    SNAP_NAME_LINKS_CREATELINK,
    SNAP_NAME_LINKS_DELETELINK,
    SNAP_NAME_LINKS_TABLE,         // Cassandra Table used for links
    SNAP_NAME_LINKS_NAMESPACE
};
char const *get_name(name_t name) __attribute__ ((const));


class links_exception : public snap_exception
{
public:
    links_exception(char const *       what_msg) : snap_exception("links", what_msg) {}
    links_exception(std::string const& what_msg) : snap_exception("links", what_msg) {}
    links_exception(QString const&     what_msg) : snap_exception("links", what_msg) {}
};

class links_exception_missing_links_table : public links_exception
{
public:
    links_exception_missing_links_table(char const *       what_msg) : links_exception(what_msg) {}
    links_exception_missing_links_table(std::string const& what_msg) : links_exception(what_msg) {}
    links_exception_missing_links_table(QString const&     what_msg) : links_exception(what_msg) {}
};

class links_exception_missing_branch_table : public links_exception
{
public:
    links_exception_missing_branch_table(char const *       what_msg) : links_exception(what_msg) {}
    links_exception_missing_branch_table(std::string const& what_msg) : links_exception(what_msg) {}
    links_exception_missing_branch_table(QString const&     what_msg) : links_exception(what_msg) {}
};

class links_exception_invalid_name : public links_exception
{
public:
    links_exception_invalid_name(char const *       what_msg) : links_exception(what_msg) {}
    links_exception_invalid_name(std::string const& what_msg) : links_exception(what_msg) {}
    links_exception_invalid_name(QString const&     what_msg) : links_exception(what_msg) {}
};

class links_exception_invalid_db_data : public links_exception
{
public:
    links_exception_invalid_db_data(char const *       what_msg) : links_exception(what_msg) {}
    links_exception_invalid_db_data(std::string const& what_msg) : links_exception(what_msg) {}
    links_exception_invalid_db_data(QString const&     what_msg) : links_exception(what_msg) {}
};

class links_exception_missing_link : public links_exception
{
public:
    links_exception_missing_link(char const *       what_msg) : links_exception(what_msg) {}
    links_exception_missing_link(std::string const& what_msg) : links_exception(what_msg) {}
    links_exception_missing_link(QString const&     what_msg) : links_exception(what_msg) {}
};



class link_info
{
public:
    link_info()
        //: f_unique(false)
        //, f_name("")
        //, f_key("")
        //, f_branch(snap_version::SPECIAL_VERSION_UNDEFINED)
    {
    }

    link_info(QString const& new_name, bool unique, QString const& new_key, snap_version::version_number_t branch_number)
        : f_unique(unique)
        , f_name(new_name)
        , f_key(new_key)
        , f_branch(branch_number)
    {
        // empty is valid on construction
        if(!new_name.isEmpty())
        {
            verify_name(new_name);
        }
    }

    void set_name(QString const& new_name, bool unique = false)
    {
        verify_name(new_name);
        f_unique = unique;
        f_name = new_name;
    }
    void set_key(QString const& new_key)
    {
        f_key = new_key;
    }
    void set_branch(snap_version::version_number_t branch_number)
    {
        f_branch = branch_number;
    }

    bool is_unique() const
    {
        return f_unique;
    }
    QString const& name() const
    {
        return f_name;
    }
    QString cell_name() const
    {
        if(f_name.isEmpty())
        {
            // no name, return an empty string
            return f_name;
        }
        // prepend "links" as a namespace for all links
        return QString("%1::%2").arg(get_name(SNAP_NAME_LINKS_NAMESPACE)).arg(f_name);
    }
    QString const& key() const
    {
        return f_key;
    }
    QString row_key() const
    {
        if(f_key.isEmpty())
        {
            throw snap_logic_exception(QString("row_key() was requested with the key still undefined (name: \"%1\", branch is \"%2\")").arg(f_name).arg(f_branch));
        }
        if(f_branch == snap_version::SPECIAL_VERSION_INVALID
        || f_branch == snap_version::SPECIAL_VERSION_UNDEFINED
        || f_branch == snap_version::SPECIAL_VERSION_EXTENDED)
        {
            throw snap_logic_exception(QString("row_key() was requested with the branch still undefined (name: \"%1\", key is \"%2\")").arg(f_name).arg(f_key));
        }
        return QString("%1#%2").arg(f_key).arg(f_branch);
    }
    QString link_key() const
    {
        if(f_name.isEmpty())
        {
            throw snap_logic_exception(QString("link_key() was requested with the name still undefined (key: \"%1\", branch: \"%2\"").arg(f_key).arg(f_branch));
        }
        if(f_key.isEmpty())
        {
            throw snap_logic_exception(QString("link_key() was requested with the key still undefined (name: \"%1\", branch: \"%2\"").arg(f_name).arg(f_branch));
        }
        if(f_branch == snap_version::SPECIAL_VERSION_INVALID
        || f_branch == snap_version::SPECIAL_VERSION_UNDEFINED
        || f_branch == snap_version::SPECIAL_VERSION_EXTENDED)
        {
            throw snap_logic_exception(QString("link_key() was requested with the branch still undefined (name: \"%1\", key: \"%2\"").arg(f_name).arg(f_key));
        }
        return QString("%1#%2/%3").arg(f_key).arg(f_branch).arg(f_name);
    }
    snap_version::version_number_t branch() const
    {
        return f_branch;
    }

    QString data() const;
    void from_data(QString const& db_data);

    static void verify_name(QString const& vname);

private:
    controlled_vars::fbool_t        f_unique;
    QString                         f_name;
    QString                         f_key;
    snap_version::version_number_t  f_branch;
};

class link_context
{
public:
    bool next_link(link_info& info);

private:
    friend class links;

    link_context(::snap::snap_child *snap, link_info const& info, const int count);

    zpsnap_child_t                                  f_snap;
    link_info                                       f_info;
    QtCassandra::QCassandraRow::pointer_t           f_row;
    QtCassandra::QCassandraColumnRangePredicate     f_column_predicate;
    QtCassandra::QCassandraCells                    f_cells;
    QtCassandra::QCassandraCells::const_iterator    f_cell_iterator;
    QString                                         f_link;
};

class links_cloned
{
public:
    virtual void        repair_link_of_cloned_page(QString const& clone, snap_version::version_number_t branch_number, link_info const& source, link_info const& destination) = 0;
};

class links : public plugins::plugin
            , public server::backend_action
{
public:
    static int const    DELETE_RECORD_COUNT = 1000;

                        links();
                        ~links();

    static links *      instance();
    virtual QString     description() const;
    virtual int64_t     do_update(int64_t last_updated);
    QtCassandra::QCassandraTable::pointer_t get_links_table();

    void                on_bootstrap(::snap::snap_child *snap);
    void                on_add_snap_expr_functions(snap_expr::functions_t& functions);
    void                on_register_backend_action(server::backend_action_map_t& actions);
    virtual void        on_backend_action(QString const& action);
    void                adjust_links_after_cloning(QString const& source_key, QString const& destination_key);

    // TBD should those be events? (or trigger events?)
    void                create_link(link_info const& src, link_info const& dst);
    void                delete_link(link_info const& info, int const delete_record_count = DELETE_RECORD_COUNT);
    void                delete_this_link(link_info const& source, link_info const& destination);

    QSharedPointer<link_context> new_link_context(link_info const& info, int const count = DELETE_RECORD_COUNT);

private:
    void                initial_update(int64_t variables_timestamp);
    void                init_tables();

    zpsnap_child_t                                  f_snap;
    QtCassandra::QCassandraTable::pointer_t         f_links_table;
    QtCassandra::QCassandraTable::pointer_t         f_branch_table;
};

} // namespace links
} // namespace snap
// vim: ts=4 sw=4 et
