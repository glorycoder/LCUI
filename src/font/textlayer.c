﻿/* ***************************************************************************
 * textlayer.c -- text bitmap layer processing module.
 *
 * Copyright (C) 2012-2016 by Liu Chao <lc-soft@live.cn>
 *
 * This file is part of the LCUI project, and may only be used, modified, and
 * distributed under the terms of the GPLv2.
 *
 * (GPLv2 is abbreviation of GNU General Public License Version 2)
 *
 * By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it fully.
 *
 * The LCUI project is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
 *
 * You should have received a copy of the GPLv2 along with this file. It is
 * usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
 * ****************************************************************************/

/* ****************************************************************************
 * textlayer.c -- 文本图层处理模块
 *
 * 版权所有 (C) 2012-2016 归属于 刘超 <lc-soft@live.cn>
 *
 * 这个文件是LCUI项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和发布。
 *
 * (GPLv2 是 GNU通用公共许可证第二版 的英文缩写)
 *
 * 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
 *
 * LCUI 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销性或特
 * 定用途的隐含担保，详情请参照GPLv2许可协议。
 *
 * 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在LICENSE.TXT文件中，如果
 * 没有，请查看：<http://www.gnu.org/licenses/>.
 * ****************************************************************************/

#include <stdlib.h>
#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include <LCUI/font.h>

 /** 文本添加类型 */
enum TextAddType {
	TAT_INSERT,	/**< 插入至插入点处 */
	TAT_APPEND	/**< 追加至文本末尾 */
};

#undef max
#define max(a, b) ((a) > (b) ? (a):(b))
#define TextRowList_AddNewRow(ROWLIST) TextRowList_InsertNewRow(ROWLIST, (ROWLIST)->length)
#define TextLayer_GetRow(layer, n) (n >= layer->rowlist.length) ? NULL:layer->rowlist.rows[n]

/* 根据对齐方式，计算文本行的起始X轴位置 */
static int TextLayer_GetRowStartX( LCUI_TextLayer layer, TextRow txtrow )
{
	int width;
	if( layer->fixed_width > 0 ) {
		width = layer->fixed_width;
	} else {
		width = layer->width;
	}
	switch( layer->text_align ) {
	case SV_CENTER: return (width - txtrow->width) / 2;
	case SV_RIGHT: return width - txtrow->width;
	case SV_LEFT:
	default: break;
	}
	return 0;
}

/** 获取文本行总数 */
int TextLayer_GetRowTotal( LCUI_TextLayer layer )
{
	return layer->rowlist.length;
}

/** 获取指定文本行的高度 */
int TextLayer_GetRowHeight( LCUI_TextLayer layer, int row )
{
        if( row >= layer->rowlist.length ) {
                return 0;
        }
        return layer->rowlist.rows[row]->height;
}

/** 获取指定文本行的文本长度 */
int TextLayer_GetRowTextLength( LCUI_TextLayer layer, int row )
{
        if( row >= layer->rowlist.length ) {
                return -1;
        }
        return layer->rowlist.rows[row]->length;
}

/** 添加 更新文本排版 的任务 */
void TextLayer_AddUpdateTypeset( LCUI_TextLayer layer, int start_row )
{
	if( start_row < layer->task.typeset_start_row ) {
		layer->task.typeset_start_row = start_row;
	}
	layer->task.update_typeset = TRUE;
}

static void TextRow_Init( TextRow txtrow )
{
	txtrow->width = 0;
	txtrow->height = 0;
	txtrow->length = 0;
	txtrow->string = NULL;
	txtrow->eol = EOL_NONE;
	txtrow->text_height = 0;
}

static void TextRow_Destroy( TextRow txtrow )
{
	int i;
	for( i=0; i<txtrow->length; ++i ) {
		if( txtrow->string[i] ) {
			free( txtrow->string[i] );
		}
	}
	txtrow->width = 0;
	txtrow->height = 0;
	txtrow->length = 0;
	txtrow->text_height = 0;
	if( txtrow->string ) {
		free( txtrow->string );
	}
	txtrow->string = NULL;
}

/** 向文本行列表中插入新的文本行 */
static TextRow TextRowList_InsertNewRow( TextRowList rowlist, int i_row )
{
	int i, size;
	TextRow txtrow, *txtrows;
	if( i_row > rowlist->length ) {
		i_row = rowlist->length;
	}
	++rowlist->length;
	size = sizeof( TextRow )*(rowlist->length + 1);
	txtrows = realloc( rowlist->rows, size );
	if( !txtrows ) {
		--rowlist->length;
		return NULL;
	}
	txtrows[rowlist->length] = NULL;
	txtrow = malloc( sizeof( TextRowRec ) );
	if( !txtrow ) {
		--rowlist->length;
		return NULL;
	}
	TextRow_Init( txtrow );
	for( i = rowlist->length - 1; i > i_row; --i ) {
		txtrows[i] = txtrows[i - 1];
	}
	txtrows[i_row] = txtrow;
	rowlist->rows = txtrows;
	return txtrow;
}

/** 从文本行列表中删除指定文本行 */
static int TextRowList_RemoveRow( TextRowList rowlist, int i_row )
{
	if( i_row < 0 || i_row >= rowlist->length ) {
		return -1;
	}
	TextRow_Destroy( rowlist->rows[i_row] );
	free( rowlist->rows[i_row] );
	for( ; i_row < rowlist->length - 1; ++i_row ) {
		rowlist->rows[i_row] = rowlist->rows[i_row + 1];
	}
	rowlist->rows[i_row] = NULL;
	--rowlist->length;
	return 0;
}

