<?xml version="1.0"?>
<!--
Snap Websites Server == Snap Software Description settings parser for our test form
Copyright (C) 2014-2017  Made to Order Software Corp.

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

	<!-- some special variables to define the theme -->
	<xsl:variable name="layout-area">oauth2-settings-parser</xsl:variable>
	<xsl:variable name="layout-modified">2015-01-23 02:51:10</xsl:variable>
	<xsl:variable name="layout-editor">oauth2-settings-page</xsl:variable>

	<xsl:template match="snap">
		<output><!-- lang="{$lang}"-->
			<div class="editor-form" form_name="oauth2_settings">
				<xsl:attribute name="session"><xsl:value-of select="page/body/editor/session/div/div/node()"/></xsl:attribute>

				<!-- xsl:if test="$action != 'edit' and $can_edit = 'yes'">
					<a class="settings-edit-button" href="?a=edit">Edit</a>
				</xsl:if>
				<xsl:if test="$action = 'edit'">
					<a class="settings-save-button" href="#">Save Changes</a>
					<a class="settings-cancel-button right-aligned" href="{/snap/head/metadata/desc[@type='page_uri']/data}">Cancel</a>
				</xsl:if-->
				<h2>OAuth2 Settings</h2>
				<div>
					<xsl:attribute name="class">test<!--xsl:if test="$action = 'edit'"> editing</xsl:if--></xsl:attribute>

					<fieldset>
						<legend>OAuth2 Access Codes</legend>

						<div class="editor-block">
							<xsl:copy-of select="page/body/oauth2/enable/node()"/>
						</div>

						<div class="editor-block">
							<label for="oauth2_email" class="settings-title">User Email</label>
							<xsl:copy-of select="page/body/oauth2/email/node()"/>
						</div>

						<div class="editor-block">
							<label for="oauth2_identifier" class="settings-title">Application Identifier</label>
							<xsl:copy-of select="page/body/oauth2/identifier/node()"/>
						</div>

						<div class="editor-block">
							<label for="oauth2_secret" class="settings-title">Application Secret</label>
							<xsl:copy-of select="page/body/oauth2/secret/node()"/>
						</div>
					</fieldset>

				</div>
			</div>

		</output>
	</xsl:template>
</xsl:stylesheet>
<!-- vim: ts=2 sw=2
-->
