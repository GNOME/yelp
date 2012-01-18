<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:msg="http://projects.gnome.org/yelp/gettext/"
                xmlns="http://projects.gnome.org/yelp/gettext/"
                version="1.0">

<xsl:output method="xml" encoding="UTF-8"/>

<xsl:variable name="source" select="document(/files/source/@href)"/>
<xsl:variable name="files" select="/files/file"/>

<xsl:key name="id.key" match="msg:msg" use="@id"/>

<xsl:template match="/files">
  <msg:l10n>
    <xsl:apply-templates select="$source/msg:l10n/msg:msg"/>
  </msg:l10n>
</xsl:template>

<xsl:template match="msg:msg">
  <xsl:variable name="id" select="@id"/>
  <msg:msg id="{$id}">
    <msg:msgstr xml:lang="C">
      <xsl:for-each select="msg:msgstr/node()">
        <xsl:copy/>
      </xsl:for-each>
    </msg:msgstr>
    <xsl:for-each select="$files">
      <xsl:for-each select="document(@href)/*">
        <xsl:variable name="lang" select="@xml:lang"/>
        <xsl:for-each select="key('id.key', $id)[1]">
          <msg:msgstr xml:lang="{$lang}">
            <xsl:for-each select="msg:msgstr/node()">
              <xsl:copy/>
            </xsl:for-each>
          </msg:msgstr>
        </xsl:for-each>
      </xsl:for-each>
    </xsl:for-each>
  </msg:msg>
</xsl:template>

</xsl:stylesheet>

