<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template name="html">
	<xsl:param name="node" select="."/>
	<xsl:param name="leaf" select="true()"/>

	<html>
		<head>
			<title>
				<xsl:apply-templates select="$node" mode="title.text.mode"/>
			</title>
			<style type="text/css">
				<xsl:call-template name="html.css">
					<xsl:with-param name="node" select="$node"/>
				</xsl:call-template>
			</style>
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
			</xsl:call-template>

			<xsl:apply-templates select="$node">
				<xsl:with-param name="depth" select="0"/>
				<xsl:with-param name="leaf" select="$leaf"/>
			</xsl:apply-templates>

			<xsl:call-template name="html.navbar.bottom">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
			<xsl:call-template name="html.body.bottom">
				<xsl:with-param name="node" select="$node"/>
			</xsl:call-template>
		</body>
	</html>
</xsl:template>

<xsl:template name="html.css"><xsl:text>
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
<p><i><xsl:apply-templates select="$node" mode="navbar.prev.link.mode"/></i></p>
<p><i><xsl:apply-templates select="$node" mode="navbar.next.link.mode"/></i></p>
</xsl:template>

<xsl:template name="html.navbar.bottom">
	<xsl:param name="node" select="."/>
</xsl:template>

</xsl:stylesheet>