/** 更新文本行的尺寸 */
static void TextLayer_UpdateRowSize( LCUI_TextLayer layer, TextRow txtrow )
{
	int i;
	TextChar txtchar;
	txtrow->width = 0;
	txtrow->text_height = layer->text_style.pixel_size;
	for( i = 0; i < txtrow->length; ++i ) {
		txtchar = txtrow->string[i];
		if( !txtchar->bitmap ) {
			continue;
		}
		txtrow->width += txtchar->bitmap->advance.x;
		if( txtrow->text_height < txtchar->bitmap->advance.y ) {
			txtrow->text_height = txtchar->bitmap->advance.y;
		}
	}
	switch( layer->line_height.type ) {
	case SVT_SCALE:
		txtrow->height = txtrow->text_height * layer->line_height.scale;
		break;
	case SVT_PX:
		txtrow->height = layer->line_height.px;
		break;
	default:
		txtrow->height = txtrow->text_height * 11 / 10;
		break;
	}
}

/** 设置文本行的字符串长度 */
static int TextRow_SetLength( TextRow txtrow, int len )
{
	TextChar *txtstr;
	if( len < 0 ) {
		len = 0;
	}
	txtstr = realloc( txtrow->string, sizeof(TextChar)*(len + 1) );
	if( !txtstr ) {
		return -1;
	}
	txtstr[len] = NULL;
	txtrow->string = txtstr;
	txtrow->length = len;
	return 0;
}

/** 将字符数据直接插入至文本行 */
static int TextRow_Insert( TextRow txtrow, int ins_pos, TextChar txtchar )
{
	int i;
	if( ins_pos < 0 ) {
		ins_pos = txtrow->length + 1 + ins_pos;
		if( ins_pos < 0 ) {
			ins_pos = 0;
		}
	} else if( ins_pos > txtrow->length ) {
		ins_pos = txtrow->length;
	}
	TextRow_SetLength( txtrow, txtrow->length + 1 );
	for( i = txtrow->length - 1; i > ins_pos; --i ) {
		txtrow->string[i] = txtrow->string[i - 1];
	}
	txtrow->string[ins_pos] = txtchar;
	return 0;
}

/** 向文本行插入一个字符数据副本 */
static int TextRow_InsertCopy( TextRow txtrow, int ins_pos, TextChar txtchar )
{
	TextChar txtchar2;
	txtchar2 = malloc( sizeof(TextCharRec) );
	*txtchar2 = *txtchar;
	return TextRow_Insert( txtrow, ins_pos, txtchar2 );
}

/** 将文本行中的内容向左移动 */
static void TextRow_LeftMove( TextRow txtrow, int n )
{
	int i, j;
	if( n <= 0 ) {
		return;
	}
	if( n > txtrow->length ) {
		n = txtrow->length;
	}
	txtrow->length -= n;
	for( i=0,j=n; i<txtrow->length; ++i,++j ) {
		txtrow->string[i] = txtrow->string[j];
	}
}

/** 更新字体位图 */
static void TextChar_UpdateBitmap( TextChar ch, LCUI_TextStyle *style )
{
	int i = 0;
	int size = style->pixel_size;
	int *font_ids = style->font_ids;
	if( ch->style ) {
		if( ch->style->has_family ) {
			font_ids = ch->style->font_ids;
		}
		if( ch->style->has_pixel_size ) {
			size = ch->style->pixel_size;
		}
	}
	while( font_ids && font_ids[i] >= 0 ) {
		int ret = LCUIFont_GetBitmap( ch->char_code, font_ids[i],
					      size, &ch->bitmap );
		if( ret == 0 ) {
			return;
		}
		++i;
	}
	LCUIFont_GetBitmap( ch->char_code, -1, size, &ch->bitmap );
}

/** 新建文本图层 */
LCUI_TextLayer TextLayer_New(void)
{
	LCUI_TextLayer layer;
	layer = malloc( sizeof( LCUI_TextLayerRec ) );
	layer->width = 0;
	layer->length = 0;
	layer->offset_x = 0;
	layer->offset_y = 0;
	layer->insert_x = 0;
	layer->insert_y = 0;
	layer->max_width = 0;
	layer->max_height = 0;
	layer->fixed_width = 0;
	layer->fixed_height = 0;
	layer->new_offset_x = 0;
	layer->new_offset_y = 0;
	layer->rowlist.length = 0;
	layer->rowlist.rows = NULL;
	layer->text_align = SV_LEFT;
	layer->is_using_buffer = FALSE;
	layer->is_autowrap_mode = FALSE;
	layer->is_mulitiline_mode = FALSE;
	layer->is_using_style_tags = FALSE;
	layer->line_height.scale = 1.428f;
	layer->line_height.type = SVT_SCALE;
	TextStyle_Init( &layer->text_style );
	LinkedList_Init( &layer->style_cache );
	layer->task.typeset_start_row = 0;
	layer->task.update_typeset = 0;
	layer->task.update_bitmap = 0;
	layer->task.redraw_all = 0;
	Graph_Init( &layer->graph );
	LinkedList_Init( &layer->dirty_rect );
	layer->graph.color_type = COLOR_TYPE_ARGB;
	TextRowList_InsertNewRow( &layer->rowlist, 0 );
	return layer;
}

static void TextRowList_Destroy( TextRowList list )
{
	int row;
	for( row=0; row<list->length; ++row ) {
		TextRow_Destroy( list->rows[row] );
		list->rows[row] = NULL;
	}
	list->length = 0;
	if( list->rows ) {
		free( list->rows );
	}
	list->rows = NULL;
}

/** 销毁TextLayer */
void TextLayer_Destroy( LCUI_TextLayer layer )
{
	RectList_Clear( &layer->dirty_rect );
	Graph_Free( &layer->graph );
	TextRowList_Destroy( &layer->rowlist );
	free( layer );
}

