<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'>

<xsl:import href="http://docbook.sourceforge.net/release/xsl/1.48/html/docbook.xsl"/>
<xsl:include href="yelp-custom.xsl"/>

<xsl:param name="gdb_docname" />

<xsl:param name="gdb_pathname" />

<xsl:param name="gdb_rootid" select="string()" />

<xsl:output encoding="ISO-8859-1" />

<!--
<xsl:template match="graphic">
  <p>
    <img>
    <xsl:attribute name="src">
    <xsl:text>file://</xsl:text>
    <xsl:value-of select="$gdb_pathname"/>
    <xsl:text>/figures/example_panel.png</xsl:text>
    </xsl:attribute>
    </img>
  </p>
</xsl:template>
-->

<xsl:template name="process.image">
  <!-- When this template is called, the current node should be  -->
  <!-- a graphic, inlinegraphic, imagedata, or videodata. All    -->
  <!-- those elements have the same set of attributes, so we can -->
  <!-- handle them all in one place.                             -->
  <xsl:param name="tag" select="'img'"/>
  <xsl:param name="alt"/>

  <xsl:variable name="filename">
    <xsl:choose>
      <xsl:when test="local-name(.) = 'graphic'
                      or local-name(.) = 'inlinegraphic'">
        <xsl:choose>
          <xsl:when test="@fileref">
            <xsl:value-of select="@fileref"/>
          </xsl:when>
          <xsl:when test="@entityref">
            <xsl:value-of select="unparsed-entity-uri(@entityref)"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message>
              <xsl:text>A fileref or entityref is required on </xsl:text>
              <xsl:value-of select="local-name(.)"/>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <!-- imagedata, videodata, audiodata -->
        <xsl:call-template name="mediaobject.filename">
          <xsl:with-param name="object" select=".."/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="width">
    <xsl:choose>
      <xsl:when test="@scale"><xsl:value-of select="@scale"/>%</xsl:when>
      <xsl:when test="@width"><xsl:value-of select="@width"/></xsl:when>
      <xsl:otherwise></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="height">
    <xsl:choose>
      <xsl:when test="@scale"></xsl:when>
      <xsl:when test="@depth"><xsl:value-of select="@depth"/></xsl:when>
      <xsl:otherwise></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="align">
    <xsl:value-of select="@align"/>
  </xsl:variable>

  <xsl:element name="{$tag}">
    <xsl:attribute name="src">

 <xsl:text>file://</xsl:text>
    <xsl:value-of select="$gdb_pathname"/>
    <xsl:text>/</xsl:text>
      <xsl:value-of select="$filename"/>
    </xsl:attribute>

    <xsl:if test="$align != ''">
      <xsl:attribute name="align">
        <xsl:value-of select="$align"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:if test="$height != ''">
      <xsl:attribute name="height">
        <xsl:value-of select="$height"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:if test="$width != ''">
      <xsl:attribute name="width">
        <xsl:value-of select="$width"/>
      </xsl:attribute>
    </xsl:if>
    <xsl:if test="$alt != ''">
      <xsl:attribute name="alt">
        <xsl:value-of select="$alt"/>
      </xsl:attribute>
    </xsl:if>
  </xsl:element>
</xsl:template>

<!-- we are cheating here - but there isn't another way-->

<xsl:template name="href.target">
   <xsl:param name="object" select="."/>
   <xsl:text>ghelp:</xsl:text>
   <xsl:value-of select="$gdb_docname"/>
   <xsl:text>?</xsl:text>
   <xsl:value-of select="$object/@id"/>
</xsl:template>

<!-- make a small custom css stylesheet reside in <head>...</head> -->

<xsl:template name="user.head.content">
   <style>
     <xsl:text>
       BODY {font-size: 10pt}
       LI {margin-left: 3em}
       LI {margin-bottom: 0}
       LI P {margin-bottom: 0}
       OL {margin-top: 0}
       OL {margin-bottom: 0}
       UL {margin-top: 0}
       UL {margin-bottom: 0}
       DD {margin-left: 2em}
       DL {margin-top: 0}
       DL {margin-bottom: 0}
     </xsl:text>
   </style>
