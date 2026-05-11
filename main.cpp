#include "GameWindow.h"
#include <QtWidgets/QApplication>

// ============================================================================
// 应用程序入口
// 创建Qt应用实例和游戏窗口，进入事件循环
// ============================================================================
int main(int argc, char* argv[])
{
	QApplication app(argc, argv);  // 初始化Qt应用程序（管理事件循环和GUI资源）
	GameWindow window;             // 创建游戏主窗口（含初始化、音效、地图、游戏循环）
	window.show();                 // 显示窗口
	return app.exec();             // 进入Qt事件循环，阻塞直到窗口关闭
}