/** 获取指定文本行中的文本段的矩形区域 */
static int TextLayer_GetRowRect( LCUI_TextLayer layer, int i_row,
				 int start_col, int end_col, LCUI_Rect *rect )
{
	int i;
	TextRow txtrow;
	if( i_row >= layer->rowlist.length ) {
		return -1;
	}
	/* 先计算在有效区域内的起始行的Y轴坐标 */
	rect->y = layer->offset_y;
	rect->x = layer->offset_x;
	for( i = 0; i < i_row; ++i ) {
		rect->y += layer->rowlist.rows[i]->height;
	}
	txtrow = layer->rowlist.rows[i_row];
	if( end_col < 0 || end_col >= txtrow->length ) {
		end_col = txtrow->length - 1;
	}
	rect->height = txtrow->height;
	rect->x += TextLayer_GetRowStartX( layer, txtrow );
	if( start_col == 0 && end_col == txtrow->length - 1 ) {
		rect->width = txtrow->width;
	} else {
		for( i = 0; i < start_col; ++i ) {
			if( !txtrow->string[i]->bitmap ) {
				continue;
			}
			rect->x += txtrow->string[i]->bitmap->advance.x;
		}
		rect->width = 0;
		for( i = start_col; i <= end_col && i < txtrow->length; ++i ) {
			if( !txtrow->string[i]->bitmap ) {
				continue;
			}
			rect->width += txtrow->string[i]->bitmap->advance.x;
		}
	}
	if( rect->width <= 0 || rect->height <= 0 ) {
		return 1;
	}
	return 0;
}

/** 标记指定文本行的矩形区域为无效 */
static void TextLayer_InvalidateRowRect( LCUI_TextLayer layer, int row,
					 int start, int end )
{
	LCUI_Rect rect;
	if( TextLayer_GetRowRect( layer, row, start, end, &rect ) == 0 ) {
		RectList_Add( &layer->dirty_rect, &rect );
	}
}

/** 标记指定范围内容的文本行的矩形为无效 */
void TextLayer_InvalidateRowsRect( LCUI_TextLayer layer,
				   int start_row, int end_row )
{
	int i, y;
	LCUI_Rect rect;

	if( end_row < 0 || end_row >= layer->rowlist.length ) {
		end_row = layer->rowlist.length - 1;
	}

	y = layer->offset_y;
	for( i = 0; i < layer->rowlist.length; ++i ) {
		y += layer->rowlist.rows[i]->height;
		if( i >= start_row && y >= 0 ) {
			y -= layer->rowlist.rows[i]->height;
			break;
		}
	}
	for( ; i <= end_row; ++i ) {
		TextLayer_GetRowRect( layer, i, 0, -1, &rect );
		RectList_Add( &layer->dirty_rect, &rect );
		y += layer->rowlist.rows[i]->height;
		if( y >= layer->max_height ) {
			break;
		}
	}
}

/** 设置插入点的行列坐标 */
void TextLayer_SetCaretPos( LCUI_TextLayer layer, int row, int col )
{
	if( row < 0 ) {
		row = 0;
	}
	else if( row >= layer->rowlist.length ) {
		if( layer->rowlist.length < 0 ) {
			row = 0;
		} else	{
			row = layer->rowlist.length-1;
		}
	}

	if( col < 0 ) {
		col = 0;
	}
	else if( layer->rowlist.length > 0 ) {
		if( col >= layer->rowlist.rows[row]->length ) {
			col = layer->rowlist.rows[row]->length;
		}
	} else {
		col = 0;
	}
	layer->insert_x = col;
	layer->insert_y = row;
}

/** 根据像素坐标设置文本光标的行列坐标 */
int TextLayer_SetCaretPosByPixelPos( LCUI_TextLayer layer, int x, int y )
{
	TextRow txtrow;
	int i, pixel_pos, ins_x, ins_y;
	for( pixel_pos = 0, i = 0; i < layer->rowlist.length; ++i ) {
		pixel_pos += layer->rowlist.rows[i]->height;;
		if( pixel_pos >= y ) {
			ins_y = i;
			break;
		}
	}
	if( i >= layer->rowlist.length ) {
		if( layer->rowlist.length > 0 ) {
			ins_y = layer->rowlist.length - 1;
		} else {
			layer->insert_x = 0;
			layer->insert_y = 0;
			return -1;
		}
	}
	txtrow = layer->rowlist.rows[ins_y];
	ins_x = txtrow->length;
	pixel_pos = TextLayer_GetRowStartX( layer, txtrow );
	for( i = 0; i < txtrow->length; ++i ) {
		TextChar txtchar;
		txtchar = txtrow->string[i];
		if( !txtchar->bitmap ) {
			continue;
		}
		pixel_pos += txtchar->bitmap->advance.x;
		/* 如果在当前字中心点的前面 */
		if( x <= pixel_pos - txtchar->bitmap->advance.x / 2 ) {
			ins_x = i;
			break;
		}
	}
	TextLayer_SetCaretPos( layer, ins_y, ins_x );
	return 0;
}

/** 获取指定行列的文字的像素坐标 */
int TextLayer_GetCharPixelPos( LCUI_TextLayer layer, int row,
			       int col, LCUI_Pos *pixel_pos )
{
	TextRow txtrow;
	int i, pixel_x = 0, pixel_y = 0;
	if( row < 0 || row >= layer->rowlist.length ) {
		return -1;
	}
	if( col < 0 ) {
		return -2;
	} else if( col > layer->rowlist.rows[row]->length ) {
		return -3;
	}
	/* 累加前几行的高度 */
	for( i = 0; i < row; ++i ) {
		pixel_y += layer->rowlist.rows[i]->height;
	}
	txtrow = layer->rowlist.rows[row];
	pixel_x = TextLayer_GetRowStartX( layer, txtrow );
	for( i = 0; i < col; ++i ) {
		if( !txtrow->string[i] ) {
			break;
		}
		if( !txtrow->string[i]->bitmap ) {
			continue;
		}
		pixel_x += txtrow->string[i]->bitmap->advance.x;
	}
	pixel_pos->x = pixel_x;
	pixel_pos->y = pixel_y;
	return 0;
}

/** 获取文本光标的像素坐标 */
int TextLayer_GetCaretPixelPos( LCUI_TextLayer layer, LCUI_Pos *pixel_pos )
{
	return TextLayer_GetCharPixelPos( layer, layer->insert_y,
					  layer->insert_x, pixel_pos );
}

