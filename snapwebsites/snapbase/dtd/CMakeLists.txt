#
# File:
#      snapwebsites/snapbase/dtd/CMakeLists.txt
#
# Description:
#      Install configuration files and samples.
#
# Documentation:
#      See the CMake documentation.
#
# License:
#      Copyright (c) 2011-2017 Made to Order Software Corp.
#
#      http://snapwebsites.org/
#      contact@m2osw.com
#
#      This program is free software; you can redistribute it and/or modify
#      it under the terms of the GNU General Public License as published by
#      the Free Software Foundation; either version 2 of the License, or
#      (at your option) any later version.
#     
#      This program is distributed in the hope that it will be useful,
#      but WITHOUT ANY WARRANTY; without even the implied warranty of
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#      GNU General Public License for more details.
#     
#      You should have received a copy of the GNU General Public License
#      along with this program; if not, write to the Free Software
#      Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#


#################################################################################
## Install DTD file.
##
install(
    FILES
        simplified-xslt20.dtd

    DESTINATION
        share/snapwebsites/dtd
)
#
file(
    INSTALL
        simplified-xslt20.dtd
    DESTINATION
        ${CMAKE_BINARY_DIR}/dist/share/snapwebsites/dtd
)


# vim: ts=4 sw=4 et
