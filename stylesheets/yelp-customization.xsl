<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:func="http://exslt.org/functions"
                xmlns:yelp="http://www.gnome.org/"
                extension-element-prefixes="func yelp"
                version='1.0'>

<xsl:import href="http://docbook.sourceforge.net/release/xsl/1.48/html/docbook.xsl"/>
<xsl:include href="yelp-custom.xsl"/>
<xsl:include href="yelp-functions.xsl"/>

<xsl:param name="gdb_docname" />
<xsl:param name="gdb_pathname" />
<xsl:param name="gdb_rootid" select="''" />
<xsl:param name="gdb_multichunk" select="0" />
<xsl:param name="gdb_stylesheet_path" select="'No Stylesheet'" />
<xsl:param name="gdb_max_chunk_depth" select="2" />

<xsl:output encoding="ISO-8859-1" />

<!-- Specifies the default path for admonition graphics -->
<xsl:param name="admon.graphics.path">
	<xsl:text>file://</xsl:text>
	<xsl:value-of select="$gdb_stylesheet_path"/>
	<xsl:text>/images/</xsl:text>
</xsl:param>

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
	<xsl:value-of select="$object/ancestor-or-self::*[yelp:is-division(.)][
		yelp:get-depth(.) &lt;= $gdb_max_chunk_depth][1]/@id"/>
<!-- Uncomment if we support fragment identification
   <xsl:if test="$object!=$ancestor">
      <xsl:text>#</xsl:text>
      <xsl:value-of select="$object/@id"/>
   </xsl:if>
-->
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
       DIV.table > TABLE > * > TR:FIRST-CHILD {border-top: solid 1px black}
       DIV.table > TABLE {border-bottom: solid 1px black}
       DIV.revhistory TABLE {border-spacing: 0}
       LI DIV.informaltable {margin-top: 1em; margin-bottom: 1em}
       LI DIV.figure P {margin-top: 1em; margin-bottom: 1em}
       DIV.figure {margin-bottom: 4em}
       H1 {font-size: 1.4em}
       H2 {font-size: 1.3em; margin-bottom: 0}
       H3 {font-size: 1.2em; margin-bottom: 0}
       H4 {font-size: 1.1em; margin-bottom: 0}
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

<!-- ======================================================================= -->

<xsl:template name="yelp.next.link.cell">
	<xsl:param name="object" select="."/>
	<td align="right" width="33%"><a accesskey="n">
		<xsl:attribute name="href">
			<xsl:call-template name="href.target">
				<xsl:with-param name="object" select="$object"/>
			</xsl:call-template>
		</xsl:attribute>
		<xsl:choose>
			<xsl:when test="yelp:get-title-text($object)">
				<xsl:value-of select="yelp:get-title-text($object)"/>
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

<xsl:template name="yelp.prev.link.cell">
	<xsl:param name="object" select="."/>
	<td align="left" width="33%"><a accesskey="p">
		<xsl:attribute name="href">
			<xsl:call-template name="href.target">
				<xsl:with-param name="object" select="$object"/>
			</xsl:call-template>
		</xsl:attribute>
		<xsl:text>&lt;&lt;&lt; </xsl:text>
		<xsl:choose>
			<xsl:when test="yelp:get-title-text($object)">
				<xsl:value-of select="yelp:get-title-text($object)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key" select="'Previous'"/>
				</xsl:call-template>
			</xsl:otherwise>
		</xsl:choose>
	</a></td>
</xsl:template>

<xsl:template name="yelp.toc.ref">
	<xsl:text>ghelp:</xsl:text>
	<xsl:value-of select="$gdb_docname"/>
</xsl:template>

<xsl:template name="yelp.titlepage.ref">
	<xsl:text>ghelp:</xsl:text>
	<xsl:value-of select="$gdb_docname"/>
	<xsl:text>?title-page</xsl:text>
</xsl:template>

<xsl:template name="yelp.navbar.prev">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="($node/preceding-sibling::*/descendant-or-self::*)
				[yelp:is-division(.)]
				[yelp:get-depth(.) &lt;= $gdb_max_chunk_depth]">
			<xsl:call-template name="yelp.prev.link.cell">
				<xsl:with-param name="object"
					select="($node/preceding-sibling::*/descendant-or-self::*)
						[yelp:is-division(.)]
						[yelp:get-depth(.) &lt;= $gdb_max_chunk_depth][last()]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="yelp:get-depth($node) &gt; 1">
			<xsl:call-template name="yelp.prev.link.cell">
				<xsl:with-param name="object"
					select="$node/ancestor::*[yelp:is-division(.)][1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<td><a accesskey="p">
				<xsl:attribute name="href">
					<xsl:call-template name="yelp.toc.ref"/>
				</xsl:attribute>
				<xsl:text>&lt;&lt;&lt; </xsl:text>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key" select="'Contents'"/>
				</xsl:call-template>
			</a></td>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="yelp.navbar.up">
	<xsl:param name="node" select="."/>
	<td align="center" width="33%"><a accesskey="u">
		<xsl:attribute name="href">
			<xsl:call-template name="yelp.toc.ref"/>
		</xsl:attribute>
		<xsl:call-template name="gentext">
			<xsl:with-param name="key" select="'Contents'"/>
		</xsl:call-template>
	</a></td>