/** 清空文本 */
void TextLayer_ClearText( LCUI_TextLayer layer )
{
	layer->length = 0;
	layer->insert_x = 0;
	layer->insert_y = 0;
	layer->width = 0;
	TextLayer_InvalidateRowsRect( layer, 0, -1 );
	TextRowList_Destroy( &layer->rowlist );
	LinkedList_Clear( &layer->style_cache, (FuncPtr)TextStyle_Destroy );
	TextRowList_InsertNewRow( &layer->rowlist, 0 );
	layer->task.redraw_all = TRUE;
}

/** 对文本行进行断行 */
static void TextLayer_BreakTextRow( LCUI_TextLayer layer, int i_row,
				    int col, EOLChar eol )
{
	int n;
	TextRow txtrow, next_txtrow;
	txtrow = layer->rowlist.rows[i_row];
	next_txtrow = TextRowList_InsertNewRow( &layer->rowlist, i_row + 1 );
	/* 将本行原有的行尾符转移至下一行 */
	next_txtrow->eol = txtrow->eol;
	txtrow->eol = eol;
	for( n = txtrow->length - 1; n >= col; --n ) {
		TextRow_Insert( next_txtrow, 0, txtrow->string[n] );
		txtrow->string[n] = NULL;
	}
	txtrow->length = col;
	TextLayer_UpdateRowSize( layer, txtrow );
	TextLayer_UpdateRowSize( layer, next_txtrow );
}

/** 对指定行的文本进行排版 */
static void TextLayer_TextRowTypeset( LCUI_TextLayer layer, int row )
{
	TextRow txtrow;
	TextChar txtchar;
	LCUI_BOOL not_autowrap;
	int col, row_width = 0;
	int max_width;
	if( layer->fixed_width > 0 ) {
		max_width = layer->fixed_width;
	} else {
		max_width = layer->max_width;
	}
	if( max_width <= 0 || !layer->is_autowrap_mode ||
	    (layer->is_autowrap_mode && !layer->is_mulitiline_mode) ) {
		not_autowrap = TRUE;
	} else {
		not_autowrap = FALSE;
	}
	txtrow = layer->rowlist.rows[row];
	for( col = 0; col < txtrow->length; ++col ) {
		txtchar = txtrow->string[col];
		if( !txtchar->bitmap ) {
			continue;
		}
		/* 累加行宽度 */
		row_width += txtchar->bitmap->advance.x;
		/* 如果是当前行的第一个字符，或者行宽度没有超过宽度限制 */
		if( not_autowrap || col < 1 || row_width <= max_width ) {
			continue;
		}
		TextLayer_BreakTextRow( layer, row, col, EOL_NONE );
		return;
	}
	TextLayer_UpdateRowSize( layer, txtrow );
	/* 如果本行有换行符，或者是最后一行 */
	if( txtrow->eol != EOL_NONE || row == layer->rowlist.length - 1 ) {
		return;
	}
	row_width = txtrow->width;
	/* 本行的文本宽度未达到限制宽度，需要将下行的文本转移至本行 */
	while( txtrow->eol == EOL_NONE ) {
		/* 获取下一行的指针 */
		TextRow next_txtrow = TextLayer_GetRow( layer, row + 1 );
		if( !next_txtrow ) {
			break;
		}
		for( col = 0; col < next_txtrow->length; ++col ) {
			txtchar = next_txtrow->string[col];
			/* 忽略无字体位图的文字 */
			if( !txtchar->bitmap ) {
				TextRow_Insert( txtrow, -1, txtchar );
				next_txtrow->string[col] = NULL;
				continue;
			}
			row_width += txtchar->bitmap->advance.x;
			/* 如果没有超过宽度限制 */
			if( not_autowrap || row_width <= max_width ) {
				TextRow_Insert( txtrow, -1, txtchar );
				next_txtrow->string[col] = NULL;
				continue;
			}
			/* 如果插入点在下一行 */
			if( layer->insert_y == row + 1 ) {
				/* 如果插入点处于被转移的几个文字中 */
				if( layer->insert_x < col ) {
					layer->insert_y = row;
					layer->insert_x += txtrow->length;
				} else {
					/* 否则，减去被转移的文字数 */
					layer->insert_x -= col;
				}
			}
			/* 将这一行剩余的文字向前移 */
			TextRow_LeftMove( next_txtrow, col );
			TextLayer_UpdateRowSize( layer, txtrow );
			return;
		}
		txtrow->eol = next_txtrow->eol;
		TextLayer_UpdateRowSize( layer, txtrow );
		TextLayer_InvalidateRowRect( layer, row, 0, -1 );
		TextLayer_InvalidateRowRect( layer, row + 1, 0, -1 );
		/* 删除这一行，因为这一行的内容已经转移至当前行 */
		TextRowList_RemoveRow( &layer->rowlist, row + 1 );
		/* 如果插入点当前行在后面 */
		if( layer->insert_y > row ) {
			--layer->insert_y;
		}
	}
}

/** 从指定行开始，对文本进行排版 */
static void TextLayer_TextTypeset( LCUI_TextLayer layer, int start_row )
{
	int row;
	/* 记录排版前各个文本行的矩形区域 */
	TextLayer_InvalidateRowsRect( layer, start_row, -1 );
	for( row = start_row; row < layer->rowlist.length; ++row ) {
		TextLayer_TextRowTypeset( layer, row );
	}
	/* 记录排版后各个文本行的矩形区域 */
	TextLayer_InvalidateRowsRect( layer, start_row, -1 );
}

