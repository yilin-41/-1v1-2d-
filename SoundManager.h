#pragma once
#include <QSoundEffect>      // 短促音效播放（枪声/脚步声等）
#include <QMediaPlayer>      // 流媒体播放器（BGM背景音乐）
#include <QAudioOutput>      // 音频输出设备管理
#include <QMap>              // 音效名称→音效对象映射
#include <QString>
#include <QStringList>
#include <QAudioDevice>      // 音频设备描述
#include <QMediaDevices>     // 获取系统默认音频设备

// ============================================================================
// 音频管理器（单例模式）
// 负责管理所有游戏音效和背景音乐
// - 短促音效：使用 QSoundEffect 播放（枪声/脚步声/爆炸等）
// - 背景音乐：使用 QMediaPlayer 流媒体播放（支持循环、智能切换）
// ============================================================================
class SoundManager {
public:
    // 获取单例实例
    static SoundManager& instance();

    // 加载所有短促音效（从qrc资源文件）
    void loadAll();
    // 播放指定名称的音效（如 "shoot_pistol", "explode_he" 等）
    void play(const QString& name);

    // ---- BGM 控制接口 ----
    // 智能播放：如果当前已在播放同一文件则不会打断
    void playBGM(const QString& fileName);
    // 强制从头播放BGM（用于比赛重置等场景）
    void restartBGM(const QString& fileName);
    // 停止BGM播放
    void stopBGM();

private:
    SoundManager();                    // 私有构造函数（单例）
    ~SoundManager();                   // 析构：释放所有音效和播放器资源
    SoundManager(const SoundManager&) = delete;            // 禁止拷贝
    SoundManager& operator=(const SoundManager&) = delete; // 禁止赋值

    // ---- 音效存储 ----
    // 短促音效映射：音效名 → QSoundEffect对象指针
    QMap<QString, QSoundEffect*> sounds;

    // ---- BGM 播放组件 ----
    QMediaPlayer* bgmPlayer;    // 流媒体播放器（主BGM播放方式）
    QAudioOutput* audioOutput;  // 音频输出设备（绑定至扬声器）
    QSoundEffect* bgmEffect;    // 备用BGM播放器（QSoundEffect方式，当前未启用）
};