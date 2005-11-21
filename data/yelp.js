var slt = {
  /* Shorten the link trail by chopping off links from the
  beginning of it until it's no longer wider than the screen */
  init: function() {
    slt.lt = document.getElementById('linktrail');
    if (!slt.lt) return;

    /* Try and add links, in case we've just resized the
       window to be bigger */
    var canContinue = true;
    while (canContinue &&
          (slt.findRightEdge() < document.body.offsetWidth)) {
      canContinue = slt.addLinktrailElement();
    }

    /* Now remove links until the linktrail is narrower than
       the window */
    canContinue = true;
    while (canContinue &&
          (slt.findRightEdge() > document.body.offsetWidth)) {
      canContinue = slt.removeLinktrailElement();
    }
  },

  findRightEdge: function() {
    /* get the position of the far right-hand-edge of the rightmost
       element in the linktrail */
    var maxright = 0;

    for (var i=0;i<slt.lt.childNodes.length;i++) {
      if (typeof slt.lt.childNodes[i].offsetLeft != 'undefined' &&
          typeof slt.lt.childNodes[i].offsetWidth != 'undefined') {
        var rightedge = slt.lt.childNodes[i].offsetLeft +
            slt.lt.childNodes[i].offsetWidth;
        if (rightedge > maxright) maxright = rightedge;
      }
    }

    return maxright;
  },

  removeLinktrailElement: function() {
    /* Walk through the link trail until we find a complete
       link; when we do, change its displayed text to "…",
       put its actual text in a tooltip, and return */
    var links = slt.lt.getElementsByTagName('a');

    for (var i=0;i<links.length;i++) {
      if (links[i].firstChild) {
        if (links[i].firstChild.nodeValue != '…') {
          links[i].title = links[i].firstChild.nodeValue;
          links[i].firstChild.nodeValue = '…';
          return true;
        }
      }
    }

    /* There are no links left to remove; indicate this back
       to the caller so it doesn't loop infinitely */
    return false;
  },

  addLinktrailElement: function() {
    /* Walk through the link trail until we find a "…"
       link; when we do, change its displayed text back to
       what it should be and return */
    var links = slt.lt.getElementsByTagName('a');

    for (var i=0;i<links.length;i++) {
      if (links[i].firstChild) {
        if (links[i].firstChild.nodeValue == '…') {
          links[i].firstChild.nodeValue = links[i].title;
          return true;
        }
      }
    }

    /* There are no links left to add; indicate this back
       to the caller so it doesn't loop infinitely */
    return false;
  }
}

/* addEventListener() is Gecko-only, but so is yelp */
window.addEventListener("load",slt.init,false);
/* load doesn't seem to get fired in Yelp.  I might need to tell Gecko
   that I'm finished or something.  DOMContentLoaded works though */
window.addEventListener("DOMContentLoaded",slt.init,false);
window.addEventListener("resize",slt.init,false);

function submit_search ()
{
    window.location = "x-yelp-search:" + document.getElementById ('search-entry').value;
    return false;
}
