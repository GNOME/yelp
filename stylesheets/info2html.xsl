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

<xsl:param name="theme.color.text"/>
<xsl:param name="theme.color.background"/>
<xsl:param name="theme.color.text_light"/>
<xsl:param name="theme.color.link"/>
<xsl:param name="theme.color.link_visited"/>
<xsl:param name="theme.color.gray_background"/>
<xsl:param name="theme.color.gray_border"/>
<xsl:param name="theme.color.blue_background"/>
<xsl:param name="theme.color.blue_border"/>
<xsl:param name="theme.color.red_background"/>
<xsl:param name="theme.color.red_border"/>
<xsl:param name="theme.color.yellow_background"/>
<xsl:param name="theme.color.yellow_border"/>

<xsl:template match="/Info">
  <xsl:apply-templates select="Section"/>
</xsl:template>

<xsl:template match="Section">
  <yelp:document href="{@id}">
    <html>
      <head>
        <title>
          <xsl:value-of select="@name"/>
        </title>
        <style type="text/css">
          <xsl:call-template name="html.css"/>
        </style>
      </head>
      <body>
        <pre class="body">
          <xsl:apply-templates select="node()[not(self::Section)]"/>
        </pre>
      </body>
    </html>
  </yelp:document>
  <xsl:apply-templates select="Section"/>
</xsl:template>

<xsl:template match="para">
  <xsl:value-of select="node()"/>
  <xsl:text>
  </xsl:text>
</xsl:template>

<xsl:template match="para1">
  <xsl:value-of select="node()"/>
</xsl:template>

<xsl:template match="spacing">
  <xsl:value-of select="node()"/>
</xsl:template>

<xsl:template match="a">
  <xsl:element name="a">
    <xsl:attribute name="href"> <xsl:value-of select="@href"/></xsl:attribute>
    <xsl:value-of select="node()"/>
  </xsl:element>
</xsl:template>

<xsl:template match="img">
  <xsl:element name="a">
    <xsl:attribute name="href"> <xsl:value-of select="@src"/></xsl:attribute>
      <xsl:element name="img">
        <xsl:attribute name="src"> <xsl:value-of select="@src"/></xsl:attribute>
      </xsl:element>
  </xsl:element>
</xsl:template>

<xsl:template match="menuholder">
  <xsl:apply-templates select="node()[not(self::menuholder)]"/>
</xsl:template>

<xsl:template match="noteholder">
  <xsl:apply-templates select="node()[not(self::noteholder)]"/>
</xsl:template>

<xsl:template name="html.css"><xsl:text>
h1 { font-size: 1.6em; font-weight: bold; }
h2 { font-size: 1.4em; font-weight: bold; }
h3 { font-size: 1.2em; font-weight: bold; }

h1, h2, h3, h4, h5, h6, h7 { color: </xsl:text>
<xsl:value-of select="$theme.color.text"/><xsl:text>; }

body { margin: 0em; padding: 0em; }
pre[class~="body"] {
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
img { border: none; }
</xsl:text></xsl:template>

</xsl:stylesheet>
