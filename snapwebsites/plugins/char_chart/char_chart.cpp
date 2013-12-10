// Snap Websites Server -- display a character map
// Copyright (C) 2012-2013  Made to Order Software Corp.
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

#include "char_chart.h"
#include "../content/content.h"
#include <iostream>
#include <QtCore/QDebug>


SNAP_PLUGIN_START(char_chart, 1, 0)



/** \brief Initialize the char_chart plugin.
 *
 * This function is used to initialize the char_chart plugin object.
 */
char_chart::char_chart()
    //: f_snap(NULL) -- auto-init
{
}

/** \brief Clean up the char_chart plugin.
 *
 * Ensure the char_chart object is clean before it is gone.
 */
char_chart::~char_chart()
{
}

/** \brief Initialize the char_chart plugin.
 *
 * This function terminates the initialization of the char_chart plugin
 * by registering for different events.
 *
 * \param[in] snap  The child handling this request.
 */
void char_chart::on_bootstrap(::snap::snap_child *snap)
{
    f_snap = snap;

    SNAP_LISTEN(char_chart, "path", path::path, can_handle_dynamic_path, _1, _2);
    SNAP_LISTEN(char_chart, "sitemapxml", sitemapxml::sitemapxml, generate_sitemapxml, _1);
}

/** \brief Get a pointer to the char_chart plugin.
 *
 * This function returns an instance pointer to the char_chart plugin.
 *
 * Note that you cannot assume that the pointer will be valid until the
 * bootstrap event is called.
 *
 * \return A pointer to the char_chart plugin.
 */
