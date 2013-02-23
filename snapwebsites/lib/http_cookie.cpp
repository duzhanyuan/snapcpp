// Snap Websites Server -- manage HTTP cookies to be sent to the browser
// Copyright (C) 2013  Made to Order Software Corp.
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

#include "http_cookie.h"
#include "snapwebsites.h"
#include <QtCassandra/QCassandra.h>

namespace snap
{


namespace
{

//     CHAR           = <any US-ASCII character (octets 0 - 127)>
//     token          = 1*<any CHAR except CTLs or separators>
//     separators     = "(" | ")" | "<" | ">" | "@"
//                    | "," | ";" | ":" | "\" | <">
//                    | "/" | "[" | "]" | "?" | "="
//                    | "{" | "}" | SP | HT
#define SNAP_HTTP_TOKEN_CHAR(base, c) (static_cast<uint32_t>(1)<<(((c)-(base))&0x1F))
uint32_t http_token[4] = {
	/* 00-1F */ 0x00000000,
	/* 20-3F */
		  SNAP_HTTP_TOKEN_CHAR(0x20, '!')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '#')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '$')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '%')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '&')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '\'')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '*')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '+')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '-')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '.')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '-')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '0')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '1')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '2')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '3')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '4')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '5')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '6')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '7')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '8')
		| SNAP_HTTP_TOKEN_CHAR(0x20, '9')
	,
	/* 40-5F */
		  SNAP_HTTP_TOKEN_CHAR(0x40, 'A')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'B')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'C')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'D')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'E')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'F')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'G')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'H')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'I')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'J')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'K')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'L')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'M')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'N')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'O')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'P')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'Q')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'R')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'S')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'T')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'U')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'V')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'W')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'X')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'Y')
		| SNAP_HTTP_TOKEN_CHAR(0x40, 'Z')
		| SNAP_HTTP_TOKEN_CHAR(0x40, '^')
		| SNAP_HTTP_TOKEN_CHAR(0x40, '_')
	,
	/* 60-7F */
		  SNAP_HTTP_TOKEN_CHAR(0x60, '`')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'a')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'b')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'c')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'd')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'e')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'f')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'g')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'h')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'i')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'j')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'k')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'l')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'm')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'n')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'o')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'p')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'q')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'r')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 's')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 't')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'u')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'v')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'w')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'x')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'y')
		| SNAP_HTTP_TOKEN_CHAR(0x60, 'z')
		| SNAP_HTTP_TOKEN_CHAR(0x60, '|')
		| SNAP_HTTP_TOKEN_CHAR(0x60, '~')
};
#undef SNAP_HTTP_TOKEN_CHAR


} // no name namespace


/** \brief Create an invalid cookie (no name).
 *
 * This function creates an invalid cookie. It is here because the QMap
 * implementation requires it. You should not use it otherwise since the
 * f_snap pointer will be set to NULL and is likely going to crash your
 * server.
 */
http_cookie::http_cookie()
	//: f_snap(NULL) -- auto-init
	//, f_name("") -- auto-init
	//, f_domain("") -- auto-init
	//, f_path("") -- auto-init
	//, f_expire() -- auto-init
	//, f_secure(false) -- auto-init
	//, f_http_only(false) -- auto-init
{
}


/** \brief Initializes the cookie.
 *
 * This function initializes the cookie. The default for any cookie
 * is to have the following parameters:
 *
 * \li The name as supplied here.
 * \li The cookie contents, empty by default.
 * \li The domain set to this website full domain unless the user defined a
 *     cookie domain as a site parameter (SNAP_NAME_CORE_COOKIE_DOMAIN).
 * \li A path set to "/".
 * \li No expiration date (i.e. cookie is for this session only, deleted when browser is closed)
 * \li Not secure
 * \li Not limited to HTTP
 *
 * Note that the name of the cookie is set when you create it. I cannot be
 * changed later.
 *
 * \note
 * The name of a cookie is case sensitive. In other words cookie "foo" and
 * cookie "Foo" can be cohexist (although it certainly should not be used!)
 *
 * \todo
 * If there is a redirect (i.e. we show website A but the user was
 * accessible website B,) then the default domain name will be wrong.
 * We should have a way to retrieve the primary domain name for this
 * purpose.
 *
 * \param[in] snap  The snap child creating this cookie.
 * \param[in] name  The name of the cookie.
 * \param[in] value  The value of the cookie. To set a binary value, use set_value() with a QByteArray instead.
 *
 * \sa set_value()
 * \sa set_domain()
 * \sa set_path()
 * \sa set_delete()
 * \sa set_session()
 * \sa set_expire()
 * \sa set_expire_in()
 * \sa set_secure()
 * \sa set_http_only()
 */