/** 对文本进行预处理 */
static int TextLayer_ProcessText( LCUI_TextLayer layer, const wchar_t *wstr,
				  int add_type, LinkedList *tags )
{
	EOLChar eol;
	TextRow txtrow;
	TextCharRec txtchar;
	LinkedList tmp_tags;
	LCUI_TextStyle *style = NULL;
	const wchar_t *p_end, *p, *pp;
	int cur_col, cur_row, start_row, ins_x, ins_y;
	LCUI_BOOL is_tmp_tag_stack, need_typeset, rect_has_added;

	if( !wstr ) {
		return -1;
	}
	need_typeset = FALSE;
	rect_has_added = FALSE;
	is_tmp_tag_stack = FALSE;
	/* 如果是将文本追加至文本末尾 */
	if( add_type == TAT_APPEND ) {
		if( layer->rowlist.length > 0 ) {
			cur_row = layer->rowlist.length - 1;
		} else {
			cur_row = 0;
		}
		txtrow = TextLayer_GetRow( layer, cur_row );
		if( !txtrow ) {
			txtrow = TextRowList_AddNewRow( &layer->rowlist );
		}
		cur_col = txtrow->length;
	} else { /* 否则，是将文本插入至当前插入点 */
		cur_row = layer->insert_y;
		cur_col = layer->insert_x;
		txtrow = TextLayer_GetRow( layer, cur_row );
		if( !txtrow ) {
			txtrow = TextRowList_AddNewRow( &layer->rowlist );
		}
	}
	start_row = cur_row;
	ins_x = cur_col;
	ins_y = cur_row;
	/* 如果没有可用的标签栈，则使用临时的标签栈 */
	if( !tags ) {
		is_tmp_tag_stack = TRUE;
		StyleTags_Init( &tmp_tags );
		tags = &tmp_tags;
	}
	p_end = wstr + wcslen( wstr );
	for( p = wstr; p < p_end; ++p ) {
		/* 如果启用的样式标签支持，则处理样式的结束标签 */
		if( layer->is_using_style_tags ) {
			pp = StyleTags_ScanEndingTag( tags, p );
			if( pp ) {
				/* 抵消本次循环后的++p，以在下次循环时还能够在当前位置 */
				p = pp - 1;
				style = StyleTags_GetTextStyle( tags );
				LinkedList_Append( &layer->style_cache, style );
				continue;
			}
			pp = StyleTags_ScanBeginTag( tags, p );
			if( pp ) {
				p = pp - 1;
				style = StyleTags_GetTextStyle( tags );
				LinkedList_Append( &layer->style_cache, style );
				continue;
			}
		}

		if( *p == '\r' || *p == '\n' ) {
			/* 判断是哪一种换行模式 */
			if( *p == '\r' ) {
				if( p + 1 < p_end && *(p + 1) == '\n' ) {
					eol = EOL_CR_LF;
				} else {
					eol = EOL_CR;
				}
			} else {
				eol = EOL_LF;
			}
			/* 如果没有记录过文本行的矩形区域 */
			if( !rect_has_added ) {
				TextLayer_InvalidateRowsRect( layer, ins_y, -1 );
				rect_has_added = TRUE;
				start_row = ins_y;
			}
			/* 将当前行中的插入点为截点，进行断行 */
			TextLayer_BreakTextRow( layer, ins_y, ins_x, eol );
			layer->width = max( layer->width, txtrow->width );
			need_typeset = TRUE;
			++layer->length;
			ins_x = 0;
			++ins_y;
			txtrow = TextLayer_GetRow( layer, ins_y );
			continue;
		}
		txtchar.style = style;
		txtchar.char_code = *p;
		TextChar_UpdateBitmap( &txtchar, &layer->text_style );
		TextRow_InsertCopy( txtrow, ins_x, &txtchar );
		++layer->length;
		++ins_x;
	}
	/* 更新当前行的尺寸 */
	TextLayer_UpdateRowSize( layer, txtrow );
	layer->width = max( layer->width, txtrow->width );
	if( add_type == TAT_INSERT ) {
		layer->insert_x = ins_x;
		layer->insert_y = ins_y;
	}
	/* 若启用了自动换行模式，则标记需要重新对文本进行排版 */
	if( layer->is_autowrap_mode || need_typeset ) {
		TextLayer_AddUpdateTypeset( layer, cur_row );
	} else {
		TextLayer_InvalidateRowRect( layer, cur_row, 0, -1 );
	}
	/* 如果已经记录过文本行矩形区域 */
	if( rect_has_added ) {
		TextLayer_InvalidateRowsRect( layer, start_row, -1 );
		rect_has_added = TRUE;
	}
	/* 如果使用的是临时标签栈，则销毁它 */
	if( is_tmp_tag_stack ) {
		StyleTags_Clear( tags );
	}
	return 0;
}

/** 插入文本内容（宽字符版） */
int TextLayer_InsertTextW( LCUI_TextLayer layer, const wchar_t *wstr,
			   LinkedList *tag_stack )
{
	return TextLayer_ProcessText( layer, wstr, TAT_INSERT,
				      tag_stack );
}

/** 插入文本内容 */
int TextLayer_InsertTextA( LCUI_TextLayer layer, const char *str )
{
	return 0;
}

/** 插入文本内容（UTF-8版） */
int TextLayer_InsertText( LCUI_TextLayer layer, const char *utf8_str )
{
	return 0;
}

/** 追加文本内容（宽字符版） */
int TextLayer_AppendTextW( LCUI_TextLayer layer, const wchar_t *wstr,
			   LinkedList *tag_stack )
{
	return TextLayer_ProcessText( layer, wstr, TAT_APPEND, 
				      tag_stack );
}

/** 追加文本内容 */
int TextLayer_AppendTextA( LCUI_TextLayer layer, const char *ascii_text )
{
	return 0;
}

/** 追加文本内容（UTF-8版） */
int TextLayer_AppendText( LCUI_TextLayer layer, const char *utf8_text )
{
	return 0;
}

/** 设置文本内容（宽字符版） */
int TextLayer_SetTextW( LCUI_TextLayer layer, const wchar_t *wstr,
			LinkedList *tag_stack )
{
	TextLayer_ClearText( layer );
	return TextLayer_AppendTextW( layer, wstr, tag_stack );
}

/** 设置文本内容 */
int TextLayer_SetTextA( LCUI_TextLayer layer, const char *ascii_text )
{
	return 0;
}

/** 设置文本内容（UTF-8版） */
int TextLayer_SetText( LCUI_TextLayer layer, const char *utf8_text )
{
	return 0;
}

