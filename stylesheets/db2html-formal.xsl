<?xml version='1.0'?><!-- -*- Mode: xml -*- -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:template name="formal.object">
	<div class="{name(.)}">
		<xsl:call-template name="formal.object.heading"/>
		<xsl:apply-templates mode="content.mode"/>
	</div>
</xsl:template>


</xsl:stylesheet>
