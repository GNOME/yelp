<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="gettext">
	<xsl:param name="key"/>
	<xsl:param name="lang" select="$lang"/>

	<xsl:value-of select="$key"/>
</xsl:template>

<xsl:template name="ngettext">
	<xsl:param name="key"/>
	<xsl:param name="num" select="1"/>
	<xsl:param name="lang" select="$lang"/>

	<xsl:value-of select="$key"/>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="dingbat">
	<xsl:param name="dingbat"/>
	<xsl:choose>
		<xsl:when test="$dingbat = 'copyright'">
			<xsl:value-of select="'&#169;'"/>
		</xsl:when>
		<xsl:when test="$dingbat = 'registered'">
			<xsl:value-of select="'&#174;'"/>
		</xsl:when>
		<xsl:when test="$dingbat = 'trade'">
			<xsl:value-of select="'&#8482;'"/>
		</xsl:when>
		<xsl:when test="$dingbat = 'service'">
			<xsl:value-of select="'&#8480;'"/>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="format.header">
	<xsl:param name="header"/>
	<xsl:value-of select="concat($header, '&#8199;')"/>
</xsl:template>

<xsl:template name="format.header.header">
	<xsl:param name="header"/>
	<xsl:param name="number"/>
	<xsl:value-of select="concat($header, '&#160;', $number, '&#8199;')"/>
</xsl:template>

<xsl:template name="format.header.number">
	<xsl:param name="number"/>
	<xsl:value-of select="concat($number, '&#8199;')"/>
</xsl:template>

<xsl:template name="format.header.inline">
	<xsl:param name="header"/>
	<xsl:param name="number"/>
	<xsl:value-of select="concat($header, '&#160;', $number)"/>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="format.appendix.number">
	<xsl:param name="appendix"/>
	<xsl:number format="A" value="$appendix"/>
</xsl:template>

<xsl:template name="format.article.number">
	<xsl:param name="article"/>
	<xsl:number format="I" value="$article"/>
</xsl:template>

<xsl:template name="format.chapter.number">
	<xsl:param name="chapter"/>
	<xsl:number value="$chapter"/>
</xsl:template>

<xsl:template name="format.example.number">
	<xsl:param name="parent"/>
	<xsl:param name="example"/>
	<xsl:value-of select="concat($parent, '-')"/>
	<xsl:number value="$example"/>
</xsl:template>

<xsl:template name="format.figure.number">
	<xsl:param name="parent"/>
	<xsl:param name="figure"/>
	<xsl:value-of select="concat($parent, '-')"/>
	<xsl:number value="$figure"/>
</xsl:template>

<xsl:template name="format.part.number">
	<xsl:param name="part"/>
	<xsl:number format="I" value="$part"/>
</xsl:template>

<xsl:template name="format.reference.number">
	<xsl:param name="article"/>
	<xsl:number format="I" value="$article"/>
</xsl:template>

<xsl:template name="format.section.number">
	<xsl:param name="section"/>
	<xsl:number value="$section"/>
</xsl:template>

<xsl:template name="format.subsection.number">
	<xsl:param name="parent"/>
	<xsl:param name="section"/>
	<xsl:value-of select="concat($parent, '.')"/>
	<xsl:number value="$section"/>
</xsl:template>

<xsl:template name="format.table.number">
	<xsl:param name="parent"/>
	<xsl:param name="table"/>
	<xsl:value-of select="concat($parent, '-')"/>
	<xsl:number value="$table"/>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
