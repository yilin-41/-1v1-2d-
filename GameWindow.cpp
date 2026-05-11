#include "GameWindow.h"
#include "WeaponRenderer.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#pragma execution_character_set("utf-8")

// ============================================================================
// 构造函数：初始化窗口属性、音频、地图障碍物、加载动画、游戏循环
// ============================================================================
GameWindow::GameWindow(QWidget* parent) : QWidget(parent)
{
    std::srand(std::time(nullptr));                     // 初始化随机数种子
    this->setFixedSize(1400, 850);                      // 固定窗口尺寸
    this->setWindowTitle("反恐1v1 2d版 (按 H 帮助)");   // 窗口标题
    this->setWindowIcon(QIcon(":/GameWindow/res/app_icon.ico"));// 设置程序运行时左上角和任务栏的图标
    this->setFocusPolicy(Qt::StrongFocus);              // 接受键盘焦点

    // 1. 加载所有短促音效（枪声/脚步声/爆炸等14个WAV）
    SoundManager::instance().loadAll();

    // 2. 启动背景音乐（mp3文件支持多路径搜索）
    SoundManager::instance().playBGM("bgm.mp3");

    // ---- 初始化地图掩体（7个障碍物） ----
    obstacles = {
        QRectF(300, 200, 80, 250),   // 掩体1：左侧大型混凝土承重墙
        QRectF(1020, 400, 80, 250),  // 掩体2：右侧大型混凝土承重墙
        QRectF(600, 160, 200, 80),   // 掩体3：顶部横向金属集装箱
        QRectF(600, 610, 200, 80),   // 掩体4：底部横向金属集装箱
        QRectF(450, 400, 60, 60),    // 掩体5：木制战术物资箱
        QRectF(890, 390, 60, 60),    // 掩体6：木制战术物资箱
        QRectF(650, 410, 100, 40)    // 掩体7：中央低矮水泥防御墩
    };

    // ---- 初始化场景切换动画状态 ----
    uiAlpha = 1.0;
    isFadingOut = false;
    gameState = LOADING_SCREEN;
    loadingTimer = 0;
    loadingProgress = 0.0;

    // ---- 初始化比赛数据 ----
    initNewMatch();

    // ---- 启动游戏主循环（16ms ≈ 60FPS） ----
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &GameWindow::updateGameLoop);
    timer->start(16);
}

// ============================================================================
// 析构函数
// ============================================================================
GameWindow::~GameWindow() {}

// ============================================================================
// 初始化新比赛
// 重置回合计数、玩家对象、准备状态、所有特效实体
// ============================================================================
void GameWindow::initNewMatch() {
    currentRound = 0; p1 = Player(); p2 = Player();  // 重置回合和玩家
    p1_ready = false; p2_ready = false; showHelp = false;  // 重置就绪和帮助状态
    // 清空所有特效容器
    flyingGrenades.clear(); fireZones.clear(); smokeZones.clear();
    particles.clear(); droppedMags.clear(); tracers.clear(); muzzleFlashes.clear();
}

// ============================================================================
// 触发场景切换（淡出动画）
// 仅在未处于淡出状态时生效，设置目标状态后由updateGameLoop处理淡出
// ============================================================================
void GameWindow::transitionTo(GameState nextState) {
    if (!isFadingOut) {
        isFadingOut = true;
        targetState = nextState;
    }
}

// ============================================================================
// 应用状态切换（淡出完成后执行实际的游戏状态变更）
// 根据不同目标状态执行对应的初始化操作
// ============================================================================
void GameWindow::applyStateTransition() {
    gameState = targetState;

    // 确保BGM持续播放（智能判断不会打断已有播放）
    SoundManager::instance().playBGM("bgm.mp3");

    if (gameState == START_SCREEN || gameState == MATCH_OVER) {
        // 返回菜单：重置就绪状态
        p1_ready = false;
        p2_ready = false;
    }
    else if (gameState == BUY_PHASE) {
        // 进入购买阶段：回合+1，重置玩家位置，清空特效
        currentRound++;
        phaseTimer = 0;
        p1.resetRoundState(150.0, 425.0, true);    // P1出生在左侧，朝右
        p2.resetRoundState(1250.0, 425.0, false);  // P2出生在右侧，朝左
        keysPressed.clear();
        flyingGrenades.clear(); fireZones.clear(); smokeZones.clear();
        particles.clear(); droppedMags.clear(); tracers.clear(); muzzleFlashes.clear();
    }
    else if (gameState == PLAYING) {
        // 进入战斗阶段：设置2分钟倒计时（120秒 * 60帧/秒）
        phaseTimer = 120 * 60;
    }
    else if (gameState == ROUND_OVER) {
        // 回合结束展示阶段：3秒展示时间
        phaseTimer = 3 * 60;
    }
}

// ============================================================================
// 处理回合结束
// @param winner 胜者标识：1=P1胜, 2=P2胜, 0=平局
// 计算经济奖励（含连败补偿机制），应用2000金钱上限封顶
// ============================================================================
void GameWindow::handleRoundEnd(int winner) {
    // ---- 经济系统：赢家高奖励，输家低补偿，含连败累进机制 ----
    int winBaseBonus = 1800; // 赢家基础奖励（加上击杀奖励可接近2000上限）

    if (winner == 1) {
        // P1获胜
        p1.wins++; p2.lossStreak++; p1.lossStreak = 0;
        // 输家累进补偿：400起，每连败+200，最高1000
        int lossBonus = std::min(400 + (p2.lossStreak * 200), 1000);
        p1.stats.moneyEarnedThisRound += winBaseBonus;
        p2.stats.moneyEarnedThisRound += lossBonus;
    }
    else if (winner == 2) {
        // P2获胜
        p2.wins++; p1.lossStreak++; p2.lossStreak = 0;
        int lossBonus = std::min(400 + (p1.lossStreak * 200), 1000);
        p2.stats.moneyEarnedThisRound += winBaseBonus;
        p1.stats.moneyEarnedThisRound += lossBonus;
    }
    else {
        // 平局：双方各得800
        p1.stats.moneyEarnedThisRound += 800;
        p2.stats.moneyEarnedThisRound += 800;
    }

    // 严苛的赏金封顶逻辑（单局最高收益锁定在2000）
    p1.stats.moneyEarnedThisRound = std::min(p1.stats.moneyEarnedThisRound, 2000);
    p2.stats.moneyEarnedThisRound = std::min(p2.stats.moneyEarnedThisRound, 2000);

    // 将本回合收益计入玩家总金钱
    p1.money += p1.stats.moneyEarnedThisRound;
    p2.money += p2.stats.moneyEarnedThisRound;
    transitionTo(ROUND_OVER);
}

// ============================================================================
// 根据武器名称播放对应的开火音效
// 通过武器名匹配14种音效中的一种
// ============================================================================
void GameWindow::playWeaponSound(const Weapon& w) {
    if (w.name == "P250手枪") SoundManager::instance().play("shoot_pistol");
    else if (w.name == "冲锋枪") SoundManager::instance().play("shoot_smg");
    else if (w.name == "M4A1步枪") SoundManager::instance().play("shoot_rifle");
    else if (w.name == "霰弹枪") SoundManager::instance().play("shoot_shotgun");
    else if (w.name == "AWP狙击") SoundManager::instance().play("shoot_sniper");
    else if (w.name == "军用匕首") SoundManager::instance().play("knife_swing");
}

