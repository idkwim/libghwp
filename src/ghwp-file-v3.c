/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ghwp-file-v3.c
 *
 * Copyright (C) 2013 Hodong Kim <cogniti@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This software have been developed with reference to
 * the HWP file format open specification by Hancom, Inc.
 * http://www.hancom.co.kr/userofficedata.userofficedataList.do?menuFlag=3
 * 한글과컴퓨터의 한/글 문서 파일(.hwp) 공개 문서를 참고하여 개발하였습니다.
 */

#include "ghwp-file-v3.h"
#include "ghwp-context-v3.h"
#include "hnc2unicode.h"

G_DEFINE_TYPE (GHWPFileV3, ghwp_file_v3, GHWP_TYPE_FILE);

GHWPFileV3 *ghwp_file_v3_new_from_uri (const gchar *uri, GError **error)
{
    g_return_val_if_fail (uri != NULL, NULL);

    gchar      *filename = g_filename_from_uri (uri, NULL, error);
    GHWPFileV3 *file     = ghwp_file_v3_new_from_filename (filename, error);
    g_free (filename);
    return file;
}

GHWPFileV3 *ghwp_file_v3_new_from_filename (const gchar *filename,
                                            GError     **error)
{
    g_return_val_if_fail (filename != NULL, NULL);
    GFile *file = g_file_new_for_path (filename);
    GFileInputStream *fis = g_file_read (file, NULL, error);
    g_object_unref(file);
    GHWPFileV3 *hwpv3file = g_object_new (GHWP_TYPE_FILE_V3, NULL);
    hwpv3file->priv->stream = G_INPUT_STREAM (fis);
    return hwpv3file;
}

gchar *ghwp_file_v3_get_hwp_version_string (GHWPFile *file)
{
    g_return_val_if_fail (GHWP_IS_FILE_V3 (file), NULL);

    return g_strdup_printf ("3.0.0.%d", GHWP_FILE_V3 (file)->rev);
}

void ghwp_file_v3_get_hwp_version (GHWPFile *file,
                                   guint8   *major_version,
                                   guint8   *minor_version,
                                   guint8   *micro_version,
                                   guint8   *extra_version)
{
    g_return_if_fail (GHWP_IS_FILE_V3 (file));

    if (major_version) *major_version = 3;
    if (minor_version) *minor_version = 0;
    if (micro_version) *micro_version = 0;
    if (extra_version) *extra_version = GHWP_FILE_V3 (file)->rev;
}

static void _ghwp_file_v3_parse_signature (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
    GInputStream *stream = GHWP_FILE_V3 (doc->file)->priv->stream;
    GHWPContextV3 *context = ghwp_context_v3_new (stream);
    gchar *signature = g_malloc(30);
    ghwp_context_v3_read (context, signature, 30);
    g_free (signature);
    g_object_unref (context);
}
#include <stdio.h>
static void _ghwp_file_v3_parse_doc_info (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
    GHWPFileV3 *file = GHWP_FILE_V3 (doc->file);
    /* 문서 정보 128 bytes */
    /* 암호 여부 */
    GInputStream *stream = file->priv->stream;
    GHWPContextV3 *context = ghwp_context_v3_new (stream);

    ghwp_context_v3_skip (context, 96);
    ghwp_context_v3_read_uint16 (context, &(file->is_crypt));

    /* offset: 124 압축 여부, 0이면 비압축 그외 압축 */
    ghwp_context_v3_skip (context, 26);
    ghwp_context_v3_read_uint8 (context, &(file->is_compress));
    /* sub revision */
    ghwp_context_v3_read_uint8 (context, &(file->rev));
    /* 정보 블럭 길이 */
    ghwp_context_v3_read_uint16 (context, &(file->info_block_len));
    g_object_unref (context);
}

