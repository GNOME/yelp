<?xml version='1.0'?><!-- -*- Mode: fundamental; tab-width: 3 -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">
<xsl:template name="entry" match="entry | entrytbl">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="colsep" select="false()"/>
	<xsl:param name="rowsep" select="false()"/>
	<xsl:param name="col" select="1"/>
	<xsl:param name="spans"/>
	<xsl:variable name="named.colnum">
		<xsl:call-template name="entry.colnum">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="entry" select="."/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="entry.colnum">
		<xsl:choose>
			<xsl:when test="$named.colnum &gt; 0">
				<xsl:value-of select="$named.colnum"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$col"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="entry.colspan">
		<xsl:choose>
			<xsl:when test="@spanname or @namest">
				<xsl:call-template name="calculate.colspan">
					<xsl:with-param name="colspecs" select="$colspecs"/>
					<xsl:with-param name="spanspecs" select="$spanspecs"/>
					<xsl:with-param name="entry" select="."/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>1</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="following.spans">
		<xsl:call-template name="calculate.following.spans">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="colspan" select="$entry.colspan"/>
			<xsl:with-param name="spans" select="$spans"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="element">
		<xsl:choose>
			<xsl:when
				test="name(../..) = 'thead' or name(../..) = 'tfoot'">th</xsl:when>
			<xsl:otherwise>td</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="style">
		<xsl:if test="(following-sibling::*) and (
				(@colsep = '1') or
				($colspecs[@colname = current()/@colname]/@colsep = '1') or
				($colspecs[@colname = current()/@nameend]/@colsep = '1') or
				($spanspecs[@spanname = current()/@spanname]/@colsep = '1') or
				($colsep = '1' and (
					(@colsep != '0') or
					($colspecs[@colname = current()/@colname]/@colsep != '0') or
					($colspecs[@colname = current()/@nameend]/@colsep != '0') or
					($spanspecs[@spanname = current()/@spanname]/@colsep != '0')
			)) )">
			<xsl:text>border-right: outset 1px; </xsl:text>
		</xsl:if>
		<xsl:if test="(../following-sibling::*) and (
				(@rowsep = '1') or
				($colspecs[@colname = current()/@colname]/@rowsep = '1') or
				($colspecs[@colname = current()/@namest]/@rowsep = '1') or
				($spanspecs[@spanname = current()/@spanname]/@rowsep = '1') or
				($rowsep = '1' and (
					(@rowsep != '0') or
					($colspecs[@colname = current()/@colname]/@rowsep != '0') or
					($colspecs[@colname = current()/@namest]/@rowsep != '0') or
					($spanspecs[@spanname = current()/@spanname]/@rowsep != '0')
			)) )">
			<xsl:text>border-bottom: outset 1px; </xsl:text>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="@align">
				<xsl:text>text-align: </xsl:text>
				<xsl:value-of select="@align"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when
					test="$colspecs[@colname = current()/@colname]/@align">
				<xsl:text>text-align: </xsl:text>
				<xsl:value-of
					select="$colspecs[@colname = current()/@colname]/@align"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when
					test="$colspecs[@colname = current()/@namest]/@align">
				<xsl:text>text-align: </xsl:text>
				<xsl:value-of
					select="$colspecs[@colname = current()/@namest]/@align"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when
					test="$spanspecs[@spanname = current()/@spanname]/@align">
				<xsl:text>text-align: </xsl:text>
				<xsl:value-of
					select="$spanspecs[@spanname = current()/@spanname]/@align"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when test="../../../@align">
				<xsl:text>text-align: </xsl:text>
				<xsl:value-of select="../../../@align"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="@valign">
				<xsl:text>vertical-align: </xsl:text>
				<xsl:value-of select="@valign"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when
					test="$colspecs[@colname = current()/@colname]/@valign">
				<xsl:text>vertical-align: </xsl:text>
				<xsl:value-of
					select="$colspecs[@colname = current()/@colname]/@valign"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when
					test="$colspecs[@colname = current()/@namest]/@valign">
				<xsl:text>vertical-align: </xsl:text>
				<xsl:value-of
					select="$colspecs[@colname = current()/@namest]/@valign"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when
					test="$spanspecs[@spanname = current()/@spanname]/@valign">
				<xsl:text>vertical-align: </xsl:text>
				<xsl:value-of
					select="$spanspecs[@spanname = current()/@spanname]/@valign"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when test="../@valign">
				<xsl:text>vertical-align: </xsl:text>
				<xsl:value-of select="../@valign"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
			<xsl:when test="../../@valign">
				<xsl:text>vertical-align: </xsl:text>
				<xsl:value-of select="../../@valign"/>
				<xsl:text>; </xsl:text>
			</xsl:when>
		</xsl:choose>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$spans != '' and not(starts-with($spans, '0:'))">
			<xsl:call-template name="entry">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="col" select="$col + 1"/>
				<xsl:with-param name="spans" select="substring-after($spans, ':')"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$entry.colnum &gt; $col">
			<td>
				<xsl:if test="$style">
					<xsl:attribute name="style">
						<xsl:value-of select="$style"/>
					</xsl:attribute>
				</xsl:if>
				<xsl:text>&#160;</xsl:text>
			</td>
			<xsl:call-template name="entry">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="col" select="$col + 1"/>
				<xsl:with-param name="spans" select="substring-after($spans, ':')"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:variable name="char">
				<xsl:choose>
					<xsl:when test="@char">
						<xsl:value-of select="@char"/>
					</xsl:when>
					<xsl:when
							test="$colspecs[@colname = current()/@colname]/@char">
						<xsl:value-of
							select="$colspecs[@colname = current()/@colname]/@char"/>
					</xsl:when>
					<xsl:when
							test="$colspecs[@colname = current()/@namest]/@char">
						<xsl:value-of
							select="$colspecs[@colname = current()/@namest]/@char"/>
					</xsl:when>
					<xsl:when
							test="$spanspecs[@spanname = current()/@spanname]/@char">
						<xsl:value-of
							select="$spanspecs[@spanname = current()/@spanname]/@char"/>
					</xsl:when>
					<xsl:when test="../../../@char">
						<xsl:value-of select="../../@char"/>
					</xsl:when>
				</xsl:choose>
			</xsl:variable>
			<xsl:variable name="charoff">
				<xsl:choose>
					<xsl:when test="@charoff">
						<xsl:value-of select="@charoff"/>
					</xsl:when>
					<xsl:when
							test="$colspecs[@colname = current()/@colname]/@charoff">
						<xsl:value-of
							select="$colspecs[@colname = current()/@colname]/@charoff"/>
					</xsl:when>
					<xsl:when
							test="$colspecs[@colname = current()/@namest]/@charoff">
						<xsl:value-of
							select="$colspecs[@colname = current()/@namest]/@charoff"/>
					</xsl:when>
					<xsl:when
							test="$spanspecs[@spanname = current()/@spanname]/@charoff">
						<xsl:value-of
							select="$spanspecs[@spanname = current()/@spanname]/@charoff"/>
					</xsl:when>
					<xsl:when test="../../../@charoff">
						<xsl:value-of select="../../@charoff"/>
					</xsl:when>
				</xsl:choose>
			</xsl:variable>
			<xsl:element name="{$element}">
				<xsl:if test="$style != ''">
					<xsl:attribute name="style">
						<xsl:value-of select="$style"/>
					</xsl:attribute>
				</xsl:if>
				<xsl:if test="@morerows &gt; 0">
					<xsl:attribute name="rowspan">
						<xsl:value-of select="@morerows + 1"/>
					</xsl:attribute>
				</xsl:if>
				<xsl:if test="$entry.colspan &gt; 1">
					<xsl:attribute name="colspan">
						<xsl:value-of select="$entry.colspan"/>
					</xsl:attribute>
				</xsl:if>
				<xsl:if test="$char != ''">
					<xsl:attribute name="char">
						<xsl:value-of select="$char"/>
					</xsl:attribute>
				</xsl:if>
				<xsl:if test="$charoff != ''">
					<xsl:attribute name="charoff">
						<xsl:value-of select="$charoff"/>
					</xsl:attribute>
				</xsl:if>
				<xsl:apply-templates/>
			</xsl:element>
			<xsl:apply-templates select="following-sibling::*[1]">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="colsep" select="$colsep"/>
				<xsl:with-param name="rowsep" select="$rowsep"/>
				<xsl:with-param name="col" select="$col + $entry.colspan"/>
				<xsl:with-param name="spans" select="$following.spans"/>
			</xsl:apply-templates>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template match="tbody | tfoot | thead">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="colsep" select="false()"/>
	<xsl:param name="rowsep" select="false()"/>
	<xsl:element name="{name(.)}">
		<xsl:if test="@valign">
			<xsl:attribute name="valign">
				<xsl:value-of select="@valign"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:choose>
			<xsl:when test="colspec">
				<xsl:apply-templates select="row[1]">
					<xsl:with-param name="colspecs" select="colspec"/>
					<xsl:with-param name="spanspecs" select="spanspec"/>
					<xsl:with-param name="colsep" select="$colsep"/>
					<xsl:with-param name="rowsep" select="$rowsep"/>
				</xsl:apply-templates>
			</xsl:when>
			<xsl:otherwise>
				<xsl:apply-templates select="row[1]">
					<xsl:with-param name="colspecs" select="$colspecs"/>
					<xsl:with-param name="spanspecs" select="$spanspecs"/>
					<xsl:with-param name="colsep" select="$colsep"/>
					<xsl:with-param name="rowsep" select="$rowsep"/>
				</xsl:apply-templates>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:element>
