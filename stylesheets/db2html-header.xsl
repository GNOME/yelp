<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                extension-element-prefixes="yelp"
                version="1.0">

<!-- == header.prefix ====================================================== -->

<!-- header.prefix -->
<xsl:template name="header.prefix">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="$node/@label">
			<xsl:value-of select="$node/@label"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates mode="header.prefix.mode" select="$node"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- header.prefix.named -->
<xsl:template name="header.prefix.named" mode="header.prefix.mode" match="
		appendix | chapter | example | figure | part | table">
	<xsl:call-template name="format.header.prefix.named">
		<xsl:with-param name="header">
			<xsl:call-template name="header"/>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<!-- header.prefix.unnamed -->
<xsl:template name="header.prefix.unnamed" mode="header.prefix.mode" match="
		sect1 | sect2 | sect3 | sect4 | sect5 | section | simplesect ">
	<xsl:call-template name="format.header.prefix.unnamed">
		<xsl:with-param name="header">
			<xsl:call-template name="header.number"/>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.prefix.mode" match="article | reference">
	<xsl:if test="
			(preceding-sibling::*[name(.) = name(current())])							or
			(following-sibling::*[name(.) = name(current())])							or
			(parent::part/preceding-sibling::part/*[name(.) = name(current())])	or
			(parent::part/following-sibling::part/*[name(.) = name(current())])	">
		<xsl:call-template name="header.prefix.unnamed"/>
	</xsl:if>
</xsl:template>

<xsl:template mode="header.prefix.mode" match="title | subtitle">
	<xsl:apply-templates mode="header.prefix.mode" select=".."/>
</xsl:template>

<xsl:template mode="header.prefix.mode" match="*"/>

<!-- == header ============================================================= -->

<!-- header -->
<xsl:template name="header">
	<xsl:param name="node" select="."/>
	<xsl:apply-templates mode="header.mode" select="$node"/>
</xsl:template>

<!-- header.named -->
<xsl:template name="header.named" mode="header.mode" match="
		appendix		| article	| book		| bibliography	| chapter		|
		colophon		| glossary	| index		| part			| preface		|
		reference	| refsect1	| refsect2	| refsect3		| refsection	|
		refentry		| sect1		| sect2		| sect3			| sect4			|
		sect5			| section	| set			| setindex		| simplesect	">
	<xsl:call-template name="format.header">
		<xsl:with-param name="header">
			<xsl:call-template name="header.name"/>
		</xsl:with-param>
		<xsl:with-param name="number">
			<xsl:call-template name="header.number"/>
		</xsl:with-param>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="example | figure | table">
	<xsl:choose>
		<xsl:when test="@label">
			<xsl:value-of select="@label"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="header.named"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="header.mode" match="synopfragment">
	<xsl:text>(</xsl:text>
	<xsl:call-template name="header.number"/>
	<xsl:text>)</xsl:text>
</xsl:template>

<xsl:template mode="header.mode" match="title | subtitle">
	<xsl:call-template name="header">
		<xsl:with-param name="node" select=".."/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="appendixinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Appendix'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="articleinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Article'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="bibliographyinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Bibliography'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="bookinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Book'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="chapterinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Chapter'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="glossaryinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Glossary'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="indexinfo | setindexinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Index'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="partinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Part'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="prefaceinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Preface'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="refentryinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Entry'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="referenceinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Reference'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="
		refsect1info	| refsect2info	| refsect3info	| refsectioninfo	|
		sect1info		| sect2info		| sect3info		| sect4info			|
		sect5info		| sectioninfo	">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="setinfo">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'About This Set'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.mode" match="*"/>

<!-- == header.name ======================================================== -->

<xsl:template name="header.name">
	<xsl:param name="node" select="."/>
	<xsl:apply-templates mode="header.name.mode" select="$node"/>
</xsl:template>

<xsl:template mode="header.name.mode" match="appendix">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Appendix'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="article">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Article'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="book">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Book'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="bibliography">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Bibliography'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="caution">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Caution'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="chapter">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Chapter'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="colophon">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Colophon'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="dedication">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Dedication'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="example">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Example'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="figure">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Figure'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="glossary">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Glossary'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="important">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Important'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="index">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Index'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="msgaud">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Message Audience'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="msglevel">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Message Level'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="msgorig">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Message Origin'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="note">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Note'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="part">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Part'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="preface">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Preface'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="reference">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Reference'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="refentry">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Reference Entry'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="
		refsect1 | refsect2 | refsect3 | refsection">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Reference Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="
		sect1 | sect2 | sect3 | sect4 | section | simplesect">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Section'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="setindex">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Set Index'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="table">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Table'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="tip">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Tip'"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.name.mode" match="warning">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Warning'"/>
	</xsl:call-template>
</xsl:template>


<xsl:template mode="header.name.mode" match="subtitle">
	<xsl:apply-templates mode="header.name.mode" select=".."/>
</xsl:template>

<xsl:template mode="header.name.mode" match="title">
	<xsl:apply-templates mode="header.name.mode" select=".."/>
</xsl:template>

<xsl:template mode="header.name.mode" match="*">
	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid" select="'Unknown'"/>
	</xsl:call-template>
</xsl:template>

<!-- == header.number ====================================================== -->

<xsl:template name="header.number">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="element-available('yelp:cache')">
			<yelp:cache key="header.number">
				<xsl:apply-templates mode="header.number.mode" select="$node"/>
			</yelp:cache>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates mode="header.number.mode" select="$node"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="header.number.mode" match="appendix">
	<xsl:call-template name="format.appendix.number">
		<xsl:with-param name="appendix" select="
			count(preceding-sibling::appendix) + 1 +
			count(parent::part/preceding-sibling::part/appendix)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="article">
	<xsl:call-template name="format.article.number">
		<xsl:with-param name="article" select="
			count(preceding-sibling::article) + 1 +
			count(parent::part/preceding-sibling::part/article)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="chapter">
	<xsl:call-template name="format.chapter.number">
		<xsl:with-param name="chapter" select="
			count(preceding-sibling::chapter) + 1 +
			count(parent::part/preceding-sibling::part/chapter)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="part">
	<xsl:call-template name="format.part.number">
		<xsl:with-param name="part" select="
			count(preceding-sibling::part) + 1"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="reference">
	<xsl:call-template name="format.reference.number">
		<xsl:with-param name="reference" select="
			count(preceding-sibling::reference) + 1 +
			count(parent::part/preceding-sibling::part/reference)"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="
		refentry	| refsect1	| refsect2	| refsect3	| refsection	|
		sect1		| sect2		| sect3		| sect4		| sect5			|
		section	| simplesect">
	<xsl:variable name="sect" select="."/>
	<xsl:choose>
		<xsl:when test="
				name(..) = 'article' or name(..) = 'partintro' or name(..) = 'preface'">
			<xsl:call-template name="format.section.number">
				<xsl:with-param name="section" select="
					count(preceding-sibling::*[name(.) = name($sect)]) + 1"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="format.subsection.number">
				<xsl:with-param name="parent">
					<xsl:call-template name="header.number">
						<xsl:with-param name="node" select=".."/>
					</xsl:call-template>
				</xsl:with-param>
				<xsl:with-param name="section" select="
					count(preceding-sibling::*[name(.) = name($sect)]) + 1"/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="header.number.mode" match="
	book	| bibliography	| colophon	| glossary	|
	index	| preface		| set			| setindex	"/>

<xsl:template mode="header.number.mode" match="synopfragment">
	<xsl:value-of select="count(preceding-sibling::synopfragment) + 1"/>
</xsl:template>

<xsl:template mode="header.number.mode" match="subtitle">
	<xsl:call-template name="header.number">
		<xsl:with-param name="node" select=".."/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="title">
	<xsl:call-template name="header.number">
		<xsl:with-param name="node" select=".."/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="*">
	<xsl:call-template name="header.number">
		<xsl:with-param name="node" select=".."/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="header.number.mode" match="example | figure | table">
	<xsl:variable name="section" select="
		ancestor::*[parent::article][
			(name(.) = 'refentry') or (name(.) = 'sect1')      or
			(name(.) = 'section')  or (name(.) = 'simplesect') ]
	"/>
	<xsl:variable name="parent">
		<xsl:choose>
			<xsl:when test="$section">
				<xsl:call-template name="header.number">
					<xsl:with-param name="node" select="$section[last()]"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:variable name="chapter"
					select="ancestor::*[name(.) = 'appendix' or name(.) = 'chapter']"/>
				<xsl:if test="$chapter">
					<xsl:call-template name="header.number">
						<xsl:with-param name="node" select="$chapter[1]"/>
					</xsl:call-template>
				</xsl:if>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="num">
		<xsl:choose>
			<xsl:when test="$section">
				<xsl:value-of select="count(
					ancestor-or-self::*[ancestor::*[parent::article][
							(name(.) = 'refentry') or (name(.) = 'sect1')      or
							(name(.) = 'section')  or (name(.) = 'simplesect') ]
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
