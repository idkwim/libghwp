/*
 * ghwp-document.c
 *
 * Copyright (C) 2012  Hodong Kim <cogniti@gmail.com>
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
 * 한글과컴퓨터의 한/글 문서 파일(.hwp) 공개 문서를 참고하여 개발하였습니다.
 */

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <gsf/gsf-doc-meta-data.h>
#include <gio/gio.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-input-impl.h>
#include <cairo.h>

#include "ghwp-document.h"
#include "gsf-input-stream.h"
#include "ghwp-parse.h"
#include "ghwp-tags.h"

G_DEFINE_TYPE (GHWPDocument, ghwp_document, G_TYPE_OBJECT);
G_DEFINE_TYPE (TextSpan, text_span, G_TYPE_OBJECT);
G_DEFINE_TYPE (TextP, text_p, G_TYPE_OBJECT);

static void ghwp_document_parse (GHWPDocument* self);
static void ghwp_document_parse_doc_info (GHWPDocument* self);
static void ghwp_document_parse_body_text (GHWPDocument* self);
static void ghwp_document_parse_prv_text (GHWPDocument* self);
static void ghwp_document_parse_summary_info (GHWPDocument* self);
static gchar* ghwp_document_get_text_from_raw_data (GHWPDocument* self, guchar* raw, int raw_length1);
static void ghwp_document_make_pages (GHWPDocument* self);
static void ghwp_document_finalize (GObject* obj);

static void text_p_finalize (GObject* obj);

static void text_span_finalize (GObject* obj);

#define _g_array_free0(var) ((var == NULL) ? NULL : (var = (g_array_free (var, TRUE), NULL)))
#define _g_free0(var) (var = (g_free (var), NULL))
#define _g_object_unref0(var) ((var == NULL) ? NULL : (var = (g_object_unref (var), NULL)))
#define _g_error_free0(var) ((var == NULL) ? NULL : (var = (g_error_free (var), NULL)))

static gpointer _g_object_ref0 (gpointer self) {
	return self ? g_object_ref (self) : NULL;
}


void text_p_add_textspan (TextP* self, TextSpan* textspan) {
	GArray* _tmp0_;
	TextSpan* _tmp1_;
	TextSpan* _tmp2_;
	g_return_if_fail (self != NULL);
	g_return_if_fail (textspan != NULL);
	_tmp0_ = self->textspans;
	_tmp1_ = textspan;
	_tmp2_ = _g_object_ref0 (_tmp1_);
	g_array_append_val (_tmp0_, _tmp2_);
}


TextP* text_p_new (void)
{
	return (TextP*) g_object_new (TEXT_TYPE_P, NULL);
}


static void text_p_class_init (TextPClass * klass)
{
	text_p_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->finalize = text_p_finalize;
}


static void text_p_init (TextP * self)
{
	self->textspans = g_array_new (TRUE, TRUE, sizeof (TextSpan*));
}


static void text_p_finalize (GObject* obj)
{
	TextP * self = G_TYPE_CHECK_INSTANCE_CAST (obj, TEXT_TYPE_P, TextP);
	_g_array_free0 (self->textspans);
	G_OBJECT_CLASS (text_p_parent_class)->finalize (obj);
}


TextSpan* text_span_new (const gchar* text)
{
	g_return_val_if_fail (text != NULL, NULL);
	TextSpan * self = (TextSpan*) g_object_new (TEXT_TYPE_SPAN, NULL);
	_g_free0 (self->text);
	self->text = g_strdup (text);
	return self;
}


static void text_span_class_init (TextSpanClass * klass) {
	text_span_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->finalize = text_span_finalize;
}


static void text_span_init (TextSpan * self)
{
}


static void text_span_finalize (GObject* obj)
{
	TextSpan * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, TEXT_TYPE_SPAN, TextSpan);
	_g_free0 (self->text);
	G_OBJECT_CLASS (text_span_parent_class)->finalize (obj);
}


