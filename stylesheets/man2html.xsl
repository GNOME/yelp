<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->

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
        <xsl:apply-templates select="TH"/>
        <xsl:apply-templates select="SH"/>
      </div>
    </body>
  </html>
</xsl:template>

<xsl:template name="html.css"><xsl:text>
h1 { font-size: 1.6em; font-weight: bold; }
h2 { font-size: 1.4em; font-weight: bold; }
h3 { font-size: 1.2em; font-weight: bold; }

div[class~="SH"] { margin-left: 1.2em; }
div[class~="SS"] { margin-left: 1.2em; }

span[class~="Section"] {margin-left: 0.4em; }

body { margin: 0em; padding: 0em; }
div[class~="body"] {
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

<xsl:template match="CELL">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="I | fI">
  <i><xsl:apply-templates/></i>
</xsl:template>

<xsl:template match="P">
  <p><xsl:apply-templates/></p>
</xsl:template>

<xsl:template match="ROW">
  <tr><xsl:apply-templates/></tr>
</xsl:template>

<xsl:template match="SS">
  <h3><xsl:apply-templates/></h3>
</xsl:template>

<xsl:template match="SH">
  <xsl:variable name="nextSH" select="following-sibling::SH[1]"/>
  <h2><xsl:apply-templates/></h2>
  <div class="SH">
    <xsl:choose>
      <xsl:when test="$nextSH">
        <xsl:apply-templates
         select="following-sibling::*[following-sibling::* = $nextSH]"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="following-sibling::*"/>
      </xsl:otherwise>
    </xsl:choose>
  </div>
</xsl:template>

<xsl:template match="TABLE">
  <table><xsl:apply-templates/></table>
</xsl:template>

<xsl:template match="TH">
  <h1>
    <span class="Title">
      <xsl:apply-templates select="Title/node()"/>
    </span>
    <span class="Section">
      <xsl:text>(</xsl:text>
      <xsl:apply-templates select="Section/node()"/>
      <xsl:text>)</xsl:text>
    </span>
  </h1>
</xsl:template>

<xsl:template match="UR">
  <a>
    <xsl:attribute name="href">
      <xsl:value-of select="URI" />
    </xsl:attribute>
    <xsl:apply-templates/>
  </a>
</xsl:template>

<xsl:template match="URI"/>

<xsl:template match="*">
  <xsl:message>
    <xsl:text>Unmatched element: </xsl:text>
    <xsl:value-of select="local-name(.)"/>
  </xsl:message>
  <xsl:apply-templates/>
</xsl:template>

</xsl:stylesheet>
