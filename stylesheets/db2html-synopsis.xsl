<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<!-- ======================================================================= -->

<xsl:template name="synopsis.br">
	<xsl:if test="
			parent::classsynopsis                  or
			following-sibling::constructorsynopsis or
			following-sibling::destructorsynopsis  or
			following-sibling::fieldsynopsis       or
			following-sibling::methodsynopsis      ">
		<br/>
	</xsl:if>
</xsl:template>

<xsl:variable name="tab_cpp">
	<xsl:text>&#160;&#160;&#160;&#160;</xsl:text>
</xsl:variable>
<xsl:variable name="tab_idl">
	<xsl:text>&#160;&#160;&#160;&#160;</xsl:text>
</xsl:variable>
<xsl:variable name="tab_java">
	<xsl:text>&#160;&#160;&#160;&#160;</xsl:text>
</xsl:variable>
<xsl:variable name="tab_python">
	<xsl:text>&#160;&#160;&#160;&#160;</xsl:text>
</xsl:variable>

<!-- == classsynopsis ====================================================== -->

<xsl:template match="classsynopsisinfo">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="
		classsynopsis  | constructorsynopsis | fieldsynopsis |
		methodsynopsis | destructorsynopsis  ">
	<xsl:param name="language">
		<xsl:choose>
			<xsl:when test="@language">
				<xsl:value-of select="@language"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$classsynopsis_default_language"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:param>

	<xsl:choose>
		<xsl:when test="$language = 'cpp'">
			<xsl:apply-templates select="." mode="classsynopsis.cpp"/>
		</xsl:when>
		<xsl:when test="$language = 'idl'">
			<xsl:apply-templates select="." mode="classsynopsis.idl"/>
		</xsl:when>
		<xsl:when test="$language = 'java'">
			<xsl:apply-templates select="." mode="classsynopsis.java"/>
		</xsl:when>
		<xsl:when test="$language = 'python'">
			<xsl:apply-templates select="." mode="classsynopsis.python"/>
		</xsl:when>
		<xsl:when test="$language = $classsynopsis_default_language">
			<!--
				If we are using the default language and we got this
				far, we should error out, rather than re-applying with
				the default language.  Otherwise we get an infinite loop.
			-->
			<xsl:message>
				<xsl:text>No information about the language '</xsl:text>
				<xsl:value-of select="$language"/>
				<xsl:text>' for classsynopsis.</xsl:text>
			</xsl:message>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates select=".">
				<xsl:with-param name="language"
					select="$classsynopsis_default_language"/>
			</xsl:apply-templates>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- == cmdsynopsis ======================================================== -->

<xsl:template match="cmdsynopsis">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates mode="cmdsynopsis.mode"/>
	</div>
</xsl:template>

<xsl:template match="arg | group">
	<xsl:variable name="sepchar">
		<xsl:choose>
			<xsl:when test="ancestor-or-self::*[@sepchar]">
				<xsl:value-of select="ancestor-or-self::*[@sepchar][1]/@sepchar"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text> </xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:value-of select="$sepchar"/>
	<xsl:choose>
		<xsl:when test="@choice = 'plain'">
			<xsl:text> </xsl:text>
		</xsl:when>
		<xsl:when test="@choice = 'req'">
			<xsl:text>{</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>[</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:choose>
		<xsl:when test="name(.) = 'group'">
			<xsl:for-each select="*">
				<xsl:if test="name(.) = 'arg' and position() != 1">
					<xsl:text> |</xsl:text>
				</xsl:if>
				<xsl:apply-templates select="."/>
			</xsl:for-each>
		</xsl:when>
		<xsl:otherwise>
			<xsl:apply-templates/>
		</xsl:otherwise>
	</xsl:choose>
	<xsl:if test="@rep = 'repeat'">
		<xsl:text>...</xsl:text>
	</xsl:if>
	<xsl:choose>
		<xsl:when test="@choice = 'plain'">
			<xsl:text> </xsl:text>
		</xsl:when>
		<xsl:when test="@choice = 'req'">
			<xsl:text>}</xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>]</xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template mode="cmdsynopsis.mode" match="arg | group">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="cmdsynopsis.mode" match="command">
	<div class="{name(.)}">
		<xsl:apply-templates select="."/>
	</div>
</xsl:template>

<xsl:template match="sbr">
	<br/>
</xsl:template>

<xsl:template mode="cmdsynopsis.mode" match="sbr">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template match="synopfragment">
	<div class="{name(.)}">
		<xsl:call-template name="anchor"/>
		<i><xsl:call-template name="header"/></i>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<xsl:template mode="cmdsynopsis.mode" match="synopfragment">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template match="synopfragmentref">
	<xsl:call-template name="xref">
		<xsl:with-param name="linkend" select="@linkend"/>
	</xsl:call-template>
</xsl:template>

<xsl:template mode="cmdsynopsis.mode" match="synopfragmentref">
	<xsl:apply-templates select="."/>
