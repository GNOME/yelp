<?xml version='1.0'?><!-- -*- tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:func="http://exslt.org/functions"
                xmlns:exsl="http://exslt.org/common"
                xmlns:yelp="http://www.gnome.org/"
                extension-element-prefixes="func exsl"
                version='1.0'>

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl"/>
<xsl:include href="yelp-custom.xsl"/>
<xsl:include href="yelp-functions.xsl"/>

<xsl:param name="yelp_docname" />
<xsl:param name="yelp_pathname" />
<xsl:param name="yelp_rootid" select="''" />
<xsl:param name="yelp_multichunk" select="0" />
<xsl:param name="yelp_stylesheet_path" select="'No Stylesheet'" />
<xsl:param name="yelp_max_chunk_depth" select="2" />
<xsl:param name="yelp_generate_navbar" select="true()"/>
<!-- either 'yelp' or 'exslt' -->
<xsl:param name="yelp_chunk_method" select="'yelp'"/>

<xsl:output encoding="ISO-8859-1" />

<!-- Specifies the default path for admonition graphics -->
<xsl:param name="admon.graphics.path">
	<xsl:choose>
		<xsl:when test="$yelp_chunk_method = 'yelp'">
			<xsl:text>file://</xsl:text>
			<xsl:value-of select="$yelp_stylesheet_path"/>
			<xsl:text>/images/</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>./stylesheet/</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:param>

<xsl:param name="table.borders.with.css" select="1"/>

<!-- direct parametrisation -->
<xsl:param name="admon.style"><xsl:text>margin-left: 0</xsl:text></xsl:param>

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
			<xsl:choose>
				<xsl:when test="$yelp_chunk_method = 'yelp'">
					<xsl:text>file://</xsl:text>
					<xsl:value-of select="$yelp_pathname"/>
					<xsl:text>/</xsl:text>
					<xsl:value-of select="$filename"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="$filename"/>
				</xsl:otherwise>
			</xsl:choose>
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

<xsl:template name="href.target">
	<xsl:param name="object" select="."/>
	<xsl:variable name="chunk" select="$object/ancestor-or-self::*
		[yelp:is-division(.)]
		[yelp:get-depth(.) &lt;= $yelp_max_chunk_depth]
		[1]"/>

	<xsl:choose>
		<xsl:when test="$yelp_chunk_method = 'yelp'">
			<xsl:text>ghelp:</xsl:text>
			<xsl:value-of select="$yelp_docname"/>
			<xsl:text>?</xsl:text>
			<xsl:value-of select="$chunk/@id"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="$chunk/@id"/>
			<xsl:text>.html</xsl:text>
			<xsl:if test="$chunk != $object">
				<xsl:text>#</xsl:text>
				<xsl:value-of select="$object/@id"/>
			</xsl:if>
		</xsl:otherwise>
	</xsl:choose>
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
<!--
  <xsl:attribute name="link">#0000FF</xsl:attribute>
  <xsl:attribute name="vlink">#840084</xsl:attribute>
  <xsl:attribute name="alink">#0000FF</xsl:attribute>
-->
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

