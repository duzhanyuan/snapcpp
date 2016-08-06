// Snap Communicator -- classes to ease handling communication between processes
// Copyright (C) 2012-2016  Made to Order Software Corp.
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

// to get the POLLRDHUP definition
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "snap_communicator.h"

#include "log.h"
#include "not_reached.h"
#include "not_used.h"
#include "qstring_stream.h"

#include <sstream>
#include <limits>
#include <atomic>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include "poison.h"

/** \file
 * \brief Implementation of the Snap Communicator class.
 *
 * This class wraps the C poll() interface in a C++ object with many types
 * of objects:
 *
 * \li Server Connections; for software that want to offer a port to
 *     which clients can connect to; the server will call accept()
 *     once a new client connection is ready; this results in a
 *     Server/Client connection object
 * \li Client Connections; for software that want to connect to
 *     a server; these expect the IP address and port to connect to
 * \li Server/Client Connections; for the server when it accepts a new
 *     connection; in this case the server gets a socket from accept()
 *     and creates one of these objects to handle the connection
 *
 * Using the poll() function is the easiest and allows us to listen
 * on pretty much any number of sockets (on my server it is limited
 * at 16,768 and frankly over 1,000 we probably will start to have
 * real slowness issues on small VPN servers.)
 */

namespace snap
{
namespace
{


/** \brief The instance of the snap_communicator singleton.
 *
 * This pointer is the one instance of the snap_communicator
 * we create to run an event loop.
 */
snap_communicator::pointer_t        g_instance;


/** \brief The array of signals handled by snap_signal objects.
 *
 * This map holds a list of signal handlers. You cannot register
 * the same signal more than once so this map is used to make
 * sure that each signal is unique.
 *
 * \todo
 * We may actually want to use a sigset_t object and just set
 * bits and remove 
 *
 * \note
 * The pointer to the snap_signal object is a bare pointer
 * for in part because we cannot use a smart pointer in
 * a constructor where we add the signal to this map. Also
 * at this time that pointer does not get used so it could
 * as well have been a boolean.
 */
sigset_t                            g_signal_handlers = sigset_t();


/** \brief Retrieve the identifier of the current thread.
 *
 * This function returns the pid_t of the current thread.
 *
 * \return The current thread identifier.
 */
pid_t gettid()
{
    return syscall(SYS_gettid);
}


/** \brief Close a file descriptor object.
 *
 * This deleter is used to make sure that any file descriptor gets
 * closed as expected even if an exception occurs.
 *
 * \param[in] fd  The file descriptor gets closed.
 */
void fd_deleter(int * fd)
{
    close(*fd);
}


} // no name namespace




///////////////////////////////
// Snap Communicator Message //
///////////////////////////////


/** \brief Parse a message from the specified paremeter.
 *
 * This function transformed the input string in a set of message
 * fields.
 *
 * The message format supported is:
 *
 * \code
 *      ( '<' sent-from-server ':' sent-from-service ' ')? ( ( server ':' )? service '/' )? command ' ' ( parameter_name '=' value ';' )*
 * \endcode
 *
 * The sender "<sent-from-server:sent-from-service" names are added by
 * snapcommunicator when it receives a message which is destined for
 * another service (i.e. not itself). This can be used by the receiver
 * to reply back to the exact same process if it is a requirement for that
 * message (i.e. a process that sends a LOCK message, for example,
 * expects to receive the LOCKED message back as an answer.) Note that
 * it is assumed that there cannot be more than one service named
 * 'service' per server. This is enforced by the snapcommunicator
 * REGISTER function.
 *
 * \code
 *      // why replying to the exact message sender, one can use the
 *      // following two lines of code:
 *      //
 *      reply.set_server(message.get_sent_from_server());
 *      reply.set_service(message.get_sent_from_service());
 *
 *      // or use the reply_to() helper function
 *      //
 *      reply.reply_to(message);
 * \endcode
 *
 * The space after the command cannot be there unless parameters follow.
 * Parameters must be separated by semi-colons.
 *
 * The value of a parameter gets quoted when it includes a ';'. Within
 * the quotes, a Double Quote can be escaped inside by adding a backslash
 * in front of it (\"). Newline characters (as well as return carriage)
 * are also escaped using \n and \r respectively. Finally, we have to
 * escape backslashes themselves by doubling them, so \ becomes \\.
 *
 * Note that only parameter values support absolutely any character.
 * All the other parameters are limited to the latin alphabet, digits,
 * and underscores ([A-Za-z0-9_]+). Also all commands are limited
 * to uppercase letters only.
 *
 * \note
 * The input message is not saved as a cached version of the message
 * because we assume it may not be 100% optimized (canonicalized.)
 *
 * \param[in] message  The string to convert into fields in this
 *                     message object.
 *
 * \return true if the message was succesfully parsed; false when an
 *         error occurs and in that case no fields get modified.
 *
 * \sa to_message()
 * \sa get_sent_from_server()
 * \sa get_sent_from_service()
 * \sa reply_to()
 */
bool snap_communicator_message::from_message(QString const & message)
{
    QString sent_from_server;
    QString sent_from_service;
    QString server;
    QString service;
    QString command;
    parameters_t parameters;

    QChar const * m(message.constData());

    // sent-from indicated?
    if(!m->isNull() && m->unicode() == '<')
    {
        // the name of the server and server sending this message
        //
        // First ++m to skip the '<'
        for(++m; !m->isNull() && m->unicode() != ':'; ++m)
        {
            if(m->unicode() == ' ')
            {
                // invalid syntax from input message
                //
                SNAP_LOG_ERROR("a message with sent_from_server must not include a space in the server name (")(message)(").");
                return false;
            }

            sent_from_server += m->unicode();
        }
        if(!m->isNull())
        {
            // First ++m to skip the ':'
            for(++m; !m->isNull() && m->unicode() != ' '; ++m)
            {
                sent_from_service += m->unicode();
            }
        }
        if(m->isNull())
        {
            // invalid syntax from input message
            //
            SNAP_LOG_ERROR("a message cannot only include a 'sent from service' definition.");
            return false;
        }
        // Skip the ' '
        ++m;
    }

    bool has_server(false);
    bool has_service(false);
    for(; !m->isNull() && m->unicode() != ' '; ++m)
    {
        if(m->unicode() == ':')
        {
            if(has_server
            || has_service
            || command.isEmpty())
            {
                // we cannot have more than one ':'
                // and the name cannot be empty if ':' is used
                // we also cannot have a ':' after the '/'
                SNAP_LOG_ERROR("a server name cannot be empty when specified, also it cannot include two server names and a server name after a service name was specified.");
                return false;
            }
            has_server = true;
            server = command;
            command.clear();
        }
        else if(m->unicode() == '/')
        {
            if(has_service
            || command.isEmpty())
            {
                // we cannot have more than one '/'
                // and the name cannot be empty if '/' is used
                SNAP_LOG_ERROR("a service name is mandatory when the message includes a slash (/), also it cannot include two service names.");
                return false;
            }
            has_service = true;
            service = command;
            command.clear();
        }
        else
        {
            command += *m;
        }
    }

    if(command.isEmpty())
    {
        // command is mandatory
        SNAP_LOG_ERROR("a command is mandatory in in a message.");
        return false;
    }

    // if we have a space, we expect one or more parameters
    if(m->unicode() == ' ')
    {
        for(++m; !m->isNull();)
        {
            // first we have to read the parameter name (up to the '=')
            QString param_name;
            for(; !m->isNull() && m->unicode() != '='; ++m)
            {
                param_name += *m;
            }
            if(param_name.isEmpty())
            {
                // parameters must have a name
                SNAP_LOG_ERROR("could not accept message because an empty parameter name is not valid.");
                return false;
            }
            try
            {
                verify_name(param_name);
            }
            catch(snap_communicator_invalid_message const & e)
            {
                // name is not empty, but it has invalid characters in it
                SNAP_LOG_ERROR("could not accept message because parameter name \"")(param_name)("\" is not considered valid: ")(e.what());
                return false;
            }

            if(m->isNull()
            || m->unicode() != '=')
            {
                // ?!?
                SNAP_LOG_ERROR("message parameters must be followed by an equal (=) character.");
                return false;
            }
            ++m;

            // retrieve the parameter name at first
            QString param_value;
            if(!m->isNull() && m->unicode() == '"')
            {
                // quoted parameter
                for(++m; !m->isNull() && m->unicode() != '"'; ++m)
                {
                    // restored escaped double quotes
                    // (note that we do not yet restore other backslashed
                    // characters, that's done below)
                    if(m->unicode() == '\\' && !m[1].isNull() && m[1].unicode() == '"')
                    {
                        ++m;
                        param_value += *m;
                    }
                    else
                    {
                        // here the character may be ';'
                        param_value += *m;
                    }
                }
                if(m->isNull()
                || m->unicode() != '"')
                {
                    // closing quote (") is missing
                    SNAP_LOG_ERROR("a quoted message parameter must end with a quote (\").");
                    return false;
                }
                // skip the quote
                ++m;
            }
            else
            {
                // parameter value is found as is
                for(; !m->isNull() && m->unicode() != ';'; ++m)
                {
                    param_value += *m;
                }
            }

            if(!m->isNull())
            {
                if(m->unicode() != ';')
                {
                    // this should never happend
                    SNAP_LOG_ERROR("two parameters must be separated by a semicolon (;).");
                    return false;
                }
                // skip the ';'
                ++m;
            }

            // also restore new lines and blackslashes if any
            param_value.replace("\\\\", "\\")
                       .replace("\\n", "\n")
                       .replace("\\r", "\r");

            // we got a valid parameter, add it
            parameters[param_name] = param_value;
        }
    }

    f_sent_from_server = sent_from_server;
    f_sent_from_service = sent_from_service;
    f_server = server;
    f_service = service;
    f_command = command;
    f_parameters.swap(parameters);
    f_cached_message.clear();

    return true;
}


/** \brief Transform all the message parameters in a string.
 *
 * This function transforms all the message parameters in a string
 * and returns the result. The string is a message we can send over
 * TCP/IP (if you make sure to add a "\n", note that the
 * send_message() does that automatically) or over UDP/IP.
 *
 * \note
 * The function caches the result so calling the function many times
 * will return the same string and thus the function is very fast
 * after the first call (assuming you do not modify the message on
 * each call to to_message().)
 *
 * \note
 * The sent-from information gets saved in the message only if both,
 * the server name and service name it was sent from are defined.
 *
 * \exception snap_communicator_invalid_message
 * This function raises an exception if the message command was not
 * defined since a command is always mandatory.
 *
 * \return The converted message as a string.
 *
 * \sa get_sent_from_server()
 * \sa get_sent_from_service()
 * \sa set_reply_to_server()
 * \sa set_reply_to_service()
 */
QString snap_communicator_message::to_message() const
{
    if(f_cached_message.isEmpty())
    {
        if(f_command.isEmpty())
        {
            throw snap_communicator_invalid_message("snap_communicator_message::to_message(): cannot build a valid message without at least a command.");
        }

        // add info about the sender
        // ['<' <sent-from-server> '/' <sent-from-service> ' ']
        //
        if(!f_sent_from_server.isEmpty()
        || !f_sent_from_service.isEmpty())
        {
            f_cached_message += "<";
            f_cached_message += f_sent_from_server;
            f_cached_message += ":";
            f_cached_message += f_sent_from_service;
            f_cached_message += " ";
        }

        // add server and optionally the destination server name if both are defined
        // ['<' <sent-from-server> '/' <sent-from-service> ' '] [[<server> ':'] <name> '/']
        //
        if(!f_service.isEmpty())
        {
            if(!f_server.isEmpty())
            {
                f_cached_message += f_server;
                f_cached_message += ':';
            }
            f_cached_message += f_service;
            f_cached_message += "/";
        }

        // ['<' <sent-from-server> '/' <sent-from-service> ' '] [[<server> ':'] <name> '/'] <command>
        f_cached_message += f_command;

        // add parameters if any
        // ['<' <sent-from-server> '/' <sent-from-service> ' '] [[<server> ':'] <name> '/'] <command> [' ' <param1> '=' <value1>][';' <param2> '=' <value2>]...
        //
        bool first(true);
        for(auto p(f_parameters.begin());
                 p != f_parameters.end();
                 ++p, first = false)
        {
            f_cached_message += QString("%1%2=").arg(first ? " " : ";").arg(p.key());
            QString param(p.value());
            param.replace("\\", "\\\\")  // existing backslashes need to be escaped
                 .replace("\n", "\\n")   // newline needs to be escaped
                 .replace("\r", "\\r");  // this one is not important, but for completeness
            if(param.indexOf(";") >= 0
            || (!param.isEmpty() && param[0] == '\"'))
            {
                // escape the double quotes
                param.replace("\"", "\\\"");
                // quote the resulting parameter and save in f_cached_message
                f_cached_message += QString("\"%1\"").arg(param);
            }
            else
            {
                // no special handling necessary
                f_cached_message += param;
            }
        }
    }

    return f_cached_message;
}


/** \brief Where this message came from.
 *
 * Some services send a message expecting an answer directly sent back
 * to them. Yet, those services may have multiple instances in your cluster
 * (i.e. snapinit and snapcommunicator run on all computers, snapwatchdog,
 * snapfirewall, snaplock, snapdbproxy are likely to run on most computers,
 * etc.) This parameter defines which computer, specifically, the message
 * came from. Thus, you can use that information to send the message back
 * to that specific computer. The snapcommunicator on that computer will
 * then forward the message to the specified service.
 *
 * If empty (the default,) then the normal snapcommunicator behavior is
 * used (i.e. send to any instance of the service that is available.)
 *
 * \return The address and port of the specific service this message has to
 *         be sent to.
 *
 * \sa set_sent_from_server()
 * \sa set_sent_from_service()
 * \sa get_sent_from_service()
 */
QString const & snap_communicator_message::get_sent_from_server() const
{
    return f_sent_from_server;
}


/** \brief Set the name of the server that sent this message.
 *
 * This function saves the name of the server that was used to
 * generate the message. This can be used later to send a reply
 * to the service that sent this message.
 *
 * The snapcommunicator tool is actually in charge of setting this
 * parameter and you should never have to do so from your tool.
 * The set happens whenever the snapcommunicator receives a
 * message from a client. If you are not using the snapcommunicator
 * then you are welcome to use this function for your own needs.
 *
 * \param[in] sent_from_server  The name of the source server.
 *
 * \sa get_sent_from_server()
 * \sa get_sent_from_service()
 * \sa set_sent_from_service()
 */
void snap_communicator_message::set_sent_from_server(QString const & sent_from_server)
{
    if(f_sent_from_server != sent_from_server)
    {
        // this name can be empty and it supports lowercase
        //
        verify_name(sent_from_server, true);

        f_sent_from_server = sent_from_server;
        f_cached_message.clear();
    }
}


/** \brief Who sent this message.
 *
 * Some services send messages expecting an answer sent right back to
 * them. For example, the snaplock tool sends the message LOCKENTERING
 * and expects the LOCKENTERED as a reply. The reply has to be sent
 * to the exact same instance t hat sent the LOCKENTERING message.
 *
 * In order to do so, the system makes use of the server and service
 * name the data was sent from. Since the name of each service
 * registering with snapcommunicator must be unique, it 100% defines
 * the sender of the that message.
 *
 * If empty (the default,) then the normal snapcommunicator behavior is
 * used (i.e. send to any instance of the service that is available locally,
 * if not available locally, try to send it to another snapcommunicator
 * that knows about it.)
 *
 * \return The address and port of the specific service this message has to
 *         be sent to.
 *
 * \sa get_sent_from_server()
 * \sa set_sent_from_server()
 * \sa set_sent_from_service()
 */
QString const & snap_communicator_message::get_sent_from_service() const
{
    return f_sent_from_service;
}


/** \brief Set the name of the server that sent this message.
 *
 * This function saves the name of the service that sent this message
 * to snapcommuncator. It is set by snapcommunicator whenever it receives
 * a message from a service it manages so you do not have to specify this
 * parameter yourselves.
 *
 * This can be used to provide the name of the service to reply to. This
 * is useful when the receiver does not already know exactly who sends it
 * certain messages.
 *
 * \param[in] sent_from_service  The name of the service that sent this message.
 *
 * \sa get_sent_from_server()
 * \sa set_sent_from_server()
 * \sa get_sent_from_service()
 */
void snap_communicator_message::set_sent_from_service(QString const & sent_from_service)
{
    if(f_sent_from_service != sent_from_service)
    {
        // this name can be empty and it supports lowercase
        //
        verify_name(sent_from_service, true);

        f_sent_from_service = sent_from_service;
        f_cached_message.clear();
    }
}


/** \brief The server where this message has to be delivered.
 *
 * Some services need their messages to be delivered to a service
 * running on a specific computer. This function returns the name
 * of that server.
 *
 * If the function returns an empty string, then snapcommunicator is
 * free to send the message to any server.
 *
 * \return The name of the server to send this message to or an empty string.
 *
 * \sa set_server()
 * \sa get_service()
 * \sa set_service()
 */
QString const & snap_communicator_message::get_server() const
{
    return f_server;
}


/** \brief Set the name of a specific server where to send this message.
 *
 * In some cases you may want to send a message to a service running
 * on a specific server. This function can be used to specify the exact
 * server where the message has to be delivered.
 *
 * This is particularly useful when you need to send a reply to a
 * specific daemon that sent you a message.
 *
 * \param[in] server  The name of the server to send this message to.
 *
 * \sa get_server()
 * \sa get_service()
 * \sa set_service()
 */
void snap_communicator_message::set_server(QString const & server)
{
    if(f_server != server)
    {
        // this name can be empty and it supports lowercase
        //
        verify_name(server, true);

        f_server = server;
        f_cached_message.clear();
    }
}


/** \brief Retrieve the name of the service the message is for.
 *
 * This function returns the name of the service this message is being
 * sent to.
 *
 * \return Destination service.
 *
 * \sa get_server()
 * \sa set_server()
 * \sa set_service()
 */
QString const & snap_communicator_message::get_service() const
{
    return f_service;
}


/** \brief Set the name of the service this message is being sent to.
 *
 * This function specifies the name of the server this message is expected
 * to be sent to.
 *
 * When a service wants to send a message to snapcommunicator, no service
 * name is required.
 *
 * \param[in] service  The name of the destination service.
 *
 * \sa get_server()
 * \sa set_server()
 * \sa get_service()
 */
void snap_communicator_message::set_service(QString const & service)
{
    if(f_service != service)
    {
        // broadcast is a special case that the verify_name() does not
        // support
        //
        if(service != "*"
        && service != "?"
        && service != ".")
        {
            // this name can be empty and it supports lowercase
            //
            verify_name(service, true);
        }

        f_service = service;
        f_cached_message.clear();
    }
}


/** \brief Copy sent information to this message.
 *
 * This function copies the sent information found in message
 * to this message server and service names.
 *
 * This is an equivalent to the following two lines of code:
 *
 * \code
 *      reply.set_server(message.get_sent_from_server());
 *      reply.set_service(message.get_sent_from_service());
 * \endcode
 *
 * \param[in] message  The source message you want to reply to.
 */
void snap_communicator_message::reply_to(snap_communicator_message const & message)
{
    set_server(message.get_sent_from_server());
    set_service(message.get_sent_from_service());
}


/** \brief Get the command being sent.
 *
 * Each message is an equivalent to an RPC command being send between
 * services.
 *
 * The command is a string of text, generally one or more words
 * concatenated (no space allowed) such as STOP and LOCKENTERING.
 *
 * \note
 * The command string may still be empty if it was not yet assigned.
 *
 * \return The command of this message.
 */
QString const & snap_communicator_message::get_command() const
{
    return f_command;
}


/** \brief Set the message command.
 *
 * This function is used to define the RPC-like command of this message.
 *
 * The name of the command gets verified using the verify_name() function.
 * It cannot be empty and all letters have to be uppercase.
 *
 * \param[in] command  The command to send to a connection.
 *
 * \sa verify_name()
 */
void snap_communicator_message::set_command(QString const & command)
{
    // this name cannot be empty and it does not support lowercase
    // characters either
    //
    verify_name(command, false, false);

    if(f_command != command)
    {
        f_command = command;
        f_cached_message.clear();
    }
}


/** \brief Add a string parameter to the message.
 *
 * Messages can include parameters (variables) such as a URI or a word.
 *
 * The value is not limited, although it probably should be limited to
 * standard text as these messages are sent as text.
 *
 * The name is verified by the verify_name() function.
 *
 * \param[in] name  The name of the parameter.
 * \param[in] value  The value of this parameter.
 *
 * \sa verify_name()
 */
void snap_communicator_message::add_parameter(QString const & name, QString const & value)
{
    verify_name(name);

    f_parameters[name] = value;
    f_cached_message.clear();
}


/** \brief Add an integer parameter to the message.
 *
 * Messages can include parameters (variables) such as a URI or a word.
 *
 * The value is not limited, although it probably should be limited to
 * standard text as these messages are sent as text.
 *
 * The name is verified by the verify_name() function.
 *
 * \param[in] name  The name of the parameter.
 * \param[in] value  The value of this parameter.
 *
 * \sa verify_name()
 */
void snap_communicator_message::add_parameter(QString const & name, int64_t value)
{
    verify_name(name);

    f_parameters[name] = QString("%1").arg(value);
    f_cached_message.clear();
}


/** \brief Check whether a parameter is defined in this message.
 *
 * This function checks whether a parameter is defined in a message. If
 * so it returns true. This is important because the get_parameter()
 * functions throw if the parameter is not available (i.e. which is
 * what is used for mandatory parameters.)
 *
 * \param[in] name  The name of the parameter.
 *
 * \return true if that parameter exists.
 *
 * \sa verify_name()
 */
bool snap_communicator_message::has_parameter(QString const & name) const
{
    verify_name(name);

    return f_parameters.contains(name);
}


/** \brief Retrieve a parameter as a string from this message.
 *
 * This function retrieves the named parameter from this message as a string,
 * which is the default.
 *
 * The name must be valid as defined by the verify_name() function.
 *
 * \note
 * This function returns a copy of the parameter so if you later change
 * the value of that parameter, what has been returned does not change
 * under your feet.
 *
 * \exception snap_communicator_invalid_message
 * This exception is raised whenever the parameter is not defined or
 * if the parameter \p name is not considered valid.
 *
 * \param[in] name  The name of the parameter.
 *
 * \return A copy of the parameter value.
 *
 * \sa verify_name()
 */
QString const snap_communicator_message::get_parameter(QString const & name) const
{
    verify_name(name);

    if(f_parameters.contains(name))
    {
        return f_parameters[name];
    }

    throw snap_communicator_invalid_message("snap_communicator_message::get_parameter(): parameter not defined, try has_parameter() before calling a get_parameter() function.");
}


/** \brief Retrieve a parameter as an integer from this message.
 *
 * This function retrieves the named parameter from this message as a string,
 * which is the default.
 *
 * The name must be valid as defined by the verify_name() function.
 *
 * \exception snap_communicator_invalid_message
 * This exception is raised whenever the parameter is not a valid integer,
 * it is not set, or the parameter name is not considered valid.
 *
 * \param[in] name  The name of the parameter.
 *
 * \return The parameter converted to an integer.
 *
 * \sa verify_name()
 */
int64_t snap_communicator_message::get_integer_parameter(QString const & name) const
{
    verify_name(name);

    if(f_parameters.contains(name))
    {
        bool ok;
        int64_t const r(f_parameters[name].toLongLong(&ok, 10));
        if(!ok)
        {
            throw snap_communicator_invalid_message("snap_communicator_message::get_integer_parameter(): message expected integer could not be converted.");
        }
        return r;
    }

    throw snap_communicator_invalid_message("snap_communicator_message::get_integer_parameter(): parameter not defined, try has_parameter() before calling a get_integer_parameter() function.");
}


/** \brief Retrieve the list of parameters from this message.
 *
 * This function returns a constant reference to the list of parameters
 * defined in this message.
 *
 * This can be useful if you allow for variable lists of parameters, but
 * generally the get_parameter() and get_integer_parameter() are prefered.
 *
 * \warning
 * This is a direct reference to the list of parameter. If you call the
 * add_parameter() function, the new parameter will be visible in that
 * new list and an iterator is likely not going to be valid on return
 * from that call.
 *
 * \return A constant reference to the list of message parameters.
 *
 * \sa get_parameter()
 * \sa get_integer_parameter()
 */
snap_communicator_message::parameters_t const & snap_communicator_message::get_all_parameters() const
{
    return f_parameters;
}


/** \brief Verify various names used with messages.
 *
 * The messages use names for:
 *
 * \li commands
 * \li services
 * \li parameters
 *
 * All those names must be valid as per this function. They are checked
 * on read and on write (i.e. add_parameter() and get_paramter() both
 * check the parameter name to make sure you did not mistype it.)
 *
 * A valid name must start with a letter or an underscore (although
 * we suggest you do not start names with underscores; we want to
 * have those reserved for low level system like messages,) and
 * it can only include letters, digits, and underscores.
 *
 * The letters are limited to uppercase for commands. Also certain
 * names may be empty (See concerned functions for details on that one.)
 *
 * \note
 * The allowed letters are 'a' to 'z' and 'A' to 'Z' only. The allowed
 * digits are '0' to '9' only. The underscore is '_' only.
 *
 * A few valid names:
 *
 * \li commands: PING, STOP, LOCK, LOCKED, QUITTING, UNKNOWN, LOCKEXITING
 * \li services: snapinit, snapcommunicator, snapserver, MyOwnService
 * \li parameters: URI, name, IP, TimeOut
 *
 * At this point all our services use lowercase, but this is not enforced.
 * Actually, mixed case or uppercase service names are allowed.
 *
 * \exception snap_communicator_invalid_message
 * This exception is raised if the name includes characters considered
 * invalid.
 *
 * \param[in] name  The name of the parameter.
 * \param[in] can_be_empty  Whether the name can be empty.
 * \param[in] can_be_lowercase  Whether the name can include lowercase letters.
 */
void snap_communicator_message::verify_name(QString const & name, bool can_be_empty, bool can_be_lowercase)
{
    if(!can_be_empty
    && name.isEmpty())
    {
        SNAP_LOG_FATAL("snap_communicator: a message name cannot be empty.");
        throw snap_communicator_invalid_message("snap_communicator: a message name cannot be empty.");
    }

    for(auto const & c : name)
    {
        if((c < 'a' || c > 'z' || !can_be_lowercase)
        && (c < 'A' || c > 'Z')
        && (c < '0' || c > '9')
        && c != '_')
        {
            SNAP_LOG_FATAL("snap_communicator: a message name must be composed of ASCII 'a'..'z', 'A'..'Z', '0'..'9', or '_' only (also a command must be uppercase only,) \"")(name)("\" is not valid.");
            throw snap_communicator_invalid_message(QString("snap_communicator: a message name must be composed of ASCII 'a'..'z', 'A'..'Z', '0'..'9', or '_' only (also a command must be uppercase only,) \"%1\" is not valid.").arg(name));
        }
    }

    ushort const fc(name[0].unicode());
    if(fc >= '0' && fc <= '9')
    {
        SNAP_LOG_FATAL("snap_communicator: parameter name cannot start with a digit, \"")(name)("\" is not valid.");
        throw snap_communicator_invalid_message(QString("snap_communicator: parameter name cannot start with a digit, \"%1\" is not valid.").arg(name));
    }
}








/////////////////////
// Snap Connection //
/////////////////////


/** \brief Initializes the connection.
 *
 * This function initializes a base connection object.
 */
snap_communicator::snap_connection::snap_connection()
    //: f_name("")
    //, f_priority(0)
    //, f_timeout(-1)
{
}


/** \brief Proceed with the cleanup of the snap_connection.
 *
 * This function cleans up a snap_connection object.
 */
snap_communicator::snap_connection::~snap_connection()
{
}


/** \brief Remove this connection from the communicator it was added in.
 *
 * This function removes the connection from the communicator that
 * it was created in.
 *
 * This happens in several circumstances:
 *
 * \li When the connection is not necessary anymore
 * \li When the connection receives a message saying it should close
 * \li When the connection receives a Hang Up event
 * \li When the connection looks erroneous
 * \li When the connection looks invalid
 *
 * If the connection is not currently connected to a snap_communicator
 * object, then nothing happens.
 */
void snap_communicator::snap_connection::remove_from_communicator()
{
    snap_communicator::instance()->remove_connection(shared_from_this());
}


/** \brief Retrieve the name of the connection.
 *
 * When generating an error or a log the library makes use of this name
 * so we actually know which type of socket generated a problem.
 *
 * \return A constant reference to the connection name.
 */
QString const & snap_communicator::snap_connection::get_name() const
{
    return f_name;
}


/** \brief Change the name of the connection.
 *
 * A connection can be given a name. This is mainly for debug purposes.
 * We will be adding this name in errors and exceptions as they occur.
 *
 * The connection makes a copy of \p name.
 *
 * \param[in] name  The name to give this connection.
 */
void snap_communicator::snap_connection::set_name(QString const & name)
{
    f_name = name;
}


/** \brief Tell us whether this socket is a listener or not.
 *
 * By default a snap_connection object does not represent a listener
 * object.
 *
 * \return The base implementation returns false. Override this
 *         virtual function if your snap_connection is a listener.
 */
bool snap_communicator::snap_connection::is_listener() const
{
    return false;
}


/** \brief Tell us whether this connection is listening on a Unix signal.
 *
 * By default a snap_connection object does not represent a Unix signal.
 * See the snap_signal implementation for further information about
 * Unix signal handling in this library.
 *
 * \return The base implementation returns false.
 */
bool snap_communicator::snap_connection::is_signal() const
{
    return false;
}


/** \brief Tell us whether this socket is used to receive data.
 *
 * If you expect to receive data on this connection, then mark it
 * as a reader by returning true in an overridden version of this
 * function.
 *
 * \return By default this function returns false (nothing to read).
 */
bool snap_communicator::snap_connection::is_reader() const
{
    return false;
}


/** \brief Tell us whether this socket is used to send data.
 *
 * If you expect to send data on this connection, then mark it
 * as a writer by returning true in an overridden version of
 * this function.
 *
 * \return By default this function returns false (nothing to write).
 */
bool snap_communicator::snap_connection::is_writer() const
{
    return false;
}


/** \brief Check whether the socket is valid for this connection.
 *
 * Some connections do not make use of a socket so just checking
 * whether the socket is -1 is not a good way to know whether the
 * socket is valid.
 *
 * The default function assumes that a socket has to be 0 or more
 * to be valid. Other connection implementations may overload this
 * function to allow other values.
 *
 * \return true if the socket is valid.
 */
bool snap_communicator::snap_connection::valid_socket() const
{
    return get_socket() >= 0;
}


/** \brief Check whether this connection is enabled.
 *
 * It is possible to turn a connection ON or OFF using the set_enable()
 * function. This function returns the current value. If true, which
 * is the default, the connection is considered enabled and will get
 * its callbacks called.
 *
 * \return true if the connection is currently enabled.
 */
bool snap_communicator::snap_connection::is_enabled() const
{
    return f_enabled;
}


/** \brief Change the status of a connection.
 *
 * This function let you change the status of a connection from
 * enabled (true) to disabled (false) and vice versa.
 *
 * A disabled connection is not listened on at all. This is similar
 * to returning false in all three functions is_listener(),
 * is_reader(), and is_writer().
 *
 * \param[in] enabled  The new status of the connection.
 */
void snap_communicator::snap_connection::set_enable(bool enabled)
{
    f_enabled = enabled;
}


/** \brief Define the priority of this connection object.
 *
 * By default snap_connection objets have a priority of 100.
 *
 * You may also use the set_priority() to change the priority of a
 * connection at any time.
 *
 * \return The current priority of this connection.
 *
 * \sa set_priority()
 */
int snap_communicator::snap_connection::get_priority() const
{
    return f_priority;
}


/** \brief Change this event priority.
 *
 * This function can be used to change the default priority (which is
 * 100) to a larger or smaller number. A larger number makes the connection
 * less important and callbacks get called later. A smaller number makes
 * the connection more important and callbacks get called sooner.
 *
 * Note that the priority of a connection can modified at any time.
 * It is not guaranteed to be taken in account immediately, though.
 *
 * \exception snap_communicator_parameter_error
 * The priority of the event is out of range when this exception is raised.
 * The value must between between 0 and EVENT_MAX_PRIORITY. Any
 * other value raises this exception.
 *
 * \param[in] priority  Priority of the event.
 */
void snap_communicator::snap_connection::set_priority(priority_t priority)
{
    if(priority < 0 || priority > EVENT_MAX_PRIORITY)
    {
        std::stringstream ss;
        ss << "snap_communicator::set_priority(): priority out of range, this instance of snap_communicator accepts priorities between 0 and "
           << EVENT_MAX_PRIORITY
           << ".";
        throw snap_communicator_parameter_error(ss.str());
    }

    f_priority = priority;

    // make sure that the new order is calculated when we execute
    // the next loop
    //
    snap_communicator::instance()->f_force_sort = true;
}


/** \brief Less than operator to sort connections by priority.
 *
 * This function is used to know whether a connection has a higher or lower
 * priority. This is used when one adds, removes, or changes the priority
 * of a connection. The sorting itself happens in the
 * snap_communicator::run() which knows that something changed whenever
 * it checks the data.
 *
 * The result of the priority mechanism is that callbacks of items with
 * a smaller priorirty will be called first.
 *
 * \param[in] lhs  The left hand side snap_connection.
 * \param[in] rhs  The right hand side snap_connection.
 *
 * \return true if lhs has a smaller priority than rhs.
 */
bool snap_communicator::snap_connection::compare(pointer_t const & lhs, pointer_t const & rhs)
{
    return lhs->get_priority() < rhs->get_priority();
}


/** \brief Return the delay between ticks when this connection times out.
 *
 * All connections can include a timeout delay in microseconds which is
 * used to know when the wait on that specific connection times out.
 *
 * By default connections do not time out. This function returns -1
 * to indicate that this connection does not ever time out. To
 * change the timeout delay use the set_timeout_delay() function.
 *
 * \return This function returns the current timeout delay.
 */
int64_t snap_communicator::snap_connection::get_timeout_delay() const
{
    return f_timeout_delay;
}


/** \brief Change the timeout of this connection.
 *
 * Each connection can be setup with a timeout in microseconds.
 * When that delay is past, the callback function of the connection
 * is called with the EVENT_TIMEOUT flag set (note that the callback
 * may happen along other events.)
 *
 * The current date when this function gets called is the starting
 * point for each following trigger. Because many other callbacks
 * get called, it is not very likely that you will be called
 * exactly on time, but the ticks are guaranteed to be requested
 * on a non moving schedule defined as:
 *
 * \f[
 * \large tick_i = start-time + k \times delay
 * \f]
 *
 * In other words the time and date when ticks happen does not slip
 * with time. However, this implementation may skip one or more
 * ticks at any time (especially if the delay is very small).
 *
 * When a tick triggers an EVENT_TIMEOUT, the snap_communicator::run()
 * function calls calculate_next_tick() to calculate the time when
 * the next tick will occur which will always be in the function.
 *
 * \exception snap_communicator_parameter_error
 * This exception is raised if the timeout_us parameter is not considered
 * valid. The minimum value is 10 and microseconds. You may use -1 to turn
 * off the timeout delay feature.
 *
 * \param[in] timeout_us  The new time out in microseconds.
 */
void snap_communicator::snap_connection::set_timeout_delay(int64_t timeout_us)
{
    if(timeout_us != -1
    && timeout_us < 10)
    {
        throw snap_communicator_parameter_error("snap_communicator::snap_connection::set_timeout_delay(): timeout_us parameter cannot be less than 10 unless it is exactly -1.");
    }

    f_timeout_delay = timeout_us;

    // immediately calculate the next timeout date
    f_timeout_next_date = get_current_date() + f_timeout_delay;
}


/** \brief Calculate when the next tick shall occur.
 *
 * This function calculates the date and time when the next tick
 * has to be triggered. This function is called after the
 * last time the EVENT_TIMEOUT callback was called.
 */
void snap_communicator::snap_connection::calculate_next_tick()
{
    if(f_timeout_delay == -1)
    {
        // no delay based timeout so forget about it
        return;
    }

    // what is now?
    int64_t const now(get_current_date());

    // gap between now and the last time we triggered this timeout
    int64_t const gap(now - f_timeout_next_date);
    if(gap < 0)
    {
        // someone we got called even though now is still larger
        // than f_timeout_next_date
        //
        SNAP_LOG_DEBUG("snap_communicator::snap_connection::calculate_next_tick() called even though the next date is still larger than 'now'.");
        return;
    }

    // number of ticks in that gap, rounded up
    int64_t const ticks((gap + f_timeout_delay - 1) / f_timeout_delay);

    // the next date may be equal to now, however, since it is very
    // unlikely that the tick has happened right on time, and took
    // less than 1ms, this is rather unlikely all around...
    //
    f_timeout_next_date += ticks * f_timeout_delay;
}


/** \brief Return when this connection times out.
 *
 * All connections can include a timeout in microseconds which is
 * used to know when the wait on that specific connection times out.
 *
 * By default connections do not time out. This function returns -1
 * to indicate that this connection does not ever time out. You
 * may overload this function to return a different value so your
 * version can time out.
 *
 * \return This function returns the timeout date in microseconds.
 */
int64_t snap_communicator::snap_connection::get_timeout_date() const
{
    return f_timeout_date;
}


/** \brief Change the date at which you want a timeout event.
 *
 * This function can be used to setup one specific date and time
 * at which this connection should timeout. This specific date
 * is used internally to calculate the amount of time the poll()
 * will have to wait, not including the time it will take
 * to execute other callbacks if any needs to be run (i.e. the
 * timeout is executed last, after all other events, and also
 * priority is used to know which other connections are parsed
 * first.)
 *
 * \exception snap_communicator_parameter_error
 * If the date_us is too small (less than -1) then this exception
 * is raised.
 *
 * \param[in] date_us  The new time out in micro seconds.
 */
void snap_communicator::snap_connection::set_timeout_date(int64_t date_us)
{
    if(date_us < -1)
    {
        throw snap_communicator_parameter_error("snap_communicator::snap_connection::set_timeout_date(): date_us parameter cannot be less than -1.");
    }

    f_timeout_date = date_us;
}


/** \brief Return when this connection expects a timeout.
 *
 * All connections can include a timeout specification which is
 * either a specific day and time set with set_timeout_date()
 * or an repetitive timeout which is defined with the
 * set_timeout_delay().
 *
 * If neither timeout is set the function returns -1. Otherwise
 * the function will calculate when the connection is to time
 * out and return that date.
 *
 * If the date is already in the past then the callback
 * is called immediately with the EVENT_TIMEOUT flag set.
 *
 * \note
 * If the timeout date is triggered, then the loop calls
 * set_timeout_date(-1) because the date timeout is expected
 * to only be triggered once. This resetting is done before
 * calling the user callback which can in turn set a new
 * value back in the connection object.
 *
 * \return This function returns -1 when no timers are set
 *         or a timestamp in microseconds when the timer is
 *         expected to trigger.
 */
int64_t snap_communicator::snap_connection::get_timeout_timestamp() const
{
    if(f_timeout_date != -1)
    {
        // this one is easy, it is already defined as expected
        return f_timeout_date;
    }

    if(f_timeout_delay != -1)
    {
        // no timeout defined
        return f_timeout_next_date;
    }

    return -1;
}


/** \brief Save the timeout stamp just before calling poll().
 *
 * This function is called by the run() function before the poll()
 * gets called. It makes sure to save the timeout timestamp so
 * when we check the connections again after poll() returns and
 * any number of callbacks were called, the timeout does or does
 * not happen as expected.
 *
 * \return The timeout timestamp as returned by get_timeout_timestamp().
 *
 * \sa get_saved_timeout_timestamp()
 * \sa run()
 */
int64_t snap_communicator::snap_connection::save_timeout_timestamp()
{
    f_saved_timeout_stamp = get_timeout_timestamp();
    return f_saved_timeout_stamp;
}


/** \brief Get the saved timeout timestamp.
 *
 * This function returns the timeout as saved by the
 * save_timeout_timestamp() function. The timestamp returned by
 * this funtion was frozen so if the user calls various timeout
 * functions that could completely change the timeout stamp that
 * the get_timeout_timestamp() would return just at the time we
 * want to know whether th timeout callback needs to be called
 * will be ignored by the loop.
 *
 * \return The saved timeout stamp as returned by save_timeout_timestamp().
 *
 * \sa save_timeout_timestamp()
 * \sa run()
 */
int64_t snap_communicator::snap_connection::get_saved_timeout_timestamp() const
{
    return f_saved_timeout_stamp;
}


/** \brief Make this connection socket a non-blocking socket.
 *
 * For the read and write to work as expected we generally need
 * to make those sockets non-blocking.
 *
 * For accept(), you do just one call and return and it will not
 * block on you. It is important to not setup a socket you
 * listen on as non-blocking if you do not want to risk having the
 * accepted sockets non-blocking.
 *
 * \param[in] non_blocking_socket  Make socket non-block if true,
 *            blocking if false.
 */
void snap_communicator::snap_connection::non_blocking() const
{
    if(valid_socket()
    && get_socket() >= 0)
    {
        int optval(1);
        ioctl(get_socket(), FIONBIO, &optval);
    }
}


/** \brief Ask the OS to keep the socket alive.
 *
 * This function marks the socket with the SO_KEEPALIVE flag. This means
 * the OS implementation of the network stack should regularly send
 * small messages over the network to keep the connection alive.
 *
 * The function returns whether the function works or not. If the function
 * fails, it logs a warning and returns.
 */
void snap_communicator::snap_connection::keep_alive() const
{
    if(get_socket() != -1)
    {
        int optval(1);
        socklen_t const optlen(sizeof(optval));
        if(setsockopt(get_socket(), SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) != 0)
        {
            SNAP_LOG_WARNING("snap_communicator::snap_connection::keep_alive(): an error occurred trying to mark socket with SO_KEEPALIVE.");
        }
    }
}


/** \brief This callback gets called whenever the connection times out.
 *
 * This function is called whenever a timeout is detected on this
 * connection. It is expected to be overwritten by your class if
 * you expect to use the timeout feature.
 *
 * The snap_timer class is expected to always have a timer (although
 * the connection can temporarily be disabled) which triggers this
 * callback on a given periodicity.
 */
void snap_communicator::snap_connection::process_timeout()
{
}


/** \brief This callback gets called whenever the signal happened.
 *
 * This function is called whenever a certain signal (as defined in
 * your snap_signal object) was detected while waiting for an
 * event.
 */
void snap_communicator::snap_connection::process_signal()
{
}


/** \brief This callback gets called whenever data can be read.
 *
 * This function is called whenever a socket has data that can be
 * read. For UDP, this means reading one packet. For TCP, it means
 * you can read at least one byte. To avoid blocking in TCP,
 * you must have called the non_blocking() function on that
 * connection, then you can attempt to read as much data as you
 * want.
 */
void snap_communicator::snap_connection::process_read()
{
}


/** \brief This callback gets called whenever data can be written.
 *
 * This function is called whenever a socket has space in its output
 * buffers to write data there.
 *
 * For UDP, this means writing one packet.
 *
 * For TCP, it means you can write at least one byte. To be able to
 * write as many bytes as you want, you must make sure to make the
 * socket non_blocking() first, then you can write as many bytes as
 * you want, although all those bytes may not get written in one
 * go (you may need to wait for the next call to this function to
 * finish up your write.)
 */
void snap_communicator::snap_connection::process_write()
{
}


/** \brief This callback gets called whenever a connection is made.
 *
 * A listening server receiving a new connection gets this function
 * called. The function is expected to create a new connection object
 * and add it to the communicator.
 *
 * \code
 *      // get the socket from the accept() function
 *      int const client_socket(accept());
 *      client_impl::pointer_t connection(new client_impl(get_communicator(), client_socket));
 *      connection->set_name("connection created by server on accept()");
 *      get_communicator()->add_connection(connection);
 * \endcode
 */
void snap_communicator::snap_connection::process_accept()
{
}


/** \brief This callback gets called whenever an error is detected.
 *
 * If an error is detected on a socket, this callback function gets
 * called. By default the function removes the connection from
 * the communicator because such errors are generally non-recoverable.
 *
 * The function also logs an error message.
 */
void snap_communicator::snap_connection::process_error()
{
    // TBD: should we offer a virtual close() function to handle this
    //      case? because the get_socket() function will not return
    //      -1 after such errors...

    SNAP_LOG_ERROR("socket ")(get_socket())(" of connection \"")(f_name)("\" was marked as erroneous by the kernel.");

    remove_from_communicator();
}


/** \brief This callback gets called whenever a hang up is detected.
 *
 * When the remote connection (client or server) closes a socket
 * on their end, then the other end is signaled by getting this
 * callback called.
 *
 * Note that this callback will be called after the process_read()
 * and process_write() callbacks. The process_write() is unlikely
 * to work at all. However, the process_read() may be able to get
 * a few more bytes from the remove connection and act on it.
 *
 * By default a connection gets removed from the communicator
 * when the hang up even occurs.
 */
void snap_communicator::snap_connection::process_hup()
{
    // TBD: should we offer a virtual close() function to handle this
    //      case? because the get_socket() function will not return
    //      -1 after such errors...

    SNAP_LOG_DEBUG("socket of connection \"")(f_name)("\" hang up.");

    remove_from_communicator();
}


/** \brief This callback gets called whenever an invalid socket is detected.
 *
 * I am not too sure at the moment when we are expected to really receive
 * this call. How does a socket become invalid (i.e. does it get closed
 * and then the user still attempts to use it)? In most cases, this should
 * probably never happen.
 *
 * By default a connection gets removed from the communicator
 * when the invalid even occurs.
 *
 * This function also logs the error.
 */
void snap_communicator::snap_connection::process_invalid()
{
    // TBD: should we offer a virtual close() function to handle this
    //      case? because the get_socket() function will not return
    //      -1 after such errors...

    SNAP_LOG_ERROR("socket of connection \"")(f_name)("\" was marked as invalid by the kernel.");

    remove_from_communicator();
}


/** \brief Callback called whenever this connection gets added.
 *
 * This function gets called whenever this connection is added to
 * the snap_communicator object. This gives you the opportunity
 * to do additional initialization before the run() loop gets
 * called or re-entered.
 */
void snap_communicator::snap_connection::connection_added()
{
}


/** \brief Callback called whenever this connection gets removed.
 *
 * This callback gets called after it got removed from the
 * snap_communicator object. This gives you the opportunity
 * to do additional clean ups before the run() loop gets
 * re-entered.
 */
void snap_communicator::snap_connection::connection_removed()
{
}







////////////////
// Snap Timer //
////////////////


/** \brief Initializes the timer object.
 *
 * This function initializes the timer object with the specified \p timeout
 * defined in microseconds.
 *
 * Note that by default all snap_connection objects are marked as persistent
 * since in most cases that is the type of connections you are interested
 * in. Therefore timers are also marked as persistent. This means if you
 * want a one time callback, you want to call the remove_connection()
 * function with your timer from your callback.
 *
 * \note
 * POSIX offers timers (in Linux since kernel version 2.6), only
 * (a) these generate signals, which is generally considered slow
 * in comparison to a timeout assigned to the poll() function, and
 * (b) the kernel posts at most one timer signal at a time across
 * one process, in other words, if 5 timers time out before you are
 * given a chance to process the timer, you only get one single
 * signal.
 *
 * \param[in] communicator  The snap communicator controlling this connection.
 * \param[in] timeout_us  The timeout in microseconds.
 */
snap_communicator::snap_timer::snap_timer(int64_t timeout_us)
{
    if(timeout_us == 0)
    {
        // if zero, we assume that the timeout is a one time trigger
        // and that it will be set to other dates at other later times
        //
        set_timeout_date(get_current_date());
    }
    else
    {
        set_timeout_delay(timeout_us);
    }
}


/** \brief Retrieve the socket of the timer object.
*
* Timer objects are never attached to a socket so this function always
 * returns -1.
 *
 * \note
 * You should not override this function since there is not other
 * value it can return.
 *
 * \return Always -1.
 */
int snap_communicator::snap_timer::get_socket() const
{
    return -1;
}


/** \brief Tell that the socket is always valid.
 *
 * This function always returns true since the timer never uses a socket.
 *
 * \return Always true.
 */
bool snap_communicator::snap_timer::valid_socket() const
{
    return true;
}







/////////////////
// Snap Signal //
/////////////////


/** \brief Initializes the signal object.
 *
 * This function initializes the signal object with the specified
 * \p posix_signal which represents a POSIX signal such as SIGHUP,
 * SIGTERM, SIGUSR1, SIGUSR2, etc.
 *
 * The signal automatically gets masked out. This allows us to
 * unmask the signal only when we are ready to call ppoll() and
 * thus not have the signal break any of our normal user code.
 *
 * The ppoll() function unblocks all the signals that you listen
 * to (i.e. for each snap_signal object you created.) The run()
 * loop ends up calling your process_signal() callback function.
 *
 * Note that the snap_signal callback is called from the normal user
 * environment and not directly from the POSIX signal handler.
 * This means you can call any function from your callback.
 *
 * \note
 * IMPORTANT: Remember that POSIX signals stop your code at a 'breakable'
 * point which in many circumstances can create many problems unless
 * you make sure to mask signals while doing work. For example, you
 * could end up with a read() returning an error when the file you
 * are reading has absolutely no error but a dude decided to signal
 * you with a 'kill -HUP 123'...
 *
 * \code
 *      {
 *          // use an RAII masking mechanism
 *          mask_posix_signal mask();
 *
 *          // do your work (i.e. read/write/etc.)
 *          ...
 *      }
 * \endcode
 *
 * \par
 * The best way in our processes will be to block all signals except
 * while poll() is called (using ppoll() for the feat.)
 *
 * \warning
 * The the signal gets masked by this constructor. If you want to make
 * sure that most of your code does not get affected by said signal,
 * make sure to create your snap_signal object early on or mask those
 * signals beforehand. Otherwise the signal could happen before it
 * gets masked. Initialization of your process may not require
 * protection anyway.
 *
 * \bug
 * You should not use signal() and setup a handler for the same signal.
 * It will not play nice to have both types of signal handlers.
 *
 * \exception snap_communicator_initialization_error
 * Create multiple snap_signal() with the same posix_signal parameter
 * is not supported and this exception is raised whenever you attempt
 * to do that. Remember that you can have at most one snap_communicator
 * object (hence the singleton.)
 *
 * \exception snap_communicator_runtime_error
 * The signalfd() function is expected to create a "socket" (file
 * descriptor) listening for incoming signals. If it fails, this
 * exception is raised (which is very similar to other socket
 * based connections which throw whenever a connection cannot
 * be achieved.)
 *
 * \param[in] posix_signal  The signal to be managed by this snap_signal.
 */
snap_communicator::snap_signal::snap_signal(int posix_signal)
    : f_signal(posix_signal)
    //, f_socket(-1) -- auto-init
    //, f_signal_info() -- auto-init
{
    int const r(sigismember(&g_signal_handlers, f_signal));
    if(r != 0)
    {
        if(r == 1)
        {
            // this could be fixed, but probably not worth the trouble...
            throw snap_communicator_initialization_error("the same signal cannot be created more than once in your entire process.");
        }
        // f_singal is not considered valid by this OS
        throw snap_communicator_initialization_error("posix_signal (f_signal) is not a valid/recognized signal number.");
    }

    // create a mask for that signal
    //
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, f_signal); // ignore error, we already know f_signal is valid

