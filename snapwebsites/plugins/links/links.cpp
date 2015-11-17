// Snap Websites Server -- manage double links
// Copyright (C) 2012-2015  Made to Order Software Corp.
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

#include "links.h"

// TODO: remove dependency on content (because content includes links...)
//       it may be that content and links should be merged (oh well!) TBD
#include "../content/content.h"

#include "log.h"
#include "not_reached.h"
#include "not_used.h"

#include <iostream>

#include <QtCore/QDebug>

#include "poison.h"


SNAP_PLUGIN_START(links, 1, 0)

/** \brief Get a fixed links name.
 *
 * The links plugin makes use of different names in the database. This
 * function ensures that you get the right spelling for a given name.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
char const * get_name(name_t name)
{
    switch(name)
    {
    case name_t::SNAP_NAME_LINKS_CLEANUPLINKS:
        return "cleanuplinks";

    case name_t::SNAP_NAME_LINKS_CREATELINK:
        return "createlink";

    case name_t::SNAP_NAME_LINKS_DELETELINK:
        return "deletelink";

    case name_t::SNAP_NAME_LINKS_NAMESPACE:
        return "links";

    case name_t::SNAP_NAME_LINKS_TABLE: // sorted index of links
        return "links";

    default:
        // invalid index
        throw snap_logic_exception("invalid name_t::SNAP_NAME_LINKS_...");

    }
    NOTREACHED();
}


/** \fn link_info::link_info();
 * \brief Create a default link descriptor.
 *
 * This function initializes the link_info object with defaults:
 *
 * \li name -- the name is set to the empty string "";
 * \li unique -- the link is marked as non-unique (false);
 * \li key -- the key is set to an empty string "";
 * \li branch_number -- the branch is marked as undefined.
 *
 * A default link_info is used to setup a link by hand or define the
 * link from data read from the database using the from_data() function.
 *
 * To define the unique flag, use the set_name() function.
 *
 * \sa set_name()
 * \sa set_key()
 * \sa set_branch()
 * \sa from_data()
 */


/** \fn link_info::link_info(QString const& new_name, bool unique, QString const& new_key, snap_version::version_number_t branch_number);
 * \brief Create a link descriptor.
 *
 * Initialize the link information with a name, a key, and a branch number.
 * See the set_name(), set_key(), and set_branch() functions for more
 * information.
 *
 * Note that a key and a name are ultimately necessary. If not defined on
 * creation then you must call the set_name() and set_key() functions later
 * but before making use of the link_info object.
 *
 * \param[in] new_name  The name of the key (column.)
 * \param[in] unique  Whether the link is unique (one to one, or one to many.)
 * \param[in] new_key  The key is the name of the row.
 * \param[in] branch_number  The branch number where this link is to be saved.
 *
 * \sa set_name()
 * \sa set_key()
 * \sa set_branch()
 */


/** \fn void link_info::set_name(QString& new_name, bool unique);
 * \brief Set the name of the column to use for the link.
 *
 * The name is used to distinguish the different links used within a row.
 * The name must include the plugin name (i.e. filter::category).
 *
 * By default a link is expected to be: many to many or many to one. The
 * unique flag can be used to transform it to: one to many or one to one
 * link.
 *
 * A number is appended to the column names when \p unique is false.
 * This gives us a many to many or many to one link capability:
 *
 *   "links::<plugin name>::<link name>-<server name>-<unique number>"
 *
 * When the unique flag is set to true, the name of the column does not
 * include the unique number:
 *
 *   "links::<plugin name>::<link name>"
 *
 * \param[in] new_name  The name of the link used as the column name.
 * \param[in] unique  The unique flag, if true it means 'one', of false, it manes 'many'
 *
 * \sa name()
 * \sa is_unique()
 */


/** \fn void link_info::set_key(QString& new_key)
 * \brief Set the key (row name) where the link is to be saved.
 *
 * This function saves the key where the link is to be saved.
 * The key actually represents the exact name of the row where the link is
 * saved.
 *
 * The destination (i.e the data of the link) is defined using another
 * link_info (i.e. the create_link() uses source (src) and
 * destination (dst) parameters which are both link_info.)
 *
 * \note
 * What changes depending on the link category (unique or not) is the
 * column name.
 * 
 * \param[in] new_key  The key to the row where the link is to be saved.
 *
 * \sa key()
 */


/** \fn bool link_info::is_unique() const
 * \brief Check whether this link is marked as unique.
 *
 * The function returns the current value of the unique flag as set on
 * construction. It can be changed with the set_name() function as
 * the second parameter. By default the set_name() function assumes that
 * the link is not unique (many).
 *
 * \return true if the link is unique (one to many, many to one, or one to one)
 *
 * \sa set_name()
 */


/** \fn const QString& link_info::name() const
 * \brief Retrieve the name of the link.
 *
 * The function returns the name of the link as set on construction
 * or with the set_name() function.
 *
 * \return The name of the link that is used to form the full name of
 *         the column.
 *
 * \sa set_name()
 */


/** \fn const QString& link_info::key() const
 * \brief Retrieve the key of the link.
 *
 * The function returns the key for the link as set on construction
 * or with the set_key() function.
 *
 * \return the key of the link that is used as the row key
 *
 * \sa set_key()
 */


/** \var controlled_vars::zbool_t link_info::f_unique;
 * \brief Unique (one) or not (many) links.
 *
 * This flag is used to tell the link system whether the link is
 * unique or not.
 *
 * \sa is_unique() const
 * \sa set_name() const
 */


/** \var QString link_info::f_name;
 * \brief The name of the column.
 *
 * The name of the column used for the link.
 *
 * \sa name() const
 * \sa set_name()
 */


/** \var QString link_info::f_key
 * \brief The key of this link.
 *
 * The key of a link is the key of the row where the link is to be
 * saved.
 *
 * \sa set_key()
 */