<xsl:template name="yelp.toc.ref">
	<xsl:choose>
		<xsl:when test="$yelp_chunk_method = 'yelp'">
			<xsl:text>ghelp:</xsl:text>
			<xsl:value-of select="$yelp_docname"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>toc.html</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="yelp.titlepage.ref">
	<xsl:choose>
		<xsl:when test="$yelp_chunk_method = 'yelp'">
			<xsl:text>ghelp:</xsl:text>
			<xsl:value-of select="$yelp_docname"/>
			<xsl:text>?title-page</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>title-page.html</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="yelp.navbar.cell">
	<xsl:param name="target" select="."/>
	<xsl:param name="show.title" select="false()"/>
	<xsl:param name="position" select="'left'"/>
	<xsl:param name="use.title" select="false()"/>

	<td width="40%">
		<xsl:attribute name="align">
			<xsl:value-of select="$position"/>
		</xsl:attribute>
		<a>
			<xsl:attribute name="accesskey">
				<xsl:choose>
					<xsl:when test="$position = 'left'"><xsl:text>p</xsl:text></xsl:when>
					<xsl:when test="$position = 'right'"><xsl:text>n</xsl:text></xsl:when>
				</xsl:choose>
			</xsl:attribute>
			<xsl:attribute name="href">
				<xsl:choose>
					<xsl:when test="$target = 'title-page'">
						<xsl:call-template name="yelp.titlepage.ref"/>
					</xsl:when>
					<xsl:when test="$target = 'toc'">
						<xsl:call-template name="yelp.toc.ref"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:call-template name="href.target">
							<xsl:with-param name="object" select="$target"/>
						</xsl:call-template>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:attribute>
			<xsl:choose>
				<xsl:when test="$position = 'left'">
					<xsl:call-template name="gentext">
						<xsl:with-param name="key" select="'Previous'"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:when test="$position = 'right'">
					<xsl:call-template name="gentext">
						<xsl:with-param name="key" select="'Next'"/>
					</xsl:call-template>
				</xsl:when>
			</xsl:choose>
		</a>
		<xsl:if test="$use.title">
			<br/>
			<xsl:choose>
				<xsl:when test="$target = 'title-page'">
					<xsl:call-template name="gentext.template">
						<xsl:with-param name="context" select="'title'"/>
						<xsl:with-param name="name" select="'bookinfo'"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:when test="$target = 'toc'">
					<xsl:call-template name="gentext">
						<xsl:with-param name="key" select="'Contents'"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="yelp:get-title-text($target)"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:if>
	</td>
</xsl:template>

<xsl:template name="yelp.navbar">
	<xsl:param name="node" select="."/>
	<xsl:param name="prev" select="yelp:get-prev($node)"/>
	<xsl:param name="next" select="yelp:get-next($node)"/>
	<xsl:param name="up" select="false()"/>
	<xsl:param name="use.title" select="false()"/>

	<xsl:if test="$yelp_generate_navbar">
	<table width="100%"><tr>
		<xsl:if test="$prev">
			<xsl:call-template name="yelp.navbar.cell">
				<xsl:with-param name="target" select="$prev"/>
				<xsl:with-param name="use.title" select="$use.title"/>
				<xsl:with-param name="position" select="'left'"/>
			</xsl:call-template>
		</xsl:if>
		<xsl:if test="$up">
			<td align="center" width="20%"><a accesskey="u">
				<xsl:attribute name="href">
					<xsl:call-template name="yelp.toc.ref"/>
				</xsl:attribute>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key" select="'Contents'"/>
				</xsl:call-template>
			</a></td>
		</xsl:if>
		<xsl:if test="$next">
			<xsl:call-template name="yelp.navbar.cell">
				<xsl:with-param name="target" select="$next"/>
				<xsl:with-param name="use.title" select="$use.title"/>
				<xsl:with-param name="position" select="'right'"/>
			</xsl:call-template>
		</xsl:if>
	</tr></table>
	</xsl:if>
</xsl:template>

<xsl:template name="yelp.navbar.top">
	<xsl:param name="node" select="."/>
	<xsl:param name="prev" select="yelp:get-prev($node)"/>
	<xsl:param name="next" select="yelp:get-next($node)"/>
	<xsl:param name="up" select="false()"/>

	<xsl:if test="$yelp_generate_navbar">
	<xsl:call-template name="yelp.navbar">
		<xsl:with-param name="node" select="$node"/>
		<xsl:with-param name="prev" select="$prev"/>
		<xsl:with-param name="next" select="$next"/>
		<xsl:with-param name="up" select="$up"/>
		<xsl:with-param name="use.title" select="false()"/>
	</xsl:call-template>
	<hr/>
	</xsl:if>
</xsl:template>

<xsl:template name="yelp.navbar.bottom">
	<xsl:param name="node" select="."/>
	<xsl:param name="prev" select="yelp:get-prev($node)"/>
	<xsl:param name="next" select="yelp:get-next($node)"/>
	<xsl:param name="up" select="true()"/>

	<xsl:if test="$yelp_generate_navbar">
	<hr class="bottom"/>
	<xsl:call-template name="yelp.navbar">
		<xsl:with-param name="node" select="$node"/>
		<xsl:with-param name="prev" select="$prev"/>
		<xsl:with-param name="next" select="$next"/>
		<xsl:with-param name="up" select="$up"/>
		<xsl:with-param name="use.title" select="true()"/>
	</xsl:call-template>
	</xsl:if>
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

