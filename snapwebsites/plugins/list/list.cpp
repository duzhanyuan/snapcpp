// Snap Websites Server -- advanced handling of lists
// Copyright (C) 2014  Made to Order Software Corp.
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

#include "list.h"

#include "../path/path.h"
#include "../output/output.h"

#include "not_reached.h"
#include "snap_expr.h"
#include "qdomhelpers.h"
#include "dbutils.h"
#include "log.h"

#include <iostream>

#include <sys/time.h>

#include "poison.h"


SNAP_PLUGIN_START(list, 1, 0)

/** \brief Get a fixed list name.
 *
 * The list plugin makes use of different names in the database. This
 * function ensures that you get the right spelling for a given name.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
char const *get_name(name_t name)
{
    switch(name)
    {
    case SNAP_NAME_LIST_ITEM_KEY_SCRIPT: // compiled
        return "list::item_key_script";

    case SNAP_NAME_LIST_KEY: // list of ordered pages
        return "list::key"; // + "::<list uri>" (cell includes <item sort key>)

    case SNAP_NAME_LIST_LAST_UPDATED:
        return "list::last_updated";

    case SNAP_NAME_LIST_LINK: // standard link between list and list items
        return "list::link";

    case SNAP_NAME_LIST_ORDERED_PAGES: // list of ordered pages
        return "list::ordered_pages"; // + "::<item sort key>"

    case SNAP_NAME_LIST_ORIGINAL_ITEM_KEY_SCRIPT: // text format
        return "list::original_item_key_script";

    case SNAP_NAME_LIST_ORIGINAL_TEST_SCRIPT: // text format
        return "list::original_test_script";

    case SNAP_NAME_LIST_PAGELIST: // --action pagelist
        return "pagelist";

    case SNAP_NAME_LIST_SELECTOR: // all, public, children, hand-picked, type=name, ...
        return "list::selector";

    case SNAP_NAME_LIST_STANDALONE: // when present in list table as a column name of a site row: signals a website managed as a standalone site
        return "*standalone*";

    case SNAP_NAME_LIST_STANDALONELIST: // --action standalonelist
        return "standalonelist";

    case SNAP_NAME_LIST_STOP: // STOP signal
        return "STOP";

    case SNAP_NAME_LIST_TABLE:
        return "list";

    case SNAP_NAME_LIST_THEME: // filter function
        return "list::theme";

    case SNAP_NAME_LIST_TEST_SCRIPT: // compiled
        return "list::test_script";

    case SNAP_NAME_LIST_TYPE:
        return "list::type";

    default:
        // invalid index
        throw snap_logic_exception("invalid SNAP_NAME_OUTPUT_...");

    }
    NOTREACHED();
}




/** \class list
 * \brief The list plugin to handle list of pages.
 *
 * The list plugin makes use of many references and links and thus it
 * is documented here:
 *
 *
 * 1) Pages that represent lists are all categorized under the following
 *    system content type:
 *
 * \code
 *     /types/taxonomy/system/list
 * \endcode
 *
 * We use that list to find all the lists defined on a website so we can
 * manage them all in our loops.
 *
 *
 *
 * 2) Items are linked to their list so that way when deleting an item
 *    we can immediately remove that item from that list. Note that an
 *    item may be part of many lists so it is a "multi" on both sides
 *    ("*:*").
 *
 *
 * 3) The list page includes links to all the items that are part of
 *    the list. These links do not use the standard link capability
 *    because the items are expected to be ordered and that is done
 *    using the Cassandra sort capability, in other words, we need
 *    to have a key which includes the sort parameters (i.e. an index).
 *
 * \code
 *    list::items::<sort key>
 * \endcode
 *
 * Important Note: This special link is double linked too, that is, the
 * item page links back to the standard list too (more precisly, it knows
 * of the special ordered key used in the list.) This is important to
 * make sure we can manage lists properly. That is, if the expression
 * used to calculate the key changes, then we could not instantly find
 * the old key anymore (i.e. we'd have to check each item in the list
 * to find the one that points to a given item... in a list with 1 million
 * pages, it would be really slow.)
 *
 *
 * Recap:
 *
 * \li Standard Link: List Page \<-\> /types/taxonomy/system/list
 * \li Standard Link: List Page \<-\> Item Page
 * \li Ordered List: List Page -\> Item Page,
 *                   Item Page includes key used in List Page
 */






/** \brief Initialize the list plugin.
 *
 * This function is used to initialize the list plugin object.
 */
list::list()
    //: f_snap(nullptr) -- auto-init
{
}


/** \brief Clean up the list plugin.
 *
 * Ensure the list object is clean before it is gone.
 */
list::~list()
{
}


/** \brief Initialize the list.
 *
 * This function terminates the initialization of the list plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void list::on_bootstrap(snap_child *snap)
{
    f_snap = snap;

    SNAP_LISTEN(list, "server", server, register_backend_action, _1);
    SNAP_LISTEN(list, "layout", layout::layout, generate_page_content, _1, _2, _3, _4);
    SNAP_LISTEN(list, "content", content::content, create_content, _1, _2, _3);
    SNAP_LISTEN(list, "content", content::content, modified_content, _1);
    SNAP_LISTEN(list, "filter", filter::filter, replace_token, _1, _2, _3, _4);
}


/** \brief Get a pointer to the list plugin.
 *
 * This function returns an instance pointer to the list plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the list plugin.
 */
