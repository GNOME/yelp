<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

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
		<xsl:choose>
			<xsl:when test="$depth_chunk &lt; $chunk_depth">
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
