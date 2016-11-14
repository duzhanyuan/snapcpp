/*
 * Text:
 *      session.h
 *
 * Description:
 *      Handling of the connection to the Cassandra database via the cassandra-cpp-driver API.
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
#pragma once

#include <map>
#include <memory>
#include <string>

#include <QString>
#include <QByteArray>

#include <unistd.h>

namespace casswrapper
{


struct data;
typedef int64_t timeout_t;

class cluster;
class future;
class session;


class Session
        : public std::enable_shared_from_this<Session>
{
public:
    typedef std::shared_ptr<Session> pointer_t;

    static timeout_t const       DEFAULT_TIMEOUT = 12 * 1000; // 12s

    static pointer_t create();
    ~Session();

    void connect( const QString& host = "localhost", const int port = 9042, const bool use_ssl = true );
    void connect( const QStringList& host_list     , const int port = 9042, const bool use_ssl = true );
    void disconnect();
    bool isConnected() const;

    QString const& get_keys_path() const;
    void           set_keys_path( QString const& path );

    void           add_ssl_trusted_cert( const QString& cert     );
    void           add_ssl_cert_file   ( const QString& filename );

    cluster getCluster()    const;
    session getSession()    const;
    future  getConnection() const;

    timeout_t timeout() const;
    timeout_t setTimeout(timeout_t timeout_ms);

    uint32_t highWaterMark () const;
    uint32_t lowWaterMark  () const;
    void setHighWaterMark  ( uint32_t val );
    void setLowWaterMark   ( uint32_t val );

private:
    Session();

    void reset_ssl_keys();
    void add_ssl_keys();

    std::unique_ptr<data> f_data;
    //
    timeout_t f_timeout         = DEFAULT_TIMEOUT;                         // 12s
    uint32_t  f_high_water_mark = 65536;
    uint32_t  f_low_water_mark  = 0;
    QString   f_keys_path       = "/var/lib/snapwebsites/cassandra-keys/";
};


class request_timeout
{
public:
    typedef std::shared_ptr<request_timeout> pointer_t;

    request_timeout(Session::pointer_t session, timeout_t timeout_ms)
        : f_session(session)
        , f_old_timeout(f_session->setTimeout(timeout_ms))
    {
    }

    ~request_timeout()
    {
        f_session->setTimeout(f_old_timeout);
    }

private:
    Session::pointer_t    f_session;
    timeout_t             f_old_timeout;
};


}
// namespace casswrapper

// vim: ts=4 sw=4 et