<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY is-division "
	(name(.) = 'appendix')   or (name(.) = 'article')      or
	(name(.) = 'book')       or (name(.) = 'bibliography') or
	(name(.) = 'chapter')    or (name(.) = 'colophon')     or
	(name(.) = 'glossary')   or (name(.) = 'index')        or
	(name(.) = 'part')       or (name(.) = 'preface')      or
	(name(.) = 'reference')  or (name(.) = 'refentry')     or
	(name(.) = 'refsect1')   or (name(.) = 'refsect2')     or
	(name(.) = 'refsect3')   or (name(.) = 'refsection')   or
	(name(.) = 'sect1')      or (name(.) = 'sect2')        or
	(name(.) = 'sect3')      or (name(.) = 'sect4')        or
	(name(.) = 'sect5')      or (name(.) = 'section')      or
	(name(.) = 'set')        or (name(.) = 'setindex')     or
	(name(.) = 'simplesect') ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template match="manvolnum">
	<xsl:text>(</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>)</xsl:text>
</xsl:template>

<xsl:template match="refclass">
	<div class="{name(.)}">
		<b>
			<xsl:if test="@role">
				<xsl:value-of select="@role"/>
				<xsl:text>: </xsl:text>
			</xsl:if>
		</b>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template match="refdescriptor">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="refentrytitle">
	<xsl:param name="depth" select="count(ancestor::*[&is-division;]) - 1"/>
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when test="$depth &lt; 7">
				<xsl:value-of select="concat('h', $depth + 1)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="'h7'"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:element name="{$element}">
		<xsl:attribute name="class">
			<xsl:value-of select="'refentry'"/>
		</xsl:attribute>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="refname">
	<xsl:apply-templates/>
	<xsl:if test="following-sibling::refname">
		<xsl:text>, </xsl:text>
	</xsl:if>
</xsl:template>

<xsl:template match="refnamediv">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="refpurpose">
	<xsl:text> &#8212; </xsl:text>
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="refsynopsisdiv">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:if test="title or refsynopsisdivinfo/title">
			<xsl:call-template name="node.heading"/>
		</xsl:if>
		<xsl:apply-templates select="*[
			name(.) != 'refsynopsisdivinfo' and
			name(.) != 'title'              and
			name(.) != 'subtitle'           and
			name(.) != 'titleabbrev'        ]"/>
	</div>
</xsl:template>

</xsl:stylesheet>
