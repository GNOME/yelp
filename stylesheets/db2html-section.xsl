<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="section">
	<xsl:param name="content" select="*[yelp:is-content(.)]"/>
	<xsl:param name="divisions" select="*[yelp:is-division(.)]"/>
	<div class="{name(.)}">
		<xsl:apply-templates select="title"/>
		<xsl:apply-templates select="subtitle"/>
		<xsl:apply-templates select="$content"/>
		<xsl:choose>
			<xsl:when test="yelp:get-depth(.) &lt; $chunk_depth">
				<xsl:call-template name="toc.verbose"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="$divisions"/>
			</xsl:otherwise>
		</xsl:choose>
	</div>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template match="appendix">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="article">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="book">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="bibliography">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="chapter">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="chapter">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="colophon">
	<xsl:call-template name="section"/>
</xsl:template>  

<xsl:template match="glossary">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="index">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="part">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="preface">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="reference">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="refentry">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="refsect1">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="refsect2">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="refsect3">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="refsection">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="sect1">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="sect2">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="sect3">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="sect4">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="sect5">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="section">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="set">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="setindex">
	<xsl:call-template name="section"/>
</xsl:template>

<xsl:template match="simplesect">
	<xsl:call-template name="section"/>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
