<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'>

<xsl:import href="http://docbook.sourceforge.net/release/xsl/1.48/html/docbook.xsl"/>
<xsl:include href="yelp-custom.xsl"/>

<xsl:param name="gdb_docname" />

<xsl:param name="gdb_pathname" />

<xsl:param name="gdb_rootid" select="''" />

<xsl:param name="gdb_multichunk" select="0" />

<xsl:param name="gdb_stylesheet_path" select="'No Stylesheet'" />

<xsl:output encoding="ISO-8859-1" />

<!-- Specifies the default path for admonition graphics -->
<xsl:param name="admon.graphics.path"><xsl:text>file://</xsl:text><xsl:value-of select="$gdb_stylesheet_path"/><xsl:text>/images/</xsl:text></xsl:param>

<xsl:param name="table.borders.with.css" select="1"/>

<!-- direct parametrisation -->
<xsl:param name="admon.style"><xsl:text>margin-left: 0</xsl:text></xsl:param>

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
       B {font-weight: bold}
       LI {margin-bottom: 0.66em;}
       UL LI {margin-left: 1em}
       OL LI {margin-left: 1.5em}
       LI P {margin-bottom: 0}
       LI P {margin-top: 0}
       P.header-title {text-align: center; margin-top: 0; margin-bottom: 0}
       P.copyright {margin-bottom: 0; margin-top: 0}
       H2 + P.copyright {margin-top: 1em}
       DIV.legalnotice P {font-size: 0.8em}
       DIV.revhistory TR {vertical-align: top}
       TT {font-family: monospace}
       P.about {margin-bottom: 0}
       DD P {margin-top: 0}
       DD P {margin-bottom: 0}
       OL {margin-top: 0; margin-bottom: 0}
       LI OL {margin-left: 2em; margin-top: 0.66em}
       UL {margin-top: 0; margin-bottom: 0}
       LI UL {margin-left: 2em; margin-top: 0.66em}
       OL LI UL {list-style-type: disc}
       UL LI UL {list-style-type: circle}
       OL LI OL {list-style-type: lower-alpha}
       DD {margin-left: 2em}
       DL {margin-top: 0}
       DL {margin-bottom: 0}
       DIV.variablelist DT {margin-top: 1em }
       DIV.variablelist DD P {margin-top: 0.5em}
       DIV.variablelist DD UL {margin-top: 0.5em}
       DIV.variablelist DD LI P {margin-top: 0}
       HR.bottom {margin-top: 2ex; margin-bottom: 0}
       TD {vertical-align: top}
       TH {vertical-align: top}
       DIV.table P {margin-top: 0}
       TABLE P {margin-bottom: 0; margin-top: 0}
       TABLE UL {margin-top: 0.66em;}
       DIV.toc {margin-bottom: 3ex}
       DIV TD {padding-right: 1em; padding-left: 1em; padding-top: 0.5ex; padding-bottom: 0.5ex}
       DIV TH {padding-right: 1em; padding-left: 1em; padding-top: 0.5ex; padding-bottom: 0.5ex}
       DIV.note TD {padding-left: 0; padding-top: 0}
       DIV.note TH {padding-left: 0; padding-top: 0}
       DIV.informaltable TABLE {border-bottom: solid 1px black}
       DIV.informaltable TR:FIRST-CHILD {border-top: solid 1px black}
       DIV.table TR:FIRST-CHILD {border-top: solid 1px black}
       DIV.table TABLE {border-bottom: solid 1px black}
       DIV.revhistory TABLE {border-spacing: 0}
       LI DIV.informaltable {margin-top: 1em; margin-bottom: 1em}
       LI DIV.figure P {margin-top: 1em; margin-bottom: 1em}
       H1 {font-size: 1.4em}
       H2 {font-size: 1.3em; margin-bottom: 0}
       H3 {font-size: 1.2em; margin-bottom: 0}
       H4 {font-size: 1.1em}
     </xsl:text>
   </style>
</xsl:template>

<!-- take over the control of the body tag attributes -->
<xsl:template name="body.attributes">
  <xsl:attribute name="link">#0000FF</xsl:attribute>
  <xsl:attribute name="vlink">#840084</xsl:attribute>
  <xsl:attribute name="alink">#0000FF</xsl:attribute>