list *list::instance()
{
    return g_plugin_list_factory.instance();
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
QString list::description() const
{
    return "Generate lists of pages using a set of parameters as defined"
          " by the system (some lists are defined internally) and the end"
          " users.";
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
int64_t list::do_update(int64_t last_updated)
{
    SNAP_PLUGIN_UPDATE_INIT();

    SNAP_PLUGIN_UPDATE(2012, 1, 1, 0, 0, 0, initial_update);
    SNAP_PLUGIN_UPDATE(2014, 2, 20, 13, 41, 30, content_update);

    SNAP_PLUGIN_UPDATE_EXIT();
}


/** \brief First update to run for the list plugin.
 *
 * This function is the first update for the list plugin. It creates
 * the list table.
 *
 * \param[in] variables_timestamp  The timestamp for all the variables added to the database by this update (in micro-seconds).
 */
void list::initial_update(int64_t variables_timestamp)
{
    static_cast<void>(variables_timestamp);

    get_list_table();
}


/** \brief Update the database with our content references.
 *
 * Send our content to the database so the system can find us when a
 * user references our pages.
 *
 * \param[in] variables_timestamp  The timestamp for all the variables added to the database by this update (in micro-seconds).
 */
void list::content_update(int64_t variables_timestamp)
{
    static_cast<void>(variables_timestamp);

    content::content::instance()->add_xml(get_plugin_name());
}


/** \brief Initialize the list table.
 *
 * This function creates the list table if it doesn't exist yet. Otherwise
 * it simple initializes the f_list_table variable member.
 *
 * If the function is not able to create the table an exception is raised.
 *
 * The list table is used to record all the pages of a website so they can
 * get sorted. As time passes older pages get removed as they are expected
 * to already be part of the list as required. Pages that are created or
 * modified are re-added to the list table so lists that include them can
 * be updated on the next run of the backend.
 *
 * New lists are created using a different scheme which is to find pages
 * using the list definitions to find said pages (i.e. all the pages link
 * under a given type, all the children of a given page, etc.)
 *
 * The table is defined as one row per website. The site_key_with_path()
 * is used as the row key. Within each row, you have one column per page
 * that was created or updated in the last little bit (until the backend
 * receives the time to work on all the lists concerned by such data.)
 * However, we need to time those entries so the column key is defined as
 * a 64 bit number representing the start date (as the
 * f_snap->get_start_date() returns) and the full key of the page that
 * was modified. This means the exact same page may appear multiple times
 * in the table. The backend is capable of ignoring duplicates.
 *
 * The content of the row is simple a boolean (signed char) set to 1.
 *
 * \return The pointer to the list table.
 */
QtCassandra::QCassandraTable::pointer_t list::get_list_table()
{
    if(!f_list_table)
    {
        f_list_table = f_snap->create_table(get_name(SNAP_NAME_LIST_TABLE), "Website list table.");
    }
    return f_list_table;
}


/** \brief Generate the page main content.
 *
 * This function generates the main content of the page. Other
 * plugins will also have the event called if they subscribed and
 * thus will be given a chance to add their own content to the
 * main page. This part is the one that (in most cases) appears
 * as the main content on the page although the content of some
 * columns may be interleaved with this content.
 *
 * Note that this is NOT the HTML output. It is the \<page\> tag of
 * the snap XML file format. The theme layout XSLT will be used
 * to generate the final output.
 *
 * \param[in,out] ipath  The path being managed.
 * \param[in,out] page  The page being generated.
 * \param[in,out] body  The body being generated.
 * \param[in] ctemplate  A fallback path in case ipath is not satisfactory.
 */
void list::on_generate_main_content(content::path_info_t& ipath, QDomElement& page, QDomElement& body, QString const& ctemplate)
{
    output::output::instance()->on_generate_main_content(ipath, page, body, ctemplate);
}


/** \brief Generate the page common content.
 *
 * This function generates some content that is expected in a page
 * by default.
 *
 * \param[in,out] ipath  The path being managed.
 * \param[in,out] page  The page being generated.
 * \param[in,out] body  The body being generated.
 * \param[in] ctemplate  The body being generated.
 */
void list::on_generate_page_content(content::path_info_t& ipath, QDomElement& page, QDomElement& body, QString const& ctemplate)
{
    static_cast<void>(ipath);
    static_cast<void>(page);
    static_cast<void>(body);
    static_cast<void>(ctemplate);
}


/** \brief Signal that a page was created.
 *
 * This function is called whenever the content plugin creates a new page.
 * At that point the page may not yet be complete so we could not handle
 * the possible list updates.
 *
 * So instead the function saves the full key to the page that was just
 * created so lists that include this page can be updated by the backend
 * as required.
 *
 * \param[in,out] ipath  The path to the page being modified.
 * \param[in] owner  The plugin owner of the page.
 * \param[in] type  The type of the page.
 */
void list::on_create_content(content::path_info_t& ipath, QString const& owner, QString const& type)
{
    static_cast<void>(owner);
    static_cast<void>(type);

    content::content *content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());

    // if a list is defined in this content, make sure to mark the
    // row as having a list with the last updated data set to zero
    //
    // Note: the exists() call is going to be very fast since the data will
    //       be in memory if true (if false, we still send a network request
    //       to Cassandra... but you never know in case the cache was reset!)
    //       this is going to be faster than such a test in the backend loop
    //       and replacing that with the test of the last update is going to
    //       make it a lot faster overall.
    QString const branch_key(ipath.get_branch_key());
    if(data_table->row(branch_key)->exists(get_name(SNAP_NAME_LIST_ORIGINAL_TEST_SCRIPT)))
    {
        QtCassandra::QCassandraTable::pointer_t content_table(content_plugin->get_content_table());

        // zero marks the list as brand new so we use a different
        // algorithm to check the data in that case (i.e. the list of
        // rows in the list table is NOT complete!)
        QString const key(ipath.get_key());
        int64_t const zero(0);
        content_table->row(key)->cell(get_name(SNAP_NAME_LIST_LAST_UPDATED))->setValue(zero);
    }

    on_modified_content(ipath); // same as on_modified_content()
}


/** \brief Signal that a page was modified.
 *
 * This function is called whenever a plugin modified a page and then called
 * the modified_content() signal of the content plugin.
 *
 * This function saves the full key to the page that was just modified so
 * lists that include this page can be updated by the backend as required.
 *
 * \todo
 * When a page is modified multiple times in the same request, as mentioned,
 * only the last request sticks (i.e. because all requests will use the
 * same start date). However, since the key used in the list table includes
 * start_date as the first 8 bytes, we do not detect the fact that we
 * end up with a duplicate when updating the same page in different requests.
 * I am thinking that we should be able to know the column to be deleted by
 * saving the key of the last entry in the page (ipath->get_key(), save
 * list::key or something of the sort.) One potential problem, though, is
 * that a page that is constantly modified may never get listed.
 *
 * \param[in,out] ipath  The path to the page being modified.
 */
void list::on_modified_content(content::path_info_t& ipath)
{
    // if the same page is modified multiple times then we overwrite the
    // same entry multiple times
    QString site_key(f_snap->get_site_key_with_slash());
    QtCassandra::QCassandraTable::pointer_t list_table(get_list_table());
    int64_t const start_date(f_snap->get_start_date());
    QByteArray key;
    QtCassandra::appendInt64Value(key, start_date);
    QtCassandra::appendStringValue(key, ipath.get_key());
    bool const modified(true);
    list_table->row(site_key)->cell(key)->setValue(modified);

    // just in case the row changed, we delete the pre-compiled (cached)
    // scripts (this could certainly be optimized but really the scripts
    // are compiled so quickly that it won't matter.)
    content::content *content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
    QString const branch_key(ipath.get_branch_key());
    data_table->row(branch_key)->dropCell(get_name(SNAP_NAME_LIST_TEST_SCRIPT), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, start_date);
    data_table->row(branch_key)->dropCell(get_name(SNAP_NAME_LIST_ITEM_KEY_SCRIPT), QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, start_date);
}


/** \brief Read a set of URIs from a list.
 *
 * This function reads a set of URIs from the list specified by \p ipath.
 *
 * The first item returned is defined by \p start. It is inclusive and the
 * very first item is number 0.
 *
 * The maximum number of items returned is defined by \p count. The number
 * may be set of -1 to returned as many items as there is available starting
 * from \p start. However, the function limits all returns to 10,000 items
 * so if the returned list is exactly 10,000 items, it is not unlikely that
 * you did not get all the items after the \p start point.
 *
 * The items are sorted by key as done by Cassandra.
 *
 * The count parameter cannot be set to zero. The function throws if you do
 * that.
 *
 * \todo
 * Note that at this point this function reads ALL item item from 0 to start
 * and throw them away. Later we'll add sub-indexes that will allow us to
 * reach any item very quickly. The sub-index will be something like this:
 *
 * \code
 *     list::index::100 = <key of the 100th item>
 *     list::index::200 = <key of the 200th item>
 *     ...
 * \endcode
 *
 * That way we can go to item 230 be starting the list scan at the 200th item.
 * We read the list::index:200 and us that key to start reading the list
 * (i.e. in the column predicate would use that key as the start key.)
 *
 * \param[in,out] ipath  The path to the list to be read.
 * \param[in] start  The first item to be returned.
 * \param[in] count  The number of items to return.
 *
 * \return The list of items
 */
list_item_vector_t list::read_list(content::path_info_t const& ipath, int start, int count)
{
    list_item_vector_t result;

    if(count == -1 || count > LIST_MAXIMUM_ITEMS)
    {
        count = LIST_MAXIMUM_ITEMS;
    }
    if(start < 0 || count <= 0)
    {
        throw snap_logic_exception(QString("list::read_list(ipath, %1, %2) called with invalid start and/or count values...")
                    .arg(start).arg(count));
    }

    content::content *content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());

    QString const branch_key(ipath.get_branch_key());
    QtCassandra::QCassandraRow::pointer_t list_row(data_table->row(branch_key));

    char const *ordered_pages(get_name(SNAP_NAME_LIST_ORDERED_PAGES));
    int const len(static_cast<int>(strlen(ordered_pages) + 2));

    QtCassandra::QCassandraColumnRangePredicate column_predicate;
    column_predicate.setStartColumnName(QString("%1::").arg(ordered_pages));
    column_predicate.setEndColumnName(QString("%1;").arg(ordered_pages));
    column_predicate.setCount(std::min(100, count)); // optimize the number of cells transferred
    column_predicate.setIndex(); // behave like an index
    for(;;)
    {
        // clear the cache before reading the next load
        list_row->clearCache();
        list_row->readCells(column_predicate);
        QtCassandra::QCassandraCells const cells(list_row->cells());
        if(cells.empty())
        {
            // all columns read
            break;
        }
        for(QtCassandra::QCassandraCells::const_iterator cell_iterator(cells.begin()); cell_iterator != cells.end(); ++cell_iterator)
        {
            if(start > 0)
            {
                --start;
            }
            else
            {
                // we keep the sort key in the item
                list_item_t item;
                item.set_sort_key(cell_iterator.key().mid(len));
                item.set_uri(cell_iterator.value()->value().stringValue());
                result.push_back(item);
                if(result.size() == count)
                {
                    break;
                }
            }
        }
    }

    return result;
}


