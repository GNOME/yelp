<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                extension-element-prefixes="exsl yelp"
                version="1.0">

<xsl:param name="stylesheet_path" select="''"/>
<xsl:param name="color_gray_background" select="'#E6E6E6'"/>
<xsl:param name="color_gray_border" select="'#A1A1A1'"/>

<xsl:template match="Man">
	<xsl:choose>
		<xsl:when test="element-available('yelp:document')">
			<yelp:document href="index">
				<xsl:call-template name="html"/>
			</yelp:document>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="html"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="html">
	<html>
		<head>
			<title>
				<xsl:value-of select="title"/>
			</title>
			<style type="text/css">
				<xsl:call-template name="html.css"/>
			</style>
		</head>
		<body>
			<div class="body">
				<xsl:apply-templates/>
			</div>
		</body>
	</html>
</xsl:template>

<xsl:template name="html.css"><xsl:text>
h1 { font-size: 1.6em; font-weight: bold; }
h2 { font-size: 1.4em; font-weight: bold; }
h3 { font-size: 1.2em; font-weight: bold; }

div[class="title"] + * { margin-top: 0.8em; }

body { margin: 0em; padding: 0em; }
div[class="body"] {
	margin-left: 0.8em;
	margin-right: 0.8em;
	margin-bottom: 1.6em;
}

p, div { margin: 0em; }
p + p, p + div, div + p, div + div { margin-top: 0.8em; }

dl { margin: 0em; }
ol { margin: 0em; }
ul { margin: 0em; }
ul li { padding-left: 0.4em; }
dd + dt { margin-top: 1.6em; }
</xsl:text></xsl:template>

<!-- ======================================================================= -->

<xsl:template match="B | fB">
	<b><xsl:apply-templates/></b>
</xsl:template>

<xsl:template match="I | fI">
	<i><xsl:apply-templates/></i>
</xsl:template>

<xsl:template match="P">
	<p><pre><xsl:apply-templates/></pre></p>
</xsl:template>

<xsl:template match="SS">
	<h1><xsl:apply-templates/></h1>
</xsl:template>

<xsl:template match="SH">
	<h2><xsl:apply-templates/></h2>
</xsl:template>

</xsl:stylesheet>

