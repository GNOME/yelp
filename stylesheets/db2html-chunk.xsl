<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                extension-element-prefixes="exsl"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="chunk">
	<xsl:param name="node" select="."/>
	<xsl:param name="id" select="$node/@id"/>
	<xsl:variable name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:variable>

	<exsl:document href="{concat($id, $html_extension)}">
		<xsl:call-template name="html">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="leaf" select="$depth_chunk &gt;= $chunk_depth"/>
		</xsl:call-template>
	</exsl:document>

	<xsl:if test="$generate_titlepage and ($node = /*)">
		<xsl:apply-templates mode="chunk.info.mode" select="."/>
	</xsl:if>

	<xsl:if test="$depth_chunk &lt; $chunk_depth">
		<xsl:apply-templates mode="chunk.divisions.mode" select="."/>
	</xsl:if>
</xsl:template>

<!-- == chunk.info.mode ==================================================== -->

<xsl:template mode="chunk.info.mode" match="appendix">
	<xsl:if test="appendixinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="appendixinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="article">
	<xsl:if test="articleinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="articleinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="bibliography">
	<xsl:if test="bibliographyinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="bibliographyinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="book">
	<xsl:if test="bookinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="bookinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="chapter">
	<xsl:if test="chapterinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="chapterinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="glossary">
	<xsl:if test="glossaryinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="glossaryinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="index">
	<xsl:if test="indexinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="indexinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="part">
	<xsl:if test="partinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="partinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="preface">
	<xsl:if test="prefaceinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="prefaceinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="refentry">
	<xsl:if test="refentryinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="refentryinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="reference">
	<xsl:if test="referenceinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="referenceinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="refsect1">
	<xsl:if test="refsect1info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="refsect1info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="refsect2">
	<xsl:if test="refsect2info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="refsect2info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="refsect3">
	<xsl:if test="refsect3info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="refsect3info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="refsection">
	<xsl:if test="refsectioninfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="refsectioninfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="sect1">
	<xsl:if test="sect1info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="sect1info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="sect2">
	<xsl:if test="sect2info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="sect2info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="sect3">
	<xsl:if test="sect3info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="sect3info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="sect4">
	<xsl:if test="sect4info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="sect4info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="sect5">
	<xsl:if test="sect5info">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="sect5info"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="section">
	<xsl:if test="sectioninfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="sectioninfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="set">
	<xsl:if test="setinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="setinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template mode="chunk.info.mode" match="setindex">
	<xsl:if test="setindexinfo">
		<xsl:call-template name="chunk">
			<xsl:with-param name="node" select="setindexinfo"/>
			<xsl:with-param name="id" select="'titlepage'"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<!-- == chunk.divisions.mode =============================================== -->

<xsl:template mode="chunk.divisions.mode" match="
		appendix | article | chapter | preface">
	<xsl:for-each select="refentry | sect1 | section | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="book">
	<xsl:for-each select="
			appendix	| article	| bibliography	| chapter	|
			colophon	| glossary	| index			| part		|
			preface	| reference	| setindex 		">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="
	bibliography	| colophon	| glossary	| index		|
	reference		| refsect3	| setindex	| simplesect"/>

<xsl:template mode="chunk.divisions.mode" match="part">
	<xsl:for-each select="
			appendix	| article	| bibliography	| chapter	| glossary	|
			index		| preface	| refentry		| reference	">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="refentry">
	<xsl:for-each select="refsect1 | refsection">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="refsect1">
	<xsl:for-each select="refsect2">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="refsect2">
	<xsl:for-each select="refsect3">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="refsection">
	<xsl:for-each select="refsection">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="sect1">
	<xsl:for-each select="refentry | sect2 | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="sect2">
	<xsl:for-each select="refentry | sect3 | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="sect3">
	<xsl:for-each select="refentry | sect4 | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="sect4">
	<xsl:for-each select="refentry | sect5 | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="sect5">
	<xsl:for-each select="refentry | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="section">
	<xsl:for-each select="refentry | section | simplesect">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<xsl:template mode="chunk.divisions.mode" match="set">
	<xsl:for-each select="book | setindex">
		<xsl:call-template name="chunk"/>
	</xsl:for-each>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="depth.in.chunk">
	<xsl:param name="node" select="."/>
	<xsl:variable name="is_chunk">
		<xsl:call-template name="is.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$is_chunk = 1">0</xsl:when>
		<xsl:when test="$node = /*">0</xsl:when>
		<xsl:otherwise>
			<xsl:variable name="parent_depth">
				<xsl:call-template name="depth.in.chunk">
					<xsl:with-param name="node" select="$node/.."/>
				</xsl:call-template>
			</xsl:variable>
			<xsl:value-of select="$parent_depth + 1"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="depth.chunk">
	<xsl:param name="node" select="."/>
	<xsl:variable name="is_chunk">
		<xsl:call-template name="is.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$node = /*">0</xsl:when>
		<xsl:when test="$is_chunk = 1">
			<xsl:variable name="parent_depth">
				<xsl:call-template name="depth.chunk">
					<xsl:with-param name="node" select="$node/.."/>
				</xsl:call-template>
			</xsl:variable>
			<xsl:value-of select="$parent_depth + 1"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="depth.chunk">
				<xsl:with-param name="node" select="$node/.."/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="chunk.id">
	<xsl:param name="node" select="."/>
	<xsl:apply-templates mode="chunk.id.mode" select="$node"/>