/** \brief Register the pagelist action.
 *
 * This function registers this plugin as supporting the "pagelist" action.
 * This is used by the backend to continuously and as fast as possible build
 * lists of pages. It understands PINGs so one can wake this backend up as
 * soon as required.
 *
 * \param[in,out] actions  The list of supported actions where we add ourselves.
 */
void list::on_register_backend_action(server::backend_action_map_t& actions)
{
    actions[get_name(SNAP_NAME_LIST_PAGELIST)] = this;
    actions[get_name(SNAP_NAME_LIST_STANDALONELIST)] = this;
}


/** \brief Start the page list server.
 *
 * When running the backend the user can ask to run the pagelist
 * server (--action pagelist). This function captures those events.
 * It loops until stopped with a STOP message via the UDP address/port.
 * Note that Ctrl-C won't work because it does not support killing
 * both: the parent and child processes (we do a fork() to create
 * this child.)
 *
 * The loop updates all the lists as required, then it
 * falls asleep until the next UDP PING event received via the
 * pagelist_udp_signal IP:Port information.
 *
 * Note that because the UDP signals are not 100% reliable, the
 * server actually sleeps for 5 minutes and checks for new pages
 * whether a PING signal was received or not.
 *
 * The lists data is found in the Cassandra cluster and never
 * sent along the UDP signal. This means the UDP signals do not need
 * to be secure.
 *
 * The server should be stopped with the snapsignal tool using the
 * STOP event as follow:
 *
 * \code
 * snapsignal -a pagelist STOP
 * \endcode
 *
 * \note
 * The \p action parameter is here because some plugins may
 * understand multiple actions in which case we need to know
 * which action is waking us up.
 *
 * \param[in] action  The action this function is being called with.
 */