// ============================================================================
// 按键按下事件处理
// 处理所有游戏输入：菜单操作、购买、移动、开火、切枪、换弹、投掷
// 屏蔽长按重复输入(isAutoRepeat)和场景切换期间的输入
// ============================================================================
void GameWindow::keyPressEvent(QKeyEvent* event) {
    // 屏蔽长按产生的连续重复输入，且在场景淡出切换时封锁输入
    if (event->isAutoRepeat() || isFadingOut) return;

    int key = event->key();
    // 记录按下的键，供updateGameLoop处理持续移动和连发开火
    keysPressed.insert(key);

    // ==========================================
    // 1. 全局快捷键
    // ==========================================
    if (key == Qt::Key_H) {
        showHelp = !showHelp;  // 切换帮助信息显示
        return;
    }

    // ==========================================
    // 2. 菜单与结算界面控制（暂停/开始/比赛结束）
    // ==========================================
    if (gameState == PAUSED) {
        if (key == Qt::Key_Escape) {
            gameState = PLAYING;  // 取消暂停
        }
        else if (key == Qt::Key_Return) {
            // 强制结束比赛返回主菜单
            initNewMatch();
            SoundManager::instance().restartBGM("bgm.mp3");
            transitionTo(START_SCREEN);
        }
        return; // 暂停时不处理其他输入
    }

    if (gameState == START_SCREEN || gameState == MATCH_OVER) {
        // P1按空格准备，P2按回车准备
        if (key == Qt::Key_Space) p1_ready = !p1_ready;
        if (key == Qt::Key_Return) p2_ready = !p2_ready;

        // 双方就绪后进入购买阶段
        if (p1_ready && p2_ready) {
            if (gameState == MATCH_OVER) {
                initNewMatch();  // 比赛结束状态下重置比赛
                SoundManager::instance().restartBGM("bgm.mp3");
            }
            transitionTo(BUY_PHASE);
        }
        return;
    }

    if (gameState == ROUND_OVER) {
        if (key == Qt::Key_Escape) {
            // 投降/放弃比赛，返回主菜单
            initNewMatch();
            SoundManager::instance().restartBGM("bgm.mp3");
            transitionTo(START_SCREEN);
        }
        return;
    }

    // ==========================================
    // 3. 购买阶段专属指令
    // ==========================================
    if (gameState == BUY_PHASE) {
        if (key == Qt::Key_B) {
            transitionTo(PLAYING);  // 直接开始战斗
            return;
        }

        // ---- P1 购买逻辑 ----
        // Z/X/C/V 购买枪械（需有足够金钱且未解锁）
        if (key == Qt::Key_Z && p1.money >= p1.weapons[1].price && !p1.weapons[1].unlocked) { p1.money -= p1.weapons[1].price; p1.weapons[1].unlocked = true; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_X && p1.money >= p1.weapons[2].price && !p1.weapons[2].unlocked) { p1.money -= p1.weapons[2].price; p1.weapons[2].unlocked = true; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_C && p1.money >= p1.weapons[3].price && !p1.weapons[3].unlocked) { p1.money -= p1.weapons[3].price; p1.weapons[3].unlocked = true; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_V && p1.money >= p1.weapons[4].price && !p1.weapons[4].unlocked) { p1.money -= p1.weapons[4].price; p1.weapons[4].unlocked = true; SoundManager::instance().play("buy_item"); }

        // F/G/Y 购买投掷物（上限3个）
        if (key == Qt::Key_F && p1.money >= 300 && p1.heCount < p1.MAX_GRENADES) { p1.money -= 300; p1.heCount++; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_G && p1.money >= 400 && p1.molotovCount < p1.MAX_GRENADES) { p1.money -= 400; p1.molotovCount++; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_Y && p1.money >= 300 && p1.smokeCount < p1.MAX_GRENADES) { p1.money -= 300; p1.smokeCount++; SoundManager::instance().play("buy_item"); }

        // ---- P2 购买逻辑 ----
        // I/O/P/[ 购买枪械
        if (key == Qt::Key_I && p2.money >= p2.weapons[1].price && !p2.weapons[1].unlocked) { p2.money -= p2.weapons[1].price; p2.weapons[1].unlocked = true; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_O && p2.money >= p2.weapons[2].price && !p2.weapons[2].unlocked) { p2.money -= p2.weapons[2].price; p2.weapons[2].unlocked = true; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_P && p2.money >= p2.weapons[3].price && !p2.weapons[3].unlocked) { p2.money -= p2.weapons[3].price; p2.weapons[3].unlocked = true; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_BracketLeft && p2.money >= p2.weapons[4].price && !p2.weapons[4].unlocked) { p2.money -= p2.weapons[4].price; p2.weapons[4].unlocked = true; SoundManager::instance().play("buy_item"); }

        // J/K/L 购买投掷物
        if (key == Qt::Key_J && p2.money >= 300 && p2.heCount < p2.MAX_GRENADES) { p2.money -= 300; p2.heCount++; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_K && p2.money >= 400 && p2.molotovCount < p2.MAX_GRENADES) { p2.money -= 400; p2.molotovCount++; SoundManager::instance().play("buy_item"); }
        else if (key == Qt::Key_L && p2.money >= 300 && p2.smokeCount < p2.MAX_GRENADES) { p2.money -= 300; p2.smokeCount++; SoundManager::instance().play("buy_item"); }
    }

    // ==========================================
    // 4. 战斗阶段专属指令（暂停和投掷物）
    // ==========================================
    if (gameState == PLAYING) {
        if (key == Qt::Key_Escape) {
            gameState = PAUSED;  // 暂停游戏
            return;
        }

        // P1 投掷（Q=手雷, E=燃烧瓶, T=烟雾弹）
        double p1_rad = p1.facingRight ? 0.0 : PI;  // 投掷方向角度
        if (key == Qt::Key_Q && p1.heCount > 0) { p1.heCount--; flyingGrenades.push_back({ p1.x, p1.y, std::cos(p1_rad) * 15.0, std::sin(p1_rad) * 15.0, 60, G_HE, 1 }); }
        else if (key == Qt::Key_E && p1.molotovCount > 0) { p1.molotovCount--; flyingGrenades.push_back({ p1.x, p1.y, std::cos(p1_rad) * 15.0, std::sin(p1_rad) * 15.0, 60, G_MOLOTOV, 1 }); }
        else if (key == Qt::Key_T && p1.smokeCount > 0) { p1.smokeCount--; flyingGrenades.push_back({ p1.x, p1.y, std::cos(p1_rad) * 15.0, std::sin(p1_rad) * 15.0, 60, G_SMOKE, 1 }); }

        // P2 投掷（U=手雷, N=燃烧瓶, M=烟雾弹）
        double p2_rad = p2.facingRight ? 0.0 : PI;
        if (key == Qt::Key_U && p2.heCount > 0) { p2.heCount--; flyingGrenades.push_back({ p2.x, p2.y, std::cos(p2_rad) * 15.0, std::sin(p2_rad) * 15.0, 60, G_HE, 2 }); }
        else if (key == Qt::Key_N && p2.molotovCount > 0) { p2.molotovCount--; flyingGrenades.push_back({ p2.x, p2.y, std::cos(p2_rad) * 15.0, std::sin(p2_rad) * 15.0, 60, G_MOLOTOV, 2 }); }
        else if (key == Qt::Key_M && p2.smokeCount > 0) { p2.smokeCount--; flyingGrenades.push_back({ p2.x, p2.y, std::cos(p2_rad) * 15.0, std::sin(p2_rad) * 15.0, 60, G_SMOKE, 2 }); }
    }

    // ==========================================
    // 5. 战斗和购买阶段通用指令（切枪和换弹）
    // ==========================================
    if (gameState == PLAYING || gameState == BUY_PHASE) {
        // P1 切枪：数字键1~6对应武器槽位0~5
        if (key == Qt::Key_1 && p1.weapons[0].unlocked) p1.weaponIndex = 0;
        else if (key == Qt::Key_2 && p1.weapons[1].unlocked) p1.weaponIndex = 1;
        else if (key == Qt::Key_3 && p1.weapons[2].unlocked) p1.weaponIndex = 2;
        else if (key == Qt::Key_4 && p1.weapons[3].unlocked) p1.weaponIndex = 3;
        else if (key == Qt::Key_5 && p1.weapons[4].unlocked) p1.weaponIndex = 4;
        else if (key == Qt::Key_6 && p1.weapons[5].unlocked) p1.weaponIndex = 5;

        // P2 切枪：7/8/9/0/-/= 对应武器槽位0~5
        if (key == Qt::Key_7 && p2.weapons[0].unlocked) p2.weaponIndex = 0;
        else if (key == Qt::Key_8 && p2.weapons[1].unlocked) p2.weaponIndex = 1;
        else if (key == Qt::Key_9 && p2.weapons[2].unlocked) p2.weaponIndex = 2;
        else if (key == Qt::Key_0 && p2.weapons[3].unlocked) p2.weaponIndex = 3;
        else if (key == Qt::Key_Minus && p2.weapons[4].unlocked) p2.weaponIndex = 4;
        else if (key == Qt::Key_Equal && p2.weapons[5].unlocked) p2.weaponIndex = 5;

        // P1 换弹（R键）：非近战武器、弹匣未满、有备用弹匣、未在冷却中
        if (key == Qt::Key_R) {
            Weapon& w = p1.weapons[p1.weaponIndex];
            if (!w.isMelee && w.currentAmmo < w.maxAmmo && w.magsLeft > 0 && p1.cooldownTimer <= 0) {
                w.magsLeft--; w.currentAmmo = w.maxAmmo;  // 消耗一个备用弹匣，填充满
                p1.cooldownTimer = w.reloadTime;           // 设置换弹冷却
                SoundManager::instance().play("reload_mag");
                // 弹出旧弹匣特效（随机速度、旋转）
                droppedMags.push_back({ p1.x, p1.y, (std::rand() % 10 - 5) * 0.2, (std::rand() % 10 - 5) * 0.2, p1.facingRight ? 0.0 : 180.0, (std::rand() % 20 - 10) * 1.0, 200 });
            }
        }

        // P2 换弹（右Shift键）
        if (key == Qt::Key_Shift) {
            Weapon& w = p2.weapons[p2.weaponIndex];
            if (!w.isMelee && w.currentAmmo < w.maxAmmo && w.magsLeft > 0 && p2.cooldownTimer <= 0) {
                w.magsLeft--; w.currentAmmo = w.maxAmmo;
                p2.cooldownTimer = w.reloadTime;
                SoundManager::instance().play("reload_mag");
                droppedMags.push_back({ p2.x, p2.y, (std::rand() % 10 - 5) * 0.2, (std::rand() % 10 - 5) * 0.2, p2.facingRight ? 0.0 : 180.0, (std::rand() % 20 - 10) * 1.0, 200 });
            }
        }
    }
}

// ============================================================================
// 按键释放事件：从keysPressed集合中移除已释放的按键
// ============================================================================
void GameWindow::keyReleaseEvent(QKeyEvent* event) {
    if (!event->isAutoRepeat())
        keysPressed.remove(event->key());
}