</xsl:template>

<xsl:template mode="chunk.id.mode" match="
		appendix		| article	| bibliography	| book		| chapter	|
		colophon		| glossary	| index			| part		| preface	|
		refentry		| reference	| refsect1		| refsect2	| refsect3	|
		refsection	| sect1		| sect2			| sect3		| sect4		|
		sect5			| section	| set				| setindex	| simplesect">
	<xsl:variable name="is_chunk">
		<xsl:call-template name="is.chunk"/>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$is_chunk = 1">
			<xsl:value-of select="@id"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="chunk.id">
				<xsl:with-param name="node" select=".."/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="chunk.id.mode" match="
		appendixinfo	| articleinfo		| bibliographyinfo	|
		bookinfo			| chapterinfo		| glossaryinfo			|
		indexinfo		| partinfo			| prefaceinfo			|
		refentryinfo	| referenceinfo	| refsect1info			|
		refsect2info	| refsect3info		| refsectioninfo		|
		sect1info		| sect2info			| sect3info				|
		sect4info		| sect5info			| sectioninfo			|
		setinfo			| setindexinfo		">
	<xsl:choose>
		<xsl:when test="$generate_titlepage and (.. = /*)">
			<xsl:text>titlepage</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="chunk.id">
				<xsl:with-param name="node" select=".."/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="chunk.id.mode" match="*">
	<xsl:call-template name="chunk.id">
		<xsl:with-param name="node" select=".."/>
	</xsl:call-template>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="is.chunk">
	<xsl:param name="node" select="."/>
	<xsl:apply-templates mode="is.chunk.mode" select="$node"/>
</xsl:template>

<xsl:template mode="is.chunk.mode" match="
		appendix		| article	| bibliography	| book		| chapter	|
		colophon		| glossary	| index			| part		| preface	|
		refentry		| reference	| refsect1		| refsect2	| refsect3	|
		refsection	| sect1		| sect2			| sect3		| sect4		|
		sect5			| section	| set				| setindex	| simplesect">
	<xsl:choose>
		<xsl:when test="count(
				ancestor::appendix		| ancestor::article		|
				ancestor::bibliography	| ancestor::book			|
				ancestor::chapter			| ancestor::colophon		|
				ancestor::glossary		| ancestor::index			|
				ancestor::part				| ancestor::preface		|
				ancestor::refentry		| ancestor::reference	|
				ancestor::refsect1		| ancestor::refsect2		|
				ancestor::refsect3		| ancestor::refsection	|
				ancestor::sect1			| ancestor::sect2			|
				ancestor::sect3			| ancestor::sect4			|
				ancestor::sect5			| ancestor::section		|
				ancestor::set				| ancestor::setindex		|
				ancestor::simplesect		)
			&lt;= $chunk_depth ">1</xsl:when>
		<xsl:otherwise>0</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="is.chunk.mode" match="
		appendixinfo	| articleinfo		| bibliographyinfo	|
		bookinfo			| chapterinfo		| glossaryinfo			|
		indexinfo		| partinfo			| prefaceinfo			|
		refentryinfo	| referenceinfo	| refsect1info			|
		refsect2info	| refsect3info		| refsectioninfo		|
		sect1info		| sect2info			| sect3info				|
		sect4info		| sect5info			| sectioninfo			|
		setinfo			| setindexinfo		">
	<xsl:choose>
		<xsl:when test="$generate_titlepage and (.. = /*)">1</xsl:when>
		<xsl:otherwise>0</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="is.chunk.mode" match="*">0</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