char_chart *char_chart::instance()
{
    return g_plugin_char_chart_factory.instance();
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
QString char_chart::description() const
{
    return "This dynamically generates tables of characters.";
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
int64_t char_chart::do_update(int64_t last_updated)
{
    SNAP_PLUGIN_UPDATE_INIT();

//std::cerr << "Got the do_update() in char_chart! "
//        << static_cast<int64_t>(last_updated) << ", "
//        << static_cast<int64_t>(SNAP_UNIX_TIMESTAMP(2012, 1, 1, 0, 0, 0) * 1000000LL) << "\n";

    SNAP_PLUGIN_UPDATE(2012, 1, 1, 0, 0, 0, initial_update);
    SNAP_PLUGIN_UPDATE(2012, 1, 1, 0, 0, 0, content_update);

    SNAP_PLUGIN_UPDATE_EXIT();
}

/** \brief First update to run for the char_chart plugin.
 *
 * This function is the first update for the char_chart plugin. It installs
 * the initial data required by the char_chart plugin.
 *
 * \param[in] variables_timestamp  The timestamp for all the variables added to the database by this update (in micro-seconds).
 */
void char_chart::initial_update(int64_t variables_timestamp)
{
}

/** \brief Update the char_chart plugin content.
 *
 * This function updates the contents in the database using the
 * system update settings found in the resources.
 *
 * This file, contrary to most content files, makes heavy use
 * of the overwrite flag to make sure that the basic system
 * types are and stay defined as expected.
 *
 * \param[in] variables_timestamp  The timestamp for all the variables added to the database by this update (in micro-seconds).
 */
void char_chart::content_update(int64_t variables_timestamp)
{
    content::content::instance()->add_xml("char_chart");
}


/** \brief Check whether \p cpath matches our introducer.
 *
 * This function checks that cpath matches our introducer and if
 * so we tell the path plugin that we're taking control to
 * manage this path.
 *
 * \param[in] path_plugin  A pointer to the path plugin.
 * \param[in] cpath  The path being handled dynamically.
 */
void char_chart::on_can_handle_dynamic_path(path::path *path_plugin, const QString& cpath)
{
    if(cpath.left(11) == "char-chart/")
    {
        // tell the path plugin that this is ours
        path_plugin->handle_dynamic_path("char_chart", this);
    }
}


/** \brief Execute the specified path.
 *
 * This is a dynamic page.
 *
 * \param[in] cpath  The canonalized path.
 */
bool char_chart::on_path_execute(const QString& cpath)
{
    f_snap->output(layout::layout::instance()->apply_layout(cpath, this));

    return true;
}


void char_chart::on_generate_main_content(layout::layout *l, const QString& cpath, QDomElement& page, QDomElement& body, const QString& ctemplate)
{
    QDomDocument doc(page.ownerDocument());

    QString value(cpath.mid(11));
    if(value.length() < 1)
    {
        // problem
        return;// false;
    }
    bool ok;
    int chart_page(value.toInt(&ok, 16));
    if(!ok)
    {
        // invalid hex
        return;// false;
    }
    // change to a page
    int c(chart_page << 8);

    // range limit
    if(c < 0 || c >= 0x100000)
    {
        // not an exact page
        return;// false;
    }

    // create table
    //f_snap->output("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">...
    //  ...<html><head><title>Char Chart</title><style>td{text-align:center;}.character{text-size:150%;color:#0044cc;}</style></head><body>");
    QDomElement titles(doc.createElement("titles"));
    body.appendChild(titles);
    QDomElement title(doc.createElement("short-title"));
    titles.appendChild(title);
    QDomText title_text(doc.createTextNode("Char Chart"));
    title.appendChild(title_text);
    title = doc.createElement("title");
    titles.appendChild(title);
    title_text = doc.createTextNode(QString("Char Chart (%1)").arg(chart_page));
    title.appendChild(title_text);
    title = doc.createElement("long-title");
    titles.appendChild(title);
    title_text = doc.createTextNode(QString("Unicode Char Chart (Page: %1)").arg(chart_page));
    title.appendChild(title_text);

    QDomElement content_tag(doc.createElement("content"));
    body.appendChild(content_tag);
    //f_snap->output("<table cellpadding=\"5\" cellspacing=\"0\" border=\"1\">");
    QDomElement table_tag(doc.createElement("table"));
    content_tag.appendChild(table_tag);
    table_tag.setAttribute("cellpadding", 5);
    table_tag.setAttribute("cellspacing", 0);
    table_tag.setAttribute("border", 1);
    {
        //f_snap->output("<tr><th>&nbsp;</th>");
        QDomElement tr_tag(doc.createElement("tr"));
        table_tag.appendChild(tr_tag);
        QDomElement th_tag(doc.createElement("th"));
        tr_tag.appendChild(th_tag);
        QDomText text(doc.createTextNode("\xA0"));
        th_tag.appendChild(text);
        for(int i = 0; i < 16; ++i)
        {
            //f_snap->output("<th>" + QString("%1").arg(i) + "</th>");
            th_tag = doc.createElement("th");
            tr_tag.appendChild(th_tag);
            text = doc.createTextNode(QString("%1").arg(i));
            th_tag.appendChild(text);
        }
        //f_snap->output("</tr>");
    }
    for(int i = 0; i < 16; ++i)
    {
        //f_snap->output("<tr><th>" + QString("%1").arg(i) + "</th>");
        QDomElement tr_tag(doc.createElement("tr"));
        table_tag.appendChild(tr_tag);
        QDomElement th_tag(doc.createElement("th"));
        tr_tag.appendChild(th_tag);
        QDomText text(doc.createTextNode(QString("%1").arg(i)));
        th_tag.appendChild(text);
        for(int j = 0; j < 16; ++j)
        {
            //f_snap->output("<td><span class='character'>");
            QDomElement td_tag(doc.createElement("td"));
            tr_tag.appendChild(td_tag);
            QDomElement span_tag(doc.createElement("span"));
            td_tag.appendChild(span_tag);
            span_tag.setAttribute("class", "character");
            QString character;
            switch(c) {
            case 0x00:
                character = "NUL";
                break;

            case 0x01:
                character = "SOH";
                break;

            case 0x02:
                character = "STX";
                break;

            case 0x03:
                character = "ETX";
                break;

            case 0x04:
                character = "EOT";
                break;

            case 0x05:
                character = "ENQ";
                break;

            case 0x06:
                character = "ACK";
                break;

            case 0x07:
                character = "BEL";
                break;

            case 0x08:
                character = "BS";
                break;

            case 0x09:
                character = "HT";
                break;

            case 0x0A:
                character = "LF";
                break;

            case 0x0B:
                character = "VT";
                break;

            case 0x0C:
                character = "FF";
                break;

            case 0x0D:
                character = "CR";
                break;

            case 0x0E:
                character = "SO";
                break;

            case 0x0F:
                character = "SI";
                break;

            case 0x10:
                character = "DLE";
                break;

            case 0x11:
                character = "DC1";
                break;

            case 0x12:
                character = "DC2";
                break;

            case 0x13:
                character = "DC3";
                break;

            case 0x14:
                character = "DC4";
                break;

            case 0x15:
                character = "NAK";
                break;

            case 0x16:
                character = "SYN";
                break;

            case 0x17:
                character = "ETB";
                break;

            case 0x18:
                character = "CAN";
                break;

            case 0x19:
                character = "EM";
                break;

            case 0x1A:
                character = "SUB";
                break;

            case 0x1B:
                character = "ESC";
                break;

            case 0x1C:
                character = "FS";
                break;

            case 0x1D:
                character = "GS";
                break;

            case 0x1E:
                character = "RS";
                break;

            case 0x1F:
                character = "US";
                break;

            case 0x20:
                character = "SP";
                break;

            case 0x7F:
                character = "DEL";
                break;

            case 0x80:
                character = "XXX";
                break;

            case 0x81:
                character = "XXX";
                break;

            case 0x82:
                character = "BPH";
                break;

            case 0x83:
                character = "NBH";
                break;

            case 0x84:
                character = "IND";
                break;

            case 0x85:
                character = "NEL";
                break;

            case 0x86:
                character = "SSA";
                break;

            case 0x87:
                character = "ESA";
                break;

            case 0x88:
                character = "HTS";
                break;

            case 0x89:
                character = "HTJ";
                break;

            case 0x8A:
                character = "VTS";
                break;

            case 0x8B:
                character = "PLD";
                break;

            case 0x8C:
                character = "PLU";
                break;

            case 0x8D:
                character = "RI";
                break;

            case 0x8E:
                character = "SS2";
                break;

            case 0x8F:
                character = "SS3";
                break;

            case 0x90:
                character = "DCS";
                break;

            case 0x91:
                character = "PU1";
                break;

            case 0x92:
                character = "PU2";
                break;

            case 0x93:
                character = "STS";
                break;

            case 0x94:
                character = "CCH";
                break;

            case 0x95:
                character = "MW";
                break;

            case 0x96:
                character = "SPA";
                break;

            case 0x97:
                character = "EPA";
                break;

            case 0x98:
                character = "SOS";
                break;

            case 0x99:
                character = "XXX";
                break;

            case 0x9A:
                character = "SCI";
                break;

            case 0x9B:
                character = "CSI";
                break;

            case 0x9C:
                character = "ST";
                break;

            case 0x9D:
                character = "OSC";
                break;

            case 0x9E:
                character = "PM";
                break;

            case 0x9F:
                character = "APC";
                break;

            case 0xA0:
                character = "NBSP";
                break;

            case 0xAD:
                {
                    //character = "<sup>S<u>H</u>Y</sup>";
                    QDomElement sup(doc.createElement("sup"));
                    span_tag.appendChild(sup);
                    QDomText s(doc.createTextNode("S"));
                    sup.appendChild(s);
                    QDomElement u(doc.createElement("u"));
                    QDomText h(doc.createTextNode("H"));
                    u.appendChild(h);
                    sup.appendChild(u);
                    QDomText y(doc.createTextNode("Y"));
                    sup.appendChild(y);
                }
                break;

            default:
                {
                    //character = "&#" + QString("%1").arg(c) + ";";
                    QDomEntityReference ref(doc.createEntityReference("#" + QString("%1").arg(c)));
                    span_tag.appendChild(ref);
                }
                break;

            }
            if(!character.isEmpty())
            {
                text = doc.createTextNode(character);
                span_tag.appendChild(text);
            }

            //f_snap->output(QString("</span><br/><small>%1</small><br/><small>%2</small></td>").arg(c, 4, 16, QChar('0')).arg(c));
            QDomElement br_tag(doc.createElement("br"));
            td_tag.appendChild(br_tag);
            QDomElement small_tag(doc.createElement("small"));
            td_tag.appendChild(small_tag);
            text = doc.createTextNode(QString("%1").arg(c, 4, 16, QChar('0')));
            small_tag.appendChild(text);
            br_tag = doc.createElement("br");
            td_tag.appendChild(br_tag);
            small_tag = doc.createElement("small");
            td_tag.appendChild(small_tag);
            text = doc.createTextNode(QString("%1").arg(c));
            small_tag.appendChild(text);

            ++c;
        }
        //f_snap->output("</tr>");
    }
    //f_snap->output("</table>");
    QDomElement p_tag(doc.createElement("p"));
    content_tag.appendChild(p_tag);

    if(chart_page != 0)
    {
        //f_snap->output(QString("<p><a href='snap.cgi?q=/char-chart/%1'>Previous</a></p>").arg(chart_page - 1));
        QDomElement a_tag(doc.createElement("a"));
        p_tag.appendChild(a_tag);
        a_tag.setAttribute("href", f_snap->snap_url(QString("/char-chart/%1").arg(chart_page - 1)));
        QDomText text(doc.createTextNode("Previous"));
        a_tag.appendChild(text);
    }
    if(chart_page != 0x0FFFFF)
    {
        QDomText text;
        if(chart_page != 0)
        {
            // add a space between both links
            text = doc.createTextNode(" ");
            p_tag.appendChild(text);
        }
        //f_snap->output(QString("<p><a href='snap.cgi?q=/char-chart/%1'>Next</a></p>").arg(chart_page + 1));
        QDomElement a_tag(doc.createElement("a"));
        p_tag.appendChild(a_tag);
        a_tag.setAttribute("href", f_snap->snap_url(QString("/char-chart/%1").arg(chart_page + 1)));
        text = doc.createTextNode("Next");
        a_tag.appendChild(text);
    }
    //f_snap->output("</body></html>");

    //return true;
}

/** \brief Give access to the first page.
 *
 * This adds the first page of all the charts in the XML sitemap.
 *
 * \param[in] sitemap  The sitemap plugin pointer.
 */
void char_chart::on_generate_sitemapxml(sitemapxml::sitemapxml *sitemap)
{
    sitemapxml::sitemapxml::url_info url;
    QString site_key(f_snap->get_site_key_with_slash());
    url.set_uri(site_key + "char_chart/0");
    url.set_last_modification(SNAP_UNIX_TIMESTAMP(2012, 1, 1, 0, 0, 0) * 1000000);
    url.set_priority(0.01f);
    url.set_frequency(0);
    sitemap->add_url(url);
}


SNAP_PLUGIN_END()

// vim: ts=4 sw=4 et
