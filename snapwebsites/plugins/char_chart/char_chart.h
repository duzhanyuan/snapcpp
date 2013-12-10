// Snap Websites Server -- char chart header
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
#ifndef SNAP_CHAR_CHART_H
#define SNAP_CHAR_CHART_H

#include "snap_child.h"
#include "../layout/layout.h"
#include "../sitemapxml/sitemapxml.h"

namespace snap
{
namespace char_chart
{

class char_chart : public plugins::plugin, public path::path_execute, public layout::layout_content
{
public:
                        char_chart();
                        ~char_chart();

    static char_chart * instance();
    virtual QString     description() const;
    virtual int64_t     do_update(int64_t last_updated);

    void                on_bootstrap(::snap::snap_child *snap);
    void                on_can_handle_dynamic_path(path::path *path_plugin, const QString& cpath);
    void                on_generate_sitemapxml(sitemapxml::sitemapxml *sitemap);
    virtual void        on_generate_main_content(layout::layout *l, const QString& path, QDomElement& page, QDomElement& body, const QString& ctemplate);
    bool                on_path_execute(const QString& cpath);

private:
    void                initial_update(int64_t variables_timestamp);
    void                content_update(int64_t variables_timestamp);

    zpsnap_child_t      f_snap;
};

} // namespace char_chart
} // namespace snap
#endif
// SNAP_CHAR_CHART_H
// vim: ts=4 sw=4 et
