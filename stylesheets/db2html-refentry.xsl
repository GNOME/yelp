<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template match="manvolnum">
	<xsl:text>(</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>)</xsl:text>
</xsl:template>

<xsl:template match="refclass">
	<div class="{name(.)}">
		<b>
			<xsl:if test="@role">
				<xsl:value-of select="@role"/>
				<xsl:text>: </xsl:text>
			</xsl:if>
		</b>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template match="refdescriptor">
	<xsl:call-template name="block.simple"/>
</xsl:template>

<xsl:template match="refentrytitle">
	<xsl:call-template name="title.h"/>
</xsl:template>

<xsl:template match="refname">
	<xsl:apply-templates/>
	<xsl:if test="following-sibling::refname">
		<xsl:text>, </xsl:text>
	</xsl:if>
</xsl:template>

<xsl:template match="refnamediv">
	<xsl:call-template name="block.simple"/>
</xsl:template>

<xsl:template match="refpurpose">
	<xsl:text> &#8212; </xsl:text>
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="refsynopsisdiv">
	<xsl:call-template name="block.simple"/>
</xsl:template>

</xsl:stylesheet>