    // first we block the signal
    //
    if(sigprocmask(SIG_BLOCK, &set, nullptr) != 0)
    {
        throw snap_communicator_runtime_error("sigprocmask() failed to block signal.");
    }

    // second we create a "socket" for the signal (really it is a file
    // descriptor manager by the kernel)
    //
    f_socket = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
    if(f_socket == -1)
    {
        int const e(errno);
        SNAP_LOG_ERROR("signalfd() failed to create a signal listener for signal ")(f_signal)(" (errno: ")(e)(" -- ")(strerror(e))(")");
        throw snap_communicator_runtime_error("signalfd() failed to create a signal listener.");
    }

    // mark this signal as in use
    //
    sigaddset(&g_signal_handlers, f_signal); // ignore error, we already know f_signal is valid
}


/** \brief Restore the signal as it was before you created a snap_signal.
 *
 * The destructor is expected to restore the signal to what it was
 * before you create this snap_signal. Of course, if you created
 * other signal handlers in between, it will not work right since
 * this function will destroy your handler pointer.
 *
 * To do it right, it has to be done in order (i.e. set handler 1, set
 * handler 2, set handler 3, remove handler 3, remove handler 2, remove
 * handler 1.) We do not guarantee anything at this level!
 */
snap_communicator::snap_signal::~snap_signal()
{
    close(f_socket);
    sigdelset(&g_signal_handlers, f_signal);     // ignore error, we already know f_signal is valid
}


