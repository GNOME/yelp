<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template match="
		appendixinfo | articleinfo  | bibliographyinfo | bookinfo       |
		chapterinfo  | glossaryinfo | indexinfo        | partinfo       |
		prefaceinfo  | refentryinfo | referenceinfo    | refsect1info   |
		refsect2info | refsect3info | refsectioninfo   | sect1info      |
		sect2info    | sect3info    | sect4info        | sect5info      |
		sectioninfo  | setinfo      | setindexinfo     ">
	<xsl:param name="depth_chunk">
		<xsl:call-template name="depth.chunk"/>
	</xsl:param>
	<xsl:param name="depth_in_chunk">
		<xsl:call-template name="depth.in.chunk"/>
	</xsl:param>

	<xsl:variable name="authors"
		select="author|authorgroup/author|corpauthor|authorgroup/corpauthor"/>
	<xsl:variable name="editors" select="editor|authorgroup/editor"/>
	<xsl:variable name="collabs" select="collab|authorgroup/collab"/>
	<xsl:variable name="credits"
		select="contrib|othercredit|authorgroup/othercredit"/>

	<div class="titlepage">
		<xsl:call-template name="anchor">
			<xsl:with-param name="name" select="'titlepage'"/>
		</xsl:call-template>
		<xsl:if test="not(title) and ../title">
			<xsl:apply-templates select="../title" mode="titlepage.mode"/>
		</xsl:if>
		<xsl:if test="not(subtitle) and ../subtitle">
			<xsl:apply-templates select="../subtitle" mode="titlepage.mode"/>
		</xsl:if>
		<xsl:if test="$authors">
			<div>
				<h2 class="author">
					<xsl:call-template name="ngettext">
						<xsl:with-param name="msgid" select="'Author'"/>
						<xsl:with-param name="msgid_plural" select="'Authors'"/>
						<xsl:with-param name="num" select="count($authors)"/>
					</xsl:call-template>
				</h2>
				<dl>
					<xsl:apply-templates select="$authors" mode="titlepage.mode"/>
				</dl>
			</div>
		</xsl:if>
		<xsl:if test="$editors">
			<div>
				<h2 class="editor">
					<xsl:call-template name="ngettext">
						<xsl:with-param name="msgid" select="'Editor'"/>
						<xsl:with-param name="msgid_plural" select="'Editors'"/>
						<xsl:with-param name="num" select="count($editors)"/>
					</xsl:call-template>
				</h2>
				<dl>
					<xsl:apply-templates select="$editors" mode="titlepage.mode"/>
				</dl>
			</div>
		</xsl:if>
		<xsl:if test="$collabs">
			<div>
				<h2 class="collab">
					<xsl:call-template name="ngettext">
						<xsl:with-param name="msgid" select="'Collaborator'"/>
						<xsl:with-param name="msgid_plural" select="'Collaborators'"/>
						<xsl:with-param name="num" select="count($collabs)"/>
					</xsl:call-template>
				</h2>
				<dl>
					<xsl:apply-templates select="$collabs" mode="titlepage.mode"/>
				</dl>
			</div>
		</xsl:if>
		<xsl:if test="$credits">
			<div>
				<h2 class="othercredit">
					<xsl:call-template name="ngettext">
						<xsl:with-param name="msgid" select="'Other Contributor'"/>
						<xsl:with-param name="msgid_plural" select="'Other Contributors'"/>
						<xsl:with-param name="num" select="count($credits)"/>
					</xsl:call-template>
				</h2>
				<dl>
					<xsl:apply-templates select="$credits" mode="titlepage.mode"/>
				</dl>
			</div>
		</xsl:if>
		<xsl:if test="publisher">
			<div>
				<h2 class="publisher">
					<xsl:call-template name="ngettext">
						<xsl:with-param name="msgid" select="'Publisher'"/>
						<xsl:with-param name="msgid_plural" select="'Publishers'"/>
						<xsl:with-param name="num" select="count(publisher)"/>
					</xsl:call-template>
				</h2>
				<dl>
					<xsl:apply-templates select="publisher" mode="titlepage.mode"/>
				</dl>
			</div>
		</xsl:if>
		<xsl:if test="copyright">
			<div>
				<h2 class="copyright">
					<xsl:call-template name="ngettext">
						<xsl:with-param name="msgid" select="'Copyright'"/>
						<xsl:with-param name="msgid_plural" select="'Copyrights'"/>
						<xsl:with-param name="num" select="count(copyright)"/>
					</xsl:call-template>
				</h2>
				<dl>
					<xsl:apply-templates select="copyright" mode="titlepage.mode"/>
				</dl>
			</div>
		</xsl:if>
		<xsl:apply-templates select="legalnotice" mode="titlepage.mode"/>
		<xsl:apply-templates select="releaseinfo" mode="titlepage.mode"/>
		<xsl:apply-templates select="revhistory" mode="titlepage.mode"/>
	</div>