/** \brief Verify that the name is valid.
 *
 * Because of the way the link plugin makes use of the link name, we
 * want to make sure that the name is valid according to the rules
 * defined below. The main reason is so we can avoid problems. A
 * link name is expected to include a plugin name and a link name.
 * There may be more than once plugin name when useful. For example,
 * the "permissions::users::edit" link name is considered valid.
 *
 * For links that are not unique, the system appends the server name
 * and a unique number separated by dashes. This is why the link plugin
 * forbids the provided link names from including a dash.
 *
 * So, a link name in the database looks like this:
 *
 * \code
 *    links::(<plugin-name>::)+<link-name>
 *    links::(<plugin-name>::)+<link-name>-<server-name>-<unique-number>
 * \endcode
 *
 * Valid link and plugin names are defined with the following BNF:
 *
 * \code
 *   plugin_name ::= link_name
 *   link_name ::= word
 *               | word '::' link_name
 *   word ::= letters | digits | '_'
 *   letters ::= ['A'-'Z']
 *             | ['a'-'z']
 *   digits ::= ['0'-'9']
 * \endcode
 *
 * As we can see, this BNF does not allow for any '-' in the link name.
 *
 * \note
 * It is to be noted that the syntax allows for a name to start with a
 * digit. This may change in the future and only letters may be allowed
 * as first characters.
 *
 * \exception links_exception_invalid_name
 * The links_exception_invalid_name is raised if the name is not valid.
 *
 * \param[in] vname  The name to be verified
 */
void link_info::verify_name(QString const & vname)
{
    // the namespace is really only for debug purposes
    // but at this time we'll keep it for security
    char const * links_namespace(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE));
    QString ns;
    ns.reserve(64);
    bool has_namespace(false);
    for(QString::const_iterator it(vname.begin()); it != vname.end(); ++it)
    {
        ushort c = it->unicode();
        if(c == ':' && it != vname.begin())
        {
            // although "links" is a valid name, it is in conflict because
            // out column name already starts with "links::" and it is not
            // unlikely that a programmer is trying to make sure that the
            // start of the name is "links::"...
            if(ns == links_namespace)
            {
                throw links_exception_invalid_name("name \"" + vname + "\" is not acceptable, a name cannot make use of the \"links\" namespace.");
            }
            ns.clear(); // TBD does that free the reserved buffer?

            // we found a ':' which was not the very first character
            ++it;
            if(it == vname.end())
            {
                throw links_exception_invalid_name("name \"" + vname + "\" is not acceptable, a name cannot end with a ':'.");
            }
            if(it->unicode() != ':')
            {
                throw links_exception_invalid_name("name \"" + vname + "\" is not acceptable, the namespace operator must be '::'.");
            }
            ++it;
            if(it == vname.end())
            {
                throw links_exception_invalid_name("name \"" + vname + "\" is not acceptable, a name cannot end with a namespace operator '::'.");
            }
            // we must have a character that's not a ':' after a '::'
            c = it->unicode();
            has_namespace = true;
        }
        // colons are not acceptable here, we must have a valid character
        if((c < '0' || c > '9')
        && (c < 'A' || c > 'Z')
        && (c < 'a' || c > 'z')
        && c != '_')
        {
            if(c == ':')
            {
                throw links_exception_invalid_name("name \"" + vname + "\" is not acceptable, character ':' was not expected here.");
            }
            throw links_exception_invalid_name("name \"" + vname + "\" is not acceptable, character '" + QChar(c) + "' is not valid.");
        }
        ns += QChar(c);
    }
    if(!has_namespace)
    {
        // at least one namespace is mandatory
        throw links_exception_invalid_name(QString("name \"%1\" is not acceptable, at least one namespace is expected. (key: %2, branch: %3)").arg(vname).arg(f_key).arg(f_branch));
    }

    if(ns == links_namespace)
    {
        throw links_exception_invalid_name(QString("name \"%1\" is not acceptable, a link name cannot end with \"links\". (key: %2, branch: %3)").arg(vname).arg(f_key).arg(f_branch));
    }
}


/** \brief Retrieve the data to be saved in the database.
 *
 * Defines the string to be saved in the database. We could use the serializer
 * but this is so limited and used so much that having a direct definition
 * will generally be much faster (early optimization...)
 *
 * The keys are defined as follow:
 *
 * \li k[key] -- the key of the destination row
 * \li n[ame] -- the name of the field in the destination row (i.e. links::\<name>)
 * \li b[ranch] -- the branch number of the destination page we are linked to
 * \li u[nique] -- whether the link is unique
 *
 * \note
 * Remember that in the source we save the destination link information and
 * vice versa. So if you would like to know whether the source is unique, you
 * have to read the destination link information.
 *
 * \return The string representing this link.
 *
 * \sa from_data()
 */
QString link_info::data() const
{
    return QString("k=%1\nn=%2\nb=%3\nu=%4")
            .arg(f_key)
            .arg(f_name)
            .arg(f_branch)
            .arg(f_unique ? "1" : "*");
}


/** \brief Parse a string of key & name pairs back to a link info.
 *
 * This function is the inverse of the data() function. It takes
 * a string as input and defines the f_key, f_name, f_branch, and
 * f_unique parameters from the data found in that string.
 *
 * The function does not return anything. Instead it saves the
 * parameters to 'this' link_info object.
 *
 * \exception links_exception_invalid_db_data
 * The data is expected to be read from the database after being generated
 * using the data() function. If any parameter is missing, misspelled,
 * not defined as a key/value pair, then the function throws this exception.
 *
 * \param[in] db_data  The data to convert to the different parameters.
 *
 * \sa data()
 */
void link_info::from_data(QString const& db_data)
{
    snap_string_list lines(db_data.split('\n'));
    if(lines.count() != 4)
    {
        throw links_exception_invalid_db_data(QString("db_data (%1) is not exactly 4 lines").arg(db_data));
    }
    snap_string_list key_data(lines[0].split('='));
    snap_string_list name_data(lines[1].split('='));
    snap_string_list branch_data(lines[2].split('='));
    snap_string_list unique_data(lines[3].split('='));
    if(key_data.count() != 2 || name_data.count() != 2 || branch_data.count() != 2 || unique_data.count() != 2
    || key_data[0] != "k" || name_data[0] != "n" || branch_data[0] != "b" || unique_data[0] != "u")
    {
        throw links_exception_invalid_db_data(QString("db_data variables in \"%1\" are not k[ey], n[ame], b[ranch], and u[nique]").arg(db_data));
    }
    set_key(key_data[1]);
    set_name(name_data[1], unique_data[1] == "1"); // TBD: verify whether unique is "*" if not "1"?
    set_branch(branch_data[1].toULong()); // TBD: verify that it is an integer?
}






/** \brief Memorize two link_info structures.
 *
 * This class is used to memorize two link structures: a source and a
 * destination.
 *
 * The source and destination structures must be complete when this
 * constructor is called because once copied in, they cannot be
 * modified anymore.
 *
 * \param[in] src  The source to memorize.
 * \param[in] dst  The destination to memorize.
 */