/** \brief Tell that this connection is listening on a Unix signal.
 *
 * The snap_signal implements the signal listening feature. We use
 * a simple flag in the virtual table to avoid a more expansive
 * dynamic_cast<>() is a loop that goes over all the connections
 * you have defined.
 *
 * \return The base implementation returns false.
 */
bool snap_communicator::snap_signal::is_signal() const
{
    return true;
}


/** \brief Retrieve the "socket" of the signal object.
 *
 * Signal objects have a socket (file descriptor) assigned to them
 * using the signalfd() function.
 *
 * \note
 * You should not override this function since there is no other
 * value it can return.
 *
 * \return The signal socket to listen on with poll().
 */
int snap_communicator::snap_signal::get_socket() const
{
    return f_socket;
}


/** \brief Retrieve the PID of the child process that just emitted SIGCHLD.
 *
 * This function returns the process identifier (pid_t) of the child that
 * just sent us a SIGCHLD Unix signal.
 *
 * \exception snap_communicator_runtime_error
 * This exception is raised if the function gets called before any signal
 * ever occurred.
 *
 * \return The process identifier (pid_t) of the child that died.
 */
pid_t snap_communicator::snap_signal::get_child_pid() const
{
    if(f_signal_info.ssi_signo == 0)
    {
        throw snap_communicator_runtime_error("snap_signal::get_child_pid() called before any signal ever occurred.");
    }

    return f_signal_info.ssi_pid;
}


/** \brief Processes this signal.
 *
 * This function reads the signal "socket" for all the signal received
 * so far.
 *
 * For each instance found in the signal queue, the process_signal() gets
 * called.
 */
void snap_communicator::snap_signal::process()
{
    // loop any number of times as required
    // (or can we receive a maximum of 1 such signal at a time?)
    //
    for(;;)
    {
        int const r(read(f_socket, &f_signal_info, sizeof(f_signal_info)));
        if(r == sizeof(f_signal_info))
        {
            process_signal();
        }
        else
        {
            if(r == -1)
            {
                // if EAGAIN then we are done as expected, any other error
                // is logged
                //
                if(errno != EAGAIN)
                {
                    int const e(errno);
                    SNAP_LOG_ERROR("an error occurred while reading from the signalfd() file descriptor. (errno: ")(e)(" -- ")(strerror(e));
                }
            }
            else
            {
                // what to do? what to do?
                SNAP_LOG_ERROR("reading from the signalfd() file descriptor did not return the expected size. (got ")(r)(", expected ")(sizeof(f_signal_info))(")");
            }
            break;
        }
    }
}







/////////////////////////////
// Snap Thread Done Signal //
/////////////////////////////


/** \brief Initializes the "thread done signal" object.
 *
 * To know that a thread is done, we need some form of signal that the
 * poll() can wake up on. For the purpose we currently use a pipe because
 * a full socket is rather slow to setup compare to a simple pipe.
 *
 * To use this signal, one creates a Thread Done Signal and adds the
 * new connection to the Snap Communicator object. Then when the thread
 * is done, the thread calls the thread_done() function. That will wake
 * up the main process.
 *
 * The same snap_thread_done_signal class can be used multiple times,
 * but only by one thread at a time. Otherwise you cannot know which
 * thread sent the message and by the time you attempt a join, you may
 * be testing the wrong thread (either that or you need another type
 * of synchronization mechanism.)
 *
 * \code
 *      class thread_done_impl
 *              : snap::snap_communicator::snap_thread_done_signal::snap_thread_done_signal
 *      {
 *          ...
 *          void process_read()
 *          {
 *              // this function gets called when the thread is about
 *              // to exit or has exited; since the write to the pipe
 *              // happens before the thread really exited, but should
 *              // be near the very end, you should be fine calling the
 *              // snap_thread::stop() function to join with it very
 *              // quickly.
 *              ...
 *          }
 *          ...
 *      };
 *
 *      // in the main thread
 *      snap::snap_communicator::snap_thread_done_signal::pointer_t s(new thread_done_impl);
 *      snap::snap_communicator::instance()->add_connection(s);
 *
 *      // create thread... and make sure the thread has access to 's'
 *      ...
 *
 *      // in the thread, before exiting we do:
 *      s->thread_done();
 *
 *      // around here, in the timeline, the process_read() function
 *      // gets called
 * \endcode
 *
 * \todo
 * Change the implementation to use eventfd() instead of pipe2().
 * Pipes are using more resources and are slower to use than
 * an eventfd.
 */
snap_communicator::snap_thread_done_signal::snap_thread_done_signal()
{
    if(pipe2(f_pipe, O_NONBLOCK | O_CLOEXEC) != 0)
    {
        // pipe could not be created
        throw snap_communicator_initialization_error("somehow the pipes used to detect the death of a thread could not be created.");
    }
}


/** \brief Close the pipe used to detect the thread death.
 *
 * The destructor is expected to close the pipe opned in the constructor.
 */
snap_communicator::snap_thread_done_signal::~snap_thread_done_signal()
{
    close(f_pipe[0]);
    close(f_pipe[1]);
}


/** \brief Tell that this connection expects incoming data.
 *
 * The snap_thread_done_signal implements a signal that a secondary
 * thread can trigger before it quits, hence waking up the main
 * thread immediately instead of polling.
 *
 * \return The function returns true.
 */
bool snap_communicator::snap_thread_done_signal::is_reader() const
{
    return true;
}


/** \brief Retrieve the "socket" of the thread done signal object.
 *
 * The Thread Done Signal is implemented using a pair of pipes.
 * One of the pipes is returned as the "socket" and the other is
 * used to "write the signal".
 *
 * \return The signal "socket" to listen on with poll().
 */
int snap_communicator::snap_thread_done_signal::get_socket() const
{
    return f_pipe[0];
}


/** \brief Read the byte that was written in the thread_done().
 *
 * This function implementation reads one byte that was written by
 * thread_done() so the pipes can be reused multiple times.
 */
void snap_communicator::snap_thread_done_signal::process_read()
{
    char c(0);
    if(read(f_pipe[0], &c, sizeof(char)) != sizeof(char))
    {
        int const e(errno);
        SNAP_LOG_ERROR("an error occurred while reading from a pipe used to know whether a thread is done (errno: ")(e)(" -- ")(strerror(e))(").");
    }
}


/** \brief Send the signal from the secondary thread.
 *
 * This function writes one byte in the pipe, which has the effect of
 * waking up the poll() of the main thread. This way we avoid having
 * to lock the file.
 *
 * The thread is expected to call this function just before it returns.
 */
void snap_communicator::snap_thread_done_signal::thread_done()
{
    char c(1);
    if(write(f_pipe[1], &c, sizeof(char)) != sizeof(char))
    {
        int const e(errno);
        SNAP_LOG_ERROR("an error occurred while writing to a pipe used to know whether a thread is done (errno: ")(e)(" -- ")(strerror(e))(").");
    }
}







//////////////////////////////////
// Snap Inter-Thread Connection //
//////////////////////////////////



/** \brief Initializes the inter-thread connection.
 *
 * This function creates two queues to communicate between two threads.
 * At this point, we expect such connections to only be used between
 * two threads because we cannot listen on more than one socket.
 *
 * The connection is expected to be created by "thread A". This means
 * the send_message() for "thread A" adds messages to the queue of
 * "thread B" and the process_message() for "thread A" reads
 * messages from the "thread A" queue, and vice versa.
 *
 * In order to know whether a queue has data in it, we use an eventfd().
 * One of them is for "thread A" and the other is for "thread B".
 *
 * \todo
 * To support all the features of a snap_connection on both sides
 * we would have to allocate a sub-connection object for thread B.
 * That sub-connection object would then be used just like a full
 * regular connection with all of its own parameters. Actually the
 * FIFO of messages could then clearly be segregated in each object.
 *
 * \exception snap_communicator_initialization_error
 * This exception is raised if the pipes (socketpair) cannot be created.
 */
snap_communicator::snap_inter_thread_message_connection::snap_inter_thread_message_connection()
{
    f_creator_id = gettid();

    f_thread_a.reset(new int(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)), fd_deleter);
    if(*f_thread_a == -1)
    {
        // eventfd could not be created
        throw snap_communicator_initialization_error("could not create eventfd for thread A");
    }

    f_thread_b.reset(new int(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)), fd_deleter);
    if(*f_thread_b == -1)
    {
        // eventfd could not be created
        throw snap_communicator_initialization_error("could not create eventfd for thread B");
    }
}


