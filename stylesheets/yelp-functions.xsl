<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:func="http://exslt.org/functions"
                xmlns:yelp="http://www.gnome.org/"
                extension-element-prefixes="func yelp"
                version='1.0'>

<func:function name="yelp:get-prev">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="
				(yelp:get-depth(.) = $yelp_max_chunk_depth) and
				$node/preceding-sibling::*[yelp:is-division(.)]">
			<func:result select="$node/preceding-sibling::*[yelp:is-division(.)][1]"/>
		</xsl:when>
		<xsl:when test="
				($node/preceding-sibling::*/descendant-or-self::*)
				[@id]
				[yelp:is-division(.)]
				[yelp:get-depth(.) &lt;= $yelp_max_chunk_depth]">
			<func:result select="
				($node/preceding-sibling::*/descendant-or-self::*)
				[@id]
				[yelp:is-division(.)]
				[yelp:get-depth(.) &lt;= $yelp_max_chunk_depth]
				[last()]"/>
		</xsl:when>
		<xsl:when test="yelp:get-depth($node) &gt; 1">
			<func:result select="$node/ancestor::*[@id][yelp:is-division(.)][1]"/>
		</xsl:when>
		<xsl:otherwise>
			<func:result select="'toc'"/>
		</xsl:otherwise>
	</xsl:choose>
</func:function>

<func:function name="yelp:get-next">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="
				(yelp:get-depth($node) &lt; $yelp_max_chunk_depth) and
				(count(yelp:get-divisions($node)[@id]) &gt; 0)">
			<func:result select="yelp:get-divisions($node)[@id][1]"/>
		</xsl:when>
		<xsl:when test="$node/following-sibling::*[@id][yelp:is-division(.)]">
			<func:result select="$node/following-sibling::*[@id][yelp:is-division(.)][1]"/>
		</xsl:when>
		<xsl:when test="$node/following::*[@id][yelp:is-division(.)]">
			<func:result select="$node/following::*[@id][yelp:is-division(.)][1]"/>
		</xsl:when>
		<xsl:otherwise>
			<func:result select="false()"/>
		</xsl:otherwise>
	</xsl:choose>
</func:function>

<func:function name="yelp:get-depth">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="/part and (count(/part/chapter) = 1)">
			<func:result select="count($node/ancestor::*[yelp:is-division(.)]) - 1"/>
		</xsl:when>
		<xsl:otherwise>
			<func:result select="count($node/ancestor::*[yelp:is-division(.)])"/>
		</xsl:otherwise>
	</xsl:choose>
</func:function>

<func:function name="yelp:get-title-text">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="yelp:get-title($node)">
			<func:result select="yelp:get-title($node)/text()"/>
		</xsl:when>
		<xsl:when test="$node/self::abstract">
			<func:result>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">Abstract</xsl:with-param>
				</xsl:call-template>
			</func:result>
		</xsl:when>
		<xsl:when test="$node/self::colophon">
			<func:result>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">Colophon</xsl:with-param>
				</xsl:call-template>
			</func:result>
		</xsl:when>
		<xsl:when test="$node/self::dedication">
			<func:result>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">Dedication</xsl:with-param>
				</xsl:call-template>
			</func:result>
		</xsl:when>
		<xsl:when test="$node/self::glossary">
			<func:result>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">Glossary</xsl:with-param>
				</xsl:call-template>
			</func:result>
		</xsl:when>
		<xsl:when test="$node/self::index">
			<func:result>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">Index</xsl:with-param>
				</xsl:call-template>
			</func:result>
		</xsl:when>
		<xsl:when test="$node/self::preface">
			<func:result>
				<xsl:call-template name="gentext">
					<xsl:with-param name="key">Preface</xsl:with-param>
				</xsl:call-template>
			</func:result>
		</xsl:when>
	</xsl:choose>
</func:function>

