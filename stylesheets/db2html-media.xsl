<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="mediadata">
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when test="
					name(.) = 'imagedata'     or
					name(.) = 'graphic'       or
					name(.) = 'inlinegraphic' ">
				<xsl:value-of select="'img'"/>
			</xsl:when>
		</xsl:choose>
	</xsl:variable>
	<xsl:element name="{$element}">
		<xsl:attribute name="src">
			<xsl:value-of select="$mediaobject_path"/>
			<xsl:choose>
				<xsl:when test="@fileref">
					<!-- FIXME: do this less stupidly, or not at all -->
					<xsl:choose>
						<xsl:when test="
								@format = 'PNG' and
								(substring(@fileref, string-length(@fileref) - 3)
									!= '.png')">
							<xsl:value-of select="concat(@fileref, '.png')"/>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="@fileref"/>
						</xsl:otherwise>
					</xsl:choose>
				</xsl:when>
				<xsl:when test="@entityref">
					<xsl:value-of select="unparsed-entity-uri(@entityref)"/>
				</xsl:when>
			</xsl:choose>
		</xsl:attribute>
		<xsl:choose>
			<xsl:when test="@scale">
				<xsl:attribute name="width">
					<xsl:value-of select="concat(@scale, '%')"/>
				</xsl:attribute>
			</xsl:when>
			<xsl:when test="@width">
				<xsl:attribute name="width">
					<xsl:value-of select="@width"/>
				</xsl:attribute>
				<xsl:if test="@height">
					<xsl:attribute name="height">
						<xsl:value-of select="@height"/>
					</xsl:attribute>
				</xsl:if>
			</xsl:when>
		</xsl:choose>
		<xsl:if test="@align">
			<xsl:attribute name="align">
				<xsl:value-of select="@align"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:if test="$textobject/phrase">
			<xsl:attribute name="alt">
				<xsl:value-of select="phrase[1]"/>
			</xsl:attribute>
		</xsl:if>
	</xsl:element>
</xsl:template>

<xsl:template name="mediaobject">
	<xsl:choose>
		<xsl:when test="$text_only">
			<xsl:apply-templates select="textobject[1]"/>
		</xsl:when>
		<xsl:when test="imageobject[imagedata/@format = 'PNG']">
			<xsl:apply-templates
					select="imageobject[imagedata/@format = 'PNG'][1]">
				<xsl:with-param name="textobject" select="textobject[1]"/>
			</xsl:apply-templates>
		</xsl:when>
		<xsl:when test="imageobjectco[imageobject/imagedata/@format = 'PNG']">
			<xsl:apply-templates
					select="imageobjectco[imageobject/imagedata/@format = 'PNG'][1]">
				<xsl:with-param name="textobject" select="textobject[1]"/>
			</xsl:apply-templates>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select="(imageobject | imageobjectco)[1]">
				<xsl:with-param name="textobject" select="textobject[1]"/>
			</xsl:apply-templates>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template match="graphic">
	<xsl:param name="textobject" select="false()"/>
	<xsl:call-template name="mediadata">
		<xsl:with-param name="textobject" select="textobject"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="graphicco">
	<xsl:param name="textobject" select="false()"/>
	<xsl:call-template name="anchor"/>
	<xsl:apply-templates select="graphic">
		<xsl:with-param name="textobject" select="$textobject"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="calloutlist"/>
</xsl:template>

<xsl:template match="audiodata">
	<!-- FIXME -->
</xsl:template>

<xsl:template match="audioobject">
	<!-- FIXME -->
</xsl:template>

<xsl:template match="imagedata">
	<xsl:param name="textobject" select="false()"/>
	<xsl:call-template name="mediadata">
		<xsl:with-param name="textobject" select="textobject"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="imageobject">
	<xsl:param name="textobject" select="false()"/>
	<xsl:apply-templates select="imagedata">
		<xsl:with-param name="textobject" select="$textobject"/>
	</xsl:apply-templates>
</xsl:template>

<xsl:template match="imageobjectco">
	<xsl:param name="textobject" select="false()"/>
	<xsl:call-template name="anchor"/>
	<xsl:apply-templates select="imageobject">
		<xsl:with-param name="textobject" select="$textobject"/>
	</xsl:apply-templates>
	<xsl:apply-templates select="calloutlist"/>
</xsl:template>

<xsl:template match="inlinegraphic">
	<xsl:param name="textobject" select="false()"/>
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="mediadata">
			<xsl:with-param name="textobject" select="textobject"/>
		</xsl:call-template>
	</span>
</xsl:template>

<xsl:template match="inlinemediaobject">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="mediaobject"/>
	</span>
</xsl:template>

<xsl:template match="mediaobject">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="mediaobject"/>
		<xsl:apply-templates select="caption"/>
	</div>
</xsl:template>

<xsl:template match="mediaobjectco">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="mediaobject"/>
	</div>
</xsl:template>

<xsl:template match="screenshot">
	<xsl:call-template name="block"/>
</xsl:template>

<xsl:template match="textdata">
	<!-- FIXME -->
</xsl:template>

<xsl:template match="textobject">
	<!-- FIXME -->
</xsl:template>

<xsl:template match="videodata">
	<!-- FIXME -->
</xsl:template>

<xsl:template match="videoobject">
	<!-- FIXME -->
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
