<?xml version='1.0' encoding='UTF-8'?><!-- -*- indent-tabs-mode: nil -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:yelp="http://www.gnome.org/yelp/ns"
                xmlns="http://www.w3.org/1999/xhtml"
                version="1.0">


<xsl:template name="search-header">
  <xsl:param name="search-term"/>
  <div class="searchform">
      Search help: <input size="24" id="search-entry" type="text" name="term" onkeypress="if (event.keyCode == 13) submit_search()" value="{$search-term}"/>
      <input class="button" type="button" onclick="submit_search()" value="Go"/>
  </div>
</xsl:template>
</xsl:stylesheet>