void list::on_backend_action(QString const& action)
{
    QtCassandra::QCassandraTable::pointer_t list_table(get_list_table());
    if(action == get_name(SNAP_NAME_LIST_PAGELIST))
    {

// Test creating just one link (*:*)
//content::path_info_t list_ipath;
//list_ipath.set_path("admin");
//content::path_info_t page_ipath;
//page_ipath.set_path("user");
//bool const source_unique(false);
//bool const destination_unique(false);
//links::link_info source("list::links_test", source_unique, list_ipath.get_key(), list_ipath.get_branch());
//links::link_info destination("list::links_test", destination_unique, page_ipath.get_key(), page_ipath.get_branch());
//links::links::instance()->create_link(source, destination);
//return;

        QSharedPointer<udp_client_server::udp_server> udp_signals(f_snap->udp_get_server("pagelist_udp_signal"));
        char const *stop(get_name(SNAP_NAME_LIST_STOP));
        // loop until stopped
        for(;;)
        {
            // work as long as there is work to do
            int did_work(1);
            while(did_work != 0)
            {
                did_work = 0;
                QtCassandra::QCassandraRowPredicate row_predicate;
                row_predicate.setCount(1000);
                for(;;)
                {
                    list_table->clearCache();
                    uint32_t count(list_table->readRows(row_predicate));
                    if(count == 0)
                    {
                        break;
                    }
                    QtCassandra::QCassandraRows const rows(list_table->rows());
                    for(QtCassandra::QCassandraRows::const_iterator o(rows.begin());
                            o != rows.end(); ++o)
                    {
                        // do not work on standalone websites
                        if(!(*o)->exists(get_name(SNAP_NAME_LIST_STANDALONE)))
                        {
                            QString const key(QString::fromUtf8(o.key().data()));
                            did_work |= generate_all_lists(key);
                        }
                    }
                }
            }

            // sleep till next PING (but max. 5 minutes)
            char buf[256];
            int r(udp_signals->timed_recv(buf, sizeof(buf), 5 * 60 * 1000)); // wait for up to 5 minutes (x 60 seconds)
            if(r != -1 || errno != EAGAIN)
            {
                if(r < 1 || r >= static_cast<int>(sizeof(buf) - 1))
                {
                    perror("udp_signals->timed_recv():");
                    SNAP_LOG_FATAL() << "error: an error occured in the UDP recv() call, returned size: " << r;
                    exit(1);
                }
                buf[r] = '\0';
                if(strcmp(buf, stop) == 0)
                {
                    // clean STOP
                    return;
                }
                // should we check that we really received a PING?
                //char const *ping(get_name(SNAP_NAME_SENDMAIL_PING));
                //if(strcmp(buf, ping) != 0)
                //{
                //    continue
                //}
            }
        }
    }
    else if(action == get_name(SNAP_NAME_LIST_STANDALONELIST))
    {
        // mark the site as a standalone website for its list management
        QString const site_key(f_snap->get_site_key_with_slash());
        int8_t const standalone(1);
        list_table->row(site_key)->cell(get_name(SNAP_NAME_LIST_STANDALONE))->setValue(standalone);
    }
    else
    {
        // unknown action (we should not have been called with that name!)
        throw snap_logic_exception(QString("list::on_backend_action(\"%1\") called with an unknown action...").arg(action));
    }
}


/** \brief Implementation of the backend process signal.
 *
 * This function captures the backend processing signal which is sent
 * by the server whenever the backend tool is run against a site.
 *
 * The list plugin refreshes lists of pages on websites when it receives
 * this signal.
 *
 * This backend may end up taking a lot of processing time and may need to
 * run very quickly (i.e. within seconds when a new page is created or a
 * page is modified). For this reason we also offer an action which supports
 * the PING signal.
 *
 * This backend process will actually NOT run if the PROCESS_LISTS parameter
 * is not defined on the command line:
 *
 * \code
 * snapbackend [--config snapserver.conf] --param PROCESS_LISTS=1
 * \endcode
 *
 * At this time the value used with PROCESS_LIST is not tested, however, it
 * is strongly recommended you use 1.
 */
void list::on_backend_process()
{
    // only process if the user clearly specified that we should do so;
    // we should never run in parallel with a background backend, hence
    // this flag (see the on_backend_action() function)
    QString const process_lists(f_snap->get_server_parameter("PROCESS_LISTS"));
    if(!process_lists.isEmpty())
    {
        // we ignore the result in this case, the backend will
        // run again soon and take care of the additional data
        // accordingly (with the action we process as much as
        // possible all in one go)
        generate_all_lists(f_snap->get_site_key_with_slash());
    }
}


