<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template name="html">
	<xsl:param name="node" select="."/>
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>

	<xsl:variable name="top">
		<xsl:call-template name="navbar.top">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="prev">
		<xsl:call-template name="navbar.prev">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="next">
		<xsl:call-template name="navbar.next">
			<xsl:with-param name="node" select="$node"/>
			<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
		</xsl:call-template>
	</xsl:variable>

	<html>
		<head>
			<title>
				<xsl:value-of select="title"/>
			</title>
			<style type="text/css">
				<xsl:call-template name="html.css">
					<xsl:with-param name="node" select="$node"/>
				</xsl:call-template>
			</style>
			<xsl:if test="$prev != ''">
				<link rel="Previous">
					<xsl:attribute name="href">
						<xsl:call-template name="xref.target">
							<xsl:with-param name="linkend" select="$prev"/>
							<xsl:with-param name="target" select="/*"/>
						</xsl:call-template>
					</xsl:attribute>
				</link>
			</xsl:if>
			<xsl:if test="$next != ''">
				<link rel="Next">
					<xsl:attribute name="href">
						<xsl:call-template name="xref.target">
							<xsl:with-param name="linkend" select="$next"/>
							<xsl:with-param name="target" select="/*"/>
						</xsl:call-template>
					</xsl:attribute>
				</link>
			</xsl:if>
			<xsl:if test="$top != ''">
				<link rel="Top">
					<xsl:attribute name="href">
						<xsl:call-template name="xref.target">
							<xsl:with-param name="linkend" select="$top"/>
							<xsl:with-param name="target" select="/*"/>
						</xsl:call-template>
					</xsl:attribute>
				</link>
			</xsl:if>
			<xsl:call-template name="html.head">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</head>
		<body>
			<xsl:call-template name="html.body.attributes">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
			<xsl:call-template name="html.body.top">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
			<xsl:call-template name="html.navbar.top">
				<xsl:with-param name="node" select="$node"/>
				<xsl:with-param name="top"  select="$top"/>
				<xsl:with-param name="prev" select="$prev"/>
				<xsl:with-param name="next" select="$next"/>
			</xsl:call-template>

			<xsl:apply-templates select="$node">
				<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
			</xsl:apply-templates>

			<xsl:call-template name="html.navbar.bottom">
				<xsl:with-param name="node" select="$node"/>
				<xsl:with-param name="top"  select="$top"/>
				<xsl:with-param name="prev" select="$prev"/>
				<xsl:with-param name="next" select="$next"/>
			</xsl:call-template>
			<xsl:call-template name="html.body.bottom">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</body>
	</html>
</xsl:template>

<xsl:template name="html.css"><xsl:text>
<!--
body {
	margin-right: 1em;
}

h1 {
	font-size: 1.6em;
	font-weight: bold;
	margin-bottom: 0em;
	padding-bottom: 0em;
}
h1 + * {
	margin-top: 0.2em;
	padding-top: 0em;
}
h2 {
	font-size: 1.4em;
	font-weight: bold;
	margin-top: 1.2em;
	margin-bottom: 0em;
	padding-bottom: 0em;
}
h2 + * {
	margin-top: 0.2em;
	padding-top: 0em;
}
h3 {
	font-size: 1.2em;
	font-weight: bold;
	margin-bottom: 0em;
	padding-bottom: 0em;
}
h3 + * {
	margin-top: 0.2em;
	padding-top: 0em;
}
h4 {
	margin-bottom: 0em;
	padding-bottom: 0em;
}
h4 + * {
	margin-top: 0.2em;
	padding-top: 0em;
}

div[class="heading"] {
	margin-bottom: 0.5em;
}

div[class="admonition"] {
	border-top: outset 1px;
	border-bottom: outset 1px;
	margin-left: 2em;
	margin-right: 2em;
	margin-bottom: 1em;
}

