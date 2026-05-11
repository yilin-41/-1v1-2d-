#pragma once
#include "GameStructs.h"
#include <QVector>
#include <QTransform>
#include <QPolygonF>
#include <QPainter>

// ============================================================================
// 玩家类
// 管理单个玩家的位置、状态、武器、子弹、统计数据及渲染
// ============================================================================
class Player {
public:
	Player();

	// ---- 核心属性 ----
	double x, y;            // 玩家中心坐标（像素）
	double vx, vy;          // 速度分量（像素/帧）
	double hp;              // 当前生命值

	// ---- 状态与动画 ----
	bool facingRight;       // 朝向：true=右, false=左
	bool isMoving;          // 是否正在移动（影响动画播放与脚步声）
	int animFrame;          // 当前动画帧（0~3，用于行走循环）
	int animTimer;          // 动画计时器（累计帧数，用于控制动画速度）

	// ---- 经济与战绩 ----
	int money;              // 当前持有金钱
	int wins;               // 累计获胜局数
	int lossStreak;         // 连败计数（用于累进经济补偿）
	int weaponIndex;        // 当前装备的武器槽位索引（0~5）
	int cooldownTimer;      // 冷却计时器（换弹/开火后的等待帧数）
	int meleeTimer;         // 近战动画计时器（控制挥刀特效显示时长）

	// ---- 投掷物库存 ----
	int heCount;            // 破片手雷持有数
	int molotovCount;       // 燃烧瓶持有数
	int smokeCount;         // 烟雾弹持有数
	static const int MAX_GRENADES = 3;  // 每种投掷物最大携带量

	// ---- 装备与弹道 ----
	QVector<Weapon> weapons;    // 武器库存（6个槽位，0=手枪,5=匕首始终解锁）
	QVector<Bullet> bullets;    // 该玩家发射的在飞子弹列表
	PlayerStats stats;          // 本回合统计数据

	// ---- 方法 ----
	void initLoadout();     // 初始化武器配置（设置6把武器的属性）
	void resetRoundState(double startX, double startY, bool startFacingRight);  // 重置回合状态（位置/HP/弹药等）
	void updateAnimation(); // 更新行走动画帧

	// 碰撞检测用命中判定区域（基于多边形，支持朝向翻转）
	QPolygonF getHeadHitbox() const;        // 头部判定区（伤害x2.0）
	QPolygonF getUpperBodyHitbox() const;   // 上半身判定区（伤害x1.0）
	QPolygonF getLowerBodyHitbox() const;   // 下半身判定区（伤害x0.7）

	// 绘制伪3D玩家角色（含身体、服装、武器）
	void drawPseudo3D(QPainter& painter, bool isP1) const;
};