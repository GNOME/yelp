<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.w3.org/1999/xhtml"
                version="1.0">

<!-- == yelp.common.css == -->
<xsl:template name="theme.html.css.custom">
  <xsl:param name="direction"/>
  <xsl:param name="left"/>
  <xsl:param name="right"/>
<xsl:text>
html {
  height: 100%;
}
body {
  padding: 0;
  background-color: </xsl:text><xsl:value-of select="$theme.color.background"/><xsl:text>;
}
div.head {
  padding: 0.2em 0 0 0;
  margin: 0 0 1em 0;
  background-color: </xsl:text>
    <xsl:value-of select="$theme.color.gray_background"/><xsl:text>;
  border-bottom: solid 1px </xsl:text>
    <xsl:value-of select="$theme.color.gray_border"/><xsl:text>;
}
div.body {
  margin: 0 12px 0 12px;
  padding: 0;
  border: none;
}
</xsl:text>
</xsl:template>

</xsl:stylesheet>
