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

#include <glib/gi18n.h>
#include <string.h>

#include "yelp-magic-decompressor.h"

#ifdef ENABLE_BZ2
#include "yelp-bz2-decompressor.h"
#endif

#ifdef ENABLE_LZMA
#include "yelp-lzma-decompressor.h"
#endif


static void yelp_magic_decompressor_iface_init          (GConverterIface *iface);

struct _YelpMagicDecompressor
{
    GObject parent_instance;

    GConverter *magic_decoder_ring;

    gboolean first;
};

G_DEFINE_TYPE_WITH_CODE (YelpMagicDecompressor, yelp_magic_decompressor, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
                                                yelp_magic_decompressor_iface_init))

static void
yelp_magic_decompressor_dispose (GObject *object)
{
    YelpMagicDecompressor *decompressor = YELP_MAGIC_DECOMPRESSOR (object);

    if (decompressor->magic_decoder_ring) {
        g_object_unref (decompressor->magic_decoder_ring);
        decompressor->magic_decoder_ring = NULL;
    }

    G_OBJECT_CLASS (yelp_magic_decompressor_parent_class)->dispose (object);
}

static void
yelp_magic_decompressor_init (YelpMagicDecompressor *decompressor)
{
    decompressor->magic_decoder_ring = NULL;
    decompressor->first = TRUE;
}

static void
yelp_magic_decompressor_class_init (YelpMagicDecompressorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = yelp_magic_decompressor_dispose;
}

YelpMagicDecompressor *
yelp_magic_decompressor_new (void)
{
    YelpMagicDecompressor *decompressor;

    decompressor = g_object_new (YELP_TYPE_MAGIC_DECOMPRESSOR, NULL);

    return decompressor;
}

static void
yelp_magic_decompressor_reset (GConverter *converter)
{
    YelpMagicDecompressor *decompressor = YELP_MAGIC_DECOMPRESSOR (converter);

    if (decompressor->magic_decoder_ring)
        g_converter_reset (decompressor->magic_decoder_ring);
}

static GConverter*
yelp_magic_decompressor_choose (const void *inbuf, gsize inbuf_size)
{
    /* If input_size is less than two the first time, we end up
     * not getting detection.  Might be worth addressing.  Not
     * sure I care.
     *
     * The two-byte magic we're doing here is not sufficient in
     * the general case.  It is sufficient for the specific data
     * Yelp deals with.
     */
    if (inbuf_size <= 2)
        return NULL;

#ifdef ENABLE_BZ2
    if (((gchar *) inbuf)[0] == 'B' && ((gchar *) inbuf)[1] == 'Z') {
        return (GConverter *) yelp_bz2_decompressor_new ();
    }
#endif
#ifdef ENABLE_LZMA
    if (((gchar *) inbuf)[0] == ']' && ((gchar *) inbuf)[1] == '\0') {
        return (GConverter *) yelp_lzma_decompressor_new ();
    }
#endif
    if (((guint8*) inbuf)[0] == 0x1F && ((guint8*) inbuf)[1] == 0x8B) {
        return (GConverter *) g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
    }

    return NULL;
}

static GConverterResult
yelp_magic_decompressor_convert (GConverter *converter,
                                 const void *inbuf,
                                 gsize       inbuf_size,
                                 void       *outbuf,
                                 gsize       outbuf_size,
                                 GConverterFlags flags,
                                 gsize      *bytes_read,
                                 gsize      *bytes_written,
                                 GError    **error)
{
    YelpMagicDecompressor *decompressor;
    gsize txfer_size;

    decompressor = YELP_MAGIC_DECOMPRESSOR (converter);

    if (decompressor->first) {
        decompressor->magic_decoder_ring =
            yelp_magic_decompressor_choose (inbuf, inbuf_size);
        decompressor->first = FALSE;
    }

    if (decompressor->magic_decoder_ring) {
        return g_converter_convert (decompressor->magic_decoder_ring,
                                    inbuf, inbuf_size,
                                    outbuf, outbuf_size,
                                    flags,
                                    bytes_read, bytes_written,
                                    error);
    }

    /* If there's no magic_decoder_ring, we just copy the data
     * straight through. */
    txfer_size = MIN (inbuf_size, outbuf_size);
    memcpy (outbuf, inbuf, txfer_size);
    *bytes_read = txfer_size;
    *bytes_written = txfer_size;
    
    if (flags & G_CONVERTER_INPUT_AT_END)
        return G_CONVERTER_FINISHED;
    return G_CONVERTER_CONVERTED;
}

static void
yelp_magic_decompressor_iface_init (GConverterIface *iface)
{
  iface->convert = yelp_magic_decompressor_convert;
  iface->reset = yelp_magic_decompressor_reset;
}
