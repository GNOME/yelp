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

<!-- == classsynopsis ====================================================== -->

<xsl:template match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
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
		<xsl:when test="$language = 'perl'">
			<xsl:apply-templates select="." mode="classsynopsis.perl"/>
		</xsl:when>
		<xsl:when test="$language = 'python'">
			<xsl:apply-templates select="." mode="classsynopsis.perl"/>
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
		<p>
			<xsl:call-template name="anchor"/>
			<xsl:for-each select="*">
				<xsl:if test="name(.) = 'command' and position() != 1">
					<br/>
				</xsl:if>
				<xsl:apply-templates select="."/>
			</xsl:for-each>
		</p>
	</div>
</xsl:template>

<xsl:template match="arg | group">
	<xsl:variable name="sepchar">
		<xsl:choose>
			<xsl:when test="ancestor-or-self::*[@sepchar]">
				<xsl:value-of select="ancestor-or-self::[@sepchar][1]/@sepchar"/>
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

<xsl:template match="sbr">
	<br/>
</xsl:template>

<xsl:template match="synopfragment">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template match="synopfragmentref">
	<xsl:call-template name="FIXME"/>
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

<!-- == synopsis.cpp.mode ================================================== -->

<xsl:template mode="synopsis.cpp.mode" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.cpp.mode" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == synopsis.idl.mode ================================================== -->

<xsl:template mode="synopsis.idl.mode" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.idl.mode" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == synopsis.java.mode ================================================= -->

<xsl:template mode="synopsis.java.mode" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.java.mode" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == synopsis.perl.mode ================================================= -->

<xsl:template mode="synopsis.perl.mode" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.perl.mode" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- == synopsis.python.mode ================================================= -->

<xsl:template mode="synopsis.python.mode" match="*">
	<xsl:apply-templates select="."/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="classsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="classsynopsisinfo">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="classname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="constructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="destructorsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="exceptionname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="fieldsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="initializer">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="interfacename">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="methodname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="methodparam">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="methodsynopsis">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="modifier">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="ooclass">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="ooexception">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="oointerface">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="parameter">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="type">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="varname">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<xsl:template mode="synopsis.python.mode" match="void">
	<xsl:call-template name="FIXME"/>
</xsl:template>

<!-- ======================================================================= -->

</xsl:stylesheet>
