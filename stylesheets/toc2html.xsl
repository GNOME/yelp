<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:param name="help_icon"/>

<xsl:output method="html"/>

<xsl:template match="toc">
  <yelp:document href="{@id}">
    <html>
      <head>
        <title>
          <xsl:value-of select="title[1]"/>
        </title>
        <style>
          body {
            padding-top: 8px;
            padding-left: 8px;
            padding-right: 12px;
          }
          h1 { font-size: 2em; }
          td[class~="leftbar"] {
            vertical-align: top;
          }
          td[class~="rightbar"] {
            vertical-align: top;
            padding-left: 8px;
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
          dt { font-size: 1.2em; margin-top: 0.5em; }
          dd { margin-left: 16px; margin-top: 0px; }

          div[class~="toc"] { margin-top: 8px; }
          span[class~="icon"] {
            margin-right: 0.8em;
          }
          img[class~="icon"] {
            border: none;
            width: 24px;
            height: 24px;
            vertical-align: middle;
          }
          span[class~="title"] {
            font-size: 1.2em;
            vertical-align: middle;
          }
          a { text-decoration: none; }
          a:hover { text-decoration: underline; }
        </style>
      </head>
      <body>
        <xsl:apply-templates mode="body.mode" select="."/>
      </body>
    </html>
  </yelp:document>
  <xsl:apply-templates select="toc[.//doc]"/>
</xsl:template>

<xsl:template mode="body.mode" match="toc[@id = 'index']">
  <table>
    <tr>
      <td class="leftbar">
        <img src="file://{$help_icon}"/>
      </td>
      <td class="rightbar">
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
      </td>
    </tr>
  </table>
</xsl:template>

<xsl:template mode="body.mode" match="toc">
  <table>
    <tr>
      <td class="leftbar">
        <img src="file://{$help_icon}"/>
      </td>
      <td class="rightbar">
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
            <dd>Lorem ipsum dolor sit amet, consectetuer adipiscing elit.
            Sed elementum enim. Praesent ac eros. Etiam eros ipsum, vulputate
            ac, ornare.</dd>
          </xsl:for-each>
        </dl>
      </td>
    </tr>
  </table>
</xsl:template>

</xsl:stylesheet>
