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

<xsl:template match="*" mode="navbar.following.link.mode">
	<xsl:variable name="depth" select="count(ancestor::*[&is-division;])"/>
	<xsl:choose>
		<xsl:when test="not(&is-division;)">
			<xsl:apply-templates mode="navbar.following.link.mode"
				 select="ancestor::*[&is-division;][1]"/>
		</xsl:when>
		<xsl:when test="following-sibling::*[&is-division;]">
			<xsl:variable name="foll"
				select="following-sibling::*[&is-division;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$foll/@id"/>
				<xsl:with-param name="target" select="$foll"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$depth &gt; 0">
			<xsl:apply-templates select=".." mode="navbar.following.link.mode"/>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template match="*" mode="navbar.last.link.mode">
	<xsl:variable name="depth" select="count(ancestor::*[&is-division;])"/>
	<xsl:choose>
		<xsl:when test="not(&is-division;)">
			<xsl:apply-templates mode="navbar.last.link.mode"
				 select="ancestor::*[&is-division;][1]"/>
		</xsl:when>
		<xsl:when test="$depth &gt; $chunk_depth">
			<xsl:apply-templates select="." mode="navbar.prev.link.mode"/>
		</xsl:when>
		<xsl:when test="$depth = $chunk_depth">
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="@id"/>
				<xsl:with-param name="target" select="."/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="*[&is-division;]">
			<xsl:apply-templates mode="navbar.last.link.mode"
				select="*[&is-division;][last()]"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="@id"/>
				<xsl:with-param name="target" select="."/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="*" mode="navbar.prev.link.mode">
	<xsl:variable name="depth" select="count(ancestor::*[&is-division;])"/>
	<xsl:variable name="chunk_id">
		<xsl:apply-templates select="." mode="chunk.id.mode"/>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="string($chunk_id) != @id">
			<xsl:apply-templates select="id($chunk_id)" mode="navbar.prev.link.mode"/>
		</xsl:when>
		<xsl:when test="&is-info;"/>
		<xsl:when test="$depth &gt; $chunk_depth">
			<xsl:apply-templates select=".." mode="navbar.prev.link.mode"/>
		</xsl:when>
		<xsl:when test="
				($depth = $chunk_depth) and (preceding-sibling::*[&is-division;])">
			<xsl:variable name="prev" select="preceding-sibling::*[&is-division;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$prev/@id"/>
				<xsl:with-param name="target" select="$prev"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="preceding-sibling::*[&is-division;]">
			<xsl:apply-templates mode="navbar.last.link.mode"
				select="preceding-sibling::*[&is-division;][1]"/>
		</xsl:when>
		<xsl:when test="parent::*[&is-division;]">
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="../@id"/>
				<xsl:with-param name="target" select=".."/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$generate_titlepage and (. = /*) and *[&is-info;]">
			<xsl:variable name="prev" select="*[&is-info;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$prev/@id"/>
				<xsl:with-param name="target" select="$prev"/>
			</xsl:call-template>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template match="*" mode="navbar.next.link.mode">
	<xsl:variable name="depth" select="count(ancestor::*[&is-division;])"/>
	<xsl:choose>
		<xsl:when test="&is-info;">
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="../@id"/>
				<xsl:with-param name="target" select=".."/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="not(&is-division;)">
			<xsl:apply-templates mode="navbar.next.link.mode"
				 select="ancestor::*[&is-division;][1]"/>
		</xsl:when>
		<xsl:when test="$depth &gt; $chunk_depth">
			<xsl:apply-templates select="." mode="navbar.next.link.mode"/>
		</xsl:when>
		<xsl:when test="($depth &lt; $chunk_depth) and (*[&is-division;])">
			<xsl:variable name="next" select="*[&is-division;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$next/@id"/>
				<xsl:with-param name="target" select="$next"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="navbar.following.link.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

</xsl:stylesheet>
