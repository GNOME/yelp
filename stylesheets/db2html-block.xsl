<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY is-division "
(name(.) = 'appendix')   or (name(.) = 'article')      or
(name(.) = 'book')       or (name(.) = 'bibliography') or
(name(.) = 'chapter')    or (name(.) = 'colophon')     or
(name(.) = 'glossary')   or (name(.) = 'index')        or
(name(.) = 'part')       or (name(.) = 'preface')      or
(name(.) = 'reference')  or (name(.) = 'refentry')     or
(name(.) = 'refsect1')   or (name(.) = 'refsect2')     or
(name(.) = 'refsect3')   or (name(.) = 'refsection')   or
(name(.) = 'sect1')      or (name(.) = 'sect2')        or
(name(.) = 'sect3')      or (name(.) = 'sect4')        or
(name(.) = 'sect5')      or (name(.) = 'section')      or
(name(.) = 'set')        or (name(.) = 'setindex')     or
(name(.) = 'simplesect') ">
<!ENTITY is-info "
(name(.) = 'appendixinfo')     or (name(.) = 'articleinfo')    or
(name(.) = 'bibliographyinfo') or (name(.) = 'bookinfo')       or
(name(.) = 'chapterinfo')      or (name(.) = 'glossaryinfo')   or
(name(.) = 'indexinfo')        or (name(.) = 'partinfo')       or
(name(.) = 'prefaceinfo')      or (name(.) = 'refentryinfo')   or
(name(.) = 'referenceinfo')    or (name(.) = 'refsect1info')   or
(name(.) = 'refsect2info')     or (name(.) = 'refsect3info')   or
(name(.) = 'refsectioninfo')   or (name(.) = 'sect1info')      or
(name(.) = 'sect2info')        or (name(.) = 'sect3info')      or
(name(.) = 'sect4info')        or (name(.) = 'sect5info')      or
(name(.) = 'sectioninfo')      or (name(.) = 'setinfo')        or
(name(.) = 'setindexinfo')     ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="block">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template name="block.pre">
	<pre class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</pre>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template match="abstract">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:if test="title or sidebarinfo/title">
			<xsl:call-template name="node.heading"/>
		</xsl:if>
		<xsl:apply-templates select="*[name(.) != 'title']"/>
	</div>
</xsl:template>

<xsl:template match="authorblurb">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="blockquote | epigraph">
	<div class="{name(.)}">
		<xsl:if test="title">
			<xsl:call-template name="node.heading"/>
		</xsl:if>
		<blockquote>
			<xsl:apply-templates
				select="*[name(.) != 'title' and name(.) != 'attribution']"/>
		</blockquote>
		<xsl:if test="attribution">
			<div class="attribution">
				<xsl:apply-templates select="attribution"/>
			</div>
		</xsl:if>
	</div>
</xsl:template>

<xsl:template match="caption">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="dedication">
	<div class="{name(.)}">
		<xsl:call-template name="node.heading"/>
		<xsl:apply-templates select="*[
			name(.) != 'title'       and
			name(.) != 'subtitle'    and
			name(.) != 'titleabbrev' ]"/>
	</div>
</xsl:template>

<xsl:template match="example">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<div class="heading">
			<i>
				<xsl:apply-templates select="." mode="node.header.header.mode"/>
			</i>
			<xsl:apply-templates select="title/node()"/>
		</div>
		<xsl:apply-templates select="*[
			name(.) != 'blockinfo'   and
			name(.) != 'title'       and
			name(.) != 'titleabbrev' ]"/>
	</div>
</xsl:template>

<xsl:template match="figure">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<div class="heading">
			<i>
				<xsl:apply-templates select="." mode="node.header.header.mode"/>
			</i>
			<xsl:apply-templates select="title/node()"/>
		</div>
		<xsl:apply-templates select="*[
			name(.) != 'blockinfo'   and
			name(.) != 'title'       and
			name(.) != 'titleabbrev' ]"/>
	</div>
</xsl:template>

<xsl:template match="formalpara">
	<div class="{name(.)}">
		<xsl:if test="title">
			<xsl:call-template name="node.heading"/>
		</xsl:if>
		<xsl:apply-templates select="*[name(.) != 'title']"/>
	</div>
</xsl:template>

<xsl:template match="highlights">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="informalexample">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template match="informalfigure">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template match="informaltable">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template match="literallayout">
	<xsl:call-template name="block.pre"/>
</xsl:template>

<xsl:template match="msg">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="msgaud | msglevel | msgorig">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="node.heading"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template match="msgentry">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="msgexplan">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="msginfo">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="para">
	<p>
		<xsl:call-template name="anchor"/>
		<xsl:if test="@role">
			<xsl:attribute name="class">
				<xsl:value-of select="@role"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</p>
</xsl:template>

<xsl:template match="partintro">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="programlisting">
	<xsl:call-template name="block.pre"/>
</xsl:template>

<xsl:template match="screen">
	<xsl:call-template name="block.pre"/>
</xsl:template>

<xsl:template match="sidebar">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:if test="title or sidebarinfo/title">
			<xsl:call-template name="node.heading"/>
		</xsl:if>
		<xsl:apply-templates select="*[
			(name(.) != 'sidebarinfo') and
			(name(.) != 'title')       and
			(name(.) != 'titleabbrev') ]"/>
	</div>
</xsl:template>

<xsl:template match="simpara">
	<p>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</p>
</xsl:template>

<xsl:template match="simplemsgentry">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="subtitle">
	<xsl:param name="depth" select="count(ancestor::*[&is-division;]) - 1"/>
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when test="$depth &lt; 6">
				<xsl:value-of select="concat('h', $depth + 2)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="'h7'"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:element name="{$element}">
		<xsl:attribute name="class" select="'subtitle'"/>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<xsl:template match="synopsis">
	<xsl:call-template name="block.pre"/>
</xsl:template>

<xsl:template match="table">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<div class="heading">
			<i>
				<xsl:apply-templates select="." mode="node.header.header.mode"/>
			</i>
			<xsl:apply-templates select="title/node()"/>
		</div>
		<xsl:apply-templates select="*[
			name(.) != 'blockinfo'   and
			name(.) != 'title'       and
			name(.) != 'titleabbrev' ]"/>
	</div>
</xsl:template>

<xsl:template match="title">
	<xsl:param name="depth" select="count(ancestor::*[&is-division;]) - 1"/>
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when test="$depth &lt; 7">
				<xsl:value-of select="concat('h', $depth + 1)"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="'h7'"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="parent" select="(parent::*[&is-info;]/.. | parent::*)[1]"/>
	<xsl:element name="{$element}">
		<xsl:attribute name="class">
			<xsl:value-of select="name($parent)"/>
		</xsl:attribute>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="$parent" mode="node.header.header.mode"/>
		<xsl:apply-templates/>
	</xsl:element>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
