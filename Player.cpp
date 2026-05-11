#include "Player.h"
#include "WeaponRenderer.h"
#pragma execution_character_set("utf-8")

// ============================================================================
// 构造函数：初始化玩家金钱、战绩、武器配置与回合状态
// ============================================================================
Player::Player() {
    money = 800; wins = 0; lossStreak = 0;       // 初始资金800，胜场和连败均为0
    heCount = 0; molotovCount = 0; smokeCount = 0; // 初始无投掷物
    initLoadout();                                // 初始化6个武器槽位
    resetRoundState(0.0, 0.0, true);              // 占位初始位置，等待购买阶段重新设置
}

// ============================================================================
// 初始化武器负载配置
// 定义全部6把武器的属性（名称/伤害/后坐力/弹速/射速/弹药/精度/价格/奖励等）
// 槽位0=手枪(默认解锁), 1=冲锋枪, 2=M4步枪, 3=霰弹枪, 4=AWP狙击, 5=匕首(默认解锁)
// ============================================================================
void Player::initLoadout() {
    weapons = {
        //  name        dmg  recoil  spd  cd  maxAmmo  curAmmo  mags  melee?  reload  spread  price  reward  unlocked  pellets
        {"P250手枪",  25.0,  3.0, 22.0, 15, 12, 12, 4, false, 45,  1.5,  300,  150, true,  1},  // 默认手枪，免费
        {"冲锋枪",    8.0,  1.5, 28.0,  4, 40, 40, 3, false, 75,  9.0, 1500, 300, false, 1},  // 高射速低伤害，移动精度差
        {"M4A1步枪",  22.0,  2.5, 35.0,  8, 30, 30, 3, false, 80,  2.5, 2700, 150, false, 1},  // 均衡型步枪，精度高
        {"霰弹枪",    12.0, 15.0, 25.0, 40,  8,  8, 4, false, 90, 18.0, 2000, 450, false, 6},  // 6发弹丸散布，近距离高伤
        {"AWP狙击", 100.0, 25.0, 2000.0, 90,  5,  5, 2, false, 180, 0.0, 4750,  50, false, 1},  // 一击必杀，零散布，昂贵
        {"军用匕首",  50.0,  0.0,  0.0, 25, 99, 99, 9, true,   0,  0.0,    0, 600, true,  1}   // 近战武器，无弹量限制
    };
}

// ============================================================================
// 重置回合状态
// 每回合开始时调用，恢复HP、弹药、位置，清除子弹和统计数据
// ============================================================================
void Player::resetRoundState(double startX, double startY, bool startFacingRight) {
    x = startX; y = startY; facingRight = startFacingRight;  // 设置重生位置和朝向
    vx = 0; vy = 0; hp = 100.0; weaponIndex = 0; cooldownTimer = 0; meleeTimer = 0;  // 重置物理和战斗状态
    isMoving = false; animFrame = 0; animTimer = 0;          // 重置动画状态
    bullets.clear(); stats.reset();                           // 清空弹道和统计数据
    // 补充弹药：AWP仅2个备用弹匣，其他武器4个
    for (auto& w : weapons) { w.currentAmmo = w.maxAmmo; w.magsLeft = (w.name == "AWP狙击") ? 2 : 4; }
}

// ============================================================================
// 更新行走动画
// 每6帧切换一次动画帧，在0~3之间循环，站立时归零
// ============================================================================
void Player::updateAnimation() {
    if (isMoving) {
        animTimer++;
        if (animTimer > 6) {         // 每6帧切换一次
            animTimer = 0;
            animFrame = (animFrame + 1) % 4;  // 0→1→2→3→0 循环
        }
    }
    else { animFrame = 0; animTimer = 0; }  // 站立时重置为初始帧
}

// ============================================================================
// 获取头部命中判定区域（多边形）
// 命中此处伤害倍率 x2.0，基于玩家当前位置和朝向进行坐标变换
// ============================================================================
QPolygonF Player::getHeadHitbox() const {
    QTransform t; t.translate(x, y);
    if (!facingRight) t.scale(-1, 1);  // 朝左时水平翻转判定区
    return t.map(QPolygonF(QRectF(-4, -38, 18, 18)));  // 头部区域：相对中心偏上
}

// ============================================================================
// 获取上半身命中判定区域（躯干）
// 命中此处伤害倍率 x1.0
// ============================================================================
QPolygonF Player::getUpperBodyHitbox() const {
    QTransform t; t.translate(x, y);
    if (!facingRight) t.scale(-1, 1);
    return t.map(QPolygonF(QRectF(-12, -22, 24, 24)));  // 躯干区域
}

