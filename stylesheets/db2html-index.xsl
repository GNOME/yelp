<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template match="indexdiv">
	<xsl:call-template name="block.simple"/>
</xsl:template>

<xsl:template match="indexentry">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="primaryie">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="secondaryie">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="see">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="seealso">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="seealsoie">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="seeie">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="tertiaryie">
	<xsl:call-template name="FIXME"/>
</xsl:template>

</xsl:stylesheet>
