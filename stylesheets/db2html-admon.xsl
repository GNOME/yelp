<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template match="caution | important | note | tip | warning">
	<div class="admonition"><div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:choose>
			<xsl:when test="$text_only">
				<xsl:apply-templates/>
			</xsl:when>
			<xsl:otherwise>
				<table style="border: none;">
					<tr>
						<td rowspan="2" align="center" valign="top">
							<img>
								<xsl:attribute name="src">
									<xsl:value-of select="$admon_graphics_path"/>
									<xsl:value-of select="name(.)"/>
									<xsl:value-of select="$admon_graphics_extension"/>
								</xsl:attribute>
							</img>
						</td>
						<th align="left" valign="top">
							<xsl:apply-templates select="title"/>
						</th>
					</tr>
					<tr>
						<td align="left" valign="top">
							<xsl:apply-templates select="*[name(.) != 'title']"/>
						</td>
					</tr>
				</table>
			</xsl:otherwise>
		</xsl:choose>
	</div></div>
</xsl:template>

</xsl:stylesheet>