link_info_pair::link_info_pair(link_info const & src, link_info const & dst)
    : f_source(src)
    , f_destination(dst)
{
}


/** \brief Return the source information.
 *
 * This function returns a copy of the source branch.
 *
 * This information generally comes from the data gathered on our
 * side of the tree.
 *
 * \return A reference to the link_info representing the source.
 */
link_info const & link_info_pair::source()
{
    return f_source;
}


/** \brief Return the destination information.
 *
 * This function returns a copy of the destination branch.
 *
 * This information generally comes from the data of the cell value
 * used for a link.
 *
 * \return A reference to the link_info representing the destination.
 */
link_info const & link_info_pair::destination()
{
    return f_destination;
}






/** \brief Initialize a link context to read links.
 *
 * This object is used to read links from the database.
 * This is particularly useful in this case because you may need
 * to call the function multiple times before you read all the
 * links.
 *
 * \note
 * The order in which links are returned is not always
 * the order in which links were created. This is because the
 * counter used to create links may get a new digit at which
 * point the order should be considered quite random.
 *
 * \param[in] snap  The snap_child object pointer.
 * \param[in] info  The link information about this link context.
 * \param[in] count  Row count to select from the row table.
 */
link_context::link_context(snap_child * snap, link_info const & info, int const count)
    : f_snap(snap)
    , f_info(info)
    //, f_row() -- auto-init
    //, f_column_predicate() -- auto-init
    //, f_link() -- auto-init
{
    // TODO: verify that unicity is equal on both sides?

    // if the link is unique, it only appears in the data table
    // and we don't need the context per se, so we just read
    // the info and keep it in the context for retrieval;
    // if not unique, then we read the first 1,000 links and
    // make them available in the context to the caller
    if(f_info.is_unique())
    {
        // TODO: we have to somehow remove this content dependency (circular
        //       dependency)
        QtCassandra::QCassandraTable::pointer_t branch_table(content::content::instance()->get_branch_table());
        // f_row remains null when unique is true (isNull() returns true)
        QtCassandra::QCassandraValue link(branch_table->row(f_info.row_key())->cell(f_info.cell_name())->value());
        if(!link.nullValue())
        {
            f_link = link.stringValue();
        }
    }
    else
    {
        // since we are loading these links from the links index we do
        // not need to specify the column names in the column predicate
        // it will automatically read all the data from that row
        QtCassandra::QCassandraTable::pointer_t links_table(links::links::instance()->get_links_table());
        if(links_table->exists(f_info.link_key()))
        {
            if(count < 10)
            {
                throw snap_logic_exception(QString("a count of %1 to read links is not valid, expected 10 or more").arg(count));
            }

            f_row = links_table->row(f_info.link_key());
            // WARNING: Here the column names are the keys, not the link names...
            f_column_predicate.setCount(count);
            f_column_predicate.setIndex(); // behave like an index
            // we MUST clear the cache in case we read the same list of links twice
            f_row->clearCache();
            // at this point begin() == end()
            f_cells = f_row->cells(); // <- this is VERY important in case someone wants to delete cells
            f_cell_iterator = f_cells.begin();
        }
        //else -- this is the default so we do not need to reset f_row
        //{
        //    // no such row; it is empty (link does not exist)
        //    f_row.reset();
        //}
    }
}

/** \brief Retrieve the next link.
 *
 * This function reads one link and saves it in the info parameter.
 * If no more links are available, then the function returns false
 * and the info parameter is not modified.
 *
 * \note
 * The order in which links are returned is not always
 * the order in which links were created. This is because the
 * counter used to create links may get a new digit at which
 * point the order should be considered quite random.
 *
 * \todo
 * The result does not return the unique flag as defined in the database.
 * The unique flag is likely going to be set to false and stay false all
 * along whether or not the link on the other side is unique.
 *
 * \param[in,out] info  The structure where the result is saved if available,
 *                      unchanged otherwise.
 *
 * \return true if info is set with the next link, false if no more
 *         links are available.
 */
bool link_context::next_link(link_info& info)
{
    // special case of a unique link
    if(f_info.is_unique())
    {
        // return the f_link entry once, then return false (no more data)
        if(f_link.isEmpty())
        {
            return false;
        }
        info.from_data(f_link);
        f_link.clear();
        return true;
    }

    // multiple links
    if(f_row)
    {
        QString links_namespace(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE));
        links_namespace += "::";
        QString name(f_info.name());
        if(!name.isEmpty())
        {
            name = links_namespace + name;
        }
        int const namespace_len(links_namespace.length());
        for(;;)
        {
            if(f_cell_iterator == f_cells.end())
            {
                // no more cells available in the cells, try to read more
                f_row->clearCache();
                f_row->readCells(f_column_predicate);
                f_cells = f_row->cells();
                f_cell_iterator = f_cells.begin();
                if(f_cell_iterator == f_cells.end())
                {
                    // no more cells available
                    f_row.reset();
                    return false;
                }
            }

            // the result is at the current iterator
            // note that from the links table we only get keys, no names
            // which does not matter as the name is f_info.name() anyway
            QString const link_key(QString::fromUtf8(f_cell_iterator.key()));
            QString const link_name(f_cell_iterator.value()->value().stringValue());
            if(!link_name.startsWith(links_namespace))
            {
                throw links_exception_invalid_name(QString("link name \"%1\" does not start with \"%2\"")
                            .arg(link_name)
                            .arg(links_namespace));
            }

            ++f_cell_iterator;

            // when the name is empty, every link is a match
            // otherwise make sure that the name starts as defined in the
            // input name (f_info)
            if(name.isEmpty() || name == link_name.left(name.length()))
            {
                // TODO: find the fastest way to determine the uniqueness?
                //       (right now we do not read that information...)

                // name is part of link_name after the namespace and before
                // the unique number which starts with a '-'
                //
                // we can start the search after the namespace since the
                // dash will not appear before that, also the name is at least
                // one character, hence the +1
                int const dash_pos(link_name.indexOf('-', namespace_len + 1));
                info.set_name(link_name.mid(namespace_len, dash_pos - namespace_len));

                // branch is found at the end after a '#'
                //
                // dash_pos + 2 because the unique number is at least 1
                // character and we can definitively skip the '-'
                int const hash_pos(link_name.indexOf('#', dash_pos + 2));
                info.set_branch(static_cast<int64_t>(link_name.mid(hash_pos + 1).toLongLong()));

                // the key (URI) of the destination
                info.set_key(link_key);

                return true;
            }
        }
        NOTREACHED();
    }

    // end of list reached (or there was no such link to start with...)
    return false;
}

