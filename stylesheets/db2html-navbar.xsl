<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- == navbar.following.link ============================================== -->

<xsl:template name="navbar.following.link">
	<xsl:param name="node" select="."/>
	<xsl:apply-templates mode="navbar.following.link.mode" select="$node"/>
</xsl:template>

<xsl:template match="*" mode="navbar.following.link.mode">
<!--
	<xsl:variable name="depth" select="count(ancestor::*[&is-div;])"/>
	<xsl:choose>
		<xsl:when test="not(&is-div;)">
			<xsl:apply-templates mode="navbar.following.link.mode"
				 select="ancestor::*[&is-div;][1]"/>
		</xsl:when>
		<xsl:when test="following-sibling::*[&is-div;]">
			<xsl:variable name="foll"
				select="following-sibling::*[&is-div;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$foll/@id"/>
				<xsl:with-param name="target" select="$foll"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$depth &gt; 0">
			<xsl:apply-templates select=".." mode="navbar.following.link.mode"/>
		</xsl:when>
	</xsl:choose>
-->
</xsl:template>

<xsl:template match="*" mode="navbar.last.link.mode">
<!--
	<xsl:variable name="depth" select="count(ancestor::*[&is-div;])"/>
	<xsl:choose>
		<xsl:when test="not(&is-div;)">
			<xsl:apply-templates mode="navbar.last.link.mode"
				 select="ancestor::*[&is-div;][1]"/>
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
		<xsl:when test="*[&is-div;]">
			<xsl:apply-templates mode="navbar.last.link.mode"
				select="*[&is-div;][last()]"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="@id"/>
				<xsl:with-param name="target" select="."/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
-->
</xsl:template>

<xsl:template match="*" mode="navbar.prev.link.mode">
<!--
	<xsl:variable name="depth" select="count(ancestor::*[&is-div;])"/>
	<xsl:variable name="chunk_id">
		<xsl:apply-templates select="." mode="chunk.id.mode"/>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="string($chunk_id) != @id">
			<xsl:apply-templates select="id($chunk_id)" mode="navbar.prev.link.mode"/>
		</xsl:when>
		<xsl:when test="&is-div-info;"/>
		<xsl:when test="$depth &gt; $chunk_depth">
			<xsl:apply-templates select=".." mode="navbar.prev.link.mode"/>
		</xsl:when>
		<xsl:when test="
				($depth = $chunk_depth) and (preceding-sibling::*[&is-div;])">
			<xsl:variable name="prev" select="preceding-sibling::*[&is-div;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$prev/@id"/>
				<xsl:with-param name="target" select="$prev"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="preceding-sibling::*[&is-div;]">
			<xsl:apply-templates mode="navbar.last.link.mode"
				select="preceding-sibling::*[&is-div;][1]"/>
		</xsl:when>
		<xsl:when test="parent::*[&is-div;]">
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="../@id"/>
				<xsl:with-param name="target" select=".."/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$generate_titlepage and (. = /*) and *[&is-div-info;]">
			<xsl:variable name="prev" select="*[&is-div-info;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$prev/@id"/>
				<xsl:with-param name="target" select="$prev"/>
			</xsl:call-template>
		</xsl:when>
	</xsl:choose>
-->
</xsl:template>

<xsl:template match="*" mode="navbar.next.link.mode">
<!--
	<xsl:variable name="depth" select="count(ancestor::*[&is-div;])"/>
	<xsl:choose>
		<xsl:when test="&is-div-info;">
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="../@id"/>
				<xsl:with-param name="target" select=".."/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="not(&is-div;)">
			<xsl:apply-templates mode="navbar.next.link.mode"
				 select="ancestor::*[&is-div;][1]"/>
		</xsl:when>
		<xsl:when test="$depth &gt; $chunk_depth">
			<xsl:apply-templates select="." mode="navbar.next.link.mode"/>
		</xsl:when>
		<xsl:when test="($depth &lt; $chunk_depth) and (*[&is-div;])">
			<xsl:variable name="next" select="*[&is-div;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$next/@id"/>
				<xsl:with-param name="target" select="$next"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="." mode="navbar.following.link.mode"/>
		</xsl:otherwise>
	</xsl:choose>
-->
</xsl:template>

</xsl:stylesheet>
