<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template match="biblioentry/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="bibliomixed/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="bibliomset/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="biblioset/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="constraintdef/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="msgexplan/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="msgmain/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="productionset/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="qandadiv/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="qandaset/title">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="biblioentry/subtitle">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="bibliomixed/subtitle">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="bibliomset/subtitle">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="biblioset/subtitle">
	<xsl:call-template name="FIXME"/>
</xsl:template>
<xsl:template match="refsynopsisdiv/subtitle">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == title.simple ======================================================= -->

<xsl:template name="title.simple" match="
		abstract/title			| authorblurb/title		| blockquote/title		|
		calloutlist/title		| caution/title			| dedication/title		|
		important/title		| formalpara/title		| itemizedlist/title		|
		note/title				| orderedlist/title		| personblurb/title		|
		procedure/title		| refsynopsisdiv/title	| segmentedlist/title	|
		sidebar/title			| step/title				| tip/title					|
		variablelist/title	| warning/title			">
	<div class="{name(.)}"><b><xsl:apply-templates/></b></div>
</xsl:template>

<xsl:template match="dedication/subtitle">
	<xsl:call-template name="title.simple"/>
</xsl:template>

<!-- == title.header ======================================================= -->

<xsl:template name="title.header" match="
		equation/title | example/title | figure/title | msg/title   |
		msgrel/title   | msgset/title  | msgsub/title | table/title ">
	<div class="{name(.)}">
		<i>
			<xsl:call-template name="header.prefix"/>
		</i>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<!-- == title.h ============================================================ -->

<xsl:template name="title.h" match="
		appendix/title			| article/title		| bibliodiv/title		|
		bibliography/title	| book/title			| chapter/title		|
		colophon/title			| glossary/title		| glossdiv/title		|
		index/title				| indexdiv/title		| part/title			|
		partintro/title		| preface/title		| reference/title		|
		refsect1/title			| refsect2/title		| refsect3/title		|
		refsection/title		| sect1/title			| sect2/title			|
		sect3/title				| sect4/title			| sect5/title			|
		section/title			| set/title				| setindex/title		|
		simplesect/title		">
	<xsl:param name="depth_in_chunk">
		<xsl:call-template name="depth.in.chunk"/>
	</xsl:param>
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when test="$depth_in_chunk &lt;= 7">
				<xsl:value-of select="concat('h', $depth_in_chunk)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="'h7'"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:element name="{$element}">
		<xsl:attribute name="class">
			<xsl:value-of select="name(..)"/>
		</xsl:attribute>
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="header.prefix"/>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<!-- == subtitle.h ========================================================= -->

<xsl:template name="subtitle.h" match="
		appendix/subtitle			| article/subtitle		| bibliodiv/subtitle		|
		bibliography/subtitle	| book/subtitle			| chapter/subtitle		|
		colophon/subtitle			| glossary/subtitle		| glossdiv/subtitle		|
		index/subtitle				| indexdiv/subtitle		| part/subtitle			|
		partintro/subtitle		| preface/subtitle		| reference/subtitle		|
		refsect1/subtitle			| refsect2/subtitle		| refsect3/subtitle		|
		refsection/subtitle		| sect1/subtitle			| sect2/subtitle			|
		sect3/subtitle				| sect4/subtitle			| sect5/subtitle			|
		section/subtitle			| set/subtitle				| setindex/subtitle		|
		simplesect/subtitle		">
	<xsl:param name="depth_in_chunk">
		<xsl:call-template name="depth.in.chunk"/>
	</xsl:param>
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when test="$depth_in_chunk &lt;= 6">
				<xsl:value-of select="concat('h', $depth_in_chunk + 1)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="'h7'"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:element name="{$element}">
		<xsl:attribute name="class">
			<xsl:value-of select="name(.)"/>
		</xsl:attribute>
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="header.prefix"/>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
