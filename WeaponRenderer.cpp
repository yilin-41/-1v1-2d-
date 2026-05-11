#include "WeaponRenderer.h"
#include <QPolygonF>

#pragma execution_character_set("utf-8")

// ============================================================================
// 战术武器通用基础色板（硬边像素军事风格配色）
// ============================================================================
// 使用时在函数内部定义，避免全局变量污染

// ============================================================================
// 绘制玩家当前装备的武器
// 根据weaponIndex绘制对应武器，采用矩形/多边形组合的硬边像素美术风格
// 每种武器都有独特的视觉特征（机匣/护木/枪管/弹匣/瞄准镜等）
// ============================================================================
void WeaponRenderer::drawEquippedWeapon(QPainter& painter, int weaponIndex, int meleeTimer) {
    painter.save();

    // ---- 战术武器通用基础色板 ----
    QColor gunmetal(35, 38, 40);      // 枪灰色（机匣主体）
    QColor polymer(20, 20, 20);       // 战术塑料（握把、枪托）
    QColor oliveDrab(65, 75, 50);     // 游骑兵绿/橄榄褐（护木、枪托）
    QColor steel(90, 95, 100);        // 亮钢色（磨损处、枪管）
    QColor coyote(140, 115, 85);      // 狼棕色（握把缠绕、护木装饰）

    painter.setPen(Qt::NoPen);

    switch (weaponIndex) {
    case 5: // 军用匕首（战术直刀）
        // 近战挥动时的弧线特效
        if (meleeTimer > 0) {
            painter.setPen(QPen(QColor(255, 255, 255, 150), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawArc(15, -30, 50, 60, -30 * 16, 60 * 16);  // 挥砍轨迹弧线
        }
        painter.setPen(Qt::NoPen);
        // 伞绳缠绕握把（棕色）
        painter.setBrush(coyote);
        painter.drawRect(5, -14, 10, 5);
        // 哑光黑刀刃（枪灰色三角形）
        painter.setBrush(gunmetal);
        {
            QPolygonF blade;
            blade << QPointF(15, -14) << QPointF(32, -11) << QPointF(15, -9);
            painter.drawPolygon(blade);
        }
        // 开刃处亮银色硬边细节
        painter.setPen(QPen(steel, 1));
        painter.drawLine(15, -14, 32, -11);
        break;

    case 0: // P250手枪（紧凑型战术手枪）
        painter.setBrush(gunmetal);
        painter.drawRect(8, -16, 14, 5);  // 套筒
        painter.setBrush(polymer);
        painter.drawRect(10, -11, 5, 7);  // 握把
        // 抛壳窗与防滑纹细节
        painter.setPen(QPen(steel, 1));
        painter.drawLine(18, -16, 18, -12);  // 抛壳窗
        painter.drawLine(10, -15, 12, -15);  // 防滑纹
        break;

    case 1: // 冲锋枪（UMP45风格）
        painter.setBrush(polymer);
        painter.drawRect(5, -16, 24, 7);  // 聚合物机匣
        painter.setBrush(gunmetal);
        painter.drawRect(29, -15, 6, 2);  // 裸露枪管
        painter.setBrush(polymer);
        painter.drawRect(18, -9, 5, 8);   // 直弹匣
        // 弹匣防滑竖纹细节
        painter.setPen(QPen(Qt::black, 1));
        painter.drawLine(19, -9, 19, -2);
        painter.drawLine(21, -9, 21, -2);
        break;

    case 2: // M4A1步枪（带导轨与附件）
        painter.setBrush(polymer);
        painter.drawRect(-2, -15, 8, 6);  // 伸缩枪托
        painter.drawRect(6, -14, 2, 4);   // 缓冲管
        painter.setBrush(gunmetal);
        painter.drawRect(8, -16, 12, 7);  // 上下机匣
        painter.setBrush(coyote);
        painter.drawRect(20, -16, 14, 6); // 战术护木（狼棕色）
        painter.setBrush(steel);
        painter.drawRect(34, -14, 8, 2);  // 枪管与鸟笼消焰器
        painter.setBrush(polymer);
        painter.drawRect(12, -9, 4, 7);   // 弹匣
        // 护木导轨细节（竖线模拟皮卡汀尼导轨槽）
        painter.setPen(QPen(Qt::black, 1));
        for (int i = 22; i < 34; i += 3) painter.drawLine(i, -16, i, -14);
        break;

    case 3: // 霰弹枪（M4战术喷）
        painter.setBrush(polymer);
        painter.drawRect(2, -15, 8, 5);   // 聚合物枪托
        painter.setBrush(gunmetal);
        painter.drawRect(10, -16, 14, 7); // 机匣
        painter.drawRect(24, -15, 18, 3); // 长枪管
        painter.drawRect(24, -12, 14, 2); // 弹仓
        painter.setBrush(polymer);
        painter.drawRect(16, -13, 10, 5); // 泵动护木
        // 抛壳窗
        painter.setBrush(steel);
        painter.setPen(Qt::NoPen);
        painter.drawRect(18, -15, 4, 2);
        break;

    case 4: // AWP狙击枪（军用重型狙击）
        painter.setBrush(oliveDrab);
        painter.drawRect(-5, -16, 16, 8); // 枪托底板与后部
        painter.drawRect(11, -16, 18, 7); // 前护木
        painter.setBrush(gunmetal);
        painter.drawRect(29, -15, 28, 3); // 重型浮置枪管（长管）
        // 瞄准镜
        painter.setBrush(polymer);
        painter.drawRect(10, -22, 18, 5);
        // 镜片反光（硬边细节，浅蓝色模拟镀膜）
        painter.setBrush(QColor(100, 150, 200));
        painter.drawRect(26, -21, 2, 3);
        // 枪栓与弹匣
        painter.setBrush(steel);
        painter.drawRect(8, -16, 3, 3);   // 枪栓拉柄
        painter.setBrush(polymer);
        painter.drawRect(14, -9, 5, 5);   // 弹匣
        break;
    }
    painter.restore();
}

// ============================================================================
// 绘制飞行中的投掷物图标
// 根据type绘制对应的投掷物外观（手雷/燃烧瓶/烟雾弹）
// 采用硬边描边风格，每种投掷物有独特的形状和颜色
// ============================================================================
void WeaponRenderer::drawFlyingGrenade(QPainter& painter, GrenadeType type) {
    painter.save();
    painter.setPen(QPen(QColor(20, 20, 20), 1)); // 硬边描边

    if (type == G_HE) {
        // 破片手雷：军绿色圆柱体，带引信
        painter.setBrush(QColor(70, 80, 60));     // 军绿色
        painter.drawRect(-6, -8, 12, 16);         // 圆柱形弹体
        painter.setPen(QPen(Qt::black, 1));
        painter.drawLine(-6, 0, 6, 0);            // 弹体刻线
        painter.setBrush(QColor(40, 40, 40));
        painter.drawRect(-3, -12, 6, 4);          // 引信
    }
    else if (type == G_MOLOTOV) {
        // 燃烧瓶：暗绿色玻璃瓶，带燃烧布条和火焰
        painter.setBrush(QColor(40, 60, 30));     // 暗绿色玻璃瓶体
        painter.drawRect(-5, -6, 10, 16);
        painter.drawRect(-3, -14, 6, 8);          // 瓶颈
        painter.setBrush(QColor(220, 200, 180));
        painter.drawRect(-4, -16, 8, 5);          // 碎布条（浅色）
        painter.setBrush(QColor(255, 80, 0));
        painter.drawRect(-3, -20, 6, 4);          // 像素火焰（橙红色）
    }
    else if (type == G_SMOKE) {
        // 烟雾弹：银灰色罐体，带蓝色识别带
        painter.setBrush(QColor(160, 165, 160));  // 银灰色罐体
        painter.drawRect(-6, -10, 12, 20);
        painter.setBrush(QColor(50, 100, 150));   // 蓝色识别带
        painter.setPen(Qt::NoPen);
        painter.drawRect(-6, -2, 12, 6);
    }
    painter.restore();
}