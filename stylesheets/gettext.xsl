<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:variable name="l10n" select="document('l10n.xml')"/>
<xsl:key name="msgid" match="msg" use="msgid"/>

<!-- ======================================================================= -->

<xsl:template name="gettext">
	<xsl:param name="msgid"/>
	<xsl:param name="lang" select="$lang"/>

	<xsl:for-each select="$l10n">
		<xsl:variable name="msg" select="key('msgid', $msgid)"/>
		<xsl:choose>
			<xsl:when test="$msg/msgstr[@xml:lang = $lang]">
				<xsl:value-of select="$msg/msgstr[@xml:lang = $lang]"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$msg/msgstr[not(@xml:lang)]"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:for-each>
</xsl:template>

<xsl:template name="ngettext">
	<xsl:param name="msgid"/>
	<xsl:param name="msgid_plural"/>
	<xsl:param name="num" select="1"/>
	<xsl:param name="lang" select="$lang"/>

	<xsl:call-template name="gettext">
		<xsl:with-param name="msgid">
			<xsl:choose>
				<xsl:when test="$num = 1">
					<xsl:value-of select="$msgid"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="$msgid_plural"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:with-param>
		<xsl:with-param name="lang" select="$lang"/>
	</xsl:call-template>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="dingbat">
	<xsl:param name="dingbat"/>
	<xsl:choose>
		<xsl:when test="$dingbat = 'copyright'">
			<xsl:value-of select="'&#x00A9;'"/>
		</xsl:when>
		<xsl:when test="$dingbat = 'registered'">
			<xsl:value-of select="'&#x00AE;'"/>
		</xsl:when>
		<xsl:when test="$dingbat = 'trade'">
			<xsl:value-of select="'&#x2122;'"/>
		</xsl:when>
		<xsl:when test="$dingbat = 'service'">
			<xsl:value-of select="'&#x2120;'"/>
		</xsl:when>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="format.header.prefix.named">
	<xsl:param name="header"/>
	<xsl:value-of select="concat($header, '&#160;&#160;')"/>
</xsl:template>

<xsl:template name="format.header.prefix.unnamed">
	<xsl:param name="header"/>
	<xsl:value-of select="concat($header, '&#160;&#160;')"/>
</xsl:template>

<xsl:template name="format.header">
	<xsl:param name="header"/>
	<xsl:param name="number"/>
	<xsl:choose>
		<xsl:when test="string-length($number) &gt; 0">
			<xsl:value-of select="concat($header, '&#x00A0;', $number)"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="$header"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="format.appendix.number">
	<xsl:param name="appendix"/>
	<xsl:number format="A" value="$appendix"/>
</xsl:template>

<xsl:template name="format.article.number">
	<xsl:param name="article"/>
	<xsl:number format="I" value="$article"/>
</xsl:template>

<xsl:template name="format.chapter.number">
	<xsl:param name="chapter"/>
	<xsl:number value="$chapter"/>
</xsl:template>

<xsl:template name="format.example.number">
	<xsl:param name="parent"/>
	<xsl:param name="example"/>
	<xsl:value-of select="concat($parent, '-')"/>
	<xsl:number value="$example"/>
</xsl:template>

<xsl:template name="format.figure.number">
	<xsl:param name="parent"/>
	<xsl:param name="figure"/>
	<xsl:value-of select="concat($parent, '-')"/>
	<xsl:number value="$figure"/>
</xsl:template>

<xsl:template name="format.part.number">
	<xsl:param name="part"/>
	<xsl:number format="I" value="$part"/>
</xsl:template>

<xsl:template name="format.reference.number">
	<xsl:param name="article"/>
	<xsl:number format="I" value="$article"/>
</xsl:template>

<xsl:template name="format.section.number">
	<xsl:param name="section"/>
	<xsl:number value="$section"/>
</xsl:template>

<xsl:template name="format.subsection.number">
	<xsl:param name="parent"/>
	<xsl:param name="section"/>
	<xsl:value-of select="concat($parent, '.')"/>
	<xsl:number value="$section"/>
</xsl:template>

<xsl:template name="format.table.number">
	<xsl:param name="parent"/>
	<xsl:param name="table"/>
	<xsl:value-of select="concat($parent, '-')"/>
	<xsl:number value="$table"/>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template name="plural">
	<xsl:param name="num" select="1"/>
	<xsl:param name="lang" select="$lang"/>

	<xsl:choose>
		<!-- cs -->
		<xsl:when test="$lang = 'cs'">
			<xsl:choose>
				<xsl:when test="($num mod 10 = 1) and ($num mod 100 != 11)">
					<xsl:value-of select="0"/>
				</xsl:when>
				<xsl:when test="
						($num mod 10 &gt;= 2) and ($num mod 10 &lt;= 4) and
						(($num mod 100 &lt; 10) or ($num mod 100 &gt;=20))
					">
					<xsl:value-of select="1"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="2"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:when>

		<!-- ja -->
		<xsl:when test="$lang = 'ja'">
			<xsl:value-of select="0"/>
		</xsl:when>

		<!-- sr sr@Latn -->
		<xsl:when test="$lang = 'sr' or $lang = 'sr@Latn'">
			<xsl:choose>
				<xsl:when test="($num mod 10 = 1) and ($num mod 100 != 11)">
					<xsl:value-of select="0"/>
				</xsl:when>
				<xsl:when test="
						($num mod 10 &gt;= 2) and ($num mod 10 &lt;= 4) and
						(($num mod 100 &lt; 10) or ($num mod 100 &gt;=20))
					">
					<xsl:value-of select="1"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="2"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:when>

		<!-- C -->
		<xsl:otherwise>
			<xsl:choose>
				<xsl:when test="$num = 1">
					<xsl:value-of select="0"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="1"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
