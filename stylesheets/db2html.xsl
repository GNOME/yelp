<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:include href="db2html-param.xsl"/>

<xsl:include href="gettext.xsl"/>

<xsl:include href="db2html-common.xsl"/>
<xsl:include href="db2html-header.xsl"/>
<xsl:include href="db2html-title.xsl"/>

<xsl:include href="db2html-chunk.xsl"/>
<xsl:include href="db2html-html.xsl"/>
<xsl:include href="db2html-navbar.xsl"/>

<xsl:include href="db2html-suppressed.xsl"/>

<xsl:include href="db2html-division.xsl"/>
<xsl:include href="db2html-titlepage.xsl"/>

<xsl:include href="db2html-admon.xsl"/>
<xsl:include href="db2html-biblio.xsl"/>
<xsl:include href="db2html-block.xsl"/>
<xsl:include href="db2html-callout.xsl"/>
<xsl:include href="db2html-glossary.xsl"/>
<xsl:include href="db2html-index.xsl"/>
<xsl:include href="db2html-inline.xsl"/>
<xsl:include href="db2html-list.xsl"/>
<xsl:include href="db2html-media.xsl"/>
<xsl:include href="db2html-refentry.xsl"/>
<xsl:include href="db2html-table.xsl"/>
<xsl:include href="db2html-toc.xsl"/>
<xsl:include href="db2html-xref.xsl"/>

<xsl:template match="/">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="*"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="*">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template name="FIXME">
	<span class="{name(.)}">
		<span class="FIXME" style="color: red">
			<xsl:apply-templates/>
		</span>
	</span>
</xsl:template>

</xsl:stylesheet>