/** \brief Return the key of the link.
 *
 * This is the key of the row, without the site key, where the column
 * of this link is saved (although the link may not exist.)
 *
 * \return The key of the row where the link is saved.
 */
//const QString& link_context::key() const
//{
//    return f_info.key();
//}

/** \brief Return the name of the link.
 *
 * This is the name of the column without the "links::" namespace.
 *
 * \return The name of the column where the link is saved.
 */
//const QString& link_context::name() const
//{
//    return f_info.name();
//}







/** \brief Initialize the links plugin.
 *
 * This function is used to initialize the links plugin object.
 */
links::links()
    //: f_snap(nullptr) -- auto-init
    //, f_links_table() -- auto-init
    //, f_branch_table() -- auto-init
{
}


/** \brief Clean up the links plugin.
 *
 * Ensure the links object is clean before it is gone.
 */
links::~links()
{
}


/** \brief Initialize the links plugin.
 *
 * This function terminates the initialization of the links plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void links::on_bootstrap(snap_child *snap)
{
    f_snap = snap;

    SNAP_LISTEN(links, "server", server, add_snap_expr_functions, _1);
    SNAP_LISTEN(links, "server", server, register_backend_action, _1);

    SNAP_TEST_PLUGIN_SUITE_LISTEN(links);
}


/** \brief Get a pointer to the links plugin.
 *
 * This function returns an instance pointer to the links plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the links plugin.
 */
links *links::instance()
{
    return g_plugin_links_factory.instance();
}


/** \brief Return the description of this plugin.
 *
 * This function returns the English description of this plugin.
 * The system presents that description when the user is offered to
 * install or uninstall a plugin on his website. Translation may be
 * available in the database.
 *
 * \return The description in a QString.
 */
QString links::description() const
{
    return "This plugin offers functions to link rows of data together."
        " For example, it allows you to attach a tag to the page of content."
        " This plugin is part of core since it links everything that core"
        " needs to make the system function as expected.";
}


/** \brief Check whether updates are necessary.
 *
 * This function updates the database when a newer version is installed
 * and the corresponding updates where not run.
 *
 * This works for newly installed plugins and older plugins that were
 * updated.
 *
 * \param[in] last_updated  The UTC Unix date when the website was last updated (in micro seconds).
 *
 * \return The UTC Unix date of the last update of this plugin.
 */
int64_t links::do_update(int64_t last_updated)
{
    SNAP_PLUGIN_UPDATE_INIT();

    SNAP_PLUGIN_UPDATE(2012, 1, 1, 0, 0, 0, initial_update);

    SNAP_PLUGIN_UPDATE_EXIT();
}


/** \brief First update to run for the links plugin.
 *
 * This function is the first update for the links plugin. It installs
 * the initial data required by the links plugin. Especially, it creates
 * the links table.
 *
 * \note
 * We reset the cached pointer to the table to make sure that they get
 * synchronized when used for the first time (very first initialization
 * only, do_update() is not generally called anyway, unless you are a
 * developer with the debug mode turned on.)
 *
 * \param[in] variables_timestamp  The timestamp for all the variables added
 *                        to the database by this update (in micro-seconds).
 */
void links::initial_update(int64_t variables_timestamp)
{
    NOTUSED(variables_timestamp);

    // read the links table to create it
    get_links_table();
    f_links_table.reset();
}


/** \brief Initialize the links table.
 *
 * This function creates the links table if it doesn't exist yet. Otherwise
 * it simple initializes the f_branch_table variable member.
 *
 * If the function is not able to create the table an exception is raised.
 *
 * \return The pointer to the links table.
 */
QtCassandra::QCassandraTable::pointer_t links::get_links_table()
{
    // retrieve links index table if not there yet
    if(!f_links_table)
    {
        f_links_table = f_snap->create_table(get_name(name_t::SNAP_NAME_LINKS_TABLE), "Links index table.");
    }
    return f_links_table;
}


/** \brief Initialize the content and links table.
 *
 * The first time one of the functions that require the links and contents
 * table runs, it calls this function to get the QCassandraTable.
 */
void links::init_tables()
{
    // retrieve links index table if not there yet
    get_links_table();

    // retrieve content table if not there yet
    if(!f_branch_table)
    {
        // TODO: remove this circular dependency on content plugin
        f_branch_table = content::content::instance()->get_branch_table();
    }
}


/** \brief Create a link between two rows.
 *
 * Links are always going both ways: the source links to the destination
 * and the destination to the source.
 *
 * If the source or destination links have a name that already exists in
 * the corresponding row and the unique flag is true, then that link will
 * be overwritten with the new information. If the unique flag is false,
 * then a new column is created unless that exact same link already exists
 * in that row.
 *
 * In order to test whether a link already exists we need to make use of
 * an index. This is done with the content of the link used as the key
 * of a column defined in the links table (ColumnFamily). This is very
 * important for very large data sets (i.e. think of a website with
 * one million pages, all of which would be of type "page". This means
 * one million links from the type "page" to the one million pages.)
 * We can forfeit the creation of that index for links marked as being
 * unique.
 *
 * A good example of unique link is a parent link (assuming a content
 * type can have only one parent.)
 *
 * References about indexes in Cassandra:
 * http://maxgrinev.com/2010/07/12/do-you-really-need-sql-to-do-it-all-in-cassandra/
 * http://stackoverflow.com/questions/3779239/how-do-i-filter-through-data-in-cassandra
 * http://www.datastax.com/docs/1.1/dml/using_cli#indexing-a-column
 *
 * Example:
 *
 * Say that:
 *
 * \li The source key is "example.com/test1"
 * \li The source name is "tag"
 * \li The destination key is "example.com/root/tags"
 * \li The destination name is "children"
 *
 * We create 2 to 4 entries as follow:
 *
 * \code
 * link table[source key][destination key] = source column number;
 * link table[destination key][source key] = destination column number;
 * branch table[source key][source name + source column number] = destination key;
 * branch table[destination key][destination name + destination column number] = source key;
 * \endcode
 *
 * If the source name is unique, then no link table entry for the source is
 * created and the source column number is empty ("").
 *
 * Similarly, if the destination name is unique, then no link table entry
 * for the destination is created and the destination column number is
 * empty ("").
 *
 * The link table is used as an index and for unique entries it is not required
 * since we already know where that data is
 * (i.e. the data saved in branch table[source key][source name .*] for the
 * source is the destination and we know exactly where it is.)
 *
 * \note
 * A link cannot be marked as unique once and non-unique another.
 * This is concidered an internal error. If you change your mind and already
 * released a plugin with a link defined one way, then you must change
 * the name in the next version.
 *
 * \todo
 * Find a way to test whether the caller changed the unicity and is about
 * to break something...
 *
 * \param[in] src  The source link
 * \param[in] dst  The destination link
 *
 * \sa snap_child::get_unique_number()
 */