static void _ghwp_file_v3_parse_summary_info (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
    GInputStream *stream = GHWP_FILE_V3 (doc->file)->priv->stream;
    GHWPContextV3 *context = ghwp_context_v3_new (stream);

    guint8 *buffer = g_malloc (112);
    gchar *str;
    guint16 c;
    guint8 count = 0;

    while (count < 112) {
        ghwp_context_v3_read_uint16 (context, &c);
        count += 2;
        str = hnchar_to_utf8 (c);
        if (str == NULL) {
            ghwp_context_v3_skip (context, 112 - count);
            break;
        } else {
            printf("%s", str);
        }
    }
    ghwp_context_v3_read (context, buffer, 112);
    ghwp_context_v3_read (context, buffer, 112);
    ghwp_context_v3_read (context, buffer, 112);
    g_free (buffer);

    buffer = g_malloc (112*2);
    ghwp_context_v3_read (context, buffer, 112*2);
    g_free (buffer);

    buffer = g_malloc (112*3);
    ghwp_context_v3_read (context, buffer, 112*3);
    g_free (buffer);

    g_object_unref (context);
}

static void _ghwp_file_v3_parse_info_block (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
    GInputStream *stream = GHWP_FILE_V3 (doc->file)->priv->stream;
    g_input_stream_skip (stream, GHWP_FILE_V3 (doc->file)->info_block_len, NULL, NULL);

    if (GHWP_FILE_V3 (doc->file)->is_compress) {
        GZlibDecompressor *zd;
        GInputStream      *cis;

        zd  = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_RAW);
        cis = g_converter_input_stream_new ((GInputStream*) stream,
                                            (GConverter*) zd);
        g_object_unref (GHWP_FILE_V3 (doc->file)->priv->stream);
        g_object_ref (cis);
        GHWP_FILE_V3 (doc->file)->priv->stream = G_INPUT_STREAM (cis);

        g_object_unref (zd);
    }
}

static void _ghwp_file_v3_parse_font_names (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
    guint16 n_fonts;
    int i = 0;
    guint8 *buffer = NULL;
    gsize bytes_read;
    GInputStream *stream = GHWP_FILE_V3 (doc->file)->priv->stream;
    for (i = 0; i < 7; i++) {
        g_input_stream_read_all (stream, &n_fonts, 2, &bytes_read, NULL, NULL);
        buffer = g_malloc (40 * n_fonts);
        g_input_stream_read_all (stream, buffer, 40 * n_fonts, &bytes_read, NULL, NULL);
        g_free (buffer);
    }
}

static void _ghwp_file_v3_parse_styles (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
    guint16 n_styles;
    guint8 *buffer = NULL;
    gsize bytes_read;
    GInputStream *stream = GHWP_FILE_V3 (doc->file)->priv->stream;
    g_input_stream_read_all (stream, &n_styles, 2, &bytes_read, NULL, NULL);
    buffer = g_malloc (n_styles * (20 + 31 + 187));
    g_input_stream_read_all (stream, buffer, n_styles * (20 + 31 + 187), &bytes_read, NULL, NULL);
    g_free (buffer);
}
static gboolean _ghwp_file_v3_parse_paragraph (GHWPDocument *doc);
static void _ghwp_file_v3_parse_paragraphs (GHWPDocument *doc)
{
    /* <문단 리스트> ::= <문단>+ <빈문단> */
    while(_ghwp_file_v3_parse_paragraph(doc)) {
    }
}

#include <stdio.h>

