<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:param name="stylesheet_path" select="''"/>

<xsl:param name="yelp.icon.blockquote"/>
<xsl:param name="yelp.icon.caution"/>
<xsl:param name="yelp.icon.important"/>
<xsl:param name="yelp.icon.note"/>
<xsl:param name="yelp.icon.programlisting"/>
<xsl:param name="yelp.icon.tip"/>
<xsl:param name="yelp.icon.warning"/>

<xsl:param name="yelp.color.text"/>
<xsl:param name="yelp.color.anchor"/>
<xsl:param name="yelp.color.background"/>
<xsl:param name="yelp.color.rule"/>
<xsl:param name="yelp.color.base0"/>
<xsl:param name="yelp.color.base1"/>
<xsl:param name="yelp.color.base2"/>
<xsl:param name="yelp.color.base3"/>
<xsl:param name="yelp.color.selected0"/>
<xsl:param name="yelp.color.selected1"/>
<xsl:param name="yelp.color.selected2"/>
<xsl:param name="yelp.color.selected3"/>

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

h1, h2, h3, h4, h5, h6, h7 { color: </xsl:text>
<xsl:value-of select="$yelp.color.selected1"/><xsl:text>; }

div[class~="SH"] { margin-left: 1.2em; }
div[class~="SS"] { margin-left: 1.6em; }

span[class~="Section"] {margin-left: 0.4em; }

body { margin: 0em; padding: 0em; }
div[class~="body"] {
margin-left: 0.8em;
margin-right: 0.8em;
margin-bottom: 1.6em;
}

p, div { margin: 0em; }
p + *, div + * { margin-top: 1em; }

dl { margin: 0em; }
dl dd + dt { margin-top: 1em; }
dl dd {
  margin-top: 0.5em;
  margin-left: 2em;
  margin-right: 1em;
}
ol { margin-left: 2em; }
ul { margin-left: 2em; }
ul li { margin-right: 1em; }
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

<xsl:template match="IP">
  <xsl:choose>
    <xsl:when test="preceding-sibling::*[1][self::IP]"/>
    <xsl:otherwise>
      <dl>
        <xsl:apply-templates mode="IP.mode" select="."/>
      </dl>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template mode="IP.mode" match="IP">
  <dt>
    <xsl:apply-templates select="Tag"/>
  </dt>
  <dd>
    <xsl:apply-templates select="Tag/following-sibling::node()"/>
  </dd>
  <xsl:apply-templates mode="IP.mode"
                       select="following-sibling::*[self::IP][1]"/>
</xsl:template>

<xsl:template match="P">
  <p><xsl:apply-templates/></p>
</xsl:template>

<xsl:template match="ROW">
  <tr><xsl:apply-templates/></tr>
</xsl:template>

<xsl:template match="SS">
  <xsl:variable name="nextSH" select="following-sibling::SH[1]"/>
  <h3><xsl:apply-templates/></h3>
  <div class="SS">
    <xsl:choose>
      <xsl:when test="$nextSH">
        <xsl:variable
         name="nextSS"
         select="following-sibling::SS[following-sibling::SH = $nextSH][1]"/>
        <xsl:choose>
          <xsl:when test="$nextSS">
            <xsl:apply-templates
             select="following-sibling::*[following-sibling::SS = $nextSS]"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates
             select="following-sibling::*[following-sibling::SH = $nextSH]"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="nextSS" select="following-sibling::SS[1]"/>
        <xsl:choose>
          <xsl:when test="$nextSS">
            <xsl:apply-templates
             select="following-sibling::*[following-sibling::SS = $nextSS]"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates select="following-sibling::*"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </div>
</xsl:template>

<xsl:template match="SH">
  <xsl:variable name="nextSH" select="following-sibling::SH[1]"/>
  <h2><xsl:apply-templates/></h2>
  <div class="SH">
    <xsl:choose>
      <xsl:when test="$nextSH">
        <xsl:choose>
          <xsl:when test="following-sibling::SS[following-sibling::* = $nextSH]">
            <xsl:apply-templates
             select="following-sibling::SS[following-sibling::* = $nextSH]"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates
             select="following-sibling::*[following-sibling::* = $nextSH]"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:choose>
          <xsl:when test="following-sibling::SS">
            <xsl:apply-templates select="following-sibling::SS"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:apply-templates select="following-sibling::*"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </div>
</xsl:template>

<xsl:template match="TABLE">
  <table><xsl:apply-templates/></table>
</xsl:template>

<xsl:template match="Tag">
  <span class="Tag"><xsl:apply-templates/></span>
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
