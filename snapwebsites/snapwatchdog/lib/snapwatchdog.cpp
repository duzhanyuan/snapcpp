// Snap Websites Server -- snap watchdog daemon
// Copyright (C) 2011-2015  Made to Order Software Corp.
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

#include "snapwatchdog.h"

#include <snapwebsites/log.h>
#include <snapwebsites/snap_cassandra.h>
#include <snapwebsites/qdomhelpers.h>

#include <fstream>

#include <sys/wait.h>

#include "poison.h"


/** \file
 * \brief This file represents the Snap! Watchdog daemon.
 *
 * The snapwatchdog.cpp and corresponding header file represents the Snap!
 * Watchdog daemon. This is not exactly a server, although it somewhat
 * (mostly) behaves like one. This tool is used as a daemon to make
 * sure that various resources on a server remain available as expected.
 */


/** \mainpage
 * \brief Snap! Watchdog Documentation
 *
 * \section introduction Introduction
 *
 * The Snap! Watchdog is a tool that works in unisson with Snap! C++.
 * It is used to monitor all the servers used with Snap! in order to
 * ensure that they all continuously work as expected.
 */


namespace snap
{

// definitions from the plugins so we can define the name and filename of
// the server plugin
namespace plugins
{
extern QString g_next_register_name;
extern QString g_next_register_filename;
}


namespace
{


/** \brief The snap communicator singleton.
 *
 * This variable holds a copy of the snap communicator singleton.
 * It is a null pointer until the watchdog() function is called.
 */
snap_communicator::pointer_t g_communicator;


/** \brief The timer to produce ticks once every minute.
 *
 * This timer is the one used to know when to gather the data again.
 *
 * By default the interval is set to one minute, although it is possible
 * to change that amount in the configuration file.
 */
class tick_timer
        : public snap::snap_communicator::snap_timer
{
public:
    typedef std::shared_ptr<tick_timer>        pointer_t;

                                tick_timer( watchdog_server::pointer_t ws, int64_t interval );

    // snap::snap_communicator::snap_timer implementation
    virtual void                process_timeout();

private:
    watchdog_server::pointer_t  f_watchdog_server;
};


/** \brief The tick timer.
 *
 * We create one tick timer. It is saved in this variable if needed.
 */
tick_timer::pointer_t             g_tick_timer;


/** \brief Initializes the timer with a pointer to the snap backend.
 *
 * The constructor saves the pointer of the snap_backend object so
 * it can later be used when the process times out.
 *
 * The timer is setup to trigger immediately after creation.
 * This is what starts the snap backend process.
 *
 * \param[in] sb  A pointer to the snap_backend object.
 */
tick_timer::tick_timer(watchdog_server::pointer_t ws, int64_t interval)
    : snap_timer(interval)
    , f_watchdog_server(ws)
{
    set_name("watchdog_server tick_timer");

    // start right away, but we do not want to use snap_timer(0)
    // because otherwise we will not get ongoing ticks as expected
    //
    set_timeout_date(snap_communicator::get_current_date());
}


/** \brief The timeout happened.
 *
 * This function gets called once every minute (although the interval can
 * be changed, it is 1 minute by default.) Whenever it happens, the
 * watchdog runs all the plugins once.
 */
void tick_timer::process_timeout()
{
    f_watchdog_server->process_tick();
}





/** \brief Handle messages from the Snap Communicator server.
 *
 * This class is an implementation of the TCP client message connection
 * so we can handle incoming messages.
 */
class messager
        : public snap_communicator::snap_tcp_client_permanent_message_connection
{
public:
    typedef std::shared_ptr<messager>    pointer_t;

                                messager(watchdog_server::pointer_t ws, std::string const & addr, int port);

    // snap::snap_communicator::snap_tcp_client_permanent_message_connection implementation
    virtual void                process_message(snap::snap_communicator_message const & message);
    virtual void                process_connection_failed(std::string const & error_message);
    virtual void                process_connected();

private:
    // this is owned by a server function so no need for a smart pointer
    watchdog_server::pointer_t  f_watchdog_server;
};


/** \brief The messager.
 *
 * We create only one messager. It is saved in this variable.
 */
messager::pointer_t             g_messager;


/** \brief The messager initialization.
 *
 * The messager is a connection to the snapcommunicator server.
 *
 * In most cases we receive STOP and LOG messages from it. We implement
 * a few other messages too (HELP, READY...)
 *
 * We use a permanent connection so if the snapcommunicator restarts
 * for whatever reason, we reconnect automatically.
 *
 * \param[in] ws  The snap watchdog server we are listening for.
 * \param[in] addr  The address to connect to. Most often it is 127.0.0.1.
 * \param[in] port  The port to listen on (4040).
 */
messager::messager(watchdog_server::pointer_t ws, std::string const & addr, int port)
    : snap_tcp_client_permanent_message_connection(
                addr,
                port,
                tcp_client_server::bio_client::mode_t::MODE_PLAIN,
                snap_communicator::snap_tcp_client_permanent_message_connection::DEFAULT_PAUSE_BEFORE_RECONNECTING,
                false) // do not use a separate thread, we do many fork()'s
    , f_watchdog_server(ws)
{
    set_name("watchdog_server messager");
}


/** \brief Pass messages to the Snap Backend.
 *
 * This callback is called whenever a message is received from
 * Snap! Communicator. The message is immediately forwarded to the
 * watchdog_server object which is expected to process it and reply
 * if required.
 *
 * \param[in] message  The message we just received.
 */
void messager::process_message(snap::snap_communicator_message const & message)
{
    f_watchdog_server->process_message(message);
}


/** \brief The messager could not connect to snapcommunicator.
 *
 * This function is called whenever the messagers fails to
 * connect to the snapcommunicator server. This could be
 * because snapcommunicator is not running or because the
 * information given to the snapwatchdog is wrong...
 *
 * With snapinit the snapcommunicator should always already
 * be running so this error should not happen once everything
 * is properly setup.
 *
 * \param[in] error_message  An error message.
 */
void messager::process_connection_failed(std::string const & error_message)
{
    SNAP_LOG_ERROR("connection to snapcommunicator failed (")(error_message)(")");

    // also call the default function, just in case
    snap_tcp_client_permanent_message_connection::process_connection_failed(error_message);
}


/** \brief The connection was established with Snap! Communicator.
 *
 * Whenever the connection is establied with the Snap! Communicator,
 * this callback function is called.
 *
 * The messager reacts by REGISTERing "snapwatchdog" service with the
 * Snap! Communicator.
 */
void messager::process_connected()
{
    snap_tcp_client_permanent_message_connection::process_connected();

    snap::snap_communicator_message register_backend;
    register_backend.set_command("REGISTER");
    register_backend.add_parameter("service", "snapwatchdog");
    register_backend.add_parameter("version", snap::snap_communicator::VERSION);
    send_message(register_backend);
}




/** \brief Handle the death of a child process.
 *
 * This class is an implementation of the snap signal connection so we can
 * get an event whenever our child dies.
 */
class sigchld_connection
        : public snap::snap_communicator::snap_signal
{
public:
    typedef std::shared_ptr<sigchld_connection>    pointer_t;

                                sigchld_connection(watchdog_server::pointer_t ws);

    // snap::snap_communicator::snap_signal implementation
    virtual void                process_signal();

private:
    // this is owned by a server function so no need for a smart pointer
    watchdog_server::pointer_t  f_watchdog_server;
};


/** \brief A pointer to the child signal connection.
 *
 * When adding connections we include this one so we can capture the
 * death of the child we create to run the statistic gathering using
 * plugins.
 */
sigchld_connection::pointer_t       g_sigchld_connection;


/** \brief The SIGCHLD signal initialization.
 *
 * The constructor defines this signal connection as a listener for
 * the SIGCHLD signal.
 *
 * \param[in] ws  The watchdog server we are listening for.
 */
sigchld_connection::sigchld_connection(watchdog_server::pointer_t ws)
    : snap_signal(SIGCHLD)
    , f_watchdog_server(ws)
{
}


/** \brief Process the child death signal.
 *
 * The watchdog_server process received a SIGCHLD. We can call the
 * process_sigchld() function of the watchdog_server object.
 */
void sigchld_connection::process_signal()
{
    // we can call the same function
    f_watchdog_server->process_sigchld();
}




} // no name namespace


namespace watchdog
{

/** \brief Get a fixed watchdog plugin name.
 *
 * The watchdog plugin makes use of different fixed names. This function
 * ensures that you always get the right spelling for a given name.
 *
 * \param[in] name  The name to retrieve.
 *
 * \return A pointer to the name.
 */
char const * get_name(name_t name)
{
    switch(name)
    {
    case name_t::SNAP_NAME_WATCHDOG_DATA_PATH:
        return "data_path";

    case name_t::SNAP_NAME_WATCHDOG_SERVER_NAME:
        return "server_name";

    case name_t::SNAP_NAME_WATCHDOG_SERVERSTATS:
        return "serverstats";

    case name_t::SNAP_NAME_WATCHDOG_STATISTICS_FREQUENCY:
        return "statistics_frequency";

    case name_t::SNAP_NAME_WATCHDOG_STATISTICS_PERIOD:
        return "statistics_period";

    case name_t::SNAP_NAME_WATCHDOG_STATISTICS_TTL:
        return "statistics_ttl";

    default:
        // invalid index
        throw snap_logic_exception("Invalid SNAP_NAME_WATCHDOG_CPU_...");

    }
    NOTREACHED();
}

}


/** \brief Initialize the watchdog server.
 *
 * This constructor makes sure to setup the correct filename for the
 * snapwatchdog server configuration file.
 */
watchdog_server::watchdog_server()
{
    set_default_config_filename( "/etc/snapwebsites/snapwatchdog.conf" );
}


/** \brief Retrieve a pointer to the watchdog server.
 *
 * This function retrieve an instance pointer of the watchdog server.
 * If the instance does not exist yet, then it gets created. A
 * server is also a plugin which is named "server".
 *
 * \return A manager pointer to the watchdog server.
 */
watchdog_server::pointer_t watchdog_server::instance()
{
    server::pointer_t s(get_instance());
    if(!s)
    {
        plugins::g_next_register_name = "server";
        plugins::g_next_register_filename = __FILE__;

        s = set_instance(server::pointer_t(new watchdog_server));

        plugins::g_next_register_name.clear();
        plugins::g_next_register_filename.clear();
    }
    return std::dynamic_pointer_cast<watchdog_server>(s);
}


/** \brief Print the version string to stderr.
 *
 * This function prints out the version string of this server to the standard
 * error stream.
 *
 * This is a virtual function so that way servers and daemons that derive
 * from snap::server have a chance to show their own version.
 */
void watchdog_server::show_version()
{
    std::cerr << SNAPWATCHDOG_VERSION_STRING << std::endl;
}


/** \brief Finish watchdog initialization and start the event loop.
 *
 * This function finishes the initialization such as defining the
 * server name, check that cassandra is available, and create various
 * connections such as the messager to communicate with the
 * snapcommunicator service.
 */
void watchdog_server::watchdog()
{
    SNAP_LOG_INFO("------------------------------------ snapwatchdog started on ")(get_parameter("server_name"));

    check_cassandra();
    init_parameters();

    // TODO: test that the "sites" table is available?
    //       (we will not need any such table here)

    g_communicator = snap_communicator::instance();

    // get the snapcommunicator IP and port
    QString communicator_addr("127.0.0.1");
    int communicator_port(4040);
    tcp_client_server::get_addr_port(get_parameter("snapcommunicator_listen"), communicator_addr, communicator_port, "tcp");

    // create the messager, a connection between the snapwatchdogserver
    // and the snapcommunicator which allows us to communicate with
    // the watchdog (STATUS and STOP especially, more later)
    //
    g_messager.reset(new messager(instance(), communicator_addr.toUtf8().data(), communicator_port));
    g_communicator->add_connection(g_messager);

    // add the ticker, this wakes the system up once in a while so
    // we can gather statistics at a given interval
    //
    g_tick_timer.reset(new tick_timer(instance(), f_statistics_frequency));
    g_communicator->add_connection(g_tick_timer);

    // create a signal handler that knows when the child dies.
    //
    g_sigchld_connection.reset(new sigchld_connection(instance()));
    g_communicator->add_connection(g_sigchld_connection);

    // now start the run() loop
    //
    g_communicator->run();
}


/** \brief Process one tick.
 *
 * This function is called once a minute (by default). It goes and gather
 * all the data from all the plugins and then save that in the database.
 *
 * In case the tick happens too often, the function makes sure that the
 * child process is started at most once.
 */
void watchdog_server::process_tick()
{
    if(!f_processes)
    {
        f_processes.reset(new watchdog_child(instance()));
        f_processes->run_watchdog_plugins();
    }
}


/** \brief The process detected that its child died.
 *
 * The watchdog starts a child to run watchdog plugins to check various
 * things on each server (i.e. whether a process is running, etc.)
 *
 * This callback is run whenever the SIGCHLD is received. The function
 * waits on the child to remove the zombie and then it resets the
 * child process object.
 */
void watchdog_server::process_sigchld()
{
    // block until child is done
    //
    // XXX should we have a way to break the wait after a "long"
    //     while in the event the child locks up?
    int status(0);
    pid_t const the_pid(waitpid(f_processes->get_child_pid(), &status, 0));

    if( the_pid == -1 )
    {
        // the waitpid() should never fail... we just generate a log and
        // go on
        //
        int const e(errno);
        SNAP_LOG_ERROR("waitpid() returned an error (")(strerror(e))(").");
    }
    else
    {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        if(WIFEXITED(status))
        {
            int const exit_code(WEXITSTATUS(status));

            if( exit_code == 0 )
            {
                // when this happens there is not really anything to tell about
                SNAP_LOG_DEBUG("\"snapwatchdog\" statistics plugins terminated normally.");
            }
            else
            {
                SNAP_LOG_INFO("\"snapwatchdog\" statistics plugins terminated normally, but with exit code ")(exit_code);
            }
        }
        else if(WIFSIGNALED(status))
        {
            int const signal_code(WTERMSIG(status));
            bool const has_code_dump(!!WCOREDUMP(status));

            SNAP_LOG_ERROR("\"snapwatchdog\" statistics plugins terminated because of OS signal \"")
                          (strsignal(signal_code))
                          ("\" (")
                          (signal_code)
                          (")")
                          (has_code_dump ? " and a core dump was generated" : "")
                          (".");
        }
        else
        {
            // I do not think we can reach here...
            //
            SNAP_LOG_ERROR("\"snapwatchdog\" statistics plugins terminated abnormally in an unknown way.");
        }
#pragma GCC diagnostic pop

    }

    // one with that child
    //
    f_processes.reset();

    if(f_stopping)
    {
        g_communicator->remove_connection(g_sigchld_connection);
    }
}


/** \brief Initialize the Cassandra connection.
 *
 * This function initializes the Cassandra connection and creates
 * the watchdog "serverstats" table.
 */
void watchdog_server::check_cassandra()
{
    snap_cassandra cassandra( f_parameters );
    cassandra.connect();
    cassandra.init_context();

    QtCassandra::QCassandraContext::pointer_t context( cassandra.get_snap_context() );
    if( !context )
    {
        SNAP_LOG_FATAL("snap_websites context does not exist! Exiting.");
        exit(1);
    }

    // this is sucky, the host/port info should not be taken that way!
    // also we should allow servers without access to cassandra...
    //
    f_snapdbproxy_addr = cassandra.get_snapdbproxy_addr();
    f_snapdbproxy_port = cassandra.get_snapdbproxy_port();

    // create possibly missing tables
    //
    create_table(context, get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_SERVERSTATS),  "Statistics of all our servers.");

