<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY is-info "(
	(name(.) = 'appendixinfo')			or (name(.) = 'articleinfo')    or
	(name(.) = 'bibliographyinfo')	or (name(.) = 'bookinfo')       or
	(name(.) = 'chapterinfo')			or (name(.) = 'glossaryinfo')   or
	(name(.) = 'indexinfo')				or (name(.) = 'partinfo')       or
	(name(.) = 'prefaceinfo')			or (name(.) = 'refentryinfo')   or
	(name(.) = 'referenceinfo')		or (name(.) = 'refsect1info')   or
	(name(.) = 'refsect2info')			or (name(.) = 'refsect3info')   or
	(name(.) = 'refsectioninfo')		or (name(.) = 'sect1info')      or
	(name(.) = 'sect2info')				or (name(.) = 'sect3info')      or
	(name(.) = 'sect4info')				or (name(.) = 'sect5info')      or
	(name(.) = 'sectioninfo')			or (name(.) = 'setinfo')        or
	(name(.) = 'setindexinfo'))">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template match="
		appendix  | article  | book     | bibliography | chapter    |
		colophon  | glossary | index    | part         | preface    |
		reference | refsect1 | refsect2 | refsect3     | refsection |
		sect1     | sect2    | sect3    | sect4        | sect5      |
		section   | set      | setindex | simplesect   ">
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk"/>
	</xsl:param>
	<xsl:param name="depth_in_chunk">
		<xsl:call-template name="depth.in.chunk"/>
	</xsl:param>
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:if test="not(title) and *[&is-info;]/title">
			<xsl:for-each select="*[&is-info;]/title">
				<xsl:call-template name="title.h">
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
					<xsl:with-param name="depth_in_chunk" select="$depth_in_chunk + 1"/>
				</xsl:call-template>
			</xsl:for-each>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="$depth_chunk &lt; $chunk_depth">
				<xsl:apply-templates mode="sans.divisions.mode">
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
					<xsl:with-param name="depth_in_chunk" select="$depth_in_chunk + 1"/>
				</xsl:apply-templates>
				<xsl:call-template name="toc">
					<xsl:with-param name="node" select="."/>
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates>
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
					<xsl:with-param name="depth_in_chunk" select="$depth_in_chunk + 1"/>
				</xsl:apply-templates>
			</xsl:otherwise>
		</xsl:choose>
	</div>
</xsl:template>

<xsl:template match="refentry">
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk"/>
	</xsl:param>
	<xsl:param name="depth_in_chunk">
		<xsl:call-template name="depth_in_chunk"/>
	</xsl:param>
	<xsl:if test="preceding-sibling::refentry and $depth_chunk &gt; 0">
		<hr class="refentry.seperator"/>
	</xsl:if>
	<div class="{name(.)}">
		<xsl:choose>
			<xsl:when test="$depth_chunk &lt;= $chunk_depth">
				<xsl:apply-templates mode="sans.divisions.mode">
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
					<xsl:with-param name="depth_in_chunk" select="$depth_in_chunk + 1"/>
				</xsl:apply-templates>
				<xsl:call-template name="toc"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates>
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
					<xsl:with-param name="depth_in_chunk" select="$depth_in_chunk + 1"/>
				</xsl:apply-templates>
			</xsl:otherwise>
		</xsl:choose>
	</div>
</xsl:template>

<!-- == sans.divisions.mode  =============================================== -->

<xsl:template mode="sans.divisions.mode" match="
		appendix		| article	| book		| bibliography	| chapter	|
		colophon		| glossary	| index		| part			| preface	|
		reference	| refentry	| refsect1	| refsect2		| refsect3	|
		refsection	| sect1		| sect2		| sect3			| sect4		|
		sect5			| section	| set			| setindex		| simplesect"/>

<xsl:template mode="sans.divisions.mode" match="
		appendixinfo	| articleinfo		| bibliographyinfo	|
		bookinfo			| chapterinfo		| glossaryinfo			|
		indexinfo		| partinfo			| prefaceinfo			|
		refentryinfo	| referenceinfo	| refsect1info			|
		refsect2info	| refsect3info		| refsectioninfo		|
		sect1info		| sect2info			| sect3info				|
		sect4info		| sect5info			| sectioninfo			|
		setinfo			| setindexinfo		"/>

<xsl:template mode="sans.divisions.mode" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
