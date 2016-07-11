/*
 * Text:
 *      manager_status.cpp
 *
 * Description:
 *      The implementation of the status gathering thread.
 *
 * License:
 *      Copyright (c) 2016 Made to Order Software Corp.
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


// ourselves
//
#include "snapmanagerdaemon.h"

// our lib
//
#include "log.h"
#include "process.h"

// C++
//
#include <algorithm>


namespace snap_manager
{





/** \brief Initialize the manager status.
 *
 * This constructor names the runner object "manager_status". It also
 * saves a reference to the status connection object which is used
 * to (1) send new MANAGERSTATUS and (2) receive STOP when we are done
 * and the thread needs to quit.
 *
 * \warning
 * Remember that the status_connection only sends messages to the
 * manager_daemon, although the manager_daemon will detect if the
 * name of the service is specified and in that case it will
 * forward messages to snapcommunicator.
 *
 * \param[in] md  The manager daemon object.
 * \param[in] sc  The status connection object.
 */
manager_status::manager_status(manager_daemon * md, status_connection::pointer_t sc)
    : snap_runner("manager_status")
    , f_manager_daemon(md)
    , f_status_connection(sc)
    //, f_running(true)
{
    f_status_connection->set_thread_b(this);
}


/** \brief Save the list of front end snapmanager.cgi computers.
 *
 * We really only need to forward the current status of the
 * cluster computer to a few front end computers accepting
 * requests from snapmanager.cgi (these should be 100% private
 * computers if you have an in house stack of computers.)
 *
 * The list includes hosts name. The same name you define in
 * the snapinit.conf file. If undefined there, then that name
 * would be your hostname.
 *
 * If the list is undefined (remains empty) then the messages
 * are broadcast to all computers.
 *
 * \param[in] snapmanager_frontend  The comma separated list of host names.
 */
void manager_status::set_snapmanager_frontend(QString const & snapmanager_frontend)
{
    // guard is not needed because the frontend IPs are set once
    // before the thread starts
    //snap::snap_thread::snap_lock guard(f_mutex);

    f_snapmanager_frontend = snapmanager_frontend.split(",", QString::SkipEmptyParts);

    std::for_each(f_snapmanager_frontend.begin(),
                  f_snapmanager_frontend.end(),
                  [](auto & f) { f = f.trimmed(); });
}


/** \brief Check whether the specified server is a front end computer.
 *
 * This function is used to check whether the specified \p server_name
 * represents a front end computer, as far as snapmanager.cgi is
 * concerned, and if so, it returns true.
 *
 * \note
 * At some point, snapmanagerdaemons will verify that all the servers
 * have the same snapmanager_frontend parameter.
 *
 * \param[in] server_name  The name of the server being checked.
 *
 * \return true if the server is considered a snapmanager.cgi front end.
 */
bool manager_status::is_snapmanager_frontend(QString const & server_name) const
{
    // guard is not needed because the frontend names are set once
    // before the thread starts
    return f_snapmanager_frontend.empty()
        || f_snapmanager_frontend.contains(server_name);
}


/** \brief Return the list of frontend server names.
 *
 * This function returns the list of frontend server names as defined
 * in the configuration file. These are the names of computers running
 * snapmanagerdaemon that get contacted whenever a new MANAGERSTATUS
 * message is to be sent.
 *
 * If the list is empty, then snapmanagerdaemon broadcasts the message
 * to all that are running in the cluster.
 *
 * \return The list of frontend server names.
 */
snap::snap_string_list const & manager_status::get_snapmanager_frontend() const
{
    // guard is not needed because the frontend names are set once
    // before the thread starts
    //snap::snap_thread::snap_lock guard(f_mutex);
    return f_snapmanager_frontend;
}


/** \brief Check whether a plugin should skip doing any work in a status call.
 *
 * Whenever we generate the status of a server, we emit the retrieve_status()
 * signal. This one runs against all the plugins and stops only once all the
 * plugins are done. Unfortunately, this can be a very long time since some
 * plugins retrieve statuses that take time to gather.
 *
 * In order to allow for a fast STOP command, the plugins are expected to
 * check whether they are required to stop as soon as possible. This should
 * be checked on entry of the retrieve_status() implementation and if the
 * plugin has several parts or a loop, check between parts and on each
 * inner loop iterations (unless the inner loop is really fast, do it in
 * the one prior.)
 *
 * \return true if the thread has been asked to stop as soon as possible.
 */
bool manager_status::stop_now_prima() const
{
    return !continue_running() || !f_running;
}


/** \brief Thread used to permanently gather this server status.
 *
 * Each computer in the Snap! cluster should be running an instance
 * of the snapmanagerdaemon system. This will gather basic information
 * about the state of the each system and send the information to
 * all the computers who have snapmanager.cgi active.
 *
 * \sa set_snapmanager_frontend()
 */