GHWPDocument* ghwp_document_new_from_uri (const gchar* uri, GError** error)
{
	GHWPDocument * self = NULL;
	const gchar* _tmp0_;
	GHWPFile* _tmp1_;
	GHWPFile* _tmp2_;
	GError * _inner_error_ = NULL;
	g_return_val_if_fail (uri != NULL, NULL);
	self = (GHWPDocument*) g_object_new (GHWP_TYPE_DOCUMENT, NULL);
	_tmp0_ = uri;
	_tmp1_ = ghwp_file_new_from_uri (_tmp0_, &_inner_error_);
	_tmp2_ = _tmp1_;
	if (_inner_error_ != NULL) {
		g_propagate_error (error, _inner_error_);
		_g_object_unref0 (self);
		return NULL;
	}
	_g_object_unref0 (self->ghwp_file);
	self->ghwp_file = _tmp2_;
	ghwp_document_parse (self);
	return self;
}


GHWPDocument*
ghwp_document_new_from_filename (const gchar* filename, GError** error)
{
	GHWPDocument * self = NULL;
	const gchar* _tmp0_;
	GHWPFile* _tmp1_;
	GHWPFile* _tmp2_;
	GError * _inner_error_ = NULL;
	g_return_val_if_fail (filename != NULL, NULL);
	self = (GHWPDocument*) g_object_new (GHWP_TYPE_DOCUMENT, NULL);
	_tmp0_ = filename;
	_tmp1_ = ghwp_file_new_from_filename (_tmp0_, &_inner_error_);
	_tmp2_ = _tmp1_;
	if (_inner_error_ != NULL) {
		g_propagate_error (error, _inner_error_);
		_g_object_unref0 (self);
		return NULL;
	}
	_g_object_unref0 (self->ghwp_file);
	self->ghwp_file = _tmp2_;
	ghwp_document_parse (self);
	return self;
}


static void ghwp_document_parse (GHWPDocument* self) {
	g_return_if_fail (self != NULL);
	ghwp_document_parse_doc_info (self);
	ghwp_document_parse_body_text (self);
	ghwp_document_parse_prv_text (self);
	ghwp_document_parse_summary_info (self);
}


guint ghwp_document_get_n_pages (GHWPDocument* self) {
    g_return_val_if_fail (self != NULL, 0U);
    return self->pages->len;
}


GHWPPage* ghwp_document_get_page (GHWPDocument* self, gint n_page)
{
	GHWPPage* result = NULL;
	GArray* _tmp0_;
	gint _tmp1_;
	GHWPPage* _tmp2_ = NULL;
	GHWPPage* _tmp3_;
	g_return_val_if_fail (self != NULL, NULL);
	_tmp0_ = self->pages;
	_tmp1_ = n_page;
	_tmp2_ = g_array_index (_tmp0_, GHWPPage*, (guint) _tmp1_);
	_tmp3_ = _g_object_ref0 (_tmp2_);
	result = _tmp3_;
	return result;
}


static void ghwp_document_parse_doc_info (GHWPDocument* self) {
	GHWPFile* _tmp0_;
	GInputStream* _tmp1_;
	GHWPContext* _tmp2_;
	GHWPContext* context;
	g_return_if_fail (self != NULL);
	_tmp0_ = self->ghwp_file;
	_tmp1_ = _tmp0_->doc_info_stream;
	_tmp2_ = ghwp_context_new (_tmp1_);
	context = _tmp2_;
	while (TRUE) {
		GHWPContext* _tmp3_;
		gboolean _tmp4_ = FALSE;
		_tmp3_ = context;
		_tmp4_ = ghwp_context_pull (_tmp3_);
		if (!_tmp4_) {
			break;
		}
	}
	_g_object_unref0 (context);
}


static gchar* g_unichar_to_string (gunichar self) {
	gchar* result = NULL;
	gchar* _tmp0_ = NULL;
	gchar* str;
	_tmp0_ = g_new0 (gchar, 7);
	str = (gchar*) _tmp0_;
	g_unichar_to_utf8 (self, str);
	result = str;
	return result;
}


