<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:param name="stylesheet_path" select="''"/>

<xsl:param name="admon_graphics_extension" select="'.png'"/>
<xsl:param name="admon_graphics_path">
	<xsl:choose>
		<xsl:when test="$stylesheet_path != ''">
			<xsl:value-of select="concat($stylesheet_path, 'images/')"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="'./images/'"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:param>

<xsl:param name="chunk_depth" select="2"/>

<xsl:param name="classsynopsis_default_langauge" select="'idl'"/>

<xsl:param name="doc_name" select="''"/>
<xsl:param name="doc_path" select="''"/>

<xsl:param name="generate_titlepage" select="true()"/>
<xsl:param name="generate_navbar" select="true()"/>

<xsl:param name="html_extension" select="'.html'"/>

<xsl:param name="lang">
	<xsl:choose>
		<xsl:when test="/*/@lang">
			<xsl:value-of select="/*/@lang"/>
		</xsl:when>
		<xsl:when test="/*/@xml:lang">
			<xsl:value-of select="/*/@xml:lang"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>C</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:param>

<xsl:param name="mediaobject_path" select="$doc_path"/>

<xsl:param name="text_only" select="false()"/>

</xsl:stylesheet>