    // make sure it is synchronized
    //
    // TODO: fix that by using snap_cassandra all along (in the snapserver
    //       too actually...)
    //
    create_table(context, get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_SERVERSTATS),  "Statistics of all our servers.");
}


/** \brief Initialize the watchdog server parameters.
 *
 * This function gets the parameters from the watchdog configuration file
 * and convert them for use by the watchdog_server implementation.
 *
 * If a parameter is not valid, the function calls exit(1) so the server
 * does not do anything.
 */
void watchdog_server::init_parameters()
{
    // Time Frequency (how often we gather the stats)
    {
        QString const statistics_frequency(get_parameter(get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_STATISTICS_FREQUENCY)));
        bool ok(false);
        f_statistics_frequency = static_cast<int64_t>(statistics_frequency.toLongLong(&ok));
        if(!ok)
        {
            SNAP_LOG_FATAL("watchdog_server::init_parameters(): statistic frequency \"")(statistics_frequency)("\" is not a valid number.");
            exit(1);
        }
        if(f_statistics_frequency < 0)
        {
            SNAP_LOG_FATAL("watchdog_server::init_parameters(): statistic frequency (")(statistics_frequency)(") cannot be a negative number.");
            exit(1);
        }
        if(f_statistics_frequency < 60)
        {
            // minimum is 1 minute
            f_statistics_frequency = 60;
        }
        f_statistics_frequency *= 1000000; // the value in microseconds
    }

    // Time Period (how many stats we keep in the db)
    {
        QString const statistics_period(get_parameter(get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_STATISTICS_PERIOD)));
        bool ok(false);
        f_statistics_period = static_cast<int64_t>(statistics_period.toLongLong(&ok));
        if(!ok)
        {
            SNAP_LOG_FATAL("watchdog_server::init_parameters(): statistic period \"")(statistics_period)("\" is not a valid number.");
            exit(1);
        }
        if(f_statistics_period < 0)
        {
            SNAP_LOG_FATAL("watchdog_server::init_parameters(): statistic period (")(statistics_period)(") cannot be a negative number.");
            exit(1);
        }
        if(f_statistics_period < 3600)
        {
            // minimum is 1 hour
            f_statistics_period = 3600;
        }
        // round up to the hour, but keep it in seconds
        f_statistics_period = (f_statistics_period + 3599) / 3600 * 3600;
    }

    // Time To Live (TTL, used to make sure we do not over crowd the database)
    {
        QString const statistics_ttl(get_parameter(get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_STATISTICS_TTL)));
        bool ok(false);
        f_statistics_ttl = static_cast<int64_t>(statistics_ttl.toLongLong(&ok));
        if(!ok)
        {
            SNAP_LOG_FATAL("watchdog_server::init_parameters(): statistic ttl \"")(statistics_ttl)("\" is not a valid number.");
            exit(1);
        }
        if(f_statistics_ttl < 0)
        {
            SNAP_LOG_FATAL("watchdog_server::init_parameters(): statistic ttl (")(statistics_ttl)(") cannot be a negative number.");
            exit(1);
        }
        if(f_statistics_ttl < 3600)
        {
            // minimum is 1 hour
            f_statistics_ttl = 3600;
        }
    }
}