</xsl:template>

<xsl:template name="yelp.navbar.next">
	<xsl:param name="node" select="."/>

	<xsl:choose>
		<xsl:when test="(yelp:get-depth($node) &lt; $gdb_max_chunk_depth) and 
				(count(yelp:get-divisions($node)) &gt; 1)">
			<xsl:call-template name="yelp.next.link.cell">
				<xsl:with-param name="object"
					select="yelp:get-divisions($node)[1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$node/following-sibling::*[yelp:is-division(.)]">
			<xsl:call-template name="yelp.next.link.cell">
				<xsl:with-param name="object"
					select="$node/following-sibling::*[yelp:is-division(.)][1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$node/following::*[yelp:is-division(.)]">
			<xsl:call-template name="yelp.next.link.cell">
				<xsl:with-param name="object"
					select="$node/following::*[yelp:is-division(.)][1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<td align="right" width="33%"></td>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="yelp.navbar">
	<xsl:param name="node" select="."/>
	<xsl:param name="link-toc" select="false()"/>
	<table width="100%"><tr>
		<xsl:call-template name="yelp.navbar.prev">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
		<xsl:if test="$link-toc">
			<xsl:call-template name="yelp.navbar.up">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</xsl:if>
		<xsl:call-template name="yelp.navbar.next">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</tr></table>
</xsl:template>

<xsl:template name="yelp.toc.navbar">
	<table width="100%">
		<tr>
			<td width="33%"><a accesskey="p">
				<xsl:attribute name="href">
					<xsl:call-template name="yelp.titlepage.ref"/>
				</xsl:attribute>
				<xsl:text>&lt;&lt;&lt; </xsl:text>
				<xsl:call-template name="gentext.template">
					<xsl:with-param name="context" select="'title'"/>
					<xsl:with-param name="name" select="'bookinfo'"/>
				</xsl:call-template>
			</a></td>
			<xsl:choose>
				<!-- Hackery for the User's Guide, which I don't like -->
				<xsl:when test="(local-name(.) = 'part') and (count(chapter) = 1)">
					<xsl:call-template name="yelp.next.link.cell">
						<xsl:with-param name="object"
							select="yelp:get-divisions(yelp:get-divisions(.)[1])[1]"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="yelp.next.link.cell">
						<xsl:with-param name="object" select="yelp:get-divisions(.)[1]"/>
					</xsl:call-template>
				</xsl:otherwise>
			</xsl:choose>
		</tr>
	</table>
</xsl:template>