<func:function name="yelp:get-title">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="$node/title">
			<func:result select="$node/title"/>
		</xsl:when>
		<xsl:when test="$node/self::appendix/appendixinfo/title">
			<func:result select="$node/appendixinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::article/articleinfo/title">
			<func:result select="$node/articleinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::bibliography/bibliographyinfo/title">
			<func:result select="$node/bibliographyinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::book/bookinfo/title">
			<func:result select="$node/bookinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::chapter/chapterinfo/title">
			<func:result select="$node/chapterinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::glossary/glossaryinfo/title">
			<func:result select="$node/glossaryinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::index/indexinfo/title">
			<func:result select="$node/indexinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::part/partinfo/title">
			<func:result select="$node/partinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::preface/prefaceinfo/title">
			<func:result select="$node/prefaceinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::refentry/refmeta/refentrytitle">
			<func:result select="$node/self::refentry/refmeta/refentrytitle"/>
		</xsl:when>
		<xsl:when test="$node/self::refentry/refentryinfo/title">
			<func:result select="$node/refentryinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::reference/referenceinfo/title">
			<func:result select="$node/referenceinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::sect1/sect1info/title">
			<func:result select="$node/sect1info/title"/>
		</xsl:when>
		<xsl:when test="$node/self::sect2/sect2info/title">
			<func:result select="$node/sect2info/title"/>
		</xsl:when>
		<xsl:when test="$node/self::sect3/sect3info/title">
			<func:result select="$node/sect3info/title"/>
		</xsl:when>
		<xsl:when test="$node/self::sect4/sect4info/title">
			<func:result select="$node/sect4info/title"/>
		</xsl:when>
		<xsl:when test="$node/self::sect5/sect5info/title">
			<func:result select="$node/sect5info/title"/>
		</xsl:when>
		<xsl:when test="$node/self::section/sectioninfo/title">
			<func:result select="$node/sectioninfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::set/setinfo/title">
			<func:result select="$node/setinfo/title"/>
		</xsl:when>
		<xsl:when test="$node/self::setindex/setindexinfo/title">
			<func:result select="$node/setindexinfo/title"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:message>
				<xsl:text>yelp:get-title could not determine the title </xsl:text>
				<xsl:text>of node with type "</xsl:text>
				<xsl:value-of select="local-name($node)"/>
				<xsl:text>"</xsl:text>
			</xsl:message>
			<func:result select="/foo"/>
		</xsl:otherwise>
	</xsl:choose>
</func:function>

<func:function name="yelp:get-divisions">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<!-- The id-less part hackery -->
		<xsl:when test="$node/self::part and not($node/@id)">
			<func:result select="yelp:get-divisions(
					$node/appendix | $node/chapter      | $node/index    |
					$node/glossary | $node/bibliography | $node/article  |
					$node/preface  | $node/refentry     | $node/reference)"/>
		</xsl:when>
		<xsl:when test="$node/self::appendix | $node/self::chapter
				| $node/self::preface">
			<func:result select="$node/sect1 | $node/refentry | $node/simplesect
				| $node/section | $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::article">
			<func:result select="$node/sect1 | $node/refentry | $node/simplesect
				| $node/section | $node/index | $node/glossary | $node/bibliography
				| $node/appendix"/>
		</xsl:when>
		<xsl:when test="$node/self::sect1">
			<func:result select="$node/sect2 | $node/simplesect | $node/refentry
				| $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::sect2">
			<func:result select="$node/sect3 | $node/simplesect | $node/refentry
				| $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::sect3">
			<func:result select="$node/sect4 | $node/simplesect | $node/refentry
				| $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::sect4">
			<func:result select="$node/sect5 | $node/simplesect | $node/refentry
				| $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::sect5">
			<func:result select="$node/simplesect | $node/refentry
				| $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::section">
			<func:result select="$node/section | $node/simplesect | $node/refentry
				| $node/index | $node/glossary | $node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::set">
			<func:result select="$node/book | $node/setindex"/>
		</xsl:when>
		<xsl:when test="$node/self::book">
			<func:result select="$node/glossary | $node/bibliography
				| $node/preface | $node/chapter | $node/reference | $node/part
				| $node/article | $node/appendix | $node/index | $node/setindex
				| $node/colophon"/>
		</xsl:when>
		<xsl:when test="$node/self::part">
			<func:result select="$node/appendix | $node/chapter | $node/index
				| $node/glossary | $node/bibliography | $node/article
				| $node/preface | $node/refentry | $node/reference"/>
		</xsl:when>
		<xsl:when test="$node/self::reference">
			<func:result select="refentry"/>
		</xsl:when>
		<xsl:when test="$node/self::glossary">
			<func:result select="$node/bibliography"/>
		</xsl:when>
		<xsl:when test="$node/self::index | $node/self::setindex
				| $node/self::bibliography | $node/self::colophon
				| $node/self::refentry | $node/self::simplesect">
			<func:result select="/foo"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:message>
				<xsl:text>yelp:get-divisions called on unknown </xsl:text>
				<xsl:text>node with type "</xsl:text>
				<xsl:value-of select="local-name($node)"/>
				<xsl:text>"</xsl:text>
			</xsl:message>
			<func:result select="/foo"/>
		</xsl:otherwise>
	</xsl:choose>