http_cookie::http_cookie(snap_child *snap, const QString& name, const QString& value)
	: f_snap(snap)
	, f_name(name)
	//, f_domain("") -- auto-init
	, f_path("/")
	//, f_expire() -- auto-init
	//, f_secure(false) -- auto-init
	//, f_http_only(false) -- auto-init
{
	// XXX make this check only in debug-like versions
	int max(f_name.length());
	if(max == 0)
	{
		throw std::runtime_error("the name of a cookie cannot be empty");
	}
	for(int i(0); i < max; ++i)
	{
		ushort c(f_name[i].unicode());
		if(c <= ' ' || c >= 127 || (http_token[c >> 5] & (1 << (c & 0x1F))) == 0)
		{
			throw std::runtime_error("the name of a cookie must only include token compatible characters");
		}
	}
	if(f_name[0] == '$')
	{
		throw std::runtime_error("cookie name cannot start with '$'; those are reserved by the HTTP protocol");
	}

	QtCassandra::QCassandraValue cookie_domain(f_snap->get_site_parameter(snap::get_name(SNAP_NAME_CORE_COOKIE_DOMAIN)));
	if(cookie_domain.nullValue())
	{
		// use the fully qualified website domain name
		f_domain = f_snap->get_website_key();
	}
	else
	{
		f_domain = cookie_domain.stringValue();
	}

	set_value(value);
}


/** \brief Set the value of the cookie.
 *
 * This function allows you to change the value of the cookie.
 * By default it is set to an empty string unless you define
 * the value in the constructor.
 *
 * The value is encoded using the usual urlencode mechanism
 * as to avoid problems with controls and other data.
 *
 * We support binary data by calling the set_value() with
 * the QByteArray parameter.
 *
 * \param[in] value  The new value of the cookie.
 */
void http_cookie::set_value(const QString& value)
{
	set_value(value.toUtf8());
}


/** \brief Set the value of the cookie.
 *
 * This function allows you to change the value of the cookie.
 * By default it is set to an empty string unless you define
 * the value in the constructor.
 *
 * The value is encoded using the usual urlencode mechanism
 * as to avoid problems with controls and other data.
 *
 * We support binary data by calling the set_value() with
 * the QByteArray parameter.
 *
 * \param[in] value  The new value of the cookie.
 */
void http_cookie::set_value(const QByteArray& value)
{
	f_value = value;
}


/** \brief Set the cookie domain.
 *
 * This function is used to set the cookie domain although
 * it generally should not be required because the constructor
 * already does so automatically for you using the website
 * key as defined in the snap child.
 *
 * If you want to support many sub-domains, then you should
 * define the cookie domain as a site parameter instead.
 *
 * \note
 * Using the wrong domain name does nothing as the browser
 * ignores cookies with wrong domain names.
 *
 * \param[in] domain  The new domain name for this cookie.
 *
 * \sa get_domain();
 */
