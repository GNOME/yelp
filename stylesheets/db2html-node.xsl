<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
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
	(name(.) = 'setindexinfo')     or (name(.) = 'sidebarinfo')    ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- == node.heading ======================================================= -->

<xsl:template name="node.heading">
	<xsl:param name="node" select="."/>
	<xsl:param name="content" select="false()"/>
	<div class="header"><b>
		<xsl:choose>
			<xsl:when test="$content">
				<xsl:copy-of select="$content"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="$node" mode="title.inline.mode"/>
			</xsl:otherwise>
		</xsl:choose>
	</b></div>
</xsl:template>

<!-- == node.header.header.mode ============================================ -->

<!-- Defaults -->
<xsl:template mode="node.header.header.mode" match="*">
	<xsl:apply-templates select="." mode="node.name.mode"/>
</xsl:template>

<!-- Divisions -->
<xsl:template mode="node.header.header.mode" match="appendix | chapter | part">
	<xsl:call-template name="format.header.header">
		<xsl:with-param name="header">
			<xsl:apply-templates select="." mode="node.name.mode"/>
		</xsl:with-param>
		<xsl:with-param name="number">
			<xsl:apply-templates select="." mode="node.number.mode"/>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.header.mode" match="article | reference">
	<xsl:variable name="node" select="."/>
	<xsl:if test="
			(preceding-sibling::*[name(.) = name($node)]) or
			(following-sibling::*[name(.) = name($node)]) or
			(parent::part/preceding-sibling::part/*[name(.) = name($node)]) or
			(parent::part/following-sibling::part/*[name(.) = name($node)]) ">
		<xsl:call-template name="format.header.number">
			<xsl:with-param name="number">
				<xsl:apply-templates select="." mode="node.number.mode"/>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="node.header.header.mode" match="book"/>
<xsl:template mode="node.header.header.mode" match="bibliography"/>
<xsl:template mode="node.header.header.mode" match="colophon"/>
<xsl:template mode="node.header.header.mode" match="glossary"/>
<xsl:template mode="node.header.header.mode" match="index"/>
<xsl:template mode="node.header.header.mode" match="preface"/>
<xsl:template mode="node.header.header.mode" match="refentry"/>
<xsl:template mode="node.header.header.mode" match="refsect1"/>
<xsl:template mode="node.header.header.mode" match="refsect2"/>
<xsl:template mode="node.header.header.mode" match="refsect3"/>
<xsl:template mode="node.header.header.mode" match="refsection"/>

<xsl:template mode="node.header.header.mode" match="
		sect1    | sect2    | sect3      | sect4      |
		sect5    | section  | simplesect ">
	<xsl:call-template name="format.header.number">
		<xsl:with-param name="number">
			<xsl:apply-templates select="." mode="node.number.mode"/>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.header.mode" match="set"/>
<xsl:template mode="node.header.header.mode" match="setindex"/>

<!-- Infos -->
<xsl:template mode="node.header.header.mode" match="*[&is-info;]"/>

<!-- Titles -->
<xsl:template match="subtitle" mode="node.header.header.mode">
	<xsl:apply-templates select=".." mode="node.header.header.mode"/>
</xsl:template>

<xsl:template match="title" mode="node.header.header.mode">
	<xsl:apply-templates select=".." mode="node.header.header.mode"/>
</xsl:template>

<xsl:template match="titleabbrev" mode="node.header.header.mode">
	<xsl:apply-templates select=".." mode="node.header.header.mode"/>
</xsl:template>

<!-- Blocks -->
<xsl:template match="example | figure | table" mode="node.header.header.mode">
	<xsl:choose>
		<xsl:when test="@label">
			<xsl:call-template name="format.header">
				<xsl:with-param name="header" select="@label"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="format.header.header">
				<xsl:with-param name="header">
					<xsl:apply-templates select="." mode="node.name.mode"/>
				</xsl:with-param>
				<xsl:with-param name="number">
					<xsl:apply-templates select="." mode="node.number.mode"/>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- == node.header.inline.mode ============================================ -->

<!-- Defaults -->
<xsl:template mode="node.header.inline.mode" match="*">
	<xsl:apply-templates select="." mode="node.name.mode"/>
</xsl:template>

<!-- Divisions -->
<xsl:template mode="node.header.inline.mode" match="
		appendix   | chapter    | part  | refsect1 | refsect2 | refsect3 |
		refsection | sect1      | sect2 | sect3    | sect4    | sect5    |
		section    | simplesect ">
	<xsl:call-template name="format.header.inline">
		<xsl:with-param name="header">
			<xsl:apply-templates select="." mode="node.name.mode"/>
		</xsl:with-param>
		<xsl:with-param name="number">
			<xsl:apply-templates select="." mode="node.number.mode"/>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="article | reference">
	<xsl:choose>
		<xsl:when test="titleabbrev">
			<xsl:apply-templates select="titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/titleabbrev">
			<xsl:apply-templates select="*[&is-info;]/titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="title">
			<xsl:apply-templates select="title/node()"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/title">
			<xsl:apply-templates select="*[&is-info;]/title/node()"/>
		</xsl:when>
		<xsl:when test="
				(preceding-sibling::*[name(.) = name($node)]) or
				(following-sibling::*[name(.) = name($node)]) or
				(parent::part/preceding-sibling::part/*[name(.) = name($node)]) or
				(parent::part/following-sibling::part/*[name(.) = name($node)]) ">
			<xsl:call-template name="format.header.inline">
				<xsl:with-param name="header">
					<xsl:apply-templates select="." mode="node.name.mode"/>
				</xsl:with-param>
				<xsl:with-param name="number">
					<xsl:apply-templates select="." mode="node.number.mode"/>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.name.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="
		book  | bibliography | colophon | glossary |
		index | preface      | set      | setindex ">
	<xsl:choose>
		<xsl:when test="titleabbrev">
			<xsl:apply-templates select="titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/titleabbrev">
			<xsl:apply-templates select="*[&is-info;]/titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="title">
			<xsl:apply-templates select="title/node()"/>
		</xsl:when>
		<xsl:when test="*[&is-info;]/title">
			<xsl:apply-templates select="*[&is-info;]/title/node()"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.name.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="refentry">
	<xsl:choose>
		<xsl:when test="refentryinfo/titleabbrev">
			<xsl:apply-templates select="refentryinfo/titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="refentryinfo/title">
			<xsl:apply-templates select="refentryinfo/title/node()"/>
		</xsl:when>
		<xsl:when test="refsynopsisdiv/titleabbrev">
			<xsl:apply-templates select="refsynopsisdiv/titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="refsynopsisdiv/title">
			<xsl:apply-templates select="refsynopsisdiv/title/node()"/>
		</xsl:when>
		<xsl:when test="refsynopsisdiv/refsynopsisdivinfo/titleabbrev">
			<xsl:apply-templates
				select="refsynopsisdiv/refsynopsisdivinfo/titleabbrev/node()"/>
		</xsl:when>
		<xsl:when test="refsynopsisdiv/refsynopsisdivinfo/title">
			<xsl:apply-templates
				select="refsynopsisdiv/refsynopsisdivinfo/title/node()"/>
		</xsl:when>
		<xsl:when test="refnamediv/refdescriptor">
			<xsl:apply-templates select="refnamediv/refdescriptor/node()"/>
		</xsl:when>
		<xsl:when test="refnamediv/refname">
			<xsl:apply-templates select="refnamediv/refname[1]/node()"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="node.name.mode"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- Infos -->
<xsl:template mode="node.header.inline.mode" match="appendixinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Appendix'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="articleinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Article'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="bibliographyinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Bibliography'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="bookinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Book'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="chapterinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Chapter'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="glossaryinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Glossary'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="indexinfo | setindexinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Index'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="partinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Part'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="prefaceinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Preface'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="refentryinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Entry'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="referenceinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Reference'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="
		refsect1info | refsect2info | refsect3info   | refsectioninfo |
		sect1info    | sect2info    | sect3info      | sect4info      |
		sect5info    | sectioninfo  ">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.header.inline.mode" match="setinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'About This Set'"/>
	</xsl:call-template>
</xsl:template>

<!-- Titles -->
<xsl:template match="subtitle" mode="node.header.inline.mode">
	<xsl:apply-templates select=".." mode="node.header.inline.mode"/>
</xsl:template>

<xsl:template match="title" mode="node.header.inline.mode">
	<xsl:apply-templates select=".." mode="node.header.inline.mode"/>
</xsl:template>

<xsl:template match="titleabbrev" mode="node.header.inline.mode">
	<xsl:apply-templates select=".." mode="node.header.inline.mode"/>
</xsl:template>

<!-- Blocks -->
<xsl:template match="example | figure | table" mode="node.header.inline.mode">
	<xsl:choose>
		<xsl:when test="@label">
			<xsl:value-of select="@label"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="format.header.inline">
				<xsl:with-param name="header">
					<xsl:apply-templates select="." mode="node.name.mode"/>
				</xsl:with-param>
				<xsl:with-param name="number">
					<xsl:apply-templates select="." mode="node.number.mode"/>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="glossentry" mode="node.header.inline.mode">
	<xsl:apply-templates select="glossterm[1]" mode="node.header.inline.mode"/>
</xsl:template>

<xsl:template match="glossterm" mode="node.header.inline.mode">
	<xsl:apply-templates/>
</xsl:template>

<!-- == node.name.mode ===================================================== -->

<!-- Defaults -->
<xsl:template match="*" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Unknown'"/>
	</xsl:call-template>
</xsl:template>

<!-- Divisions -->
<xsl:template match="appendix" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Appendix'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="article" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Article'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="book" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Book'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="bibliography" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Bigliography'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="chapter" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Chapter'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="colophon" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Colophon'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="glossary" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Glossary'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="index" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Index'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="part" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Part'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="preface" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Preface'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="reference" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Reference'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="refentry" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Reference Entry'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="refsect1" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Reference Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="refsect2" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Reference Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="refsect3" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Reference Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="refsection" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Reference Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="sect1" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="sect2" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="sect3" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="sect4" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="sect5" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="section" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="set" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Set'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="setindex" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Set Index'"/>
	</xsl:call-template>
</xsl:template>

<!-- Titles -->
<xsl:template match="subtitle" mode="node.name.mode">
	<xsl:apply-templates select=".." mode="node.name.mode"/>
</xsl:template>

<xsl:template match="title" mode="node.name.mode">
	<xsl:apply-templates select=".." mode="node.name.mode"/>
</xsl:template>

<xsl:template match="titleabbrev" mode="node.name.mode">
	<xsl:apply-templates select=".." mode="node.name.mode"/>
</xsl:template>

<!-- Blocks -->
<xsl:template match="caution" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Caution'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="dedication" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Dedication'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="example" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Example'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="figure" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Figure'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="important" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Important'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="msgaud" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Message Audience'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="msglevel" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Message Level'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="msgorig" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Message Origin'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="note" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Note'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="table" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Table'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="tip" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Tip'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="warning" mode="node.name.mode">
	<xsl:call-template name="gettext">
		<xsl:with-param name="key" select="'Warning'"/>
	</xsl:call-template>
</xsl:template>

<!-- == node.number.mode =================================================== -->

<!-- Defaults -->
<xsl:template match="*" mode="node.number.mode">
	<xsl:apply-templates select=".." mode="node.number.mode"/>
</xsl:template>

<!-- Divisions -->
<xsl:template mode="node.number.mode" match="appendix">
	<xsl:call-template name="format.appendix.number">
		<xsl:with-param name="appendix" select="
			count(preceding-sibling::appendix) + 1 +
			count(parent::part/preceding-sibling::part/appendix)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.number.mode" match="article">
	<xsl:call-template name="format.article.number">
		<xsl:with-param name="article" select="
			count(preceding-sibling::article) + 1 +
			count(parent::part/preceding-sibling::part/article)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.number.mode" match="book"/>
<xsl:template mode="node.number.mode" match="bibliography"/>
<xsl:template mode="node.number.mode" match="colophon"/>

<xsl:template mode="node.number.mode" match="chapter">
	<xsl:call-template name="format.chapter.number">
		<xsl:with-param name="chapter" select="
			count(preceding-sibling::chapter) + 1 +
			count(parent::part/preceding-sibling::part/chapter)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.number.mode" match="glossary"/>
<xsl:template mode="node.number.mode" match="index"/>

<xsl:template mode="node.number.mode" match="part">
	<xsl:call-template name="format.part.number">
		<xsl:with-param name="part" select="
			count(preceding-sibling::part) + 1"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.number.mode" match="preface"/>

<xsl:template mode="node.number.mode" match="reference">
	<xsl:call-template name="format.reference.number">
		<xsl:with-param name="reference" select="
			count(preceding-sibling::reference) + 1 +
			count(parent::part/preceding-sibling::part/reference)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.number.mode" match="
		refentry | refsect1   | refsect2 | refsect3 | refsection |
		sect1    | sect2      | sect3    | sect4    | sect5      |
		section  | simplesect ">
	<xsl:variable name="sect" select="."/>
	<xsl:call-template name="format.section.number">
		<xsl:with-param name="parent">
			<xsl:apply-templates select=".." mode="node.number.mode"/>
		</xsl:with-param>
		<xsl:with-param name="section" select="
			count(preceding-sibling::*[name(.) = name($sect)]) + 1"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="node.number.mode" match="set"/>
<xsl:template mode="node.number.mode" match="setindex"/>

<!-- Titles -->
<xsl:template match="subtitle" mode="node.number.mode">
	<xsl:apply-templates select=".." mode="node.number.mode"/>
</xsl:template>

<xsl:template match="title" mode="node.number.mode">
	<xsl:apply-templates select=".." mode="node.number.mode"/>
</xsl:template>

<xsl:template match="titleabbrev" mode="node.number.mode">
	<xsl:apply-templates select=".." mode="node.number.mode"/>
</xsl:template>

<!-- Blocks -->
<xsl:template mode="node.number.mode" match="example | figure | table">
	<xsl:variable name="parent">
		<xsl:choose>
			<xsl:when test="
					ancestor::*[
						(name(.) = 'refentry') or (name(.) = 'sect1')      or
						(name(.) = 'section')  or (name(.) = 'simplesect') ]
					[parent::article]">
				<xsl:apply-templates mode="node.number.mode" select="
					ancestor::*[
						(name(.) = 'refentry') or (name(.) = 'sect1')      or
						(name(.) = 'section')  or (name(.) = 'simplesect') ]
					[parent::article][last()]"/>
			</xsl:when>
			<xsl:when test="ancestor::*[name(.) = 'appendix' or name(.) = 'chapter']">
				<xsl:apply-templates mode="node.number.mode" select="
					ancestor::*[name(.) = 'appendix' or name(.) = 'chapter'][1]"/>
			</xsl:when>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="num">
		<xsl:choose>
			<xsl:when test="
					ancestor::*[
						(name(.) = 'refentry') or (name(.) = 'sect1')      or
						(name(.) = 'section')  or (name(.) = 'simplesect') ]
					[parent::article]">
				<xsl:value-of select="count(
					ancestor-or-self::*[ancestor::*[
						(name(.) = 'refentry') or (name(.) = 'sect1')      or
						(name(.) = 'section')  or (name(.) = 'simplesect') ]
							[parent::article]
					]/preceding-sibling::*/descendant-or-self::*[
						name(.) = name(current())]
					) + 1"/>
			</xsl:when>
			<xsl:when test="ancestor::*[name(.) = 'appendix' or name(.) = 'chapter']">
				<xsl:value-of select="count(
					ancestor-or-self::*[ancestor::*[
						name(.) = 'appendix' or name(.) = 'chapter' ]
					]/preceding-sibling::*/descendant-or-self::*[
						name(.) = name(current())]
					) + 1"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="count(preceding::*[name(.) = name(current())]) + 1"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="not($parent)">
			<xsl:value-of select="$num"/>
		</xsl:when>
		<xsl:when test="name(.) = 'example'">
			<xsl:call-template name="format.example.number">
				<xsl:with-param name="parent" select="$parent"/>
				<xsl:with-param name="example" select="$num"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="name(.) = 'figure'">
			<xsl:call-template name="format.figure.number">
				<xsl:with-param name="parent" select="$parent"/>
				<xsl:with-param name="figure" select="$num"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="name(.) = 'table'">
			<xsl:call-template name="format.table.number">
				<xsl:with-param name="parent" select="$parent"/>
				<xsl:with-param name="table" select="$num"/>
			</xsl:call-template>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