</func:function>

<func:function name="yelp:is-division">
	<xsl:param name="node" select="."/>
	<func:result select="
		(local-name($node) = 'appendix') or (local-name($node) = 'chapter') or
		(local-name($node) = 'preface') or (local-name($node) = 'article') or
		(local-name($node) = 'sect1') or (local-name($node) = 'sect2') or
		(local-name($node) = 'sect3') or (local-name($node) = 'sect4') or
		(local-name($node) = 'sect5') or (local-name($node) = 'section') or
		(local-name($node) = 'set') or (local-name($node) = 'book') or
		(local-name($node) = 'part') or (local-name($node) = 'reference') or
		(local-name($node) = 'glossary') or (local-name($node) = 'index') or
		(local-name($node) = 'setindex') or (local-name($node) = 'bibliography') or
		(local-name($node) = 'colophon') or (local-name($node) = 'refentry') or
		(local-name($node) = 'simplesect')"/>
</func:function>

<func:function name="yelp:get-content">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="$node/self::appendix | $node/self::article
				| $node/self::chapter | $node/self::preface | $node/self::sect1
				| $node/self::sect2 | $node/self::sect3 | $node/self::sect4
				| $node/self::sect5 | $node/self::section | $node/self::simplesect">
			<func:result select="yelp:get-divcomponent.mix($node)"/>
		</xsl:when>
		<xsl:when test="$node/self::bibliography">
			<func:result select="yelp:get-component.mix($node)
				| ($node/bibliodiv | $node/biblioentry | $node/bibliomixed)"/>
		</xsl:when>
		<xsl:when test="$node/self::glossary">
			<func:result select="yelp:get-component.mix($node)
				| ($node/glossdiv | $node/glossentry)"/>
		</xsl:when>
		<xsl:when test="$node/self::index | $node/self::setindex">
			<func:result select="yelp:get-component.mix($node)"/>
		</xsl:when>
		<xsl:when test="$node/self::book">
			<func:result select="$node/dedication"/>
		</xsl:when>
		<xsl:when test="$node/self::colophon">
			<func:result select="yelp:get-textobject.mix($node)"/>
		</xsl:when>
		<xsl:when test="$node/self::part | $node/self::reference">
			<func:result select="$node/partintro"/>
		</xsl:when>
		<xsl:when test="$node/self::refentry">
			<func:result select="yelp:get-ndxterm.class($node)
				| ($node/refsect1 | $node/refsection)"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:message>
				<xsl:text>yelp:get-content called on unknown node </xsl:text>
				<xsl:value-of select="local-name($node)"/>
			</xsl:message>
			<func:result select="/foo"/>
		</xsl:otherwise>
	</xsl:choose>
</func:function>