</xsl:template>

<xsl:template match="*" mode="titlepage.mode">
	<xsl:apply-templates select="."/>
</xsl:template>

<!--
<xsl:template match="abstract" mode="titlepage.mode">
	<div class="{name(.)}">
		<xsl:call-template name="formal.object.heading">
			<xsl:with-param name="title">
				<xsl:apply-templates select="." mode="title.markup"/>
			</xsl:with-param>
		</xsl:call-template>
		<xsl:apply-templates mode="titlepage.mode"/>
	</div>
</xsl:template>

<xsl:template match="abstract/title" mode="titlepage.mode">
</xsl:template>
-->

<xsl:template match="authorinitials" mode="titlepage.mode">
	<span class="{name(.)}">
		<xsl:apply-templates mode="titlepage.mode"/>
	</span>
</xsl:template>

<xsl:template match="author" mode="titlepage.mode">
	<dt><xsl:call-template name="person.name"/></dt>
	<xsl:for-each select="affiliation/orgname">
		<dd>
			<i>
				<xsl:call-template name="gettext">
					<xsl:with-param name="msgid" select="'Affiliation'"/>
				</xsl:call-template>
				<xsl:text>: </xsl:text>
			</i>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</dd>
	</xsl:for-each>
	<xsl:for-each select="email">
		<dd>
			<strong>
				<xsl:call-template name="gettext">
					<xsl:with-param name="msgid" select="'Email'"/>
				</xsl:call-template>
				<xsl:text>: </xsl:text>
			</strong>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</dd>
	</xsl:for-each>
	<xsl:apply-templates select="authorblurb | personblurb"/>
</xsl:template>

<xsl:template match="authorblurb" mode="titlepage.mode">
	<dd><xsl:apply-templates mode="titlepage.mode"/></dd>
</xsl:template>

<xsl:template match="collab" mode="titlepage.mode">
	<dt>
		<xsl:apply-templates select="collabname" mode="titlepage.mode"/>
	</dt>
	<xsl:for-each select="affiliation/orgname">
		<dd>
			<strong>
				<xsl:call-template name="gettext">
					<xsl:with-param name="msgid" select="'Affiliation'"/>
				</xsl:call-template>
				<xsl:text>: </xsl:text>
			</strong>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</dd>
	</xsl:for-each>
</xsl:template>

<xsl:template match="collabname" mode="titlepage.mode">
	<span class="{name(.)}">
		<xsl:apply-templates mode="titlepage.mode"/>
	</span>
</xsl:template>

<xsl:template match="contrib" mode="titlepage.mode">
	<dt><xsl:apply-templates mode="titlepage.mode"/></dt>
</xsl:template>

<xsl:template match="copyright" mode="titlepage.mode">
	<dt>
		<xsl:call-template name="gettext">
			<xsl:with-param name="msgid" select="'Copyright'"/>
		</xsl:call-template>
		<xsl:text>&#x00A0;&#x00A9;&#x00A0;</xsl:text>
		<xsl:for-each select="year">
			<xsl:if test="position() &gt; 1">
				<xsl:text>, </xsl:text>
			</xsl:if>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</xsl:for-each>
		<xsl:text>&#160;&#160;</xsl:text>
		<xsl:for-each select="holder">
			<xsl:if test="position() &gt; 1">
				<xsl:text>, </xsl:text>
			</xsl:if>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</xsl:for-each>
	</dt>
</xsl:template>

<xsl:template match="corpauthor" mode="titlepage.mode">
	<dt><xsl:apply-templates mode="titlepage.mode"/></dt>
</xsl:template>

<xsl:template match="editor" mode="titlepage.mode">
	<dt><xsl:call-template name="person.name"/></dt>
	<xsl:for-each select="email">
		<dd>
			<strong>
				<xsl:call-template name="gettext">
					<xsl:with-param name="msgid" select="'Email'"/>
				</xsl:call-template>
				<xsl:text>: </xsl:text>
			</strong>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</dd>
	</xsl:for-each>
	<xsl:apply-templates select="personblurb"/>
</xsl:template>

<xsl:template match="holder" mode="titlepage.mode">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="legalnotice" mode="titlepage.mode">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:choose>
			<xsl:when test="title">
				<xsl:apply-templates select="title" mode="titlepage.mode"/>
			</xsl:when>
			<xsl:otherwise>
				<h2 class="{name(.)}">
					<xsl:call-template name="gettext">
						<xsl:with-param name="msgid" select="'Legal Notice'"/>
					</xsl:call-template>
				</h2>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:apply-templates mode="titlepage.mode" select="
			beginpage    | blockquote     | calloutlist      | caution     |
			formalpara   | glosslist      | important        | indexterm   |
			itemizedlist | literallayout  | note             | orderedlist |
			para         | programlisting | programlistingco | screen      |
			screenco     | screenshot     | segmentedlist    | simpara     |
			simplelist   | tip            | variablelist     | warning     "/>
	</div>
