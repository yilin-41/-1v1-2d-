#pragma once
#include <QWidget>              // Qt窗口基类
#include <QKeyEvent>            // 键盘事件
#include <QTimer>               // 游戏循环定时器
#include <QPainter>             // 2D绘图
#include <QSet>                 // 按键状态集合
#include <QVector>              // 动态数组
#include <QRectF>               // 矩形（浮点精度）
#include <QPolygonF>            // 多边形（浮点精度）
#include <QEasingCurve>         // 缓动曲线（预留）
#include <QPropertyAnimation>   // 属性动画（预留）
#include "GameStructs.h"        // 游戏数据结构
#include "Player.h"             // 玩家类
#include "SoundManager.h"       // 音频管理器

// ============================================================================
// 游戏主窗口类
// 继承自QWidget，是整个游戏的核心控制器
// 负责：游戏循环更新、输入处理、物理模拟、碰撞检测、渲染绘制、状态管理
// ============================================================================
class GameWindow : public QWidget
{
    Q_OBJECT

public:
    GameWindow(QWidget* parent = nullptr);
    ~GameWindow();

protected:
    // Qt事件重写
    void paintEvent(QPaintEvent* event) override;       // 绘制事件（每帧渲染）
    void keyPressEvent(QKeyEvent* event) override;      // 按键按下事件
    void keyReleaseEvent(QKeyEvent* event) override;    // 按键释放事件

private slots:
    void updateGameLoop();  // 游戏主循环（由定时器每16ms触发，约60FPS）

private:
    // ---- 初始化与状态管理 ----
    void initNewMatch();                                    // 初始化新比赛（重置所有数据）
    void transitionTo(GameState nextState);                 // 触发场景切换淡出动画
    void applyStateTransition();                            // 应用场景切换（淡出完成后的实际切换）
    void handleRoundEnd(int winner);                        // 处理回合结束逻辑（1=P1胜, 2=P2胜, 0=平局）
    void handlePlayerCollision();                           // 处理两玩家之间的碰撞推开
    void playWeaponSound(const Weapon& w);                  // 根据武器名称播放对应开火音效
    void drawLoadingScreen(QPainter& painter);              // 绘制加载屏幕（已内联至paintEvent）

    // ---- 核心组件 ----
    QTimer* timer;                  // 游戏主循环定时器（16ms间隔 → 约60FPS）
    QSet<int> keysPressed;          // 当前按下的按键集合（用于持续移动检测）
    bool showHelp = false;          // 是否显示帮助信息（H键切换）

    // ---- 场景切换动画 ----
    double uiAlpha = 1.0;           // UI透明度（1.0=完全不透明, 0.0=全透明）
    bool isFadingOut = false;       // 是否正在淡出
    GameState targetState = START_SCREEN;  // 淡出后的目标状态

    // ---- 就绪状态 ----
    bool p1_ready = false;          // P1是否已准备（开始界面）
    bool p2_ready = false;          // P2是否已准备

    // ---- 游戏状态 ----
    GameState gameState = LOADING_SCREEN;  // 当前游戏状态
    int phaseTimer = 0;                     // 阶段计时器（购买/战斗/结算倒计时）
    int currentRound = 0;                   // 当前回合数
    static constexpr int WINS_NEEDED = 8;   // 获胜所需局数（抢8胜制）

    // ---- 画面抖动效果 ---- 【新增】
    double screenShakeIntensity = 0.0;  // 当前抖动强度（像素），每帧衰减，归零后停止抖动
    double shakeOffsetX = 0.0;          // 当前帧X轴抖动偏移量（随机生成）
    double shakeOffsetY = 0.0;          // 当前帧Y轴抖动偏移量（随机生成）

    // ---- 地图与特效实体 ----
    QVector<QRectF> obstacles;         // 地图障碍物列表（掩体）
    QVector<FlyingGrenade> flyingGrenades;  // 飞行中的投掷物
    QVector<FireZone> fireZones;       // 火焰区域
    QVector<SmokeZone> smokeZones;     // 烟雾区域
    QVector<Particle> particles;       // 粒子特效
    QVector<DroppedMag> droppedMags;   // 掉落弹匣
    QVector<Tracer> tracers;           // 弹道拖尾
    QVector<MuzzleFlash> muzzleFlashes; // 枪口闪光

    // ---- 物理常量 ----
    static constexpr double playerSize = 35.0;     // 玩家碰撞体积直径（像素）
    static constexpr double acceleration = 1.0;     // 移动加速度（像素/帧²）
    static constexpr double friction = 0.82;        // 摩擦力系数（每帧速度衰减）

    // ---- 玩家对象 ----
    Player p1;      // 玩家1（CT防守方，WASD移动，空格开火）
    Player p2;      // 玩家2（T进攻方，方向键移动，回车开火）

    // ---- 脚步声计数器 ----
    int p1FootstepCounter = 0;  // P1移动帧计数（每15帧触发一次脚步声）
    int p2FootstepCounter = 0;  // P2移动帧计数

    // ---- 加载动画相关 ----
    double loadingProgress = 0.0;       // 加载进度（0~100）
    int loadingPhase = 0;               // 加载阶段（预留）
    int loadingTimer = 0;               // 加载计时器
    double logoScale = 0.0;             // Logo缩放（预留）
    double logoAlpha = 0.0;             // Logo透明度（预留）
    QVector<Particle> loadingParticles; // 加载画面的背景粒子
};