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
				<xsl:value-of select="title"/>
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
			<xsl:call-template name="chunk">
				<xsl:with-param name="node" select="*"/>
			</xsl:call-template>
			<xsl:comment> Start of footer </xsl:comment>

			<xsl:call-template name="html.body.bottom">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</body>
	</html>
</xsl:template>

<xsl:template name="chunk">
	<xsl:param name="node" select="."/>
	<xsl:param name="id">
		<xsl:choose>
			<xsl:when test="$node = /*">toc</xsl:when>
			<xsl:otherwise><xsl:value-of select="$node/@id"/></xsl:otherwise>
		</xsl:choose>
	</xsl:param>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>

	<xsl:comment> Start of chunk: [<xsl:value-of select="$id"/>] </xsl:comment>
	<xsl:call-template name="html">
		<xsl:with-param name="node" select="$node"/>
		<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
	</xsl:call-template>
	<xsl:comment> End of chunk </xsl:comment>

	<xsl:if test="$generate_titlepage and ($node = /*)">
		<xsl:apply-templates mode="chunk.info.mode" select=".">
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:apply-templates>
	</xsl:if>

	<xsl:if test="$depth_chunk &lt; $chunk_depth">
		<xsl:apply-templates mode="chunk.divisions.mode" select=".">
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:apply-templates>
	</xsl:if>
</xsl:template>

<xsl:template name="html">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>

	<xsl:variable name="prevlink">
		<xsl:call-template name="navbar.prev.link">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="nextlink">
		<xsl:call-template name="navbar.next.link">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:call-template>
	</xsl:variable>

	<div>
		<xsl:call-template name="html.navbar.top">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="prevlink" select="$prevlink"/>
			<xsl:with-param name="nextlink" select="$nextlink"/>
		</xsl:call-template>

		<xsl:apply-templates select="$node">
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:apply-templates>

		<xsl:call-template name="html.navbar.bottom">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="prevlink" select="$prevlink"/>
			<xsl:with-param name="nextlink" select="$nextlink"/>
		</xsl:call-template>
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