/** \brief Make sure to close the eventfd objects.
 *
 * The destructor ensures that the eventfd objects allocated by the
 * constructor get closed.
 */
snap_communicator::snap_inter_thread_message_connection::~snap_inter_thread_message_connection()
{
}


/** \brief Close the thread communication early.
 *
 * This function closes the pair of eventfd managed by this
 * inter-thread connection object.
 *
 * After this call, the inter-thread connection is closed and cannot be
 * used anymore. The read and write functions will return immediately
 * if called.
 */
void snap_communicator::snap_inter_thread_message_connection::close()
{
    f_thread_a.reset();
    f_thread_b.reset();
}


/** \brief Poll the connection in the child.
 *
 * There can be only one snap_communicator, therefore, the threa
 * cannot make use of it since it is only for the main application.
 * This poll() function can be used by the child to wait on the
 * connection.
 *
 * You may specify a timeout as usual.
 *
 * \exception snap_communicator_runtime_error
 * If an interrupt happens and stops the poll() then this exception is
 * raised. If not enough memory is available to run the poll() function,
 * this errors is raised.
 *
 * \exception snap_communicator_parameter_error
 * Somehow a buffer was moved out of our client's space (really that one
 * is not likely to happen...). Too many file descriptors in the list of
 * fds (not likely to happen since we just have one!)
 *
 * \exception snap_communicator_parameter_error
 *
 * \param[in] timeout  The maximum amount of time to wait in microseconds.
 *                     Use zero (0) to not block at all.
 *
 * \return -1 if an error occurs, 0 on success
 */
int snap_communicator::snap_inter_thread_message_connection::poll(int timeout)
{
    for(;;)
    {
        // are we even enabled?
        //
        struct pollfd fd;
        fd.events = POLLIN | POLLPRI | POLLRDHUP;
        fd.fd = get_socket();

        if(fd.fd < 0
        || !is_enabled())
        {
            return -1;
        }

        // we cannot use this connection timeout information; it would
        // otherwise be common to both threads; so instead we have
        // a parameter which is used by the caller to tell us how long
        // we have to wait
        //
        // convert microseconds to milliseconds for poll()
        //
        if(timeout > 0)
        {
            timeout /= 1000;
            if(timeout == 0)
            {
                // less than one is a waste of time (CPU intenssive
                // until the time is reached, we can be 1 ms off
                // instead...)
                //
                timeout = 1;
            }
        }
        else
        {
            // negative numbers are adjusted to zero.
            //
            timeout = 0;
        }

        int const r(::poll(&fd, 1, timeout));
        if(r < 0)
        {
            // r < 0 means an error occurred
            //
            int const e(errno);

            if(e == EINTR)
            {
                // Note: if the user wants to prevent this error, he should
                //       use the snap_signal with the Unix signals that may
                //       happen while calling poll().
                //
                throw snap_communicator_runtime_error("EINTR occurred while in poll() -- interrupts are not supported yet though");
            }
            if(e == EFAULT)
            {
                throw snap_communicator_parameter_error("buffer was moved out of our address space?");
            }
            if(e == EINVAL)
            {
                // if this is really because nfds is too large then it may be
                // a "soft" error that can be fixed; that being said, my
                // current version is 16K files which frankly when we reach
                // that level we have a problem...
                //
                struct rlimit rl;
                getrlimit(RLIMIT_NOFILE, &rl);
                throw snap_communicator_parameter_error(QString("too many file fds for poll, limit is currently %1, your kernel top limit is %2")
                            .arg(rl.rlim_cur)
                            .arg(rl.rlim_max).toStdString());
            }
            if(e == ENOMEM)
            {
                throw snap_communicator_runtime_error("poll() failed because of memory");
            }
            throw snap_communicator_runtime_error(QString("poll() failed with error %1").arg(e).toStdString());
        }

        if(r == 0)
        {
            // poll() timed out, just return so the thread can do some
            // additional work
            //
            return 0;
        }

        // we reach here when there is something to read
        //
        if((fd.revents & (POLLIN | POLLPRI)) != 0)
        {
            process_read();
        }
        // at this point we do not request POLLOUT and assume that the
        // write() function will never fail
        //
        //if((fd.revents & POLLOUT) != 0)
        //{
        //    process_write();
        //}
        if((fd.revents & POLLERR) != 0)
        {
            process_error();
        }
        if((fd.revents & (POLLHUP | POLLRDHUP)) != 0)
        {
            process_hup();
        }
        if((fd.revents & POLLNVAL) != 0)
        {
            process_invalid();
        }
    }
    NOTREACHED();
}


/** \brief Pipe connections accept reads.
 *
 * This function returns true meaning that the pipe connection can be
 * used to read data.
 *
 * \return true since a pipe connection is a reader.
 */
bool snap_communicator::snap_inter_thread_message_connection::is_reader() const
{
    return true;
}


/** \brief This function returns the pipe we want to listen on.
 *
 * This function returns the file descriptor of one of the two
 * sockets. The parent process returns the descriptor of socket
 * number 0. The child process returns the descriptor of socket
 * number 1.
 *
 * \note
 * If the close() function was called, this function returns -1.
 *
 * \return A pipe descriptor to listen on with poll().
 */
int snap_communicator::snap_inter_thread_message_connection::get_socket() const
{
    if(f_creator_id == gettid())
    {
        return *f_thread_a;
    }

    return *f_thread_b;
}


/** \brief Read one message from the FIFO.
 *
 * This function reads one message from the FIFO specific to this
 * thread. If the FIFO is empty, 
 *
 * The function makes sure to use the correct socket for the calling
 * process (i.e. depending on whether this is the parent or child.)
 *
 * Just like the system write(2) function, errno is set to the error
 * that happened when the function returns -1.
 *
 * \return The number of bytes written to this pipe socket, or -1 on errors.
 */