void links::create_link(const link_info& src, const link_info& dst)
{
    // define the column names
    QString src_col, dst_col;

    // there is one special case: if a page is linked to itself (yes, it
    // happens, the page type of the system-page...); then the source and
    // destination names must differ otherwise we cannot read the link back
    if(src.key() == dst.key()
    && src.name() == dst.name())
    {
        throw links_exception_invalid_name(QString("when the source and destination are the same key (%1), then each must use a different name (%2)")
                    .arg(src.key())
                    .arg(src.name()));
    }

    init_tables();

    if(src.is_unique())
    {
        src_col = src.cell_name();
    }
    else
    {
        // not unique, first check whether it was already created
        QtCassandra::QCassandraValue value(f_links_table->row(src.link_key())->cell(dst.key())->value());
        if(value.nullValue())
        {
            // it does not exist, create a unique number
            src_col = src.cell_name(f_snap->get_unique_number());
            // save in the index table
            (*f_links_table)[src.link_key()][dst.key()] = QtCassandra::QCassandraValue(src_col);
        }
        else
        {
            // it exists, make use of the existing key
            src_col = value.stringValue();
        }
    }

    if(dst.is_unique())
    {
        dst_col = dst.cell_name();
    }
    else
    {
        // not unique, first check whether it was already created
        QtCassandra::QCassandraValue value(f_links_table->row(dst.link_key())->cell(src.key())->value());
        if(value.nullValue())
        {
            // it does not exist, create a unique number
            dst_col = dst.cell_name(f_snap->get_unique_number());
            // save in the index table
            (*f_links_table)[dst.link_key()][src.key()] = QtCassandra::QCassandraValue(dst_col);
        }
        else
        {
            // it exists, make use of the existing key
            dst_col = value.stringValue();
        }
    }

    // save the links in the rows (branches)
    // note that these two lines may just overwrite an already existing link
    (*f_branch_table)[src.row_key()][src_col] = dst.data(); // save dst in src
    (*f_branch_table)[dst.row_key()][dst_col] = src.data(); // save src in dst

    // signal that a link was modified
    // TODO: check whether a link is really created before sending this
    //       signal? (i.e. maybe it already existed...)
    modified_link(src, true);
    modified_link(dst, true);
}


/** \brief Create a new link context to read links from.
 *
 * This function creates a new link context instance using your
 * link_info information. The resulting context can be used to
 * read all the links using the next_link() function to read
 * the following link.
 *
 * Note that if no such link exists then the function returns a
 * link context which immediately returns false when next_link()
 * is called. On creation we do not count the number of links
 * because we do not know that number without reading all the
 * links.
 *
 * \param[in] info  The link key and name.
 * \param[in] count  Row count to fetch.
 *
 * \return A shared pointer to a link context, it will always exist.
 */
QSharedPointer<link_context> links::new_link_context(const link_info& info, const int count)
{
    QSharedPointer<link_context> context(new link_context(f_snap, info, count));
    return context;
}


/** \brief Read the list of existing links on this page.
 *
 * This function reads the list of links defined on this page.
 *
 * In most cases, you should not need to call this function because you
 * show already know what links are present on your page and thus be
 * able to access them without first having to list them. Also this
 * function is considered SLOW.
 *
 * \param[in] path  The path to the list to be read.
 *
 * \return An array of names with each one of the links.
 */
link_info_pair::vector_t links::list_of_links(QString const & path)
{
    link_info_pair::vector_t results;

    content::path_info_t ipath;
    ipath.set_path(path);

    content::content * content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t branch_table(content_plugin->get_branch_table());

    QtCassandra::QCassandraRow::pointer_t row(branch_table->row(ipath.get_branch_key()));

    QString const links_namespace_start(QString("%1::").arg(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE)));
    QString const links_namespace_end(QString("%1:;").arg(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE)));
    int const start_pos(links_namespace_start.length());

    QtCassandra::QCassandraColumnRangePredicate column_predicate;
    column_predicate.setCount(100);
    column_predicate.setIndex(); // behave like an index
    column_predicate.setStartColumnName(links_namespace_start); // limit the loading to links at least
    column_predicate.setEndColumnName(links_namespace_end);

    // loop until all cells are handled
    for(;;)
    {
        row->clearCache();
        row->readCells(column_predicate);
        QtCassandra::QCassandraCells const cells(row->cells());
        if(cells.isEmpty())
        {
            // no more cells
            break;
        }

        // handle one batch
        for(QtCassandra::QCassandraCells::const_iterator c(cells.begin());
                c != cells.end();
                ++c)
        {
            QtCassandra::QCassandraCell::pointer_t cell(*c);

            link_info src;
            src.set_key(ipath.get_key());

            QString const cell_name(cell->columnName());
            int const hash(cell_name.indexOf('#'));
            if(hash == -1)
            {
                throw links_exception_invalid_name("cell name includes no '-' and no '#' which is not valid for a link");
            }
            int pos(hash);
            int const dash(cell_name.indexOf('-'));
            if(dash != -1)
            {
                pos = dash;
            }
            QString const link_name(cell_name.mid(start_pos, pos - start_pos));
            src.set_name(link_name, dash == -1);

            // the multiple link number cannot be saved in the link_info
            // at this point... so we ignore it. For what we need links
            // for, it is fine.
            //if(dash != -1)
            //{
            //    QString const unique_number(cell_name.mid(dash + 1, hash - dash - 1));
            //    ... // nothing we can do with this one for now
            //}

            // the branch is defined after the '#'
            QString const branch_number(cell_name.mid(hash + 1));
            src.set_branch(branch_number.toLong());

            // this one we have all the data in the cell's value
            link_info dst;
            dst.from_data(cell->value().stringValue());

            link_info_pair pair(src, dst);
            results.push_back(pair);
        }
    }

    return results;
}



