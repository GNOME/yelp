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
                xmlns:exsl="http://exslt.org/common"
                extension-element-prefixes="exsl"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="chunk">
	<xsl:param name="node" select="."/>
	<xsl:param name="info" select="false()"/>
	<xsl:param name="divisions" select="false()"/>
	<xsl:param name="id" select="$node/@id"/>

	<exsl:document href="{concat($id, $html_extension)}">
		<xsl:call-template name="html">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="leaf" select="
				count($node/ancestor::*[&is-division;]) &gt;= $chunk_depth"/>
		</xsl:call-template>
	</exsl:document>

	<xsl:if test="$generate_titlepage and $info and ($node = /*)">
		<xsl:apply-templates select="$info" mode="chunk.mode"/>
	</xsl:if>

	<xsl:if test="count($node/ancestor::*[&is-division;]) &lt; $chunk_depth">
		<xsl:apply-templates select="$divisions" mode="chunk.mode"/>
	</xsl:if>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="chunk.mode" match="
		appendixinfo | articleinfo  | bibliographyinfo | bookinfo       |
		chapterinfo  | glossaryinfo | indexinfo        | partinfo       |
		prefaceinfo  | refentryinfo | referenceinfo    | refsect1info   |
		refsect2info | refsect3info | refsectioninfo   | sect1info      |
		sect2info    | sect3info    | sect4info        | sect5info      |
		sectioninfo  | setinfo      | setindexinfo     ">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="id" select="'titlepage'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="appendix">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="appendixinfo"/>
		<xsl:with-param name="divisions" select="
			refentry | sect1 | section | simplesect "/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="article">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="articleinfo"/>
		<xsl:with-param name="divisions" select="
			refentry | sect1 | section | simplesect "/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="book">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="bookinfo"/>
		<xsl:with-param name="divisions" select="
			appendix | article | bibliography | chapter   | colophon | glossary |
			index    | part    | preface      | reference | setindex "/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="bibliography">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="bibliographyinfo"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="chapter">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="chapterinfo"/>
		<xsl:with-param name="divisions" select="
			refentry | sect1 | section | simplesect "/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="colophon">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="false()"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="glossary">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="glossaryinfo"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="index">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="indexinfo"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="part">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="partinfo"/>
		<xsl:with-param name="divisions" select="
			appendix | article | bibliography | chapter   | glossary |
			index    | preface | refentry     | reference "/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="preface">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="prefaceinfo"/>
		<xsl:with-param name="divisions" select="
			refentry | sect1 | section | simplesect "/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="reference">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="referenceinfo"/>
		<xsl:with-param name="divisions" select="refentry"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="refentry">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="refentryinfo"/>
		<xsl:with-param name="divisions" select="refsect1 | refsection"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="refsect1">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="refsect1info"/>
		<xsl:with-param name="divisions" select="refsect2"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="refsect2">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="refsect2info"/>
		<xsl:with-param name="divisions" select="refsect3"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="refsect3">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="refsect3info"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="refsection">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="refsectioninfo"/>
		<xsl:with-param name="divisions" select="refsection"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="sect1">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="sect1info"/>
		<xsl:with-param name="divisions" select="refentry | sect2 | simplesect"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="sect2">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="sect2info"/>
		<xsl:with-param name="divisions" select="refentry | sect3 | simplesect"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="sect3">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="sect3info"/>
		<xsl:with-param name="divisions" select="refentry | sect4 | simplesect"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="sect4">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="sect4info"/>
		<xsl:with-param name="divisions" select="refentry | sect5 | simplesect"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="sect5">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="sect5info"/>
		<xsl:with-param name="divisions" select="refentry | simplesect"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="section">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="sectioninfo"/>
		<xsl:with-param name="divisions" select="refentry | section | simplesect"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="set">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="setinfo"/>
		<xsl:with-param name="divisions" select="book | setindex"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="setindex">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="setindexinfo"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="chunk.mode" match="simplesect">
	<xsl:call-template name="chunk">
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="info" select="false()"/>
		<xsl:with-param name="divisions" select="false()"/>
	</xsl:call-template>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="chunk.id.mode" match="*">
	<xsl:choose>
		<xsl:when test="self::preface[@role = 'bookintro']">
			<xsl:apply-templates select=".." mode="chunk.id.mode"/>
		</xsl:when>
		<xsl:when test="
				(&is-division;) and
				(count(ancestor::*[&is-division;]) &lt;= $chunk_depth)">
			<xsl:value-of select="@id"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select=".." mode="chunk.id.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