void snap_communicator::snap_inter_thread_message_connection::process_read()
{
    snap_communicator_message message;

    bool const is_thread_a(f_creator_id == gettid());

    // retrieve the message
    //
    bool const got_message((is_thread_a ? f_message_a : f_message_b).pop_front(message, 0));

    // "remove" that one object from the semaphore counter
    //
    uint64_t value(1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    if(read(is_thread_a ? *f_thread_a : *f_thread_b, &value, sizeof(value)) != sizeof(value))
    {
        throw snap_communicator_runtime_error("an error occurred while reading from inter-thread eventfd description.");
    }
#pragma GCC diagnostic pop

    // send the message for processing
    // got_message should always be true, but just in case...
    //
    if(got_message)
    {
        if(is_thread_a)
        {
            process_message_a(message);
        }
        else
        {
            process_message_b(message);
        }
    }
}


/** \brief Send a message to the other end of this connection.
 *
 * This function sends the specified \p message to the thread
 * on the other side of the connection.
 *
 * \note
 * We are not a writer. We directly write to the corresponding
 * thread eventfd() so it can wake up and read the message we
 * just sent. There is only one reason for which the write
 * would not be available, we already sent 2^64-2 messages,
 * which is not likely to happen since memory would not support
 * that many messages.
 *
 * \todo
 * One day we probably will want to be able to have support for a
 * process_write() callback... Maybe we should do the write there.
 * Only we need to know where the write() would have to happen...
 * That's a bit complicated right now for a feature that would not
 * get tested well...
 *
 * \param[in] message  The message to send to the other side.
 */
void snap_communicator::snap_inter_thread_message_connection::send_message(snap_communicator_message const & message)
{
    if(f_creator_id == gettid())
    {
        f_message_b.push_back(message);
        uint64_t const value(1);
        if(write(*f_thread_b, &value, sizeof(value)) != sizeof(value))
        {
            throw snap_communicator_runtime_error("an error occurred while writing to inter-thread eventfd description (thread B)");
        }
    }
    else
    {
        f_message_a.push_back(message);
        uint64_t const value(1);
        if(write(*f_thread_a, &value, sizeof(value)) != sizeof(value))
        {
            throw snap_communicator_runtime_error("an error occurred while writing to inter-thread eventfd description (thread B)");
        }
    }
}




//////////////////////////
// Snap Pipe Connection //
//////////////////////////



/** \brief Initializes the pipe connection.
 *
 * This function creates the pipes that are to be used to connect
 * two processes (these are actually Unix sockets). These are
 * used whenever you fork() so the parent process can very quickly
 * communicate with the child process using complex messages just
 * like you do between services and Snap Communicator.
 *
 * \warning
 * The sockets are opened in a non-blocking state. However, they are
 * not closed when you call fork() since they are to be used across
 * processes.
 *
 * \warning
 * You need to create a new snap_pipe_connection each time you want
 * to create a new child.
 *
 * \exception snap_communicator_initialization_error
 * This exception is raised if the pipes (socketpair) cannot be created.
 */
snap_communicator::snap_pipe_connection::snap_pipe_connection()
{
    if(socketpair(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0, f_socket) != 0)
    {
        // pipe could not be created
        throw snap_communicator_initialization_error("somehow the pipes used for a two way pipe connection could not be created.");
    }

    f_parent = getpid();
}


/** \brief Read data from this pipe connection.
 *
 * This function reads up to count bytes from this pipe connection.
 *
 * The function makes sure to use the correct socket for the calling
 * process (i.e. depending on whether this is the parent or child.)
 *
 * Just like the system read(2) function, errno is set to the error
 * that happened when the function returns -1.
 *
 * \param[in] buf  A pointer to a buffer of data.
 * \param[in] count  The number of bytes to read from the pipe connection.
 *
 * \return The number of bytes read from this pipe socket, or -1 on errors.
 */
ssize_t snap_communicator::snap_pipe_connection::read(void * buf, size_t count)
{
    int const s(get_socket());
    if(s == -1)
    {
        errno = EBADF;
        return -1;
    }
    return ::read(s, buf, count);
}


/** \brief Write data to this pipe connection.
 *
 * This function writes count bytes to this pipe connection.
 *
 * The function makes sure to use the correct socket for the calling
 * process (i.e. depending on whether this is the parent or child.)
 *
 * Just like the system write(2) function, errno is set to the error
 * that happened when the function returns -1.
 *
 * \param[in] buf  A pointer to a buffer of data.
 * \param[in] count  The number of bytes to write to the pipe connection.
 *
 * \return The number of bytes written to this pipe socket, or -1 on errors.
 */
ssize_t snap_communicator::snap_pipe_connection::write(void const * buf, size_t count)
{
    int const s(get_socket());
    if(s == -1)
    {
        errno = EBADF;
        return -1;
    }
    if(buf != nullptr && count > 0)
    {
        return ::write(s, buf, count);
    }
    return 0;
}


/** \brief Make sure to close the pipes.
 *
 * The destructor ensures that the pipes get closed.
 *
 * They may already have been closed if a broken pipe was detected.
 */
snap_communicator::snap_pipe_connection::~snap_pipe_connection()
{
    close();
}


/** \brief Close the sockets.
 *
 * This function closes the pair of sockets managed by this
 * pipe connection object.
 *
 * After this call, the pipe connection is closed and cannot be
 * used anymore. The read and write functions will return immediately
 * if called.
 */
void snap_communicator::snap_pipe_connection::close()
{
    if(f_socket[0] != -1)
    {
        ::close(f_socket[0]);
        ::close(f_socket[1]);
        f_socket[0] = -1;
        f_socket[1] = -1;
    }
}


/** \brief Pipe connections accept reads.
 *
 * This function returns true meaning that the pipe connection can be
 * used to read data.
 *
 * \return true since a pipe connection is a reader.
 */
bool snap_communicator::snap_pipe_connection::is_reader() const
{
    return true;
}


/** \brief This function returns the pipe we want to listen on.
 *
 * This function returns the file descriptor of one of the two
 * sockets. The parent process returns the descriptor of socket
 * number 0. The child process returns the descriptor of socket
 * number 1.
 *
 * \note
 * If the close() function was called, this function returns -1.
 *
 * \return A pipe descriptor to listen on with poll().
 */
int snap_communicator::snap_pipe_connection::get_socket() const
{
    if(f_parent == getpid())
    {
        return f_socket[0];
    }

    return f_socket[1];
}




/////////////////////////////////
// Snap Pipe Buffer Connection //
/////////////////////////////////




/** \brief Pipe connections accept writes.
 *
 * This function returns true when there is some data in the pipe
 * connection buffer meaning that the pipe connection needs to
 * send data to the other side of the pipe.
 *
 * \return true if some data has to be written to the pipe.
 */
bool snap_communicator::snap_pipe_buffer_connection::is_writer() const
{
    return get_socket() != -1 && !f_output.empty();
}


/** \brief Write the specified data to the pipe buffer.
 *
 * This function writes the data specified by \p data to the pipe buffer.
 * Note that the data is not sent immediately. This will only happen
 * when the Snap Communicator loop is re-entered.
 *
 * \param[in] data  The pointer to the data to write to the pipe.
 * \param[in] length  The size of the data buffer.
 *
 * \return The number of bytes written. The function returns 0 when no
 *         data can be written to that connection (i.e. it was already
 *         closed or data is a null pointer.)
 */
ssize_t snap_communicator::snap_pipe_buffer_connection::write(void const * data, size_t length)
{
    if(get_socket() == -1)
    {
        errno = EBADF;
        return -1;
    }

    if(data != nullptr && length > 0)
    {
        char const * d(reinterpret_cast<char const *>(data));
        f_output.insert(f_output.end(), d, d + length);
        return length;
    }

    return 0;
}


/** \brief Read data that was received on this pipe.
 *
 * This function is used to read data whenever the process on
 * the other side sent us a message.
 */
void snap_communicator::snap_pipe_buffer_connection::process_read()
{
    if(get_socket() != -1)
    {
        // we read one character at a time until we get a '\n'
        // since we have a non-blocking socket we can read as
        // much as possible and then check for a '\n' and keep
        // any extra data in a cache.
        //
        std::vector<char> buffer;
        buffer.resize(1024);
        for(;;)
        {
            errno = 0;
            ssize_t const r(read(&buffer[0], buffer.size()));
            if(r > 0)
            {
                for(ssize_t position(0); position < r; )
                {
                    std::vector<char>::const_iterator it(std::find(buffer.begin() + position, buffer.begin() + r, '\n'));
                    if(it == buffer.begin() + r)
                    {
                        // no newline, just add the whole thing
                        f_line += std::string(&buffer[position], r - position);
                        break; // do not waste time, we know we are done
                    }

                    // retrieve the characters up to the newline
                    // character and process the line
                    //
                    f_line += std::string(&buffer[position], it - buffer.begin() - position);
                    process_line(QString::fromUtf8(f_line.c_str()));

                    // done with that line
                    f_line.clear();

                    // we had a newline, we may still have some data
                    // in that buffer; (+1 to skip the '\n' itself)
                    //
                    position = it - buffer.begin() + 1;
                }
            }
            else if(r == 0 || errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // no more data available at this time
                break;
            }
            else //if(r < 0)
            {
                int const e(errno);
                SNAP_LOG_ERROR("an error occurred while reading from socket (errno: ")(e)(" -- ")(strerror(e))(").");
                process_error();
                return;
            }
        }
    }
    //else -- TBD: should we at least log an error when read() is called without a valid socket?

    // process the next level
    snap_pipe_connection::process_read();
}


/** \brief Write as much data as we can to the pipe.
 *
 * This function writes the data that was cached in our f_output
 * buffer to the pipe, as much as possible, then it returns.
 *
 * The is_writer() function takes care of returning true if more
 * data is present in the f_output buffer and thus the process_write()
 * needs to be called again.
 */
void snap_communicator::snap_pipe_buffer_connection::process_write()
{
    if(get_socket() != -1)
    {
        errno = 0;
        ssize_t const r(snap_pipe_connection::write(&f_output[f_position], f_output.size() - f_position));
        if(r > 0)
        {
            // some data was written
            f_position += r;
            if(f_position >= f_output.size())
            {
                f_output.clear();
                f_position = 0;
            }
        }
        else if(r != 0 && errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // connection is considered bad, get rid of it
            //
            int const e(errno);
            SNAP_LOG_ERROR("an error occurred while writing to socket of \"")(f_name)("\" (errno: ")(e)(" -- ")(strerror(e))(").");
            process_error();
            return;
        }
    }
    //else -- TBD: should we generate an error when the socket is not valid?

    // process next level too
    snap_pipe_connection::process_write();
}


/** \brief The process received a hanged up pipe.
 *
 * The pipe on the other end was closed, somehow.
 */
void snap_communicator::snap_pipe_buffer_connection::process_hup()
{
    close();

    snap_pipe_connection::process_hup();
}






//////////////////////////////////
// Snap Pipe Message Connection //
//////////////////////////////////



/** \brief Send a message.
 *
 * This function sends a message to the process on the other side
 * of this pipe connection.
 *
 * \param[in] message  The message to be sent.
 */
void snap_communicator::snap_pipe_message_connection::send_message(snap_communicator_message const & message)
{
    // transform the message to a string and write to the socket
    // the writing is asynchronous so the message is saved in a cache
    // and transferred only later when the run() loop is hit again
    //
    QString const msg(message.to_message());
    QByteArray const utf8(msg.toUtf8());
    std::string buf(utf8.data(), utf8.size());
    buf += "\n";
    write(buf.c_str(), buf.length());
}


/** \brief Process a line (string) just received.
 *
 * The function parses the line as a message (snap_communicator_message)
 * and then calls the process_message() function if the line was valid.
 *
 * \param[in] line  The line of text that was just read.
 */
void snap_communicator::snap_pipe_message_connection::process_line(QString const & line)
{
    if(line.isEmpty())
    {
        return;
    }

    snap_communicator_message message;
    if(message.from_message(line))
    {
        process_message(message);
    }
    else
    {
        // TODO: what to do here? This could be that the version changed
        //       and the messages are not compatible anymore.
        //
        SNAP_LOG_ERROR("snap_communicator::snap_pipe_message_connection::process_line() was asked to process an invalid message (")(line)(")");
    }
}







////////////////////////////////
// Snap TCP Client Connection //
////////////////////////////////


/** \brief Initializes the client connection.
 *
 * This function creates a connection using the address, port, and mode
 * parameters. This is very similar to using the bio_client class to
 * create a connection, only the resulting connection can be used with
 * the snap_communicator object.
 *
 * \note
 * The function also saves the remote address and port used to open
 * the connection which can later be retrieved using the
 * get_remote_address() function. That address will remain valid
 * even after the socket is closed.
 *
 * \todo
 * If the remote address is an IPv6, we need to put it between [...]
 * (i.e. [::1]:4040) so we can extract the port safely.
 *
 * \param[in] communicator  The snap communicator controlling this connection.
 * \param[in] addr  The address of the server to connect to.
 * \param[in] port  The port to connect to.
 * \param[in] mode  Type of connection: plain or secure.
 */
snap_communicator::snap_tcp_client_connection::snap_tcp_client_connection(std::string const & addr, int port, mode_t mode)
    : bio_client(addr, port, mode)
    , f_remote_address(QString("%1:%2").arg(get_client_addr().c_str()).arg(get_client_port()))
{
}


/** \brief Retrieve the remote address information.
 *
 * This function can be used to retrieve the remove address and port
 * information as was specified on the constructor. These can be used
 * to find this specific connection at a later time or create another
 * connection.
 *
 * For example, you may get 192.168.2.17:4040.
 *
 * The function works even after the socket gets closed as we save
 * the remote address and port in a string just after the connection
 * was established.
 *
 * \note
 * These parameters are the same as what was passed to the constructor,
 * only both will have been converted to numbers. So for example when
 * you used "localhost", here you get "::1" or "127.0.0.1" for the
 * address.
 *
 * \return The remote host address and connection port.
 */
QString const & snap_communicator::snap_tcp_client_connection::get_remote_address() const
{
    return f_remote_address;
}


/** \brief Read from the client socket.
 *
 * This function reads data from the client socket and copy it in
 * \p buf. A maximum of \p count bytes are read.
 *
 * \param[in,out] buf  The buffer where the data is read.
 * \param[in] count  The maximum number of bytes to read.
 *
 * \return -1 if an error occurs, zero if no data gets read, a positive
 *         number representing the number of bytes read otherwise.
 */
ssize_t snap_communicator::snap_tcp_client_connection::read(void * buf, size_t count)
{
    if(get_socket() == -1)
    {
        errno = EBADF;
        return -1;
    }
    return bio_client::read(reinterpret_cast<char *>(buf), count);
}


/** \brief Write to the client socket.
 *
 * This function writes \p count bytes from \p buf to this
 * client socket.
 *
 * The function can safely be called after the socket was closed, although
 * it will return -1 and set errno to EBADF in tha case.
 *
 * \param[in] buf  The buffer to write to the client connection socket.
 * \param[in] count  The maximum number of bytes to write on this connection.
 *
 * \return -1 if an error occurs, zero if nothing was written, a positive
 *         number representing the number of bytes successfully written.
 */
ssize_t snap_communicator::snap_tcp_client_connection::write(void const * buf, size_t count)
{
    if(get_socket() == -1)
    {
        errno = EBADF;
        return -1;
    }
    return bio_client::write(reinterpret_cast<char const *>(buf), count);
}


/** \brief Check whether this connection is a reader.
 *
 * We change the default to true since TCP sockets are generally
 * always readers. You can still overload this function and
 * return false if necessary.
 *
 * However, we do not overload the is_writer() because that is
 * much more dynamic (i.e. you do not want to advertise as
 * being a writer unless you have data to write to the
 * socket.)
 *
 * \return The events to listen to for this connection.
 */
bool snap_communicator::snap_tcp_client_connection::is_reader() const
{
    return true;
}


/** \brief Retrieve the socket of this client connection.
 *
 * This function retrieves the socket this client connection. In this case
 * the socket is defined in the bio_client class.
 *
 * \return The socket of this client connection.
 */
int snap_communicator::snap_tcp_client_connection::get_socket() const
{
    return bio_client::get_socket();
}






////////////////////////////////
// Snap TCP Buffer Connection //
////////////////////////////////

/** \brief Initialize a client socket.
 *
 * The client socket gets initialized with the specified 'socket'
 * parameter.
 *
 * This constructor creates a writer connection too. This gives you
 * a read/write connection. You can get the writer with the writer()
 * function. So you may write data with:
 *
 * \code
 *      my_reader.writer().write(buf, buf_size);
 * \endcode
 *
 * \param[in] addr  The address to connect to.
 * \param[in] port  The port to connect to.
 * \param[in] mode  The mode to connect as (PLAIN or SECURE).
 * \param[in] blocking  If true, keep a blocking socket, other non-blocking.
 */
snap_communicator::snap_tcp_client_buffer_connection::snap_tcp_client_buffer_connection(std::string const & addr, int const port, mode_t const mode, bool const blocking)
    : snap_tcp_client_connection(addr, port, mode)
{
    if(!blocking)
    {
        non_blocking();
    }
}


/** \brief Write data to the connection.
 *
 * This function can be used to send data to this TCP/IP connection.
 * The data is bufferized and as soon as the connection can WRITE
 * to the socket, it will wake up and send the data. In other words,
 * we cannot just sleep and wait for an answer. The transfer will
 * be asynchroneous.
 *
 * \todo
 * Optimization: look into writing the \p data buffer directly in
 * the socket if the f_output cache is empty. If that works then
 * we can completely bypass our intermediate cache. This works only
 * if we make sure that the socket is non-blocking, though.
 *
 * \todo
 * Determine whether we may end up with really large buffers that
 * grow for a long time. This function only inserts and the
 * process_signal() function only reads some of the bytes but it
 * does not reduce the size of the buffer until all the data was
 * sent.
 *
 * \param[in] data  The pointer to the buffer of data to be sent.
 * \param[out] length  The number of bytes to send.
 *
 * \return The number of bytes that were saved in our buffer, 0 if
 *         no data was written to the buffer (i.e. the socket is
 *         closed, length is zero, or data is a null pointer.)
 */
ssize_t snap_communicator::snap_tcp_client_buffer_connection::write(void const * data, size_t length)
{
    if(get_socket() == -1)
    {
        errno = EBADF;
        return -1;
    }

    if(data != nullptr && length > 0)
    {
        char const * d(reinterpret_cast<char const *>(data));
        f_output.insert(f_output.end(), d, d + length);
        return length;
    }

    return 0;
}


/** \brief The buffer is a writer when the output buffer is not empty.
 *
 * This function returns true as long as the output buffer of this
 * client connection is not empty.
 *
 * \return true if the output buffer is not empty, false otherwise.
 */
bool snap_communicator::snap_tcp_client_buffer_connection::is_writer() const
{
    return get_socket() != -1 && !f_output.empty();
}


/** \brief Instantiation of process_read().
 *
 * This function reads incoming data from a socket.
 *
 * The function is what manages our low level TCP/IP connection protocol
 * which is to read one line of data (i.e. bytes up to the next '\n'
 * character; note that '\r' are not understood.)
 *
 * Once a complete line of data was read, it is converted to UTF-8 and
 * sent to the next layer using the process_line() function passing
 * the line it just read (without the '\n') to that callback.
 *
 * \sa process_write()
 * \sa process_line()
 */
void snap_communicator::snap_tcp_client_buffer_connection::process_read()
{
    // we read one character at a time until we get a '\n'
    // since we have a non-blocking socket we can read as
    // much as possible and then check for a '\n' and keep
    // any extra data in a cache.
    //
    if(get_socket() != -1)
    {
        std::vector<char> buffer;
        buffer.resize(1024);
        for(;;)
        {
            errno = 0;
            ssize_t const r(read(&buffer[0], buffer.size()));
            if(r > 0)
            {
                for(ssize_t position(0); position < r; )
                {
                    std::vector<char>::const_iterator it(std::find(buffer.begin() + position, buffer.begin() + r, '\n'));
                    if(it == buffer.begin() + r)
                    {
                        // no newline, just add the whole thing
                        f_line += std::string(&buffer[position], r - position);
                        break; // do not waste time, we know we are done
                    }

                    // retrieve the characters up to the newline
                    // character and process the line
                    //
                    f_line += std::string(&buffer[position], it - buffer.begin() - position);
                    process_line(QString::fromUtf8(f_line.c_str()));

                    // done with that line
                    f_line.clear();

                    // we had a newline, we may still have some data
                    // in that buffer; (+1 to skip the '\n' itself)
                    //
                    position = it - buffer.begin() + 1;
                }
            }
            else if(r == 0 || errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // no more data available at this time
                break;
            }
            else //if(r < 0)
            {
                // TODO: do something about the error
                int const e(errno);
                SNAP_LOG_ERROR("an error occurred while reading from socket (errno: ")(e)(" -- ")(strerror(e))(").");
                process_error();
                return;
            }
        }
    }

    // process next level too
    snap_tcp_client_connection::process_read();
}


/** \brief Instantiation of process_write().
 *
 * This function writes outgoing data to a socket.
 *
 * This function manages our own internal cache, which we use to allow
 * for out of synchronization (non-blocking) output.
 *
 * \sa write()
 * \sa process_read()
 */
void snap_communicator::snap_tcp_client_buffer_connection::process_write()
{
    if(get_socket() != -1)
    {
        errno = 0;
        ssize_t const r(snap_tcp_client_connection::write(&f_output[f_position], f_output.size() - f_position));
        if(r > 0)
        {
            // some data was written
            f_position += r;
            if(f_position >= f_output.size())
            {
                f_output.clear();
                f_position = 0;
            }
        }
        else if(r < 0 && errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // connection is considered bad, generate an error
            //
            int const e(errno);
            SNAP_LOG_ERROR("an error occurred while writing to socket of \"")(f_name)("\" (errno: ")(e)(" -- ")(strerror(e))(").");
            process_error();
            return;
        }
    }

    // process next level too
    snap_tcp_client_connection::process_write();
}


/** \brief The hang up event occurred.
 *
 * This function closes the socket and then calls the previous level
 * hang up code which removes this connection from the snap_communicator
 * object it was last added in.
 */
void snap_communicator::snap_tcp_client_buffer_connection::process_hup()
{
    // this connection is dead...
    //
    close();

    // process next level too
    snap_tcp_client_connection::process_hup();
}


/** \fn snap_communicator::snap_tcp_client_buffer_connection::process_line(QString const & line);
 * \brief Process a line of data.
 *
 * This is the default virtual class that can be overridden to implement
 * your own processing. By default this function does nothing.
 *
 * \note
 * At this point I implemented this function so one can instantiate
 * a snap_tcp_server_client_buffer_connection without having to
 * derive it, although I do not think that is 100% proper.
 *
 * \param[in] line  The line of data that was just read from the input
 *                  socket.
 */





///////////////////////////////////////////////
// Snap TCP Server Message Buffer Connection //
///////////////////////////////////////////////

/** \brief Initializes a client to read messages from a socket.
 *
 * This implementation creates a message in/out client.
 * This is the most useful client in our Snap! Communicator
 * as it directly sends and receives messages.
 *
 * \param[in] addr  The address to connect to.
 * \param[in] port  The port to connect to.
 * \param[in] mode  Use this mode to connect as (PLAIN or SECURE).
 * \param[in] blocking  Whether to keep the socket blocking or make it
 *                      non-blocking.
 */
snap_communicator::snap_tcp_client_message_connection::snap_tcp_client_message_connection(std::string const & addr, int const port, mode_t const mode, bool const blocking)
    : snap_tcp_client_buffer_connection(addr, port, mode, blocking)
{
}


/** \brief Process a line (string) just received.
 *
 * The function parses the line as a message (snap_communicator_message)
 * and then calls the process_message() function if the line was valid.
 *
 * \param[in] line  The line of text that was just read.
 */
void snap_communicator::snap_tcp_client_message_connection::process_line(QString const & line)
{
    if(line.isEmpty())
    {
        return;
    }

    snap_communicator_message message;
    if(message.from_message(line))
    {
        process_message(message);
    }
    else
    {
        // TODO: what to do here? This could be that the version changed
        //       and the messages are not compatible anymore.
        //
        SNAP_LOG_ERROR("snap_communicator::snap_tcp_server_client_message_reader_connection::process_line() was asked to process an invalid message (")(line)(")");
    }
}


/** \brief Send a message.
 *
 * This function sends a message to the client on the other side
 * of this connection.
 *
 * \param[in] message  The message to be sent.
 */
void snap_communicator::snap_tcp_client_message_connection::send_message(snap_communicator_message const & message)
{
    // transform the message to a string and write to the socket
    // the writing is asynchronous so the message is saved in a cache
    // and transferred only later when the run() loop is hit again
    //
    QString const msg(message.to_message());
    QByteArray const utf8(msg.toUtf8());
    std::string buf(utf8.data(), utf8.size());
    buf += "\n";
    write(buf.c_str(), buf.length());
}








////////////////////////////////
// Snap TCP Server Connection //
////////////////////////////////


/** \brief Initialize a server connection.
 *
 * This function is used to initialize a server connection, a TCP/IP
 * listener which can accept() new connections.
 *
 * \param[in] communicator  The snap communicator controlling this connection.
 * \param[in] addr  The address to listen on. It may be set to "0.0.0.0".
 * \param[in] port  The port to listen on.
 * \param[in] max_connections  The number of connections to keep in the listen queue.
 * \param[in] reuse_addr  Whether to mark the socket with the SO_REUSEADDR flag.
 * \param[in] auto_close  Automatically close the client socket in accept and the destructor.
 */
snap_communicator::snap_tcp_server_connection::snap_tcp_server_connection(std::string const & addr, int port, int max_connections, bool reuse_addr, bool auto_close)
    : tcp_server(addr, port, max_connections, reuse_addr, auto_close)
{
}


/** \brief Reimplement the is_listener() for the snap_tcp_server_connection.
 *
 * A server connection is a listener socket. The library makes
 * use of a completely different callback when a "read" event occurs
 * on these connections.
 *
 * The callback is expected to create the new connection and add
 * it the communicator.
 *
 * \return This version of the function always returns true.
 */
bool snap_communicator::snap_tcp_server_connection::is_listener() const
{
    return true;
}


/** \brief Retrieve the socket of this server connection.
 *
 * This function retrieves the socket this server connection. In this case
 * the socket is defined in the tcp_server class.
 *
 * \return The socket of this client connection.
 */
int snap_communicator::snap_tcp_server_connection::get_socket() const
{
    return tcp_server::get_socket();
}







///////////////////////////////////////
// Snap TCP Server Client Connection //
///////////////////////////////////////


/** \brief Create a client connection created from an accept().
 *
 * This constructor initializes a client connection from a socket
 * that we received from an accept() call.
 *
 * The destructor will automatically close that socket on destruction.
 *
 * \param[in] communicator  The snap communicator controlling this connection.
 * \param[in] socket  The socket that acecpt() returned.
 */
snap_communicator::snap_tcp_server_client_connection::snap_tcp_server_client_connection(int socket)
    : f_socket(socket < 0 ? -1 : socket)
{
}


/** \brief Make sure the socket gets released once we are done witht he connection.
 *
 * This destructor makes sure that the socket gets closed.
 */
snap_communicator::snap_tcp_server_client_connection::~snap_tcp_server_client_connection()
{
    close();
}


/** \brief Read data from the TCP server client socket.
 *
 * This function reads as much data up to the specified amount
 * in \p count. The read data is saved in \p buf.
 *
 * \param[in,out] buf  The buffer where the data gets read.
 * \param[in] count  The maximum number of bytes to read in buf.
 *
 * \return The number of bytes read or -1 if an error occurred.
 */
ssize_t snap_communicator::snap_tcp_server_client_connection::read(void * buf, size_t count)
{
    if(f_socket == -1)
    {
        errno = EBADF;
        return -1;
    }
    return ::read(f_socket, buf, count);
}


/** \brief Write data to this connection's socket.
 *
 * This function writes up to \p count bytes of data from \p buf
 * to this connection's socket.
 *
 * \warning
 * This write function may not always write all the data you are
 * trying to send to the remote connection. If you want to make
 * sure that all your data is written to the other connection,
 * you want to instead use the snap_tcp_server_client_buffer_connection
 * which overloads the write() function and saves the data to be
 * written to the socket in a buffer. The snap communicator run
 * loop is then responsible for sending all the data.
 *
 * \param[in] buf  The buffer of data to be written to the socket.
 * \param[in] count  The number of bytes the caller wants to write to the
 *                   conneciton.
 *
 * \return The number of bytes written to the socket or -1 if an error occurred.
 */
ssize_t snap_communicator::snap_tcp_server_client_connection::write(void const * buf, size_t count)
{
    if(f_socket == -1)
    {
        errno = EBADF;
        return -1;
    }
    return ::write(f_socket, buf, count);
}


/** \brief Close the socket of this connection.
 *
 * This function is automatically called whenever the object gets
 * destroyed (see destructor) or detects that the client closed
 * the network connection.
 *
 * Connections cannot be reopened.
 */
void snap_communicator::snap_tcp_server_client_connection::close()
{
    if(f_socket != -1)
    {
        if(::close(f_socket) != 0)
        {
            int const e(errno);
            SNAP_LOG_ERROR("closing socket generated error: ")(e);
        }
        f_socket = -1;
    }
}


/** \brief Retrieve the socket of this connection.
 *
 * This function returns the socket defined in this connection.
 */
int snap_communicator::snap_tcp_server_client_connection::get_socket() const
{
    return f_socket;
}


/** \brief Tell that we are always a reader.
 *
 * This function always returns true meaning that the connection is
 * always of a reader. In most cases this is safe because if nothing
 * is being written to you then poll() never returns so you do not
 * waste much time in have a TCP connection always marked as a
 * reader.
 *
 * \return The events to listen to for this connection.
 */
bool snap_communicator::snap_tcp_server_client_connection::is_reader() const
{
    return true;
}


/** \brief Retrieve a copy of the client's address.
 *
 * This function makes a copy of the address of this client connection
 * to the \p address parameter and returns the length.
 *
 * If the function returns zero, then the \p address buffer is not
 * modified and no address is defined in this connection.
 *
 * \param[out] address  The reference to an address variable where the
 *                      client's address gets copied.
 *
 * \return Return the length of the address which may be smaller than
 *         sizeof(address). If zero, then no address is defined.
 *
 * \sa get_addr()
 */
size_t snap_communicator::snap_tcp_server_client_connection::get_client_address(struct sockaddr_storage & address) const
{
    // make sure the address is defined and the socket open
    //
    if(const_cast<snap_communicator::snap_tcp_server_client_connection *>(this)->define_address() != 0)
    {
        return 0;
    }

    address = f_address;
    return f_length;
}


/** \brief Retrieve the address in the form of a string.
 *
 * Like the get_addr() of the tcp client and server classes, this
 * function returns the address in the form of a string which can
 * easily be used to log information and other similar tasks.
 *
 * \return The client's address in the form of a string.
 */
std::string snap_communicator::snap_tcp_server_client_connection::get_client_addr() const
{
    // make sure the address is defined and the socket open
    //
    if(!const_cast<snap_communicator::snap_tcp_server_client_connection *>(this)->define_address())
    {
        return std::string();
    }

    size_t const max_length(std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1);

// in release mode this should not be dynamic (although the syntax is so
// the warning would happen), but in debug it is likely an alloca()
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
    char buf[max_length];
#pragma GCC diagnostic pop

    char const * r(nullptr);

    if(f_address.ss_family == AF_INET)
    {
        r = inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in const &>(f_address).sin_addr, buf, max_length);
    }
    else
    {
        r = inet_ntop(AF_INET6, &reinterpret_cast<struct sockaddr_in6 const &>(f_address).sin6_addr, buf, max_length);
    }

    if(r == nullptr)
    {
        int const e(errno);
        SNAP_LOG_FATAL("inet_ntop() could not convert IP address (errno: ")(e)(" -- ")(strerror(e))(").");
        throw snap_communicator_runtime_error("snap_tcp_server_client_connection::get_addr(): inet_ntop() could not convert IP address properly.");
    }

    return buf;
}


/** \brief Retrieve the port.
 *
 * This function returns the port of the socket on our side.
 *
 * If the port is not available (not connected?), then -1 is returned.
 *
 * \return The client's port in host order.
 */
int snap_communicator::snap_tcp_server_client_connection::get_client_port() const
{
    // make sure the address is defined and the socket open
    //
    if(!const_cast<snap_communicator::snap_tcp_server_client_connection *>(this)->define_address())
    {
        return -1;
    }

    if(f_address.ss_family == AF_INET)
    {
        return ntohs(reinterpret_cast<struct sockaddr_in const &>(f_address).sin_port);
    }
    else
    {
        return ntohs(reinterpret_cast<struct sockaddr_in6 const &>(f_address).sin6_port);
    }
}


/** \brief Retrieve the address in the form of a string.
 *
 * Like the get_addr() of the tcp client and server classes, this
 * function returns the address in the form of a string which can
 * easily be used to log information and other similar tasks.
 *
 * \return The client's address in the form of a string.
 */
std::string snap_communicator::snap_tcp_server_client_connection::get_client_addr_port() const
{
    // get the current address and port
    std::string const addr(get_client_addr());
    int const port(get_client_port());

    // make sure they are defined
    if(addr.empty()
    || port < 0)
    {
        return std::string();
    }

    // calculate the result
    std::stringstream buf;
    if(f_address.ss_family == AF_INET)
    {
        buf << addr << ":" << port;
    }
    else
    {
        buf << "[" << addr << "]:" << port;
    }
    return buf.str();
}


/** \brief Retrieve the socket address if we have not done so yet.
 *
 * This function make sure that the f_address and f_length parameters are
 * defined. This is done by calling the getsockname() function.
 *
 * If f_length is still zero, then it is expected that address was not
 * yet read.
 *
 * Note that the function returns -1 if the socket is now -1 (i.e. the
 * connection is closed) whether or not the function worked before.
 *
 * \return false if the address cannot be defined, true otherwise
 */
bool snap_communicator::snap_tcp_server_client_connection::define_address()
{
    int const s(get_socket());
    if(s == -1)
    {
        return false;
    }

    if(f_length == 0)
    {
        // address not defined yet, retrieve with with getsockname()
        //
        f_length = sizeof(f_address);
        if(getsockname(s, reinterpret_cast<struct sockaddr *>(&f_address), &f_length) != 0)
        {
            int const e(errno);
            SNAP_LOG_ERROR("getsockname() failed retrieving IP address (errno: ")(e)(" -- ")(strerror(e))(").");
            f_length = 0;
            return false;
        }
        if(f_address.ss_family != AF_INET
        && f_address.ss_family != AF_INET6)
        {
            SNAP_LOG_ERROR("address family (")(f_address.ss_family)(") returned by getsockname() is not understood, it is neither an IPv4 nor IPv6.");
            f_length = 0;
            return false;
        }
        if(f_length < sizeof(f_address))
        {
            // reset the rest of the structure, just in case
            memset(reinterpret_cast<char *>(&f_address) + f_length, 0, sizeof(f_address) - f_length);
        }
    }

    return true;
}