/** 获取文本图层中的文本（宽字符版） */
int TextLayer_GetTextW( LCUI_TextLayer layer, int start_pos,
			int max_len, wchar_t *wstr_buff )
{
	int row, col = 0, i;
	TextRow row_ptr;

	if( start_pos < 0 ) {
		return -1;
	}
	if( max_len <= 0 ) {
		return 0;
	}
	/* 先根据一维坐标计算行列坐标 */
	for( i = 0, row = 0; row < layer->rowlist.length; ++row ) {
		if( i >= start_pos ) {
			col = start_pos - i;
			break;
		}
		i += layer->rowlist.rows[row]->length;
	}
	for( i = 0; row < layer->rowlist.length && i < max_len; ++row ) {
		row_ptr = layer->rowlist.rows[row];
		for( ; col < row_ptr->length && i < max_len; ++col, ++i ) {
			wstr_buff[i] = row_ptr->string[col]->char_code;
		}
	}
	wstr_buff[i] = L'\0';
	return i;
}

LCUI_Graph* TextLayer_GetGraphBuffer( LCUI_TextLayer layer )
{
	if( layer->is_using_buffer ) {
		return &layer->graph;
	}
	return NULL;
}

int TextLayer_GetWidth( LCUI_TextLayer layer )
{
	int i, row, w, max_w;
	TextRow txtrow;

	for( row = 0, max_w = 0; row < layer->rowlist.length; ++row ) {
		txtrow = layer->rowlist.rows[row];
		for( i = 0, w = 0; i < txtrow->length; ++i ) {
			if( !txtrow->string[i]->bitmap ||
			    !txtrow->string[i]->bitmap->buffer ) {
				continue;
			}
			w += txtrow->string[i]->bitmap->advance.x;
		}
		if( w > max_w ) {
			max_w = w;
		}
	}
	return max_w;
}

int TextLayer_GetHeight( LCUI_TextLayer layer )
{
	int i, h;
	for( i = 0, h = 0; i < layer->rowlist.length; ++i ) {
		h += layer->rowlist.rows[i]->height;
	}
	return h;
}

int TextLayer_SetFixedSize( LCUI_TextLayer layer, int width, int height )
{
	layer->fixed_width = width;
	layer->fixed_height = height;
	if( layer->is_using_buffer ) {
		Graph_Create( &layer->graph, width, height );
	}
	layer->task.redraw_all = TRUE;
	if( layer->is_autowrap_mode ) {
		layer->task.typeset_start_row = 0;
		layer->task.update_typeset = TRUE;
	}
	return 0;
}

int TextLayer_SetMaxSize( LCUI_TextLayer layer, int width, int height )
{
	layer->max_width = width;
	layer->max_height = height;
	if( layer->is_using_buffer ) {
		Graph_Create( &layer->graph, width, height );
	}
	layer->task.redraw_all = TRUE;
	if( layer->is_autowrap_mode ) {
		layer->task.typeset_start_row = 0;
		layer->task.update_typeset = TRUE;
	}
	return 0;
}

/** 设置是否启用多行文本模式 */
void TextLayer_SetMultiline( LCUI_TextLayer layer, int is_true )
{
	if( (layer->is_mulitiline_mode && !is_true)
	 || (!layer->is_mulitiline_mode && is_true) ) {
		layer->is_mulitiline_mode = is_true;
		TextLayer_AddUpdateTypeset( layer, 0 );;
	}
}

/** 删除指定行列的文字及其右边的文本 */
static int TextLayer_TextDeleteEx( LCUI_TextLayer layer, int char_y,
				   int char_x, int n_char )
{
	int end_x, end_y, i, j, len;
	TextRow txtrow, end_txtrow, prev_txtrow;

	if( char_x < 0 ) {
		char_x = 0;
	}
	if( char_y < 0 ) {
		char_y = 0;
	}
	if( n_char <= 0 ) {
		return -1;
	}
	if( char_y >= layer->rowlist.length ) {
		return -2;
	}
	txtrow = layer->rowlist.rows[char_y];
	if( char_x > txtrow->length ) {
		char_x = txtrow->length;
	}
	i = n_char;
	end_x = char_x;
	end_y = char_y;
	/* 计算结束点的位置 */
	for( ; end_y < layer->rowlist.length && n_char > 0; ++end_y ) {
		txtrow = layer->rowlist.rows[end_y];
		if( end_x + n_char <= txtrow->length ) {
			end_x += n_char;
			n_char = 0;
			break;
		}
		n_char -= (txtrow->length - end_x);
		if( txtrow->eol == EOL_NONE ) {
			end_x = 0;
		} else {
			n_char -= 1;
			end_x = 0;
		}
	}
	if( n_char >= 0 ) {
		layer->length -= i - n_char;
	} else {
		layer->length -= n_char;
	}
	if( end_y >= layer->rowlist.length ) {
		end_y = layer->rowlist.length - 1;
		end_txtrow = layer->rowlist.rows[end_y];
		end_x = end_txtrow->length;
	} else {
		end_txtrow = layer->rowlist.rows[end_y];
	}
	if( end_x > end_txtrow->length ) {
		end_x = end_txtrow->length;
	}
	if( end_x == char_x && end_y == char_y ) {
		return 0;
	}
	/* 获取上一行文本 */
	prev_txtrow = layer->rowlist.rows[char_y - 1];
	// 计算起始行与结束行拼接后的长度
	// 起始行：0 1 2 3 4 5，起点位置：2
	// 结束行：0 1 2 3 4 5，终点位置：4
	// 拼接后的长度：2 + 6 - 4 = 4
	len = char_x + end_txtrow->length - end_x;
	if( len < 0 ) {
		return -3;
	}
	/* 如果是同一行 */
	if( txtrow == end_txtrow ) {
		if( end_x > end_txtrow->length ) {
			return -4;
		}
		TextLayer_InvalidateRowRect( layer, char_y, char_x, -1 );
		TextLayer_AddUpdateTypeset( layer, char_y );
		for( i = char_x, j = end_x; j < txtrow->length; ++i, ++j ) {
			txtrow->string[i] = txtrow->string[j];
		}
		/* 如果当前行为空，也不是第一行，并且上一行没有结束符 */
		if( len <= 0 && end_y > 0 && prev_txtrow->eol != EOL_NONE ) {
			TextRowList_RemoveRow( &layer->rowlist, end_y );
		}
		/* 调整起始行的容量 */
		TextRow_SetLength( txtrow, len );
		/* 更新文本行的尺寸 */
		TextLayer_UpdateRowSize( layer, txtrow );
		return 0;
	}
	/* 如果结束点在行尾，并且该行不是最后一行 */
	if( end_x == end_txtrow->length && end_y < layer->rowlist.length - 1 ) {
		++end_y;
		end_txtrow = TextLayer_GetRow( layer, end_y );
		end_x = -1;
		len = char_x + end_txtrow->length;
	}
	TextRow_SetLength( txtrow, len );
	/* 标记当前行后面的所有行的矩形需区域需要刷新 */
	TextLayer_InvalidateRowsRect( layer, char_y + 1, -1 );
	/* 移除起始行与结束行之间的文本行 */
	for( i = char_y + 1, j = i; j < end_y; ++j ) {
		TextLayer_InvalidateRowRect( layer, i, 0, -1 );
		TextRowList_RemoveRow( &layer->rowlist, i );
	}
	i = char_x;
	j = end_x + 1;
	end_y = char_y + 1;
	/* 将结束行的内容拼接至起始行 */
	for( ; i < len && j < end_txtrow->length; ++i, ++j ) {
		txtrow->string[i] = end_txtrow->string[j];
	}
	TextLayer_UpdateRowSize( layer, txtrow );
	TextLayer_InvalidateRowRect( layer, end_y, 0, -1 );
	/* 移除结束行 */
	TextRowList_RemoveRow( &layer->rowlist, end_y );
	/* 如果起始行无内容，并且上一行没有结束符（换行符），则
	 * 说明需要删除起始行 */
	if( len <= 0 && char_y > 0 && prev_txtrow->eol != EOL_NONE ) {
		TextLayer_InvalidateRowRect( layer, char_y, 0, -1 );
		TextRowList_RemoveRow( &layer->rowlist, char_y );
	}
	TextLayer_AddUpdateTypeset( layer, char_y );;
	return 0;
}

