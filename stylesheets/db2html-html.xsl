<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<!DOCTYPE xsl:stylesheet [
<!ENTITY is-info "(
	(name(.) = 'appendixinfo')			or (name(.) = 'articleinfo')    or
	(name(.) = 'bibliographyinfo')	or (name(.) = 'bookinfo')       or
	(name(.) = 'chapterinfo')			or (name(.) = 'glossaryinfo')   or
	(name(.) = 'indexinfo')				or (name(.) = 'partinfo')       or
	(name(.) = 'prefaceinfo')			or (name(.) = 'refentryinfo')   or
	(name(.) = 'referenceinfo')		or (name(.) = 'refsect1info')   or
	(name(.) = 'refsect2info')			or (name(.) = 'refsect3info')   or
	(name(.) = 'refsectioninfo')		or (name(.) = 'sect1info')      or
	(name(.) = 'sect2info')				or (name(.) = 'sect3info')      or
	(name(.) = 'sect4info')				or (name(.) = 'sect5info')      or
	(name(.) = 'sectioninfo')			or (name(.) = 'setinfo')        or
	(name(.) = 'setindexinfo'))">
]>

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

			<div class="body">
				<xsl:apply-templates select="$node">
					<xsl:with-param name="depth_chunk" select="$depth_chunk"/>
				</xsl:apply-templates>
			</div>

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
h1 { font-size: 1.6em; font-weight: bold; }
h2 { font-size: 1.4em; font-weight: bold; }
h3 { font-size: 1.2em; font-weight: bold; }

div[class="title"] + * { margin-top: 0.8em; }

body { margin: 0em; padding: 0em; }
div[class="body"] {
	margin-left: 0.8em;
	margin-right: 0.8em;
	margin-bottom: 1.6em;
}

p, div { margin: 0em; }
p + p, p + div, div + p, div + div { margin-top: 0.8em; }

dl { margin: 0em; }
ol { margin: 0em; }
ul { margin: 0em; }
ul li { padding-left: 0.4em; }
dd + dt { margin-top: 1.6em; }

div[class="navbar-top"],
div[class="navbar-bottom"],
{
	padding: 0.8em;
	background-color: </xsl:text>
<xsl:value-of select="$color_gray_background"/><xsl:text>;
	border: solid 1px </xsl:text>
<xsl:value-of select="$color_gray_border"/><xsl:text>;
}

div[class="example"],
div[class="figure"],
div[class="informalexample"],
div[class="informalfigure"],
div[class="informaltable"],
div[class="msgset"],
div[class="table"] {
	margin-top: 1.6em;
	margin-left: 1.6em;
	margin-right: 1.6em;
	margin-bottom: 0.8em;
}

div[class="variablelist"] {
	margin-top: 0.8em;
	margin-left: 1.6em;
	margin-right: 1.6em;
	margin-bottom: 0.8em;
}

div[class="sidebar"] {
	border: outset 1px;
	margin-left: 1.6em;
	margin-right: 1.6em;
}

div[class="admonition"] {
	margin-top: 1.6em;
	margin-left: 1.6em;
	margin-right: 1.6em;
	margin-bottom: 0.8em;
	padding: 0.8em;
	background-color: </xsl:text>
<xsl:value-of select="$color_gray_background"/><xsl:text>;
	border: solid 1px </xsl:text>
<xsl:value-of select="$color_gray_border"/><xsl:text>;
}
div[class="admonition"] td[class="image"] { margin-right: 1.6em; }

div[class="classsynopsis"],
div[class="literallayout"],
div[class="programlisting"],
div[class="screen"],
div[class="synopsis"] {
	margin-top: 1.6em;
	margin-left: 1.6em;
	margin-right: 1.6em;
	margin-bottom: 0.8em;
	padding: 0.8em;
	background-color: </xsl:text>
<xsl:value-of select="$color_gray_background"/><xsl:text>;
	border: solid 1px </xsl:text>
<xsl:value-of select="$color_gray_border"/><xsl:text>;
}

tt { font-family: monospace; }

<!--
table {
	border: outset 1px;
}
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
	<!--
	<div class="navbar-top">
		<table width="100%" cellpadding="0" cellspacing="0">
			<tr width="100%">
				<td width="50%" style="text-align: left;">
					<xsl:choose>
						<xsl:when test="$prev = 'titlepage'">
							<xsl:call-template name="xref">
								<xsl:with-param name="linkend" select="$prev"/>
								<xsl:with-param name="target" select="/*/*[&is-info;]"/>
							</xsl:call-template>
						</xsl:when>
						<xsl:otherwise>
							<xsl:call-template name="xref">
								<xsl:with-param name="linkend" select="$prev"/>
							</xsl:call-template>
						</xsl:otherwise>
					</xsl:choose>
				</td>
				<td width="50%" style="text-align: right;">
					<xsl:call-template name="xref">
						<xsl:with-param name="linkend" select="$next"/>
					</xsl:call-template>
				</td>
			</tr>
		</table>
	</div>
	-->
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
	<div class="navbar-bottom">
		<table width="100%" cellpadding="0" cellspacing="0">
			<tr width="100%">
				<td width="40%" style="text-align: left;">
					<xsl:choose>
						<xsl:when test="$prev = 'titlepage'">
							<xsl:call-template name="xref">
								<xsl:with-param name="linkend" select="$prev"/>
								<xsl:with-param name="target" select="/*/*[&is-info;]"/>
							</xsl:call-template>
						</xsl:when>
						<xsl:otherwise>
							<xsl:call-template name="xref">
								<xsl:with-param name="linkend" select="$prev"/>
							</xsl:call-template>
						</xsl:otherwise>
					</xsl:choose>
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
								<xsl:with-param name="msgid" select="'Contents'"/>
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
	</div>
</xsl:template>

</xsl:stylesheet>