</xsl:template>

<!-- change some formating choices -->

<xsl:template match="guibutton">
  <xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="command">
  <xsl:call-template name="inline.monoseq"/>
</xsl:template>


<xsl:template match="inlinemediaobject">
  <span class="{name(.)}">
    <xsl:if test="@id">
	<a name="{@id}"/>
    </xsl:if>
    <xsl:call-template name="select.mediaobject"/>
  </span>
<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text>
</xsl:template>



<!-- the chunking machinery -->

<xsl:template name="next.link.cell">
<xsl:param name="object" select="."/>
   <td align="right" width="33%"><a accesskey="n">
   <xsl:attribute name="href">
      <xsl:call-template name="href.target">
         <xsl:with-param name="object" select="$object"/>
      </xsl:call-template>
   </xsl:attribute>
   <xsl:choose>
      <xsl:when test="$object/title">
         <xsl:value-of select="$object/title/text()"/>
      </xsl:when>
      <xsl:otherwise>
         <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="'Next'"/>
         </xsl:call-template>
      </xsl:otherwise>
   </xsl:choose>
   <xsl:text> &gt;&gt;&gt;</xsl:text>
   </a></td>
</xsl:template>

<xsl:template name="prev.link.cell">
<xsl:param name="object" select="."/>
   <td align="left" width="33%"><a accesskey="p">
   <xsl:attribute name="href">
      <xsl:call-template name="href.target">
         <xsl:with-param name="object" select="$object"/>
      </xsl:call-template>
   </xsl:attribute>
   <xsl:text>&lt;&lt;&lt; </xsl:text>
   <xsl:choose>
      <xsl:when test="$object/title">
         <xsl:value-of select="$object/title/text()"/>
      </xsl:when>
      <xsl:otherwise>
         <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="'Previous'"/>
         </xsl:call-template>
      </xsl:otherwise>
   </xsl:choose>
   </a></td>
</xsl:template>

<xsl:template name="article.toc.ref">
<xsl:text>ghelp:</xsl:text>
<xsl:value-of select="$gdb_docname"/>
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
    <xsl:choose>

      <xsl:when test="count($node/preceding-sibling::*) > 1">
        <xsl:call-template name="prev.link.cell">
          <xsl:with-param name="object" select="$node/preceding-sibling::*[1]"/>
        </xsl:call-template>
      </xsl:when>

      <xsl:when test="local-name($node)='sect2' and count($node/../preceding-sibling::sect1[position()=last()]/sect2) > 0">
        <xsl:call-template name="indirect.prev.cell">
          <xsl:with-param name="object" select="$node/../preceding-sibling::sect1[position()=last()]"/>
        </xsl:call-template>
      </xsl:when>

      <!-- we need to treat the first sect1 specially -->
      <xsl:when test="$node=/article/sect1[1] or $node=/part/chapter/sect1[1]">
        <td><a accesskey="p">
          <xsl:attribute name="href">
            <xsl:call-template name="article.toc.ref"/>
          </xsl:attribute>
          <xsl:text>&lt;&lt;&lt; </xsl:text>
          <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="'Previous'"/>
          </xsl:call-template>
        </a></td>
      </xsl:when>

      <!-- And the first sect2 of the first sect1 needs the same -->
      <xsl:when test="$node=/article/sect1[1]/sect2[1] or $node=/part/chapter/sect1[1]/sect2[1]">
        <td><a accesskey="p">
          <xsl:attribute name="href">
            <xsl:call-template name="article.toc.ref"/>
          </xsl:attribute>
          <xsl:text>&lt;&lt;&lt; </xsl:text>
          <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="'Previous'"/>
          </xsl:call-template>
	</a></td>
      </xsl:when>

    </xsl:choose>
</xsl:template>

<xsl:template name="article.chunk.up">
<xsl:param name="node" select="."/>
<xsl:param name="doit" select="1"/>
<xsl:choose>
  <xsl:when test="$doit=1">
   <td align="center" width="33%">
         <a accesskey="u">
           <xsl:attribute name="href">
             <xsl:call-template name="article.toc.ref"/>
           </xsl:attribute>
           <xsl:call-template name="gentext">
             <xsl:with-param name="key" select="'Contents'"/>
           </xsl:call-template>
         </a>
   </td>
  </xsl:when>
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
      <xsl:when test="local-name($node)='sect2' and count($node/../following-sibling::sect1[1]/sect2) > 0">
        <xsl:call-template name="indirect.next.cell">
          <xsl:with-param name="object" select="$node/../following-sibling::sect1[1]"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
    	  <td align="right" width="33%">
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

