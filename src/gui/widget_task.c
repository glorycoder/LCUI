﻿/* ***************************************************************************
 * widget_task.c -- LCUI widget task module.
 *
 * Copyright (C) 2014-2015 by Liu Chao <lc-soft@live.cn>
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
 * ***************************************************************************/

/* ****************************************************************************
 * widget_task.c -- LCUI部件任务处理模块
 *
 * 版权所有 (C) 2014-2015 归属于 刘超 <lc-soft@live.cn>
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
 * ***************************************************************************/

/*
 * 任务数据主要用于存储执行任务前的数据，例如：“移动”任务，在添加该任务时会
 * 存储部件当前位置，供执行“移动”任务时计算脏矩形。
 * 同一帧下，同一种任务只存在一个，当在同一帧内对同一部件添加重复的任务时，部
 * 件数据只保留第一个，后面添加的任务数据都会被忽略，不会覆盖掉。
 */

#define __IN_WIDGET_TASK_SOURCE_FILE__

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/misc/rbtree.h>
#include <LCUI/widget_build.h>

#undef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))

struct LCUI_WidgetTaskBoxRec_ {
	int		i;				/**< 当前使用的记录号 */
	LCUI_BOOL	for_self;			/**< 标志，指示当前部件是否有待处理的任务 */
	LCUI_BOOL	for_children;			/**< 标志，指示是否有待处理的子级部件 */
	LCUI_BOOL	buffer[2][WTT_TOTAL_NUM];	/**< 两个记录缓存 */
};

static void HandleTopLevelWidgetEvent( LCUI_Widget w, int event_type )
{
	if( w->parent == LCUIRootWidget || w == LCUIRootWidget ) {
		int *n;
		LCUI_WidgetEvent e;

		n = (int*)&event_type;
		e.type_name = "TopLevelWidget";
		e.target = w;
		Widget_PostEvent( LCUIRootWidget, &e, *((int**)n) );
	}
}

/** 计算背景样式 */
static void ComputeBackgroundStyle( LCUI_StyleSheet ss, LCUI_Background *bg )
{
	LCUI_Style *style;
	int key = key_background_start + 1;

	for( ; key < key_background_end; ++key ) {
		style = &ss[key];
		if( !style->is_valid ) {
			continue;
		}
		switch( key ) {
		case key_background_color:
			bg->color = style->color;
			break;
		case key_background_image:
			if( !style->value_image ) {
				Graph_Init( &bg->image );
				break;
			}
			Graph_Quote( &bg->image, style->value_image, NULL );
			break;
		case key_background_position:
			bg->position.using_value = TRUE;
			bg->position.value = style->value_style;
			break;
		case key_background_position_x:
			bg->position.using_value = FALSE;
			bg->position.x.type = style->type;
			if( style->type == SVT_SCALE ) {
				bg->position.x.scale = style->value_scale;
			} else {
				bg->position.x.px = style->value;
			}
			break;
		case key_background_position_y:
			bg->position.using_value = FALSE;
			bg->position.x.type = style->type;
			if( style->type == SVT_SCALE ) {
				bg->position.x.scale = style->value_scale;
			} else {
				bg->position.x.px = style->value;
			}
			break;
		case key_background_size:
			bg->size.using_value = TRUE;
			bg->position.value = style->value_style;
			break;
		case key_background_size_width:
			bg->size.using_value = FALSE;
			bg->size.w.type = style->type;
			if( style->type == SVT_SCALE ) {
				bg->size.w.scale = style->value_scale;
			} else {
				bg->size.w.px = style->value;
			}
			break;
		case key_background_size_height:
			bg->size.using_value = FALSE;
			bg->size.h.type = style->type;
			if( style->type == SVT_SCALE ) {
				bg->size.h.scale = style->value_scale;
			} else {
				bg->size.h.px = style->value;
			}
			break;
		default: break;
		}
	}
}