/** \brief Process a message received from the watchdog server.
 *
 * The process for the watchdog server handles events incoming from
 * Snap Communicator using this function.
 *
 * \param[in] message  The message we just received.
 */
void watchdog_server::process_message(snap::snap_communicator_message const & message)
{
    SNAP_LOG_TRACE("received message [")(message.to_message())("]");

    QString const command(message.get_command());

// ******************* TCP and UDP messages

    // someone sent "snapwatchdog/STOP" to snapcommunicator
    //
    if(command == "STOP"
    || command == "QUITTING")
    {
        SNAP_LOG_INFO("Stopping watchdog server.");

        f_stopping = true;
        g_messager->mark_done();

        // someone asking us to stop snap_init; this means we want to stop
        // all the services that snap_init started; if we have a
        // snapcommunicator, then we use that to send the STOP signal to
        // all services at once
        //
        snap_communicator_message unregister;
        unregister.set_command("UNREGISTER");
        unregister.add_parameter("service", "snapwatchdog");
        g_messager->send_message(unregister);

        g_communicator->remove_connection(g_tick_timer);
        if(!f_processes)
        {
            g_communicator->remove_connection(g_sigchld_connection);
        }

        return;
    }

// ******************* TCP only messages

    if(command == "READY")
    {
        // TBD: should we wait on this signal before we start the g_tick_timer?
        //      since we do not need the snap communicator, probably not useful
        //
        return;
    }

    if(command == "LOG")
    {
        SNAP_LOG_INFO("Logging reconfiguration.");
        logging::reconfigure();
        return;
    }

    // all have to implement the HELP command
    //
    if(command == "HELP")
    {
        snap::snap_communicator_message reply;
        reply.set_command("COMMANDS");

        // list of commands understood by snapinit
        //
        reply.add_parameter("list", "HELP,LOG,QUITTING,READY,STOP,UNKNOWN");

        g_messager->send_message(reply);
        return;
    }

    if(command == "UNKNOWN")
    {
        SNAP_LOG_ERROR("we sent unknown command \"")(message.get_parameter("command"))("\" and probably did not get the expected result.");
        return;
    }

    // unknown command is reported and process goes on
    //
    SNAP_LOG_ERROR("unsupported command \"")(command)("\" was received on the TCP connection.");
    {
        snap::snap_communicator_message reply;
        reply.set_command("UNKNOWN");
        reply.add_parameter("command", command);
        g_messager->send_message(reply);
    }
}