<xsl:template name="titlepage.ref">
  <xsl:text>ghelp:</xsl:text>
  <xsl:value-of select="$gdb_docname"/>
  <xsl:text>?title-page</xsl:text>
</xsl:template>

<xsl:template name="article.render.chunk">
<xsl:param name="node" select="."/>
<xsl:param name="title" select="/article/articleinfo/title"/>
  <p class="header-title"><xsl:value-of select="$title"/></p>
  <table width="100%" align="justify">
    <xsl:call-template name="article.chunk.navigate">
      <xsl:with-param name="node" select="$node"/>
      <xsl:with-param name="link-toc" select="0"/>
    </xsl:call-template>
  </table>
  <xsl:element name="hr" />
  <xsl:apply-templates select="$node"/>
  <xsl:element name="hr">
    <xsl:attribute name="class"><xsl:text>bottom</xsl:text></xsl:attribute>
  </xsl:element>
  <table width="100%" align="justify">
    <xsl:call-template name="article.chunk.navigate">
      <xsl:with-param name="node" select="$node"/>
      <xsl:with-param name="link-toc" select="1"/>
    </xsl:call-template>
  </table>
</xsl:template>

<xsl:template name="make.toc.navbar">
  <table width="100%">
    <tr>
      <td width="33%"><a accesskey="p">
        <xsl:attribute name="href">
          <xsl:call-template name="titlepage.ref"/>
        </xsl:attribute>
        <xsl:text>&lt;&lt;&lt; </xsl:text>
        <xsl:call-template name="gentext">
          <xsl:with-param name="key" select="'Previous'"/>
        </xsl:call-template>
      </a></td>
      <td></td>
      <xsl:choose>
      <xsl:when test="local-name(.) = 'part'">
        <xsl:call-template name="next.link.cell">
          <xsl:with-param name="object" select="chapter[1]/sect1[1]"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="next.link.cell">
          <xsl:with-param name="object" select="sect1[1]"/>
        </xsl:call-template>
      </xsl:otherwise>
      </xsl:choose>
    </tr>
  </table>
</xsl:template>

<xsl:template name="make.titlep.navbar">
  <table width="100%">
    <tr>
      <td align="right"><a accesskey="n">
        <xsl:attribute name="href">
          <xsl:call-template name="article.toc.ref"/>
        </xsl:attribute>
          <xsl:call-template name="gentext">
            <xsl:with-param name="key" select="'Next'"/>
          </xsl:call-template>
          <xsl:text> &gt;&gt;&gt;</xsl:text>
      </a></td>
    </tr>
  </table>
</xsl:template>


<xsl:template name="article.render.titlepage">
<xsl:param name="container" select="."/>
  <xsl:call-template name="make.titlep.navbar"/>
  <xsl:apply-templates select="$container/*" mode="titlepage.mode"/>
  <xsl:call-template name="make.titlep.navbar"/>
</xsl:template>

<xsl:template name="yelp.render.toc">
<xsl:param name="title" select="''" />
  <xsl:call-template name="make.toc.navbar"/>
  <xsl:element name="hr"/>
  <H1 class="title"><xsl:value-of select="$title"/></H1>
  <p class="about"><a>
    <xsl:attribute name="href">
      <xsl:call-template name="titlepage.ref"/>
    </xsl:attribute>
    <xsl:call-template name="gentext.template">
      <xsl:with-param name="context" select="'title'"/>
      <xsl:with-param name="name" select="'bookinfo'"/>
    </xsl:call-template>
  </a></p>
  <xsl:choose>
  <xsl:when test="local-name(.)='part'">
    <xsl:for-each select="chapter">
      <xsl:call-template name="component.toc"/>
    </xsl:for-each>
  </xsl:when>
  <xsl:when test="local-name(.)='book'">
    <xsl:call-template name="division.toc"/>
  </xsl:when>
  <xsl:otherwise>
    <xsl:call-template name="component.toc"/>
  </xsl:otherwise>
  </xsl:choose>
  <xsl:element name="hr"/>
  <xsl:call-template name="make.toc.navbar"/>