/** 计算边框样式 */
static void ComputeBorderStyle( LCUI_StyleSheet ss, LCUI_Border *b )
{
	LCUI_Style *style;
	int key = key_border_start + 1;

	for( ; key < key_border_end; ++key ) {
		style = &ss[key];
		if( !style->is_valid ) {
			continue;
		}
		switch( key ) {
		case key_border_color:
			b->top.color = style->color;
			b->right.color = style->color;
			b->bottom.color = style->color;
			b->left.color = style->color;
			break;
		case key_border_style:
			b->top.style = style->value;
			b->right.style = style->value;
			b->bottom.style = style->value;
			b->left.style = style->value;
			break;
		case key_border_width:
			b->top.width = style->value;
			b->right.width = style->value;
			b->bottom.width = style->value;
			b->left.width = style->value;
			break;
		case key_border_top_color:
			b->top.color = style->color;
			break;
		case key_border_right_color:
			b->right.color = style->color;
			break;
		case key_border_bottom_color:
			b->bottom.color = style->color;
			break;
		case key_border_left_color:
			b->left.color = style->color;
			break;
		case key_border_top_width:
			b->top.width = style->value;
			break;
		case key_border_right_width:
			b->right.width = style->value;
			break;
		case key_border_bottom_width:
			b->bottom.width = style->value;
			break;
		case key_border_left_width:
			b->left.width = style->value;
			break;
		case key_border_top_style:
			b->top.style = style->value;
			break;
		case key_border_right_style:
			b->right.style = style->value;
			break;
		case key_border_bottom_style:
			b->bottom.style = style->value;
			break;
		case key_border_left_style:
			b->left.style = style->value;
			break;
		default: break;
		}
	}
}

/** 计算矩形阴影样式 */
static void ComputeBoxShadowStyle( LCUI_StyleSheet ss, LCUI_BoxShadow *bsd )
{
	LCUI_Style *style;
	int key = key_box_shadow_start + 1;

	for( ; key < key_box_shadow_end; ++key ) {
		style = &ss[key];
		if( !style->is_valid ) {
			continue;
		}
		switch( key ) {
		case key_box_shadow_x: bsd->x = style->value; break;
		case key_box_shadow_y: bsd->y = style->value; break;
		case key_box_shadow_spread: bsd->spread = style->value; break;
		case key_box_shadow_blur: bsd->blur = style->value; break;
		case key_box_shadow_color: bsd->color = style->color; break;
		default: break;
		}
	}
}

static void HandleRefreshStyle( LCUI_Widget w )
{
	Widget_Update( w, TRUE );
	w->task->buffer[w->task->i][WTT_UPDATE_STYLE] = FALSE;
}

static void HandleUpdateStyle( LCUI_Widget w )
{
	Widget_Update( w, FALSE );
}

/** 处理位置变化 */
static void HandlePosition( LCUI_Widget w )
{
	LCUI_Rect rect;
	rect = w->base.box.graph;
	Widget_ComputePosition( w );
	if( w->parent ) {
		/* 标记移动前后的区域 */
		Widget_InvalidateArea( w->parent, &w->base.box.graph, SV_CONTENT_BOX );
		Widget_InvalidateArea( w->parent, &rect, SV_CONTENT_BOX );
	}
	/* 检测是否为顶级部件并做相应处理 */
	HandleTopLevelWidgetEvent( w, WET_MOVE );
}

static void HandleSetTitle( LCUI_Widget w )
{
	HandleTopLevelWidgetEvent( w, WET_TITLE );
}

