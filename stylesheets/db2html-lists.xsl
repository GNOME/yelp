<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY listcomponent "
	abstract         | address          | anchor              | authorblurb        |
	beginpage        | blockquote       | bridgehead          | caution            |
	classsynopsis    | cmdsynopsis      | constructorsynopsis | destructorsynopsis |
	epigraph         | fieldsynopsis    | formalpara          | funcsynopsis       |
	graphic          | graphicco        | highlights          | important          |
	indexterm        | informalequation | informalexample     | informalfigure     |
	informaltable    | literallayout    | mediaobject         | mediaobjectco      |
	methodsynopsis   | note             | para                | programlisting     |
	programlistingco | remark           | screen              | screenco           |
	screenshot       | simpara          | synopsis            | tip                |
	warning          ">
]>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- == itemizedlist ======================================================= -->

<xsl:template match="itemizedlist">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:if test="title">
			<xsl:call-template name="node.heading"/>
		</xsl:if>
		<xsl:apply-templates select="&listcomponent;"/>
		<ul>
			<xsl:if test="@spacing='compact'">
				<xsl:attribute name="compact">
					<xsl:value-of select="@spacing"/>
				</xsl:attribute>
			</xsl:if>
			<xsl:if test="@mark">
				<xsl:attribute name="style">
					<xsl:text>list-style-type: </xsl:text>
					<xsl:choose>
						<xsl:when test="@mark = 'opencircle'">
							<xsl:text>circle</xsl:text>
						</xsl:when>
						<xsl:when test="@mark = 'bullet'">
							<xsl:text>disc</xsl:text>
						</xsl:when>
						<xsl:when test="@mark = 'box'">
							<xsl:text>square</xsl:text>
						</xsl:when>
						<xsl:otherwise>
							<xsl:value-of select="@mark"/>
						</xsl:otherwise>
					</xsl:choose>
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
					<xsl:when test="@override = 'opencircle'">
						<xsl:text>circle</xsl:text>
					</xsl:when>
					<xsl:when test="@override = 'bullet'">
						<xsl:text>disc</xsl:text>
					</xsl:when>
					<xsl:when test="@override = 'box'">
						<xsl:text>square</xsl:text>
					</xsl:when>
					<xsl:otherwise>
						<xsl:value-of select="@override"/>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:attribute>
		</xsl:if>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</li>
</xsl:template>

</xsl:stylesheet>
