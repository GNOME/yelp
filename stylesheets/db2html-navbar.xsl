<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY is-division "(
	(name(.) = 'appendix')	or (name(.) = 'article')      or
	(name(.) = 'book')	or (name(.) = 'bibliography') or
	(name(.) = 'chapter')	or (name(.) = 'colophon')     or
	(name(.) = 'glossary')	or (name(.) = 'index')        or
	(name(.) = 'part')	or (name(.) = 'preface')      or
	(name(.) = 'reference')	or (name(.) = 'refentry')     or
	(name(.) = 'refsect1')	or (name(.) = 'refsect2')     or
	(name(.) = 'refsect3')	or (name(.) = 'refsection')   or
	(name(.) = 'sect1')	or (name(.) = 'sect2')        or
	(name(.) = 'sect3')	or (name(.) = 'sect4')        or
	(name(.) = 'sect5')	or (name(.) = 'section')      or
	(name(.) = 'set')	or (name(.) = 'setindex')     or
	(name(.) = 'simplesect'))">
<!ENTITY is-info "(
	(name(.) = 'appendixinfo')	or (name(.) = 'articleinfo')    or
	(name(.) = 'bibliographyinfo')	or (name(.) = 'bookinfo')       or
	(name(.) = 'chapterinfo')	or (name(.) = 'glossaryinfo')   or
	(name(.) = 'indexinfo')		or (name(.) = 'partinfo')       or
	(name(.) = 'prefaceinfo')	or (name(.) = 'refentryinfo')   or
	(name(.) = 'referenceinfo')	or (name(.) = 'refsect1info')   or
	(name(.) = 'refsect2info')	or (name(.) = 'refsect3info')   or
	(name(.) = 'refsectioninfo')	or (name(.) = 'sect1info')      or
	(name(.) = 'sect2info')		or (name(.) = 'sect3info')      or
	(name(.) = 'sect4info')		or (name(.) = 'sect5info')      or
	(name(.) = 'sectioninfo')	or (name(.) = 'setinfo')        or
	(name(.) = 'setindexinfo'))">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- == navbar.following.link ============================================== -->

<xsl:template name="navbar.following.link">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:choose>
		<xsl:when test="
				($depth_chunk &lt; $chunk_depth)				and
				$node/following-sibling::*[&is-division;]	">
			<xsl:call-template name="xref">
				<xsl:with-param name="target"
					select="$node/following-sibling::*[&is-division;][1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$node/..">
			<xsl:call-template name="navbar.following.link">
				<xsl:with-param name="node" select="$node/.."/>
				<xsl:with-param name="depth_chunk" select="$depth_chunk - 1"/>
			</xsl:call-template>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<xsl:template match="*" mode="navbar.following.link.mode">
<!--
	<xsl:variable name="depth" select="count(ancestor::*[&is-div;])"/>
	<xsl:choose>
		<xsl:when test="not(&is-div;)">
			<xsl:apply-templates mode="navbar.following.link.mode"
				 select="ancestor::*[&is-div;][1]"/>
		</xsl:when>
		<xsl:when test="following-sibling::*[&is-div;]">
			<xsl:variable name="foll"
				select="following-sibling::*[&is-div;][1]"/>
			<xsl:call-template name="xref">
				<xsl:with-param name="linkend" select="$foll/@id"/>
				<xsl:with-param name="target" select="$foll"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$depth &gt; 0">
			<xsl:apply-templates select=".." mode="navbar.following.link.mode"/>
		</xsl:when>
	</xsl:choose>
-->
</xsl:template>

<!-- == navbar.last.link =================================================== -->

<xsl:template name="navbar.last.link">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:choose>
		<xsl:when test="($depth_chunk &lt; $chunk_depth) and $node/*[&is-division;]">
			<xsl:call-template name="navbar.last.link">
				<xsl:with-param name="node" select="$node/*[&is-division;][last()]"/>
				<xsl:with-param name="depth_chunk" select="$depth_chunk + 1"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="xref">
				<xsl:with-param name="target" select="$node"/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- == navbar.prev.link =================================================== -->

<xsl:template name="navbar.prev.link">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:choose>
		<xsl:when test="($node = /*)">
			<xsl:if test="$generate_titlepage and $node/*[&is-info;]">
				<xsl:call-template name="xref">
					<xsl:with-param name="target" select="$node/*[&is-info;]"/>
				</xsl:call-template>
			</xsl:if>
		</xsl:when>
		<xsl:when test="$node/preceding-sibling::*[&is-division;]">
			<xsl:choose>
				<xsl:when test="$depth_chunk &lt; $chunk_depth">
					<xsl:call-template name="navbar.last.link">
						<xsl:with-param name="node"
							select="$node/preceding-sibling::*[&is-division;][1]"/>
						<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
					</xsl:call-template>
				</xsl:when>
				<xsl:otherwise>
					<xsl:call-template name="xref">
						<xsl:with-param name="target"
							select="$node/preceding-sibling::*[&is-division;][1]"/>
					</xsl:call-template>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="xref">
				<xsl:with-param name="target" select="$node/.."/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- == navbar.next.link =================================================== -->

<xsl:template name="navbar.next.link">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:choose>
		<xsl:when test="$node/self::*[&is-info;]">
			<xsl:call-template name="xref">
				<xsl:with-param name="target" select="$node/.."/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when
				test="$depth_chunk &lt; $chunk_depth and $node/*[&is-division;]">
			<xsl:call-template name="xref">
				<xsl:with-param name="target" select="$node/*[&is-division;][1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="following-sibling::*[&is-division;]">
			<xsl:call-template name="xref">
				<xsl:with-param name="target"
					select="following-sibling::*[&is-division;][1]"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="navbar.following.link">
				<xsl:with-param name="node" select="$node/.."/>
				<xsl:with-param name="depth_chunk" select="$depth_chunk - 1"/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
