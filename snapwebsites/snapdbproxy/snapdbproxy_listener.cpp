/*
 * Text:
 *      snapdbproxy_listener.cpp
 *
 * Description:
 *      Listen for connections on localhost.
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
#include "snapdbproxy.h"

// our lib
//
#include "log.h"




/** \class snapdbproxy_listener
 * \brief Handle new connections from clients.
 *
 * This function is an implementation of the snap server so we can
 * handle new connections from various clients.
 */



/** \brief The listener initialization.
 *
 * The listener receives a pointer back to the snap::server object and
 * information on how to generate the new network connection to listen
 * on incoming connections from clients.
 *
 * The server listens to two types of messages:
 *
 * \li accept() -- a new connection is accepted from a client
 * \li recv() -- a UDP message was received
 *
 * \param[in] s  The server we are listening for.
 * \param[in] addr  The address to listen on. Most often it is 0.0.0.0.
 * \param[in] port  The port to listen on.
 * \param[in] max_connections  The maximum number of connections to keep
 *            waiting; if more arrive, refuse them until we are done with
 *            some existing connections.
 * \param[in] reuse_addr  Whether to let the OS reuse that socket immediately.
 * \param[in] auto_close  Whether to automatically close the socket once more
 *            needed anymore.
 */
snapdbproxy_listener::snapdbproxy_listener(snapdbproxy * proxy, std::string const & addr, int port, int max_connections, bool reuse_addr, bool auto_close)
    : snap_tcp_server_connection(addr, port, max_connections, reuse_addr, auto_close)
    , f_snapdbproxy(proxy)
{
    set_name("snapdb proxy listener");
    non_blocking();
    set_priority(30);
}


/** \brief This callback is called whenever a client tries to connect.
 *
 * This callback function is called whenever a new client tries to connect
 * to the server.
 *
 * The function retrieves the new connection socket, makes the socket
 * "keep alive" and then calls the process_connection() function of
 * the server.
 */
void snapdbproxy_listener::process_accept()
{
    // a new client just connected
    //
    int const new_socket(accept());
    if(new_socket < 0)
    {
        // TBD: should we call process_error() instead? problem is this
        //      listener would be removed from the list of connections...
        //
        int const e(errno);
        SNAP_LOG_ERROR("accept() returned an error. (errno: ")(e)(" -- ")(strerror(e))("). No new connection will be created.");
        return;
    }

    // we just have a socket and the keepalive() function in the
    // snap_connection requires... a snap_connection object.
    //
    int optval(1);
    socklen_t const optlen(sizeof(optval));
    if(setsockopt(new_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) != 0)
    {
        SNAP_LOG_WARNING("snapdbproxy_listener::process_accept(): an error occurred trying to mark socket with SO_KEEPALIVE.");
    }

    // process the new connection, which means create a child process
    // and run the necessary code to return an HTML page, a document,
    // robots.txt, etc.
    //
    f_snapdbproxy->process_connection(new_socket);
}


// vim: ts=4 sw=4 et