void http_cookie::set_domain(const QString& domain)
{
	f_domain = domain;

	// TODO?
	// Enhance the check so we don't accept two periods one after
	// another or two dashes or a name that starts/ends with invalid
	// characters (i.e. cannot start/end with a dash.) Although some
	// of those would not be necessary if we check the domain against
	// the website domain name.
	int max(f_domain.length());
	if(max > 0 && f_domain[0] == '.')
	{
		f_domain = f_domain.mid(1);
		max = f_domain.length();
	}
	if(max == 0)
	{
		throw std::runtime_error("the domain of a cookie cannot be empty");
	}
	for(int i(0); i < max; ++i)
	{
		// TODO:
		// TBD -- How is that supporting Unicode characters in domain names?
		ushort c(f_domain[i].unicode());
		if((c < 'A' || c > 'Z')
		&& (c < 'a' || c > 'z')
		&& (c < '0' || c > '9')
		&& c != '.' && c != '-' && c != '_')
		{
			throw std::runtime_error("the domain of a cookie must only include domain name compatible characters");
		}
	}

	// TODO: add a check to make sure this is a valid domain name
	//       (i.e. to the minimum "domain + TLD")
}


/** \brief Set the path where the cookie is to be saved.
 *
 * By default the cookie is setup to be viewed everywhere (i.e. the path
 * is set to "/".) To constrain the cookie to a section of a website
 * set the path to that section here.
 *
 * This can be useful to setup a secure cookie so administrators can
 * get a special cookie that really only works in the administrative
 * part of the website.
 *
 * \param[in] path  The new cookie path.
 *
 * \sa get_path();
 */
void http_cookie::set_path(const QString& path)
{
	// TODO:
	// TBD -- How is that supporting Unicode characters in paths?
	// (we may have to change them to some %XX syntax
	int max(path.length());
	for(int i(0); i < max; ++i)
	{
		ushort c(f_domain[i].unicode());
		if((c < ' ' || c > '~')
		&& c != ',' && c != ';')
		{
			throw std::runtime_error("the path of a cookie must only include ASCII characters except controls, ',' and ';'.");
		}
	}

	f_path = path;
}


/** \brief Mark the cookie for deletion.
 *
 * This function sets the expiration date in the past so the cookie
 * gets deleted. It is usual to set the date to January 1, 1970
 * (i.e. Unix time of 0) and we do so here.
 *
 * \sa set_expire();
 * \sa set_session();
 * \sa get_type();
 */
void http_cookie::set_delete()
{
	// January 1, 1970 00:00:00 is represented as 0
	f_expire = QDateTime::fromMSecsSinceEpoch(0);
}


/** \brief Mark the cookie as a session cookie.
 *
 * This function invalidates the expiration date of the cookie, which is the
 * default. When the expiration date is invalid, it is not sent to the browser
 * and it transform the cookie in a session cookie.
 *
 * This type of cookie only lasts until you close the browser window. For
 * sensitive accounts (dealing with e-Commerce and similar) it is a good idea
 * to use this form of cookie.
 *
 * \sa set_delete();
 * \sa set_expire();
 * \sa get_type();
 */
void http_cookie::set_session()
{
	// use an invalid date
	f_expire = QDateTime();
}


/** \brief Set the expiration date of the cookie.
 *
 * This function changes the expiration date to the specified date and time.
 *
 * In most cases, it is easier to use the set_expire_in() function which uses
 * now + the specified number of seconds. The result will be the same either
 * way because we only send an explicit expiration date and not a Max-Age
 * parameter.
 *
 * In order to create a session cookie (the default), you may set the date
 * to an invalid date or call the set_session() function:
 *
 * \code
 *   QDateTime invalid;
 *   cookie.set_expiration(invalid);
 * \endcode
 *
 * To delete a cookie, you can set the expiration date to a date in the past.
 * This is also achieved by calling the set_delete() function.
 *
 * \param[in] date_time  The new expiration date and time.
 *
 * \sa set_session();
 * \sa set_delete();
 * \sa get_expire();
 * \sa get_type();
 */
void http_cookie::set_expire(const QDateTime& date_time)
{
	f_expire = date_time;
}