// ============================================================================
// 获取下半身命中判定区域（腿部）
// 命中此处伤害倍率 x0.7
// ============================================================================
QPolygonF Player::getLowerBodyHitbox() const {
    QTransform t; t.translate(x, y);
    if (!facingRight) t.scale(-1, 1);
    return t.map(QPolygonF(QRectF(-12, -5, 24, 20)));  // 腿部区域
}

// ============================================================================
// 绘制伪3D玩家角色
// 包含：脚下阴影、双腿（含行走摆动）、躯干、防弹衣、头部、头饰、武器
// isP1=true 绘制CT防守方（深蓝灰配色），false绘制T进攻方（绿棕配色）
// ============================================================================
void Player::drawPseudo3D(QPainter& painter, bool isP1) const {
    painter.save();
    painter.translate(x, y);             // 将坐标原点移至玩家位置
    if (!facingRight) painter.scale(-1, 1);  // 朝左时水平镜像整个角色

    // ---- 脚下阴影（椭圆，提供立体感） ----
    painter.setBrush(QColor(40, 35, 30));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(-20, 10, 40, 12);

    // ---- 行走时上下浮动偏移 ----
    double bobY = (isMoving && (animFrame == 1 || animFrame == 3)) ? -2.0 : 0.0;

    // ---- 双腿位置计算（行走时交替前后摆动） ----
    double legBackX = -6, legFrontX = 2;  // 默认站立姿态
    if (isMoving) {
        if (animFrame == 1) { legBackX = -12; legFrontX = 8; }  // 左腿前右腿后
        else if (animFrame == 3) { legBackX = 2;  legFrontX = -12; } // 右腿前左腿后
    }

    // ---- 阵营配色定义 ----
    QColor p1Shirt(45, 55, 65);      // CT上衣：深蓝灰
    QColor p1Pants(35, 40, 45);      // CT裤子：深灰
    QColor p1Vest(20, 20, 20);       // CT防弹衣：黑
    QColor p2Shirt(130, 140, 100);   // T上衣：军绿
    QColor p2Pants(160, 130, 90);    // T裤子：卡其
    QColor p2Vest(80, 60, 40);       // T装具：深棕

    // ---- 绘制双腿 ----
    painter.setBrush(isP1 ? p1Pants : p2Pants);
    painter.drawRect(legBackX, -5, 8, 20);   // 后腿
    painter.drawRect(legFrontX, -5, 8, 20);  // 前腿
    // 鞋子
    painter.setBrush(QColor(20, 20, 20));
    painter.drawRect(legBackX + 2, 5, 6, 6);
    painter.drawRect(legFrontX + 2, 5, 6, 6);

    // ---- 绘制上半身（应用行走浮动） ----
    painter.translate(0, bobY);

    // 上衣
    painter.setBrush(isP1 ? p1Shirt : p2Shirt);
    painter.drawRect(-12, -22, 24, 24);
    // 防弹衣/战术背心
    painter.setBrush(isP1 ? p1Vest : p2Vest);
    painter.drawRect(-10, -20, 20, 16);
    // 弹匣袋
    painter.setBrush(QColor(40, 40, 40));
    painter.drawRect(-6, -14, 4, 8);
    painter.drawRect(2, -14, 4, 8);

    // ---- 头部（肤色） ----
    painter.setBrush(QColor(220, 180, 140));
    painter.drawRect(-4, -38, 16, 16);

    // ---- 头饰（阵营区分） ----
    if (isP1) {
        // CT：防弹头盔+护目镜
        painter.setBrush(QColor(30, 30, 30));
        painter.drawRect(-6, -40, 20, 8);        // 头盔顶部
        painter.setBrush(QColor(10, 10, 10));
        painter.drawRect(4, -36, 10, 4);         // 护目镜
        painter.setBrush(QColor(40, 40, 40));
        painter.drawRect(-4, -30, 16, 8);        // 面罩/头盔下沿
    }
    else {
        // T：头巾+墨镜
        painter.setBrush(QColor(180, 170, 150));
        painter.drawRect(-6, -40, 20, 6);        // 头巾
        painter.drawRect(-6, -34, 4, 12);        // 左侧垂布
        painter.setBrush(QColor(20, 20, 20));
        painter.drawRect(4, -34, 10, 3);         // 墨镜
        painter.setBrush(QColor(60, 40, 20));
        painter.drawRect(-4, -26, 16, 4);        // 面罩
    }

    // ---- 绘装配武器（委托给 WeaponRenderer） ----
    WeaponRenderer::drawEquippedWeapon(painter, weaponIndex, meleeTimer);

    painter.restore();
}