static gchar* ghwp_document_get_text_from_raw_data (GHWPDocument* self, guchar* raw, int raw_length1) {
	gchar* result = NULL;
	gunichar ch = 0U;
	gchar* _tmp0_;
	gchar* text;
	g_return_val_if_fail (self != NULL, NULL);
	_tmp0_ = g_strdup ("");
	text = _tmp0_;
	{
		gint i;
		i = 0;
		{
			gboolean _tmp1_;
			_tmp1_ = TRUE;
			while (TRUE) {
				gboolean _tmp2_;
				gint _tmp4_;
				gint _tmp5__length1;
				guchar* _tmp6_;
				gint _tmp7_;
				guchar _tmp8_;
				guchar* _tmp9_;
				gint _tmp10_;
				guchar _tmp11_;
				gunichar _tmp12_;
				_tmp2_ = _tmp1_;
				if (!_tmp2_) {
					gint _tmp3_;
					_tmp3_ = i;
					i = _tmp3_ + 2;
				}
				_tmp1_ = FALSE;
				_tmp4_ = i;
				_tmp5__length1 = raw_length1;
				if (!(_tmp4_ < _tmp5__length1)) {
					break;
				}
				_tmp6_ = raw;
				_tmp7_ = i;
				_tmp8_ = _tmp6_[_tmp7_ + 1];
				_tmp9_ = raw;
				_tmp10_ = i;
				_tmp11_ = _tmp9_[_tmp10_];
				ch = (gunichar) ((_tmp8_ << 8) | _tmp11_);
				_tmp12_ = ch;
				switch (_tmp12_) {
					case 0:
					{
						break;
					}
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
					case 8:
					{
						gint _tmp13_;
						_tmp13_ = i;
						i = _tmp13_ + 14;
						break;
					}
					case 9:
					{
						gint _tmp14_;
						const gchar* _tmp15_;
						gunichar _tmp16_;
						gchar* _tmp17_ = NULL;
						gchar* _tmp18_;
						gchar* _tmp19_;
						_tmp14_ = i;
						i = _tmp14_ + 14;
						_tmp15_ = text;
						_tmp16_ = ch;
						_tmp17_ = g_unichar_to_string (_tmp16_);
						_tmp18_ = _tmp17_;
						_tmp19_ = g_strconcat (_tmp15_, _tmp18_, NULL);
						_g_free0 (text);
						text = _tmp19_;
						_g_free0 (_tmp18_);
						break;
					}
					case 10:
					{
						break;
					}
					case 11:
					case 12:
					{
						gint _tmp20_;
						_tmp20_ = i;
						i = _tmp20_ + 14;
						break;
					}
					case 13:
					{
						break;
					}
					case 14:
					case 15:
					case 16:
					case 17:
					case 18:
					case 19:
					case 20:
					case 21:
					case 22:
					case 23:
					{
						gint _tmp21_;
						_tmp21_ = i;
						i = _tmp21_ + 14;
						break;
					}
					case 24:
					case 25:
					case 26:
					case 27:
					case 28:
					case 29:
					case 30:
					case 31:
					{
						break;
					}
					default:
					{
						const gchar* _tmp22_;
						gunichar _tmp23_;
						gchar* _tmp24_ = NULL;
						gchar* _tmp25_;
						gchar* _tmp26_;
						_tmp22_ = text;
						_tmp23_ = ch;
						_tmp24_ = g_unichar_to_string (_tmp23_);
						_tmp25_ = _tmp24_;
						_tmp26_ = g_strconcat (_tmp22_, _tmp25_, NULL);
						_g_free0 (text);
						text = _tmp26_;
						_g_free0 (_tmp25_);
						break;
					}
				}
			}
		}
	}
	result = text;
	return result;
}