</xsl:template>

<xsl:template match="tgroup">
	<xsl:variable name="style">
		<xsl:if test="(../@frame) and (../@frame != 'all')">
			<xsl:text>border: none; </xsl:text>
		</xsl:if>
		<xsl:if test="../@frame = 'bottom' or ../@frame = 'topbot'">
			<xsl:text>border-bottom: outset 1px; </xsl:text>
		</xsl:if>
		<xsl:if test="../@frame = 'top' or ../@frame = 'topbot'">
			<xsl:text>border-top: outset 1px; </xsl:text>
		</xsl:if>
		<xsl:if test="../@frame = 'sides'">
			<xsl:text>border-left: outset 1px; border-right: outset 1px; </xsl:text>
		</xsl:if>
	</xsl:variable>
	<table>
		<xsl:if test="../title">
			<xsl:attribute name="summary">
				<xsl:value-of select="../title"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:if test="../@pgwide = '1'">
			<xsl:attribute name="width">100%</xsl:attribute>
		</xsl:if>
		<xsl:if test="string($style) != ''">
			<xsl:attribute name="style">
				<xsl:value-of select="$style"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:apply-templates select="thead">
			<xsl:with-param name="colspecs" select="colspec"/>
			<xsl:with-param name="spanspecs" select="spanspec"/>
			<xsl:with-param name="colsep" select="@colsep = '1'"/>
			<xsl:with-param name="rowsep" select="@rowsep = '1'"/>
		</xsl:apply-templates>
		<xsl:apply-templates select="tbody">
			<xsl:with-param name="colspecs" select="colspec"/>
			<xsl:with-param name="spanspecs" select="spanspec"/>
			<xsl:with-param name="colsep" select="@colsep = '1'"/>
			<xsl:with-param name="rowsep" select="@rowsep = '1'"/>
		</xsl:apply-templates>
		<xsl:apply-templates select="tfoot">
			<xsl:with-param name="colspecs" select="colspec"/>
			<xsl:with-param name="spanspecs" select="spanspec"/>
			<xsl:with-param name="colsep" select="@colsep = '1'"/>
			<xsl:with-param name="rowsep" select="@rowsep = '1'"/>
		</xsl:apply-templates>
	</table>
