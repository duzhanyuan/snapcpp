// Snap Websites Server -- filter
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

#include "filter.h"

#include "../locale/snap_locale.h"
#include "../messages/messages.h"

#include "log.h"
#include "not_used.h"
#include "qdomxpath.h"
#include "qdomhelpers.h"
#include "qstring_stream.h"

#include <iostream>
#include <cctype>

#include "poison.h"


SNAP_PLUGIN_START(filter, 1, 0)



filter::filter_text_t::filter_text_t(content::path_info_t & ipath, QDomDocument & xml_document, QString const & text)
    : f_ipath(ipath)
    , f_xml_document(xml_document)
    , f_text(text)
    //, f_changed(false) -- auto-init
    //, f_support_edit(true) -- auto-init
{
}


void filter::filter_text_t::set_support_edit(bool support_edit)
{
    f_support_edit = support_edit;
}


bool filter::filter_text_t::get_support_edit() const
{
    return f_support_edit;
}


content::path_info_t & filter::filter_text_t::get_ipath()
{
    return f_ipath;
}


bool filter::filter_text_t::has_changed() const
{
    return f_changed;
}


QDomDocument & filter::filter_text_t::get_xml_document() const
{
    return f_xml_document;
}


void filter::filter_text_t::set_text(QString const & text)
{
    if(f_text != text)
    {
        f_text = text;
        f_changed = true;
    }
}


QString const & filter::filter_text_t::get_text() const
{
    return f_text;
}




void filter::filter_teaser_info_t::set_max_words(int words)
{
    f_words = words;
}


int filter::filter_teaser_info_t::get_max_words() const
{
    return f_words;
}


void filter::filter_teaser_info_t::set_max_tags(int tags)
{
    f_tags = tags;
}


int filter::filter_teaser_info_t::get_max_tags() const
{
    return f_tags;
}


/** \brief Initialize the filter plugin.
 *
 * This function is used to initialize the filter plugin object.
 */
filter::filter()
    //: f_snap(nullptr) -- auto-init
{
}


/** \brief Clean up the filter plugin.
 *
 * Ensure the filter object is clean before it is gone.
 */
filter::~filter()
{
}


/** \brief Get a pointer to the filter plugin.
 *
 * This function returns an instance pointer to the filter plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the filter plugin.
 */