<!-- ======================================================================= -->
<func:function name="yelp:get-textobject.mix">
	<xsl:param name="node" select="."/>
	<func:result select="
		yelp:get-list.class($node)         | yelp:get-admon.class($node)     |
		yelp:get-linespecific.class($node) | yelp:get-para.class($node)      |
		($node/blockquote)"/>
</func:function>
<func:function name="yelp:get-component.mix">
	<xsl:param name="node" select="."/>
	<func:result select="
		yelp:get-list.class($node)         | yelp:get-admon.class($node)     |
		yelp:get-linespecific.class($node) | yelp:get-synop.class($node)     |
		yelp:get-para.class($node)         | yelp:get-informal.class($node)  |
		yelp:get-formal.class($node)       | yelp:get-compound.class($node)  |
		yelp:get-genobj.class($node)       | yelp:get-descobj.class($node)   |
		yelp:get-ndxterm.class($node)"/>
</func:function>
<func:function name="yelp:get-divcomponent.mix">
	<xsl:param name="node" select="."/>
	<func:result select="
		yelp:get-list.class($node)         | yelp:get-admon.class($node)     |
		yelp:get-linespecific.class($node) | yelp:get-synop.class($node)     |
		yelp:get-para.class($node)         | yelp:get-informal.class($node)  |
		yelp:get-formal.class($node)       | yelp:get-compound.class($node)  |
		yelp:get-genobj.class($node)       | yelp:get-descobj.class($node)   |
		yelp:get-ndxterm.class($node)"/>
</func:function>
<func:function name="yelp:get-refcomponent.mix">
	<xsl:param name="node" select="."/>
	<func:result select="
		yelp:get-list.class($node)         | yelp:get-admon.class($node)     |
		yelp:get-linespecific.class($node) | yelp:get-synop.class($node)     |
		yelp:get-para.class($node)         | yelp:get-informal.class($node)  |
		yelp:get-formal.class($node)       | yelp:get-compound.class($node)  |
		yelp:get-genobj.class($node)       | yelp:get-descobj.class($node)   |
		yelp:get-ndxterm.class($node)"/>
</func:function>

<!-- ======================================================================= -->
<func:function name="yelp:get-list.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/calloutlist | $node/glosslist
		| $node/itemizedlist | $node/orderedlist | $node/segmentedlist
		| $node/simplelist | $node/variablelist"/>
</func:function>
<func:function name="yelp:get-admon.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/caution | $node/important | $node/note
		| $node/tip | $node/warning"/>
</func:function>
<func:function name="yelp:get-linespecific.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/literallayout | $node/programlisting
		| $node/programlistingco | $node/screen | $node/screenco
		| $node/screenshot"/>
</func:function>
<func:function name="yelp:get-synop.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/construcotsynopsis | $node/destructorsynopsis
		| $node/synopsis | $node/cmdsynopsis | $node/funcsynopsis
		| $node/classsynopsis | $node/fieldsynopsis"/>
</func:function>
<func:function name="yelp:get-para.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/formalpara | $node/para | $node/simpara"/>
</func:function>
<func:function name="yelp:get-informal.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/address | $node/graphic | $node/graphicco
		| $node/medaiobject | $node/mediaobjectco | $node/informalequation
		| $node/informalexample | $node/informalfigure | $node/informaltable"/>
</func:function>
<func:function name="yelp:get-formal.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/equation | $node/example | $node/figure
		| $node/table"/>
</func:function>
<func:function name="yelp:get-compound.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/msgset | $node/procedure | $node/sidebar
		| $node/qandaset"/>
</func:function>
<func:function name="yelp:get-genobj.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/anchor | $node/bridgehead | $node/remark
		| $node/highlights"/>
</func:function>
<func:function name="yelp:get-descobj.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/abstract | $node/authorblurb | $node/epigraph"/>
</func:function>
<func:function name="yelp:get-ndxterm.class">
	<xsl:param name="node" select="."/>
	<func:result select="$node/indexterm"/>
</func:function>

</xsl:stylesheet>