static void ghwp_document_parse_body_text (GHWPDocument* self) {
	guint curr_lv;
	guint prev_lv;
	g_return_if_fail (self != NULL);
	curr_lv = (guint) 0;
	prev_lv = (guint) 0;
	{
		guint index;
		index = (guint) 0;
		{
			gboolean _tmp0_;
			_tmp0_ = TRUE;
			while (TRUE) {
				gboolean _tmp1_;
				guint _tmp3_;
				GHWPFile* _tmp4_;
				GArray* _tmp5_;
				guint _tmp6_;
				GHWPFile* _tmp7_;
				GArray* _tmp8_;
				guint _tmp9_;
				GInputStream* _tmp10_ = NULL;
				GInputStream* _tmp11_;
				GInputStream* section_stream;
				GInputStream* _tmp12_;
				GHWPContext* _tmp13_;
				GHWPContext* context;
				_tmp1_ = _tmp0_;
				if (!_tmp1_) {
					guint _tmp2_;
					_tmp2_ = index;
					index = _tmp2_ + 1;
				}
				_tmp0_ = FALSE;
				_tmp3_ = index;
				_tmp4_ = self->ghwp_file;
				_tmp5_ = _tmp4_->section_streams;
				_tmp6_ = _tmp5_->len;
				if (!(_tmp3_ < _tmp6_)) {
					break;
				}
				_tmp7_ = self->ghwp_file;
				_tmp8_ = _tmp7_->section_streams;
				_tmp9_ = index;
				_tmp10_ = g_array_index (_tmp8_, GInputStream*, _tmp9_);
				_tmp11_ = _g_object_ref0 (_tmp10_);
				section_stream = _tmp11_;
				_tmp12_ = section_stream;
				_tmp13_ = ghwp_context_new (_tmp12_);
				context = _tmp13_;
				while (TRUE) {
					GHWPContext* _tmp14_;
					gboolean _tmp15_ = FALSE;
					GHWPContext* _tmp22_;
					guint16 _tmp23_;
					GHWPContext* _tmp24_;
					guint16 _tmp25_;
					guint _tmp60_;
					_tmp14_ = context;
					_tmp15_ = ghwp_context_pull (_tmp14_);
					if (!_tmp15_) {
						break;
					}
					{
						gint i;
						i = 0;
						{
							gboolean _tmp16_;
							_tmp16_ = TRUE;
							while (TRUE) {
								gboolean _tmp17_;
								gint _tmp19_;
								GHWPContext* _tmp20_;
								guint16 _tmp21_;
								_tmp17_ = _tmp16_;
								if (!_tmp17_) {
									gint _tmp18_;
									_tmp18_ = i;
									i = _tmp18_ + 1;
								}
								_tmp16_ = FALSE;
								_tmp19_ = i;
								_tmp20_ = context;
								_tmp21_ = _tmp20_->level;
								if (!(_tmp19_ < ((gint) _tmp21_))) {
									break;
								}
							}
						}
					}
					_tmp22_ = context;
					_tmp23_ = _tmp22_->level;
					curr_lv = (guint) _tmp23_;
					_tmp24_ = context;
					_tmp25_ = _tmp24_->tag_id;
					switch (_tmp25_) {
						case GHWP_TAG_PARA_HEADER:
						{
							guint _tmp26_;
							guint _tmp27_;
							_tmp26_ = curr_lv;
							_tmp27_ = prev_lv;
							if (_tmp26_ > _tmp27_) {
								GArray* _tmp28_;
								TextP* _tmp29_;
								_tmp28_ = self->office_text;
								_tmp29_ = text_p_new ();
								g_array_append_val (_tmp28_, _tmp29_);
							} else {
								guint _tmp30_;
								guint _tmp31_;
								_tmp30_ = curr_lv;
								_tmp31_ = prev_lv;
								if (_tmp30_ < _tmp31_) {
									GArray* _tmp32_;
									TextP* _tmp33_;
									_tmp32_ = self->office_text;
									_tmp33_ = text_p_new ();
									g_array_append_val (_tmp32_, _tmp33_);
								} else {
									guint _tmp34_;
									guint _tmp35_;
									_tmp34_ = curr_lv;
									_tmp35_ = prev_lv;
									if (_tmp34_ == _tmp35_) {
										GArray* _tmp36_;
										TextP* _tmp37_;
										_tmp36_ = self->office_text;
										_tmp37_ = text_p_new ();
										g_array_append_val (_tmp36_, _tmp37_);
									}
								}
							}
							break;
						}
						case GHWP_TAG_PARA_TEXT:
						{
							guint _tmp38_;
							guint _tmp39_;
							_tmp38_ = curr_lv;
							_tmp39_ = prev_lv;
							if (_tmp38_ > _tmp39_) {
								GArray* _tmp40_;
								GArray* _tmp41_;
								guint _tmp42_;
								TextP* _tmp43_ = NULL;
								TextP* _tmp44_;
								TextP* textp;
								GHWPContext* _tmp45_;
								guchar* _tmp46_;
								gint _tmp46__length1;
								gchar* _tmp47_ = NULL;
								gchar* text;
								TextP* _tmp48_;
								const gchar* _tmp49_;
								TextSpan* _tmp50_;
								TextSpan* _tmp51_;
								_tmp40_ = self->office_text;
								_tmp41_ = self->office_text;
								_tmp42_ = _tmp41_->len;
								_tmp43_ = g_array_index (_tmp40_, TextP*, _tmp42_ - 1);
								_tmp44_ = _g_object_ref0 (_tmp43_);
								textp = _tmp44_;
								_tmp45_ = context;
								_tmp46_ = _tmp45_->data;
								_tmp46__length1 = _tmp45_->data_length1;
								_tmp47_ = ghwp_document_get_text_from_raw_data (self, _tmp46_, _tmp46__length1);
								text = _tmp47_;
								_tmp48_ = textp;
								_tmp49_ = text;
								_tmp50_ = text_span_new (_tmp49_);
								_tmp51_ = _tmp50_;
								text_p_add_textspan (G_TYPE_CHECK_INSTANCE_CAST (_tmp48_, TEXT_TYPE_P, TextP), _tmp51_);
								_g_object_unref0 (_tmp51_);
								_g_free0 (text);
								_g_object_unref0 (textp);
							} else {
								guint _tmp52_;
								guint _tmp53_;
								_tmp52_ = curr_lv;
								_tmp53_ = prev_lv;
								if (_tmp52_ < _tmp53_) {
								} else {
									guint _tmp54_;
									guint _tmp55_;
									_tmp54_ = curr_lv;
									_tmp55_ = prev_lv;
									if (_tmp54_ == _tmp55_) {
									}
								}
							}
							break;
						}
						case GHWP_TAG_PARA_CHAR_SHAPE:
						{
							break;
						}
						case GHWP_TAG_PARA_LINE_SEG:
						{
							break;
						}
						case GHWP_TAG_CTRL_HEADER:
						{
							break;
						}
						case GHWP_TAG_PAGE_DEF:
						{
							break;
						}
						case GHWP_TAG_FOOTNOTE_SHAPE:
						{
							break;
						}
						case GHWP_TAG_PAGE_BORDER_FILL:
						{
							break;
						}
						case GHWP_TAG_LIST_HEADER:
						{
							break;
						}
						case GHWP_TAG_EQEDIT:
						{
							break;
						}
						default:
						{
							FILE* _tmp56_;
							GHWPContext* _tmp57_;
							guint16 _tmp58_;
							const gchar* _tmp59_;
							_tmp56_ = stderr;
							_tmp57_ = context;
							_tmp58_ = _tmp57_->tag_id;
							_tmp59_ = GHWP_TAG_NAMES[_tmp58_];
							fprintf (_tmp56_, "%s: not implemented\n", _tmp59_);
							break;
						}
					}
					_tmp60_ = curr_lv;
					prev_lv = _tmp60_;
				}
				_g_object_unref0 (context);
				_g_object_unref0 (section_stream);
			}
		}
	}
	ghwp_document_make_pages (self);
}


