<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template name="person.name">
	<xsl:param name="node" select="."/>
	<xsl:choose>
		<xsl:when test="$node/personname">
			<xsl:call-template name="person.name">
				<xsl:with-param name="node" select="$node/personname"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="name($node)='corpauthor'">
			<xsl:apply-templates select="$node"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:if test="$node/honorific">
				<xsl:value-of select="$node/honorific[1]"/>
				<xsl:text>.</xsl:text>
			</xsl:if>
			<xsl:choose>
				<xsl:when test="$node/@role = 'family-given'">
					<xsl:if test="$node/surname">
						<xsl:if test="$node/honorific">
							<xsl:text> </xsl:text>
						</xsl:if>
						<xsl:value-of select="$node/surname[1]"/>
					</xsl:if>
					<xsl:if test="$node/othername">
						<xsl:if test="$node/honorific or $node/surname">
							<xsl:text> </xsl:text>
						</xsl:if>
						<xsl:value-of select="$node/othername[1]"/>
					</xsl:if>
					<xsl:if test="$node/firstname">
						<xsl:if test="$node/honorific or $node/surname or $node/othername">
							<xsl:text> </xsl:text>
						</xsl:if>
						<xsl:value-of select="$node/firstname[1]"/>
					</xsl:if>
				</xsl:when>
				<xsl:otherwise>
					<xsl:if test="$node/firstname">
						<xsl:if test="$node/honorific">
							<xsl:text> </xsl:text>
						</xsl:if>
						<xsl:value-of select="$node/firstname[1]"/>
					</xsl:if>
					<xsl:if test="$node/othername">
						<xsl:if test="$node/honorific or $node/firstname">
							<xsl:text> </xsl:text>
						</xsl:if>
						<xsl:value-of select="$node/othername[1]"/>
					</xsl:if>
					<xsl:if test="$node/surname">
						<xsl:if test="$node/honorific or $node/firstname or $node/othername">
							<xsl:text> </xsl:text>
						</xsl:if>
						<xsl:value-of select="$node/surname[1]"/>
					</xsl:if>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:if test="$node/lineage">
				<xsl:text>, </xsl:text>
				<xsl:value-of select="$node/lineage"/>
			</xsl:if>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="copy-string">
	<xsl:param name="string"/>
	<xsl:param name="count" select="0"/>
	<xsl:param name="result"/>
	<xsl:choose>
		<xsl:when test="$count &gt; 0">
			<xsl:call-template name="copy-string">
				<xsl:with-param name="string" select="$string"/>
				<xsl:with-param name="count" select="$count - 1"/>
				<xsl:with-param name="result">
					<xsl:value-of select="$result"/>
					<xsl:value-of select="$string"/>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="$result"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

</xsl:stylesheet>
