<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template match="glossdef">
	<dd>
		<xsl:apply-templates select="*[name(.) != 'glossseealso']"/>
		<xsl:if test="glossseealso">
			<p>
				<xsl:call-template name="gettext">
					<xsl:with-param name="key" select="'See Also'"/>
				</xsl:call-template>
				<xsl:text> </xsl:text>
				<xsl:for-each select="glossseealso">
					<xsl:apply-templates select="."/>
					<xsl:choose>
						<xsl:when test="position() = last()">
							<xsl:text>.</xsl:text>
						</xsl:when>
						<xsl:otherwise>
							<xsl:text>, </xsl:text>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:for-each>
			</p>
		</xsl:if>
	</dd>
</xsl:template>

<xsl:template match="glossdiv">
	<div class="{name(.)}">
		<xsl:apply-templates select="(glossentry[1]/preceding-sibling::*)"/>
		<dl>
			<xsl:apply-templates select="glossentry"/>
		</dl>
	</div>
</xsl:template>

<xsl:template match="glossentry">
	<xsl:call-template name="block.simple"/>
</xsl:template>

<xsl:template match="glosslist">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<dl>
			<xsl:apply-templates/>
		</dl>
	</div>
</xsl:template>

<xsl:template match="glosssee">
	<dd>
		<p>
			<xsl:call-template name="gettext">
				<xsl:with-param name="key" select="'See'"/>
			</xsl:call-template>
			<xsl:choose>
				<xsl:when test="@otherterm">
					<a href="#{@otherterm}">
						<xsl:call-template name="xref.content">
							<xsl:with-param name="linkend" select="@otherterm"/>
						</xsl:call-template>
					</a>
				</xsl:when>
				<xsl:otherwise>
					<xsl:apply-templates/>
				</xsl:otherwise>
			</xsl:choose>
		</p>
	</dd>
</xsl:template>

<xsl:template match="glossseealso">
	<xsl:choose>
		<xsl:when test="@otherterm">
			<a href="#{@otherterm}">
				<xsl:call-template name="xref.content">
					<xsl:with-param name="linkend" select="@otherterm"/>
				</xsl:call-template>
			</a>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="glossterm">
	<dt>
		<xsl:apply-templates/>
	</dt>
</xsl:template>

</xsl:stylesheet>
