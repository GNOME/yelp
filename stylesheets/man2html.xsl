<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="yelp"
                version="1.0">

<xsl:output method="html" encoding="UTF-8"/>

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
        <xsl:value-of select="//TH/Title"/>
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

    span[class~="R"] { font-family: serif; }
    span[class~="Section"] { margin-left: 0.4em; }
  
    dd { padding-bottom: 10px; }
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

<xsl:template match="br">
  <xsl:apply-templates/><br/>
</xsl:template>

<!-- ignore anything in the Indent,Count,sp element for now -->
<xsl:template match="Indent" />
<xsl:template match="Count" />
<xsl:template match="sp" />

<xsl:template match="B | fB">
  <b><xsl:apply-templates/></b>
</xsl:template>

<xsl:template match="CELL">
  <td><xsl:apply-templates/></td>
</xsl:template>

<xsl:template match="I | fI">
  <i><xsl:apply-templates/></i>
</xsl:template>

<xsl:template match="R | fR">
  <span class="R"><xsl:apply-templates/></span>
</xsl:template>

<xsl:template match="Verbatim">
  <pre>
    <xsl:choose>
      <xsl:when test="node()[1]/self::text()">
        <xsl:variable name="node" select="node()[1]"/>
        <xsl:choose>
          <xsl:when test="starts-with(string($node), '&#x000A;')">
            <xsl:value-of select="substring-after(string($node), '&#x000A;')"/>
            <xsl:apply-templates select="node()[position() != 1]"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="string($node)"/>
            <xsl:apply-templates select="node()[position() != 1]"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates />
      </xsl:otherwise>
    </xsl:choose>
  </pre>
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
    <xsl:choose>
      <xsl:when test="Tag">
        <xsl:apply-templates select="Tag"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates/>
      </xsl:otherwise>
    </xsl:choose>
  </dt>
  <dd>
    <xsl:apply-templates select="Tag/following-sibling::node()"/>
  </dd>
  <xsl:apply-templates mode="IP.mode"
                       select="following-sibling::*[1][self::IP]"/>
</xsl:template>

<xsl:template match="P">
  <p><xsl:apply-templates/></p>
</xsl:template>

<xsl:template match="ROW">
  <tr><xsl:apply-templates/></tr>
</xsl:template>

<xsl:template match="SS">
  <xsl:variable name="nextSH" select="following-sibling::SH[1]"/>
  <xsl:variable name="nextSS"
                select="following-sibling::SS[not($nextSH) or
                                              following-sibling::SH[1] = $nextSH][1]"/>
  <h3><xsl:apply-templates/></h3>
  <div class="SS">
    <xsl:choose>
      <xsl:when test="$nextSS">
        <xsl:apply-templates
         select="following-sibling::*[following-sibling::SS[1] = $nextSS and 
                                      following-sibling::SS[1]/@id = $nextSS/@id]"/>
      </xsl:when>
      <xsl:when test="$nextSH">
        <xsl:apply-templates
         select="following-sibling::*[following-sibling::SH[1] = $nextSH and
                                      following-sibling::SH[1]/@id = $nextSH/@id]"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="following-sibling::*"/>
      </xsl:otherwise>
    </xsl:choose>
  </div>
</xsl:template>

<xsl:template match="SH">
  <xsl:variable name="nextSH" select="following-sibling::SH[1]"/>
  <xsl:variable name="nextSS"
                select="following-sibling::SS[not($nextSH) or
                                              following-sibling::SH[1] = $nextSH]"/>
  <h2><xsl:apply-templates/></h2>
  <div class="SH">
    <xsl:choose>
      <xsl:when test="$nextSS">
        <xsl:apply-templates
         select="following-sibling::*[following-sibling::SS[1] = $nextSS[1] and
                                      following-sibling::SS[1]/@id = $nextSS[1]/@id]"/>  
        <xsl:apply-templates select="$nextSS"/>
      </xsl:when>
      <xsl:when test="$nextSH">
        <xsl:apply-templates
         select="following-sibling::*[following-sibling::SH[1] = $nextSH and
                                      following-sibling::SH[1]/@id = $nextSH/@id]"/>
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

<xsl:template match="UN">
  <a name="text()" id="text()"/>
</xsl:template>

<!-- these are all for mdoc (BSD) man page support -->

<!-- these are just printed out -->
<xsl:template match="An | Dv | Er | Ev | Ic | Li | St">
  <xsl:text>
</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<!-- these are italicized -->
<xsl:template match="Ad | Ar | Fa | Ot | Pa | Va | Vt">
  <i><xsl:apply-templates/></i>
</xsl:template>

<!-- these are bold -->
<xsl:template match="Cd | Cm | Fd | Ic | Nm">
  <b><xsl:apply-templates/></b>
</xsl:template>

<!-- Function call - TODO need to do the ( , ) here -->
<xsl:template match="Fn | Fo | Fc">
  <i><xsl:apply-templates/></i>
</xsl:template>

<!-- Cross reference -->
<xsl:template match="Xr">
  <xsl:variable name="manpage" select="substring-before(string(.), ' ')"/>
  <xsl:variable name="section" select="substring-before(substring-after(string(.), ' '), ' ')"/>
  <xsl:variable name="extra"   select="substring-after(substring-after(string(.), ' '), ' ')"/>
  <a>
    <xsl:attribute name="href">
      <xsl:text>man:</xsl:text>
      <xsl:value-of select="$manpage"/>
      <xsl:text>(</xsl:text>
      <xsl:value-of select="$section"/>
      <xsl:text>)</xsl:text>
    </xsl:attribute>
    <xsl:value-of select="$manpage"/>
    <xsl:text>(</xsl:text>
    <xsl:value-of select="$section"/>
    <xsl:text>)</xsl:text>
  </a>
  <xsl:value-of select="$extra"/>
</xsl:template>

<!-- Option -->
<xsl:template match="Op | Oo | Oc">
  <xsl:text> [</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>]</xsl:text>
</xsl:template>

<!-- Trade or type name (small Caps). -->
<xsl:template match="Tn">
  <xsl:variable name="txt" select="string(child::text())"/>
    <xsl:text> </xsl:text>
    <xsl:value-of select="translate($txt, 'abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
  <xsl:apply-templates select="*"/>
</xsl:template>

<xsl:template match="Nd">
  <xsl:text> - </xsl:text>
  <xsl:apply-templates />
</xsl:template>

<xsl:template match="Fl">
  <xsl:text>-</xsl:text>
  <b><xsl:apply-templates select="child::text()"/></b>
  <xsl:apply-templates select="*"/>
</xsl:template>

<xsl:template match="Bl">
  <dl>
    <xsl:for-each select="It">
      <xsl:choose>
        <xsl:when test="ItTag">
          <dt><xsl:apply-templates select="ItTag"/></dt>
          <dd>
            <xsl:apply-templates select="ItTag/following-sibling::node()"/>
          </dd>
        </xsl:when>
        <xsl:otherwise>
          <dt>
            <xsl:text>•</xsl:text>
          </dt>
          <dd>
            <xsl:apply-templates />
          </dd>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </dl>
</xsl:template>

<xsl:template match="ItTag">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="*">
  <xsl:message>
    <xsl:text>Unmatched element: </xsl:text>
    <xsl:value-of select="local-name(.)"/>
  </xsl:message>
  <xsl:apply-templates/>
</xsl:template>

</xsl:stylesheet>
