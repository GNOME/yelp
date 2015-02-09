/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Shaun McCance <shaunm@gnome.org>
 */

#include "config.h"

#include <errno.h>
#include <string.h>

#include <glib/gi18n.h>

#include "yelp-bz2-decompressor.h"

static void yelp_bz2_decompressor_iface_init          (GConverterIface *iface);

struct _YelpBz2Decompressor
{
    GObject parent_instance;

    bz_stream bzstream;
};

G_DEFINE_TYPE_WITH_CODE (YelpBz2Decompressor, yelp_bz2_decompressor, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
                                                yelp_bz2_decompressor_iface_init))

static void
yelp_bz2_decompressor_finalize (GObject *object)
{
    YelpBz2Decompressor *decompressor;

    decompressor = YELP_BZ2_DECOMPRESSOR (object);

    BZ2_bzDecompressEnd (&decompressor->bzstream);

    G_OBJECT_CLASS (yelp_bz2_decompressor_parent_class)->finalize (object);
}

static void
yelp_bz2_decompressor_init (YelpBz2Decompressor *decompressor)
{
}

static void
yelp_bz2_decompressor_constructed (GObject *object)
{
    YelpBz2Decompressor *decompressor;
    int res;

    decompressor = YELP_BZ2_DECOMPRESSOR (object);

    res = BZ2_bzDecompressInit (&decompressor->bzstream, 0, FALSE);

    if (res == BZ_MEM_ERROR )
        g_error ("YelpBz2Decompressor: Not enough memory for bzip2 use");

    if (res != BZ_OK)
        g_error ("YelpBz2Decompressor: Unexpected bzip2 error");
}

static void
yelp_bz2_decompressor_class_init (YelpBz2DecompressorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = yelp_bz2_decompressor_finalize;
    gobject_class->constructed = yelp_bz2_decompressor_constructed;
}

YelpBz2Decompressor *
yelp_bz2_decompressor_new (void)
{
    YelpBz2Decompressor *decompressor;

    decompressor = g_object_new (YELP_TYPE_BZ2_DECOMPRESSOR, NULL);

    return decompressor;
}

static void
yelp_bz2_decompressor_reset (GConverter *converter)
{
    YelpBz2Decompressor *decompressor = YELP_BZ2_DECOMPRESSOR (converter);
    int res;

    /* libbzip2 doesn't have a reset function.  Ending and reiniting
     * might do the trick.  But this is untested.  If reset matters
     * to you, test this.
     */
    BZ2_bzDecompressEnd (&decompressor->bzstream);
    res = BZ2_bzDecompressInit (&decompressor->bzstream, 0, FALSE);

    if (res == BZ_MEM_ERROR )
        g_error ("YelpBz2Decompressor: Not enough memory for bzip2 use");

    if (res != BZ_OK)
        g_error ("YelpBz2Decompressor: Unexpected bzip2 error");
}

static GConverterResult
yelp_bz2_decompressor_convert (GConverter *converter,
                               const void *inbuf,
                               gsize       inbuf_size,
                               void       *outbuf,
                               gsize       outbuf_size,
                               GConverterFlags flags,
                               gsize      *bytes_read,
                               gsize      *bytes_written,
                               GError    **error)
{
    YelpBz2Decompressor *decompressor;
    int res;

    decompressor = YELP_BZ2_DECOMPRESSOR (converter);

    decompressor->bzstream.next_in = (void *)inbuf;
    decompressor->bzstream.avail_in = inbuf_size;

    decompressor->bzstream.next_out = outbuf;
    decompressor->bzstream.avail_out = outbuf_size;

    res = BZ2_bzDecompress (&decompressor->bzstream);

    if (res == BZ_DATA_ERROR || res == BZ_DATA_ERROR_MAGIC) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             _("Invalid compressed data"));
        return G_CONVERTER_ERROR;
    }

    if (res == BZ_MEM_ERROR) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Not enough memory"));
        return G_CONVERTER_ERROR;
    }

    if (res == BZ_OK || res == BZ_STREAM_END) {
        *bytes_read = inbuf_size - decompressor->bzstream.avail_in;
        *bytes_written = outbuf_size - decompressor->bzstream.avail_out;

        if (res == BZ_STREAM_END)
            return G_CONVERTER_FINISHED;
        return G_CONVERTER_CONVERTED;
    }

    g_assert_not_reached ();
}

static void
yelp_bz2_decompressor_iface_init (GConverterIface *iface)
{
  iface->convert = yelp_bz2_decompressor_convert;
  iface->reset = yelp_bz2_decompressor_reset;
}