<xsl:template name="yelp.titlep.navbar">
	<table width="100%">
		<tr>
			<td align="right"><a accesskey="n">
				<xsl:attribute name="href">
					<xsl:call-template name="yelp.toc.ref"/>
				</xsl:attribute>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key" select="'Contents'"/>
				</xsl:call-template>
				<xsl:text> &gt;&gt;&gt;</xsl:text>
			</a></td>
		</tr>
	</table>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="yelp.title">
	<xsl:choose>
		<xsl:when test="yelp:get-title(.)">
			<xsl:apply-templates select="yelp:get-title(.)" mode="titlepage.mode"/>
		</xsl:when>
		<xsl:when test="yelp:get-title-text(.)">
			<h2 class="title">
				<a><xsl:attribute name="href">
					<xsl:call-template name="yelp.titlepage.ref"/>
				</xsl:attribute></a>
				<xsl:value-of select="yelp:get-title-text(.)"/>
			</h2>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template name="yelp.titlepage.chunk">
	<xsl:choose>
		<xsl:when test="self::article/articleinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="article.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::appendix/appendixinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="appendix.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::chapter/chapterinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="chapter.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::preface/prefaceinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="preface.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::sect1/sect1info">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="sect1.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::sect2/sect2info">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="sect2.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::sect3/sect3info">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="sect3.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::sect4/sect4info">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="sect4.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::sect5/sect5info">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="sect5.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::section/sectioninfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="section.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::set/setinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="set.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::book/bookinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="book.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::part/partinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="part.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::reference/referenceinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="reference.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::glossary/glossaryinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="glossary.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::index/indexinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="index.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::bibliography/bibliographyinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="bibliography.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::refentry/refentryinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="refentry.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="self::simplesect/simplesectinfo">
			<xsl:comment> Start of chunk: [title-page] </xsl:comment>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:element name="hr"/>
			<xsl:call-template name="simplesect.titlepage"/>
			<xsl:element name="hr">
				<xsl:attribute name="class">
					<xsl:text>bottom</xsl:text>
				</xsl:attribute>
			</xsl:element>
			<xsl:call-template name="yelp.titlep.navbar"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:otherwise>
			<xsl:message>Could not construct a titlepage for <xsl:value-of
					select="local-name(.)"/>.</xsl:message>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="yelp.component.chunk">
	<xsl:param name="depth" select="0"/>

	<xsl:if test="($depth != 0) or ($gdb_max_chunk_depth = 0)">
		<xsl:comment> Start of chunk: [<xsl:value-of select="@id"/>] </xsl:comment>
		<xsl:call-template name="yelp.navbar">
			<xsl:with-param name="node" select="."/>
			<xsl:with-param name="link-toc" select="false()"/>
		</xsl:call-template>
		<xsl:element name="hr"/>
		<div class="{local-name(.)}">
			<xsl:call-template name="yelp.title"/>
			<xsl:apply-templates select="yelp:get-content(.)"/>
			<xsl:choose>
				<xsl:when test="($depth &lt; $gdb_max_chunk_depth)
						and (count(yelp:get-divisions(.)) &gt; 1)">
					<div class="toc">
						<p><b>
							<xsl:call-template name="gentext">
								<xsl:with-param name="key">TableofContents</xsl:with-param>
							</xsl:call-template>
						</b></p>
						<xsl:apply-templates select="yelp:get-divisions(.)" mode="toc"/>
					</div>
				</xsl:when>
				<xsl:when test="count(yelp:get-divisions(.)) &gt; 1">
					<xsl:apply-templates select="yelp:get-divisions(.)"/>
				</xsl:when>
			</xsl:choose>
		</div>
		<xsl:element name="hr">
			<xsl:attribute name="class">
				<xsl:text>bottom</xsl:text>
			</xsl:attribute>
		</xsl:element>
		<xsl:call-template name="yelp.navbar">
			<xsl:with-param name="node" select="."/>
			<xsl:with-param name="link-toc" select="true()"/>
		</xsl:call-template>
		<xsl:comment> End of chunk </xsl:comment>
	</xsl:if>

	<xsl:if test="($depth &lt; $gdb_max_chunk_depth)
			and (count(yelp:get-divisions(.)) &gt; 1)">
		<xsl:for-each select="yelp:get-divisions(.)">
			<xsl:call-template name="yelp.component.chunk">
				<xsl:with-param name="depth" select="$depth + 1"/>
			</xsl:call-template>
		</xsl:for-each>
	</xsl:if>
</xsl:template>

<xsl:template match="/*">
	<xsl:call-template name="yelp.root"/>
</xsl:template>

<xsl:template name="yelp.root">
	<xsl:comment> End of header </xsl:comment>

	<xsl:call-template name="yelp.titlepage.chunk"/>

	<xsl:comment> Start of chunk: [toc] </xsl:comment>
	<xsl:call-template name="yelp.toc.navbar"/>
	<xsl:element name="hr"/>
	<xsl:call-template name="yelp.title"/>
	<p class="about"><a>
		<xsl:attribute name="href">
			<xsl:call-template name="yelp.titlepage.ref"/>
		</xsl:attribute>
		<xsl:call-template name="gentext.template">
			<xsl:with-param name="context" select="'title'"/>
			<xsl:with-param name="name" select="'bookinfo'"/>
		</xsl:call-template>
	</a></p>
	<xsl:apply-templates select="yelp:get-content(.)"/>
	<div class="toc">
		<p><b>
			<xsl:call-template name="gentext">
				<xsl:with-param name="key">TableofContents</xsl:with-param>
			</xsl:call-template>
		</b></p>
		<xsl:choose>
			<!-- Hackery for the User's Guide, which I don't like -->
			<xsl:when test="(local-name(.) = 'part') and (count(chapter) = 1)">
				<xsl:for-each select="yelp:get-divisions(.)">
					<xsl:apply-templates select="yelp:get-divisions(.)" mode="toc"/>
				</xsl:for-each>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="yelp:get-divisions(.)" mode="toc"/>
			</xsl:otherwise>
		</xsl:choose>
	</div>
	<xsl:element name="hr">
		<xsl:attribute name="class">
			<xsl:text>bottom</xsl:text>
		</xsl:attribute>
	</xsl:element>
	<xsl:call-template name="yelp.toc.navbar"/>
	<xsl:comment> End of chunk </xsl:comment>

	<xsl:choose>
		<!-- Hackery for the User's Guide, which I don't like -->
		<xsl:when test="(local-name(.) = 'part') and (count(chapter) = 1)">
			<xsl:for-each select="yelp:get-divisions(yelp:get-divisions(.))">
				<xsl:call-template name="yelp.component.chunk">
					<xsl:with-param name="depth" select="1"/>
				</xsl:call-template>
			</xsl:for-each>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="yelp.component.chunk">
				<xsl:with-param name="depth" select="0"/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>

	<xsl:comment> Start of footer </xsl:comment>
</xsl:template>

</xsl:stylesheet>