</xsl:template>

<xsl:template match="row">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="colsep" select="false()"/>
	<xsl:param name="rowsep" select="false()"/>
	<xsl:param name="spans"/>
	<tr>
		<xsl:if test="name(..) = 'tbody'">
			<xsl:attribute name="class">
				<xsl:choose>
					<xsl:when
						test="count(preceding-sibling::row) mod 2">odd</xsl:when>
					<xsl:otherwise>even</xsl:otherwise>
				</xsl:choose>
			</xsl:attribute>
		</xsl:if>
		<xsl:apply-templates select="*[1]">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="colsep" select="$colsep"/>
			<xsl:with-param name="rowsep" select="
				(@rowsep = '1') or ((@rowsep != '0') and $rowsep)"/>
			<xsl:with-param name="spans" select="$spans"/>
		</xsl:apply-templates>
	</tr>
	<xsl:apply-templates select="following-sibling::row[1]">
		<xsl:with-param name="colspecs" select="$colspecs"/>
		<xsl:with-param name="spanspecs" select="$spanspecs"/>
		<xsl:with-param name="colsep" select="$colsep"/>
		<xsl:with-param name="rowsep" select="$rowsep"/>
		<xsl:with-param name="spans">
			<xsl:apply-templates select="*[1]" mode="span">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="spans" select="$spans"/>
			</xsl:apply-templates>
		</xsl:with-param>
	</xsl:apply-templates>