static void ghwp_document_make_pages (GHWPDocument* self) {
	gdouble y;
	guint len;
	GHWPPage* _tmp0_;
	GHWPPage* page;
	GArray* _tmp43_;
	GHWPPage* _tmp44_;
	GHWPPage* _tmp45_;
	g_return_if_fail (self != NULL);
	y = 0.0;
	len = (guint) 0;
	_tmp0_ = ghwp_page_new ();
	page = _tmp0_;
	{
		gint i;
		i = 0;
		{
			gboolean _tmp1_;
			_tmp1_ = TRUE;
			while (TRUE) {
				gboolean _tmp2_;
				gint _tmp4_;
				GArray* _tmp5_;
				guint _tmp6_;
				GArray* _tmp7_;
				gint _tmp8_;
				TextP* _tmp9_ = NULL;
				TextP* _tmp10_;
				TextP* textp;
				_tmp2_ = _tmp1_;
				if (!_tmp2_) {
					gint _tmp3_;
					_tmp3_ = i;
					i = _tmp3_ + 1;
				}
				_tmp1_ = FALSE;
				_tmp4_ = i;
				_tmp5_ = self->office_text;
				_tmp6_ = _tmp5_->len;
				if (!(((guint) _tmp4_) < _tmp6_)) {
					break;
				}
				_tmp7_ = self->office_text;
				_tmp8_ = i;
				_tmp9_ = g_array_index (_tmp7_, TextP*, (guint) _tmp8_);
				_tmp10_ = _g_object_ref0 (_tmp9_);
				textp = _tmp10_;
				{
					gint j;
					j = 0;
					{
						gboolean _tmp11_;
						_tmp11_ = TRUE;
						while (TRUE) {
							gboolean _tmp12_;
							gint _tmp14_;
							TextP* _tmp15_;
							GArray* _tmp16_;
							guint _tmp17_;
							TextP* _tmp18_;
							GArray* _tmp19_;
							gint _tmp20_;
							TextSpan* _tmp21_ = NULL;
							TextSpan* _tmp22_;
							TextSpan* textspan;
							TextSpan* _tmp23_;
							const gchar* _tmp24_;
							gint _tmp25_;
							gint _tmp26_;
							gdouble _tmp27_;
							guint _tmp28_;
							gdouble _tmp29_ = 0.0;
							gdouble _tmp30_;
							_tmp12_ = _tmp11_;
							if (!_tmp12_) {
								gint _tmp13_;
								_tmp13_ = j;
								j = _tmp13_ + 1;
							}
							_tmp11_ = FALSE;
							_tmp14_ = j;
							_tmp15_ = textp;
							_tmp16_ = _tmp15_->textspans;
							_tmp17_ = _tmp16_->len;
							if (!(((guint) _tmp14_) < _tmp17_)) {
								break;
							}
							_tmp18_ = textp;
							_tmp19_ = _tmp18_->textspans;
							_tmp20_ = j;
							_tmp21_ = g_array_index (_tmp19_, TextSpan*, (guint) _tmp20_);
							_tmp22_ = _g_object_ref0 (_tmp21_);
							textspan = _tmp22_;
							_tmp23_ = textspan;
							_tmp24_ = _tmp23_->text;
							_tmp25_ = strlen (_tmp24_);
							_tmp26_ = _tmp25_;
							len = (guint) _tmp26_;
							_tmp27_ = y;
							_tmp28_ = len;
							_tmp29_ = ceil (_tmp28_ / 100.0);
							y = _tmp27_ + (18.0 * _tmp29_);
							_tmp30_ = y;
							if (_tmp30_ > (842.0 - 80.0)) {
								GArray* _tmp31_;
								GHWPPage* _tmp32_;
								GHWPPage* _tmp33_;
								GHWPPage* _tmp34_;
								GHWPPage* _tmp35_;
								GArray* _tmp36_;
								TextSpan* _tmp37_;
								GObject* _tmp38_;
								_tmp31_ = self->pages;
								_tmp32_ = page;
								_tmp33_ = _g_object_ref0 (_tmp32_);
								g_array_append_val (_tmp31_, _tmp33_);
								_tmp34_ = ghwp_page_new ();
								_g_object_unref0 (page);
								page = _tmp34_;
								_tmp35_ = page;
								_tmp36_ = _tmp35_->elements;
								_tmp37_ = textspan;
								_tmp38_ = _g_object_ref0 ((GObject*) _tmp37_);
								g_array_append_val (_tmp36_, _tmp38_);
								y = 0.0;
							} else {
								GHWPPage* _tmp39_;
								GArray* _tmp40_;
								TextSpan* _tmp41_;
								GObject* _tmp42_;
								_tmp39_ = page;
								_tmp40_ = _tmp39_->elements;
								_tmp41_ = textspan;
								_tmp42_ = _g_object_ref0 ((GObject*) _tmp41_);
								g_array_append_val (_tmp40_, _tmp42_);
							}
							_g_object_unref0 (textspan);
						}
					}
				}
				_g_object_unref0 (textp);
			}
		}
	}
	_tmp43_ = self->pages;
	_tmp44_ = page;
	_tmp45_ = _g_object_ref0 (_tmp44_);
	g_array_append_val (_tmp43_, _tmp45_);
	_g_object_unref0 (page);
}