// ============================================================================
// 处理两玩家之间的碰撞
// 当两个玩家距离小于playerSize时，推开双方并减速
// ============================================================================
void GameWindow::handlePlayerCollision() {
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    double distance = std::sqrt(dx * dx + dy * dy);
    if (distance < playerSize && distance > 0.001) {
        double overlap = playerSize - distance;  // 重叠量
        double nx = dx / distance;               // 碰撞法线X
        double ny = dy / distance;               // 碰撞法线Y
        // 各推一半
        p1.x += nx * overlap / 2.0; p1.y += ny * overlap / 2.0;
        p2.x -= nx * overlap / 2.0; p2.y -= ny * overlap / 2.0;
        // 碰撞减速
        p1.vx *= 0.5; p1.vy *= 0.5; p2.vx *= 0.5; p2.vy *= 0.5;
    }
}

// ============================================================================
// 游戏主循环（每16ms调用一次 ≈ 60FPS）
// 负责：场景切换动画、加载动画、状态更新、物理模拟、碰撞检测、弹道结算
// ============================================================================
void GameWindow::updateGameLoop() {
    // 暂停状态只刷新画面，不更新逻辑
    if (gameState == PAUSED) { update(); return; }

    // [新增] 画面抖动衰减计算
    if (screenShakeIntensity > 0.1) {
        shakeOffsetX = (std::rand() % 100 - 50) / 50.0 * screenShakeIntensity;
        shakeOffsetY = (std::rand() % 100 - 50) / 50.0 * screenShakeIntensity;
        screenShakeIntensity *= 0.9;  // 每帧衰减抖动强度
    }
    else {
        screenShakeIntensity = 0.0;
        shakeOffsetX = 0.0;
        shakeOffsetY = 0.0;
    }

    // ---- 场景淡出动画处理 ----
    if (isFadingOut) {
        uiAlpha -= 0.08;  // 每帧减少透明度
        if (uiAlpha <= 0.0) {
            uiAlpha = 0.0; isFadingOut = false; applyStateTransition();  // 淡出完成，切换状态
        }
        update(); return;
    }
    else if (uiAlpha < 1.0) {
        // 淡入恢复
        uiAlpha += 0.05;
        if (uiAlpha > 1.0) uiAlpha = 1.0;
    }

    // ---- 加载屏幕动画 ----
    if (gameState == LOADING_SCREEN) {
        loadingTimer++;
        loadingProgress = std::min(100.0, loadingTimer * 0.5);  // 进度条增长

        // 生成上升背景粒子（模拟加载氛围）
        if (std::rand() % 3 == 0) {
            loadingParticles.push_back({ (double)(std::rand() % width()), (double)height() + 10, 0, -(double)(std::rand() % 5 + 2), 60, 60, QColor(100, 110, 120, 120), (double)(std::rand() % 15 + 5) });
        }
        // 更新和清理加载粒子
        for (int i = loadingParticles.size() - 1; i >= 0; i--) {
            loadingParticles[i].y += loadingParticles[i].vy;
            if (--loadingParticles[i].life <= 0) loadingParticles.removeAt(i);
        }

        // 加载完成后切换到开始界面
        if (loadingProgress >= 100.0 && loadingTimer > 250) transitionTo(START_SCREEN);
        update(); return;
    }

    // ---- 非战斗状态的画面更新（帮助/开始/结算/购买界面不执行物理逻辑） ----
    if (showHelp || gameState == START_SCREEN || gameState == MATCH_OVER || gameState == BUY_PHASE) {
        update(); return;
    }

    // ---- 回合结束倒计时 ----
    if (gameState == ROUND_OVER) {
        if (--phaseTimer <= 0) {
            // 判断比赛是否结束（抢8）
            if (p1.wins >= WINS_NEEDED || p2.wins >= WINS_NEEDED) transitionTo(MATCH_OVER);
            else transitionTo(BUY_PHASE);  // 否则进入下一回合购买阶段
        }
        update(); return;
    }

    // ---- 战斗阶段倒计时 ----
    if (gameState == PLAYING) {
        if (--phaseTimer <= 0) { handleRoundEnd(0); update(); return; }  // 时间耗尽→平局
    }

    // ---- 更新玩家冷却计时器 ----
    if (p1.cooldownTimer > 0) p1.cooldownTimer--;
    if (p2.cooldownTimer > 0) p2.cooldownTimer--;
    if (p1.meleeTimer > 0) p1.meleeTimer--;   // 近战动画计时器
    if (p2.meleeTimer > 0) p2.meleeTimer--;

    // ---- 更新视觉特效生命周期 ----
    for (int i = muzzleFlashes.size() - 1; i >= 0; i--) { if (--muzzleFlashes[i].life <= 0) muzzleFlashes.removeAt(i); }
    for (int i = tracers.size() - 1; i >= 0; i--) { if (--tracers[i].life <= 0) tracers.removeAt(i); }

    // ---- 更新掉落弹匣物理 ----
    for (int i = droppedMags.size() - 1; i >= 0; i--) {
        droppedMags[i].x += droppedMags[i].vx; droppedMags[i].y += droppedMags[i].vy;
        droppedMags[i].vx *= 0.8; droppedMags[i].vy *= 0.8;  // 速度衰减
        droppedMags[i].angle += droppedMags[i].vAngle; droppedMags[i].vAngle *= 0.9;  // 旋转衰减
        if (--droppedMags[i].life <= 0) droppedMags.removeAt(i);
    }

    // ---- 更新通用粒子 ----
    for (int i = particles.size() - 1; i >= 0; i--) {
        particles[i].x += particles[i].vx; particles[i].y += particles[i].vy;
        particles[i].vx *= 0.92; particles[i].vy *= 0.92;  // 速度缓慢衰减
        if (--particles[i].life <= 0) particles.removeAt(i);
    }

    // ---- 更新飞行中的投掷物 ----
    for (int i = flyingGrenades.size() - 1; i >= 0; --i) {
        flyingGrenades[i].x += flyingGrenades[i].vx;
        flyingGrenades[i].y += flyingGrenades[i].vy;
        flyingGrenades[i].vx *= 0.95;  // 空气阻力
        flyingGrenades[i].vy *= 0.95;

        // 边界反弹
        if (flyingGrenades[i].x < 0) { flyingGrenades[i].x = 0; flyingGrenades[i].vx *= -0.8; }
        if (flyingGrenades[i].x > width()) { flyingGrenades[i].x = width(); flyingGrenades[i].vx *= -0.8; }
        if (flyingGrenades[i].y < 60) { flyingGrenades[i].y = 60; flyingGrenades[i].vy *= -0.8; }
        if (flyingGrenades[i].y > height() - 100) { flyingGrenades[i].y = height() - 100; flyingGrenades[i].vy *= -0.8; }

        // 飞行轨迹粒子（灰色尾迹）
        particles.push_back({ flyingGrenades[i].x, flyingGrenades[i].y, 0, 0, 10, 10, QColor(200,200,200,100), 3 });

        // 引信归零→引爆
        if (--flyingGrenades[i].fuseTimer <= 0) {
            double gx = flyingGrenades[i].x; double gy = flyingGrenades[i].y;
            if (flyingGrenades[i].type == G_HE) {
                // ---- 高爆手雷：范围伤害最高100，半径200 ----
                SoundManager::instance().play("explode_he");

                // [新增] 高爆雷爆炸产生强烈画面抖动
                screenShakeIntensity = 25.0;

                // 爆炸粒子效果
                for (int p = 0; p < 30; p++) {
                    double pA = (std::rand() % 360) * PI / 180.0; double pV = (std::rand() % 150) / 10.0;
                    particles.push_back({ gx, gy, std::cos(pA) * pV, std::sin(pA) * pV, 20, 20, QColor(255,150,0), 6 });
                }
                // 伤害计算（距离衰减）
                double dist1 = std::sqrt(std::pow(p1.x - gx, 2) + std::pow(p1.y - gy, 2));
                double dist2 = std::sqrt(std::pow(p2.x - gx, 2) + std::pow(p2.y - gy, 2));
                if (dist1 < 200) p1.hp -= 100.0 * (1.0 - dist1 / 200.0);
                if (dist2 < 200) p2.hp -= 100.0 * (1.0 - dist2 / 200.0);

                // 死亡判定
                if (p1.hp <= 0 && p2.hp > 0) { handleRoundEnd(2); return; }
                if (p2.hp <= 0 && p1.hp > 0) { handleRoundEnd(1); return; }
                if (p1.hp <= 0 && p2.hp <= 0) { handleRoundEnd(0); return; }
            }
            else if (flyingGrenades[i].type == G_MOLOTOV) {
                // ---- 燃烧瓶：生成持续火焰区域 ----
                SoundManager::instance().play("explode_molotov");
                fireZones.push_back({ gx, gy, 300, flyingGrenades[i].owner });
            }
            else if (flyingGrenades[i].type == G_SMOKE) {
                // ---- 烟雾弹：生成遮蔽烟雾区域 ----
                SoundManager::instance().play("explode_smoke");
                smokeZones.push_back({ gx, gy, 600 });
            }
            flyingGrenades.removeAt(i);  // 移除已引爆的投掷物
        }
    }

    // ---- 更新火焰区域 ----
    for (int i = fireZones.size() - 1; i >= 0; --i) {
        // 火焰粒子效果
        double rx = fireZones[i].x + (std::rand() % 160 - 80); double ry = fireZones[i].y + (std::rand() % 160 - 80);
        particles.push_back({ rx, ry, 0, -2.0 - (std::rand() % 20) / 10.0, 30, 30, QColor(255, 100 + std::rand() % 100, 0), 4 });
        // 范围内每帧0.3伤害
        double dist1 = std::sqrt(std::pow(p1.x - fireZones[i].x, 2) + std::pow(p1.y - fireZones[i].y, 2));
        if (dist1 < 100) { p1.hp -= 0.3; if (p1.hp <= 0) { handleRoundEnd(2); return; } }
        double dist2 = std::sqrt(std::pow(p2.x - fireZones[i].x, 2) + std::pow(p2.y - fireZones[i].y, 2));
        if (dist2 < 100) { p2.hp -= 0.3; if (p2.hp <= 0) { handleRoundEnd(1); return; } }
        if (--fireZones[i].duration <= 0) fireZones.removeAt(i);
    }

    // ---- 更新烟雾区域 ----
    for (int i = smokeZones.size() - 1; i >= 0; --i) {
        // 烟雾扩散粒子
        if (std::rand() % 3 == 0) {
            double pA = (std::rand() % 360) * PI / 180.0; double pV = (std::rand() % 20) / 10.0;
            particles.push_back({ smokeZones[i].x, smokeZones[i].y, std::cos(pA) * pV, std::sin(pA) * pV, 60, 60, QColor(150,150,150, 100), 20 });
        }
        if (--smokeZones[i].duration <= 0) smokeZones.removeAt(i);
    }

    // ==========================================
    // P1 移动处理（WASD八向）
    // ==========================================
    p1.isMoving = false;
    if (keysPressed.contains(Qt::Key_A)) { p1.vx -= acceleration; p1.facingRight = false; p1.isMoving = true; }
    if (keysPressed.contains(Qt::Key_D)) { p1.vx += acceleration; p1.facingRight = true; p1.isMoving = true; }
    if (keysPressed.contains(Qt::Key_W)) { p1.vy -= acceleration; p1.isMoving = true; }
    if (keysPressed.contains(Qt::Key_S)) { p1.vy += acceleration; p1.isMoving = true; }
    p1.updateAnimation();

    // 脚步声（每15帧播放一次）
    if (p1.isMoving) {
        p1FootstepCounter++;
        if (p1FootstepCounter >= 15) { p1FootstepCounter = 0; SoundManager::instance().play("footstep"); }
    }
    else { p1FootstepCounter = 0; }

    // P1 X轴移动与障碍物碰撞
    p1.x += p1.vx;
    QRectF p1_rectX(p1.x - playerSize / 2, p1.y - playerSize / 2, playerSize, playerSize);
    for (const QRectF& obs : obstacles) { if (p1_rectX.intersects(obs)) { p1.x -= p1.vx; p1.vx = 0; break; } }

    // P1 Y轴移动与障碍物碰撞（分离轴检测）
    p1.y += p1.vy;
    QRectF p1_rectY(p1.x - playerSize / 2, p1.y - playerSize / 2, playerSize, playerSize);
    for (const QRectF& obs : obstacles) { if (p1_rectY.intersects(obs)) { p1.y -= p1.vy; p1.vy = 0; break; } }

    // ---- P1 开火处理（空格键） ----
    if (keysPressed.contains(Qt::Key_Space) && p1.cooldownTimer <= 0) {
        Weapon& w = p1.weapons[p1.weaponIndex];
        double p1_rad = p1.facingRight ? 0.0 : PI;

        if (w.isMelee) {
            // 近战攻击
            p1.cooldownTimer = w.fireCooldown; p1.meleeTimer = 10; playWeaponSound(w);
            QRectF meleeHitbox(p1.x + std::cos(p1_rad) * 40.0 - 25, p1.y + std::sin(p1_rad) * 40.0 - 25, 50, 50);
            QRectF rectP2(p2.x - playerSize / 2, p2.y - playerSize / 2, playerSize, playerSize);
            if (meleeHitbox.intersects(rectP2)) {
                p2.hp -= w.damage; p1.stats.bodyDmgDealt += w.damage; SoundManager::instance().play("hit_body");
                if (p2.hp <= 0) { p1.stats.lastKillWeapon = w.name; p1.stats.moneyEarnedThisRound += w.killReward; handleRoundEnd(1); return; }
            }
        }
        else if (w.currentAmmo > 0) {
            // 射击
            w.currentAmmo--; p1.cooldownTimer = w.fireCooldown; playWeaponSound(w);

            // [新增] AWP重型武器开火产生后坐力画面抖动
            if (w.name == "AWP狙击") screenShakeIntensity = 12.0;

            double spawnX = p1.x + (p1.facingRight ? 35.0 : -35.0);  // 枪口X位置
            double spawnY = p1.y - 12.0;                               // 枪口Y位置

            // 枪口闪光
            muzzleFlashes.push_back({ spawnX, spawnY, p1.facingRight ? 0.0 : 180.0, 3 });

            // 动态精度：移动时散布x2.5，静止时x0.3
            double currentSpeed = std::sqrt(p1.vx * p1.vx + p1.vy * p1.vy);
            double dynamicSpread = (currentSpeed > 0.5) ? w.spread * 2.5 : w.spread * 0.3;

            // 射出弹丸（霰弹枪6发，其他1发）
            for (int p = 0; p < w.pellets; ++p) {
                double finalRad = p1_rad + ((std::rand() % 2000 - 1000) / 1000.0 * dynamicSpread * PI / 180.0);
                double bx = p1.vx + std::cos(finalRad) * w.bulletSpeed;  // 子弹速度含玩家惯性
                double by = p1.vy + std::sin(finalRad) * w.bulletSpeed;
                bool isSnipe = (w.name == "AWP狙击");
                p1.bullets.push_back({ spawnX, spawnY, bx, by, w.damage, w.name, false, isSnipe });
                // 弹道拖尾
                tracers.push_back({ spawnX, spawnY, spawnX + bx * 5, spawnY + by * 5, isSnipe ? 30 : 10, isSnipe ? 30 : 10, isSnipe });
            }
            // 后坐力反推
            p1.vx -= std::cos(p1_rad) * w.recoil; p1.vy -= std::sin(p1_rad) * w.recoil;
        }
        else { SoundManager::instance().play("empty_click"); }  // 空仓音效
    }

    // ==========================================
    // P2 移动处理（方向键八向）
    // ==========================================
    p2.isMoving = false;
    if (keysPressed.contains(Qt::Key_Left)) { p2.vx -= acceleration; p2.facingRight = false; p2.isMoving = true; }
    if (keysPressed.contains(Qt::Key_Right)) { p2.vx += acceleration; p2.facingRight = true; p2.isMoving = true; }
    if (keysPressed.contains(Qt::Key_Up)) { p2.vy -= acceleration; p2.isMoving = true; }
    if (keysPressed.contains(Qt::Key_Down)) { p2.vy += acceleration; p2.isMoving = true; }
    p2.updateAnimation();

    // P2脚步声
    if (p2.isMoving) {
        p2FootstepCounter++;
        if (p2FootstepCounter >= 15) { p2FootstepCounter = 0; SoundManager::instance().play("footstep"); }
    }
    else { p2FootstepCounter = 0; }

    // P2 X轴移动与障碍物碰撞
    p2.x += p2.vx;
    QRectF p2_rectX(p2.x - playerSize / 2, p2.y - playerSize / 2, playerSize, playerSize);
    for (const QRectF& obs : obstacles) { if (p2_rectX.intersects(obs)) { p2.x -= p2.vx; p2.vx = 0; break; } }

    // P2 Y轴移动与障碍物碰撞
    p2.y += p2.vy;
    QRectF p2_rectY(p2.x - playerSize / 2, p2.y - playerSize / 2, playerSize, playerSize);
    for (const QRectF& obs : obstacles) { if (p2_rectY.intersects(obs)) { p2.y -= p2.vy; p2.vy = 0; break; } }

    // ---- P2 开火处理（回车键） ----
    if (keysPressed.contains(Qt::Key_Return) && p2.cooldownTimer <= 0) {
        Weapon& w = p2.weapons[p2.weaponIndex];
        double p2_rad = p2.facingRight ? 0.0 : PI;

        if (w.isMelee) {
            // 近战攻击
            p2.cooldownTimer = w.fireCooldown; p2.meleeTimer = 10; playWeaponSound(w);
            QRectF meleeHitbox(p2.x + std::cos(p2_rad) * 40.0 - 25, p2.y + std::sin(p2_rad) * 40.0 - 25, 50, 50);
            QRectF rectP1(p1.x - playerSize / 2, p1.y - playerSize / 2, playerSize, playerSize);
            if (meleeHitbox.intersects(rectP1)) {
                p1.hp -= w.damage; p2.stats.bodyDmgDealt += w.damage; SoundManager::instance().play("hit_body");
                if (p1.hp <= 0) { p2.stats.lastKillWeapon = w.name; p2.stats.moneyEarnedThisRound += w.killReward; handleRoundEnd(2); return; }
            }
        }
        else if (w.currentAmmo > 0) {
            // 射击
            w.currentAmmo--; p2.cooldownTimer = w.fireCooldown; playWeaponSound(w);

            // [新增] AWP重型武器开火产生后坐力画面抖动
            if (w.name == "AWP狙击") screenShakeIntensity = 12.0;

            double spawnX = p2.x + (p2.facingRight ? 35.0 : -35.0);
            double spawnY = p2.y - 12.0;

            muzzleFlashes.push_back({ spawnX, spawnY, p2.facingRight ? 0.0 : 180.0, 3 });

            double currentSpeed = std::sqrt(p2.vx * p2.vx + p2.vy * p2.vy);
            double dynamicSpread = (currentSpeed > 0.5) ? w.spread * 2.5 : w.spread * 0.3;

            for (int p = 0; p < w.pellets; ++p) {
                double finalRad = p2_rad + ((std::rand() % 2000 - 1000) / 1000.0 * dynamicSpread * PI / 180.0);
                double bx = p2.vx + std::cos(finalRad) * w.bulletSpeed;
                double by = p2.vy + std::sin(finalRad) * w.bulletSpeed;
                bool isSnipe = (w.name == "AWP狙击");
                p2.bullets.push_back({ spawnX, spawnY, bx, by, w.damage, w.name, false, isSnipe });
                tracers.push_back({ spawnX, spawnY, spawnX + bx * 5, spawnY + by * 5, isSnipe ? 30 : 10, isSnipe ? 30 : 10, isSnipe });
            }
            p2.vx -= std::cos(p2_rad) * w.recoil; p2.vy -= std::sin(p2_rad) * w.recoil;
        }
        else { SoundManager::instance().play("empty_click"); }
    }

    // ---- 应用摩擦力 ----
    p1.vx *= friction; p1.vy *= friction;
    p2.vx *= friction; p2.vy *= friction;

    // ---- 玩家间碰撞 ----
    handlePlayerCollision();

    // ---- 边界约束（防止玩家移出可视区域） ----
    if (p1.x < playerSize / 2) p1.x = playerSize / 2;
    if (p1.x > width() - playerSize / 2) p1.x = width() - playerSize / 2;
    if (p1.y < 60 + playerSize / 2) p1.y = 60 + playerSize / 2;    // 顶部HUD栏下方
    if (p1.y > height() - 100 - playerSize / 2) p1.y = height() - 100 - playerSize / 2;  // 底部UI栏上方
    if (p2.x < playerSize / 2) p2.x = playerSize / 2;
    if (p2.x > width() - playerSize / 2) p2.x = width() - playerSize / 2;
    if (p2.y < 60 + playerSize / 2) p2.y = 60 + playerSize / 2;
    if (p2.y > height() - 100 - playerSize / 2) p2.y = height() - 100 - playerSize / 2;

    // ---- 获取玩家命中判定区域 ----
    QPolygonF p1_head = p1.getHeadHitbox(); QPolygonF p1_up = p1.getUpperBodyHitbox(); QPolygonF p1_low = p1.getLowerBodyHitbox();
    QPolygonF p2_head = p2.getHeadHitbox(); QPolygonF p2_up = p2.getUpperBodyHitbox(); QPolygonF p2_low = p2.getLowerBodyHitbox();

    // ==========================================
    // P1 弹道结算（优先检测玩家碰撞，再检测墙壁，避免爆头被后方掩体吞掉）
    // ==========================================
    for (int i = p1.bullets.size() - 1; i >= 0; --i) {
        double oldX = p1.bullets[i].x;
        double oldY = p1.bullets[i].y;
        p1.bullets[i].x += p1.bullets[i].vx;
        p1.bullets[i].y += p1.bullets[i].vy;

        // [新增] 烟雾弹穿透削减逻辑：判定当前帧轨迹是否穿烟
        bool insideSmoke = false;
        for (const SmokeZone& s : smokeZones) {
            if (std::sqrt(std::pow(p1.bullets[i].x - s.x, 2) + std::pow(p1.bullets[i].y - s.y, 2)) < 130) {
                insideSmoke = true; break;
            }
        }
        if (insideSmoke) p1.bullets[i].damage *= 0.998;

        bool bulletRemoved = false;

        // [修改] CCD (连续碰撞检测) 解决子弹速度过快导致的穿透Bug
        int steps = std::max(1, (int)std::ceil(std::sqrt(p1.bullets[i].vx * p1.bullets[i].vx + p1.bullets[i].vy * p1.bullets[i].vy) / 5.0));

        for (int step = 1; step <= steps; ++step) {
            double interpX = oldX + (p1.bullets[i].x - oldX) * ((double)step / steps);
            double interpY = oldY + (p1.bullets[i].y - oldY) * ((double)step / steps);
            QPointF bulletPoint(interpX, interpY);

            // 头部判定（x2.0伤害）
            if (p2_head.containsPoint(bulletPoint, Qt::OddEvenFill)) {
                p2.hp -= p1.bullets[i].damage * 2.0; p1.stats.headDmgDealt += p1.bullets[i].damage * 2.0; SoundManager::instance().play("hit_body");
                for (int k = 0; k < 8; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.8, (std::rand() % 10 - 5) * 0.8, 15, 15, QColor(255,0,0), 4 });  // 血花粒子
                if (p2.hp <= 0) { p1.stats.lastKillWeapon = p1.bullets[i].weaponName; p1.stats.moneyEarnedThisRound += p1.weapons[p1.weaponIndex].killReward + 200; handleRoundEnd(1); return; }  // 爆头额外+200
                bulletRemoved = true; break;
            }
            // 上半身判定（x1.0伤害）
            else if (p2_up.containsPoint(bulletPoint, Qt::OddEvenFill)) {
                p2.hp -= p1.bullets[i].damage * 1.0; p1.stats.bodyDmgDealt += p1.bullets[i].damage * 1.0; SoundManager::instance().play("hit_body");
                for (int k = 0; k < 5; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.8, (std::rand() % 10 - 5) * 0.8, 15, 15, QColor(255,0,0), 4 });
                if (p2.hp <= 0) { p1.stats.lastKillWeapon = p1.bullets[i].weaponName; p1.stats.moneyEarnedThisRound += p1.weapons[p1.weaponIndex].killReward; handleRoundEnd(1); return; }
                bulletRemoved = true; break;
            }
            // 下半身判定（x0.7伤害）
            else if (p2_low.containsPoint(bulletPoint, Qt::OddEvenFill)) {
                p2.hp -= p1.bullets[i].damage * 0.7; p1.stats.bodyDmgDealt += p1.bullets[i].damage * 0.7; SoundManager::instance().play("hit_body");
                for (int k = 0; k < 5; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.8, (std::rand() % 10 - 5) * 0.8, 15, 15, QColor(255,0,0), 4 });
                if (p2.hp <= 0) { p1.stats.lastKillWeapon = p1.bullets[i].weaponName; p1.stats.moneyEarnedThisRound += p1.weapons[p1.weaponIndex].killReward; handleRoundEnd(1); return; }
                bulletRemoved = true; break;
            }

            // 未命中玩家→检测障碍物碰撞
            for (const QRectF& obs : obstacles) {
                if (obs.contains(bulletPoint)) {
                    // 墙壁命中火花粒子
                    for (int k = 0; k < 5; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.5, (std::rand() % 10 - 5) * 0.5, 10, 10, QColor(255,200,0), 3 });
                    bulletRemoved = true; break;
                }
            }
            if (bulletRemoved) break;
        }

        if (bulletRemoved) {
            p1.bullets.removeAt(i);
            continue;
        }

        // 边界检测（飞出屏幕则移除）
        if (p1.bullets[i].x < 0 || p1.bullets[i].x > width() || p1.bullets[i].y < 60 || p1.bullets[i].y > height() - 100) {
            p1.bullets.removeAt(i);
        }
    }

    // ==========================================
    // P2 弹道结算（逻辑与P1完全对称）
    // ==========================================
    for (int i = p2.bullets.size() - 1; i >= 0; --i) {
        double oldX = p2.bullets[i].x;
        double oldY = p2.bullets[i].y;
        p2.bullets[i].x += p2.bullets[i].vx;
        p2.bullets[i].y += p2.bullets[i].vy;

        // [新增] 烟雾弹穿透削减逻辑
        bool insideSmoke = false;
        for (const SmokeZone& s : smokeZones) {
            if (std::sqrt(std::pow(p2.bullets[i].x - s.x, 2) + std::pow(p2.bullets[i].y - s.y, 2)) < 130) {
                insideSmoke = true; break;
            }
        }
        if (insideSmoke) p2.bullets[i].damage *= 0.998;

        bool bulletRemoved = false;

        // [修改] CCD (连续碰撞检测) 解决子弹穿透Bug
        int steps = std::max(1, (int)std::ceil(std::sqrt(p2.bullets[i].vx * p2.bullets[i].vx + p2.bullets[i].vy * p2.bullets[i].vy) / 5.0));

        for (int step = 1; step <= steps; ++step) {
            double interpX = oldX + (p2.bullets[i].x - oldX) * ((double)step / steps);
            double interpY = oldY + (p2.bullets[i].y - oldY) * ((double)step / steps);
            QPointF bulletPoint(interpX, interpY);

            if (p1_head.containsPoint(bulletPoint, Qt::OddEvenFill)) {
                p1.hp -= p2.bullets[i].damage * 2.0; p2.stats.headDmgDealt += p2.bullets[i].damage * 2.0; SoundManager::instance().play("hit_body");
                for (int k = 0; k < 8; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.8, (std::rand() % 10 - 5) * 0.8, 15, 15, QColor(255,0,0), 4 });
                if (p1.hp <= 0) { p2.stats.lastKillWeapon = p2.bullets[i].weaponName; p2.stats.moneyEarnedThisRound += p2.weapons[p2.weaponIndex].killReward + 200; handleRoundEnd(2); return; }
                bulletRemoved = true; break;
            }
            else if (p1_up.containsPoint(bulletPoint, Qt::OddEvenFill)) {
                p1.hp -= p2.bullets[i].damage * 1.0; p2.stats.bodyDmgDealt += p2.bullets[i].damage * 1.0; SoundManager::instance().play("hit_body");
                for (int k = 0; k < 5; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.8, (std::rand() % 10 - 5) * 0.8, 15, 15, QColor(255,0,0), 4 });
                if (p1.hp <= 0) { p2.stats.lastKillWeapon = p2.bullets[i].weaponName; p2.stats.moneyEarnedThisRound += p2.weapons[p2.weaponIndex].killReward; handleRoundEnd(2); return; }
                bulletRemoved = true; break;
            }
            else if (p1_low.containsPoint(bulletPoint, Qt::OddEvenFill)) {
                p1.hp -= p2.bullets[i].damage * 0.7; p2.stats.bodyDmgDealt += p2.bullets[i].damage * 0.7; SoundManager::instance().play("hit_body");
                for (int k = 0; k < 5; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.8, (std::rand() % 10 - 5) * 0.8, 15, 15, QColor(255,0,0), 4 });
                if (p1.hp <= 0) { p2.stats.lastKillWeapon = p2.bullets[i].weaponName; p2.stats.moneyEarnedThisRound += p2.weapons[p2.weaponIndex].killReward; handleRoundEnd(2); return; }
                bulletRemoved = true; break;
            }

            for (const QRectF& obs : obstacles) {
                if (obs.contains(bulletPoint)) {
                    for (int k = 0; k < 5; k++) particles.push_back({ interpX, interpY, (std::rand() % 10 - 5) * 0.5, (std::rand() % 10 - 5) * 0.5, 10, 10, QColor(255,200,0), 3 });
                    bulletRemoved = true; break;
                }
            }
            if (bulletRemoved) break;
        }

        if (bulletRemoved) {
            p2.bullets.removeAt(i);
            continue;
        }

        if (p2.bullets[i].x < 0 || p2.bullets[i].x > width() || p2.bullets[i].y < 60 || p2.bullets[i].y > height() - 100) {
            p2.bullets.removeAt(i);
        }
    }

    update();  // 触发重绘
}

