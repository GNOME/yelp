<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY is-info "
(name(.) = 'appendixinfo')     or (name(.) = 'articleinfo')        or
(name(.) = 'bibliographyinfo') or (name(.) = 'blockinfo')          or
(name(.) = 'bookinfo')         or (name(.) = 'chapterinfo')        or
(name(.) = 'glossaryinfo')     or (name(.) = 'indexinfo')          or
(name(.) = 'objectinfo')       or (name(.) = 'partinfo')           or
(name(.) = 'prefaceinfo')      or (name(.) = 'refentryinfo')       or
(name(.) = 'referenceinfo')    or (name(.) = 'refsect1info')       or
(name(.) = 'refsect2info')     or (name(.) = 'refsect3info')       or
(name(.) = 'refsectioninfo')   or (name(.) = 'refsynopsisdivinfo') or
(name(.) = 'sect1info')        or (name(.) = 'sect2info')          or
(name(.) = 'sect3info')        or (name(.) = 'sect4info')          or
(name(.) = 'sect5info')        or (name(.) = 'sectioninfo')        or
(name(.) = 'setinfo')          or (name(.) = 'setindexinfo')       or
(name(.) = 'sidebarinfo')      ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template mode="title.text.mode" match="*">
	<xsl:choose>
		<xsl:when test="title">
			<xsl:value-of select="title"/>
		</xsl:when>
		<xsl:when test="(&is-info;) and ../title">
			<xsl:value-of select="../title"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/title">
			<xsl:value-of select="*[&is-info;]/title"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.header.inline.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="title.text.mode" match="refentry">
	<xsl:choose>
		<xsl:when test="refmeta/refentrytitle">
			<xsl:value-of select="refmeta/refentrytitle"/>
		</xsl:when>
		<xsl:when test="refentryinfo/title">
			<xsl:value-of select="refentryinfo/title"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.header.inline.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="titleabbrev.text.mode" match="*">
	<xsl:choose>
		<xsl:when test="titleabbrev">
			<xsl:value-of select="titleabbrev"/>
		</xsl:when>
		<xsl:when test="(&is-info;) and ../titleabbrev">
			<xsl:value-of select="../titleabbrev"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/titleabbrev">
			<xsl:value-of select="*[&is-info;]/titleabbrev"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="title.text.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="title.inline.mode" match="*">
	<xsl:choose>
		<xsl:when test="title">
			<xsl:apply-templates select="title/node()"/>
		</xsl:when>
		<xsl:when test="(&is-info;) and ../title">
			<xsl:apply-templates select="../title/node()"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/title">
			<xsl:apply-templates select="*[&is-info;]/title/node()"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.header.inline.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="title.inline.mode" match="refentry">
	<xsl:choose>
		<xsl:when test="refmeta/refentrytitle">
			<xsl:value-of select="refmeta/refentrytitle"/>
		</xsl:when>
		<xsl:when test="refentryinfo/title">
			<xsl:value-of select="refentryinfo/title"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.header.inline.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="titleabbrev.inline.mode" match="*">
	<xsl:choose>
		<xsl:when test="titleabbrev">
			<xsl:apply-templates select="titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="(&is-info;) and ../titleabbrev">
			<xsl:apply-templates select="../titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/titleabbrev">
			<xsl:apply-templates select="*[&is-info;]/titleabbrev/node()"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="title.inline.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
