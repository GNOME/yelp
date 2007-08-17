<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml"
                version="1.0">

<!-- == yelp.common.css == -->
<xsl:template name="yelp.common.css">
  <xsl:text>
    html {
      height: 100%;
    }
    body {
      background-color: </xsl:text><xsl:value-of select="$theme.color.background"/><xsl:text>;
    }
    div.body {
      padding: 0;
      border: none;
    }

    <!--/* Gecko seems to get selection color wrong on some themes */
    ::-moz-selection {
      background-color:  </xsl:text>
      <xsl:value-of select="$yelp.color.selected.bg"/><xsl:text>;
      color: </xsl:text>
      <xsl:value-of select="$yelp.color.selected.fg"/><xsl:text>;
    } -->

  </xsl:text>
</xsl:template>

</xsl:stylesheet>