////////////////////////////////
// Snap TCP Buffer Connection //
////////////////////////////////

/** \brief Initialize a client socket.
 *
 * The client socket gets initialized with the specified 'socket'
 * parameter.
 *
 * If you are a pure client (opposed to a client that was just accepted)
 * you may want to consider using the snap_tcp_client_buffer_connection
 * instead. That gives you a way to open the socket from a set of address
 * and port definitions among other things.
 *
 * This initialization, so things work as expected in our environment,
 * the function marks the socket as non-blocking. This is important for
 * the reader and writer capabilities.
 *
 * \param[in] communicator  The snap communicator controlling this connection.
 * \param[in] socket  The socket to be used for reading and writing.
 */
snap_communicator::snap_tcp_server_client_buffer_connection::snap_tcp_server_client_buffer_connection(int socket)
    : snap_tcp_server_client_connection(socket)
{
    non_blocking();
}


/** \brief Tells that this connection is a writer when we have data to write.
 *
 * This function checks to know whether there is data to be writen to
 * this connection socket. If so then the function returns true. Otherwise
 * it just returns false.
 *
 * This happens whenever you called the write() function and our cache
 * is not empty yet.
 *
 * \return true if there is data to write to the socket, false otherwise.
 */
bool snap_communicator::snap_tcp_server_client_buffer_connection::is_writer() const
{
    return get_socket() != -1 && !f_output.empty();
}


/** \brief Write data to the connection.
 *
 * This function can be used to send data to this TCP/IP connection.
 * The data is bufferized and as soon as the connection can WRITE
 * to the socket, it will wake up and send the data. In other words,
 * we cannot just sleep and wait for an answer. The transfer will
 * be asynchroneous.
 *
 * \todo
 * Determine whether we may end up with really large buffers that
 * grow for a long time. This function only inserts and the
 * process_signal() function only reads some of the bytes but it
 * does not reduce the size of the buffer until all the data was
 * sent.
 *
 * \param[in] data  The pointer to the buffer of data to be sent.
 * \param[out] length  The number of bytes to send.
 */
ssize_t snap_communicator::snap_tcp_server_client_buffer_connection::write(void const * data, size_t length)
{
    if(get_socket() == -1)
    {
        errno = EBADF;
        return -1;
    }

    if(data != nullptr && length > 0)
    {
        char const * d(reinterpret_cast<char const *>(data));
        f_output.insert(f_output.end(), d, d + length);
        return length;
    }

    return 0;
}


/** \brief Read and process as much data as possible.
 *
 * This function reads as much incoming data as possible and processes
 * it.
 *
 * If the input includes a newline character ('\n') then this function
 * calls the process_line() callback which can further process that
 * line of data.
 *
 * \todo
 * Look into a way, if possible, to have a single instantiation since
 * as far as I know this code matches the one written in the
 * process_read() of the snap_tcp_client_buffer_connection and
 * the snap_pipe_buffer_connection classes.
 */
void snap_communicator::snap_tcp_server_client_buffer_connection::process_read()
{
    // we read one character at a time until we get a '\n'
    // since we have a non-blocking socket we can read as
    // much as possible and then check for a '\n' and keep
    // any extra data in a cache.
    //
    if(get_socket() != -1)
    {
        std::vector<char> buffer;
        buffer.resize(1024);
        for(;;)
        {
            errno = 0;
            ssize_t const r(read(&buffer[0], buffer.size()));
            if(r > 0)
            {
                for(ssize_t position(0); position < r; )
                {
                    std::vector<char>::const_iterator it(std::find(buffer.begin() + position, buffer.begin() + r, '\n'));
                    if(it == buffer.begin() + r)
                    {
                        // no newline, just add the whole thing
                        f_line += std::string(&buffer[position], r - position);
                        break; // do not waste time, we know we are done
                    }

                    // retrieve the characters up to the newline
                    // character and process the line
                    //
                    f_line += std::string(&buffer[position], it - buffer.begin() - position);
                    process_line(QString::fromUtf8(f_line.c_str()));

                    // done with that line
                    f_line.clear();

                    // we had a newline, we may still have some data
                    // in that buffer; (+1 to skip the '\n' itself)
                    //
                    position = it - buffer.begin() + 1;
                }
            }
            else if(r == 0 || errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // no more data available at this time
                break;
            }
            else //if(r < 0)
            {
                int const e(errno);
                SNAP_LOG_ERROR("an error occurred while reading from socket (errno: ")(e)(" -- ")(strerror(e))(").");
                process_error();
                return;
            }
        }
    }

    // process next level too
    snap_tcp_server_client_connection::process_read();
}


/** \brief Write to the connection's socket.
 *
 * This function implementation writes as much data as possible to the
 * connection's socket.
 */
void snap_communicator::snap_tcp_server_client_buffer_connection::process_write()
{
    if(get_socket() != -1)
    {
        errno = 0;
        ssize_t const r(snap_tcp_server_client_connection::write(&f_output[f_position], f_output.size() - f_position));
        if(r > 0)
        {
            // some data was written
            f_position += r;
            if(f_position >= f_output.size())
            {
                f_output.clear();
                f_position = 0;
            }
        }
        else if(r != 0 && errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // connection is considered bad, get rid of it
            //
            int const e(errno);
            SNAP_LOG_ERROR("an error occurred while writing to socket of \"")(f_name)("\" (errno: ")(e)(" -- ")(strerror(e))(").");
            process_error();
            return;
        }
    }

    // process next level too
    snap_tcp_server_client_connection::process_write();
}


/** \brief The remote used hanged up.
 *
 * This function makes sure that the connection gets closed properly.
 */
void snap_communicator::snap_tcp_server_client_buffer_connection::process_hup()
{
    // this connection is dead...
    //
    close();

    snap_tcp_server_client_connection::process_hup();
}







////////////////////////////////////////
// Snap TCP Server Message Connection //
////////////////////////////////////////

/** \brief Initializes a client to read messages from a socket.
 *
 * This implementation creates a message in/out client.
 * This is the most useful client in our Snap! Communicator
 * as it directly sends and receives messages.
 *
 * \param[in] communicator  The communicator connected with this client.
 * \param[in] socket  The in/out socket.
 */
snap_communicator::snap_tcp_server_client_message_connection::snap_tcp_server_client_message_connection(int socket)
    : snap_tcp_server_client_buffer_connection(socket)
{
    // TODO: somehow the port seems wrong (i.e. all connections return the same port)

    struct sockaddr_storage address = (sockaddr_storage());
    socklen_t length(sizeof(address));
    if(getpeername(socket, reinterpret_cast<struct sockaddr *>(&address), &length) != 0)
    {
        int const e(errno);
        SNAP_LOG_ERROR("getpeername() failed retrieving IP address (errno: ")(e)(" -- ")(strerror(e))(").");
        throw std::runtime_error("getpeername() failed to retrieve IP address in snap_communicator::snap_tcp_server_client_message_connection::snap_tcp_server_client_message_connection()");
    }
    if(address.ss_family != AF_INET
    && address.ss_family != AF_INET6)
    {
        SNAP_LOG_ERROR("address family (")(address.ss_family)(") returned by getpeername() is not understood, it is neither an IPv4 nor IPv6.");
        throw std::runtime_error("getpeername() returned an address which is not understood in snap_communicator::snap_tcp_server_client_message_connection::snap_tcp_server_client_message_connection()");
    }
    if(length < sizeof(address))
    {
        // reset the rest of the structure, just in case
        memset(reinterpret_cast<char *>(&address) + length, 0, sizeof(address) - length);
    }

    size_t const max_length(std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1);

// in release mode this should not be dynamic (although the syntax is so
// the warning would happen), but in debug it is likely an alloca()
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
    char buf[max_length];
#pragma GCC diagnostic pop

    char const * r(nullptr);

    if(address.ss_family == AF_INET)
    {
        r = inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in const &>(address).sin_addr, buf, max_length);
    }
    else
    {
        r = inet_ntop(AF_INET6, &reinterpret_cast<struct sockaddr_in6 const &>(address).sin6_addr, buf, max_length);
    }

    if(r == nullptr)
    {
        int const e(errno);
        SNAP_LOG_FATAL("inet_ntop() could not convert IP address (errno: ")(e)(" -- ")(strerror(e))(").");
        throw snap_communicator_runtime_error("snap_tcp_server_client_message_connection::snap_tcp_server_client_message_connection(): inet_ntop() could not convert IP address properly.");
    }

    if(address.ss_family == AF_INET)
    {
        f_remote_address = QString("%1:%2").arg(buf).arg(ntohs(reinterpret_cast<struct sockaddr_in const &>(address).sin_port));
    }
    else
    {
        f_remote_address = QString("[%1]:%2").arg(buf).arg(ntohs(reinterpret_cast<struct sockaddr_in6 const &>(address).sin6_port));
    }
}


/** \brief Process a line (string) just received.
 *
 * The function parses the line as a message (snap_communicator_message)
 * and then calls the process_message() function if the line was valid.
 *
 * \param[in] line  The line of text that was just read.
 */
void snap_communicator::snap_tcp_server_client_message_connection::process_line(QString const & line)
{
    // empty lines should not occur, but just in case, just ignore
    if(line.isEmpty())
    {
        return;
    }

    snap_communicator_message message;
    if(message.from_message(line))
    {
        process_message(message);
    }
    else
    {
        // TODO: what to do here? This could because the version changed
        //       and the messages are not compatible anymore.
        //
        SNAP_LOG_ERROR("snap_communicator::snap_tcp_server_client_message_connection::process_line() was asked to process an invalid message (")(line)(")");
    }
}


/** \brief Send a message.
 *
 * This function sends a message to the client on the other side
 * of this connection.
 *
 * \param[in] message  The message to be processed.
 */
void snap_communicator::snap_tcp_server_client_message_connection::send_message(snap_communicator_message const & message)
{
    // transform the message to a string and write to the socket
    // the writing is asynchronous so the message is saved in a cache
    // and transferred only later when the run() loop is hit again
    //
    QString const msg(message.to_message());
    QByteArray const utf8(msg.toUtf8());
    std::string buf(utf8.data(), utf8.size());
    buf += "\n";
    write(buf.c_str(), buf.length());
}


/** \brief Retrieve the remote address information.
 *
 * This function can be used to retrieve the remove address and port
 * information as was specified on the constructor. These can be used
 * to find this specific connection at a later time or create another
 * connection.
 *
 * For example, you may get 192.168.2.17:4040.
 *
 * The function works even after the socket gets closed as we save
 * the remote address and port in a string just after the connection
 * was established.
 *
 * \warning
 * This function returns BOTH: the address and the port.
 *
 * \note
 * These parameters are the same as what was passed to the constructor,
 * only both will have been converted to numbers. So for example when
 * you used "localhost", here you get "::1" or "127.0.0.1" for the
 * address.
 *
 * \return The remote host address and connection port.
 */
QString const & snap_communicator::snap_tcp_server_client_message_connection::get_remote_address() const
{
    return f_remote_address;
}




//////////////////////////////////////////////////
// Snap TCP Client Permanent Message Connection //
//////////////////////////////////////////////////

/** \brief Internal implementation of the snap_tcp_client_permanent_message_connection class.
 *
 * This class is used to handle a thread that will process a connection for
 * us. This allows us to connect in any amount of time required by the
 * Unix system to obtain the connection with the remote server.
 *
 * \todo
 * Having threads at the time we do a fork() is not safe. We may
 * want to reconsider offering this functionality here. Because at
 * this time we would have no control of when the thread is created
 * and thus a way to make sure that no such thread is running when
 * we call fork().
 */
class snap_tcp_client_permanent_message_connection_impl
{
public:
    class messenger
        : public snap_communicator::snap_tcp_server_client_message_connection
    {
    public:
        messenger(snap_communicator::snap_tcp_client_permanent_message_connection * client, int socket)
            : snap_tcp_server_client_message_connection(socket)
            , f_client(client)
        {
            set_name("snap_tcp_client_permanent_message_connection_impl::messenger");
        }

        // snap_connection implementation
        virtual void process_error()
        {
            snap_tcp_server_client_message_connection::process_error();
            f_client->process_error();
        }

        // snap_connection implementation
        virtual void process_hup()
        {
            snap_tcp_server_client_message_connection::process_hup();
            f_client->process_hup();
        }

        // snap_connection implementation
        virtual void process_invalid()
        {
            snap_tcp_server_client_message_connection::process_invalid();
            f_client->process_invalid();
        }

        // snap_tcp_server_client_message_connection implementation
        virtual void process_message(snap_communicator_message const & message)
        {
            f_client->process_message(message);
        }

    private:
        snap_communicator::snap_tcp_client_permanent_message_connection *  f_client;
    };

    class thread_done_signal
        : public snap_communicator::snap_thread_done_signal
    {
    public:
        typedef std::shared_ptr<thread_done_signal>   pointer_t;

        thread_done_signal(snap_tcp_client_permanent_message_connection_impl * client)
            : f_client(client)
        {
            set_name("snap_tcp_client_permanent_message_connection_impl::thread_done_signal");
        }

        /** \brief This signal was emitted.
         *
         * This function gets called whenever the thread is just about to
         * quit. Calling f_thread.is_running() may still return true when
         * you get in the 'thread_done()' callback. However, an
         * f_thread.stop() will return very quickly.
         */
        virtual void process_read()
        {
            snap_thread_done_signal::process_read();

            f_client->thread_done();
        }

    private:
        snap_tcp_client_permanent_message_connection_impl *  f_client;
    };

    class runner
        : public snap_thread::snap_runner
    {
    public:
        runner(snap_tcp_client_permanent_message_connection_impl * client_impl, std::string const & address, int port, tcp_client_server::bio_client::mode_t mode)
            : snap_runner("background snap_tcp_client_permanent_message_connection for asynchroneous connections")
            , f_client_impl(client_impl)
            , f_address(address)
            , f_port(port)
            , f_mode(mode)
            , f_socket(-1)
            //, f_last_error("") -- auto-init
        {
        }


        /** \brief This is the actual function run by the thread.
         *
         * This function calls the connect() function and then
         * tells the main thread we are done.
         */
        virtual void run()
        {
            connect();

            // tell the main thread that we are done
            //
            f_client_impl->trigger_thread_done();
        }


        /** \brief This function attempts to connect.
         *
         * This function attempts a connection to the specified address
         * and port with the specified mode (i.e. plain or encrypted.)
         *
         * The function may take a long time to succeed connecting with
         * the server. The main thread will be awaken whenever this
         * thread dies.
         *
         * If an error occurs, then the f_socket variable member will
         * be set to -1. Otherwise it represents the socket that we
         * just connected with.
         */
        void connect()
        {
            try
            {
                // create a socket using the bio_client class,
                // but then just create a duplicate that we will
                // use in a server-client TCP object (because
                // we cannot directly create the right type of
                // object otherwise...)
                //
                f_tcp_connection.reset(new tcp_client_server::bio_client(f_address, f_port, f_mode));

                // we make a copy of the socket because we may
                // be using a thread and the variable has to be
                // atomic, which f_socket is
                //
                f_socket = f_tcp_connection->get_socket();
            }
            catch(tcp_client_server::tcp_client_server_runtime_error const & e)
            {
                // connection failed... we will have to try again later
                //
                // WARNING: our logger is not multi-thread safe
                //SNAP_LOG_ERROR("connection to ")(f_address)(":")(f_port)(" failed with: ")(e.what());
                f_last_error = e.what();
                f_socket = -1;
            }
        }


        /** \brief Retrieve the address to connect to.
         *
         * This function returns the address passed in on creation.
         *
         * \note
         * Since the variable is constant, it is likely to never change.
         * However, the c_str() function may change the buffer pointer.
         * Hence, to be 100% safe, you cannot call this function until
         * you make sure that the thread is fully stopped.
         */
        std::string const & get_address() const
        {
            return f_address;
        }


        /** \brief Retrieve the port to connect to.
         *
         * This function returns the port passed in on creation.
         *
         * \note
         * Since the variable is constant, it never gets changed
         * which means it is always safe to use it between
         * both threads.
         */
        int get_port() const
        {
            return f_port;
        }


        /** \brief Retrieve the socket allocated and connected by the thread.
         *
         * This functio returns the socket resulting from connection
         * attempt of the background thread.
         *
         * If the socket is -1, then you may get the error message
         * using the get_last_error() function.
         *
         * \note
         * The socket is an atomic variable so it is safe to check it
         * between threads without any additional memory barrier.
         *
         * \return The connection socket.
         */
        int get_socket() const
        {
            return f_socket;
        }


        /** \brief Retrieve the last error message that happened.
         *
         * This function returns the last error message that was captured
         * when trying to connect to the socket. The message is the
         * e.what() message from the exception we captured.
         *
         * The message does not get cleared so the function can be called
         * any number of times. To know whether an error was generated
         * on the last attempt, make sure to first get the get_socket()
         * and if it returns -1, then this message is significant,
         * otherwise it is from a previous error.
         *
         * \warning
         * Remember that if the background thread was used the error will
         * NOT be available in the main thread until a full memory barrier
         * was executed. For that reason we make sure that the thread
         * was stopped when we detect an error.
         *
         * \return The last error message.
         */
        std::string const & get_last_error() const
        {
            return f_last_error;
        }


        /** \brief Close the connection.
         *
         * This function closes the connection. Since the f_tcp_connection
         * holds the socket to the remote server, we have get this function
         * called in order to completely disconnect.
         *
         * \note
         * This function does not clear the f_last_error parameter so it
         * can be read later.
         */
        void close()
        {
            f_tcp_connection.reset();
            f_socket = -1;
        }


    private:
        snap_tcp_client_permanent_message_connection_impl * f_client_impl;
        std::string const                                   f_address;
        int const                                           f_port;
        tcp_client_server::bio_client::mode_t const         f_mode;
        tcp_client_server::bio_client::pointer_t            f_tcp_connection;
        std::atomic<int>                                    f_socket; // = -1 -- must be done in constructor
        std::string                                         f_last_error;
    };


    /** \brief Initialize a permanent message connection implementation object.
     *
     * This object manages the thread used to asynchronically connect to
     * the specified address and port.
     *
     * This class and its sub-classes may end up executing callbacks
     * of the snap_tcp_client_permanent_message_connection object.
     * However, in all cases these are never run from the thread.
     *
     * \param[in] client  A pointer to the owner of this
     *            snap_tcp_client_permanent_message_connection_impl object.
     * \param[in] address  The address we are to connect to.
     * \param[in] port  The port we are to connect to.
     * \param[in] mode  The mode used to connect.
     */
    snap_tcp_client_permanent_message_connection_impl(snap_communicator::snap_tcp_client_permanent_message_connection * client, std::string const & address, int port, tcp_client_server::bio_client::mode_t mode)
        : f_client(client)
        //, f_thread_done() -- auto-init
        , f_thread_runner(this, address, port, mode)
        , f_thread("background connection handler thread", &f_thread_runner)
        //, f_messager(nullptr) -- auto-init
        //, f_message_cache() -- auto-init
    {
    }


    /** \brief Destroy the permanent message connection.
     *
     * This function makes sure that the messenger was lost.
     */
    ~snap_tcp_client_permanent_message_connection_impl()
    {
        // to make sure we can lose the messenger, first we want to be sure
        // that we do not have a thread running
        //
        f_thread.stop();

        // although the message variable is deleted, it would not get
        // removed from the snap communicator if we were not doing
        // it explicitly
        //
        lose_messager();
    }


    /** \brief Direct connect to the messenger.
     *
     * In this case we try to connect without the thread. This allows
     * us to avoid the thread problems, but we are blocked until the
     * OS decides to time out or the connection worked.
     */
    void connect()
    {
        // call the thread connect() function from the main thread
        //
        f_thread_runner.connect();

        // simulate receiving the thread_done() signal
        //
        thread_done();
    }


    /** \brief Check whether the permanent connection is currently connected.
     *
     * This function returns true if the messenger exists, which means that
     * the connection is up.
     *
     * \return true if the connection is up.
     */
    bool is_connected()
    {
        return !!f_messager;
    }


