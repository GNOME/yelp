<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="inline">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</span>
</xsl:template>

<xsl:template name="inline.bold">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<b><xsl:apply-templates/></b>
	</span>
</xsl:template>

<xsl:template name="inline.bold.italic">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<b><i><xsl:apply-templates/></i></b>
	</span>
</xsl:template>

<xsl:template name="inline.bold.mono">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<b><tt><xsl:apply-templates/></tt></b>
	</span>
</xsl:template>

<xsl:template name="inline.italic">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<i><xsl:apply-templates/></i>
	</span>
</xsl:template>

<xsl:template name="inline.italic.mono">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<i><tt><xsl:apply-templates/></tt></i>
	</span>
</xsl:template>

<xsl:template name="inline.mono">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<tt><xsl:apply-templates/></tt>
	</span>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template match="abbrev">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="accel">
	<u><xsl:call-template name="inline"/></u>
</xsl:template>

<xsl:template match="acronym">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="action">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="address">
	<span class="{name(.)}" style="white-space: pre;">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</span>
</xsl:template>

<xsl:template match="application">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<xsl:template match="author">
	<span class="{name(.)}"><xsl:call-template name="person.name"/></span>
</xsl:template>

<xsl:template match="authorinitials">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="city">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="citation">
	<xsl:text>[</xsl:text>
	<xsl:call-template name="inline"/>
	<xsl:text>]</xsl:text>
</xsl:template>

<xsl:template match="citerefentry">
	<a>
		<xsl:attribute name="href">
			<xsl:value-of select="concat('man:', refentrytitle)"/>
		</xsl:attribute>
		<xsl:apply-templates select="refentrytitle/node()"/>
		<xsl:if test="manvolnum">
			<xsl:text>(</xsl:text>
			<xsl:apply-templates select="manvolnum/node()"/>
			<xsl:text>)</xsl:text>
		</xsl:if>
	</a>
</xsl:template>

<xsl:template match="citetitle">
	<xsl:choose>
		<xsl:when test="@pubwork = 'article'">
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'&#8220;'"/>
			</xsl:call-template>
			<xsl:call-template name="inline"/>
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'&#8221;'"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="inline.italic"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="classname">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="command">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="computeroutput">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="constant">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="corpauthor">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="country">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="database">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="date">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="editor">
	<span class="{name(.)}"><xsl:call-template name="person.name"/></span>
</xsl:template>

<xsl:template match="email">
	<span class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<tt>
			<xsl:text>&lt;</xsl:text>
			<a>
				<xsl:attribute name="href">
					<xsl:text>mailto:</xsl:text>
					<xsl:value-of select="."/>
				</xsl:attribute>
				<xsl:apply-templates/>
			</a>
			<xsl:text>&gt;</xsl:text>
		</tt>
	</span>
</xsl:template>

<xsl:template match="emphasis">
	<xsl:choose>
		<xsl:when test="(@role = 'bold') or (@role = 'strong')">
			<xsl:call-template name="inline.bold"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="inline.italic"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="envar">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="errorcode">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="errorname">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="errortext">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="errortype">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="exceptionname">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="fax">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="filename">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="firstterm">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<xsl:template match="foreignphrase">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<xsl:template match="function">
	<xsl:choose>
		<xsl:when test="function or parameter or replaceable">
			<span class="{name(.)}">
				<xsl:call-template name="anchor"/>
				<tt><xsl:apply-templates select="(* | text())[1]"/></tt>
				<xsl:for-each select="(* | text())[position() != 1]">
					<xsl:apply-templates select="."/>
					<xsl:if test="
							(self::parameter or self::replaceable) and
							(following-sibling::*)">
						<xsl:text>, </xsl:text>
					</xsl:if>
				</xsl:for-each>
			</span>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="inline.mono"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="guibutton">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="guiicon">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="guilabel">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="guimenu">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="guimenuitem">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="guisubmenu">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="hardware">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="interface">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="interfacename">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="jobtitle">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="keycap">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="keycode">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="keycombo">
	<xsl:variable name="joinchar">
		<xsl:choose>
			<xsl:when test="@action = 'seq'"><xsl:text> </xsl:text></xsl:when>
			<xsl:when test="@action = 'simul'">+</xsl:when>
			<xsl:when test="@action = 'press'">-</xsl:when>
			<xsl:when test="@action = 'click'">-</xsl:when>
			<xsl:when test="@action = 'double-click'">-</xsl:when>
			<xsl:when test="@action = 'other'"></xsl:when>
			<xsl:otherwise>+</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:for-each select="*">
		<xsl:if test="position()>1"><xsl:value-of select="$joinchar"/></xsl:if>
		<xsl:apply-templates select="."/>
	</xsl:for-each>
</xsl:template>

