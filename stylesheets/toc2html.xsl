<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:param name="help_icon"/>
<xsl:param name="help_icon_size"/>

<xsl:output method="html"/>

<xsl:template match="toc">
  <yelp:document href="{@id}">
    <html>
      <head>
        <title>
          <xsl:value-of select="title[1]"/>
        </title>
        <style><xsl:text>
        body {
          padding-top: 8px;
          padding-left: 8px;
          padding-right: 12px;
        }
        h1 { font-size: 2em; }
        div[class~="leftbar"] {
          width: 200px;
          text-align: center;
        }
        div[class~="body"] {
          background-image: url("</xsl:text>
          <xsl:value-of select="$help_icon"/><xsl:text>");
          background-position: </xsl:text>
          <xsl:value-of select="(192 - $help_icon_size) div 2"/><xsl:text> 0;
          background-repeat: no-repeat;
          height: 200px;
        }
        div[class~="rightbar"] {
          padding-left: 200px;
        }
        ul { margin-left: 16px; padding-left: 0px; }
        li {
          margin-top: 0.4em;
          margin-left: 0px;
          padding-left: 0px;
          font-size: 1.2em;
          list-style-type: square;
        }
        dl { margin-left: 0px; padding-left: 0px; }
        dt { font-size: 1.2em; margin-top: 0.6em; }
        dd { margin-left: 16px; margin-top: 0.2em; }
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

<xsl:template mode="body.mode" match="toc[@id = 'index']">
  <div class="body">
    <div class="rightbar">
      <h1>
        <xsl:apply-templates select="title[1]/node()"/>
      </h1>
      <ul>
        <xsl:for-each select="toc">
          <li class="toc">
            <a href="x-yelp-toc:{@id}">
              <xsl:apply-templates select="title[1]/node()"/>
            </a>
          </li>
        </xsl:for-each>
      </ul>
    </div>
  </div>
</xsl:template>

<xsl:template mode="body.mode" match="toc">
  <div class="body">
    <div class="rightbar">
      <h1>
        <xsl:apply-templates select="title[1]/node()"/>
      </h1>
      <ul>
        <xsl:for-each select="toc[.//doc]">
          <xsl:sort select="title"/>
          <li class="toc">
            <a href="x-yelp-toc:{@id}">
              <xsl:apply-templates select="title[1]/node()"/>
            </a>
          </li>
        </xsl:for-each>
      </ul>
      <dl>
        <xsl:for-each select="doc">
          <xsl:sort select="title"/>
          <dt class="doc">
            <a href="{@href}">
              <xsl:value-of select="title"/>
            </a>
          </dt>
          <dd>
            <xsl:value-of select="description"/>
          </dd>
        </xsl:for-each>
      </dl>
    </div>
  </div>
</xsl:template>

</xsl:stylesheet>