/** \brief This function regenerates all the lists of all the websites.
 *
 * This function reads the complete list of all the lists as defined in the
 * lists table for each website defined in there.
 *
 * The process can take a very long time, especially if you have a large
 * number of websites with a lot of activity. For this reason the system
 * allows you to run this process on a backend server with the --action
 * command line option.
 *
 * The process is to:
 *
 * 1. go through all the rows of the list table (one row per website)
 * 2. go through all the columns of each row of the list table
 *    (one column per page that changed since the last update; note that
 *    it can continue to grow as we work on the list!)
 * 3. if the last update(s) happened more than LIST_PROCESSING_LATENCY
 *    then that specific page is processed and any list that include
 *    this page get updated appropriately
 * 4. entries that were processed between now and now + latency are
 *    ignored in this run (this way we avoid some problems where a client
 *    is still working on that page and thus the resulting sort of the
 *    list is not going to be accurate)
 *    TBD -- we may want to preprocess these and reprocess them at least
 *    LIST_PROCESSING_LATENCY later to make sure that the sort is correct;
 *    that way lists are still, in most cases, updated really quickly
 * 5. once we got a page that needs to be checked, we look whether this
 *    page is part of a list, if not then there is nothing to do
 *
 * \todo
 * At a later time we want to also add a way to mark a website as "standalone"
 * meaning that its lists are managed by a dedicated process (possibly even
 * a dedicated server.)
 *
 * \param[in] site_key  The site we want to process, if empty, process all
 *                      sites.
 *
 * \return 1 if the function changed anything, 0 otherwise
 */
int list::generate_all_lists(QString const& site_key)
{
    QtCassandra::QCassandraTable::pointer_t list_table(get_list_table());
    QtCassandra::QCassandraRow::pointer_t list_row(list_table->row(site_key));

    QtCassandra::QCassandraColumnRangePredicate column_predicate;
    column_predicate.setCount(100); // do one round then exit
    column_predicate.setIndex(); // behave like an index

    list_row->clearCache();
    list_row->readCells(column_predicate);
    QtCassandra::QCassandraCells const cells(list_row->cells());
    if(cells.isEmpty())
    {
        return 0;
    }

    int did_work(0);

    // handle one batch
    for(QtCassandra::QCassandraCells::const_iterator c(cells.begin());
            c != cells.end();
            ++c)
    {
        // we cannot just use f_snap->get_start_date() since in the backend
        // that date does not get updated...
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        int64_t const start_date = static_cast<int64_t>(tv.tv_sec) * static_cast<int64_t>(1000000)
                                 + static_cast<int64_t>(tv.tv_usec);

        // the cell
        QtCassandra::QCassandraCell::pointer_t cell(*c);
        // the key starts with the "start date" and it is followed by a
        // string representing the row key in the content table
        QByteArray const& key(cell->columnKey());

        // print out the row being worked on
        // (if it crashes it is really good to know where)
        {
            QString name;
            uint64_t time(QtCassandra::uint64Value(key, 0));
            char buf[64];
            struct tm t;
            time_t const seconds(time / 1000000);
            gmtime_r(&seconds, &t);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
            name = QString("%1.%2 (%3) %4").arg(buf).arg(time % 1000000, 6, 10, QChar('0')).arg(time).arg(QtCassandra::stringValue(key, sizeof(uint64_t)));
            SNAP_LOG_TRACE("work on column ")(name);
        }

        int64_t const page_start_date(QtCassandra::int64Value(key, 0));
        if(page_start_date + LIST_PROCESSING_LATENCY > start_date)
        {
            // since the columns are sorted, anything after that will be
            // inaccessible date wise
            break;
        }

        QString const row_key(QtCassandra::stringValue(key, sizeof(int64_t)));
        if(generate_all_lists_for_page(site_key, row_key) == 0)
        {
            did_work = 1;
        }

        // we handled that page for all the lists that we have on
        // this website, so drop it now
        list_row->dropCell(key, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());
    }

    // clear our cache
    f_check_expressions.clear();
    f_item_key_expressions.clear();

    return did_work;
}