</xsl:template>

<xsl:template name="yelp.book.multichunk">
<xsl:param name="root" select="."/>

<xsl:for-each select="$root/part">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="division.toc"/>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:for-each select="$root/part/chapter">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="component.toc"/>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:for-each select="$root/part/appendix">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="component.toc"/>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:for-each select="$root/chapter">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="component.toc"/>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:for-each select="$root/appendix">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="component.toc"/>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>
</xsl:template>

<xsl:template name="yelp.multichunk">
<xsl:param name="type"/>
<xsl:param name="container"/>
<xsl:param name="root" select="."/>

<xsl:comment> End of header </xsl:comment>
<xsl:comment> Start of chunk: [title-page] </xsl:comment>

<xsl:choose>
<xsl:when test="$type = 'book'">
  <xsl:call-template name="book.titlepage"/>
</xsl:when>
<xsl:otherwise>
<xsl:call-template name="article.render.titlepage">
  <xsl:with-param name="container" select="$container"/>
</xsl:call-template>
</xsl:otherwise>
</xsl:choose>

<xsl:comment> End of chunk </xsl:comment>
<xsl:comment> Start of chunk: [toc] </xsl:comment>

<xsl:call-template name="yelp.render.toc">
  <xsl:with-param name="title" select="$container/title"/>
</xsl:call-template>

<xsl:comment> End of chunk </xsl:comment>

<xsl:if test="$type = 'book'">
  <xsl:call-template name="yelp.book.multichunk"/>
</xsl:if>

<xsl:for-each select="//sect1">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="article.render.chunk">
   <xsl:with-param name="title" select="$container/title"/> 
  </xsl:call-template>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:for-each select="//sect2">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="article.render.chunk"> 
    <xsl:with-param name="title" select="$container/title"/>
  </xsl:call-template> 
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:for-each select="//sect3">
  <xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
  <xsl:call-template name="article.render.chunk">
    <xsl:with-param name="title" select="$container/title"/>
  </xsl:call-template>
  <xsl:comment> End of chunk </xsl:comment>
</xsl:for-each>

<xsl:comment> Start of footer </xsl:comment>
</xsl:template>

<xsl:template match="/article">
<xsl:call-template name="yelp.generic.root">
  <xsl:with-param name="container" select="/article/articleinfo"/>
  <xsl:with-param name="type" select="'article'"/>
</xsl:call-template>
</xsl:template>

<xsl:template match="/part">
<xsl:call-template name="yelp.generic.root">
  <xsl:with-param name="container" select="/part/partinfo"/>
  <xsl:with-param name="type" select="'part'"/>
  <xsl:with-param name="root" select="chapter"/>
</xsl:call-template>
</xsl:template>

<xsl:template match="/book">
<xsl:call-template name="yelp.generic.root">
  <xsl:with-param name="container" select="."/>
  <xsl:with-param name="type" select="'book'"/>
</xsl:call-template>
</xsl:template>

<xsl:template name="yelp.generic.root">
<xsl:param name="type"/>
<xsl:param name="container"/>
<xsl:param name="root" select="."/>
  <xsl:choose>
    <xsl:when test="$gdb_multichunk=1">
      <xsl:call-template name="yelp.multichunk">
        <xsl:with-param name="type" select="$type"/>
        <xsl:with-param name="container" select="$container"/>
        <xsl:with-param name="root" select="$root"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:when test="$gdb_rootid = ''">
      <xsl:call-template name="yelp.render.toc">
        <xsl:with-param name="title">
          <xsl:value-of select="$container/title"/>
        </xsl:with-param>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="yelp.render.chunk">
        <xsl:with-param name="container" select="$container"/>
      </xsl:call-template>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="yelp.render.chunk">
<xsl:param name="container" select="."/>
      <xsl:choose>
        <xsl:when test="$gdb_rootid='title-page'">
          <xsl:call-template name="article.render.titlepage">
            <xsl:with-param name="container" select="$container"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
         <xsl:call-template name="article.render.chunk">
           <xsl:with-param name="node" select="descendant::*[attribute::id=$gdb_rootid]" />
         </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
</xsl:template>

</xsl:stylesheet>
