#pragma once
#include <QPainter>
#include <QString>
#include "GameStructs.h"

// ============================================================================
// 武器渲染器（静态工具类）
// 负责绘制玩家手持武器和飞行中的投掷物
// 采用硬边像素风格，使用几何图形组合呈现各类武器外观
// ============================================================================
class WeaponRenderer {
public:
    // 绘制玩家当前装备的武器
    // @param painter     QPainter绘图对象（已平移到玩家坐标）
    // @param weaponIndex 武器槽位索引（0=P250, 1=冲锋枪, 2=M4, 3=霰弹, 4=AWP, 5=匕首）
    // @param meleeTimer  近战动画计时器（>0时绘制挥刀弧线特效）
    static void drawEquippedWeapon(QPainter& painter, int weaponIndex, int meleeTimer);

    // 绘制飞行中的投掷物图标（手雷/燃烧瓶/烟雾弹）
    // @param painter QPainter绘图对象（已平移到投掷物坐标）
    // @param type    投掷物类型（G_HE=破片雷, G_MOLOTOV=燃烧瓶, G_SMOKE=烟雾弹）
    static void drawFlyingGrenade(QPainter& painter, GrenadeType type);
};