/** 删除文本光标的当前坐标右边的文本 */
int TextLayer_TextDelete( LCUI_TextLayer layer, int n_char )
{
	return TextLayer_TextDeleteEx(	layer, layer->insert_y,
					layer->insert_x, n_char );
}

/** 退格删除文本，即删除文本光标的当前坐标左边的文本 */
int TextLayer_TextBackspace( LCUI_TextLayer layer, int n_char )
{
	int n_del;
	int char_x, char_y;
	TextRow txtrow;

	/* 先获取当前字的位置 */
	char_x = layer->insert_x;
	char_y = layer->insert_y;
	/* 再计算删除 n_char 个字后的位置 */
	for( n_del = n_char; char_y >= 0; --char_y ) {
		txtrow = layer->rowlist.rows[char_y];
		/* 如果不是当前行，则重定位至行尾 */
		if( char_y < layer->insert_y ) {
			char_x = txtrow->length;
			if( txtrow->eol == EOL_NONE ) {
				--char_x;
			}
		}
		if( char_x >= n_del ) {
			char_x = char_x - n_del;
			n_del = 0;
			break;
		}
		n_del = n_del - char_x - 1;
	}
	if( char_y < 0 || n_del == n_char ) {
		return -1;
	}
	/* 若能够被删除的字不够 n_char 个，则调整需删除的字数 */
	if( n_del > 0 ) {
		n_char -= n_del;
	}
	/* 开始删除文本 */
	TextLayer_TextDeleteEx( layer, char_y, char_x, n_char );
	/* 若最后一行被完全移除，则移动输入点至上一行的行尾处 */
	if( char_x == 0 && layer->rowlist.length > 0
	    && char_y >= layer->rowlist.length ) {
		char_y = layer->rowlist.length - 1;
		char_x = layer->rowlist.rows[char_y]->length;
	}
	/* 更新文本光标的位置 */
	TextLayer_SetCaretPos( layer, char_y, char_x );
	return 0;
}

/** 设置是否启用自动换行模式 */
void TextLayer_SetAutoWrap( LCUI_TextLayer layer, int is_true )
{
	if( (!layer->is_autowrap_mode && is_true)
	 || (layer->is_autowrap_mode && !is_true) ) {
		layer->is_autowrap_mode = is_true;
		TextLayer_AddUpdateTypeset( layer, 0 );
	}
}

/** 设置是否使用样式标签 */
void TextLayer_SetUsingStyleTags( LCUI_TextLayer layer, LCUI_BOOL is_true )
{
	layer->is_using_style_tags = is_true;
}

/** 重新载入各个文字的字体位图 */
void TextLayer_ReloadCharBitmap( LCUI_TextLayer layer )
{
	int row, col;
	for( row = 0; row < layer->rowlist.length; ++row ) {
		TextRow txtrow = layer->rowlist.rows[row];
		for( col = 0; col < txtrow->length; ++col ) {
			TextChar txtchar = txtrow->string[col];
			TextChar_UpdateBitmap( txtchar, &layer->text_style );
		}
		TextLayer_UpdateRowSize( layer, txtrow );
	}
}

