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
<!ENTITY is-info "
	(name(.) = 'appendixinfo')     or (name(.) = 'articleinfo')    or
	(name(.) = 'bibliographyinfo') or (name(.) = 'bookinfo')       or
	(name(.) = 'chapterinfo')      or (name(.) = 'glossaryinfo')   or
	(name(.) = 'indexinfo')        or (name(.) = 'partinfo')       or
	(name(.) = 'prefaceinfo')      or (name(.) = 'refentryinfo')   or
	(name(.) = 'referenceinfo')    or (name(.) = 'refsect1info')   or
	(name(.) = 'refsect2info')     or (name(.) = 'refsect3info')   or
	(name(.) = 'refsectioninfo')   or (name(.) = 'sect1info')      or
	(name(.) = 'sect2info')        or (name(.) = 'sect3info')      or
	(name(.) = 'sect4info')        or (name(.) = 'sect5info')      or
	(name(.) = 'sectioninfo')      or (name(.) = 'setinfo')        or
	(name(.) = 'setindexinfo')     ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="toc">
	<xsl:call-template name="toc.verbose"/>
</xsl:template>

<!-- ======================================================================= -->
<!--
<xsl:template name="toc.simple">
	<xsl:param name="node" select="."/>
	<div class="toc">
		<xsl:call-template name="anchor">
			<xsl:with-param name="name" select="'toc'"/>
		</xsl:call-template>

		<xsl:if test="$generate_titlepage and /*/*[&is-info;]">
			<p class="about">
				<xsl:call-template name="xref">
					<xsl:with-param name="linkend" select="'titlepage'"/>
				</xsl:call-template>
			</p>
		</xsl:if>
		<xsl:call-template name="node.heading">
			<xsl:with-param name="content">
				<xsl:call-template name="gettext">
					<xsl:with-param name="key" select="'Table of Contents'"/>
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
		<ol>
			<li value="1.1">Foo</li>
			<li value="12.2">Bar</li>
		</ol>
	</div>
</xsl:template>
-->

<!-- ======================================================================= -->

<xsl:template name="toc.verbose">
	<div class="toc"><ul>
		<xsl:for-each select="*[&is-division;]">
			<xsl:variable name="chunk_id">
				<xsl:apply-templates select="." mode="chunk.id.mode"/>
			</xsl:variable>
			<xsl:if test="string($chunk_id) = @id">
				<xsl:apply-templates select="." mode="toc.verbose.mode"/>
			</xsl:if>
		</xsl:for-each>
	</ul></div>
</xsl:template>

<xsl:template match="*" mode="toc.verbose.mode">
	<li>
		<a>
			<xsl:attribute name="href">
				<xsl:call-template name="xref.target">
					<xsl:with-param name="linkend" select="@id"/>
				</xsl:call-template>
			</xsl:attribute>
			<xsl:apply-templates select="." mode="node.header.inline.mode"/>
		</a>
		<xsl:choose>
			<xsl:when test="title">
				<xsl:text>&#160;</xsl:text>
				<b><xsl:apply-templates select="title/node()"/></b>
			</xsl:when>
			<xsl:when test="*[&is-info;]/title">
				<xsl:text>&#160;</xsl:text>
				<b><xsl:apply-templates select="*[&is-info;]/title/node()"/></b>
			</xsl:when>
		</xsl:choose>
	</li>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