int list::generate_all_lists_for_page(QString const& site_key, QString const& page_key)
{
    content::content *content_plugin(content::content::instance());
    QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
    content::path_info_t page_ipath;
    page_ipath.set_path(page_key);
    if(!data_table->exists(page_ipath.get_branch_key()))
    {
        // branch disappeared... ignore
        // (it could have been deleted or moved--i.e. renamed)
        return 0;
    }
    QtCassandra::QCassandraRow::pointer_t page_branch_row(data_table->row(page_ipath.get_branch_key()));

    int did_work(0);

    QString const link_name(get_name(SNAP_NAME_LIST_LINK));
    content::path_info_t ipath;
    ipath.set_path(site_key + "types/taxonomy/system/list");
    links::link_info info(get_name(SNAP_NAME_LIST_TYPE), false, ipath.get_key(), ipath.get_branch());
    QSharedPointer<links::link_context> link_ctxt(links::links::instance()->new_link_context(info));
    links::link_info child_info;
    while(link_ctxt->next_link(child_info))
    {
        // Entries are defined with the following:
        //
        // SNAP_NAME_LIST_ITEM_KEY_SCRIPT
        //    The script used to generate the item key used to sort items
        //    of the list.
        //
        // SNAP_NAME_LIST_KEY
        //    list::key::<list key>
        //
        //    The <list key> part is the the ipath.get_key() from the
        //    list page. This way we can find the lists this item is a
        //    part of.
        //
        // SNAP_NAME_LIST_ORDERED_PAGES
        //    list::ordered_pages::<item key>
        //
        //    The <item key> part is defined using the
        //    SNAP_NAME_LIST_ITEM_KEY_SCRIPT script. If not yet defined, use
        //    SNAP_NAME_LIST_ORIGINAL_ITEM_KEY_SCRIPT to create the compiled
        //    script. Note that this script may change under our feet so that
        //    means we'd lose access to the reference. For this reason, the
        //    reference is saved in the item under "list::key::<list key>".
        //
        // SNAP_NAME_LIST_ORIGINAL_ITEM_KEY_SCRIPT
        //    This cell includes the original script used to compute the
        //    item key. This script is compiled from the script in the
        //    SNAP_NAME_LIST_ITEM_KEY_SCRIPT.
        //
        // SNAP_NAME_LIST_TYPE
        //    The list type, used for the standard link of a list page to
        //    the list content type.
        //

        QString const key(child_info.key());
        content::path_info_t list_ipath;
        list_ipath.set_path(key);
        QString const list_key_in_page(QString("%1::%2").arg(get_name(SNAP_NAME_LIST_KEY)).arg(list_ipath.get_key()));
        QtCassandra::QCassandraRow::pointer_t list_row(data_table->row(list_ipath.get_branch_key()));
        bool const included(run_list_check(list_ipath, page_ipath));
        QString const new_item_key(run_list_item_key(list_ipath, page_ipath));
        QString const new_item_key_full(QString("%1::%2").arg(get_name(SNAP_NAME_LIST_ORDERED_PAGES)).arg(new_item_key));
        if(included)
        {
            // the check script says to include this item in this list;
            // first we need to check to find under which key it was
            // included if it is already there because it may have
            // changed
            if(page_branch_row->exists(list_key_in_page))
            {
                // check to see whether the current key changed
                QtCassandra::QCassandraValue current_item_key(page_branch_row->cell(list_key_in_page)->value());
                QString const current_item_key_full(QString("%1::%2").arg(get_name(SNAP_NAME_LIST_ORDERED_PAGES)).arg(current_item_key.stringValue()));
                if(current_item_key_full != new_item_key_full)
                {
                    // it changed, we have to delete the old one and
                    // create a new one
                    list_row->dropCell(current_item_key_full, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());
                    list_row->cell(new_item_key_full)->setValue(page_key);
                    page_branch_row->cell(list_key_in_page)->setValue(new_item_key);

                    did_work = 1;
                }
                // else -- nothing changed, we're done
            }
            else
            {
                // it doesn't exist yet, add it

                // create a standard link between the list and the page item
                bool const source_unique(false);
                bool const destination_unique(false);
                links::link_info source(link_name, source_unique, list_ipath.get_key(), list_ipath.get_branch());
                links::link_info destination(link_name, destination_unique, page_ipath.get_key(), page_ipath.get_branch());
                links::links::instance()->create_link(source, destination);

                // create the ordered list
                list_row->cell(new_item_key_full)->setValue(page_key);

                // save a back reference to the ordered list so we can
                // quickly find it
                page_branch_row->cell(list_key_in_page)->setValue(new_item_key);

                did_work = 1;
            }
        }
        else
        {
            // the check script says that this path is not included in this
            // list; the item may have been included earlier so we have to
            // make sure it gets removed if still there
            if(page_branch_row->exists(list_key_in_page))
            {
                QtCassandra::QCassandraValue current_item_key(page_branch_row->cell(list_key_in_page)->value());
                QString const current_item_key_full(QString("%1::%2").arg(get_name(SNAP_NAME_LIST_ORDERED_PAGES)).arg(current_item_key.stringValue()));

                list_row->dropCell(current_item_key_full, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());
                page_branch_row->dropCell(list_key_in_page, QtCassandra::QCassandraValue::TIMESTAMP_MODE_DEFINED, QtCassandra::QCassandra::timeofday());

                bool const source_unique(false);
                bool const destination_unique(false);
                links::link_info source(link_name, source_unique, list_ipath.get_key(), list_ipath.get_branch());
                links::link_info destination(link_name, destination_unique, page_ipath.get_key(), page_ipath.get_branch());
                links::links::instance()->delete_this_link(source, destination);

                did_work = 1;
            }
        }
    }

    return did_work;
}


/** \brief Retrieve the test script of a list.
 *
 * This function is used to extract the test script of a list object.
 * The test script is saved in the list::test_script field of a page,
 * on a per branch basis. This function makes use of the branch
 * defined in the ipath.
 *
 * \param[in,out] list_ipath  The ipath used to find the list.
 * \param[in,out] page_ipath  The ipath used to find the page.
 *
 * \return true if the page is to be included.
 */
bool list::run_list_check(content::path_info_t& list_ipath, content::path_info_t& page_ipath)
{
    QString const branch_key(list_ipath.get_branch_key());
    snap_expr::expr::expr_pointer_t e(nullptr);
    if(!f_check_expressions.contains(branch_key))
    {
        e = snap_expr::expr::expr_pointer_t(new snap_expr::expr);
        QByteArray program;
        content::content *content_plugin(content::content::instance());
        QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
        QtCassandra::QCassandraValue compiled_script(data_table->row(branch_key)->cell(get_name(SNAP_NAME_LIST_TEST_SCRIPT))->value());
        if(compiled_script.nullValue())
        {
            QtCassandra::QCassandraValue script(data_table->row(branch_key)->cell(get_name(SNAP_NAME_LIST_ORIGINAL_TEST_SCRIPT))->value());
            if(script.nullValue())
            {
                // no list here?!
                // TODO: generate an error
                return false;
            }
            if(!e->compile(script.stringValue()))
            {
                // script could not be compiled (invalid script!)
                // TODO: generate an error

                // create a default script so we do not try to compile the
                // broken script over and over again
                if(!e->compile("0"))
                {
                    // TODO: generate a double error!
                    //       this should really not happen
                    //       because "0" is definitively a valid script
                    return false;
                }
            }
            // save the result for next time
            data_table->row(branch_key)->cell(get_name(SNAP_NAME_LIST_TEST_SCRIPT))->setValue(e->serialize());
        }
        else
        {
            e->unserialize(compiled_script.binaryValue());
        }
        f_check_expressions[branch_key] = e;
    }
    else
    {
        e = f_check_expressions[branch_key];
    }

    // run the script with this path
    snap_expr::variable_t result;
    snap_expr::variable_t::variable_map_t variables;
    snap_expr::variable_t var_path("path");
    var_path.set_value(page_ipath.get_cpath());
    variables["path"] = var_path;
    snap_expr::variable_t var_page("page");
    var_page.set_value(page_ipath.get_key());
    variables["page"] = var_page;
    snap_expr::variable_t var_list("list");
    var_list.set_value(list_ipath.get_key());
    variables["list"] = var_list;
    snap_expr::functions_t functions;
    e->execute(result, variables, functions);

    return result.is_true();
}