/** \brief Initialize the watchdog child.
 *
 * This function saves the server pointer so it can be accessed
 * as we do in plugins with the f_snap pointer.
 *
 * \param[in] s  The server watchdog.
 */
watchdog_child::watchdog_child(server_pointer_t s)
    : snap_child(s)
{
}


/** \brief Clean up the watchdog child.
 *
 * Make sure the child is cleaned.
 */
watchdog_child::~watchdog_child()
{
}


/** \brief Run watchdog plugins.
 *
 * This function runs all the watchdog plugins and saves the results
 * in a file and the database.
 *
 * If no plugins are are defined, the result will be empty.
 *
 * The function makes use of a try/catch block to avoid ending the
 * process if an error occurs. However, problems should get fixed
 * or you will certainly not get the results you are looking for.
 */
void watchdog_child::run_watchdog_plugins()
{
    // create a child process so the data between sites does not get
    // shared (also the Cassandra data would remain in memory increasing
    // the foot print each time we run a new website,) but the worst
    // are the plugins; we can request a plugin to be unloaded but
    // frankly the system is not very well written to handle that case.
    f_child_pid = fork_child();
    if(f_child_pid != 0)
    {
        int const e(errno);

        // parent process
        if(f_child_pid == -1)
        {
            // we do not try again, we just abandon the whole process
            //
            SNAP_LOG_ERROR("watchdog_server::run_watchdog_process() could not create child process, fork() failed with errno: ")(e)(" -- ")(strerror(e))(".");
        }
        return;
    }

    // we are the child, run the watchdog_process() signal
    try
    {
        f_ready = false;

        // on fork() we lose the configuration so we have to reload it
        logging::reconfigure();

        init_start_date();

        connect_cassandra();

        auto server(std::dynamic_pointer_cast<watchdog_server>(f_server.lock()));
        if(!server)
        {
            throw snap_child_exception_no_server("watchdog_child::run_watchdog_plugins(): The p_server weak pointer could not be locked");
        }

        // initialize the plugins
        init_plugins(false);

        f_ready = true;

        // create the watchdog document
        QDomDocument doc("watchdog");

        // run each plugin watchdog function
        server->process_watch(doc);
        QString result(doc.toString());
        if(result.isEmpty())
        {
            static bool err_once(true);
            if(err_once)
            {
                err_once = false;
                SNAP_LOG_ERROR("watchdog_child::run_watchdog_plugins() generated a completely empty result. This can happen if you do not define any watchdog plugins.");
            }
        }
        else
        {
            int64_t const start_date(get_start_date());
            // round to the minute first, then apply period
            int64_t const date((start_date / (1000000LL * 60LL) * 60LL) % server->get_statistics_period());

            // add the date in ns to this result
            QDomElement watchdog_tag(snap_dom::create_element(doc, "watchdog"));
            watchdog_tag.setAttribute("date", static_cast<qlonglong>(start_date));
            result = doc.toString(-1);

            // save the result in a file first
            QString data_path(server->get_parameter(watchdog::get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_DATA_PATH)));
            data_path += QString("/%1.xml").arg(date);
            {
                std::ofstream out;
                out.open(data_path.toUtf8().data(), std::ios_base::binary);
                if(out.is_open())
                {
                    // result already ends with a "\n"
                    out << result;
                }
            }

            // then try to save it in the Cassandra database
            // (if the cluster is not available, we still have the files!)
            //
            // retrieve server statistics table
            QString const table_name(get_name(watchdog::name_t::SNAP_NAME_WATCHDOG_SERVERSTATS));
            QtCassandra::QCassandraTable::pointer_t table(f_context->table(table_name));

            QtCassandra::QCassandraValue value;
            value.setStringValue(doc.toString(-1));
            value.setTtl(server->get_statistics_ttl());
            QByteArray cell_key;
            QtCassandra::setInt64Value(cell_key, date);
            table->row(server->get_server_name())->cell(cell_key)->setValue(result);
        }

        // the child has to exit()
        exit(0);
        NOTREACHED();
    }
    catch(snap_exception const & e)
    {
        SNAP_LOG_FATAL("watchdog_server::run_watchdog_plugins(): exception caught ")(e.what());
    }
    catch(std::exception const & e)
    {
        SNAP_LOG_FATAL("watchdog_server::run_watchdog_plugins(): exception caught ")(e.what())(" (there are mainly two kinds of exceptions happening here: Snap logic errors and Cassandra exceptions that are thrown by thrift)");
    }
    catch(...)
    {
        SNAP_LOG_FATAL("watchdog_server::run_watchdog_plugins(): unknown exception caught!");
    }
    exit(1);
    NOTREACHED();
}


/** \brief Return the child pid.
 *
 * This function can be used to retrieve the PID of the child process.
 * The child is created after when the run_watchdog_plugins() is called.
 * Until then it is -1. Note that in the child process, this function
 * will return 0.
 *
 * \return The child process identifier.
 */
pid_t watchdog_child::get_child_pid() const
{
    return f_child_pid;
}


} // namespace snap
// vim: ts=4 sw=4 et