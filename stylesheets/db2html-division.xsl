<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY divcomponent "
abstract            | address             | anchor         | authorblurb      |
beginpage           | blockquote          | bridgehead     | calloutlist      |
caution             | classsynopsis       | cmdsynopsis    | constraintdef    |
constructorsynopsis | desctructorsynopsis | epigraph       | equation         |
example             | fieldsynopsis       | figure         | formalpara       |
funcsynopsis        | glosslist           | graphic        | graphicco        |
highlights          | important           | indexterm      | informalequation |
informalexample     | informalfigure      | informaltable  | itemizedlist     |
literallayout       | mediaobject         | mediaobjectco  | methodsynopsis   |
msgset              | note                | orderedlist    | para             |
procedure           | productionset       | programlisting | programlistingco |
qandaset            | remark              | screen         | screenco         |
screenshot          | segmentedlist       | sidebar        | simpara          |
simplelist          | synopsis            | table          | tip              |
variablelist        | warning             ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template match="
		appendix  | article  | book     | bibliography | chapter    |
		colophon  | glossary | index    | part         | preface    |
		reference | refsect1 | refsect2 | refsect3     | refsection |
		sect1     | sect2    | sect3    | sect4        | sect5      |
		section   | set      | setindex | simplesect   ">
	<xsl:param name="depth" select="0"/>
	<xsl:param name="leaf" select="true()"/>
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="." mode="content.mode">
			<xsl:with-param name="depth" select="$depth"/>
		</xsl:apply-templates>
		<xsl:choose>
			<xsl:when test="$leaf">
				<xsl:apply-templates select="." mode="division.mode">
					<xsl:with-param name="depth" select="$depth"/>
				</xsl:apply-templates>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="toc"/>
			</xsl:otherwise>
		</xsl:choose>
	</div>
</xsl:template>

<xsl:template match="refentry">
	<xsl:param name="depth" select="0"/>
	<xsl:param name="leaf" select="true()"/>
	<xsl:if test="preceding-sibling::refentry and $depth &gt; 0">
		<hr class="refentry.seperator"/>
	</xsl:if>
	<div class="{name(.)}">
		<xsl:apply-templates select="." mode="content.mode">
			<xsl:with-param name="depth" select="$depth"/>
		</xsl:apply-templates>
		<xsl:choose>
			<xsl:when test="$leaf">
				<xsl:apply-templates select="." mode="division.mode">
					<xsl:with-param name="depth" select="$depth"/>
				</xsl:apply-templates>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="toc"/>
			</xsl:otherwise>
		</xsl:choose>
	</div>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="content.mode" match="
		appendix | article  | chapter    | colophon   | preface | refsect1 |
		refsect2 | refsect3 | refsection | sect1      | sect2   | sect3    |
		sect4    | sect5    | section    | simplesect ">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="&divcomponent;"/>
</xsl:template>

<xsl:template mode="content.mode" match="book">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="dedication"/>
	<xsl:apply-templates select="preface[@role = 'bookintro']"/>
</xsl:template>

<xsl:template mode="content.mode" match="bibliography">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="
		&divcomponent; | bibliodiv | biblioentry | bibliomixed"/>
</xsl:template>

<xsl:template mode="content.mode" match="glossary">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="&divcomponent; | glossdiv | glossentry"/>
</xsl:template>

<xsl:template mode="content.mode" match="index | setindex">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="
		&divcomponent; | indexdiv | indexentry"/>
</xsl:template>

<xsl:template mode="content.mode" match="part | reference">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="partintro"/>
</xsl:template>

<xsl:template mode="content.mode" match="refentry">
	<xsl:param name="depth" select="0"/>
	<xsl:choose>
		<xsl:when test="refmeta/refentrytitle">
			<xsl:apply-templates select="refmeta/refentrytitle">
				<xsl:with-param name="depth" select="$depth"/>
			</xsl:apply-templates>
		</xsl:when>
		<xsl:when test="refentryinfo/title">
			<xsl:apply-templates select="refentryinfo/title">
				<xsl:with-param name="depth" select="$depth"/>
			</xsl:apply-templates>
		</xsl:when>
	</xsl:choose>
	<xsl:apply-templates
		select="*[name(.) != 'refsect1' and name(.) != 'refsection']"/>
</xsl:template>

<xsl:template mode="content.mode" match="set">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="title | subtitle">
		<xsl:with-param name="depth" select="$depth"/>
	</xsl:apply-templates>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="division.mode" match="
		appendix | article | chapter | preface">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | sect1 | section | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="book">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="
			appendix | article | bibliography | chapter   | colophon |
			glossary | index   | part         | reference | setindex |
			preface[@role != 'bookintro']     ">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="bibliography"/>
<xsl:template mode="division.mode" match="colophon"/>

<xsl:template mode="division.mode" match="glossary">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="bibliography">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="index"/>

<xsl:template mode="division.mode" match="part">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="
			appendix | article | bibliography | chapter   | glossary |
			index    | preface | refentry     | reference ">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="refentry">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refsect1 | refsection">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="reference">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="refsect1">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refsect2">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="refsect2">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refsect3">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="refsect3"/>

<xsl:template mode="division.mode" match="refsection">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refsection">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="sect1">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | sect2 | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="sect2">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | sect3 | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="sect3">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | sect4 | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="sect4">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | sect5 | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="sect5">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="section">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="refentry | section | simplesect">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="set">
	<xsl:param name="depth" select="0"/>
	<xsl:apply-templates select="book | setindex">
		<xsl:with-param name="depth" select="$depth + 1"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template mode="division.mode" match="setindex"/>
<xsl:template mode="division.mode" match="simplesect"/>

<!-- ======================================================================= -->

</xsl:stylesheet>