</xsl:template>

<xsl:template name="next.link.cell">
<xsl:param name="object" select="."/>
   <td align="right"><a accesskey="n">
   <xsl:attribute name="href">
      <xsl:call-template name="href.target">
         <xsl:with-param name="object" select="$object"/>
      </xsl:call-template>
   </xsl:attribute>
   <xsl:text>Next &gt;&gt;&gt;</xsl:text>
   </a></td>
</xsl:template>

<xsl:template name="prev.link.cell">
<xsl:param name="object" select="."/>
   <td align="left"><a accesskey="p">
   <xsl:attribute name="href">
      <xsl:call-template name="href.target">
         <xsl:with-param name="object" select="$object"/>
      </xsl:call-template>
   </xsl:attribute>
   <xsl:text>&lt;&lt;&lt; Prev</xsl:text>
   </a></td>
</xsl:template>

<xsl:template name="article.toc.ref">
<xsl:attribute name="href">
  <xsl:text>ghelp:</xsl:text>
  <xsl:value-of select="$gdb_docname"/>
</xsl:attribute>
</xsl:template>

<xsl:template name="indirect.prev.cell">
<xsl:param name="object" select="."/>
<xsl:choose>
  <xsl:when test="count($object/sect2) > 0">
    <xsl:call-template name="prev.link.cell">
      <xsl:with-param name="object" select="$object/sect2[last()]"/>
    </xsl:call-template>
  </xsl:when>
  <xsl:otherwise>
    <xsl:call-template name="prev.link.cell">
      <xsl:with-param name="object" select="$object"/>
    </xsl:call-template>
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template name="indirect.next.cell">
<xsl:param name="object" select="."/>
<xsl:choose>
  <xsl:when test="count($object/sect2) > 0">
    <xsl:call-template name="next.link.cell">
      <xsl:with-param name="object" select="$object/sect2[1]"/>
    </xsl:call-template>
  </xsl:when>
  <xsl:otherwise>
    <xsl:call-template name="next.link.cell">
      <xsl:with-param name="object" select="$object"/>
    </xsl:call-template>
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<!--
  article.chunk.prev and article.chunk.next need to be augumented
  to take parents siblings children into account
-->

<xsl:template name="article.chunk.prev">
<xsl:param name="node" select="."/>
  <td>
    <xsl:choose>

      <xsl:when test="count($node/preceding-sibling::*) > 1">
        <xsl:call-template name="prev.link.cell">
          <xsl:with-param name="object" select="$node/preceding-sibling::*[1]"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:when test="local-name($node)='sect2' and count($node/../preceding-sibling::*) > 1">
        <xsl:call-template name="indirect.prev.cell">
          <xsl:with-param name="object" select="$node/../preceding-sibling::*[position()=last()]"/>
        </xsl:call-template>
      </xsl:when>

      <!-- we need to treat the first sect1 specially -->
      <xsl:when test="$node=/article/sect1[1]">
        <td><a accesskey="p">
          <xsl:call-template name="article.toc.ref"/>
          <xsl:text>&lt;&lt;&lt; Prev</xsl:text>
        </a></td>
      </xsl:when>

      <xsl:otherwise>
        <xsl:text>&lt;&lt;&lt; Prev</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </td>
</xsl:template>

<xsl:template name="article.chunk.up">
<xsl:param name="node" select="."/>
<xsl:param name="doit" select="1"/>
<xsl:choose>
  <xsl:when test="$doit=1">
   <td align="center">
     <xsl:choose>
       <xsl:when test="local-name($node)='sect1' or local-name($node)='sect2'">
         <a accesskey="u">
           <xsl:call-template name="article.toc.ref"/>
             <xsl:text>TOC</xsl:text>
         </a>
       </xsl:when>
     </xsl:choose>
   </td>
  </xsl:when>
  <xsl:otherwise>
    <td></td>
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template name="article.chunk.next">
<xsl:param name="node" select="."/>
    <xsl:choose>
      <xsl:when test="count($node/following-sibling::*) > 0"> 
        <xsl:call-template name="next.link.cell">
          <xsl:with-param name="object" select="$node/following-sibling::*[1]"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="local-name($node)='sect2' and count($node/../following-sibling::*) > 0">
        <xsl:call-template name="indirect.next.cell">
          <xsl:with-param name="object" select="$node/../following-sibling::*[1]"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
    	  <td align="right">
          <xsl:text>Next &gt;&gt;&gt;</xsl:text>
    	  </td>
      </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template name="article.chunk.navigate">