/** \brief Make sure that the specified link is deleted.
 *
 * Once two nodes are linked together, it is possible to remove that
 * link by calling this function.
 *
 * When nodes are linked with mode (1:1), then either node can be picked
 * to delete that link. Links created with (1:*) or (*:1) should pick the
 * node that had the (1) to remove just that one link. In all other cases,
 * all the links get deleted (which is useful when you delete something
 * such as a tag because all the pages that were linked to that tag must
 * not be linked to it anymore.)
 *
 * In order to find the data in the database, the info must be properly
 * initialized with the link name  and the full URI &amp; path to the link.
 * The unicity flag is ignored to better ensure that the link will be
 * deleted whether it is unique or not.
 *
 * If the link does not exist, nothing happens. Actually, when a multi-link
 * gets deleted, all problems are reported, but as many links that can be
 * deleted get deleted.
 *
 * \warning
 * If more than one computer tries to delete the same link at the same
 * time errors will ensue. This should be relatively rare though and most
 * certainly still be safe. However, if someone adds a link at the same
 * time as it gets deleted, the result can be that the new link gets
 * partially created and deleted.
 *
 * \param[in] info  The key and name of the link to be deleted.
 * \param[in] delete_record_count  The record count of the rows to select
 *                                 to be deleted.
 */