/** \brief Generate the test script of a list.
 *
 * This function is used to extract the test script of a list object.
 * The test script is saved in the list::test_script field of a page,
 * on a per branch basis. This function makes use of the branch
 * defined in the ipath.
 *
 * \param[in,out] list_ipath  The ipath used to find the list.
 * \param[in,out] page_ipath  The ipath used to find the page.
 *
 * \return The item key as a string.
 */
QString list::run_list_item_key(content::path_info_t& list_ipath, content::path_info_t& page_ipath)
{
    QString const branch_key(list_ipath.get_branch_key());
    snap_expr::expr::expr_pointer_t e(nullptr);
    if(!f_item_key_expressions.contains(branch_key))
    {
        e = snap_expr::expr::expr_pointer_t(new snap_expr::expr);
        QByteArray program;
        content::content *content_plugin(content::content::instance());
        QtCassandra::QCassandraTable::pointer_t data_table(content_plugin->get_data_table());
        QtCassandra::QCassandraValue compiled_script(data_table->row(branch_key)->cell(get_name(SNAP_NAME_LIST_ITEM_KEY_SCRIPT))->value());
        if(compiled_script.nullValue())
        {
            QtCassandra::QCassandraValue script(data_table->row(branch_key)->cell(get_name(SNAP_NAME_LIST_ORIGINAL_ITEM_KEY_SCRIPT))->value());
            if(script.nullValue())
            {
                // no list here?!
                // TODO: generate an error
                return "";
            }
            if(!e->compile(script.stringValue()))
            {
                // script could not be compiled (invalid script!)
                // TODO: generate an error

                // create a default script so we do not try to compile the
                // broken script over and over again
                if(!e->compile("\"---\""))
                {
                    // TODO: generate a double error!
                    //       this should really not happen
                    //       because "0" is definitively a valid script
                    return "";
                }
            }
            // save the result for next time
            data_table->row(branch_key)->cell(get_name(SNAP_NAME_LIST_ITEM_KEY_SCRIPT))->setValue(e->serialize());
        }
        else
        {
            e->unserialize(compiled_script.binaryValue());
        }
        f_item_key_expressions[branch_key] = e;
    }
    else
    {
        e = f_item_key_expressions[branch_key];
    }

    // run the script with this path
    snap_expr::variable_t result;
    snap_expr::variable_t::variable_map_t variables;
    snap_expr::variable_t var_path("path");
    var_path.set_value(page_ipath.get_cpath());
    variables["path"] = var_path;
    snap_expr::variable_t var_page("page");
    var_page.set_value(page_ipath.get_key());
    variables["page"] = var_page;
    snap_expr::variable_t var_list("list");
    var_list.set_value(list_ipath.get_key());
    variables["list"] = var_list;
    snap_expr::functions_t functions;
    e->execute(result, variables, functions);

    return result.get_string("*result*");
}


/** \brief Replace a [list::...] token with the contents of a list.
 *
 * This function replaces the list tokens with themed lists.
 *
 * The supported tokens are:
 *
 * \li [list::theme(path="\<list path\>", theme="\<theme name\>", start="\<start\>", count="\<count\>")]
 *
 * Theme the list define at \<list path\> with the theme \<theme name\>.
 * You may skip some items and start with item \<start\> instead of item 0.
 * You may specified the number of items to display with \<count\>. Be
 * careful because by default all the items are shown (Although there is a
 * system limit which at this time is 10,000 that still a LARGE list!)
 * The them name, start, and count paramters are all optional.
 *
 * \param[in,out] ipath  The path to the page being worked on.
 * \param[in] plugin_owner  The plugin owner of the ipath data.
 * \param[in,out] xml  The XML document used with the layout.
 * \param[in,out] token  The token object, with the token name and optional parameters.
 */