<xsl:param name="node" select="."/>
<xsl:param name="link-toc" select="1"/>
  <tr>
    <xsl:call-template name="article.chunk.prev">
      <xsl:with-param name="node" select="$node"/>
    </xsl:call-template>
    <xsl:call-template name="article.chunk.up">
      <xsl:with-param name="node" select="$node"/>
      <xsl:with-param name="doit" select="$link-toc"/>
    </xsl:call-template>
    <xsl:call-template name="article.chunk.next">
      <xsl:with-param name="node" select="$node"/>
    </xsl:call-template>
  </tr>
</xsl:template>

<xsl:template name="article.render.chunk">
<xsl:param name="node" select="."/>
  <p align="center"><xsl:value-of select="/article/articleinfo/title"/></p>
  <table width="100%">
    <xsl:call-template name="article.chunk.navigate">
      <xsl:with-param name="node" select="$node"/>
      <xsl:with-param name="link-toc" select="0"/>
    </xsl:call-template>
  </table>
  <xsl:element name="hr" />
  <xsl:apply-templates select="$node"/>
  <xsl:element name="hr" />
  <table width="100%">
    <xsl:call-template name="article.chunk.navigate">
      <xsl:with-param name="node" select="$node"/>
    </xsl:call-template>
  </table>
</xsl:template>

<xsl:template name="make.toc.navbar">
  <table width="100%">
    <tr>
      <td><a accesskey="p">
        <xsl:attribute name="href">
          <xsl:text>ghelp:</xsl:text>
          <xsl:value-of select="$gdb_docname"/>
          <xsl:text>?title-page</xsl:text>
        </xsl:attribute>
        <xsl:text>&lt;&lt;&lt; Prev</xsl:text>
      </a></td>
      <td></td>
      <xsl:call-template name="next.link.cell">
        <xsl:with-param name="object" select="sect1[1]"/>
      </xsl:call-template>
    </tr>
  </table>
</xsl:template>

<xsl:template name="make.titlep.navbar">
  <table width="100%">
    <tr>
      <td>
        <xsl:text>&lt;&lt;&lt; Prev</xsl:text>
      </td>
      <td align="center"><xsl:text>Up</xsl:text></td>
      <td align="right"><a accesskey="n">
        <xsl:call-template name="article.toc.ref"/>
          <xsl:text>Next &gt;&gt;&gt;</xsl:text>
      </a></td>
    </tr>
  </table>
</xsl:template>


<xsl:template name="article.render.titlepage">
  <xsl:call-template name="make.titlep.navbar"/>
  <xsl:apply-templates select="/descendant::articleinfo/*" mode="titlepage.mode"/>
  <xsl:call-template name="make.titlep.navbar"/>
</xsl:template>

<xsl:template match="/article">
  <xsl:choose>
    <xsl:when test="string-length($gdb_rootid) &lt; 1">
      <p align="center"><xsl:value-of select="/article/articleinfo/title"/></p>
      <xsl:call-template name="make.toc.navbar"/>
      <xsl:call-template name="component.toc"/>
      <xsl:call-template name="make.toc.navbar"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:choose>
        <xsl:when test="$gdb_rootid='title-page'">
          <xsl:call-template name="article.render.titlepage"/>
        </xsl:when>
        <xsl:otherwise>
         <xsl:call-template name="article.render.chunk">
           <xsl:with-param name="node" select="descendant::*[attribute::id=$gdb_rootid]" />
         </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
