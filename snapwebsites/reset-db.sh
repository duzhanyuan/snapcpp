#!/bin/sh
#
# To be run when the server is installed globally, for production.
# This assumes that the snap* commands are in your path, and
# that the configuration files are stored under "/etc/snapwebsites".
#
# Warning: do not run this on a production server, unless you want to
# erase the entire database! This is intended for testing only.
#
HOSTNAME=127.0.0.1
PORT=9160
LAYOUTDIR=/usr/share/snapwebsites/layouts
snapdb --host ${HOSTNAME} --port ${PORT} --drop-tables
snaplayout --host ${HOSTNAME} --port ${PORT} \
	${LAYOUTDIR}/bare-body-parser.xsl ${LAYOUTDIR}/bare-theme-parser.xsl ${LAYOUTDIR}/bare-content.xml \
	${LAYOUTDIR}/white-body-parser.xsl ${LAYOUTDIR}/white-theme-parser.xsl ${LAYOUTDIR}/white-content.xml
snapserver -d --add-host -p cassandra_host=${HOSTNAME} cassandra_port=${PORT}
