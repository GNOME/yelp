<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="block.simple" match="
		abstract   | attribution     | authorblurb    | caption       |
		dedication | example         | figure         | formalpara    |
		highlights | informalexample | informalfigure | informaltable |
		msg        | msgentry        | msgexplan      | msginfo       |
		partintro  | simplemsgentry  | sidebar        | table         ">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template name="block.blockquote" match="blockquote | epigraph">
	<div class="{name(.)}">
		<xsl:apply-templates select="title"/>
		<blockquote>
			<xsl:apply-templates
				select="*[name(.) != 'title' and name(.) != 'attribution']"/>
		</blockquote>
		<xsl:apply-templates select="attribution"/>
	</div>
</xsl:template>

<xsl:template name="block.pre" match="
		literallayout | programlisting | screen | synopsis">
	<pre class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</pre>
</xsl:template>

<xsl:template name="block.header" match="msgaud | msglevel | msgorig">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:call-template name="node.heading"/>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template match="para">
	<p>
		<xsl:call-template name="anchor"/>
		<xsl:if test="@role">
			<xsl:attribute name="class">
				<xsl:value-of select="@role"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</p>
</xsl:template>

<xsl:template match="simpara">
	<p>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</p>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