<xsl:template match="keysym">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="lineannotation">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<xsl:template match="literal">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="markup">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="medialabel">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<xsl:template match="menuchoice">
	<span class="{name(.)}">
		<xsl:for-each select="*[name(.) != 'shortcut']">
			<xsl:if test="position() &gt; 1">
				<xsl:text>&#160;&#x2192;&#160;</xsl:text>
			</xsl:if>
			<xsl:apply-templates select="."/>
		</xsl:for-each>
		<xsl:if test="shortcut">
			<xsl:text> (</xsl:text>
			<xsl:apply-templates select="shortcut"/>
			<xsl:text>)</xsl:text>
		</xsl:if>
	</span>
</xsl:template>

<xsl:template match="methodname">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="mousebutton">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="msgmain">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="msgrel">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="msgrel/title">
	<b><xsl:apply-templates/></b>
</xsl:template>

<xsl:template match="msgset">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="msgsub">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="msgtext">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="option">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="optional">
	<xsl:text>[</xsl:text>
	<xsl:call-template name="inline"/>
	<xsl:text>]</xsl:text>
</xsl:template>

<xsl:template match="orgdiv">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="orgname">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="otheraddr">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="othercredit">
	<span class="{name(.)}"><xsl:call-template name="person.name"/></span>
</xsl:template>

<xsl:template match="parameter">
	<xsl:call-template name="inline.italic.mono"/>
</xsl:template>

<xsl:template match="personname">
	<xsl:call-template name="person.name"/>
</xsl:template>

<xsl:template match="phone">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="phrase">
	<span>
		<xsl:call-template name="anchor"/>
		<xsl:if test="@role">
			<xsl:attribute name="class">
				<xsl:value-of select="@role"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:apply-templates/>
	</span>
</xsl:template>

<xsl:template match="pob">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="postcode">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="productname">
	<xsl:call-template name="inline"/>
	<xsl:if test="@class">
		<xsl:call-template name="dingbat">
			<xsl:with-param name="dingbat" select="@class"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

<xsl:template match="productnumber">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="prompt">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="property">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="quote">
	<xsl:choose>
		<xsl:when test="(count(ancestor::quote) mod 2) = 0">
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'&#8220;'"/>
			</xsl:call-template>
			<xsl:apply-templates/>
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'&#8221;'"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'&#8216;'"/>
			</xsl:call-template>
			<xsl:apply-templates/>
			<xsl:call-template name="gettext">
				<xsl:with-param name="msgid" select="'&#8217;'"/>
			</xsl:call-template>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="replaceable">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<xsl:template match="returnvalue">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="sgmltag">
	<xsl:variable name="class">
		<xsl:choose>
			<xsl:when test="@class">
				<xsl:value-of select="@class"/>
			</xsl:when>
			<xsl:otherwise>element</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<tt class="sgmltag-{$class}">
		<xsl:choose>
			<xsl:when test="$class = 'attribute'">
				<xsl:apply-templates/>
			</xsl:when>
			<xsl:when test="$class = 'attvalue'">
				<xsl:apply-templates/>
			</xsl:when>
			<xsl:when test="$class = 'element'">
				<xsl:apply-templates/>
			</xsl:when>
			<xsl:when test="$class = 'emptytag'">
				<xsl:text>&lt;</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>/&gt;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'endtag'">
				<xsl:text>&lt;/</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>&gt;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'genentity'">
				<xsl:text>&amp;</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'numcharref'">
				<xsl:text>&amp;#</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'paramentity'">
				<xsl:text>%</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'pi'">
				<xsl:text>&lt;?</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>&gt;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'sgmlcomment'">
				<xsl:text>&lt;!--</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>--&gt;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'starttag'">
				<xsl:text>&lt;</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>&gt;</xsl:text>
			</xsl:when>
			<xsl:when test="$class = 'xmlpi'">
				<xsl:text>&lt;?</xsl:text>
				<xsl:apply-templates/>
				<xsl:text>?&gt;</xsl:text>
			</xsl:when>
		</xsl:choose>
	</tt>
</xsl:template>

<xsl:template match="shortcut">
	<xsl:call-template name="inline.bold"/>
</xsl:template>

<xsl:template match="state">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="street">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="structfield">
	<xsl:call-template name="inline.italic.mono"/>
</xsl:template>

<xsl:template match="structname">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="subscript">
	<sub>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</sub>
</xsl:template>

<xsl:template match="superscript">
	<sup>
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</sup>
</xsl:template>

<xsl:template match="symbol">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="systemitem">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="token">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="trademark">
	<xsl:variable name="class">
		<xsl:choose>
			<xsl:when test="@class">
				<xsl:value-of select="@class"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="'trade'"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:call-template name="dingbat">
		<xsl:with-param name="dingbat" select="$class"/>
	</xsl:call-template>
</xsl:template>

<xsl:template match="type">
	<xsl:call-template name="inline"/>
</xsl:template>

<xsl:template match="userinput">
	<xsl:call-template name="inline.bold.mono"/>
</xsl:template>

<xsl:template match="varname">
	<xsl:call-template name="inline.mono"/>
</xsl:template>

<xsl:template match="wordasword">
	<xsl:call-template name="inline.italic"/>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