void TextLayer_Update( LCUI_TextLayer layer, LinkedList *rects )
{
	if( layer->task.update_bitmap ) {
		TextLayer_InvalidateRowsRect( layer, 0, -1 );
		TextLayer_ReloadCharBitmap( layer );
		TextLayer_InvalidateRowsRect( layer, 0, -1 );
		layer->task.update_bitmap = FALSE;
		layer->task.redraw_all = TRUE;
	}
	if( layer->task.update_typeset ) {
		TextLayer_TextTypeset( layer, layer->task.typeset_start_row );
		layer->task.update_typeset = FALSE;
		layer->task.typeset_start_row = 0;
	}
	layer->width = TextLayer_GetWidth( layer );
	/* 如果坐标偏移量有变化，记录各个文本行区域 */
	if( layer->new_offset_x != layer->offset_x
	 || layer->new_offset_y != layer->offset_y ) {
		TextLayer_InvalidateRowsRect( layer, 0, -1 );
		layer->offset_x = layer->new_offset_x;
		layer->offset_y = layer->new_offset_y;
		TextLayer_InvalidateRowsRect( layer, 0, -1 );
		layer->task.redraw_all = TRUE;
	}
	if( rects ) {
		LinkedList_Concat( rects, &layer->dirty_rect );
	 }
}

int TextLayer_DrawToGraph( LCUI_TextLayer layer, LCUI_Rect area,
			   LCUI_Pos layer_pos, LCUI_Graph *graph )
{
	TextRow txtrow;
	TextChar txtchar;
	LCUI_Pos char_pos;
	int x, y, row, col, width, height;
	y = layer->offset_y;
	if( layer->fixed_width > 0 ) {
		width = layer->fixed_width;
	} else {
		width = layer->width;
	}
	if( layer->fixed_width > 0 ) {
		height = layer->fixed_width;
	} else {
		height = TextLayer_GetHeight( layer );
	}
	LCUIRect_ValidateArea( &area, width, height );
	for( row = 0; row < layer->rowlist.length; ++row ) {
		txtrow = TextLayer_GetRow( layer, row );
		y += txtrow->height;
		if( y > area.y ) {
			y -= txtrow->height;
			break;
		}
	}
	/* 如果没有可绘制的文本行 */
	if( row >= layer->rowlist.length ) {
		return -1;
	}
	for( ; row < layer->rowlist.length; ++row ) {
		txtrow = TextLayer_GetRow( layer, row );
		x = TextLayer_GetRowStartX( layer, txtrow );
		x += layer->offset_x;
		/* 确定从哪个文字开始绘制 */
		for( col = 0; col < txtrow->length; ++col ) {
			txtchar = txtrow->string[col];
			/* 忽略无字体位图的文字 */
			if( !txtchar->bitmap ) {
				continue;
			}
			x += txtchar->bitmap->advance.x;
			if( x > area.x ) {
				x -= txtchar->bitmap->advance.x;
				break;
			}
		}
		/* 若一整行的文本都不在可绘制区域内 */
		if( col >= txtrow->length ) {
			y += txtrow->height;
			continue;
		}
		/* 遍历该行的文字 */
		for( ; col < txtrow->length; ++col ) {
			txtchar = txtrow->string[col];
			if( !txtchar->bitmap ) {
				continue;
			}
			/* 计算字体位图的绘制坐标 */
			char_pos.x = layer_pos.x + x;
			char_pos.y = layer_pos.y + y;
			char_pos.x += txtchar->bitmap->left;
			char_pos.y += txtrow->text_height * 4 / 5;
			char_pos.y += (txtrow->height - txtrow->text_height) / 2;
			char_pos.y -= txtchar->bitmap->top;
			x += txtchar->bitmap->advance.x;
			/* 判断文字使用的前景颜色，再进行绘制 */
			if( txtchar->style && txtchar->style->has_fore_color ) {
				FontBitmap_Mix( graph, char_pos, txtchar->bitmap,
						txtchar->style->fore_color );
			} else {
				FontBitmap_Mix( graph, char_pos, txtchar->bitmap,
						layer->text_style.fore_color );
			}
			/* 如果超过绘制区域则不继续绘制该行文本 */
			if( x > area.x + area.width ) {
				break;
			}
		}
		y += txtrow->height;
		/* 超出绘制区域范围就不绘制了 */
		if( y > area.y + area.height ) {
			break;
		}
	}
	return 0;
}

/** 绘制文本 */
int TextLayer_Draw( LCUI_TextLayer layer )
{
	LCUI_Rect rect;
	LCUI_Pos pos = {0,0};

	/* 如果文本位图缓存无效 */
	if( layer->is_using_buffer && !Graph_IsValid( &layer->graph ) ) {
		return -1;
	}
	rect.x = 0;
	rect.y = 0;
	rect.w = layer->max_width;
	rect.h = layer->max_height;
	return TextLayer_DrawToGraph( layer, rect, pos, &layer->graph );
}

/** 清除已记录的无效矩形 */
void TextLayer_ClearInvalidRect( LCUI_TextLayer layer )
{
	LinkedListNode *node;
	LCUI_Graph invalid_graph;

	if( !layer->is_using_buffer ) {
		RectList_Clear( &layer->dirty_rect );
		return;
	}
	for( LinkedList_Each( node, &layer->dirty_rect ) ) {
		Graph_Quote( &invalid_graph, &layer->graph, node->data );
		Graph_FillAlpha( &invalid_graph, 0 );
	}
	RectList_Clear( &layer->dirty_rect );
}

/** 设置全局文本样式 */
void TextLayer_SetTextStyle( LCUI_TextLayer layer, LCUI_TextStyle *style )
{
	TextStyle_Destroy( &layer->text_style );
	TextStyle_Copy( &layer->text_style, style );
	layer->task.update_bitmap = TRUE;
}

/** 设置文本对齐方式 */
void TextLayer_SetTextAlign( LCUI_TextLayer layer, int align )
{
	layer->text_align = align;
	layer->task.update_typeset = TRUE;
	layer->task.typeset_start_row = 0;
}

/** 设置文本行的高度 */
void TextLayer_SetLineHeight( LCUI_TextLayer layer, LCUI_Style val )
{
	layer->line_height = *val;
	layer->task.update_typeset = TRUE;
	layer->task.typeset_start_row = 0;
}

void TextLayer_SetOffset( LCUI_TextLayer layer, int offset_x, int offset_y )
{
	layer->new_offset_x = offset_x;
	layer->new_offset_y = offset_y;
}
