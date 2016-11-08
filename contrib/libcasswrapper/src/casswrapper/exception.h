#pragma once

#include "casswrapper/casswrapper.h"

#include <stdexcept>
#include <string>

#include <QString>


namespace casswrapper
{


class exception_t : public std::runtime_error
{
public:
    exception_t( const QString&     what ) : std::runtime_error(qPrintable(what)) {}
    exception_t( const std::string& what ) : std::runtime_error(what.c_str())     {}
    exception_t( const char*        what ) : std::runtime_error(what)             {}
};


class cassandra_exception_t : public std::exception
{
public:
    cassandra_exception_t( const casswrapper::future& future, const QString& msg );

    uint32_t        getCode()    const { return f_code;    }
    QString const&  getError()   const { return f_error;   }
    QString const&  getErrMsg()  const { return f_errmsg;  }
    QString const&  getMessage() const { return f_message; }

    virtual const char* what() const throw() override;

private:
    uint32_t    f_code;
    QString     f_error;
    QString     f_errmsg;
    QString     f_message;
    std::string f_what;
};


}
// namespace casswrapper

// vim: ts=4 sw=4 et