void links::delete_link(link_info const& info, int const delete_record_count)
{
    // here we assume that the is_unique() could be misleading
    // this way we can avoid all sorts of pitfalls where someone
    // creates a link with "*:1" and tries to delete it with "1:*"

    init_tables();

    if(!f_branch_table->exists(info.row_key()))
    {
        // probably not an error if the row does not even exist...
        return;
    }

    // note: we consider the content row defined in the info structure
    //       to be the source; obviously, as a result, the other one will
    //       be the destination
    QtCassandra::QCassandraRow::pointer_t src_row(f_branch_table->row(info.row_key()));

    // check if the link is defined as is (i.e. this info represents
    // a unique link, a "1")

    QString const unique_link_name(info.cell_name());
    if(src_row->exists(unique_link_name))
    {
        // we are here, this means it was a "1,1" or "1,*" link
        QtCassandra::QCassandraValue link(src_row->cell(unique_link_name)->value());

        // delete the source link right now
        src_row->dropCell(unique_link_name, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

        // we read the link so that way we have information about the
        // destination and can delete it too
        link_info destination;
        destination.from_data(link.stringValue());
        if(!f_branch_table->exists(destination.row_key()))
        {
            // still tell the system that the source page changed
            modified_link(info, false);

            SNAP_LOG_WARNING("links::delete_link() could not find the destination link for \"")
                        (destination.row_key())("\" (destination row missing in \"branch\" table).");
            return;
        }
        QtCassandra::QCassandraRow::pointer_t dst_row(f_branch_table->row(destination.row_key()));

        // to delete the link on the other side, we have to test whether
        // it is unique (1:1) or multiple (1:*)
        QString dest_cell_unique_name(destination.cell_name());
        if(dst_row->exists(dest_cell_unique_name))
        {
            // unique links are easy to handle!
            dst_row->dropCell(dest_cell_unique_name, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

            // in this case, it is easy enough; note that we first use
            // destination to match the other case on multiple links
            // (see the else part)
            modified_link(destination, false);
            modified_link(info, false);
        }
        else
        {
            // with a multiple link we have to use the links table to find the
            // exact destination
            if(!f_links_table->exists(destination.link_key()))
            {
                // still tell the system that the source page changed
                modified_link(info, false);

                // if the unique name does not exist,
                // then the multi-name must exist...
                SNAP_LOG_WARNING("links::delete_link() could not find the destination link for \"")
                            (destination.row_key())("\" (destination row missing in \"links\" table)).");
                return;
            }
            QtCassandra::QCassandraRow::pointer_t dst_multi_row(f_links_table->row(destination.link_key()));
            if(!dst_multi_row->exists(info.key()))
            {
                // still tell the system that the source page changed
                modified_link(info, false);

                // the destination does not exist anywhere!?
                // (this could happen in case the server crashes or something
                // of the sort...)
                SNAP_LOG_WARNING("links::delete_link() could not find the destination link for \"")
                            (destination.row_key())(" / ")
                            (info.key())("\" (cell missing in \"links\" table).");
                return;
            }
            // note that this is a multi-link, but in a (1:*) there is only
            // one destination that correspond to the (1:...) and thus only
            // one link that we need to load here
            QtCassandra::QCassandraValue destination_link(dst_multi_row->cell(info.key())->value());

            // we can drop that link immediately, since we got the information we needed
            // (this is a drop in the "links" table)
            dst_multi_row->dropCell(info.key(), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

            // TODO: should we drop the row if empty?
            //       I think it automatically happens when a row is empty
            //       (no more cells) then it gets removed by Cassandra anyway

            // this value represents the multi-name (i.e. <link namespace>::<link name>-<server name>-<number>)
            QString dest_cell_multi_name(destination_link.stringValue());
            if(dst_row->exists(dest_cell_multi_name))
            {
                dst_row->dropCell(dest_cell_multi_name, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

                // this worked as expected, tell that both destination and
                // source were changed (in that order to match the other
                // case where we delete all the destinations first and
                // call the signal on destinations first)
                modified_link(destination, false);
                modified_link(info, false);
            }
            else
            {
                // still tell the system that the source page changed
                modified_link(info, false);

                // again, this could happen if the server crashed or was
                // killed at the wrong time or another computer was deleting
                // under our feet
                SNAP_LOG_WARNING("links::delete_link() could not find the destination link for \"")
                            (destination.row_key())(" / ")
                            (dest_cell_multi_name)("\" (destination cell missing in \"branch\" table).");
                return;
            }
        }
    }
    else
    {
        // in this case we have a "*,1" or a "*,*" link
        // the links need to be loaded from the links table and there can
        // be many so we have to loop over the rows we read

        QString const links_namespace(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE));

        // here we get the row, we do not delete it yet because we need
        // to go through the whole list first
        QtCassandra::QCassandraRow::pointer_t row(f_links_table->row(info.link_key()));
        QtCassandra::QCassandraColumnRangePredicate column_predicate;
        // The columns names are keys (i.e. http://snap.m2osw.com/...)
        //column_predicate.setStartColumnName(QString("%1::").arg(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE)));
        //column_predicate.setEndColumnName(QString("%1;").arg(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE)));
        column_predicate.setCount(delete_record_count);
        column_predicate.setIndex(); // behave like an index
        bool modified(false);
        for(;;)
        {
            // we MUST clear the cache in case we read the same list of links twice
            row->clearCache();
            row->readCells(column_predicate);
            QtCassandra::QCassandraCells const cells(row->cells());
            if(cells.empty())
            {
                // all columns read
                break;
            }
            modified = true;
            for(QtCassandra::QCassandraCells::const_iterator cell_iterator(cells.begin()); cell_iterator != cells.end(); ++cell_iterator)
            //for(auto cell_iterator : cells) -- cannot use that one because we need the key
            {
                // from the cell key and value we compute the list info
                // from the destination of this link
                QString const key(QString::fromUtf8(cell_iterator.key()));
                QString const field_name(cell_iterator.value()->value().stringValue());

                if(!src_row->exists(field_name))
                {
                    // probably not an error if a link does not exist at all...
                    SNAP_LOG_WARNING("links::delete_link() could not find the destination link for \"")
                                (key)("\" with name \"")
                                (field_name)("\" (destination row missing in \"branch\" table.)");
                }
                else
                {
                    link_info destination_info;
                    destination_info.from_data(src_row->cell(field_name)->value().stringValue());

                    // drop the branch cell in the source page
                    src_row->dropCell(field_name, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

                    // drop the cell in the "links" table
                    row->dropCell(key, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

                    // drop the destination info
                    if(destination_info.is_unique())
                    {
                        // here we have a "*:1"
                        f_branch_table->row(destination_info.row_key())->dropCell(destination_info.cell_name(), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

                        // let others know that a link changed on a page
                        modified_link(destination_info, false);
                    }
                    else
                    {
                        QtCassandra::QCassandraRow::pointer_t dst_row(f_links_table->row(destination_info.link_key()));
                        if(dst_row->exists(info.key())) // should always be true
                        {
                            QString const dst_key(dst_row->cell(info.key())->value().stringValue());
                            dst_row->dropCell(info.key(), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());
                            f_branch_table->row(destination_info.row_key())->dropCell(dst_key, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

                            // let others know that a link changed on a page
                            modified_link(destination_info, false);
                        }
                    }
                }
            }
        }

        // NOTE: I'm wary of this simplification at this time; I think it
        //       works, but it is easier to understand the code if we
        //       delete each cell one by one
        //
        // finally we can delete this row
        //f_links_table->dropRow(info.link_key());

        // finally, tell that the source changed after all the drops
        // happened in the source;
        if(modified)
        {
            modified_link(info, false);
        }
    }
}


/** \brief Delete one specific link in a multi-linked list.
 *
 * If you need to delete one specific link in a multiple link to a
 * multiple link (*:*) list, then you cannot call the delete_link()
 * function because that would delete all the links on one or the
 * other sides.
 *
 * This function resolves that problem, but it requires you to
 * supply both sides of the link you want to delete (which you
 * probably have anyway if you want to delete just that one
 * link.)
 *
 * If one of \p source or \p destination have their unique flag
 * set to true, then this function calls the delete_link()
 * function since it will do exactly what needs to be done.
 *
 * \bug
 * This function does not (yet) check whether you lied when calling
 * it. If a link is not a multi-link, then the function fails deleting
 * the link on one side. It should be possible to fix the problem
 * and we will look into that later.
 *
 * \param[in] source  The source link.
 * \param[in] destination  The destination link.
 */
void links::delete_this_link(link_info const & source, link_info const & destination)
{
    if(source.is_unique())
    {
        delete_link(source);
        return;
    }

    if(destination.is_unique())
    {
        delete_link(destination);
        return;
    }

    init_tables();

    // drop the source info
    QtCassandra::QCassandraRow::pointer_t src_row(f_links_table->row(source.link_key()));
    if(src_row->exists(destination.key())) // should always be true
    {
        QString src_key(src_row->cell(destination.key())->value().stringValue());
        src_row->dropCell(destination.key(), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());
        f_branch_table->row(source.row_key())->dropCell(src_key, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

        modified_link(source, false);
    }

    // drop the destination info
    QtCassandra::QCassandraRow::pointer_t dst_row(f_links_table->row(destination.link_key()));
    if(dst_row->exists(source.key())) // should always be true
    {
        QString dst_key(dst_row->cell(source.key())->value().stringValue());
        dst_row->dropCell(source.key(), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());
        f_branch_table->row(destination.row_key())->dropCell(dst_key, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

        modified_link(destination, false);
    }
}


/** \brief Adjust the links after a clone_page() process.
 *
 * This function is called at the end of the clone_page process when
 * the page_cloned() signal is called. This is done from the
 * content::page_cloned_impl() since the links plugin cannot include
 * the content plugin from its header.
 *
 * \param[in] source_branch  The source page being cloned.
 * \param[in] destination_branch  The new page.
 */
void links::adjust_links_after_cloning(QString const& source_branch, QString const& destination_branch)
{
    init_tables();

    QtCassandra::QCassandraRow::pointer_t source_row(f_branch_table->row(source_branch));
    QtCassandra::QCassandraRow::pointer_t destination_row(f_branch_table->row(destination_branch));

    int const dst_branch_pos(destination_branch.indexOf('#'));
    QString const destination_uri(destination_branch.mid(0, dst_branch_pos));
    snap_version::version_number_t const branch_number(destination_branch.mid(dst_branch_pos + 1).toULong());

    QtCassandra::QCassandraColumnRangePredicate column_predicate;
    column_predicate.setStartColumnName(QString("%1::").arg(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE)));
    column_predicate.setEndColumnName(QString("%1;").arg(get_name(name_t::SNAP_NAME_LINKS_NAMESPACE)));
    column_predicate.setCount(100);
    column_predicate.setIndex(); // behave like an index
    int const src_branch_pos(source_branch.indexOf('#'));
    QString const source_uri(source_branch.mid(0, src_branch_pos));
    for(;;)
    {
        // we MUST clear the cache in case we read the same list of links twice
        source_row->clearCache();
        source_row->readCells(column_predicate);
        QtCassandra::QCassandraCells const cells(source_row->cells());
        if(cells.empty())
        {
            // all columns read
            break;
        }
        for(QtCassandra::QCassandraCells::const_iterator cell_iterator(cells.begin());
                                                         cell_iterator != cells.end();
                                                         ++cell_iterator)
        {
            QString const key(QString::fromUtf8(cell_iterator.key()));

            QString const dst_link(source_row->cell(key)->value().stringValue());
            link_info dst_li;
            dst_li.from_data(dst_link);

            QString const other_row(dst_li.row_key());
            if(other_row != destination_branch)
            {
                QString cell_name;
                if(dst_li.is_unique())
                {
                    cell_name = dst_li.cell_name();
                }
                else
                {
                    // in this case the info is in the links table
                    cell_name = f_links_table->row(dst_li.link_key())->cell(source_uri)->value().stringValue();
                }
                QtCassandra::QCassandraRow::pointer_t dst_row(f_branch_table->row(other_row));
                QString const src_link(dst_row->cell(cell_name)->value().stringValue());
                link_info src_li;
                src_li.from_data(src_link);

                QString const name(src_li.name());
                int const namespace_end(name.indexOf(':'));
                if(namespace_end <= 0)
                {
                    throw links_exception_invalid_name("invalid link field name, no namespace found");
                }
                QString const plugin_name(name.mid(0, namespace_end));
                plugins::plugin *plugin_owner(plugins::get_plugin(plugin_name));
                links_cloned *link_owner(dynamic_cast<links_cloned *>(plugin_owner));
                if(link_owner != nullptr)
                {
                    link_owner->repair_link_of_cloned_page(destination_uri, branch_number, src_li, dst_li, true);
                }
            }
        }
    }
}


void links::fix_branch_copy_link(QtCassandra::QCassandraCell::pointer_t source_cell, QtCassandra::QCassandraRow::pointer_t destination_row, snap_version::version_number_t const destination_branch_number)
{
    init_tables();

    // the source data is the destination link information
    QString const dst_link(source_cell->value().stringValue());
    link_info dst_li;
    dst_li.from_data(dst_link);
    QString const destination_key(destination_row->rowName());
    int const destination_branch_pos(destination_key.indexOf('#'));
    QString const destination_uri(destination_key.mid(0, destination_branch_pos));

    QtCassandra::QCassandraRow::pointer_t source_row(source_cell->parentRow());
    QString const source_key(source_row->rowName());
    int const source_branch_pos(source_key.indexOf('#'));
    QString const source_uri(source_key.mid(0, source_branch_pos));

    QString const other_row(dst_li.row_key());
    if(other_row != destination_key)
    {
        QString cell_name;
        if(dst_li.is_unique())
        {
            cell_name = dst_li.cell_name();
        }
        else
        {
            // in this case the info is in the links table
            cell_name = f_links_table->row(dst_li.link_key())->cell(source_uri)->value().stringValue();
        }
        QtCassandra::QCassandraRow::pointer_t dst_row(f_branch_table->row(other_row));
        QString const src_link(dst_row->cell(cell_name)->value().stringValue());
        link_info src_li;
        src_li.from_data(src_link);

        QString const name(src_li.name());
        int const namespace_end(name.indexOf(':'));
        if(namespace_end <= 0)
        {
            throw links_exception_invalid_name("invalid link field name, no namespace found");
        }
        QString const plugin_name(name.mid(0, namespace_end));
        plugins::plugin * plugin_owner(plugins::get_plugin(plugin_name));
        links_cloned * link_owner(dynamic_cast<links_cloned *>(plugin_owner));
        if(link_owner != nullptr)
        {
            // the repair itself is exactly the same as for a cloned page,
            // the link_owner may or may not re-create that link, voila
            link_owner->repair_link_of_cloned_page(destination_uri, destination_branch_number, src_li, dst_li, false);
        }
    }
}


namespace details
{

// TBD maybe this should be a taxonomy function and not directly a links option?
//     (it would remove some additional dependencies on the content plugin!)
void call_linked_to(snap_expr::variable_t & result, snap_expr::variable_t::variable_vector_t const & sub_results)
{
    if(sub_results.size() != 3
    && sub_results.size() != 4)
    {
        throw snap_expr::snap_expr_exception_invalid_number_of_parameters("invalid number of parameters to call linked_to() expected 3 or 4 parameters");
    }
    QString const link_name(sub_results[0].get_string("linked_to(1)"));
    QString const page(sub_results[1].get_string("linked_to(2)"));
    QString type_name(sub_results[2].get_string("linked_to(3)"));
    if(link_name.isEmpty()
    || page.isEmpty()
    || type_name.isEmpty())
    {
        throw snap_expr::snap_expr_exception_invalid_parameter_value("invalid parameters to call linked_to(), the first 3 parameters cannot be empty strings");
    }
    bool unique_link(true);
    if(sub_results.size() >= 4)
    {
        unique_link = sub_results[3].get_bool("linked_to(4)");
    }

    // if last char is '*' then the expected path is changed to
    // expected to start with path (startsWith() function instead of '==')
    // (Note: we know that type_name is not an empty string)
    bool const starts_with(*(type_name.end() - 1) == '*');
    if(starts_with)
    {
        type_name.remove(type_name.length() - 1, 1);
    }

    content::path_info_t ipath;
    ipath.set_path(page);
    link_info link_context_info(link_name, unique_link, ipath.get_key(), ipath.get_branch());
    QSharedPointer<link_context> link_ctxt(links::instance()->new_link_context(link_context_info));
    link_info result_info;
    bool r(false);
    if(link_ctxt->next_link(result_info))
    {
        content::path_info_t type_ipath;
        type_ipath.set_path(type_name);
        QString const expected_path(type_ipath.get_key());
        if(starts_with
                ? result_info.key().startsWith(expected_path)
                : result_info.key() == expected_path)
        {
            // is linked!
            r = true;
        }
        else if(!unique_link)
        {
            // not unique, check all the existing links
            while(link_ctxt->next_link(result_info))
            {
                if(starts_with
                        ? result_info.key().startsWith(expected_path)
                        : result_info.key() == expected_path)
                {
                    // is linked!
                    r = true;
                    break;
                }
            }
        }
    }
    result.set_value(r);
}


snap_expr::functions_t::function_call_table_t const links_functions[] =
{
    { // check whether a page is linked to a type
        "linked_to",
        call_linked_to
    },
    {
        nullptr,
        nullptr
    }
};


} // namespace details

void links::on_add_snap_expr_functions(snap_expr::functions_t& functions)
{
    functions.add_functions(details::links_functions);
}




SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et
