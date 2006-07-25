<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="yelp"
                version="1.0">

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
          background-image: url("file://</xsl:text>
          <xsl:value-of select="$yelp.topimage"/><xsl:text>");
          background-position: 0px 0px;
          background-repeat: no-repeat;
          position: absolute;
          width: 100%;
        }
        div.content {
          margin-top: 36px;
          padding-left: 38px;
        }
        div.header {
          font-weight: bold;
          border-bottom: 2px solid </xsl:text>
          <xsl:value-of select="$yelp.color.gray.bg.dark1"/><xsl:text>;
          margin-bottom: 0px;
          padding-bottom: 0px;
        }
        div.footer {
          padding-left: 38px;
        }
        ul { margin-left: 0em; padding-left: 0em; }
        li {
          margin-left: 1em;
          padding-left: 0em;
          list-style-type: none;
        }
        dl { margin-left: 36px; padding-left: 0em; }
        dt { margin-top: 1em; }
        dd { margin-left: 1em; margin-top: 0.5em; }
        a { text-decoration: none; }
        a:hover { text-decoration: underline; }
        </xsl:text></style>
      </head>
      <body>
        <div class="content">
          <div class="header">
            <h1><xsl:value-of select="title" /></h1>
          </div>
          <xsl:if test="text">
            <p>
              <xsl:value-of select="text" />
            </p>
          </xsl:if>
          <dl>
            <xsl:for-each select="result[@uri != '' and position() &lt; 21]">
              <!-- Don't sort.  Program deals with that. We do however 
                   need it for slow search-->
                <xsl:sort order="descending" data-type="number" select="normalize-space(@score)"/>
              <xsl:apply-templates select="."/>
            </xsl:for-each>
          </dl>
        </div>
        <div class="footer">
            <br/>
            <xsl:apply-templates select="online"/><xsl:apply-templates select="online1"/>
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

<xsl:template match="em">
  <strong>
    <xsl:apply-templates/>
  </strong>
</xsl:template>
<xsl:template match="score">
  <!--Empty to kill score result-->
</xsl:template>

<xsl:template match="online">
  <hr/>
  <br/>
  <xsl:apply-templates/>
  <a href="{@href}">
    <xsl:value-of select="@name"/>
  </a>
</xsl:template>
<xsl:template match="online1">
  <xsl:apply-templates/>
</xsl:template>

</xsl:stylesheet>