static gboolean _ghwp_file_v3_parse_paragraph (GHWPDocument *doc)
{
    g_return_val_if_fail (doc != NULL, FALSE);

    guint8 *buffer = NULL;
    GInputStream *stream = GHWP_FILE_V3 (doc->file)->priv->stream;

    /* 문단 정보 */
    guint8  prev_paragraph_shape;
    guint16 n_chars;
    guint16 n_lines;
    guint8  char_shape_included;

    guint8 flag;
    int i;
    gsize bytes_read;
    g_input_stream_read_all (stream, &prev_paragraph_shape, 1, &bytes_read, NULL, NULL);
    g_input_stream_read_all (stream, &n_chars, 2, &bytes_read, NULL, NULL);
    g_input_stream_read_all (stream, &n_lines, 2, &bytes_read, NULL, NULL);
    g_input_stream_read_all (stream, &char_shape_included, 1, &bytes_read, NULL, NULL);

    buffer = g_malloc (1 + 4 + 1 + 31);
    g_input_stream_read_all (stream, buffer, 1 + 4 + 1 + 31, &bytes_read, NULL, NULL);
    g_free (buffer);
    /* 여기까지 43 바이트 */

    if (prev_paragraph_shape == 0 && n_chars > 0) {
        buffer = g_malloc (187);
        g_input_stream_read_all (stream, buffer, 187, &bytes_read, NULL, NULL);
        g_free (buffer);
    }

    /* 빈문단이면 FALSE 반환 */
    if (n_chars == 0)
        return FALSE;

    /* 줄 정보 */
    buffer = g_malloc (n_lines * 14);
    g_input_stream_read_all (stream, buffer, n_lines * 14, &bytes_read, NULL, NULL);
    g_free (buffer);

    /* 글자 모양 정보 */
    if (char_shape_included != 0) {
        for (i = 0; i < n_chars; i++) {
            g_input_stream_read_all (stream, &flag, 1, &bytes_read, NULL, NULL);
            if (flag != 1) {
                buffer = g_malloc (31);
                g_input_stream_read_all (stream, buffer, 31, &bytes_read, NULL, NULL);
                g_free (buffer);
            }
        }
    }

    /* 글자들 */
    guint16 n_chars_read = 0;
    guint16 c;

    while (n_chars_read < n_chars) {
        g_input_stream_read_all (stream, &c, 2, &bytes_read, NULL, NULL);
        n_chars_read += 1;

        if (c == 6) {
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);

            buffer = g_malloc (34);
            g_input_stream_read_all (stream, buffer, 34, &bytes_read, NULL, NULL);
            g_free (buffer);
            continue;
        } else if (c == 9) { /* tab */
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);
            printf("\t");
            fflush(stdout);
            continue;
        } else if (c == 10) { /* table */
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);
            /* 테이블 식별 정보 84 바이트 */
            buffer = g_malloc (80);
            g_input_stream_read_all (stream, buffer, 80, &bytes_read, NULL, NULL);
            g_free (buffer);

            guint16 n_cells;
            g_input_stream_read_all (stream, &n_cells, 2, &bytes_read, NULL, NULL);

            buffer = g_malloc (2);
            g_input_stream_read_all (stream, buffer, 2, &bytes_read, NULL, NULL);
            g_free (buffer);

            buffer = g_malloc (27 * n_cells);
            g_input_stream_read_all (stream, buffer, 27 * n_cells, &bytes_read, NULL, NULL);
            g_free (buffer);

            /* <셀 문단 리스트>+ */
            for (i = 0; i < n_cells; i++) {
                /* <셀 문단 리스트> ::= <셀 문단>+ <빈문단> */
                while(_ghwp_file_v3_parse_paragraph(doc)) {
                }
            }

            /* <캡션 문단 리스트> ::= <캡션 문단>+ <빈문단> */
            while(_ghwp_file_v3_parse_paragraph(doc)) {
            }
            continue;
        } else if (c == 11) {
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);
            guint32 len;
            g_input_stream_read_all (stream, &len, 4, &bytes_read, NULL, NULL);
            buffer = g_malloc (344);
            g_input_stream_read_all (stream, buffer, 344, &bytes_read, NULL, NULL);
            g_free (buffer);
            buffer = g_malloc (len);
            g_input_stream_read_all (stream, buffer, len, &bytes_read, NULL, NULL);
            g_free (buffer);
            /* <캡션 문단 리스트> ::= <캡션 문단>+ <빈문단> */
            while(_ghwp_file_v3_parse_paragraph(doc)) {
            }
            continue;
        } else if (c == 13) { /* 글자들 끝 */
            printf("\n");
            fflush(stdout);
            continue;
        } else if (c == 16) {
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);
            buffer = g_malloc (10);
            g_input_stream_read_all (stream, buffer, 10, &bytes_read, NULL, NULL);
            g_free (buffer);
            /* <문단 리스트> ::= <문단>+ <빈문단> */
            while(_ghwp_file_v3_parse_paragraph(doc)) {
            }
            continue;
        } else if (c == 17) { /* 각주/미주 */
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);
            buffer = g_malloc (14);
            g_input_stream_read_all (stream, buffer, 14, &bytes_read, NULL, NULL);
            g_free (buffer);
            while(_ghwp_file_v3_parse_paragraph(doc)) {
            }
            continue;
        } else if (c == 18 || c == 19 || c == 20 || c == 21) {
            n_chars_read += 3;
            buffer = g_malloc (6);
            g_input_stream_read_all (stream, buffer, 6, &bytes_read, NULL, NULL);
            g_free (buffer);
            continue;
        } else if (c == 23) { /*글자 겹침 */
            n_chars_read += 4;
            buffer = g_malloc (8);
            g_input_stream_read_all (stream, buffer, 8, &bytes_read, NULL, NULL);
            g_free (buffer);
            continue;
        } else if (c == 24 || c == 25) {
            n_chars_read += 2;
            buffer = g_malloc (4);
            g_input_stream_read_all (stream, buffer, 4, &bytes_read, NULL, NULL);
            g_free (buffer);
            continue;
        } else if (c == 28) { /* 개요 모양/번호 */
            n_chars_read += 31;
            buffer = g_malloc (62);
            g_input_stream_read_all (stream, buffer, 62, &bytes_read, NULL, NULL);
            g_free (buffer);
            continue;
        } else if (c == 30 || c == 31) {
            n_chars_read += 1;
            buffer = g_malloc (2);
            g_input_stream_read_all (stream, buffer, 2, &bytes_read, NULL, NULL);
            g_free (buffer);
            continue;
        } else if (c >= 0x0020 && c <= 0xffff) {
            gchar *tmp = hnchar_to_utf8 (c);
            printf ("%s", tmp);
            g_free (tmp);
            continue;
        } else {
            g_warning ("special character: %04x", c);
        }
    }
    return TRUE;
}

