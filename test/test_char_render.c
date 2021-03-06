﻿#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/graph.h>
#include <LCUI/font.h>

int test_char_render( void )
{
	int ret, fid;
	LCUI_Graph img;
	LCUI_FontBitmap bmp;
	LCUI_Pos pos = {25, 25};

	/* 初始化字体处理功能 */
	LCUI_InitFont();

	/* 创建一个图像，并使用灰色填充 */
	Graph_Init( &img );
	Graph_Create( &img, 100, 100 );
	Graph_FillRect( &img, RGB( 240, 240, 240 ), NULL, FALSE );

	/* 载入字体文件 */
	ret = LCUIFont_LoadFile( "C:/Windows/fonts/simsun.ttc" );
	while( ret == 0 ) {
		/* 获取字体ID */
		fid = LCUIFont_GetId( "SimSun", NULL );
		if( fid < 0 ) {
			break;
		}
		/* 载入对应的文字位图，大小为 48 像素 */
		ret = FontBitmap_Load( &bmp, L'字', fid, 48 );
		if( ret != 0 ) {
			break;
		}
		/* 绘制红色文字到图像上 */
		FontBitmap_Mix( &img, pos, &bmp, RGB( 255, 0, 0 ) );
		Graph_WritePNG( "test_char_render.png", &img );
		/* 释放内存资源 */
		FontBitmap_Free( &bmp );
		Graph_Free( &img );
		break;
	}

	/* 退出字体处理功能 */
	LCUI_ExitFont();
	return ret;
}