static void ghwp_document_parse_prv_text (GHWPDocument* self) {
	GHWPFile* _tmp0_;
	GInputStream* _tmp1_;
	GsfInputStream* _tmp2_;
	GsfInputStream* gis;
	GsfInputStream* _tmp3_;
	gssize _tmp4_ = 0L;
	gssize size;
	gssize _tmp5_;
	guchar* _tmp6_ = NULL;
	guchar* buf;
	gint buf_length1;
	GError * _inner_error_ = NULL;
	g_return_if_fail (self != NULL);
	_tmp0_ = self->ghwp_file;
	_tmp1_ = _tmp0_->prv_text_stream;
	_tmp2_ = _g_object_ref0 (G_TYPE_CHECK_INSTANCE_CAST (_tmp1_, GSF_TYPE_INPUT_STREAM, GsfInputStream));
	gis = _tmp2_;
	_tmp3_ = gis;
	_tmp4_ = gsf_input_stream_size (_tmp3_);
	size = _tmp4_;
	_tmp5_ = size;
	_tmp6_ = g_new0 (guchar, _tmp5_);
	buf = _tmp6_;
	buf_length1 = _tmp5_;
	{
		GsfInputStream* _tmp7_;
		guchar* _tmp8_;
		gint _tmp8__length1;
		guchar* _tmp9_;
		gssize _tmp10_;
		gchar* _tmp11_ = NULL;
		gchar* _tmp12_;
		_tmp7_ = gis;
		_tmp8_ = buf;
		_tmp8__length1 = buf_length1;
		g_input_stream_read ((GInputStream*) _tmp7_, _tmp8_, (gsize) _tmp8__length1, NULL, &_inner_error_);
		if (_inner_error_ != NULL) {
			goto __catch3_g_error;
		}
		_tmp9_ = buf;
		_tmp10_ = size;
		_tmp11_ = g_convert ((const gchar*) _tmp9_, (gssize) _tmp10_, "UTF-8", "UTF-16LE", NULL, NULL, &_inner_error_);
		_tmp12_ = _tmp11_;
		if (_inner_error_ != NULL) {
			goto __catch3_g_error;
		}
		_g_free0 (self->prv_text);
		self->prv_text = _tmp12_;
	}
	goto __finally3;
	__catch3_g_error:
	{
		GError* e = NULL;
		const gchar* _tmp13_;
		e = _inner_error_;
		_inner_error_ = NULL;
		_tmp13_ = e->message;
		g_error ("document.vala:266: %s", _tmp13_);
		_g_error_free0 (e);
	}
	__finally3:
	if (_inner_error_ != NULL) {
		buf = (g_free (buf), NULL);
		_g_object_unref0 (gis);
		g_critical ("file %s: line %d: uncaught error: %s (%s, %d)", __FILE__, __LINE__, _inner_error_->message, g_quark_to_string (_inner_error_->domain), _inner_error_->code);
		g_clear_error (&_inner_error_);
		return;
	}
	buf = (g_free (buf), NULL);
	_g_object_unref0 (gis);
}