void manager_status::run()
{
    // run as long as the parent thread did not ask us to quit
    //
    QString status;

    for(;;)
    {
        // first gather a new set of statuses
        //
        server_status plugin_status("");

        f_manager_daemon->retrieve_status(plugin_status);
        if(!continue_running() || !f_running)
        {
            return;
        }

        // now convert the resulting server_status to a string,
        // make sure to place the "status" first since we load just
        // that when we show the entire cluster information
        //
        QString const previous_status(status);
        status.clear();
        QString status_string;
        snap_manager::status_t::map_t const & statuses(plugin_status.get_statuses());
        for(auto const & ss : statuses)
        {
            if(ss.second.get_plugin_name() == "self"
            && ss.second.get_field_name() == "status")
            {
                status_string = ss.second.to_string();
            }
            else
            {
                status += ss.second.to_string() + "\n";
            }
        }

        // always prepend the status variable
        status = QString("%1\n%2").arg(status_string).arg(status);

        // generate a message to send the snapmanagerdaemon
        // (but only if the status changed, otherwise it would be a waste)
        //
        bool resend(false);
        {
            snap::snap_thread::snap_lock guard(f_mutex);
            resend = f_resend_status;
            f_resend_status = false;
        }
        // XXX: see whether it would be useful to also save the last time
        //      we send the MANAGERSTATUS message and if it was more than
        //      X hours or days, resend it to make sure "all" as of now
        //      have a copy as expected
        //
        //      i.e. the MANAGERRESEND message may fail because we may
        //      not yet be inter-computer connected when that event gets
        //      sent (i.e. a remote computer may wait 15 minutes before
        //      connecting back to us...)
        //
        if(status != previous_status
        || resend)
        {
            // TODO: designate a few computers that are to be used as
            //       front ends with snapmanager.cgi and only send the
            //       status information to those computers
            //
            if(f_snapmanager_frontend.isEmpty())
            {
                // user did not specify a list of front end hosts for
                // snapmanager.cgi so we instead broadcast the message
                // to all computers in the cluster (with a large cluster
                // this is not a good idea...)
                //
                snap::snap_communicator_message status_message;
                status_message.set_command("MANAGERSTATUS");
                status_message.set_service("*");
                status_message.add_parameter("status", status);
                f_status_connection->send_message(status_message);
            }
            else
            {
                // send the message only to the few specified frontends
                // so that way we can be sure to avoid send a huge pile
                // of messages throughout the entire cluster
                //
                for(auto const & f : f_snapmanager_frontend)
                {
                    snap::snap_communicator_message status_message;
                    status_message.set_command("MANAGERSTATUS");
                    status_message.set_server(f);
                    status_message.set_service("snapmanagerdaemon");
                    status_message.add_parameter("status", status);
                    f_status_connection->send_message(status_message);
                }
            }
        }

        // wait for messages or 1 minute
        //
        f_status_connection->poll(60 * 1000000LL);
    }
}


/** \brief Process a message sent to us by our "parent".
 *
 * This function gets called whenever the manager_daemon object
 * sends us a message.
 *
 * \param[in] message  The message to process.
 */
void manager_status::process_message(snap::snap_communicator_message const & message)
{
    SNAP_LOG_TRACE("manager-status thread received messenger message [")(message.to_message())("]");

    QString const & command(message.get_command());

    switch(command[0].unicode())
    {
    case 'S':
        if(command == "STOP")
        {
            // this will stop the manager_status thread as soon as possible
            //
            f_running = false;
        }
        break;

    }
}


/** \brief Request for the status to be resent.
 *
 * This function clears the last status information so that way we can
 * make sure it gets resent to all the other snapmanagerdaemons running.
 */
void manager_status::resend_status()
{
    snap::snap_thread::snap_lock guard(f_mutex);
    f_resend_status = true;
}


/** \brief Installs one Debian package.
 *
 * This function installs ONE package as specified by \p package_name.
 *
 * \param[in] package_name  The name of the package to install.
 * \param[in] add_info_only_if_present  Whether to add information about
 *            non installed packages in the output.
 *
 * \return The exit code of the apt-get install command.
 */
int manager_status::package_status(QString const & package_name, bool add_info_only_if_present)
{
    snap::process p("check status");
    p.set_mode(snap::process::mode_t::PROCESS_MODE_OUTPUT);
    p.set_command("dpkg-query");
    p.add_argument("-W");
    p.add_argument(package_name);
    int const r(p.run());

    // the output is saved so we can send it to the user and log it...
    if(r == 0)
    {
        QString const output(p.get_output(true));
        //f_output += output;
        SNAP_LOG_TRACE("package status:\n")(output);
    }
    else if(!add_info_only_if_present)
    {
        // in this case the output is likely empty (i.e. we do not read
        // stderr...), so we ignore it
        //
        //f_output += package_name + " is not installed";
        SNAP_LOG_TRACE("package named \"")(package_name)("\" is not installed.");
    }

    return r;
}







/*
 * This needs to be 100% dynamic
 *
# server_types=<list of names>
#
# List what functionality this server offers as a list of names as defined
# below.
#
# The "Comp." column (Computer) defines where such and such service should
# be running. Each set of services can run on the same computer (often does
# while doing development.) However, running everything on a single computer
# is not how Snap! was designed. The idea is to be able to run the websites
# on any number of computers in a cluster. This is why we have Cassandra,
# but really all parts can be running on any number of computers.
#
# +===============+===========================================+=======+
# |    Name       |              Description                  | Comp. |
# +===============+===========================================+=======+
# | anti-virus    | clamav along snapbackend                  | Back  |
# | application   | snapserver and clamav                     | Back  |
# | backend       | one or more snapbackend                   | Back  |
# | base          | snamanager.cgi and dependencies           | All   |
# | cassandra     | cassandra database                        | Back  |
# | firewall      | snapfirewall + iplock (need overhaul)     | All   |
# | frontend      | apache with snap.cgi                      | Front |
# | mailserver    | postfix with snapbounce                   | Front |
# | ntp           | time server                               | All   |
# | vpn           | tinc / openvpn                            | All   |
# | logserver     | loggingserver from log4cplus              | Back  |
# | rkhunter      | check that no rootkit gets installed      | All   |
# | tripwire      | check that no files gets changed          | All   |
# +===============+===========================================+=======+
#
# Note that the snapserver plugins come with clamav.
#
# Default: base
server_types=base
*/




} // namespace snap_manager
// vim: ts=4 sw=4 et