</xsl:template>

<!-- ======================================================================= -->

<xsl:template mode="span" name="entry.span" match="entry | entrytbl">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="col" select="1"/>
	<xsl:param name="spans"/>
	<xsl:variable name="entry.colnum">
		<xsl:call-template name="entry.colnum">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="entry" select="."/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="entry.colspan">
		<xsl:choose>
			<xsl:when test="@spanname or @namest">
				<xsl:call-template name="calculate.colspan">
					<xsl:with-param name="colspecs" select="$colspecs"/>
					<xsl:with-param name="spanspecs" select="$spanspecs"/>
					<xsl:with-param name="entry" select="."/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>1</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="following.spans">
		<xsl:call-template name="calculate.following.spans">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="colspan" select="$entry.colspan"/>
			<xsl:with-param name="spans" select="$spans"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$spans != '' and not(starts-with($spans, '0:'))">
			<xsl:value-of select="substring-before($spans, ':') - 1"/>
			<xsl:text>:</xsl:text>
			<xsl:call-template name="entry.span">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="col" select="$col + 1"/>
				<xsl:with-param name="spans" select="substring-after($spans, ':')"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$entry.colnum &gt; $col">
			<xsl:text>0:</xsl:text>
			<xsl:call-template name="entry.span">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="col" select="$col + $entry.colspan"/>
				<xsl:with-param name="spans" select="$following-spans"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:call-template name="copy-string">
				<xsl:with-param name="count" select="$entry.colspan"/>
				<xsl:with-param name="string">
					<xsl:choose>
						<xsl:when test="@morerows">
							<xsl:value-of select="@morerows"/>
						</xsl:when>
						<xsl:otherwise>0</xsl:otherwise>
					</xsl:choose>
					<xsl:text>:</xsl:text>
				</xsl:with-param>
			</xsl:call-template>
			<xsl:if test="following-sibling::*">
				<xsl:apply-templates select="following-sibling::*[1]" mode="span">
					<xsl:with-param name="colspecs" select="$colspecs"/>
					<xsl:with-param name="spanspecs" select="$spanspecs"/>
					<xsl:with-param name="col" select="$col + $entry.colspan"/>
					<xsl:with-param name="spans" select="$following.spans"/>
				</xsl:apply-templates>
			</xsl:if>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="entry.colnum">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="entry" select="."/>
	<xsl:choose>
		<xsl:when test="$entry/@spanname">
			<xsl:variable name="spanspec"
				select="$spanspecs[@spanname = $entry/@spanname]"/>
			<xsl:variable name="colspec"
				select="$colspecs[@colname = $spanspec/@namest]"/>
			<xsl:call-template name="colspec.colnum">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="colspec" select="$colspec"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$entry/@colname">
			<xsl:variable name="colspec"
				select="$colspecs[@colname = $entry/@colname]"/>
			<xsl:call-template name="colspec.colnum">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="colspec" select="$colspec"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:when test="$entry/@namest">
			<xsl:variable name="colspec"
				select="$colspecs[@colname = $entry/namest]"/>
			<xsl:call-template name="colspec.colnum">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="colspec" select="$colspec"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>0</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="colspec.colnum">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="colspec" select="."/>
	<xsl:choose>
		<xsl:when test="$colspec/@colnum">
			<xsl:value-of select="$colspec/@colnum"/>
		</xsl:when>
		<xsl:when test="$colspec/preceding-sibling::colspec">
			<xsl:variable name="prec.colspec.colnum">
				<xsl:call-template name="colspec.colnum">
					<xsl:with-param name="colspecs" select="$colspecs"/>
					<xsl:with-param name="spanspecs" select="$spanspecs"/>
					<xsl:with-param name="colspec"
						select="$colspec/preceding-sibling::colspec[1]"/>
				</xsl:call-template>
			</xsl:variable>
			<xsl:value-of select="$prec.colspec.colnum + 1"/>
		</xsl:when>
		<xsl:otherwise>1</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="calculate.colspan">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="entry" select="."/>
	<xsl:variable name="namest">
		<xsl:choose>
			<xsl:when test="$entry/@spanname">
				<xsl:value-of
					select="$spanspecs[@spanname = $entry/@spanname]/@namest"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$entry/@namest"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="nameend">
		<xsl:choose>
			<xsl:when test="$entry/@spanname">
				<xsl:value-of
					select="$spanspecs[@spanname = $entry/@spanname]/@nameend"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$entry/@nameend"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<xsl:variable name="colnumst">
		<xsl:call-template name="colspec.colnum">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="colspec" select="$colspecs[@colname = $namest]"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="colnumend">
		<xsl:call-template name="colspec.colnum">
			<xsl:with-param name="colspecs" select="$colspecs"/>
			<xsl:with-param name="spanspecs" select="$spanspecs"/>
			<xsl:with-param name="colspec" select="$colspecs[@colname = $nameend]"/>
		</xsl:call-template>
	</xsl:variable>
	<xsl:choose>
		<xsl:when test="$namest = '' or $nameend = ''">1</xsl:when>
		<xsl:when test="$colnumend &gt; $colnumst">
			<xsl:value-of select="$colnumend - $colnumst + 1"/>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="$colnumst - $colnumend + 1"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="calculate.following.spans">
	<xsl:param name="colspecs"/>
	<xsl:param name="spanspecs"/>
	<xsl:param name="colspan" select="1"/>
	<xsl:param name="spans" select="''"/>
	<xsl:choose>
		<xsl:when test="$colspan &gt; 0">
			<xsl:call-template name="calculate.following.spans">
				<xsl:with-param name="colspecs" select="$colspecs"/>
				<xsl:with-param name="spanspecs" select="$spanspecs"/>
				<xsl:with-param name="colspan" select="$colspan - 1"/>
				<xsl:with-param name="spans" select="substring-after($spans, ':')"/>
			</xsl:call-template>
		</xsl:when>
		<xsl:otherwise>
			<xsl:value-of select="$spans"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

</xsl:stylesheet>