div[class="informalexample"] {
	margin-left: 2em;
	margin-right: 1em;
	margin-bottom: 1em;
}
div[class="informalfigure"] {
	margin-left: 2em;
	margin-right: 1em;
	margin-bottom: 1em;
}
div[class="informaltable"] {
	margin-left: 2em;
	margin-right: 1em;
	margin-bottom: 1em;
}
div[class="example"] {
	margin-left: 2em;
	margin-right: 1em;
	margin-bottom: 1em;
}
div[class="figure"] {
	margin-left: 2em;
	margin-right: 1em;
	margin-bottom: 1em;
}
div[class="table"] {
	margin-left: 2em;
	margin-right: 1em;
	margin-bottom: 1em;
}

div[class="sidebar"] {
	border: outset 1px;
	margin-left: 2em;
	margin-right: 2em;
}

ul {
	list-style-image: url("li.png")
}

<!--
table {
	border: outset 1px;
}
-->
thead th {
	border-bottom: solid 1px;
}
tfoot th {
	border-top: solid 1px;
}
tr[class="even"] {
	background-color: #EEEEEE;
}
tr &gt; * {
	text-align: left;
	padding: 0.2em 0.5em 0.2em 0.5em;
}
td {
	vertical-align: top;
}
th {
	vertical-align: bottom;
}

div[class="attribution"] {
	text-align: right;
}
-->
</xsl:text></xsl:template>

<xsl:template name="html.head">
	<xsl:param name="node" select="."/>
</xsl:template>

<xsl:template name="html.body.attributes">
	<xsl:param name="node" select="."/>
</xsl:template>

<xsl:template name="html.body.top">
	<xsl:param name="node" select="."/>
</xsl:template>

<xsl:template name="html.body.bottom">
	<xsl:param name="node" select="."/>
</xsl:template>

<xsl:template name="html.navbar.top">
	<xsl:param name="node" select="."/>
	<xsl:param name="top">
		<xsl:call-template name="navbar.top">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:param name="prev">
		<xsl:call-template name="navbar.prev">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:param name="next">
		<xsl:call-template name="navbar.next">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<table width="100%" cellpadding="0" cellspacing="0">
		<tr width="100%">
			<td width="50%" style="text-align: left;">
				<xsl:call-template name="xref">
					<xsl:with-param name="linkend" select="$prev"/>
				</xsl:call-template>
			</td>
			<td width="50%" style="text-align: right;">
				<xsl:call-template name="xref">
					<xsl:with-param name="linkend" select="$next"/>
				</xsl:call-template>
			</td>
		</tr>
	</table>
	<hr/>
</xsl:template>

<xsl:template name="html.navbar.bottom">
	<xsl:param name="node" select="."/>
	<xsl:param name="top">
		<xsl:call-template name="navbar.top">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:param name="prev">
		<xsl:call-template name="navbar.prev">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<xsl:param name="next">
		<xsl:call-template name="navbar.next">
			<xsl:with-param name="node" select="$node"/>
		</xsl:call-template>
	</xsl:param>
	<hr/>
	<table width="100%" cellpadding="0" cellspacing="0">
		<tr width="100%">
			<td width="40%" style="text-align: left;">
				<xsl:call-template name="xref">
					<xsl:with-param name="linkend" select="$prev"/>
				</xsl:call-template>
			</td>
			<td width="20%" style="text-align: center;">
				<xsl:if test="$top != ''">
					<a>
						<xsl:attribute name="href">
							<xsl:call-template name="xref.target">
								<xsl:with-param name="linkend" select="$top"/>
								<xsl:with-param name="target" select="/*"/>
							</xsl:call-template>
						</xsl:attribute>
						<xsl:call-template name="gettext">
							<xsl:with-param name="key" select="'Contents'"/>
						</xsl:call-template>
					</a>
				</xsl:if>
			</td>
			<td width="40%" style="text-align: right;">
				<xsl:call-template name="xref">
					<xsl:with-param name="linkend" select="$next"/>
				</xsl:call-template>
			</td>
		</tr>
	</table>
</xsl:template>

</xsl:stylesheet>
