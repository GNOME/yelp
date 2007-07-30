<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml"
                version="1.0">

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

<!-- == yelp.common.css == -->
<xsl:template name="yelp.common.css">
  <xsl:text>
    html {
      height: 100%;
    }
    body {
      -moz-box-sizing: border-box;
      position: relative;
      min-height: 100%;
      margin: 0px;
    }
    div[class~="body"] {
      -moz-box-sizing: border-box;
      position: relative;
      margin-top: 0px;
      padding: 12px;
      padding-bottom: 3em;
    }

    h1 { font-size: 1.4em; }
    h2 { font-size: 1.2em; }
    h1 + div h2[class~="title"] { margin-top: 2em; }
    h1, h2, h3, h4, h5, h6, h7 { color: </xsl:text>
    <xsl:value-of select="$yelp.color.gray.fg"/><xsl:text>; }
    h3 span[class~="title"] { border-bottom: none; }
    h4 span[class~="title"] { border-bottom: none; }
    h5 span[class~="title"] { border-bottom: none; }
    h6 span[class~="title"] { border-bottom: none; }
    h7 span[class~="title"] { border-bottom: none; }

    <!--/* Gecko seems to get selection color wrong on some themes */
    ::-moz-selection {
      background-color:  </xsl:text>
      <xsl:value-of select="$yelp.color.selected.bg"/><xsl:text>;
      color: </xsl:text>
      <xsl:value-of select="$yelp.color.selected.fg"/><xsl:text>;
    } -->

    div[class~="linktrail"] {
      -moz-box-sizing: border-box;
      font-size: 0.8em;
      white-space: nowrap;
      margin: 0px;
      padding: 3px;
      padding-left: 12px;
      padding-right: 12px;
      background-color: </xsl:text>
      <xsl:value-of select="$yelp.color.gray.bg"/><xsl:text>;
      border: none;
      border: solid 1px </xsl:text>
      <xsl:value-of select="$yelp.color.gray.bg.dark1"/><xsl:text>;
    }
    span[class~="linktrail"]:after {
      content: '&#x2003;/&#x2003;';
      color: </xsl:text>
      <xsl:value-of select="$yelp.color.gray.fg"/><xsl:text>;
    }
    a[class~="linktrail"] { text-decoration: none; }
    a[class~="linktrail"]:hover { text-decoration: underline; }
  </xsl:text>
</xsl:template>

</xsl:stylesheet>
