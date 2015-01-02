<?xml version="1.0"?>
<!--
Snap Websites Server == avatar setting page data to XML
Copyright (C) 2011-2015  Made to Order Software Corp.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
-->
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
															xmlns:xs="http://www.w3.org/2001/XMLSchema"
															xmlns:fn="http://www.w3.org/2005/xpath-functions"
															xmlns:snap="snap:snap">
	<!-- include system data -->
	<xsl:include href="functions"/>

	<!-- some special variables to define the theme -->
	<xsl:variable name="layout-area">avatar-settings-parser</xsl:variable>
	<xsl:variable name="layout-modified">2015-01-02 04:19:36</xsl:variable>
	<xsl:variable name="layout-editor">avatar-settings-page</xsl:variable>

	<xsl:template match="snap">
		<output><!-- lang="{$lang}"-->
			<div id="content" class="editor-form" form_name="avatar">
				<xsl:attribute name="session"><xsl:copy-of select="page/body/editor/session/div/div/node()"/></xsl:attribute>

				<h2>Avatar Settings</h2>
				<div>
					<xsl:attribute name="class">test<!--xsl:if test="$action = 'edit'"> editing</xsl:if--></xsl:attribute>

					<fieldset>
						<legend>Local Avatars</legend>

						<div class="editor-block">
							<xsl:copy-of select="page/body/avatar/local_images/node()"/>
						</div>

						<div class="editor-block">
							<xsl:copy-of select="page/body/avatar/share_images/node()"/>
						</div>

						<div class="editor-block">
							<xsl:copy-of select="page/body/avatar/snap_images/node()"/>
						</div>
					</fieldset>

					<fieldset>
						<legend>External Avatars</legend>

						<div class="editor-block">
							<xsl:copy-of select="page/body/avatar/gravatar_images/node()"/>
						</div>
					</fieldset>

				</div>
			</div>

		</output>
	</xsl:template>
</xsl:stylesheet>
<!-- vim: ts=2 sw=2
-->