</xsl:template>

<xsl:template match="legalnotice/title" mode="titlepage.mode">
	<h2 class="legalnotice">
		<xsl:apply-templates/>
	</h2>
</xsl:template>

<xsl:template match="othercredit" mode="titlepage.mode">
	<dt><xsl:call-template name="person.name"/></dt>
	<dt><xsl:call-template name="person.name"/></dt>
	<xsl:for-each select="email">
		<dd>
			<strong>
				<xsl:call-template name="gettext">
					<xsl:with-param name="msgid" select="'Email'"/>
				</xsl:call-template>
				<xsl:text>: </xsl:text>
			</strong>
			<xsl:apply-templates select="." mode="titlepage.mode"/>
		</dd>
	</xsl:for-each>
	<xsl:apply-templates select="personblurb"/>
</xsl:template>

<xsl:template match="personblurb" mode="titlepage.mode">
	<dd><xsl:apply-templates mode="titlepage.mode"/></dd>
</xsl:template>

<xsl:template match="publisher" mode="titlepage.mode">
	<dt><xsl:apply-templates mode="titlepage.mode"/></dt>
</xsl:template>

<xsl:template match="publishername" mode="titlepage.mode">
	<span class="{name(.)}">
		<xsl:apply-templates mode="titlepage.mode"/>
	</span>
</xsl:template>

<xsl:template match="releaseinfo" mode="titlepage.mode">
	<div class="{name(.)}">
		<h2 class="{name(.)}">
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'Release Information'"/>
			</xsl:call-template>
		</h2>
		<p><xsl:apply-templates/></p>
	</div>
</xsl:template>

<xsl:template match="revhistory" mode="titlepage.mode">
	<div class="{name(.)}">
		<h2 class="{name(.)}">
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'Revision History'"/>
			</xsl:call-template>
		</h2>
		<table class="{name(.)}">
			<xsl:attribute name="summary">
				<xsl:call-template name="gettext">
					<xsl:with-param name="msgid" select="'Revision History'"/>
				</xsl:call-template>
			</xsl:attribute>
			<thead><tr>
				<th>
					<xsl:call-template name="gettext">
						<xsl:with-param name="msgid" select="'Title'"/>
					</xsl:call-template>
				</th>
				<th>
					<xsl:call-template name="gettext">
						<xsl:with-param name="msgid" select="'Date'"/>
					</xsl:call-template>
				</th>
				<th>
					<xsl:call-template name="gettext">
						<xsl:with-param name="msgid" select="'Author'"/>
					</xsl:call-template>
				</th>
				<th>
					<xsl:call-template name="gettext">
						<xsl:with-param name="msgid" select="'Publisher'"/>
					</xsl:call-template>
				</th>
			</tr></thead>
			<tbody>
				<xsl:apply-templates mode="titlepage.mode"/>
			</tbody>
		</table>
	</div>
</xsl:template>

<xsl:template match="revision" mode="titlepage.mode">
	<tr>
		<xsl:attribute name="class">
			<xsl:choose>
				<xsl:when test="count(preceding-sibling::revision) mod 2">
					<xsl:text>odd</xsl:text>
				</xsl:when>
				<xsl:otherwise>	
					<xsl:text>even</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:attribute>
		<td><xsl:apply-templates mode="titlepage.mode" select="revnumber"/></td>
		<td><xsl:apply-templates mode="titlepage.mode" select="date"/></td>
		<td><xsl:apply-templates mode="titlepage.mode" select="
				revremark/para[@role='author']      |
				revdescription/para[@role='author'] |
				authorinitials                      "/></td>
		<td><xsl:apply-templates mode="titlepage.mode" select="
				revremark/para[@role='publisher']      |
				revdescription/para[@role='publisher'] "/></td>
	</tr>
</xsl:template>

<xsl:template match="revnumber" mode="titlepage.mode">
	<span class="{name(.)}">
		<xsl:apply-templates mode="titlepage.mode"/>
	</span>
</xsl:template>

<xsl:template match="subtitle" mode="titlepage.mode">
	<h2 class="{name(.)}">
		<xsl:apply-templates mode="titlepage.mode"/>
	</h2>
</xsl:template>

<xsl:template match="title" mode="titlepage.mode">
	<h1 class="titlepage">
		<xsl:apply-templates mode="titlepage.mode"/>
	</h1>
</xsl:template>

<xsl:template match="year" mode="titlepage.mode">
	<xsl:apply-templates mode="titlepage.mode"/>
</xsl:template>

</xsl:stylesheet>
