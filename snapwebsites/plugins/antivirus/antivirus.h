// Snap Websites Server -- anti-virus verifies uploaded files cleanliness
// Copyright (C) 2014-2015  Made to Order Software Corp.
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
#pragma once

#include "../content/content.h"
#include "../form/form.h"
#include "../versions/versions.h"

namespace snap
{
namespace antivirus
{


enum class name_t
{
    SNAP_NAME_ANTIVIRUS_SCAN_ARCHIVE
};
char const *get_name(name_t name) __attribute__ ((const));


class antivirus_exception : public snap_exception
{
public:
    antivirus_exception(char const *        what_msg) : snap_exception("Anti-Virus", what_msg) {}
    antivirus_exception(std::string const & what_msg) : snap_exception("Anti-Virus", what_msg) {}
    antivirus_exception(QString const &     what_msg) : snap_exception("Anti-Virus", what_msg) {}
};



class antivirus
        : public plugins::plugin
        , public layout::layout_content
{
public:
    static sessions::sessions::session_info::session_id_t const ANTIVIRUS_SESSION_ID_SETTINGS = 1;      // settings-form.xml

                            antivirus();
                            ~antivirus();

    // plugins::plugin implementation
    static antivirus *      instance();
    virtual QString         description() const;
    virtual int64_t         do_update(int64_t last_updated);
    void                    on_bootstrap(snap_child * snap);

    // layout::layout_content implementation
    virtual void            on_generate_main_content(content::path_info_t & path, QDomElement & page, QDomElement & body);

    // content signals
    void                    on_check_attachment_security(content::attachment_file const & file, content::permission_flag & secure, bool const fast);

    // versions signals
    void                    on_versions_tools(filter::filter::token_info_t & token);

private:
    void                    content_update(int64_t variables_timestamp);

    zpsnap_child_t          f_snap;
};


} // namespace antivirus
} // namespace snap
// vim: ts=4 sw=4 et