static void ghwp_document_parse_summary_info (GHWPDocument* self) {
	GHWPFile* _tmp0_;
	GInputStream* _tmp1_;
	GsfInputStream* _tmp2_;
	GsfInputStream* gis;
	gssize _tmp3_ = 0L;
	gssize size;
	guchar* _tmp4_ = NULL;
	guchar* buf;
	gint buf_length1;
	guint8* _tmp6_ = NULL;
	guint8* component_guid;
	gint component_guid_length1;
	GsfInputMemory* _tmp7_;
	GsfInputMemory* summary;
	GsfDocMetaData* _tmp8_;
	GsfDocMetaData* meta;
	GsfDocMetaData* _tmp9_;
	GError * _inner_error_ = NULL;
	g_return_if_fail (self != NULL);
	_tmp0_ = self->ghwp_file;
	_tmp1_ = _tmp0_->summary_info_stream;
	_tmp2_ = _g_object_ref0 (G_TYPE_CHECK_INSTANCE_CAST (_tmp1_, GSF_TYPE_INPUT_STREAM, GsfInputStream));
	gis = _tmp2_;
	_tmp3_ = gsf_input_stream_size (gis);
	size = _tmp3_;
	_tmp4_ = g_new0 (guchar, size);
	buf = _tmp4_;
	buf_length1 = size;
	{
		g_input_stream_read ((GInputStream*) gis, buf, (gsize) buf_length1, NULL, &_inner_error_);
		if (_inner_error_ != NULL) {
			goto __catch4_g_error;
		}
	}
	goto __finally4;
	__catch4_g_error:
	{
		GError* e = NULL;
		const gchar* _tmp5_;
		e = _inner_error_;
		_inner_error_ = NULL;
		_tmp5_ = e->message;
		g_error ("document.vala:279: %s", _tmp5_);
		_g_error_free0 (e);
	}
	__finally4:
	if (_inner_error_ != NULL) {
		buf = (g_free (buf), NULL);
		_g_object_unref0 (gis);
		g_critical ("file %s: line %d: uncaught error: %s (%s, %d)", __FILE__, __LINE__, _inner_error_->message, g_quark_to_string (_inner_error_->domain), _inner_error_->code);
		g_clear_error (&_inner_error_);
		return;
	}
	_tmp6_ = g_new0 (guint8, 16);
	_tmp6_[0] = (guint8) 0xe0;
	_tmp6_[1] = (guint8) 0x85;
	_tmp6_[2] = (guint8) 0x9f;
	_tmp6_[3] = (guint8) 0xf2;
	_tmp6_[4] = (guint8) 0xf9;
	_tmp6_[5] = (guint8) 0x4f;
	_tmp6_[6] = (guint8) 0x68;
	_tmp6_[7] = (guint8) 0x10;
	_tmp6_[8] = (guint8) 0xab;
	_tmp6_[9] = (guint8) 0x91;
	_tmp6_[10] = (guint8) 0x08;
	_tmp6_[11] = (guint8) 0x00;
	_tmp6_[12] = (guint8) 0x2b;
	_tmp6_[13] = (guint8) 0x27;
	_tmp6_[14] = (guint8) 0xb3;
	_tmp6_[15] = (guint8) 0xd9;
	component_guid = _tmp6_;
	component_guid_length1 = 16;
	memcpy (buf + ((guchar) 28), component_guid, (gsize) component_guid_length1);
	_tmp7_ = (GsfInputMemory*) gsf_input_memory_new (buf, buf_length1, FALSE);
	summary = _tmp7_;
	_tmp8_ = gsf_doc_meta_data_new ();
	meta = _tmp8_;
	gsf_msole_metadata_read ((GsfInput*) summary, meta);
	_tmp9_ = _g_object_ref0 (meta);
	_g_object_unref0 (self->summary_info);
	self->summary_info = _tmp9_;
	_g_object_unref0 (meta);
	_g_object_unref0 (summary);
	component_guid = (g_free (component_guid), NULL);
	buf = (g_free (buf), NULL);
	_g_object_unref0 (gis);
}


GHWPDocument* ghwp_document_new (void)
{
	return (GHWPDocument*) g_object_new (GHWP_TYPE_DOCUMENT, NULL);
}


static void ghwp_document_class_init (GHWPDocumentClass * klass) {
	ghwp_document_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->finalize = ghwp_document_finalize;
}


static void ghwp_document_init (GHWPDocument * self) {
	GArray* _tmp0_;
	GArray* _tmp1_;
	_tmp0_ = g_array_new (TRUE, TRUE, sizeof (TextP*));
	self->office_text = _tmp0_;
	_tmp1_ = g_array_new (TRUE, TRUE, sizeof (GHWPPage*));
	self->pages = _tmp1_;
}


static void ghwp_document_finalize (GObject* obj) {
	GHWPDocument * self;
	self = G_TYPE_CHECK_INSTANCE_CAST (obj, GHWP_TYPE_DOCUMENT, GHWPDocument);
	_g_object_unref0 (self->ghwp_file);
	_g_free0 (self->prv_text);
	_g_array_free0 (self->office_text);
	_g_array_free0 (self->pages);
	_g_object_unref0 (self->summary_info);
	G_OBJECT_CLASS (ghwp_document_parent_class)->finalize (obj);
}