// ============================================================================
// 绘制事件：渲染整个游戏画面
// 根据当前游戏状态绘制对应的UI和游戏场景
// 渲染顺序：加载画面→地图→特效→玩家→UI覆盖层
// ============================================================================
void GameWindow::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);  // 关闭抗锯齿，保持像素风格

    // ==========================================
    // 加载屏幕
    // ==========================================
    if (gameState == LOADING_SCREEN) {
        painter.fillRect(rect(), QColor(25, 28, 30));  // 深色背景
        // 背景粒子
        for (const auto& p : loadingParticles) { painter.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size, p.color); }
        // 游戏标题
        painter.setPen(QColor(220, 220, 225));
        painter.setFont(QFont("Arial Black", 56, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "反恐1v1 2d版");
        // 加载进度条背景
        painter.fillRect(width() / 2 - 250, height() - 200, 500, 16, QColor(40, 45, 50));
        // 加载进度条前景
        painter.fillRect(width() / 2 - 250, height() - 200, 5 * loadingProgress, 16, QColor(100, 150, 200));
        // 加载提示文字
        painter.setPen(QColor(150, 150, 150));
        painter.setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
        painter.drawText(QRect(0, height() - 170, width(), 50), Qt::AlignCenter, "正 在 载 入 战 术 资 源 . . .");
        return;
    }

    // [新增] 保存原本静止的坐标系，避免后续画面抖动影响到 UI
    painter.save();
    painter.translate(shakeOffsetX, shakeOffsetY); // 应用画面抖动偏移

    // ==========================================
    // 地图背景与地面装饰
    // ==========================================
    QColor urbanGround(70, 75, 80);  // 城市地面灰色
    painter.fillRect(0, 60, width(), height() - 60, urbanGround);

    // 地面网格线（提供空间感）
    painter.setPen(QPen(QColor(60, 65, 70), 1, Qt::DashLine));
    for (int i = 0; i < width(); i += 150) painter.drawLine(i, 60, i, height());
    for (int j = 60; j < height(); j += 150) painter.drawLine(0, j, width(), j);

    // 地面污水/油渍装饰
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 20, 150));
    painter.drawEllipse(350, 200, 45, 25);
    painter.drawEllipse(1000, 500, 55, 30);
    painter.drawEllipse(650, 350, 30, 40);

    // 地面碎片装饰（弹壳、碎石）
    auto drawDebris = [&](double x, double y, double w, double h, QColor c, double angle) {
        painter.save(); painter.translate(x, y); painter.rotate(angle);
        painter.fillRect(QRectF(-w / 2, -h / 2, w, h), c); painter.restore();
        };
    drawDebris(150, 120, 15, 20, QColor(220, 220, 220), 15);
    drawDebris(165, 125, 10, 10, QColor(180, 180, 180), -20);
    drawDebris(1150, 700, 30, 40, QColor(100, 60, 40), 45);
    drawDebris(800, 300, 25, 15, QColor(40, 40, 40), 12);
    drawDebris(500, 600, 40, 5, QColor(150, 50, 50), -10);

    // ---- 绘制障碍物（含阴影和伪3D立体效果） ----
    int obsIdx = 0;
    for (const QRectF& obs : obstacles) {
        // 底部阴影
        painter.fillRect(obs.x() + 15, obs.y() + 15, obs.width(), obs.height(), QColor(30, 35, 40, 150));

        if (obs.width() == 80 && obs.height() == 250) {
            // 大型混凝土承重墙（灰白色，带侧面厚度）
            painter.setBrush(QColor(120, 125, 130));
            painter.drawRect(obs);
            QPolygonF side;
            side << obs.bottomLeft() << obs.bottomRight() << QPointF(obs.right(), obs.bottom() + 20) << QPointF(obs.left(), obs.bottom() + 20);
            painter.setBrush(QColor(80, 85, 90));
            painter.drawPolygon(side);
            painter.setPen(QPen(QColor(90, 95, 100), 2));
            painter.drawLine(obs.topLeft(), obs.bottomLeft());
        }
        else if (obs.width() == 200 && obs.height() == 80) {
            // 金属集装箱（顶部/底部，带波纹纹理）
            QColor baseCol = (obsIdx % 2 == 0) ? QColor(50, 80, 120) : QColor(130, 60, 55);
            QColor sideCol = (obsIdx % 2 == 0) ? QColor(30, 50, 80) : QColor(90, 40, 35);
            painter.setBrush(baseCol); painter.setPen(Qt::NoPen);
            painter.drawRect(obs);
            QPolygonF side;
            side << obs.bottomLeft() << obs.bottomRight() << QPointF(obs.right(), obs.bottom() + 30) << QPointF(obs.left(), obs.bottom() + 30);
            painter.setBrush(sideCol);
            painter.drawPolygon(side);
            painter.setPen(QPen(QColor(20, 20, 20, 100), 2));
            for (int x = obs.x() + 20; x < obs.right(); x += 20) {
                painter.drawLine(x, obs.y(), x, obs.bottom() + 30);
            }
        }
        else if (obs.width() == 60 && obs.height() == 60) {
            // 木制战术物资箱（棕色，带X形绑带）
            painter.setBrush(QColor(160, 110, 60)); painter.setPen(Qt::NoPen);
            painter.drawRect(obs);
            QPolygonF side;
            side << obs.bottomLeft() << obs.bottomRight() << QPointF(obs.right(), obs.bottom() + 15) << QPointF(obs.left(), obs.bottom() + 15);
            painter.setBrush(QColor(110, 70, 40));
            painter.drawPolygon(side);
            painter.setPen(QPen(QColor(90, 50, 20), 3));
            painter.drawLine(obs.topLeft(), obs.bottomRight());
            painter.drawLine(obs.topRight(), obs.bottomLeft());
            painter.drawRect(obs);
        }
        else {
            // 低矮水泥墩（米黄色，带钢筋孔）
            painter.setBrush(QColor(180, 160, 120)); painter.setPen(Qt::NoPen);
            painter.drawRect(obs);
            QPolygonF side;
            side << obs.bottomLeft() << obs.bottomRight() << QPointF(obs.right(), obs.bottom() + 10) << QPointF(obs.left(), obs.bottom() + 10);
            painter.setBrush(QColor(130, 110, 80));
            painter.drawPolygon(side);
            painter.setPen(QPen(QColor(100, 80, 50), 1));
            for (int i = 0; i < 3; i++) painter.drawEllipse(obs.x() + 10 + i * 30, obs.y() + 5, 20, 20);
        }
        obsIdx++;
    }

    // ---- 绘制火焰区域 ----
    for (const FireZone& f : fireZones) {
        painter.setPen(Qt::NoPen);
        painter.fillRect(f.x - 80, f.y - 80, 160, 160, QColor(30, 25, 25));     // 暗化背景
        painter.fillRect(f.x - 60, f.y - 60, 120, 120, QColor(255, 100, 0, 150)); // 半透明橙红
    }

    // ---- 绘制掉落弹匣 ----
    for (const DroppedMag& m : droppedMags) {
        painter.save();
        painter.translate(m.x, m.y);
        painter.rotate(m.angle);
        painter.fillRect(-3, -5, 6, 10, QColor(30, 30, 30));  // 深色弹匣
        painter.restore();
    }

    // ---- 绘制玩家角色 ----
    p1.drawPseudo3D(painter, true);   // P1=CT（深蓝灰）
    p2.drawPseudo3D(painter, false);  // P2=T（绿棕）

    // ---- 绘制枪口闪光 ----
    painter.setPen(Qt::NoPen);
    for (const MuzzleFlash& mf : muzzleFlashes) {
        painter.save();
        painter.translate(mf.x, mf.y);
        painter.rotate(mf.angle);
        painter.setBrush(QColor(255, 220, 100));  // 黄白色三角形闪光
        painter.drawPolygon(QPolygonF({ QPointF(0, -3), QPointF(25, 0), QPointF(0, 3) }));
        painter.restore();
    }

    // [新增] 绘制狙击枪激光红点瞄准线
    // 仅在玩家装备 AWP 且静止（瞄准状态）时显示，动态受到障碍物遮挡
    auto drawLaser = [&](const Player& p) {
        if (p.weapons[p.weaponIndex].name == "AWP狙击" && !p.isMoving) {
            double startX = p.x + (p.facingRight ? 35.0 : -35.0);
            double startY = p.y - 12.0;
            double laserLen = 1400.0;
            double endX = startX + (p.facingRight ? laserLen : -laserLen);

            // 射线检测障碍物，被墙壁挡住则截断激光
            for (const QRectF& obs : obstacles) {
                if (obs.top() < startY && obs.bottom() > startY) {
                    if (p.facingRight && obs.left() > startX && obs.left() < endX) endX = obs.left();
                    else if (!p.facingRight && obs.right() < startX && obs.right() > endX) endX = obs.right();
                }
            }

            // 绘制半透明激光线
            painter.setPen(QPen(QColor(255, 50, 50, 120), 1, Qt::SolidLine));
            painter.drawLine(QPointF(startX, startY), QPointF(endX, startY));
            // 绘制激光落点红点
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 50, 50, 200));
            painter.drawEllipse(QPointF(endX, startY), 3, 3);
        }
        };
    drawLaser(p1);
    drawLaser(p2);

    // ---- 绘制飞行中的子弹 ----
    for (const Bullet& b : p1.bullets) { painter.fillRect(b.x - 3, b.y - 1, 6, 2, QColor(255, 200, 50)); }  // 黄色弹头
    for (const Bullet& b : p2.bullets) { painter.fillRect(b.x - 3, b.y - 1, 6, 2, QColor(255, 200, 50)); }

    // ---- 绘制弹道拖尾 ----
    for (const Tracer& t : tracers) {
        if (t.isSniper) painter.setPen(QPen(QColor(200, 200, 200), 2));  // 狙击：白色粗线
        else painter.setPen(QPen(QColor(255, 220, 50), 1));              // 普通：淡黄细线
        painter.drawLine(t.startX, t.startY, t.endX, t.endY);
    }

    // ---- 绘制粒子特效（爆炸/血花/烟雾/火焰） ----
    painter.setPen(Qt::NoPen);
    for (const Particle& p : particles) { painter.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size, p.color); }

    // ---- 绘制飞行中的投掷物 ----
    for (const FlyingGrenade& g : flyingGrenades) {
        painter.save();
        painter.translate(g.x, g.y);
        painter.rotate(g.fuseTimer * 20.0);  // 旋转动画
        WeaponRenderer::drawFlyingGrenade(painter, g.type);
        painter.restore();
    }

    // ---- 绘制烟雾区域（不透明圆形，完全遮蔽视线） ----
    for (const SmokeZone& s : smokeZones) {
        painter.setBrush(QColor(110, 115, 110, 255));  // 不透明灰绿色
        painter.drawEllipse(s.x - 130, s.y - 130, 260, 260);
    }

    // [新增] 恢复防抖的静止坐标系，开始绘制全局UI
    painter.restore();

    // ==========================================
    // 顶部比分栏
    // ==========================================
    painter.fillRect(0, 0, width(), 60, Qt::black);
    painter.setFont(QFont("Arial", 28, QFont::Bold));
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, width(), 60), Qt::AlignCenter, QString("%1  :  %2").arg(p1.wins).arg(p2.wins));

    // 战斗中显示倒计时和回合数
    if (gameState == PLAYING) {
        painter.setFont(QFont("Microsoft YaHei", 14, QFont::Bold));
        painter.setPen(Qt::yellow);
        painter.drawText(QRect(15, 0, 200, 60), Qt::AlignVCenter | Qt::AlignLeft, QString("倒计时: %1").arg(phaseTimer / 60));
        painter.setPen(Qt::lightGray);
        painter.drawText(QRect(width() - 215, 0, 200, 60), Qt::AlignVCenter | Qt::AlignRight, QString("第 %1 局 (抢8)").arg(currentRound));
    }

    // ==========================================
    // 底部玩家信息栏（战斗/购买/暂停时显示）
    // ==========================================
    if (gameState == PLAYING || gameState == BUY_PHASE || gameState == PAUSED) {
        // ---- P1 信息栏（左下） ----
        QString p1WepList = "按键: ";
        for (int i = 0; i < 6; i++) { if (p1.weapons[i].unlocked) p1WepList += QString("[%1]%2 ").arg(i + 1).arg(p1.weapons[i].name.left(2)); }
        QString p1Nades = QString("道具: [Q]雷(%1) [E]火(%2) [T]烟(%3)").arg(p1.heCount).arg(p1.molotovCount).arg(p1.smokeCount);

        painter.fillRect(10, height() - 95, 600, 85, QColor(0, 0, 0, 150));  // 半透明黑色背景
        painter.setPen(QPen(QColor(100, 150, 200), 2));                      // 蓝色边框（CT色）
        painter.drawRect(10, height() - 95, 600, 85);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
        Weapon w1 = p1.weapons[p1.weaponIndex];
        QString p1State = (p1.cooldownTimer > w1.fireCooldown && !w1.isMelee) ? "(换弹中)" : "";  // 换弹状态提示
        painter.drawText(20, height() - 70, QString("CT 防守方 | $ %1 | 当前: %2 %3").arg(p1.money).arg(w1.name).arg(p1State));
        if (!w1.isMelee) painter.drawText(330, height() - 70, QString("弹药: %1/%2 备用: %3").arg(w1.currentAmmo).arg(w1.maxAmmo).arg(w1.magsLeft));

        painter.setPen(QColor(100, 150, 200));
        painter.drawText(20, height() - 40, QString("HP: %1").arg((int)p1.hp));

        // [修改] P1 柔和蓝血条
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(80, 50, 50, 150)); // 暗灰红打底
        painter.drawRect(95, height() - 53, 150, 12);
        painter.setBrush(QColor(100, 180, 220, 180)); // 低饱和柔和蓝血条
        painter.drawRect(95, height() - 53, 150.0 * (std::max(0.0, p1.hp) / 100.0), 12);

        painter.setPen(Qt::lightGray);
        painter.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
        painter.drawText(20, height() - 15, p1WepList + "  |  " + p1Nades);

        // ---- P2 信息栏（右下） ----
        QString p2WepList = "按键: ";
        QString p2Keys[] = { "7", "8", "9", "0", "-", "=" };
        for (int i = 0; i < 6; i++) { if (p2.weapons[i].unlocked) p2WepList += QString("[%1]%2 ").arg(p2Keys[i]).arg(p2.weapons[i].name.left(2)); }
        QString p2Nades = QString("道具: [U]雷(%1) [N]火(%2) [M]烟(%3)").arg(p2.heCount).arg(p2.molotovCount).arg(p2.smokeCount);

        painter.fillRect(width() - 610, height() - 95, 600, 85, QColor(0, 0, 0, 150));
        painter.setPen(QPen(QColor(200, 120, 80), 2));                      // 橙色边框（T色）
        painter.drawRect(width() - 610, height() - 95, 600, 85);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 12, QFont::Bold));
        Weapon w2 = p2.weapons[p2.weaponIndex];
        QString p2State = (p2.cooldownTimer > w2.fireCooldown && !w2.isMelee) ? "(换弹中)" : "";
        painter.drawText(width() - 600, height() - 70, QString("T 进攻方 | $ %1 | 当前: %2 %3").arg(p2.money).arg(w2.name).arg(p2State));
        if (!w2.isMelee) painter.drawText(width() - 250, height() - 70, QString("弹药: %1/%2 备用: %3").arg(w2.currentAmmo).arg(w2.maxAmmo).arg(w2.magsLeft));

        painter.setPen(QColor(200, 120, 80));
        painter.drawText(width() - 600, height() - 40, QString("HP: %1").arg((int)p2.hp));

        // [修改] P2 柔和橙血条
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(80, 50, 50, 150)); // 暗灰红打底
        painter.drawRect(width() - 525, height() - 53, 150, 12);
        painter.setBrush(QColor(220, 140, 80, 180)); // 低饱和柔和橙色血条 (摒弃高饱和绿)
        painter.drawRect(width() - 525, height() - 53, 150.0 * (std::max(0.0, p2.hp) / 100.0), 12);

        painter.setPen(Qt::lightGray);
        painter.setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
        painter.drawText(width() - 600, height() - 15, p2WepList + "  |  " + p2Nades);
    }

    // ==========================================
    // UI覆盖层（使用uiAlpha控制透明度，实现切换动画）
    // ==========================================
    painter.setOpacity(uiAlpha);

    if (gameState == PAUSED) {
        // 暂停界面
        painter.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 200));
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 60, QFont::Bold));
        painter.drawText(QRect(0, 200, width(), 150), Qt::AlignCenter, "游 戏 已 暂 停");
        painter.setFont(QFont("Microsoft YaHei", 24));
        painter.setPen(Qt::lightGray);
        painter.drawText(QRect(0, 400, width(), 100), Qt::AlignCenter, "按 [Esc] 取消暂停，返回游戏");
        painter.setPen(QColor(200, 100, 100));
        painter.drawText(QRect(0, 500, width(), 100), Qt::AlignCenter, "按 [回车 Enter] 强制结束并退出至主菜单");
    }
    else if (gameState == START_SCREEN || gameState == MATCH_OVER) {
        // 开始界面 / 比赛结束界面
        painter.fillRect(0, 0, width(), height(), QColor(25, 28, 30, 230));
        painter.setFont(QFont("Microsoft YaHei", 50, QFont::Bold));

        if (gameState == START_SCREEN) {
            painter.setPen(QColor(220, 220, 225));
            painter.drawText(QRect(0, 50, width(), 100), Qt::AlignCenter, "反恐1v1 2d版");
        }
        else {
            // 比赛结束，显示胜者
            if (p1.wins > p2.wins) {
                painter.setPen(QColor(100, 150, 200));
                painter.drawText(QRect(0, 50, width(), 100), Qt::AlignCenter, "CT 防 守 方 取 得 胜 利 ！");
            }
            else if (p2.wins > p1.wins) {
                painter.setPen(QColor(200, 120, 80));
                painter.drawText(QRect(0, 50, width(), 100), Qt::AlignCenter, "T 进 攻 方 取 得 胜 利 ！");
            }
        }

        painter.setPen(QColor(150, 150, 150));
        painter.setFont(QFont("Microsoft YaHei", 18, QFont::Bold));
        painter.drawText(QRect(0, 150, width(), 50), Qt::AlignCenter, "【 抢 八 胜 制 】");

        // P1 操作说明（左侧面板）
        painter.fillRect(150, 220, 450, 460, QColor(40, 50, 60, 200));
        painter.setPen(QPen(QColor(100, 150, 200), 2));
        painter.drawRect(150, 220, 450, 460);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 14));
        QString p1Info = "【CT 防守方 - Player 1】\n\n移动：W A S D (八向走位)\n射击：空格 (Space)\n换弹：R\n切枪：1~6\n\n[投掷物]\n破片雷(Q) | 燃烧瓶(E) | 烟雾弹(T)\n\n\n";
        p1Info += p1_ready ? ">> 已准备就绪 <<" : "按 [空格键] 准备";
        painter.drawText(QRect(150, 250, 450, 400), Qt::AlignCenter, p1Info);

        // P2 操作说明（右侧面板）
        // [修改] T阵营背景色改为暗酒红 (摒弃偏绿色调)
        painter.fillRect(800, 220, 450, 460, QColor(60, 45, 45, 200));
        painter.setPen(QPen(QColor(200, 120, 80), 2));
        painter.drawRect(800, 220, 450, 460);
        painter.setPen(Qt::white);
        QString p2Info = "【T 进攻方 - Player 2】\n\n移动：↑ ↓ ← → (八向走位)\n射击：回车 (Enter)\n换弹：右Shift\n切枪：7, 8, 9, 0, -, =\n\n[投掷物]\n破片雷(U) | 燃烧瓶(N) | 烟雾弹(M)\n\n\n";
        p2Info += p2_ready ? ">> 已准备就绪 <<" : "按 [回车键] 准备";
        painter.drawText(QRect(800, 250, 450, 400), Qt::AlignCenter, p2Info);
    }
    else if (gameState == BUY_PHASE) {
        // 购买界面
        painter.fillRect(0, 0, width(), height(), QColor(20, 20, 20, 210));
        painter.setPen(QColor(220, 220, 225));
        painter.setFont(QFont("Microsoft YaHei", 36, QFont::Bold));
        painter.drawText(QRect(0, 60, 1400, 80), Qt::AlignCenter, "配 备 战 术 武 器");
        painter.setPen(QColor(150, 150, 150));
        painter.setFont(QFont("Microsoft YaHei", 18));
        painter.drawText(QRect(0, 130, 1400, 40), Qt::AlignCenter, "【 购买完毕后，按 B 键直接开战 】");

        // P1 购买面板
        painter.fillRect(100, 190, 550, 480, QColor(40, 50, 60, 200));
        painter.setPen(QPen(QColor(100, 150, 200), 2));
        painter.drawRect(100, 190, 550, 480);
        painter.setPen(Qt::white);
        QString buyP1 = QString("CT 防守方 资金: $%1\n\n-- 枪械 --\n[Z] 冲锋枪  $1500  |  [C] 战术霰弹  $2000\n[X] M4步枪  $2700  |  [V] 重型狙击  $4750\n\n-- 道具 (上限3个) --\n[F] 破片雷($300) | [G] 燃烧弹($400) | [Y] 烟雾($300)").arg(p1.money);
        painter.drawText(QRect(100, 210, 550, 440), Qt::AlignCenter, buyP1);

        // P2 购买面板
        // [修改] T阵营购买菜单背景色改为暗酒红
        painter.fillRect(750, 190, 550, 480, QColor(60, 45, 45, 200));
        painter.setPen(QPen(QColor(200, 120, 80), 2));
        painter.drawRect(750, 190, 550, 480);
        painter.setPen(Qt::white);
        QString buyP2 = QString("T 进攻方 资金: $%1\n\n-- 枪械 --\n[I] 冲锋枪  $1500  |  [P] 战术霰弹  $2000\n[O] M4步枪  $2700  |  [ [ ] 重型狙击  $4750\n\n-- 道具 (上限3个) --\n[J] 破片雷($300) | [K] 燃烧弹($400) | [L] 烟雾($300)").arg(p2.money);
        painter.drawText(QRect(750, 210, 550, 440), Qt::AlignCenter, buyP2);
    }
    else if (gameState == ROUND_OVER) {
        // 回合结束战报界面
        painter.fillRect(0, 0, width(), height(), QColor(20, 20, 20, 220));
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 40, QFont::Bold));
        painter.drawText(QRect(0, 60, 1400, 100), Qt::AlignCenter, "本 局 战 报");

        // P1 战报
        painter.fillRect(150, 200, 500, 400, QColor(40, 50, 60, 200));
        painter.setPen(QPen(QColor(100, 150, 200), 2));
        painter.drawRect(150, 200, 500, 400);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Microsoft YaHei", 18));
        QString p1Rep = QString("【CT 防守方 P1】\n\n爆头伤害: %1\n躯干伤害: %2\n致命武器: %3\n\n获得赏金: +$%4").arg(p1.stats.headDmgDealt).arg(p1.stats.bodyDmgDealt).arg(p1.stats.lastKillWeapon).arg(p1.stats.moneyEarnedThisRound);
        painter.drawText(QRect(150, 230, 500, 350), Qt::AlignCenter, p1Rep);

        // P2 战报
        // [修改] T阵营战报菜单背景色改为暗酒红
        painter.fillRect(750, 200, 500, 400, QColor(60, 45, 45, 200));
        painter.setPen(QPen(QColor(200, 120, 80), 2));
        painter.drawRect(750, 200, 500, 400);
        painter.setPen(Qt::white);
        QString p2Rep = QString("【T 进攻方 P2】\n\n爆头伤害: %1\n躯干伤害: %2\n致命武器: %3\n\n获得赏金: +$%4").arg(p2.stats.headDmgDealt).arg(p2.stats.bodyDmgDealt).arg(p2.stats.lastKillWeapon).arg(p2.stats.moneyEarnedThisRound);
        painter.drawText(QRect(750, 230, 500, 350), Qt::AlignCenter, p2Rep);

        // 倒计时和提示
        painter.setPen(QColor(220, 220, 225));
        painter.setFont(QFont("Microsoft YaHei", 16, QFont::Bold));
        QString dots = QString(".").repeated((180 - phaseTimer) / 15 % 4);  // 动态省略号
        int secondsLeft = (phaseTimer / 60) + 1;
        painter.drawText(QRect(0, height() - 160, width(), 50), Qt::AlignCenter, QString("正在生成下一局战术配备 %1").arg(dots));

        painter.setPen(QColor(100, 100, 100));
        painter.setFont(QFont("Microsoft YaHei", 14));
        painter.drawText(QRect(0, height() - 110, width(), 50), Qt::AlignCenter, QString("%1 秒后自动继续  |  按 [Esc] 放弃比赛并返回主菜单").arg(secondsLeft));
    }

    painter.setOpacity(1.0);  // 恢复透明度
}