<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:include href="yelp-common.xsl"/>

<xsl:param name="stylesheet_path" select="''"/>
<xsl:param name="linktrail"/>

<xsl:param name="yelp.javascript"/>

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
      <script type="text/javascript">
        <xsl:attribute name="src">
          <xsl:value-of select="concat('file://', $yelp.javascript)"/>
        </xsl:attribute>
      </script>
    </head>
    <body>
      <xsl:call-template name="html.linktrail"/>
      <div class="body">
        <xsl:apply-templates select="TH"/>
        <xsl:apply-templates select="SH"/>
      </div>
    </body>
  </html>
</xsl:template>

<xsl:template name="html.css">
  <xsl:call-template name="yelp.common.css"/>
  <xsl:text>
    div[class~="SH"] { margin-left: 1.2em; }
    div[class~="SS"] { margin-left: 1.6em; }

    span[class~="Section"] { margin-left: 0.4em; }
  </xsl:text>
</xsl:template>

<xsl:template name="html.linktrail">
  <div class="linktrail" id="linktrail">
    <xsl:call-template name="html.linktrail.one">
      <xsl:with-param name="str" select="$linktrail"/>
    </xsl:call-template>
  </div>
</xsl:template>

<xsl:template name="html.linktrail.one">
  <xsl:param name="str"/>
  <xsl:variable name="id" select="substring-before($str, '|')"/>
  <xsl:variable name="post_id" select="substring-after($str, '|')"/>

  <span class="linktrail">
    <a class="linktrail" href="x-yelp-toc:{$id}">
      <xsl:choose>
        <xsl:when test="contains($post_id, '|')">
          <xsl:value-of select="substring-before($post_id, '|')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$post_id"/>
        </xsl:otherwise>
      </xsl:choose>
    </a>
  </span>

  <xsl:if test="contains($post_id, '|')">
    <xsl:call-template name="html.linktrail.one">
      <xsl:with-param name="str" select="substring-after($post_id, '|')"/>
    </xsl:call-template>
  </xsl:if>
</xsl:template>

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