/** \brief This function sets the expiration date seconds in the future.
 *
 * This function is most often the one used and allows you to set the
 * expiration date of the cookie the specified number of seconds in
 * the future.
 *
 * The function makes use of the snap child start date plus that number
 * of seconds, but it sends the cookie with an Expire field (i.e. we do
 * not make use of the Max-Age field.)
 *
 * \param[in] seconds  The number of seconds from now when the cookie expires.
 *
 * \sa set_expire();
 * \sa get_expire();
 * \sa set_delete();
 * \sa set_session();
 */
void http_cookie::set_expire_in(int seconds)
{
	f_expire = QDateTime::fromMSecsSinceEpoch(f_snap->get_start_date() / 1000 + seconds * 1000);
}


/** \brief Mark the cookie as secure.
 *
 * By default cookies are not marked as secure. Call this function with
 * the \p secure parameter set to true so the cookie only travels between
 * the browser and the server if SSL is used.
 *
 * Note that a secure cookie is not seen if the user decides to access
 * your website using the HTTP protocol (opposed to the HTTPS protocol.)
 * Websites make use of a secure cookie when they have a certificate.
 *
 * \note
 * Snap! implements ways to support logged in users on non-secure
 * connections, but with much lower rights.
 *
 * \param[in] secure  Whether the cookie should be made secure or not.
 *
 * \sa get_secure();
 */
void http_cookie::set_secure(bool secure)
{
	f_secure = secure;
}


/** \brief Set the HttpOnly flag.
 *
 * This function changes the value of the HttpOnly flag. This flag is
 * used by browsers to prevent cookies from being visible form JavaScript.
 * This is important to avoid all sorts of session highjack attacks.
 *
 * By default the cookies are visible by JavaScript and other client
 * supported languages.
 *
 * \param[in] http_only  Whether this cookie is only for HTTP.
 *
 * \sa get_http_only();
 */
void http_cookie::set_http_only(bool http_only)
{
	f_http_only = http_only;
}


/** \brief Retrive the name of the cookie.
 *
 * The name of the cookie is set when you create it. It cannot be
 * changed.
 *
 * \return The name of the cookie.
 */
const QString& http_cookie::get_name() const
{
	return f_name;
}


/** \brief Retrieve the cookie value.
 *
 * This function returns the cookie value as a QByteValue. If you set the cookie
 * as a string, then you can convert it back to a string with the following:
 *
 * \code
 * const QByteArray& v(cookie.get_value());
 * QString str(QString::fromUtf8(v.data(), v.size()));
 * \endcode
 *
 * \return A constant reference to the cookie contents.
 *
 * \sa set_value()
 */
const QByteArray& http_cookie::get_value() const
{
	return f_value;
}


/** \brief Get the current cookie type.
 *
 * Depending on how the expiration date is setup, the cookie may have
 * one of the following types:
 *
 * \li HTTP_COOKIE_TYPE_PERMANENT -- the expiration date and time is valid and in the future
 * \li HTTP_COOKIE_TYPE_SESSION -- the expiration date and time is not valid
 * \li HTTP_COOKIE_TYPE_DELETE -- the expiration date and time is in the past
 *
 * \return One of the HTTP_COOKIE_TYPE_... values.
 *
 * \sa set_expire()
 * \sa set_delete()
 * \sa set_session()
 * \sa get_expire()
 * \sa set_expire_in()
 */
http_cookie::http_cookie_type_t http_cookie::get_type() const
{
	if(!f_expire.isValid())
	{
		return HTTP_COOKIE_TYPE_SESSION;
	}
	// TBD -- Utc? or not Utc? As far as I know the cookie date is
	//        in UTC so we need to compare with the UTC current date
	if(f_expire < QDateTime::currentDateTimeUtc())
	{
		return HTTP_COOKIE_TYPE_DELETE;
	}
	return HTTP_COOKIE_TYPE_PERMANENT;
}


/** \brief Get the cookie domain information.
 *
 * This function retreives the current domain information of the cookie.
 * In general the domain is equal to the website key.
 *
 * \return The domain defined in this cookie.
 *
 * \sa set_domain();
 */
