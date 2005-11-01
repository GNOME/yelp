<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:import href="search-header.xsl"/>
<xsl:param name="help_icon"/>
<xsl:param name="help_icon_size"/>

<xsl:param name="yelp.javascript"/>
<xsl:param name="yelp.topimage"/>

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

<xsl:template match="search">
  <yelp:document href="results">
    <html>
      <head>
        <title>
          <xsl:value-of select="@title"/>
        </title>
        <script type="text/javascript">
          <xsl:attribute name="src">
            <xsl:value-of select="concat('file://', $yelp.javascript)"/>
          </xsl:attribute>
        </script>
        <style type="text/css"><xsl:text>
        body {
          margin: 0px;
          padding: 0px;
          background: white;
          background-image: url("file://</xsl:text>
          <xsl:value-of select="$yelp.topimage"/><xsl:text>");
          background-position: 0px 0px;
          background-repeat: no-repeat;
          font-family: Segoe, Sans;
          font-size: 11px;
          position: absolute;
          width: 100%;
        }
        div.content {
          margin-top: 36px;
          padding-left: 38px;
        }
        div.searchform {
          font-size: 24px;
          font-weight: bold;
          color: #000000;
          border-bottom: 2px dotted #999999;
          margin-bottom: 0px;
          padding-bottom: 0px;
        }
        form {
          margin: 0px;
          padding: 12px;
        }
        input {
          background: #ffffff;
          border: 1px solid #cccccc;
          padding: 4px;
          vertical-align: bottom;
        }
        input.button {
          font-weight: bold;
          padding: 3px;
          -moz-border-radius: 4px;
        }
        ul { font-size: 1em; margin-left: 0em; padding-left: 0em; }
        li {
          margin-left: 1em;
          padding-left: 0em;
          list-style-type: none;
        }
        dl { margin-left: 36px; padding-left: 0em; }
        dt { font-size: 1.2em; margin-top: 1em; }
        dd { margin-left: 1em; margin-top: 0.5em; }
        a { text-decoration: none; }
        a:hover { text-decoration: underline; }
        .match {
          padding-left: 3px;
          padding-right: 3px;
          -moz-border-radius: 3px;
          background: #eeeeee;
          font-weight: bold;
        }
        </xsl:text></style>
      </head>
      <body>
        <div class="content">
          <xsl:call-template name="search-header">
            <xsl:with-param name="search-term" select="@title"/>
          </xsl:call-template>
          <dl>
            <xsl:for-each select="result[@uri != '']">
<!-- Don't sort.  Program deals with that. -->
<!--              <xsl:sort order="descending" data-type="number" select="normalize-space(@score)"/> -->
              <xsl:apply-templates select="."/>
            </xsl:for-each>
          </dl>
        </div>
      </body>
    </html>
  </yelp:document>
</xsl:template>

<xsl:template match="result">
  <dt>
    <a href="{@uri}">
      <xsl:choose>
        <xsl:when test="@title != ''">
          <xsl:value-of select="@title"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@uri"/>
        </xsl:otherwise>
      </xsl:choose>
    </a>
    <xsl:choose>
      <xsl:when test="@base_title != ''">
        from <xsl:value-of select="@base_title"/>
      </xsl:when>
      <xsl:when test="@parent_uri != ''">
        from <xsl:value-of select="@parent_uri"/>
      </xsl:when>
    </xsl:choose>
  </dt>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="snippet">
  <dd>
    <xsl:apply-templates/>
  </dd>
</xsl:template>
<xsl:template match="font">
  <span class="match">
    <xsl:apply-templates/>
  </span>
</xsl:template>
<xsl:template match="b">
  <xsl:apply-templates/>
</xsl:template>
</xsl:stylesheet>
