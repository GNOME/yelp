<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:param name="help_icon"/>
<xsl:param name="help_icon_size"/>

<xsl:param name="yelp.color.text"/>
<xsl:param name="yelp.color.anchor"/>
<xsl:param name="yelp.color.background"/>
<xsl:param name="yelp.color.base0"/>
<xsl:param name="yelp.color.base1"/>
<xsl:param name="yelp.color.base2"/>
<xsl:param name="yelp.color.base3"/>
<xsl:param name="yelp.color.selected0"/>
<xsl:param name="yelp.color.selected1"/>
<xsl:param name="yelp.color.selected2"/>
<xsl:param name="yelp.color.selected3"/>

<xsl:template match="toc">
  <yelp:document href="{@id}">
    <html>
      <head>
        <title>
          <xsl:value-of select="title[1]"/>
        </title>
        <style><xsl:text>
        body {
          padding-top: 0em;
          padding-left: 0em;
          padding-right: 2em;
        }
        h1 { font-size: 1.6em; margin-bottom: 0.4em; }
        h1 img { float: right; }
        h1 { color: </xsl:text>
        <xsl:value-of select="$yelp.color.selected1"/><xsl:text>; }
        div[class~="leftbar"] {
          width: 200px;
          text-align: center;
        }
        div[class~="body"] {
          background-image: url("</xsl:text>
          <xsl:value-of select="$help_icon"/><xsl:text>");
          background-position: </xsl:text>
          <xsl:value-of select="(192 - $help_icon_size) div 2"/><xsl:text>px 0px;
          background-repeat: no-repeat;
          height: 200px;
        }
        div[class~="rightbar"] {
          padding-left: 200px;
          padding-bottom: 1em;
        }
        div[class~="tocs"] { border-top: solid 1px </xsl:text>
        <xsl:value-of select="$yelp.color.selected0"/><xsl:text>; }
        div[class~="docs"] { border-top: solid 1px </xsl:text>
        <xsl:value-of select="$yelp.color.selected0"/><xsl:text>; }
        ul { margin-left: 0em; padding-left: 0em; }
        li {
          margin-top: 0.5em;
          margin-left: 0em;
          padding-left: 0em;
          font-size: 1.2em;
          list-style-type: none;
        }
        dl { margin-left: 0em; padding-left: 0em; }
        dt { font-size: 1.2em; margin-top: 1em; }
        dd { margin-left: 1em; margin-top: 0.5em; }
        a { text-decoration: none; }
        a:hover { text-decoration: underline; }
        </xsl:text></style>
      </head>
      <body>
        <xsl:apply-templates mode="body.mode" select="."/>
      </body>
    </html>
  </yelp:document>
  <xsl:apply-templates select="toc[.//doc]"/>
</xsl:template>

<xsl:template mode="body.mode" match="toc">
  <div class="body">
    <div class="rightbar">
      <h1>
        <xsl:if test="icon">
          <img src="{icon/@file}"/>
        </xsl:if>
        <xsl:apply-templates select="title[1]/node()"/>
      </h1>
      <xsl:if test="toc[.//doc]">
        <div class="tocs">
          <ul>
            <xsl:for-each select="toc[../@id = 'index' or .//doc]">
              <xsl:sort select="number(../@id = 'index') * position()"/>
              <xsl:sort select="title"/>
              <li class="toc">
                <a href="x-yelp-toc:{@id}">
                  <xsl:apply-templates select="title[1]/node()"/>
                </a>
              </li>
            </xsl:for-each>
          </ul>
        </div>
      </xsl:if>
      <xsl:if test="doc">
        <div class="docs">
          <dl>
            <xsl:for-each select="doc">
              <xsl:sort select="title"/>
              <dt class="doc">
                <a href="{@href}" title="{@href}">
                  <xsl:if test="tooltip">
                    <xsl:attribute name="title">
                      <xsl:value-of select="tooltip"/>
                    </xsl:attribute>
                  </xsl:if>
                  <xsl:value-of select="title"/>
                </a>
              </dt>
              <dd>
                <xsl:value-of select="description"/>
              </dd>
            </xsl:for-each>
          </dl>
        </div>
      </xsl:if>
    </div>
  </div>
</xsl:template>

</xsl:stylesheet>