/** 处理尺寸调整 */
static void HandleResize( LCUI_Widget w )
{
	LCUI_Rect rect;
	
	rect = w->base.box.graph;
	/* 从样式表中获取尺寸 */
	w->style.width.type = w->css[key_width].type;
	if( w->style.width.type == SVT_SCALE ) {
		w->style.width.scale = w->css[key_width].value_scale;
	} else {
		w->style.width.px = w->css[key_width].value_px;
	}
	w->style.height.type = w->css[key_height].type;
	if( w->style.height.type == SVT_SCALE ) {
		w->style.height.scale = w->css[key_height].value_scale;
	} else {
		w->style.height.px = w->css[key_height].value_px;
	}
	Widget_ComputeSize( w );
	if( w->parent ) {
		Widget_InvalidateArea( w->parent, &rect, SV_CONTENT_BOX );
		rect.width = w->base.box.graph.width;
		rect.height = w->base.box.graph.height;
		Widget_InvalidateArea( w->parent, &rect, SV_CONTENT_BOX );
	}
	Widget_UpdateGraphBox( w );
	Widget_AddTask( w, WTT_REFRESH );
	HandleTopLevelWidgetEvent( w, WET_RESIZE );
}

/** 处理可见性 */
static void HandleVisibility( LCUI_Widget w )
{
	w->style.visible = w->base.css[key_visible].value_boolean;
	if( w->parent ) {
		Widget_InvalidateArea( w, NULL, SV_GRAPH_BOX );
	}
	_DEBUG_MSG("visible: %s\n", w->style.visible ? "TRUE":"FALSE");
	HandleTopLevelWidgetEvent( w, w->style.visible ? WET_SHOW:WET_HIDE );
}

/** 处理透明度 */
static void HandleOpacity( LCUI_Widget w )
{

}

/** 处理阴影（标记阴影区域为脏矩形，但不包括主体区域） */
static void HandleShadow( LCUI_Widget w )
{
	LCUI_BoxShadow bs;

	_DEBUG_MSG("update shadow\n");
	bs = w->style.shadow;
	ComputeBoxShadowStyle( w->base.css, &w->style.shadow );
	/* 如果阴影变化并未导致图层尺寸变化，则只重绘阴影 */
	if( bs.x == w->style.shadow.x && bs.y == w->style.shadow.y
	 && bs.spread == w->style.shadow.spread ) {
		LCUI_Rect rects[4];
		LCUIRect_CutFourRect( &w->base.box.border,
				      &w->base.box.graph, rects );
		Widget_InvalidateArea( w, &rects[0], SV_GRAPH_BOX );
		Widget_InvalidateArea( w, &rects[1], SV_GRAPH_BOX );
		Widget_InvalidateArea( w, &rects[2], SV_GRAPH_BOX );
		Widget_InvalidateArea( w, &rects[3], SV_GRAPH_BOX );
		return;
	}
	Widget_AddTask( w, WTT_RESIZE );
}

static void HandleBackground( LCUI_Widget w )
{
	ComputeBackgroundStyle( w->base.css, &w->style.background );
	Widget_AddTask( w, WTT_BODY );
}

/** 处理主体刷新（标记主体区域为脏矩形，但不包括阴影区域） */
static void HandleBody( LCUI_Widget w )
{
	_DEBUG_MSG( "body\n" );
	Widget_InvalidateArea( w, NULL, SV_BORDER_BOX );
}

/** 处理刷新（标记整个部件区域为脏矩形） */
static void HandleRefresh( LCUI_Widget w )
{
	_DEBUG_MSG( "refresh\n" );
	Widget_InvalidateArea( w, NULL, SV_GRAPH_BOX );
}