    /** \brief Try to start the thread runner.
     *
     * This function tries to start the thread runner in order to initiate
     * a connection in the background. If the thread could not be started,
     * then the function returns false.
     *
     * If the thread started, then the function returns true. This does
     * not mean that the connection was obtained. This is known once
     * the process_connected() function is called.
     *
     * \return true if the thread was successfully started.
     */
    bool background_connect()
    {
        if(f_thread.is_running())
        {
            SNAP_LOG_ERROR("A background connection attempt is already in progress. Further requests are ignored.");
            return false;
        }

        // create the f_thread_done only when required
        //
        if(!f_thread_done)
        {
            f_thread_done.reset(new thread_done_signal(this));
        }

        snap_communicator::instance()->add_connection(f_thread_done);

        if(!f_thread.start())
        {
            SNAP_LOG_ERROR("The thread used to run the background connection process did not start.");
            return false;
        }

        return true;
    }


    /** \brief Tell the main thread that the background thread is done.
     *
     * This function is called by the thread so the thread_done()
     * function of the thread done object gets called. Only the
     * thread should call this function.
     *
     * As a result the thread_done() function of this class will be
     * called by the main thread.
     */
    void trigger_thread_done()
    {
        f_thread_done->thread_done();
    }


    /** \brief Signal that the background thread is done.
     *
     * This callback is called whenever the background thread sends
     * a signal to us. This is used to avoid calling end user functions
     * that would certainly cause a lot of problem if called from the
     * thread.
     *
     * The function calls the process_connection_failed() if the
     * connection did not happen.
     *
     * The function calls the process_connected() if the connection
     * did happen.
     *
     * \note
     * This is used only if the user requested that the connection
     * happen in the background (i.e. use_thread was set to true
     * in the snap_tcp_client_permanent_message_connection object
     * constructor.)
     */
    void thread_done()
    {
        // if we used the thread we have to remove the signal used
        // to know that the thread was done
        //
        snap_communicator::instance()->remove_connection(f_thread_done);

        if(f_thread_runner.get_socket() == -1)
        {
            // we will access the f_last_error member of the thread runner
            // which may not be available to the main thread yet, calling
            // stop forces a memory barrier so we are all good.
            //
            f_thread.stop();

            // TODO: fix address in error message using a snap::addr so
            //       as to handle IPv6 seemlessly.
            SNAP_LOG_ERROR("connection to ")(f_thread_runner.get_address())(":")(f_thread_runner.get_port())(" failed with: ")(f_thread_runner.get_last_error());

            // signal that an error occurred
            //
            f_client->process_connection_failed(f_thread_runner.get_last_error());
        }
        else
        {
            f_messager.reset(new messenger(f_client, f_thread_runner.get_socket()));

            // add the messenger to the communicator
            //
            snap_communicator::instance()->add_connection(f_messager);

            // if some messages were cached, process them immediately
            //
            while(!f_message_cache.empty())
            {
                f_messager->send_message(f_message_cache[0]);
                f_message_cache.erase(f_message_cache.begin());
            }

            // let the client know we are now connected
            //
            f_client->process_connected();
        }
    }

    /** \brief Send a message to the connection.
     *
     * This implementation function actually sends the message to the
     * connection, assuming that the connection exists. Otherwise, it
     * may cache the message (if cache is true.)
     *
     * \param[in] message  The message to send.
     * \param[in] cache  Whether to cache the message if the connection is
     *                   currently down.
     *
     * \return true if the message was forwarded, false if the message
     *         was ignored or cached.
     */
    bool send_message(snap_communicator_message const & message, bool cache)
    {
        if(f_messager)
        {
            f_messager->send_message(message);
            return true;
        }

        if(cache)
        {
            f_message_cache.push_back(message);
        }

        return false;
    }


    /** \brief Forget about the messenger connect.
     *
     * This function resets the f_messager pointer as an error occurred
     * on that connection.
     *
     * \note
     * This is safe, even though it is called from the messenger itself
     * because it will not get deleted yet. This is because the run()
     * loop has a copy in its own temporary copy of the vector of
     * connections.
     */
    void lose_messager()
    {
        if(f_messager)
        {
            snap_communicator::instance()->remove_connection(f_messager);
            f_messager.reset();

            // just the messenger does not close the TCP connection because
            // we have a copy in the thread runner
            f_thread_runner.close();
        }
    }


    /** \brief Return the address and size of the remote computer.
     *
     * This function retrieve the socket address.
     *
     * \param[out] address  The binary address of the remote computer.
     *
     * \return The size of the sockaddr structure.
     */
    size_t get_client_address(struct sockaddr_storage & address) const
    {
        return f_messager->get_client_address(address);
    }


    /** \brief Return the address of the f_message object.
     *
     * This function returns the address of the message object.
     *
     * \return The address of the remote computer.
     */
    std::string get_client_addr() const
    {
        return f_messager->get_client_addr();
    }


private:
    snap_communicator::snap_tcp_client_permanent_message_connection *   f_client;
    thread_done_signal::pointer_t                                       f_thread_done;
    runner                                                              f_thread_runner;
    snap::snap_thread                                                   f_thread;
    messenger::pointer_t                                                f_messager;
    snap_communicator_message::vector_t                                 f_message_cache;
};


/** \brief Initializes this TCP client message connection.
 *
 * This implementation creates what we call a permanent connection.
 * Such a connection may fail once in a while. In such circumstances,
 * the class automatically requests for a reconnection (see various
 * parameters in the regard below.) However, this causes one issue:
 * by default, the connection just never ends. When you are about
 * ready to close the connection, you must call the mark_done()
 * function first. This will tell the various error functions to
 * drop this connection instead of restarting it after a small pause.
 *
 * This constructor makes sure to initialize the timer and saves
 * the address, port, mode, pause, and use_thread parameters.
 *
 * The timer is first set to trigger immediately. This means the TCP
 * connection will be attempted as soon as possible (the next time
 * the run() loop is entered, it will time out immediately.) You
 * are free to call set_timeout_date() with a date in the future if
 * you prefer that the connect be attempted a little later.
 *
 * The \p pause parameter is used if the connection is lost and this
 * timer is used again to attempt a new connection. It will be reused
 * as long as the connection fails (as a delay). It has to be at least
 * 10 microseconds, although really you should not use less than 1
 * second (1000000). You may set the pause parameter to 0 in which case
 * you are responsible to set the delay (by default there will be no
 * delay and thus the timer will never time out.)
 *
 * To start with a delay, instead of trying to connect immediately,
 * you may pass a negative pause parameter. So for example to get the
 * first attempt 5 seconds after you created this object, you use
 * -5000000LL as the pause parameter.
 *
 * The \p use_thread parameter determines whether the connection should
 * be attempted in a thread (asynchroneously) or immediately (which means
 * the timeout callback may block for a while.) If the connection is to
 * a local server with an IP address specified as numbers (i.e. 127.0.0.1),
 * the thread is probably not required. For connections to a remote
 * computer, though, it certainly is important.
 *
 * \param[in] address  The address to listen on. It may be set to "0.0.0.0".
 * \param[in] port  The port to listen on.
 * \param[in] mode  The mode to use to open the connection.
 * \param[in] pause  The amount of time to wait before attempting a new
 *                   connection after a failure, in microseconds, or 0.
 * \param[in] use_thread  Whether a thread is used to connect to the
 *                        server.
 */
snap_communicator::snap_tcp_client_permanent_message_connection::snap_tcp_client_permanent_message_connection(std::string const & address, int port, tcp_client_server::bio_client::mode_t mode, int64_t const pause, bool const use_thread)
    : snap_timer(pause < 0 ? -pause : 0)
    , f_impl(new snap_tcp_client_permanent_message_connection_impl(this, address, port, mode))
    , f_pause(llabs(pause))
    , f_use_thread(use_thread)
{
}


/** \brief Attempt to send a message to this connection.
 *
 * If the connection is currently enabled, the message is sent immediately.
 * Otherwise, it may be cached if the \p cache parameter is set to true.
 * A cached message is forwarded as soon as a new successful connection
 * happens, which can be a problem if messages need to happen in a very
 * specific order (For example, after a reconnection to snapcommunicator
 * you first need to REGISTER or CONNECT...)
 *
 * \param[in] message  The message to send to the connected server.
 * \param[in] cache  Whether the message should be cached.
 *
 * \return true if the message was sent, false if it was not sent, although
 *         if cache was true, it was cached
 */
bool snap_communicator::snap_tcp_client_permanent_message_connection::send_message(snap_communicator_message const & message, bool cache)
{
    return f_impl->send_message(message, cache);
}


/** \brief Check whether the connection is up.
 *
 * This function returns true if the connection is considered to be up.
 * This means sending messages will work quickly instead of being
 * cached up until an actual TCP/IP connection gets established.
 *
 * Note that the connection may have hanged up since, and the system
 * may not have yet detected the fact (i.e. the connection is going
 * to receive the process_hup() call after the event in which you are
 * working.)
 *
 * \return true if connected
 */
bool snap_communicator::snap_tcp_client_permanent_message_connection::is_connected() const
{
    return f_impl->is_connected();
}


/** \brief Call once you are done with a permanent connection.
 *
 * This function lets the permanent connection know that you are done with
 * it. It is very important to call this function before you send the last
 * message to the server. For example, the snapbackend tool will do:
 *
 * \code
 *      f_messager->mark_done();
 *      f_messager->send_message(stop_message);
 * \endcode
 *
 * That way the next HUP error is properly interpreted as "we are done".
 * Otherwise, a HUP is interpreted as a lost connection and since this
 * is a permanent connection, it simply restarts the process and reconnects
 * to the server.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::mark_done()
{
    f_done = true;
}


/** \brief Retrieve a copy of the client's address.
 *
 * This function makes a copy of the address of this client connection
 * to the \p address parameter and returns the length.
 *
 * \param[in] address  The reference to an address variable where the
 *                     address gets copied.
 *
 * \return Return the length of the address which may be smaller than
 *         sizeof(struct sockaddr). If zero, then no address is defined.
 *
 * \sa get_addr()
 */
size_t snap_communicator::snap_tcp_client_permanent_message_connection::get_client_address(struct sockaddr_storage & address) const
{
    return f_impl->get_client_address(address);
}


/** \brief Retrieve the remote computer address as a string.
 *
 * This function returns the address of the remote computer as a string.
 * It will be a canonicalized IP address.
 *
 * \return The cacnonicalized IP address.
 */
std::string snap_communicator::snap_tcp_client_permanent_message_connection::get_client_addr() const
{
    return f_impl->get_client_addr();
}


/** \brief Internal timeout callback implementation.
 *
 * This callback implements the guts of this class: it attempts to connect
 * to the specified address and port, optionally after creating a thread
 * so the attempt can happen asynchroneously.
 *
 * When the connection fails, the timer is used to try again pause
 * microseconds later (pause as specified in the constructor).
 *
 * When a connection succeeds, the timer is disabled until you detect
 * an error while using the connection and re-enable the timer.
 *
 * \warning
 * This function changes the timeout delay to the pause amount
 * as defined with the constructor. If you want to change that
 * amount, you can do so an any point after this function call
 * using the set_timeout_delay() function. If the pause parameter
 * was set to -1, then the timeout never gets changed.
 * However, you should not use a permanent message timer as your
 * own or you will interfer with the internal use of the timer.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::process_timeout()
{
    // change the timeout delay although we will not use it immediately
    // if we start the thread or attempt an immediate connection, but
    // that way the user can change it by calling set_timeout_delay()
    // at any time after the first process_timeout() call
    //
    if(f_pause > 0)
    {
        set_timeout_delay(f_pause);
        f_pause = 0;
    }

    if(f_use_thread)
    {
        // in this case we create a thread, run it and know whether the
        // connection succeeded only when the thread tells us it did
        //
        // TODO: the background_connect() may return false in two situations:
        //       1) when the thread is already running and then the behavior
        //          we have below is INCORRECT
        //       2) when the thread cannot be started (i.e. could not
        //          allocate the stack?) in which case the if() below
        //          is the correct behavior
        //
        if(f_impl->background_connect())
        {
            // we started the thread successfully, so block the timer
            //
            set_enable(false);
        }
    }
    else
    {
        // the success is noted when we receive a call to
        // process_connected(); there we do set_enable(false)
        // so the timer stops
        //
        f_impl->connect();
    }
}


/** \brief Process an error.
 *
 * When an error occurs, we restart the timer so we can attempt to reconnect
 * to that server.
 *
 * If you overload this function, make sure to either call this
 * implementation or enable the timer yourselves.
 *
 * \warning
 * This function does not call the snap_timer::process_error() function
 * which means that this connection is not automatically removed from
 * the snapcommunicator object on failures.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::process_error()
{
    if(f_done)
    {
        snap_timer::process_error();
    }
    else
    {
        f_impl->lose_messager();
        set_enable(true);
    }
}


/** \brief Process a hang up.
 *
 * When a hang up occurs, we restart the timer so we can attempt to reconnect
 * to that server.
 *
 * If you overload this function, make sure to either call this
 * implementation or enable the timer yourselves.
 *
 * \warning
 * This function does not call the snap_timer::process_hup() function
 * which means that this connection is not automatically removed from
 * the snapcommunicator object on failures.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::process_hup()
{
    if(f_done)
    {
        snap_timer::process_hup();
    }
    else
    {
        f_impl->lose_messager();
        set_enable(true);
    }
}


/** \brief Process an invalid signal.
 *
 * When an invalid signal occurs, we restart the timer so we can attempt
 * to reconnect to that server.
 *
 * If you overload this function, make sure to either call this
 * implementation or enable the timer yourselves.
 *
 * \warning
 * This function does not call the snap_timer::process_invalid() function
 * which means that this connection is not automatically removed from
 * the snapcommunicator object on failures.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::process_invalid()
{
    if(f_done)
    {
        snap_timer::process_invalid();
    }
    else
    {
        f_impl->lose_messager();
        set_enable(true);
    }
}


/** \brief Make sure that the messenger connection gets removed.
 *
 * This function makes sure that the messenger sub-connection also gets
 * removed from the snap communicator. Otherwise it would lock the system
 * since connections are saved in the snap communicator object as shared
 * pointers.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::connection_removed()
{
    f_impl->lose_messager();
}


/** \brief Process a connection failed callback.
 *
 * When a connection attempt fails, we restart the timer so we can
 * attempt to reconnect to that server.
 *
 * If you overload this function, make sure to either call this
 * implementation or enable the timer yourselves.
 *
 * \param[in] error_message  The error message that trigged this callback.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::process_connection_failed(std::string const & error_message)
{
    NOTUSED(error_message);
    set_enable(true);
}


/** \brief The connection is ready.
 *
 * This callback gets called whenever the connection succeeded and is
 * ready to be used.
 *
 * You should implement this virtual function if you have to initiate
 * the communication. For example, the snapserver has to send a
 * REGISTER to the snapcommunicator system and thus implements this
 * function.
 *
 * The default implementation makes sure that the timer gets turned off
 * so we do not try to reconnect every minute or so.
 */
void snap_communicator::snap_tcp_client_permanent_message_connection::process_connected()
{
    set_enable(false);
}




////////////////////////////////
// Snap UDP Server Connection //
////////////////////////////////


/** \brief Initialize a UDP listener.
 *
 * This function is used to initialize a server connection, a UDP/IP
 * listener which wakes up whenever a send() is sent to this listener
 * address and port.
 *
 * \param[in] communicator  The snap communicator controlling this connection.
 * \param[in] addr  The address to listen on. It may be set to "0.0.0.0".
 * \param[in] port  The port to listen on.
 */
snap_communicator::snap_udp_server_connection::snap_udp_server_connection(std::string const & addr, int port)
    : udp_server(addr, port)
{
}


/** \brief Retrieve the socket of this server connection.
 *
 * This function retrieves the socket this server connection. In this case
 * the socket is defined in the udp_server class.
 *
 * \return The socket of this client connection.
 */
int snap_communicator::snap_udp_server_connection::get_socket() const
{
    return udp_server::get_socket();
}


/** \brief Check to know whether this UDP connection is a reader.
 *
 * This function returns true to say that this UDP connection is
 * indeed a reader.
 *
 * \return This function already returns true as we are likely to
 *         always want a UDP socket to be listening for incoming
 *         packets.
 */
bool snap_communicator::snap_udp_server_connection::is_reader() const
{
    return true;
}



////////////////////////////////
// Snap UDP Server Connection //
////////////////////////////////


/** \brief Initialize a UDP server to send and receive messages.
 *
 * This function initialises a UDP server as a Snap UDP server
 * connection attached to the specified address and port.
 *
 * It is expected to be used to send and receive UDP messages.
 *
 * Note that to send messages, you need the address and port
 * of the destination. In effect, we do not use this server
 * when sending. Instead we create a client that we immediately
 * destruct once the message was sent.
 *
 * \param[in] addr  The address to listen on.
 * \param[in] port  The port to listen on.
 */
snap_communicator::snap_udp_server_message_connection::snap_udp_server_message_connection(std::string const & addr, int port)
    : snap_udp_server_connection(addr, port)
{
    // allow for looping over all the messages in one go
    //
    non_blocking();
}


/** \brief Send a UDP message.
 *
 * This function offers you to send a UDP message to the specified
 * address and port. The message should be small enough to fit in
 * on UDP packet or the call will fail.
 *
 * \note
 * The function return true when the message was successfully sent.
 * This does not mean it was received.
 *
 * \param[in] addr  The destination address for the message.
 * \param[in] port  The destination port for the message.
 * \param[in] message  The message to send to the destination.
 *
 * \return true when the message was sent, false otherwise.
 */
bool snap_communicator::snap_udp_server_message_connection::send_message(std::string const & addr, int port, snap_communicator_message const & message)
{
    // Note: contrary to the TCP version, a UDP message does not
    //       need to include the '\n' character since it is sent
    //       in one UDP packet. However, it has a maximum size
    //       limit which we enforce here.
    //
    udp_client_server::udp_client client(addr, port);
    QString const msg(message.to_message());
    QByteArray const utf8(msg.toUtf8());
    if(static_cast<size_t>(utf8.size()) > DATAGRAM_MAX_SIZE)
    {
        // packet too large for our buffers
        throw snap_communicator_invalid_message("message too large for a UDP server");
    }
    if(client.send(utf8.data(), utf8.size()) != utf8.size()) // we do not send the '\0'
    {
        SNAP_LOG_ERROR("snap_udp_server_message_connection::send_message(): could not send UDP message.");
        return false;
    }

    return true;
}


/** \brief Implementation of the process_read() callback.
 *
 * This function reads the datagram we just received using the
 * recv() function. The size of the datagram cannot be more than
 * DATAGRAM_MAX_SIZE (1Kb at time of writing.)
 *
 * The message is then parsed and further processing is expected
 * to be accomplished in your implementation of process_message().
 *
 * The function actually reads as many pending datagrams as it can.
 */
void snap_communicator::snap_udp_server_message_connection::process_read()
{
    char buf[DATAGRAM_MAX_SIZE];
    for(;;)
    {
        ssize_t const r(recv(buf, sizeof(buf) / sizeof(buf[0]) - 1));
        if(r <= 0)
        {
            break;
        }
        buf[r] = '\0';
        QString const udp_message(QString::fromUtf8(buf));
        snap::snap_communicator_message message;
        if(message.from_message(udp_message))
        {
            // we received a valid message, process it
            process_message(message);
        }
        else
        {
            SNAP_LOG_ERROR("snap_communicator::snap_udp_server_message_connection::process_read() was asked to process an invalid message (")(udp_message)(")");
        }
    }
}




/////////////////////////////////////////////////
// Snap TCP Blocking Client Message Connection //
/////////////////////////////////////////////////

/** \brief Blocking client message connection.
 *
 * This object allows you to create a blocking, generally temporary
 * one message connection client. This is specifically used with
 * the snaplock daemon, but it can be used for other things too as
 * required.
 *
 * The connection is expected to be used as shown in the following
 * example which is how it is used to implement the LOCK through
 * our snaplock daemons.
 *
 * \code
 *      class my_blocking_connection
 *          : public snap_tcp_blocking_client_message_connection
 *      {
 *      public:
 *          my_blocking_connection(std::string const & addr, int port, mode_t mode)
 *              : snap_tcp_blocking_client_message_connection(addr, port, mode)
 *          {
 *              // need to register with snap communicator
 *              snap_communicator_message register_message;
 *              register_message.set_command("REGISTER");
 *              ...
 *              blocking_connection.send_message(register_message);
 *
 *              run();
 *          }
 *
 *          ~my_blocking_connection()
 *          {
 *              // done, send UNLOCK and then make sure to unregister
 *              snap_communicator_message unlock_message;
 *              unlock_message.set_command("UNLOCK");
 *              ...
 *              blocking_connection.send_message(unlock_message);
 *
 *              snap_communicator_message unregister_message;
 *              unregister_message.set_command("UNREGISTER");
 *              ...
 *              blocking_connection.send_message(unregister_message);
 *          }
 *
 *          virtual void process_message(snap_communicator_message const & message)
 *          {
 *              QString const command(message.get_command());
 *              if(command == "LOCKED")
 *              {
 *                  // the lock worked, release hand back to the user
 *                  done();
 *              }
 *              else if(command == "READY")
 *              {
 *                  // the REGISTER worked, wait for the HELP message
 *              }
 *              else if(command == "HELP")
 *              {
 *                  // snapcommunicator wants us to tell it what commands
 *                  // we accept
 *                  snap_communicator_message commands_message;
 *                  commands_message.set_command("COMMANDS");
 *                  ...
 *                  blocking_connection.send_message(commands_message);
 *
 *                  // no reply expected from the COMMANDS message,
 *                  // so send the LOCK now
 *                  snap_communicator_message lock_message;
 *                  lock_message.set_command("LOCK");
 *                  ...
 *                  blocking_connection.send_message(lock_message);
 *              }
 *          }
 *      };
 *      my_blocking_connection blocking_connection("127.0.0.1", 4040);
 *
 *
 *      // then we can send a message to the service we are interested in
 *      blocking_connection.send_message(my_message);
 *
 *      // now we call run() waiting for a reply
 *      blocking_connection.run();
 * \endcode
 */