void list::on_replace_token(content::path_info_t& ipath, QString const& plugin_owner, QDomDocument& xml, filter::filter::token_info_t& token)
{
    static_cast<void>(ipath);
    static_cast<void>(plugin_owner);
    static_cast<void>(xml);

    // a list::... token?
    if(!token.is_namespace("list::"))
    {
        return;
    }

    if(token.is_token(get_name(SNAP_NAME_LIST_THEME)))
    {
        // list::theme expects one to four parameters
        if(!token.verify_args(1, 4))
        {
            return;
        }

        // Path
        filter::filter::parameter_t path_param(token.get_arg("path", 0, filter::filter::TOK_STRING));
        if(token.f_error)
        {
            return;
        }
        if(path_param.f_value.isEmpty())
        {
            token.f_error = true;
            token.f_replacement = "<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list path (first parameter) of the list::theme() function cannot be an empty string.</span>";
            return;
        }

        // Theme
        QString theme("qrc:/xsl/list/default"); // default theming, simple <ul>{<li>...</li>}</ul> list
        if(token.has_arg("theme", 1))
        {
            filter::filter::parameter_t theme_param(token.get_arg("theme", 1, filter::filter::TOK_STRING));
            if(token.f_error)
            {
                return;
            }
            // if user uses "" ignore it
            if(theme_param.f_value.endsWith(".xsl"))
            {
                theme_param.f_value = theme_param.f_value.left(theme_param.f_value.length() - 4);
            }
            if(!theme_param.f_value.isEmpty())
            {
                theme = theme_param.f_value;
            }
        }

        // Start
        int start(0); // start with very first item
        if(token.has_arg("start", 2))
        {
            filter::filter::parameter_t start_param(token.get_arg("start", 2, filter::filter::TOK_INTEGER));
            if(token.f_error)
            {
                return;
            }
            bool ok(false);
            start = start_param.f_value.toInt(&ok, 10);
            if(!ok)
            {
                token.f_error = true;
                token.f_replacement = "<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list start (third parameter) of the list::theme() function must be a valid integer.</span>";
                return;
            }
            if(start < 0)
            {
                token.f_error = true;
                token.f_replacement = "<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list start (third parameter) of the list::theme() function must be a positive integer or zero.</span>";
                return;
            }
        }

        // Count
        int count(-1); // all items
        if(token.has_arg("count", 3))
        {
            filter::filter::parameter_t count_param(token.get_arg("count", 3, filter::filter::TOK_INTEGER));
            if(token.f_error)
            {
                return;
            }
            bool ok(false);
            count = count_param.f_value.toInt(&ok, 10);
            if(!ok)
            {
                token.f_error = true;
                token.f_replacement = "<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list count (forth parameter) of the list::theme() function must be a valid integer.</span>";
                return;
            }
            if(count != -1 && count <= 0)
            {
                token.f_error = true;
                token.f_replacement = "<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list count (forth parameter) of the list::theme() function must be a valid integer large than zero or -1.</span>";
                return;
            }
        }

        content::path_info_t list_ipath;
        list_ipath.set_path(path_param.f_value);

        quiet_error_callback list_error_callback(f_snap, true);
        plugin *list_plugin(path::path::instance()->get_plugin(list_ipath, list_error_callback));
        if(!list_error_callback.has_error() && list_plugin)
        {
            layout::layout_content *list_content(dynamic_cast<layout::layout_content *>(list_plugin));
            if(list_content == nullptr)
            {
                f_snap->die(snap_child::HTTP_CODE_INTERNAL_SERVER_ERROR,
                        "Plugin Missing",
                        "Plugin \"" + list_plugin->get_plugin_name() + "\" does not know how to handle a list assigned to it.",
                        "list::on_replace_token() the plugin does not derive from layout::layout_content.");
                NOTREACHED();
            }

            // read the list of items
            list_item_vector_t items(read_list(list_ipath, start, count));
            snap_child::post_file_t f;

            // Load the list body
            f.set_filename(theme + "-list-body.xsl");
            if(!f_snap->load_file(f) || f.get_size() == 0)
            {
                token.f_error = true;
                token.f_replacement = QString("<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list theme (%1-list-body.xsl) could not be loaded.</span>")
                                            .arg(theme);
                return;
            }
            QString const list_body_xsl(QString::fromUtf8(f.get_data()));

            // Load the list theme
            f.set_filename(theme + "-list-theme.xsl");
            if(!f_snap->load_file(f) || f.get_size() == 0)
            {
                token.f_error = true;
                token.f_replacement = QString("<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list theme (%1-list-theme.xsl) could not be loaded.</span>")
                                            .arg(theme);
                return;
            }
            QString const list_theme_xsl(QString::fromUtf8(f.get_data()));

            // Load the item body
            f.set_filename(theme + "-item-body.xsl");
            if(!f_snap->load_file(f) || f.get_size() == 0)
            {
                token.f_error = true;
                token.f_replacement = QString("<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list theme (%1-item-theme.xsl) could not be loaded.</span>")
                                            .arg(theme);
                return;
            }
            QString const item_body_xsl(QString::fromUtf8(f.get_data()));

            // Load the item theme
            f.set_filename(theme + "-item-theme.xsl");
            if(!f_snap->load_file(f) || f.get_size() == 0)
            {
                token.f_error = true;
                token.f_replacement = QString("<span class=\"filter-error\"><span class=\"filter-error-word\">ERROR:</span> list theme (%1-item-theme.xsl) could not be loaded.</span>")
                                            .arg(theme);
                return;
            }
            QString const item_theme_xsl(QString::fromUtf8(f.get_data()));

            layout::layout *layout_plugin(layout::layout::instance());
            QDomDocument list_doc(layout_plugin->create_document(list_ipath, list_plugin));
            layout_plugin->create_body(list_doc, list_ipath, list_body_xsl, list_content);

            QDomElement body(snap_dom::get_element(list_doc, "body"));
            QDomElement list_element(list_doc.createElement("list"));
            body.appendChild(list_element);

            // now theme the list
            int const max_items(items.size());
            for(int i(0), index(1); i < max_items; ++i)
            {
                list_error_callback.clear_error();
                content::path_info_t item_ipath;
                item_ipath.set_path(items[i].get_uri());
                item_ipath.set_parameter("action", "view"); // at this point we only support viewing list items
                plugin *item_plugin(path::path::instance()->get_plugin(item_ipath, list_error_callback));
                if(!list_error_callback.has_error() && item_plugin)
                {
                    // put each box in a filter tag so that way we have
                    // a different owner and path for each
                    QDomDocument item_doc(layout_plugin->create_document(item_ipath, item_plugin));

                    layout_plugin->create_body(item_doc, item_ipath, item_body_xsl, dynamic_cast<layout_content *>(item_plugin));
//std::cerr << "source to be parsed [" << item_doc.toString() << "]\n";
                    QDomElement item_body(snap_dom::get_element(item_doc, "body"));
                    item_body.setAttribute("index", index);
                    QString themed_item(layout_plugin->apply_theme(item_doc, item_theme_xsl));
//std::cerr << "themed item [" << themed_item << "]\n";

                    // add that result to the list document
                    QDomElement item(list_doc.createElement("item"));
                    list_element.appendChild(item);
                    snap_dom::insert_html_string_to_xml_doc(item, themed_item);

                    ++index; // index only counts items added to the output
                }
            }
//std::cerr << "resulting XML [" << list_doc.toString() << "]\n";

            // now theme the list as a whole
            // we add a wrapper so we can use /node()/* in the final theme
            token.f_replacement = layout_plugin->apply_theme(list_doc, list_theme_xsl);
        }
        // else list is not accessible (permission "problem")
    }
}


void list::on_generate_boxes_content(content::path_info_t& page_cpath, content::path_info_t& ipath, QDomElement& page, QDomElement& box, QString const& ctemplate)
{
    static_cast<void>(page_cpath);

    output::output::instance()->on_generate_main_content(ipath, page, box, ctemplate);
}


SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et