static void _ghwp_file_v3_parse_supplementary_info_block1 (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
}

static void _ghwp_file_v3_parse_supplementary_info_block2 (GHWPDocument *doc)
{
    g_return_if_fail (doc != NULL);
}

static void _ghwp_file_v3_parse (GHWPDocument *doc, GError **error)
{
    g_return_if_fail (doc != NULL);

    _ghwp_file_v3_parse_signature (doc);
    _ghwp_file_v3_parse_doc_info (doc);
    _ghwp_file_v3_parse_summary_info (doc);
    _ghwp_file_v3_parse_info_block (doc);
    _ghwp_file_v3_parse_font_names (doc);
    _ghwp_file_v3_parse_styles (doc);
    _ghwp_file_v3_parse_paragraphs (doc);
    _ghwp_file_v3_parse_supplementary_info_block1 (doc);
    _ghwp_file_v3_parse_supplementary_info_block2 (doc);
}

GHWPDocument *ghwp_file_v3_get_document (GHWPFile *file, GError **error)
{
    g_return_val_if_fail (GHWP_IS_FILE_V3 (file), NULL);
    GHWPDocument *doc = ghwp_document_new();
    doc->file = GHWP_FILE(file);
    _ghwp_file_v3_parse (doc, error);
    return doc;
}

static void ghwp_file_v3_init (GHWPFileV3 *file)
{
    file->priv = G_TYPE_INSTANCE_GET_PRIVATE (file, GHWP_TYPE_FILE_V3,
                                                    GHWPFileV3Private);
}

static void ghwp_file_v3_finalize (GObject *object)
{
    GHWPFileV3 *file = GHWP_FILE_V3(object);
    g_object_unref (file->priv->stream);
    G_OBJECT_CLASS (ghwp_file_v3_parent_class)->finalize (object);
}

static void ghwp_file_v3_class_init (GHWPFileV3Class *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GHWPFileV3Private));
    GHWP_FILE_CLASS (klass)->get_document = ghwp_file_v3_get_document;
    GHWP_FILE_CLASS (klass)->get_hwp_version_string = ghwp_file_v3_get_hwp_version_string;
    GHWP_FILE_CLASS (klass)->get_hwp_version = ghwp_file_v3_get_hwp_version;
    object_class->finalize = ghwp_file_v3_finalize;
}