snap_communicator::snap_tcp_blocking_client_message_connection::snap_tcp_blocking_client_message_connection(std::string const & addr, int const port, mode_t const mode)
    : snap_tcp_client_message_connection(addr, port, mode, true)
    //, f_done(false)
{
}


/** \brief Blocking run on the connection.
 *
 * This function reads the incoming messages and calls process_message()
 * on each one of them, in a blocking manner.
 *
 * If you called done() before, the done flag is reset back to false.
 * You will have to call done() again if you receive a message that
 * is expected to process and that message marks the end of the process.
 */
void snap_communicator::snap_tcp_blocking_client_message_connection::run()
{
    f_done = false;

    do
    {
        std::string line;
        for(;;)
        {
            // TBD: can the socket become -1 within the read() loop?
            //      (i.e. should not that be just ourside of the for(;;)?)
            struct pollfd fd;
            fd.events = POLLIN | POLLPRI | POLLRDHUP;
            fd.fd = get_socket();
            if(fd.fd < 0
            || !is_enabled())
            {
                // invalid socket
                process_error();
                return;
            }

            // at this time, this class is used with the lock and
            // the lock has a timeout so we need to block at most
            // for that amount of time and not forever (presumably
            // the snaplock would send us a LOCKFAILED marked with
            // a "timeout" parameter, but we cannot rely on the
            // snaplock being there and responding as expected.)
            //
            // calculate the number of microseconds and then convert
            // them to milliseconds for poll()
            //
            int64_t const next_timeout_timestamp(save_timeout_timestamp());
            int64_t const now(get_current_date());
            int64_t const timeout((next_timeout_timestamp - now) / 1000);
            if(timeout <= 0)
            {
                // timed out
                //
                process_timeout();
                if(f_done)
                {
                    return;
                }
                SNAP_LOG_FATAL("snap_communicator::snap_tcp_blocking_client_message_connection::run(): connection timed out before we could get the lock.");
                throw snap_communicator_runtime_error("connection timed out");
            }
            errno = 0;
            fd.revents = 0; // probably useless... (kernel should clear those)
            int const r(::poll(&fd, 1, timeout));
            if(r < 0)
            {
                // r < 0 means an error occurred
                //
                if(errno == EINTR)
                {
                    // Note: if the user wants to prevent this error, he should
                    //       use the snap_signal with the Unix signals that may
                    //       happen while calling poll().
                    //
                    throw snap_communicator_runtime_error("EINTR occurred while in poll() -- interrupts are not supported yet though");
                }
                if(errno == EFAULT)
                {
                    throw snap_communicator_parameter_error("buffer was moved out of our address space?");
                }
                if(errno == EINVAL)
                {
                    // if this is really because nfds is too large then it may be
                    // a "soft" error that can be fixed; that being said, my
                    // current version is 16K files which frankly when we reach
                    // that level we have a problem...
                    //
                    struct rlimit rl;
                    getrlimit(RLIMIT_NOFILE, &rl);
                    throw snap_communicator_parameter_error(QString("too many file fds for poll, limit is currently %1, your kernel top limit is %2")
                                .arg(rl.rlim_cur)
                                .arg(rl.rlim_max).toStdString());
                }
                if(errno == ENOMEM)
                {
                    throw snap_communicator_runtime_error("poll() failed because of memory");
                }
                int const e(errno);
                throw snap_communicator_runtime_error(QString("poll() failed with error %1").arg(e).toStdString());
            }

            if((fd.revents & (POLLIN | POLLPRI)) != 0)
            {
                // read one character at a time otherwise we would be
                // blocked forever
                //
                char buf[2];
                int const size(::read(fd.fd, buf, 1));
                if(size != 1)
                {
                    // invalid read
                    process_error();
                    throw snap_communicator_runtime_error(QString("read() failed reading data from socket (return value = %1)").arg(size).toStdString());
                }
                if(buf[0] == '\n')
                {
                    // end of a line, we got a whole message in our buffer
                    // notice that we do not add the '\n' to line
                    break;
                }
                buf[1] = '\0';
                line += buf;
            }
            if((fd.revents & POLLERR) != 0)
            {
                process_error();
                throw snap_communicator_runtime_error(QString("poll() failed with an error").toStdString());
            }
            if((fd.revents & (POLLHUP | POLLRDHUP)) != 0)
            {
                process_hup();
                throw snap_communicator_runtime_error(QString("poll() failed with hang up").toStdString());
            }
            if((fd.revents & POLLNVAL) != 0)
            {
                process_invalid();
                throw snap_communicator_runtime_error(QString("poll() says the socket is invalid").toStdString());
            }
        }
        process_line(QString::fromUtf8(line.c_str()));
    }
    while(!f_done);
}


/** \brief Mark the run() loop as done.
 *
 * This function can be used to ask the run() loop to exit after
 * you return from your process_message() callback. This will
 * break the run loop.
 */
void snap_communicator::snap_tcp_blocking_client_message_connection::done()
{
    f_done = true;
}


/** \brief Send the specified message to the connection on the other end.
 *
 * This function sends the specified message to the other side of the
 * socket connection. If the write somehow fails, then the function
 * returns false.
 *
 * The function blocks until the entire message was written to the
 * socket.
 *
 * \param[in] message  The message to send to the connection.
 *
 * \return true if the message was sent succesfully, false otherwise.
 */
bool snap_communicator::snap_tcp_blocking_client_message_connection::send_message(snap_communicator_message const & message)
{
    int const s(get_socket());
    if(s >= 0)
    {
        // transform the message to a string and write to the socket
        // the writing is blocking and thus fully synchronous so the
        // function blocks until the message gets fully sent
        //
        // WARNING: we cannot use f_connection.write() because that one
        //          is asynchronous (at least, it writes to a buffer
        //          and not directly to the socket!)
        //
        QString const msg(message.to_message());
        QByteArray const utf8(msg.toUtf8());
        std::string buf(utf8.data(), utf8.size());
        buf += "\n";
        return ::write(s, buf.c_str(), buf.length()) == static_cast<ssize_t>(buf.length());
    }

    return false;
}


/** \brief Overridden callback.
 *
 * This function is overriding the lower level process_error() to make
 * (mostly) sure that the remove_from_communicator() function does not
 * get called because that would generate the creation of a
 * snap_communicator object which we do not want with blocking
 * clients.
 */
void snap_communicator::snap_tcp_blocking_client_message_connection::process_error()
{
}









///////////////////////
// Snap Communicator //
///////////////////////


/** \brief Initialize a snap communicator object.
 *
 * This function initializes the snap_communicator object.
 */
snap_communicator::snap_communicator()
    //: f_connections() -- auto-init
    //, f_force_sort(true) -- auto-init
{
}


/** \brief Retrieve the instance() of the snap_communicator.
 *
 * This function returns the instance of the snap_communicator.
 * There is really no reason and it could also create all sorts
 * of problems to have more than one instance hence we created
 * the communicator as a singleton. It also means you cannot
 * actually delete the communicator.
 */
snap_communicator::pointer_t snap_communicator::instance()
{
    if(!g_instance)
    {
        g_instance.reset(new snap_communicator);
    }

    return g_instance;
}


/** \brief Retrieve a reference to the vector of connections.
 *
 * This function returns a reference to all the connections that are
 * currently attached to the snap_communicator system.
 *
 * This is useful to search the array.
 *
 * \return The vector of connections.
 */
snap_communicator::snap_connection::vector_t const & snap_communicator::get_connections() const
{
    return f_connections;
}


/** \brief Attach a connection to the communicator.
 *
 * This function attaches a connection to the communicator. This allows
 * us to execute code for that connection by having the process_signal()
 * function called.
 *
 * Connections are kept in the order in which they are added. This may
 * change the order in which connection callbacks are called. However,
 * events are received asynchronously so do not expect callbacks to be
 * called in any specific order.
 *
 * \note
 * A connection can only be added once to a snap_communicator object.
 * Also it cannot be shared between multiple communicator objects.
 *
 * \param[in] connection  The connection being added.
 *
 * \return true if the connection was added, false if the connection
 *         was already present in the communicator list of connections.
 */
bool snap_communicator::add_connection(snap_connection::pointer_t connection)
{
    if(!connection->valid_socket())
    {
        throw snap_communicator_parameter_error("snap_communicator::add_connection(): connection without a socket cannot be added to a snap_communicator object.");
    }

    auto const it(std::find(f_connections.begin(), f_connections.end(), connection));
    if(it != f_connections.end())
    {
        // already added, can be added only once but we allow multiple
        // calls (however, we do not count those calls, so first call
        // to the remove_connection() does remove it!)
        return false;
    }

    f_connections.push_back(connection);

    connection->connection_added();

    return true;
}


/** \brief Remove a connection from a snap_communicator object.
 *
 * This function removes a connection from this snap_communicator object.
 * Note that any one connection can only be added once.
 *
 * \param[in] connection  The connection to remove from this snap_communicator.
 *
 * \return true if the connection was removed, false if it was not found.
 */
bool snap_communicator::remove_connection(snap_connection::pointer_t connection)
{
    auto it(std::find(f_connections.begin(), f_connections.end(), connection));
    if(it == f_connections.end())
    {
        return false;
    }

    SNAP_LOG_TRACE("snap_communicator::remove_connection(): name=")(connection->get_name());
    f_connections.erase(it);

    connection->connection_removed();

    return true;
}


/** \brief Run until all connections are removed.
 *
 * This function "blocks" until all the events added to this
 * snap_communicator instance are removed. Until then, it
 * wakes up and run callback functions whenever an event occurs.
 *
 * In other words, you want to add_connection() before you call
 * this function otherwise the function returns immediately.
 *
 * Note that you can include timeout events so if you need to
 * run some code once in a while, you may just use a timeout
 * event and process your repetitive events that way.
 *
 * \return true if the loop exits because the list of connections is empty.
 */
bool snap_communicator::run()
{
    // the loop promises to exit once the even_base object has no
    // more connections attached to it
    //
    std::vector<bool> enabled;
    std::vector<struct pollfd> fds;
    f_force_sort = true;
    for(;;)
    {
        // any connections?
        if(f_connections.empty())
        {
            return true;
        }

        if(f_force_sort)
        {
            // sort the connections by priority
            //
            std::stable_sort(f_connections.begin(), f_connections.end(), snap_connection::compare);
            f_force_sort = false;
        }

        // make a copy because the callbacks may end up making
        // changes to the main list and we would have problems
        // with that here...
        //
        snap_connection::vector_t connections(f_connections);
        size_t max_connections(connections.size());

        // timeout is do not time out by default
        //
        int64_t next_timeout_timestamp(std::numeric_limits<int64_t>::max());

        // clear() is not supposed to delete the buffer of vectors
        enabled.clear();
        fds.clear();
        fds.reserve(max_connections); // avoid more than 1 allocation
        for(size_t idx(0); idx < max_connections; ++idx)
        {
            snap_connection::pointer_t c(connections[idx]);
            c->f_fds_position = -1;

            // is the connection enabled?
            enabled.push_back(c->is_enabled());
            if(!enabled[idx])
            {
                //SNAP_LOG_TRACE("snap_communicator::run(): connection '")(c->get_name())("' has been disabled, so ignored.");
                continue;
            }

            // check whether a timeout is defined in this connection
            //
            int64_t const timestamp(c->save_timeout_timestamp());
            if(timestamp != -1)
            {
                // the timeout event gives us a time when to tick
                //
                if(timestamp < next_timeout_timestamp)
                {
                    next_timeout_timestamp = timestamp;
                }
            }

            // is there any events to listen on?
            int e(0);
            if(c->is_listener() || c->is_signal())
            {
                e |= POLLIN;
            }
            if(c->is_reader())
            {
                e |= POLLIN | POLLPRI | POLLRDHUP;
            }
            if(c->is_writer())
            {
                e |= POLLOUT | POLLRDHUP;
            }
            if(e == 0)
            {
                // this should only happend on snap_timer objects
                //
                continue;
            }

            // do we have a currently valid socket (i.e. the connection
            // may have been closed or we may be handling a timer or
            // signal object)
            //
            if(c->get_socket() < 0)
            {
                continue;
            }

            // this is considered valid, add this connection to the list
            //
            // save the position since we may skip some entries...
            // (otherwise we would have to use -1 as the socket to
            // allow for such dead entries, but avoiding such entries
            // saves time)
            //
            c->f_fds_position = fds.size();

//std::cerr << "[" << getpid() << "]: *** still waiting on \"" << c->get_name() << "\".\n";
            struct pollfd fd;
            fd.fd = c->get_socket();
            fd.events = e;
            fd.revents = 0; // probably useless... (kernel should clear those)
            fds.push_back(fd);
        }

        // compute the right timeout
        int64_t timeout(-1);
        if(next_timeout_timestamp != std::numeric_limits<int64_t>::max())
        {
            int64_t const now(get_current_date());
            timeout = next_timeout_timestamp - now;
            if(timeout < 0)
            {
                // timeout is in the past so timeout immediately, but
                // still check for events if any
                timeout = 0;
            }
            else
            {
                // convert microseconds to milliseconds for poll()
                timeout /= 1000;
                if(timeout == 0)
                {
                    // less than one is a waste of time (CPU intenssive
                    // until the time is reached, we can be 1 ms off
                    // instead...)
                    timeout = 1;
                }
            }
        }
        else if(fds.empty())
        {
            SNAP_LOG_FATAL("snap_communicator::run(): nothing to poll() on. All connections are disabled? (Ignoring ")
                          (max_connections)(" and exiting the run() loop anyway.)");
            return false;
        }
//std::cerr << QString("%1: timeout %2 (next was: %3, current ~ %4)\n").arg(getpid()).arg(timeout).arg(next_timeout_timestamp).arg(get_current_date());
        //SNAP_LOG_TRACE("snap_communicator::run(): ")
            //(QString("timeout %2 (next was: %3, current ~ %4)").arg(timeout).arg(next_timeout_timestamp).arg(get_current_date()));

        // TODO: add support for ppoll() so we can support signals cleanly
        //       with nearly no additional work from us
        //
        errno = 0;
        int const r(poll(&fds[0], fds.size(), timeout));
        if(r >= 0)
        {
            // quick sanity check
            //
            if(static_cast<size_t>(r) > connections.size())
            {
                throw snap_communicator_runtime_error("poll() returned a number larger than the input");
            }
//std::cerr << getpid() << ": ------------------- new set of " << r << " events to handle\n";
            //SNAP_LOG_TRACE("snap_communicator::run(): ------------------- new set of ")(r)(" events to handle");

            // check each connection one by one for:
            //
            // 1) fds events, including signals
            // 2) timeouts
            //
            // and execute the corresponding callbacks
            //
            for(size_t idx(0); idx < connections.size(); ++idx)
            {
                snap_connection::pointer_t c(connections[idx]);

                // is the connection enabled?
                // TODO: check on whether we should save the enable
                //       flag from before and not use the current
                //       one (i.e. a callback could disable something
                //       that we otherwise would expect to run at least
                //       once...)
                //
                if(!enabled[idx])
                {
                    //SNAP_LOG_TRACE("snap_communicator::run(): in loop, connection '")(c->get_name())("' has been disabled, so ignored!");
                    continue;
                }

                // if we have a valid fds position then an event other
                // than a timeout occurred on that connection
                //
                if(c->f_fds_position >= 0)
                {
                    struct pollfd * fd(&fds[c->f_fds_position]);

                    // if any events were found by poll(), process them now
                    //
                    if(fd->revents != 0)
                    {
                        // an event happened on this one
                        //
                        if((fd->revents & (POLLIN | POLLPRI)) != 0)
                        {
                            // we consider that Unix signals have the greater priority
                            // and thus handle them first
                            //
                            if(c->is_signal())
                            {
                                snap_signal * ss(dynamic_cast<snap_signal *>(c.get()));
                                if(ss)
                                {
                                    ss->process();
                                }
                            }
                            else if(c->is_listener())
                            {
                                // a listener is a special case and we want
                                // to call process_accept() instead
                                //
                                c->process_accept();
                            }
                            else
                            {
                                c->process_read();
                            }
                        }
                        if((fd->revents & POLLOUT) != 0)
                        {
                            c->process_write();
                        }
                        if((fd->revents & POLLERR) != 0)
                        {
                            c->process_error();
                        }
                        if((fd->revents & (POLLHUP | POLLRDHUP)) != 0)
                        {
                            c->process_hup();
                        }
                        if((fd->revents & POLLNVAL) != 0)
                        {
                            c->process_invalid();
                        }
                    }
                }

                // now check whether we have a timeout on this connection
                //
                int64_t const timestamp(c->get_saved_timeout_timestamp());
                if(timestamp != -1)
                {
                    int64_t const now(get_current_date());
                    if(now >= timestamp)
                    {
/*SNAP_LOG_TRACE("snap_communicator::run(): connection = '")(c->get_name())
    ("', timestamp = ")(timestamp)
    (", now = ")(now)
    (", now >= timestamp --> ")(now >= timestamp);*/
                        // move the timeout as required first
                        // (because the callback may move it again)
                        //
                        c->calculate_next_tick();

                        // the timeout date needs to be reset if the tick
                        // happened for that date
                        //
                        if(now >= c->get_timeout_date())
                        {
                            c->set_timeout_date(-1);
                        }

                        // then run the callback
                        //
                        c->process_timeout();
                    }
                }
            }
        }
        else
        {
            // r < 0 means an error occurred
            //
            if(errno == EINTR)
            {
                // Note: if the user wants to prevent this error, he should
                //       use the snap_signal with the Unix signals that may
                //       happen while calling poll().
                //
                throw snap_communicator_runtime_error("EINTR occurred while in poll() -- interrupts are not supported yet though");
            }
            if(errno == EFAULT)
            {
                throw snap_communicator_parameter_error("buffer was moved out of our address space?");
            }
            if(errno == EINVAL)
            {
                // if this is really because nfds is too large then it may be
                // a "soft" error that can be fixed; that being said, my
                // current version is 16K files which frankly when we reach
                // that level we have a problem...
                //
                struct rlimit rl;
                getrlimit(RLIMIT_NOFILE, &rl);
                throw snap_communicator_parameter_error(QString("too many file fds for poll, limit is currently %1, your kernel top limit is %2")
                            .arg(rl.rlim_cur)
                            .arg(rl.rlim_max).toStdString());
            }
            if(errno == ENOMEM)
            {
                throw snap_communicator_runtime_error("poll() failed because of memory");
            }
            int const e(errno);
            throw snap_communicator_runtime_error(QString("poll() failed with error %1").arg(e).toStdString());
        }
    }
}





/** \brief Get the current date.
 *
 * This function retrieves the current date and time with a precision
 * to the microseconds.
 *
 * \todo
 * This is also defined in snap_child::get_current_date() so we should
 * unify that in some way...
 */
int64_t snap_communicator::get_current_date()
{
    struct timeval tv;
    if(gettimeofday(&tv, nullptr) != 0)
    {
        int const err(errno);
        SNAP_LOG_FATAL("gettimeofday() failed with errno: ")(err);
        throw std::runtime_error("gettimeofday() failed");
    }
    return static_cast<int64_t>(tv.tv_sec) * static_cast<int64_t>(1000000)
         + static_cast<int64_t>(tv.tv_usec);
}




} // namespace snap
// vim: ts=4 sw=4 et