static void HandleBorder( LCUI_Widget w )
{
	LCUI_Rect rect;
	LCUI_Border ob, *nb;
	
	ob = w->style.border;
	ComputeBorderStyle( w->base.css, &w->style.border );
	nb = &w->style.border;
	/* 如果边框变化并未导致图层尺寸变化的话，则只重绘边框 */
	if( ob.top.width == nb->top.width
	 && ob.right.width == nb->right.width
	 && ob.bottom.width == nb->bottom.width
	 && ob.left.width == nb->left.width ) {
		rect.x = rect.y = 0;
		rect.width = w->base.box.border.width;
		rect.width -= max( ob.top_right_radius, ob.right.width );
		rect.height = max( ob.top_left_radius, ob.top.width );
		Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
		rect.x = w->base.box.border.w;
		rect.width = max( ob.top_right_radius, ob.right.width );
		rect.x -= rect.width;
		rect.height = w->base.box.border.height;
		rect.height -= max( ob.bottom_right_radius, ob.bottom.width );
		Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
		rect.x = max( ob.bottom_left_radius, ob.left.width );
		rect.width = w->base.box.border.width;
		rect.width -= rect.x;
		rect.height = max( ob.bottom_right_radius, ob.bottom.width );
		Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
		rect.width = rect.x;
		rect.x = 0;
		rect.height = w->base.box.border.height;
		rect.height -= max( ob.top_left_radius, ob.left.width );
		Widget_InvalidateArea( w, &rect, SV_BORDER_BOX );
		return;
	}
	/* 更新尺寸 */
	Widget_AddTask( w, WTT_RESIZE );
}

/** 处理销毁任务 */
static void HandleDestroy( LCUI_Widget w )
{
	/* 销毁先子部件，最后销毁自己 */
	// code ...
	/* 向父级部件添加相关任务 */
	// code ...
	LCUIMutex_Unlock( &w->mutex );
}

/** 更新当前任务状态，确保部件的任务能够被处理到 */
void Widget_UpdateTaskStatus( LCUI_Widget widget )
{
	int i;
	for( i=0; i<WTT_TOTAL_NUM && !widget->task->for_self; ++i ) {
		if( widget->task->buffer[i] ) {
			widget->task->for_self = TRUE;
		}
	}
	if( !widget->task->for_self ) {
		return;
	}
	widget = widget->parent;
	/* 向没有标记的父级部件添加标记 */
	while( widget && !widget->task->for_children ) {
		widget->task->for_children = TRUE;
		widget = widget->parent;
	}
}

/** 添加任务并扩散到子级部件 */
void Widget_AddTaskToSpread( LCUI_Widget widget, int task_type )
{
	LCUI_BOOL *buffer;
	LCUI_Widget child;

	buffer = widget->task->buffer[widget->task->i];
	buffer[task_type] = TRUE;
	widget->task->for_self = TRUE;
	widget->task->for_children = TRUE;
	LinkedList_ForEach( child, 0, &widget->children ) {
		Widget_AddTaskToSpread( child, task_type );
	}
}

/** 添加任务 */
void Widget_AddTask( LCUI_Widget widget, int task_type )
{
	LCUI_BOOL *buffer;
	buffer = widget->task->buffer[widget->task->i];
	if( buffer[task_type] ) {
		return;
	}
	buffer[task_type] = TRUE;
	widget->task->for_self = TRUE;
	DEBUG_MSG("widget: %p, parent_is_root: %d, for_childen: %d, task_id: %d\n",
	widget, widget->parent == LCUIRootWidget, widget->task->for_children, data->type);
	widget = widget->parent;
	/* 向没有标记的父级部件添加标记 */
	while( widget && !widget->task->for_children ) {
		widget->task->for_children = TRUE;
		widget = widget->parent;
		DEBUG_MSG("widget: %p\n", widget );
	}
}

typedef void (*callback)(LCUI_Widget);

static callback task_handlers[WTT_TOTAL_NUM];

/** 映射任务处理器 */
static void MapTaskHandler(void)
{
	task_handlers[WTT_DESTROY] = HandleDestroy;
	task_handlers[WTT_VISIBLE] = HandleVisibility;
	task_handlers[WTT_POSITION] = HandlePosition;
	task_handlers[WTT_RESIZE] = HandleResize;
	task_handlers[WTT_SHADOW] = HandleShadow;
	task_handlers[WTT_BORDER] = HandleBorder;
	task_handlers[WTT_OPACITY] = HandleOpacity;
	task_handlers[WTT_BODY] = HandleBody;
	task_handlers[WTT_TITLE] = HandleSetTitle;
	task_handlers[WTT_REFRESH] = HandleRefresh;
	task_handlers[WTT_UPDATE_STYLE] = HandleUpdateStyle;
	task_handlers[WTT_REFRESH_STYLE] = HandleRefreshStyle;
	task_handlers[WTT_BACKGROUND] = HandleBackground;
}