<xsl:template name="yelp.chunk">
	<xsl:param name="node" select="."/>
	<xsl:param name="id" select="$node/@id"/>
	<xsl:param name="content">
		<xsl:apply-templates select="$node"/>
	</xsl:param>

	<xsl:choose>
		<xsl:when test="$yelp_chunk_method = 'yelp'">
			<xsl:comment> Start of chunk: [<xsl:value-of select="$id"/>] </xsl:comment>
			<xsl:copy-of select="$content"/>
			<xsl:comment> End of chunk </xsl:comment>
		</xsl:when>
		<xsl:when test="$yelp_chunk_method = 'exslt'">
			<exsl:document href="{concat($id, '.html')}">
				<html>
					<head>
						<xsl:call-template name="head.content">
							<xsl:with-param name="node" select="$node"/>
						</xsl:call-template>
						<xsl:call-template name="user.head.content">
							<xsl:with-param name="node" select="$node"/>
						</xsl:call-template>
					</head>
					<body>
						<xsl:call-template name="body.attributes"/>
						<xsl:call-template name="user.header.content">
							<xsl:with-param name="node" select="$node"/>
						</xsl:call-template>
						<xsl:copy-of select="$content"/>
						<xsl:call-template name="user.footer.content">
							<xsl:with-param name="node" select="$node"/>
						</xsl:call-template>
					</body>
				</html>
			</exsl:document>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template name="yelp.titlepage.chunk">
	<xsl:variable name="info" select="
		self::article/articleinfo   | self::appendix/appendixinfo     |
		self::chapter/chapterinfo   | self::preface/prefaceinfo       |
		self::sect1/sect1info       | self::sect2/sect2info           |
		self::sect3/sect3info       | self::sect4/sect4info           |
		self::sect5/sect5info       | self::section/sectioninfo       |
		self::set/setinfo           | self::book/bookinfo             |
		self::part/partinfo         | self::reference/referenceinfo   |
		self::glossary/glossaryinfo | self::index/indexinfo           |
		self::refentry/refentryinfo | self::simplesect/simplesectinfo |
		self::bibliography/bibliographyinfo"/>
	<xsl:if test="$info">
		<xsl:call-template name="yelp.chunk">
			<xsl:with-param name="id" select="'title-page'"/>
			<xsl:with-param name="node" select="$info"/>
			<xsl:with-param name="content">
				<xsl:call-template name="yelp.navbar.top">
					<xsl:with-param name="prev" select="false()"/>
					<xsl:with-param name="next" select="'toc'"/>
				</xsl:call-template>
				<xsl:choose>
					<xsl:when test="self::article/articleinfo">
						<xsl:call-template name="article.titlepage"/>
					</xsl:when>
					<xsl:when test="self::appendix/appendixinfo">
						<xsl:call-template name="appendix.titlepage"/>
					</xsl:when>
					<xsl:when test="self::chapter/chapterinfo">
						<xsl:call-template name="chapter.titlepage"/>
					</xsl:when>
					<xsl:when test="self::preface/prefaceinfo">
						<xsl:call-template name="preface.titlepage"/>
					</xsl:when>
					<xsl:when test="self::sect1/sect1info">
						<xsl:call-template name="sect1.titlepage"/>
					</xsl:when>
					<xsl:when test="self::sect2/sect2info">
						<xsl:call-template name="sect2.titlepage"/>
					</xsl:when>
					<xsl:when test="self::sect3/sect3info">
						<xsl:call-template name="sect3.titlepage"/>
					</xsl:when>
					<xsl:when test="self::sect4/sect4info">
						<xsl:call-template name="sect4.titlepage"/>
					</xsl:when>
					<xsl:when test="self::sect5/sect5info">
						<xsl:call-template name="sect5.titlepage"/>
					</xsl:when>
					<xsl:when test="self::section/sectioninfo">
						<xsl:call-template name="section.titlepage"/>
					</xsl:when>
					<xsl:when test="self::set/setinfo">
						<xsl:call-template name="set.titlepage"/>
					</xsl:when>
					<xsl:when test="self::book/bookinfo">
						<xsl:call-template name="book.titlepage"/>
					</xsl:when>
					<xsl:when test="self::part/partinfo">
						<xsl:call-template name="part.titlepage"/>
					</xsl:when>
					<xsl:when test="self::reference/referenceinfo">
						<xsl:call-template name="reference.titlepage"/>
					</xsl:when>
					<xsl:when test="self::glossary/glossaryinfo">
						<xsl:call-template name="glossary.titlepage"/>
					</xsl:when>
					<xsl:when test="self::index/indexinfo">
						<xsl:call-template name="index.titlepage"/>
					</xsl:when>
					<xsl:when test="self::bibliography/bibliographyinfo">
						<xsl:call-template name="bibliography.titlepage"/>
					</xsl:when>
					<xsl:when test="self::refentry/refentryinfo">
						<xsl:call-template name="refentry.titlepage"/>
					</xsl:when>
					<xsl:when test="self::simplesect/simplesectinfo">
						<xsl:call-template name="simplesect.titlepage"/>
					</xsl:when>
					<xsl:otherwise>
						<xsl:message>
							<xsl:text>Could not construct a titlepage for </xsl:text>
							<xsl:value-of select="local-name(.)"/>
							<xsl:text>.</xsl:text>
						</xsl:message>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:call-template name="yelp.navbar.bottom">
					<xsl:with-param name="prev" select="false()"/>
					<xsl:with-param name="next" select="'toc'"/>
					<xsl:with-param name="up" select="false()"/>
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template name="yelp.component.chunk">
	<xsl:param name="depth" select="0"/>
	<xsl:variable name="prev" select="yelp:get-prev(.)"/>
	<xsl:variable name="next" select="yelp:get-next(.)"/>

	<xsl:if test="($depth != 0) or ($yelp_max_chunk_depth = 0)">
		<xsl:call-template name="yelp.chunk">
			<xsl:with-param name="id" select="@id"/>
			<xsl:with-param name="node" select="."/>
			<xsl:with-param name="content">
				<xsl:call-template name="yelp.navbar.top">
					<xsl:with-param name="node" select="."/>
					<xsl:with-param name="prev" select="$prev"/>
					<xsl:with-param name="next" select="$next"/>
				</xsl:call-template>
				<div class="{local-name(.)}">
					<xsl:call-template name="yelp.title"/>
					<xsl:apply-templates select="yelp:get-content(.)"/>
					<xsl:choose>
						<xsl:when test="($depth &lt; $yelp_max_chunk_depth)
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
				<xsl:call-template name="yelp.navbar.bottom">
					<xsl:with-param name="node" select="."/>
					<xsl:with-param name="prev" select="$prev"/>
					<xsl:with-param name="next" select="$next"/>
				</xsl:call-template>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:if>

	<xsl:if test="($depth &lt; $yelp_max_chunk_depth)
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
	<xsl:variable name="next" select="yelp:get-next(.)"/>

	<xsl:comment> End of header </xsl:comment>

	<xsl:call-template name="yelp.titlepage.chunk"/>

	<xsl:call-template name="yelp.chunk">
		<xsl:with-param name="id" select="'toc'"/>
		<xsl:with-param name="node" select="."/>
		<xsl:with-param name="content">
			<xsl:call-template name="yelp.navbar.top">
				<xsl:with-param name="node" select="."/>
				<xsl:with-param name="prev" select="'title-page'"/>
				<xsl:with-param name="next" select="$next"/>
			</xsl:call-template>
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
				<xsl:apply-templates select="yelp:get-divisions(.)" mode="toc"/>
			</div>
			<xsl:call-template name="yelp.navbar.bottom">
				<xsl:with-param name="node" select="."/>
				<xsl:with-param name="prev" select="'title-page'"/>
				<xsl:with-param name="next" select="$next"/>
				<xsl:with-param name="up" select="false()"/>
			</xsl:call-template>
		</xsl:with-param>
	</xsl:call-template>

	<xsl:for-each select="yelp:get-divisions(.)">
		<xsl:call-template name="yelp.component.chunk">
			<xsl:with-param name="depth" select="1"/>
		</xsl:call-template>
	</xsl:for-each>

	<xsl:comment> Start of footer </xsl:comment>
</xsl:template>

</xsl:stylesheet>
