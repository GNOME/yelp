<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="anchor" match="anchor">
	<xsl:param name="name" select="@id"/>
	<xsl:if test="$name">
		<a name="{$name}"/>
	</xsl:if>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template match="link">
	<a>
		<xsl:attribute name="href">
			<xsl:call-template name="xref.target">
				<xsl:with-param name="linkend" select="@linkend"/>
			</xsl:call-template>
		</xsl:attribute>
		<xsl:choose>
			<xsl:when test="@endterm">
				<xsl:apply-templates select="id(@endterm)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates/>
			</xsl:otherwise>
		</xsl:choose>
	</a>
</xsl:template>

<xsl:template match="olink">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template name="ulink" match="ulink">
	<xsl:param name="url" select="@url"/>
	<xsl:param name="content" select="false()"/>
	<a href="{$url}">
		<xsl:choose>
			<xsl:when test="$content">
				<xsl:copy-of select="$content"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates/>
			</xsl:otherwise>
		</xsl:choose>
	</a>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="xref" match="xref">
	<xsl:param name="linkend" select="@linkend"/>
	<xsl:param name="target" select="id($linkend)"/>
	<xsl:param name="content" select="false()"/>
	<a>
		<xsl:attribute name="href">
			<xsl:call-template name="xref.target">
				<xsl:with-param name="linkend" select="$linkend"/>
				<xsl:with-param name="target" select="$target"/>
			</xsl:call-template>
		</xsl:attribute>
		<xsl:choose>
			<xsl:when test="$content">
				<xsl:copy-of select="$content"/>
			</xsl:when>
			<xsl:when test="@endterm">
				<xsl:apply-templates select="id(@endterm)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="xref.content">
					<xsl:with-param name="linkend" select="$linkend"/>
					<xsl:with-param name="target" select="$target"/>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</a>
</xsl:template>

<xsl:template name="xref.target">
	<xsl:param name="linkend" select="@linkend"/>
	<xsl:param name="target" select="id($linkend)"/>
	<xsl:variable name="chunk_id">
		<xsl:apply-templates select="$target" mode="chunk.id.mode"/>
	</xsl:variable>
	<xsl:value-of select="concat($chunk_id, $html_extension)"/>
	<xsl:if test="$linkend and string($chunk_id) != $linkend">
		<xsl:text>#</xsl:text>
		<xsl:value-of select="$linkend"/>
	</xsl:if>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="xref.content">
	<xsl:param name="linkend" select="@linkend"/>
	<xsl:param name="target" select="id($linkend)"/>
	<xsl:choose>
		<xsl:when test="$target/@xreflabel">
			<xsl:value-of select="$target/@xreflabel"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates mode="xref.content.mode" select="$target"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="header.mode" match="glossentry">
	<xsl:apply-templates mode="xref.content.mode" select="glossterm[1]"/>
</xsl:template>

<xsl:template mode="xref.content.mode" match="glossterm">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template mode="xref.content.mode" match="*">
	<xsl:call-template name="header"/>
</xsl:template>


<!-- ======================================================================= -->


</xsl:stylesheet>