</xsl:template>

<!-- == funcsynopsis ======================================================= -->

<xsl:template match="funcsynopsisinfo">
	<pre class="{name(.)}"><xsl:apply-templates/></pre>
</xsl:template>

<xsl:template match="funcdef">
	<code class="{name(.)}">
		<xsl:apply-templates/>
	</code>
</xsl:template>

<xsl:template match="funcparams">
	<xsl:text>(</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>)</xsl:text>
</xsl:template>

<xsl:template match="funcprototype">
	<p>
		<code>
			<xsl:apply-templates select="funcdef"/>
			<xsl:text> (</xsl:text>
			<xsl:for-each select="*">
				<xsl:apply-templates select="."/>
				<xsl:if test="position() != last()">
					<xsl:text>, </xsl:text>
				</xsl:if>
			</xsl:for-each>
			<xsl:text>);</xsl:text>
		</code>
	</p>
</xsl:template>

<xsl:template match="paramdef">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="varargs">
	<xsl:text>...</xsl:text>
</xsl:template>

<xsl:template match="void">
	<xsl:text>void</xsl:text>
</xsl:template>

<!-- == classsynopsis.cpp ================================================== -->

<xsl:template mode="classsynopsis.cpp" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.cpp" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == classsynopsis.idl ================================================== -->

<xsl:template mode="classsynopsis.idl" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.idl" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == classsynopsis.java ================================================= -->

<xsl:template mode="classsynopsis.java" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="classsynopsis">
	<div class="{name(.)}"><pre class="java">
		<xsl:choose>
			<xsl:when test="@class = 'interface'">
				<xsl:call-template name="FIXME"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:for-each select="ooclass[1]/modifier">
					<xsl:apply-templates mode="classsynopsis.java" select="."/>
					<xsl:text> </xsl:text>
				</xsl:for-each>
				<xsl:text>class </xsl:text>
				<xsl:apply-templates mode="classsynopsis.java" select="ooclass[1]"/>
				<xsl:if test="ooclass[position() &gt; 1]">
					<xsl:text> extends</xsl:text>
					<xsl:for-each select="ooclass[position() &gt; 1]">
						<xsl:text> </xsl:text>
						<xsl:apply-templates mode="classsynopsis.java" select="."/>
						<xsl:if test="following-sibling::ooclass">
							<xsl:text>,</xsl:text>
						</xsl:if>
					</xsl:for-each>
					<xsl:if test="oointerface|ooexception">
						<xsl:value-of select="concat($newline, $tab_java)"/>
					</xsl:if>
				</xsl:if>
				<xsl:if test="oointerface">
					<xsl:text>implements</xsl:text>
					<xsl:for-each select="oointerface">
						<xsl:text> </xsl:text>
						<xsl:apply-templates mode="classsynopsis.java" select="."/>
						<xsl:if test="following-sibling::oointerface">
							<xsl:text>,</xsl:text>
						</xsl:if>
					</xsl:for-each>
					<xsl:if test="ooexception">
						<xsl:value-of select="concat($newline, $tab_java)"/>
					</xsl:if>
				</xsl:if>
				<xsl:if test="ooexception">
					<xsl:text>throws</xsl:text>
					<xsl:for-each select="ooexception">
						<xsl:text> </xsl:text>
						<xsl:apply-templates mode="classsynopsis.java" select="."/>
						<xsl:if test="following-sibling::ooexception">
							<xsl:text>,</xsl:text>
						</xsl:if>
					</xsl:for-each>
				</xsl:if>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:value-of select="$newline"/>
		<xsl:text>{</xsl:text>
		<xsl:value-of select="$newline"/>
		<xsl:apply-templates mode="classsynopsis.java" select="
			constructorsynopsis	| destructorsynopsis	| fieldsynopsis	|
			methodsynopsis			| classsynopsis		"/>
		<xsl:text>}</xsl:text>
	</pre></div>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="classsynopsisinfo">
	<xsl:apply-templates mode="classsynopsis.java"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="
		classname		| exceptionname	| interfacename	| methodname	|
		modifier			| parameter			| type				| varname		">
	<span class="{name(.)}">
		<xsl:apply-templates mode="classsynopsis.java"/>
	</span>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="ooclass">
	<span class="{name(.)}">
		<xsl:apply-templates mode="classsynopsis.java" select="classname"/>
	</span>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="oointerface">
	<span class="{name(.)}">
		<xsl:apply-templates mode="classsynopsis.java" select="interfacename"/>
	</span>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="ooexception">
	<span class="{name(.)}">
		<xsl:apply-templates mode="classsynopsis.java" select="exceptionname"/>
	</span>
</xsl:template>

<xsl:template mode="classsynopsis.java" match="void">
	<span class="{name(.)}">void</span>
</xsl:template>

<!-- == classsynopsis.python ================================================= -->

<xsl:template mode="classsynopsis.python" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="classsynopsis.python" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
