<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- == itemizedlist ======================================================= -->

<xsl:template match="itemizedlist">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="*[name(.) != 'listitem']"/>
		<ul>
			<xsl:if test="@mark">
				<xsl:attribute name="style">
					<xsl:text>list-style-type: </xsl:text>
					<xsl:choose>
						<xsl:when test="@mark = 'bullet'">disc</xsl:when>
						<xsl:when test="@mark = 'box'">square</xsl:when>
						<xsl:otherwise><xsl:value-of select="@mark"/></xsl:otherwise>
					</xsl:choose>
				</xsl:attribute>
			</xsl:if>
			<xsl:if test="@spacing = 'compact'">
				<xsl:attribute name="compact">
					<xsl:value-of select="@spacing"/>
				</xsl:attribute>
			</xsl:if>
			<xsl:apply-templates select="listitem"/>
		</ul>
	</div>
</xsl:template>

<xsl:template match="itemizedlist/listitem">
	<li>
		<xsl:if test="@override">
			<xsl:attribute name="style">
				<xsl:text>list-style-type: </xsl:text>
				<xsl:choose>
					<xsl:when test="@override = 'bullet'">disc</xsl:when>
					<xsl:when test="@override = 'box'">square</xsl:when>
					<xsl:otherwise><xsl:value-of select="@override"/></xsl:otherwise>
				</xsl:choose>
			</xsl:attribute>
		</xsl:if>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</li>
</xsl:template>

<!-- == orderedlist ======================================================== -->

<xsl:template name="orderedlist.start">
	<xsl:param name="list" select="."/>
	<xsl:choose>
		<xsl:when test="$list/@continutation != 'continues'">1</xsl:when>
		<xsl:otherwise>
			<xsl:variable name="prevlist"
				select="$list/preceding::orderedlist[1]"/>
			<xsl:choose>
				<xsl:when test="count($prevlist) = 0">1</xsl:when>
				<xsl:otherwise>
					<xsl:variable name="prevlength" select="count($prevlist/listitem)"/>
					<xsl:variable name="prevstart">
						<xsl:call-template name="orderedlist.start">
							<xsl:with-param name="list" select="$prevlist"/>
						</xsl:call-template>
					</xsl:variable>
					<xsl:value-of select="$prevstart + $prevlength"/>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="orderedlist">
	<xsl:variable name="start">
		<xsl:choose>
			<xsl:when test="@continuation = 'continues'">
				<xsl:call-template name="orderedlist.start"/>
			</xsl:when>
			<xsl:otherwise>1</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="*[name(.) != 'listitem']"/>
		<ol>
			<xsl:if test="@numeration">
				<xsl:attribute name="type">
					<xsl:choose>
						<xsl:when test="@numeration = 'arabic'">1</xsl:when>
						<xsl:when test="@numeration = 'loweralpha'">a</xsl:when>
						<xsl:when test="@numeration = 'lowerroman'">i</xsl:when>
						<xsl:when test="@numeration = 'upperalpha'">A</xsl:when>
						<xsl:when test="@numeration = 'upperroman'">I</xsl:when>
						<xsl:otherwise>1</xsl:otherwise>
					</xsl:choose>
				</xsl:attribute>
			</xsl:if>
			<xsl:if test="$start != '1'">
				<xsl:attribute name="start">
					<xsl:value-of select="$start"/>
				</xsl:attribute>
			</xsl:if>
			<xsl:if test="@spacing = 'compact'">
				<xsl:attribute name="compact">
					<xsl:value-of select="@spacing"/>
				</xsl:attribute>
			</xsl:if>
			<!-- FIXME: inheritnum -->
			<xsl:apply-templates select="listitem"/>
		</ol>
	</div>
</xsl:template>

<xsl:template match="orderedlist/listitem">
	<li>
		<xsl:if test="@override">
			<xsl:attribute name="value">
				<xsl:value-of select="@override"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</li>
</xsl:template>

<!-- == procedure ========================================================== -->

<xsl:template match="procedure">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="*[name(.) != 'step']"/>
		<xsl:choose>
			<xsl:when test="count(step) = 1">
				<ul>
					<xsl:apply-templates select="step"/>
				</ul>
			</xsl:when>
			<xsl:otherwise>
				<ol>
					<xsl:apply-templates select="step"/>
				</ol>
			</xsl:otherwise>
		</xsl:choose>
	</div>	
</xsl:template>

<xsl:template match="step">
	<li>
		<xsl:apply-templates/>
	</li>
</xsl:template>

<xsl:template match="substeps">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<ol>
			<xsl:apply-templates/>
		</ol>
	</div>
</xsl:template>

<!-- == segmentedlist ====================================================== -->

<xsl:template match="segmentedlist">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="title"/>
		<ul style="list-style-type: none;">
			<xsl:apply-templates select="seglistitem"/>
		</ul>
	</div>
</xsl:template>

<xsl:template match="seglistitem">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="segtitle">
	<b>
		<xsl:apply-templates/>
		<xsl:text>: </xsl:text>
	</b>
</xsl:template>

<xsl:template match="seg">
	<xsl:variable name="position" select="count(preceding-sibling::seg) + 1"/>
	<p>
		<li>
			<xsl:if test="$position = 1">
				<xsl:attribute name="class">
					<xsl:text>segfirst</xsl:text>
				</xsl:attribute>
			</xsl:if>
			<xsl:apply-templates
				select="../../segtitle[position() = $position]"/>
			<xsl:apply-templates/>
		</li>
	</p>
</xsl:template>

<!-- == simplelist ========================================================= -->

<xsl:template match="simplelist">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="member">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == variablelist ======================================================= -->

<xsl:template match="variablelist">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates select="*[name(.) != 'varlistentry']"/>
		<dl>
			<xsl:apply-templates select="varlistentry"/>
		</dl>
	</div>
</xsl:template>

<xsl:template match="varlistentry">
	<dt>
		<xsl:call-template name="anchor"/>
		<xsl:for-each select="term">
			<xsl:if test="position() != 1">
				<xsl:text>, </xsl:text>
			</xsl:if>
			<xsl:apply-templates select="."/>
		</xsl:for-each>
	</dt>
	<xsl:apply-templates select="listitem"/>
</xsl:template>

<xsl:template match="varlistentry/listitem">
	<dd>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</dd>
</xsl:template>

<xsl:template match="term">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

</xsl:stylesheet>
