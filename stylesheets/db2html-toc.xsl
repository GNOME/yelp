<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="toc">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:call-template name="toc.table">
		<xsl:with-param name="node" select="$node"/>
		<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
	</xsl:call-template>
</xsl:template>

<!-- == toc.table ========================================================== -->

<xsl:template name="toc.table">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>

	<xsl:variable name="cols" select="$chunk_depth - $depth_chunk + 1"/>

	<h2><xsl:call-template name="gettext">
		<xsl:with-param name="msgid">
			<xsl:choose>
				<xsl:when test=". = /*">
					<xsl:value-of select="'Table of Contents'"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="'Contents'"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:with-param>
	</xsl:call-template></h2>
	<div class="toc"><table class="toc">
		<xsl:apply-templates mode="toc.table.tr.mode">
			<xsl:with-param name="cols" select="$cols"/>
			<xsl:with-param name="depth_table" select="0"/>
			<xsl:with-param name="depth_chunk" select="$depth_chunk + 1"/>
		</xsl:apply-templates>
	</table></div>
</xsl:template>

<xsl:template mode="toc.table.tr.mode" match="
		appendix		| article	| book		| bibliography	| chapter		|
		colophon		| glossary	| index		| part			| preface		|
		reference	| refentry	| refsect1	| refsect2		| refsect3		|
		refsection	| sect1		| sect2		| sect3			| sect4			|
		sect5			| section	| set			| setindex		| simplesect	">
	<xsl:param name="cols"/>
	<xsl:param name="depth_table"/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk"/>
	</xsl:param>
	<xsl:param name="indent" select="false()"/>

	<tr class="{name(.)}">
		<xsl:if test="$depth_table &gt; 0">
			<td colspan="{$depth_table}"/>
		</xsl:if>
		<td>
			<xsl:if test="$indent"><xsl:text>&#160;&#160;</xsl:text></xsl:if>
			<a>
				<xsl:attribute name="href">
					<xsl:call-template name="xref.target">
						<xsl:with-param name="linkend" select="@id"/>
						<xsl:with-param name="target" select="."/>
					</xsl:call-template>
				</xsl:attribute>
				<xsl:choose>
					<xsl:when test="@xreflabel">
						<xsl:value-of select="$target/@xreflabel"/>
					</xsl:when>
					<xsl:when test="$depth_table = 0">
						<xsl:call-template name="xref.content">
							<xsl:with-param name="linkend" select="@id"/>
							<xsl:with-param name="target" select="."/>
						</xsl:call-template>
					</xsl:when>
					<xsl:otherwise>
						<xsl:call-template name="header.number"/>
					</xsl:otherwise>
				</xsl:choose>
			</a>
		</td>
		<td colspan="{$cols - $depth_table - 1}">
			<xsl:apply-templates select="title/node()"/>
		</td>
	</tr>
	<xsl:if test="$depth_chunk &lt; $chunk_depth">
		<xsl:apply-templates mode="toc.table.tr.mode">
			<xsl:with-param name="cols" select="$cols"/>
			<xsl:with-param name="depth_table">
				<xsl:choose>
					<xsl:when test="name(.) = 'part'">
						<xsl:value-of select="$depth_table"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:value-of select="$depth_table + 1"/>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:with-param>
			<xsl:with-param name="depth_chunk" select="$depth_chunk + 1"/>
			<xsl:with-param name="indent">
				<xsl:choose>
					<xsl:when test="name(.) = 'part'">
						<xsl:value-of select="true()"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:value-of select="false()"/>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:with-param>
		</xsl:apply-templates>
	</xsl:if>
</xsl:template>

<xsl:template mode="toc.table.tr.mode" match="
		appendixinfo	| articleinfo	| bibliographyinfo	| bookinfo		|
		chapterinfo		| glossaryinfo	| indexinfo				| partinfo		|
		prefaceinfo		| refentryinfo	| referenceinfo		| refsect1info	|
		refsect2info	| refsect3info	| refsectioninfo		| sect1info		|
		sect2info		| sect3info		| sect4info				| sect5info		|
		sectioninfo		| setinfo		| setindexinfo			">
	<xsl:param name="cols"/>
	<xsl:param name="depth_table"/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk"/>
	</xsl:param>
	<xsl:param name="indent" select="false()"/>

	<xsl:if test="$depth_chunk = 1">
		<tr><td colspan="{$cols}">
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="'titlepage'"/>
				<xsl:with-param name="target" select="."/>
			</xsl:call-template>
		</td></tr>
	</xsl:if>
</xsl:template>

<xsl:template mode="toc.table.tr.mode" match="*"/>

<!-- ======================================================================= -->

</xsl:stylesheet>