const QString& http_cookie::get_domain() const
{
	return f_domain;
}


/** \brief Retrieve the path under which the cookie is valid.
 *
 * This function retrieves the current path for this cookie.
 * By default it is set to "/" which means the cookie is
 * available over the entire website. It is rarely necessary
 * to change this value.
 *
 * \return The current path for this cookie.
 *
 * \sa set_path();
 */
const QString& http_cookie::get_path() const
{
	return f_path;
}


/** \brief Get the expiration date.
 *
 * This function returns the current expiration date. The date represents
 * different status of the cookie which can be determined by calling the
 * get_type() function.
 *
 * The default is an invalid cookie which means that the cookie is a
 * session cookie (lasts until the browser is closed.)
 *
 * \return The expiration date of this cookie.
 *
 * \sa set_expire();
 * \sa set_expire_in();
 * \sa set_delete();
 * \sa set_session();
 * \sa get_type();
 */
const QDateTime& http_cookie::get_expire() const
{
	return f_expire;
}


/** \brief Retrieve whether the cookie is secure.
 *
 * This function returns whether the cookie should only travel on a
 * secure (SSL) connection.
 *
 * The default is false (cookie is visible on non-secure connections.)
 *
 * \return true if the cookie is only for secure connections, false otherwise.
 *
 * \sa set_secure();
 */
bool http_cookie::get_secure() const
{
	return f_secure;
}


/** \brief Retrieve whether the cookie is only for HTTP.
 *
 * By default cookies are visible to JavaScript and other client languages.
 * This parameter can be used to hide the content of the cookie from praying
 * eyes and only allow the data to travel between the browser and server.
 *
 * The default is false.
 *
 * \return The current HttpOnly flag status.
 *
 * \sa set_http_only();
 */
bool http_cookie::get_http_only() const
{
	return f_http_only;
}


/** \brief Transform the cookie for the HTTP header.
 *
 * This function transforms the cookie so it works as an HTTP header.
 * It follows the HTTP 1.1 specifications, although it never makes
 * use of the Max-Age field.
 *
 * \return A valid HTTP cookie header.
 */
QString http_cookie::to_http_header() const
{
	// Note: the name was already checked for invalid characters
	QString result("Set-Cookie: " + f_name + "=");

	const char *v(f_value.constData());
	int max(f_value.size());
	for(int i(0); i < max; ++i)
	{
		char c(v[i]);
		if(c == 0x21
		|| (c >= 0x23 && c <= 0x2B)
		|| (c >= 0x2D && c <= 0x3A)
		|| (c >= 0x3C && c <= 0x5B)
		|| (c >= 0x5D && c <= 0x7E))
		{
			result += c;
		}
		else
		{
			// add the byte as %XX
			result += QString("%%%1").arg(c, 2, 16, QChar('0'));
		}
	}

	switch(get_type())
	{
	case HTTP_COOKIE_TYPE_PERMANENT:
		// compute date/time
		// HTTP format generates: Sun, 06 Nov 1994 08:49:37 GMT
		// (see http://tools.ietf.org/html/rfc2616#section-3.3.1)
		result += "; Expires=" + f_expire.toString("ddd, dd MMM yyyy hh:mm:ss GMT");
		break;

	case HTTP_COOKIE_TYPE_SESSION:
		// no Expires
		break;

	case HTTP_COOKIE_TYPE_DELETE:
		// no need to waste time computing that date
		result += "; Expires=Thu, 01-Jan-1970 00:00:01 GMT";
		break;

	}

	if(!f_domain.isEmpty())
	{
		// the domain sanity was already checked so we can save it as it here
		result += "; Domain=" + f_domain;
	}

	if(!f_path.isEmpty())
	{
		// the path sanity was already checked so we can save it as it here
		result += "; Path=" + f_path;
	}

	if(f_secure)
	{
		result += "; Secure";
	}

	if(f_http_only)
	{
		result += "; HttpOnly";
	}

	return result;
}



} // namespace snap

// vim: ts=4 sw=4