filter * filter::instance()
{
    return g_plugin_filter_factory.instance();
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
QString filter::description() const
{
    return "This plugin offers functions to filter XML and HTML data."
        " Especially, it is used to avoid Cross Site Attacks (XSS) from"
        " hackers. XSS is a way for a hacker to gain access to a person's"
        " computer through someone's website.";
}


/** \brief Initialize the filter plugin.
 *
 * This function terminates the initialization of the filter plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void filter::on_bootstrap(::snap::snap_child * snap)
{
    f_snap = snap;

    SNAP_LISTEN(filter, "server", server, xss_filter, _1, _2, _3);
}


/** \brief Filter an DOM node and remove all unwanted tags.
 *
 * This filter accepts:
 *
 * \li A DOM node (QDomNode) which is to be filtered
 * \li A string of tags to be kept
 * \li A string of attributes to be kept (or removed)
 *
 * The \p accepted_tags parameter is a list of tags names separated
 * by spaces (i.e. "a h1 h2 h3 img br".)
 *
 * By default the \p accepted_attributes parameter includes all the
 * attributes to be kept. You can inverse the meaning using a ! character
 * at the beginning of the string (i.e. "!style" instead of
 * "href target title alt src".) At some point we probably want
 * to have a DTD like support where each tag has its list of
 * attributes checked and unknown attributes removed. This list would
 * be limited since only user entered tags need to be checked
 * (i.e. "p div a b img ul dl ol li ...")
 *
 * The ! character does not work in the \p accepted_tags parameter.
 *
 * The filtering makes sure of the following:
 *
 * 1) The strings are valid UTF-8 (Bad UTF-8 can cause problems in IE6)
 *
 * 2) The content does not include a NUL
 *
 * 3) The syntax of the tags (this is done through the use of the htmlcxx tree)
 *
 * 4) Remove entities that look like NT4 entities: &{\<name>};
 *
 * \note
 * The call \c QDomNode::toString(-1).toUft8().data() generates a valid UTF-8
 * string no matter what. Therefore we do not need to filter for illegal
 * characters in this filter function. The output will always be correct.
 *
 * \param[in,out] node  The HTML to be filtered.
 * \param[in] accepted_tags  The list of tags kept in the text.
 * \param[in] accepted_attributes  The list of attributes kept in the tags.
 */
void filter::on_xss_filter(QDomNode & node,
                           QString const & accepted_tags,
                           QString const & accepted_attributes)
{
    // initialize the array of tags so it starts and ends with spaces
    // this allows for much faster searches (i.e. indexOf())
    QString const tags(" " + accepted_tags + " ");

    QString attr(" " + accepted_attributes + " ");
    bool const attr_refused(accepted_attributes[0] == '!');
    if(attr_refused)
    {
        // erase the '!' from the attr string
        attr.remove(1, 1);
    }

    // go through the entire tree
    QDomNode n = node.firstChild();
    while(!n.isNull())
    {
        // determine the next pointer so we can delete this node
        QDomNode parent = n.parentNode();
        QDomNode next = n.firstChild();
        if(next.isNull())
        {
            next = n.nextSibling();
            if(next.isNull())
            {
                QDomNode p(parent);
                do
                {
                    next = p.nextSibling();
                    p = p.parentNode();
                }
                while(next.isNull() && !p.isNull());
            }
        }

        // Is this node a tag? (i.e. an element)
        if(n.isElement())
        {
            QDomElement e(n.toElement());
            // check whether this tag is acceptable
            QString const & name(e.tagName());
            if(tags.indexOf(" " + name.toLower() + " ") == -1)
            {
//qDebug() << "removing tag: [" << name << "] (" << tags << ")\n";
                // remove this tag, now there are two different type of
                // removals: complete removal (i.e. <script>) and removal
                // of the tag, but not the children (i.e. <b>)
                // the xmp and plaintext are browser extensions
                if(name != "script" && name != "style" && name != "textarea"
                && name != "xmp" && name != "plaintext")
                {
                    // in this case we can just remove the tag itself but keep
                    // its children which we have to move up one level
                    QDomNode c(n.firstChild());
                    while(!c.isNull())
                    {
                        QDomNode next_sibling(c.nextSibling());
                        n.removeChild(c);
                        parent.insertBefore(c, n);
                        c = next_sibling;
                    }
                }
                parent.removeChild(n);
                next = parent.nextSibling();
                if(next.isNull())
                {
                    next = parent;
                }
            }
            else
            {
                // remove unwanted attributes too
                QDomNamedNodeMap attributes(n.attributes());
                int const max_attr(attributes.length());
                for(int i(0); i < max_attr; ++i)
                {
                    QDomAttr const a(attributes.item(i).toAttr());
                    QString const & attr_name(a.name());
                    if((attr.indexOf(" " + attr_name.toLower() + " ") == -1) ^ attr_refused)
                    {
                        e.removeAttribute(attr_name);
//qDebug() << "removing attribute: " << attr_name << " max: " << max_attr << " size: " << attributes.length() << "\n";
                    }
                }
            }
        }
        else if(n.isComment() || n.isProcessingInstruction()
             || n.isNotation() || n.isEntity() || n.isDocument()
             || n.isDocumentType() || n.isCDATASection())
        {
            // remove all sorts of unwanted nodes
            // these are not tags, but XML declarations which have nothing
            // to do in clients code that is parsed via the XSS filter
            //
            // to consider:
            // transform a CDATA section to plain text
            //
            // Note: QDomComment derives from QDomCharacterData
            //       QDomCDATASection derives from QDomText which derives from QDomCharacterData
//qDebug() << "removing \"wrong\" node type\n";
            parent.removeChild(n);
            next = parent.nextSibling();
            if(next.isNull())
            {
                next = parent;
            }
        }
        // the rest is considered to be text
        n = next;
    }
}


/** \brief Replace a token with a corresponding value.
 *
 * This function is expected to replace the specified token with a
 * replacement value. For example, the '[year]' token can be replaced
 * with the current year.
 *
 * The default filter function knows how to handle the special token
 * '[test]' which is replaced by the text "The Test Token Worked" written
 * in bold. It does not support parameters although it ignores the fact
 * (i.e. parameters can be used, they will be ignored.)
 *
 * The default filter replace_token event supports the following
 * general tokens:
 *
 * \li [date(\"format\")] -- date with format as per strftime(); without
 *                           format, the default depends on the locale
 * \li [select("\<xpath>")] -- select content from the XML document using
 *                             the specified \<xpath>
 * \li [select_text("\<xpath>")] -- select content from the XML document using
 *                                  the specified \<xpath>, output as text
 * \li [site-name] -- the name of the site as defined in the 'sites' table
 * \li [test] -- a simple test token, it inserts "The Test Token Worked"
 *               message, in English.
 * \li [version] -- version of the Snap! C++ server
 * \li [year] -- the 4-digit year when the request started
 *
 * \param[in,out] ipath  The canonicalized path linked with this XML document.
 * \param[in,out] xml  The XML document where tokens are being replaced.
 * \param[in,out] token  The token object, with the token name and optional parameters.
 *
 * \return true if other plugins may work on the replacement, and return
 *         false if this function already converted the token
 */
bool filter::replace_token_impl(content::path_info_t & ipath, QDomDocument & xml, token_info_t & token)
{
    NOTUSED(ipath);

    if(token.is_token("test"))
    {
        token.f_replacement = "<span style=\"font-weight: bold;\">The Test Token Worked</span>";
        return false;
    }
    else if(token.is_token("select") || token.is_token("select_text"))
    {
        if(token.verify_args(1, 1))
        {
            parameter_t param(token.get_arg("xpath", 0));
            if(!token.f_error)
            {
                // in this case the XPath is dynamic so we have to compile now
                QDomXPath dom_xpath;
                dom_xpath.setXPath(param.f_value);
                QDomXPath::node_vector_t result(dom_xpath.apply(xml));
                // at this point we expect the result to be 1 (or 0) entries
                // if more than 1, ignore the following nodes
                if(result.size() > 0)
                {
                    // apply the replacement
                    if(result[0].isElement())
                    {
                        if(token.f_name == "select_text")
                        {
                            token.f_replacement = result[0].toElement().text();
                        }
                        else
                        {
                            QDomDocument document;
                            QDomNode copy(document.importNode(result[0], true));
                            document.appendChild(copy);
                            token.f_replacement = document.toString(-1);
                        }
                    }
                    else if(result[0].isAttr())
                    {
                        token.f_replacement = result[0].toAttr().value();
                    }
                }
            }
        }
        return false;
    }
    else if(token.is_token("year"))
    {
        // TODO: add support for local time and user defined unix time
        time_t now(f_snap->get_start_time());
        struct tm time_info;
        gmtime_r(&now, &time_info);
        token.f_replacement = QString("%1").arg(time_info.tm_year + 1900);
        return false;
    }
    else if(token.is_token("date"))
    {
        if(token.verify_args(0, 2))
        {
            time_t unix_time(f_snap->get_start_time());
            QString date_format;
            if(token.has_arg("format", 0))
            {
                parameter_t param(token.get_arg("format", 0, token_t::TOK_STRING));
                date_format = param.f_value;
            }
            if(token.has_arg("unixtime", 1))
            {
                parameter_t param(token.get_arg("unixtime", 1, token_t::TOK_STRING));
                bool ok(false);
                unix_time = param.f_value.toLongLong(&ok);
                // TODO: verify ok
            }
            token.f_replacement = locale::locale::instance()->format_date(unix_time, date_format, true);
        }
        return false;
    }
    else if(token.is_token("gmdate"))
    {
        if(token.verify_args(0, 2))
        {
            time_t unix_time(f_snap->get_start_time());
            QString date_format;
            if(token.has_arg("format", 0))
            {
                parameter_t param(token.get_arg("format", 0, token_t::TOK_STRING));
                date_format = param.f_value;
            }
            if(token.has_arg("unixtime", 1))
            {
                parameter_t param(token.get_arg("unixtime", 1, token_t::TOK_STRING));
                bool ok(false);
                unix_time = param.f_value.toLongLong(&ok);
                // TODO: verify ok
            }
            token.f_replacement = locale::locale::instance()->format_date(unix_time, date_format, false);
        }
        return false;
    }
    else if(token.is_token("version"))
    {
        token.f_replacement = SNAPWEBSITES_VERSION_STRING;
        return false;
    }
    else if(token.is_token("site-name"))
    {
        token.f_replacement = f_snap->get_site_parameter(get_name(snap::name_t::SNAP_NAME_CORE_SITE_NAME)).stringValue();
        return false;
    }

    return true;
}



/** \brief Read all the XML text and replace its tokens.
 *
 * This function searches all the XML text and replace the tokens it finds
 * in these texts with the corresponding replacement value.
 *
 * The currently supported syntax is:
 *
 * \code
 *   '[' <name> [ '(' [ [ <name> '=' ] <param> ',' ... ] ')' ] ']'
 * \endcode
 *
 * where \<name> is composed of letter, digit, and colon characters.
 *
 * where \<param> is composed of identifiers, numbers, or quoted strings
 * (' or "); parameters are separated by commas and can be named if
 * preceded by a name and an equal sign.
 *
 * Spaces are allowed between parameters and parenthesis. However, no space
 * is allowed after the opening square bracket ([). Spaces are ignored and
 * are not required.
 *
 * \param[in,out] ipath  The canonicalized path being processed.
 * \param[in,out] xml  The XML document to filter.
 */
void filter::on_token_filter(content::path_info_t & ipath, QDomDocument & xml)
{
    class filter_state_t
    {
    public:
        filter_state_t(QDomDocument doc, content::path_info_t & path)
            //: f_state() -- auto-init
        {
            state_t s;
            s.f_node = doc;
            s.f_owner = "filter";
            s.f_ipath = path;
            f_state.push_back(s);
        }

        void push(QDomElement e)
        {
            state_t s;
            s.f_node = e;
            s.f_owner = e.attribute("owner");
            s.f_ipath.set_path(e.attribute("path"));
            // TBD error or default if f_owner is empty?
            //     (f_ipath can be empty because the root cpath is "")
            f_state.push_back(s);
//std::cerr << "Push " << s.f_owner << ", path " << s.f_ipath << "\n";
        }

        void pop(QDomNode p)
        {
            if(f_state.isEmpty())
            {
                throw snap_logic_exception("filter state stack empty on a pop() call");
            }

            if(f_state.last().f_node == p)
            {
//std::cerr << "Pop " << f_state.last().f_owner << ", path " << f_state.last().f_ipath << "\n";
                f_state.pop_back();
            }
        }

        QString const & owner() const
        {
            if(f_state.isEmpty())
            {
                throw snap_logic_exception("filter state stack empty on an owner() call");
            }

            return f_state.last().f_owner;
        }

        content::path_info_t & ipath() const
        {
            if(f_state.isEmpty())
            {
                throw snap_logic_exception("filter state stack empty on a path() call");
            }

            content::path_info_t & last_ipath(const_cast<content::path_info_t &>(f_state.last().f_ipath));
            last_ipath.set_parameter("token_owner", f_state.last().f_owner);
            return last_ipath;
        }

    private:
        struct state_t
        {
            QDomNode                f_node;
            QString                 f_owner;
            content::path_info_t    f_ipath;
        };

        QVector<state_t>    f_state;
    };

    // Avoid recursivity
    typedef QMap<QString, bool> save_paths_t;
    static save_paths_t g_ipaths;

    // list of ipaths are saved in g_ipaths to avoid infinite loop
    if(g_ipaths.contains(ipath.get_key()))
    {
        // we do not throw, instead we want to "return" an error
        // however we're not (yet) replacing a token here so we just
        // generate a "standard" message
        QString paths;
        for(save_paths_t::const_iterator it(g_ipaths.begin());
                                        it != g_ipaths.end();
                                        ++it)
        {
            if(!paths.isEmpty())
            {
                paths += ", ";
            }
            paths += it.key();
        }
        // Lists have a HUGE problem with this one... for now I'm
        // turning off the error message because in most cases it
        // is not a real problem (We can move on...)
        //
        SNAP_LOG_ERROR("One or more tokens are looping back to page \"")(ipath.get_key())("\" (all paths are: \"")(paths)("\").");
        //messages::messages::instance()->set_error(
        //    "Recursive Token(s)",
        //    QString("One or more tokens are looping back to page \"%1\" (all paths are: \"%2\").").arg(ipath.get_key()).arg(paths),
        //    QString("to fix, look at the tokens that loop"),
        //    false
        //);
        return;
    }
    struct add_remove_path_t
    {
        add_remove_path_t(save_paths_t & map, content::path_info_t & ipath)
            : f_map(map)
            , f_ipath(ipath)
        {
            f_map[f_ipath.get_key()] = true;
        }

        ~add_remove_path_t()
        {
            f_map.remove(f_ipath.get_key());
        }

        save_paths_t&           f_map;
        content::path_info_t&   f_ipath;
    };
    add_remove_path_t safe_ipath(g_ipaths, ipath);

    // start the token replacement
    filter_state_t state(xml, ipath);

    QDomNode n(xml.firstChild());
    while(!n.isNull())
    {
        QVector<QDomNode> to_pop;

        // determine the next pointer so we can delete this node
        QDomNode parent(n.parentNode());
        QDomNode next(n.firstChild());
        if(next.isNull())
        {
            next = n.nextSibling();
            if(next.isNull())
            {
                QDomNode p(parent);
                do
                {
                    to_pop.push_back(p);
                    next = p.nextSibling();
                    p = p.parentNode();
                }
                while(next.isNull() && !p.isNull());
            }
        }

        // TODO support comments, instructions, etc.

        // we want to transform tokens in text areas and in attributes
        if(n.isCDATASection())
        {
            // this works too, although the final result is still "plain text"!
            // (it must be xslt that converts the contents of CDATA sections)
            //
            // TODO: if the CDATA section includes tags, then this will not
            //       work quite as expected (i.e. it could "convert" and
            //       even break tags.)
            //
            QDomCDATASection cdata_section(n.toCDATASection());
//std::cerr << "*** CDATA section [" << cdata_section.data() << "]\n";

            filter_text_t txt_filt(state.ipath(), xml, cdata_section.data());
            filter_text(txt_filt);
            if(txt_filt.has_changed())
            {
//std::cerr << "replace CDATA [" << cdata_section.data() << "]\n";
                // replace the text with its contents
                cdata_section.setData(txt_filt.get_text());
            }
        }
        else if(n.isText())
        {
            QDomText text(n.toText());
            filter_text_t txt_filt(state.ipath(), xml, text.data());
            filter_text(txt_filt);
            if(txt_filt.has_changed())
            {
//SNAP_LOG_WARNING("***\n*** replace text [")(text.data())("] with [")(t.result())("]\n***\n");
                // replace the text with its contents
                snap_dom::replace_node_with_html_string(n, txt_filt.get_text());
            }
        }
        else if(n.isElement())
        {
            QDomElement e(n.toElement());

            // apply the replacement to all the attributes of each tag
            QDomNamedNodeMap attrs(e.attributes());
            int const max_attrs(attrs.size());
            for(int i(0); i < max_attrs; ++i)
            {
                QDomAttr a(attrs.item(i).toAttr());
                filter_text_t txt_filt(state.ipath(), xml, a.value());
                txt_filt.set_support_edit(false);
                filter_text(txt_filt);
                if(txt_filt.has_changed())
                {
                    // TBD: should we warn the user that some of his
                    //      tokens and other generated data included
                    //      a tag or two...
                    //
                    a.setValue(snap_dom::remove_tags(txt_filt.get_text()));
                }
            }

            QString const tag_name(e.tagName());
            // TBD -- is it a problem to have hard coded tag names here?
            if(tag_name == "snap" || tag_name == "filter")
            {
                // if the element has no children then we do
                // not want to push anything because it will
                // not get popped properly otherwise
                QDomNode child = n.firstChild();
                if(!child.isNull())
                {
                    state.push(e);
                }
            }
        }

        // we need to pop this one after handling or we get the
        // wrong information in the tag before exiting a tag
        // (also must be popped in order, i.e. FIFO)
        int const pop_max(to_pop.size());
        for(int i(0); i < pop_max; ++i)
        {
            state.pop(to_pop[i]);
        }

        // the rest is considered to be text
        n = next;
    }
}


/** \brief Filter a text area.
 *
 * The signal is given each time the input XML includes a tag with
 * some text. Only each filter may transform that text in HTML.
 *
 * The filter_text_t class includes functions to retrieve the
 * text between each tag since it may end up including tags
 * even if it starts with just text. If you want to insert
 * a \<, then make sure to addt he entity \&lt;.
 *
 * \param[in,out] txt_filt  The text filter class used to access the
 *                          resulting string.
 *
 * \return true if the filter_text() signal should be propagated.
 */
bool filter::filter_text_impl(filter_text_t & txt_filt)
{
    class text_t
    {
    public:
        typedef ushort char_t;

        text_t(
                snap_child * snap,
                filter * f,
                content::path_info_t & ipath,
                QDomDocument & xml,
                QString const & text,
                bool support_edit = true
            )
            : f_snap(snap)
            , f_filter(f)
            , f_ipath(ipath)
            , f_xml(xml)
            , f_index(0)
            , f_extra_index(0)
            , f_text(text)
            //, f_result("") -- auto-init
            //, f_token("") -- auto-init
            //, f_replacement("") -- auto-init
            //, f_extra_input("") -- auto-init
            , f_support_edit(support_edit)
        {
        }

        bool parse()
        {
            f_result = "";
            f_result.reserve(f_text.size() * 2);

            bool changed(false);
            for(char_t c(0); c = getc(); )
            {
                if(c == '[')
                {
                    if(parse_token())
                    {
                        changed = true;
                    }
                    else
                    {
                        // it failed, add the token content as is
                        // to the result
                        f_result += f_token;
                    }
                }
                else
                {
                    f_result += QChar(c);
                }
            }

            return changed;
        }

        QString const & result() const
        {
            return f_result;
        }

    private:
        // it is not yet proven to be a token...
        bool parse_token()
        {
            token_info_t info;

            // reset the token variable
            f_token = "[";
            token_t t(get_token(info.f_name, false));
            f_token += info.f_name;
            if(t != token_t::TOK_IDENTIFIER)
            {
                // the '[' must be followed by an identifier, no choice here
                return false;
            }
            QString tok;
            t = get_token(tok);
            f_token += tok;
            if(t != token_t::TOK_SEPARATOR || (tok != "]" && tok != "("))
            {
                // we can only have a ']' or '(' separator at this point
                return false;
            }
            if(tok == "(")
            {
                // note: the list of parameters may be empty
                t = get_token(tok);
                f_token += tok;
                if(t != token_t::TOK_SEPARATOR || tok != ")")
                {
                    parameter_t param;
                    param.f_type = t;
                    param.f_value = tok;
                    for(;;)
                    {
                        switch(param.f_type)
                        {
                        case token_t::TOK_IDENTIFIER:
                            {
                                t = get_token(tok);
                                f_token += tok;
                                if(t == token_t::TOK_SEPARATOR && tok == "=")
                                {
                                    // named parameter; the identifier was the name
                                    // and not the value, swap those
                                    param.f_name = param.f_value;
                                    param.f_type = get_token(param.f_value);
                                    f_token += param.f_value;
                                    switch(param.f_type)
                                    {
                                    case token_t::TOK_STRING:
                                        // remove the quotes from the parameters
                                        param.f_value = param.f_value.mid(1, param.f_value.size() - 2);
                                        break;

                                    case token_t::TOK_INTEGER:
                                    case token_t::TOK_REAL:
                                        break;

                                    default:
                                        return false;

                                    }
                                    t = get_token(tok);
                                    f_token += tok;
                                }
                            }
                            break;

                        case token_t::TOK_STRING:
                            // remove the quotes from the parameters
                            param.f_value = param.f_value.mid(1, param.f_value.size() - 2);
                            /*FALLTHROUGH*/
                        case token_t::TOK_INTEGER:
                        case token_t::TOK_REAL:
                            t = get_token(tok);
                            f_token += tok;
                            break;

                        default:
                            // anything else is wrong
                            return false;

                        }
                        info.f_parameters.push_back(param);

                        if(t != token_t::TOK_SEPARATOR)
                        {
                            // only commas are accepted here until we find
                            // a closing parenthesis
                            return false;
                        }

                        if(tok == ")")
                        {
                            // we're done reading the list of parameters
                            break;
                        }
                        if(tok != ",")
                        {
                            // we only accept commas between parameters
                            return false;
                        }

                        param.reset();
                        param.f_type = get_token(param.f_value);
                        f_token += param.f_value;
                    }
                }
                t = get_token(tok);
                f_token += tok;
            }
            if(tok != "]")
            {
                // a token must end with ']'
                return false;
            }

            // valid input, now verify that it does exist in the current
            // installation
            f_filter->replace_token(f_ipath, f_xml, info);
            if(!info.f_found)
            {
                // the token is not known, that's an error so we do not
                // replace anything
                return false;
            }
            // TODO: at this point this check test whether the page as a
            //       whole is in edit mode, when some parts may not be
            //       editable to the current user
            //
            if(f_support_edit && f_snap->get_action() == "edit")
            {
                // if the editor is turned on, then we want to mark all
                // fields as such so the editor is aware of them
                QByteArray utf8(info.f_replacement.toUtf8());
                bool use_span(true);
                for(char const *s(utf8.data()); *s != '\0'; ++s)
                {
                    if(*s == '<')
                    {
                        ++s;
                        if((*s >= 'a' && *s <= 'z')
                        || (*s >= 'A' && *s <= 'Z'))
                        {
                            char const *start(s);
                            for(++s; (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'); ++s);
                            int const len(static_cast<int>(s - start));
                            use_span = f_snap->tag_is_inline(utf8.data(), len);
                            if(!use_span)
                            {
                                break;
                            }
                        }
                    }
                }
                QString const unbracketed_quote(f_token.mid(1, f_token.length() - 2));
                info.f_replacement = QString("<%1 class=\"filter-token\" token=\"%2\">%3</%1>")
                        .arg(use_span ? "span" : "div")
                        .arg(encode_text_for_html(unbracketed_quote))
                        .arg(info.f_replacement);
            }
            ungets(info.f_replacement);

            return true;
        }

        token_t get_token(QString& tok, bool skip_spaces = true)
        {
            char_t c;
            for(;;)
            {
                c = getc();
                if(c == '[')
                {
                    // recursively parse sub-tokens
                    QString const save_token(f_token);
                    if(!parse_token())
                    {
                        f_token = save_token + f_token;
                        return token_t::TOK_INVALID;
                    }
                    f_token = save_token;
                }
                else if(c != ' ' || !skip_spaces)
                {
                    break;
                }
                else
                {
                    // the space is needed in case the whole thing fails
                    f_token += QChar(c);
                }
            }
            tok = QChar(c);
            switch(c)
            {
            case '"':
            case '\'':
                {
                    char_t const quote(c);
                    tok = QChar(quote);
                    do
                    {
                        c = getc();
                        if(c == '\0')
                        {
                            return token_t::TOK_INVALID;
                        }
                        tok += QChar(c);
                        if(c == '\\')
                        {
                            c = getc();
                            if(c == '\0')
                            {
                                return token_t::TOK_INVALID;
                            }
                            tok += QChar(c);
                            // ignore quotes if escaped
                            c = '\0';
                        }
                    }
                    while(c != quote);
                    tok = tok.mid(0, tok.size() - 1) + QChar(quote);
                }
                return token_t::TOK_STRING;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                c = getc();
                while(c >= '0' && c <= '9')
                {
                    tok += QChar(c);
                    c = getc();
                }
                /*FLOWTHROUGH*/
            case '.':
                if(c == '.')
                {
                    tok += QChar(c);
                    c = getc();
                    while(c >= '0' && c <= '9')
                    {
                        tok += QChar(c);
                        c = getc();
                    }
                    ungetc(c);
                    return token_t::TOK_REAL;
                }
                ungetc(c);
                return token_t::TOK_INTEGER;

            // separators
            case ']':
            case '(':
            case ')':
            case ',':
            case '=':
                return token_t::TOK_SEPARATOR;

            case '-':
                // XXX: Should the be an error instead?
                //
                //      IMPORTANT: Do not use a throw because we do not
                //                 expect to lose control over a user
                //                 entered piece of text.
                //
                SNAP_LOG_WARNING("tokens found in on_token_filter() cannot use dash ('-') in their name; use underscore (_) instead.");
                return token_t::TOK_INVALID;

            default:
                if((c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z'))
                {
                    // identifier
                    c = getc();
                    while((c >= 'a' && c <= 'z')
                       || (c >= 'A' && c <= 'Z')
                       || (c >= '0' && c <= '9')
                       || c == '_' || c == ':')
                    {
                        tok += QChar(c);
                        c = getc();
                    }
                    // TODO: prevent use of ':' at the end of a token
                    ungetc(c);
                    return token_t::TOK_IDENTIFIER;
                }
                return token_t::TOK_INVALID;

            }
        }

        void ungets(QString const & s)
        {
            f_extra_input.remove(0, f_extra_index);
            f_extra_input.insert(0, s);

            // plugins that generate a token  replacement from a QDomDocument
            // start with the <!DOCTYPE ...> tag which we have to remove here
            if(f_extra_input.startsWith("<!DOCTYPE"))
            {
                // note that if the indexOf() fails to find the '>' then
                // it returns -1 which is fine because the + 1 will get
                // it right back to 0 which is what we want in that case
                f_extra_index = f_extra_input.indexOf('>') + 1;
            }
            else
            {
                f_extra_index = 0;
            }
        }

        void ungetc(char_t c)
        {
            f_extra_input.remove(0, f_extra_index);
            f_extra_index = 0;
            f_extra_input.insert(0, c);
        }

        // TODO: We probably want to convert UTF-32 characters as such
        //       instead of two UTF-16 encoded values.
        //
        char_t getc()
        {
            if(!f_extra_input.isEmpty())
            {
                if(f_extra_index < f_extra_input.size())
                {
                    char_t const wc(f_extra_input[f_extra_index].unicode());
                    ++f_extra_index;
                    return wc;
                }
                f_extra_index = 0;
                f_extra_input.clear();
            }
            if(f_index >= f_text.size())
            {
                return static_cast<char_t>('\0');
            }
            else
            {
                char_t const wc(f_text[f_index].unicode());
                ++f_index;
                return wc;
            }
        }

        snap_child *                f_snap;
        filter *                    f_filter;
        content::path_info_t        f_ipath;
        QDomDocument                f_xml;
        controlled_vars::mint32_t   f_index;
        controlled_vars::mint32_t   f_extra_index;
        QString                     f_text;
        QString                     f_result;
        QString                     f_token;
        QString                     f_replacement;
        QString                     f_extra_input;
        controlled_vars::mlbool_t   f_support_edit;
    };

    text_t t(f_snap, this, txt_filt.get_ipath(), txt_filt.get_xml_document(), txt_filt.get_text(), txt_filt.get_support_edit());
    if(t.parse())
    {
        txt_filt.set_text(t.result());
    }

    return true;
}


/** \brief Filter a URI for safety.
 *
 * This function transforms a URI in an acceptable string for the Snap!
 * system. The parsing is mainly to ensure valid URIs for most search
 * engines.
 *
 * \note
 * This is used to fix the URI (path really) of a page when the path
 * was specified by the client and could be tainted (not 100% given
 * by our JavaScript code--our our JS code is not quite perfect...)
 *
 * \param[in,out] uri  The URI to filter, changed in place if required.
 *
 * \return true if the filtering did not change anything.
 */
bool filter::filter_uri(QString & uri)
{
    typedef ushort char_t;

    // the system refuses some of the characters entered in the URI
    // and that should be taken as a signal for hacker's detection
    bool bad_char(false);
    QString unwanted;
    for(int i(0); i < uri.length(); ++i)
    {
        // TODO: allow other Unicode characters (i.e. accentuated characters, etc.)
        char_t c(uri.at(i).unicode());
        if(c == ' ')
        {
            // transform spaces in dashes
            uri[i] = '-';
            unwanted += QChar(c);
        }
        else if((c < '0' || c > '9')    // TODO: change to accept all Unicode characters
             && (c < 'a' || c > 'z')    // (see test in lib/snap_utf8.cpp)
             && (c < 'A' || c > 'Z')
             && c != '-' && c != '_')
        {
            // refuse controls, invalid code points, etc.
            bad_char = true;
            unwanted += QChar(c);

            // character refused
            uri.remove(i, 1);
            --i;
        }
        if(c == '-' && i > 0 && uri.at(i - 1) == '-')
        {
            // refuse '--'
            unwanted += QChar(c);
            uri.remove(i, 1);
            --i;
        }
        if((c == '-' || c == '_') && i == 0)
        {
            // refuse '-' and '_' at the beginning of the string
            unwanted += QChar(c);
            uri.remove(i, 1);
            --i;
        }
    }

    if(bad_char)
    {
        messages::messages::instance()->set_error(
            "Invalid Character",
            QString("One or more characters in the URL that you chose for your page was refused and thus your URL was changed to \"%1\".").arg(uri),
            QString("removed unwanted character(s) \"%1\"").arg(unwanted),
            false
        );
    }

    return unwanted.isEmpty();
}


/** \brief Replace special character in entities.
 *
 * This function transforms the specified text with entities instead
 * of special HTML characters. For example, the quote and less than (\<)
 * are two character that have special meaning in HTML. These get
 * transformed to corresponding entities.
 *
 * The transformation handles the following characters:
 *
 * \li " -- becomes \&quot;
 * \li \< -- becomes \&lt;
 * \li \> -- becomes \&gt;
 * \li \& -- becomes \&amp;
 * \li ' -- becomes \&#39;
 *
 * Note that " and ' are transformed because if the text is to be used
 * in an attribute, it needs to be encoded that way (although you should
 * really be using setAttribute() on a DOM to make 100% you get things
 * right in the end.)
 *
 * \param[in] text  The text to filter.
 *
 * \return A copy of the text, filtered.
 */
QString filter::encode_text_for_html(QString const & text)
{
    QString quoted(text);

    // replacing quotes is not required in plain text areas, but that way
    // the function works for both: plain text and attributes
    quoted.replace('"', "&quot;")
          .replace('<', "&lt;")
          .replace('>', "&gt;")
          .replace('&', "&amp;")
          .replace('\'', "&#39;");

    return quoted;
}


/** \brief From the body XML of a page, calculates the teaser.
 *
 * This function calculates the teaser (small snippet) of a page.
 *
 * The function removes elements and possibly words from the body
 * in order for the body to have a certain size in terms of
 * characters, words, tags...
 *
 * The changes are made directly to the input element. The \p body
 * node is expected to be a root and it does not itself get modified.
 *
 * \warning
 * You must call this function after you applied all the other
 * filters (token filterings, hashtag, external links, etc.) otherwise
 * it may end up cutting a token in half... and also it would not
 * take in account any kind of length from what the token outputs.
 *
 * \param[in,out] body  The body to tweak as per the specified \p info.
 * \param[in] info  The information used to tweak the body.
 */
void filter::body_to_teaser(QDomElement body, filter_teaser_info_t const & info)
{
    NOTUSED(body);
    NOTUSED(info);

    QDomNode n(body.firstChild());
    while(!n.isNull())
    {
        // determine the next pointer so we can delete this node
        QDomNode parent(n.parentNode());
        QDomNode next(n.firstChild());
        if(next.isNull())
        {
            next = n.nextSibling();
            if(next.isNull())
            {
                QDomNode p(parent);
                if(p == body)
                {
                    // in this case we do not walk the entire tree,
                    // instead we walk all the nodes below body.
                    break;
                }
                do
                {
                    next = p.nextSibling();
                    p = p.parentNode();
                }
                while(next.isNull() && !p.isNull());
            }
        }

#if 0
        // we want to count words in any kind of text areas
        if(n.isCDATASection())
        {
            // this works too, although the final result is still "plain text"!
            // (it must be xslt that converts the contents of CDATA sections)
            //
            // TODO: if the CDATA section includes tags, then this will not
            //       work quite as expected (i.e. it could "convert" and
            //       even break tags.)
            //
            QDomCDATASection cdata_section(n.toCDATASection());
//std::cerr << "*** CDATA section [" << cdata_section.data() << "]\n";

            //text_t t(f_snap, this, state.ipath(), state.owner(), xml, cdata_section.data());
            filter_text_t txt_filt(state.ipath(), xml, cdata_section.data());
            filter_text(txt_filt);
            if(txt_filt.has_changed())
            {
//std::cerr << "replace CDATA [" << cdata_section.data() << "]\n";
                // replace the text with its contents
                cdata_section.setData(txt_filt.get_text());
            }
        }
        else if(n.isText())
        {
            QDomText text(n.toText());
            //text_t t(f_snap, this, state.ipath(), state.owner(), xml, text.data());
            filter_text_t txt_filt(state.ipath(), xml, text.data());
            filter_text(txt_filt); //state.ipath(), xml, result, changed);
            if(txt_filt.has_changed())
            {
//SNAP_LOG_WARNING("***\n*** replace text [")(text.data())("] with [")(t.result())("]\n***\n");
                // replace the text with its contents
                snap_dom::replace_node_with_html_string(n, txt_filt.get_text());
            }
        }
#endif

        // the rest is considered to be text
        n = next;
    }
}


SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et
