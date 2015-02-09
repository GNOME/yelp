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

#include "yelp-lzma-decompressor.h"

#include <errno.h>
#include <string.h>

#include <lzma.h>

#include <glib/gi18n.h>
#include <gio/gio.h>


static void yelp_lzma_decompressor_iface_init          (GConverterIface *iface);

struct _YelpLzmaDecompressor
{
    GObject parent_instance;

    lzma_stream lzmastream;
};

G_DEFINE_TYPE_WITH_CODE (YelpLzmaDecompressor, yelp_lzma_decompressor, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
                                                yelp_lzma_decompressor_iface_init))

static void
yelp_lzma_decompressor_finalize (GObject *object)
{
    YelpLzmaDecompressor *decompressor;

    decompressor = YELP_LZMA_DECOMPRESSOR (object);

    lzma_end (&decompressor->lzmastream);

    G_OBJECT_CLASS (yelp_lzma_decompressor_parent_class)->finalize (object);
}

static void
yelp_lzma_decompressor_init (YelpLzmaDecompressor *decompressor)
{
}

static void
yelp_lzma_decompressor_constructed (GObject *object)
{
    YelpLzmaDecompressor *decompressor;
    lzma_stream tmp = LZMA_STREAM_INIT;
    lzma_ret res;

    decompressor = YELP_LZMA_DECOMPRESSOR (object);

    decompressor->lzmastream = tmp;
    res = lzma_auto_decoder (&decompressor->lzmastream, SIZE_MAX, 0);

    if (res == LZMA_MEM_ERROR)
        g_error ("YelpLzmaDecompressor: Not enough memory for lzma use");

    if (res == LZMA_OPTIONS_ERROR)
        g_error ("YelpLzmaDecompressor: Unsupported flags");

    if (res != LZMA_OK)
        g_error ("YelpLzmaDecompressor: Unexpected lzma error");
}

static void
yelp_lzma_decompressor_class_init (YelpLzmaDecompressorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = yelp_lzma_decompressor_finalize;
    gobject_class->constructed = yelp_lzma_decompressor_constructed;
}

YelpLzmaDecompressor *
yelp_lzma_decompressor_new (void)
{
    YelpLzmaDecompressor *decompressor;

    decompressor = g_object_new (YELP_TYPE_LZMA_DECOMPRESSOR, NULL);

    return decompressor;
}

static void
yelp_lzma_decompressor_reset (GConverter *converter)
{
    YelpLzmaDecompressor *decompressor = YELP_LZMA_DECOMPRESSOR (converter);
    lzma_ret res;

    /* lzma doesn't have a reset function.  Ending and reiniting
     * might do the trick.  But this is untested.  If reset matters
     * to you, test this.
     */
    lzma_end (&decompressor->lzmastream);
    res = lzma_code (&decompressor->lzmastream, LZMA_RUN);

    if (res == LZMA_MEM_ERROR )
        g_error ("YelpLzmaDecompressor: Not enough memory for lzma use");

    if (res != LZMA_OK)
        g_error ("YelpLzmaDecompressor: Unexpected lzma error");
}

static GConverterResult
yelp_lzma_decompressor_convert (GConverter *converter,
                                const void *inbuf,
                                gsize       inbuf_size,
                                void       *outbuf,
                                gsize       outbuf_size,
                                GConverterFlags flags,
                                gsize      *bytes_read,
                                gsize      *bytes_written,
                                GError    **error)
{
    YelpLzmaDecompressor *decompressor;
    lzma_ret res;

    decompressor = YELP_LZMA_DECOMPRESSOR (converter);

    decompressor->lzmastream.next_in = (void *)inbuf;
    decompressor->lzmastream.avail_in = inbuf_size;

    decompressor->lzmastream.next_out = outbuf;
    decompressor->lzmastream.avail_out = outbuf_size;

    res = lzma_code (&decompressor->lzmastream, LZMA_RUN);

    if (res == LZMA_DATA_ERROR) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             _("Invalid compressed data"));
        return G_CONVERTER_ERROR;
    }

    if (res == LZMA_MEM_ERROR) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Not enough memory"));
        return G_CONVERTER_ERROR;
    }

    if (res == LZMA_OK || res == LZMA_STREAM_END) {
        *bytes_read = inbuf_size - decompressor->lzmastream.avail_in;
        *bytes_written = outbuf_size - decompressor->lzmastream.avail_out;

        if (res == LZMA_STREAM_END)
            return G_CONVERTER_FINISHED;
        return G_CONVERTER_CONVERTED;
    }

    g_assert_not_reached ();
}

static void
yelp_lzma_decompressor_iface_init (GConverterIface *iface)
{
  iface->convert = yelp_lzma_decompressor_convert;
  iface->reset = yelp_lzma_decompressor_reset;
}
