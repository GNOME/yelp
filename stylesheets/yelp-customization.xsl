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
                version='1.0'>

<xsl:include href="db2html.xsl"/>

<xsl:template match="/">
	<xsl:variable name="node" select="*"/>
	<html>
		<head>
			<title>
				<xsl:apply-templates select="$node" mode="title.text.mode"/>
			</title>
			<style type="text/css">
				<xsl:call-template name="html.css">
					<xsl:with-param name="node" select="$node"/>
				</xsl:call-template>
			</style>
			<xsl:call-template name="html.head">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</head>
		<body>
			<xsl:call-template name="html.body.attributes">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
			<xsl:call-template name="html.body.top">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>

			<xsl:comment> End of header </xsl:comment>
			<xsl:apply-templates select="book | article" mode="chunk.mode"/>
			<xsl:comment> Start of footer </xsl:comment>

			<xsl:call-template name="html.body.bottom">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</body>
	</html>
</xsl:template>

<xsl:template name="chunk">
	<xsl:param name="node" select="."/>
	<xsl:param name="info" select="false()"/>
	<xsl:param name="divisions" select="false()"/>
	<xsl:param name="id">
		<xsl:choose>
			<xsl:when test="$node = /*">
				<xsl:text>toc</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$node/@id"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:param>

	<xsl:comment> Start of chunk: [<xsl:value-of select="$id"/>] </xsl:comment>
	<xsl:call-template name="html">
		<xsl:with-param name="node" select="$node"/>
		<xsl:with-param name="leaf" select="
			count($node/ancestor::*[&is-division;]) &gt;= $chunk_depth"/>
	</xsl:call-template>
	<xsl:comment> End of chunk </xsl:comment>

	<xsl:if test="$generate_titlepage and $info and ($node = /*)">
		<xsl:apply-templates select="$info" mode="chunk.mode"/>
	</xsl:if>

	<xsl:if test="count($node/ancestor::*[&is-division;]) &lt; $chunk_depth">
		<xsl:apply-templates select="$divisions" mode="chunk.mode"/>
	</xsl:if>
</xsl:template>

<xsl:template name="html">
	<xsl:param name="node" select="."/>
	<xsl:param name="leaf" select="true()"/>
	<xsl:variable name="prevlink">
		<xsl:apply-templates select="$node" mode="navbar.prev.link.mode"/>
	</xsl:variable>
	<xsl:variable name="nextlink">
		<xsl:apply-templates select="$node" mode="navbar.next.link.mode"/>
	</xsl:variable>

	<div>
		<table width="100%" cellpadding="0" cellspacing="0">
			<tr width="100%">
				<td width="50%" style="text-align: left;">
					<xsl:copy-of select="$prevlink"/>
				</td>
				<td width="50%" style="text-align: right;">
					<xsl:copy-of select="$nextlink"/>
				</td>
			</tr>
		</table>
		<hr/>

		<xsl:apply-templates select="$node">
			<xsl:with-param name="depth" select="0"/>
			<xsl:with-param name="leaf" select="$leaf"/>
		</xsl:apply-templates>

		<hr/>
		<table width="100%" cellpadding="0" cellspacing="0">
			<tr width="100%">
				<td width="50%" style="text-align: left;">
					<xsl:copy-of select="$prevlink"/>
				</td>
				<td width="50%" style="text-align: right;">
					<xsl:copy-of select="$nextlink"/>
				</td>
			</tr>
		</table>
	</div>
</xsl:template>

<xsl:template name="xref.target">
	<xsl:param name="linkend" select="@linkend"/>
	<xsl:param name="target" select="id($linkend)"/>
	<xsl:variable name="chunk_id">
		<xsl:apply-templates select="$target" mode="chunk.id.mode"/>
	</xsl:variable>
	<xsl:text>ghelp:</xsl:text>
	<xsl:value-of select="$doc_name"/>
	<xsl:text>?</xsl:text>
	<xsl:value-of select="$chunk_id"/>
</xsl:template>

</xsl:stylesheet>