/** 初始化 LCUI 部件任务处理功能 */
void LCUIWidget_InitTask(void)
{
	MapTaskHandler();
}

/** 销毁（释放） LCUI 部件任务处理功能的相关资源 */
void LCUIWidget_ExitTask(void)
{

}

/** 初始化部件的任务处理 */
void Widget_InitTaskBox( LCUI_Widget widget )
{
	int i;
	widget->task = (LCUI_WidgetTaskBox)
	malloc(sizeof(struct LCUI_WidgetTaskBoxRec_));
	widget->task->for_children = FALSE;
	widget->task->for_self = FALSE;
	widget->task->i = 0;
	for( i=0; i<WTT_TOTAL_NUM; ++i ) {
		widget->task->buffer[0][i] = FALSE;
		widget->task->buffer[1][i] = FALSE;
	}
}

/** 销毁（释放）部件的任务处理功能的相关资源 */
void Widget_DestroyTaskBox( LCUI_Widget widget )
{
	free( widget->task );
	widget->task = NULL;
}

/** 处理部件的各种任务 */
static int Widget_ProcTask( LCUI_Widget w )
{
	int ret = 1, i;
	LCUI_BOOL *buffer;
	DEBUG_MSG("widget: %p, is_root: %d, for_self: %d, for_children: %d\n", w, w == LCUIRootWidget, w->task->for_self, w->task->for_children);

	/* 如果该部件有任务需要处理 */
	if( w->task->for_self ) {
		ret = LCUIMutex_TryLock( &w->mutex );
		if( ret != 0 ) {
			ret = 1;
			goto skip_proc_self_task;
		}
		w->task->for_self = FALSE;
		buffer = w->task->buffer[w->task->i];
		/* 切换前后台记录 */
		w->task->i = w->task->i == 1 ? 0:1;
		/* 如果该部件需要销毁，其它任务就不用再处理了 */
		if( buffer[WTT_DESTROY] ) {
			buffer[WTT_DESTROY] = FALSE;
			task_handlers[WTT_DESTROY]( w );
			return -1;
		}
		/* 如果有用户自定义任务 */
		if( buffer[WTT_USER] ) {
			LCUI_WidgetClass *wc;
			wc = LCUIWidget_GetClass( w->type );
			wc->task_handler( w );
		}
		for( i=0; i<WTT_USER; ++i ) {
			DEBUG_MSG( "task_id: %d, is_valid: %d\n", i, buffer[i].is_valid );
			if( buffer[i] && task_handlers[i] ) {
				buffer[i] = FALSE;
				task_handlers[i]( w );
			}
		}
		/* 只留了一个位置用于存放用户自定义任务，以后真有需求再改吧 */
		LCUIMutex_Unlock( &w->mutex );
		ret = 0;
	}

skip_proc_self_task:;

	/* 如果子级部件中有待处理的部件，则递归进去 */
	if( w->task->for_children ) {
		LCUI_Widget child;
		int i = 0;
		w->task->for_children = FALSE;
		LinkedList_ForEach( child, 0, &w->children ) {
			_DEBUG_MSG("%d: %p\n", i++, child);
			/* 如果该级部件的任务需要留到下次再处理 */
			if( Widget_ProcTask( child ) == 1 ) {
				w->task->for_children = TRUE;
			}
		}
	}
	return ret;
}

/** 处理一次当前积累的部件任务 */
void LCUIWidget_StepTask(void)
{
	Widget_ProcTask( LCUIRootWidget );
}
