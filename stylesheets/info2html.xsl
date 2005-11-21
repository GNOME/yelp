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

<xsl:param name="yelp.color.fg"/>
<xsl:param name="yelp.color.bg"/>
<xsl:param name="yelp.color.anchor"/>
<xsl:param name="yelp.color.rule"/>
<xsl:param name="yelp.color.gray.fg"/>
<xsl:param name="yelp.color.gray.bg"/>
<xsl:param name="yelp.color.gray.bg.dark1"/>
<xsl:param name="yelp.color.gray.bg.dark2"/>
<xsl:param name="yelp.color.gray.bg.dark3"/>
<xsl:param name="yelp.color.selected.fg"/>
<xsl:param name="yelp.color.selected.bg"/>
<xsl:param name="yelp.color.selected.bg.dark1"/>
<xsl:param name="yelp.color.selected.bg.dark2"/>
<xsl:param name="yelp.color.selected.bg.dark3"/>
<xsl:param name="yelp.color.admon.fg"/>
<xsl:param name="yelp.color.admon.bg"/>
<xsl:param name="yelp.color.admon.bg.dark1"/>
<xsl:param name="yelp.color.admon.bg.dark2"/>
<xsl:param name="yelp.color.admon.bg.dark3"/>

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
        <xsl:if test="Section">
          <ul>
            <xsl:for-each select="Section">
              <li>
                <xsl:value-of select="@name"/>
              </li>
            </xsl:for-each>
          </ul>
        </xsl:if>
        <pre class="body">
          <xsl:value-of select="node()[not(self::Section)]"/>
        </pre>
      </body>
    </html>
  </yelp:document>
  <xsl:apply-templates select="Section"/>
</xsl:template>

<xsl:template name="html.css"><xsl:text>
h1 { font-size: 1.6em; font-weight: bold; }
h2 { font-size: 1.4em; font-weight: bold; }
h3 { font-size: 1.2em; font-weight: bold; }

h1, h2, h3, h4, h5, h6, h7 { color: </xsl:text>
<xsl:value-of select="$yelp.color.gray.fg"/><xsl:text>; }

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
</xsl:text></xsl:template>

</xsl:stylesheet>
