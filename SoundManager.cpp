#include "SoundManager.h"
#include <QUrl>
#include <QDebug>
#include <QFile>
#include <QCoreApplication>
#include <QDir>

// ============================================================================
// 获取单例实例
// 使用静态局部变量保证线程安全（C++11起）
// ============================================================================
SoundManager& SoundManager::instance() {
    static SoundManager s;
    return s;
}

// ============================================================================
// 构造函数：初始化音频设备和BGM播放器
// ============================================================================
SoundManager::SoundManager() {
    // ========== 获取默认扬声器设备 ==========
    QAudioDevice defaultSpeaker = QMediaDevices::defaultAudioOutput();
    qDebug() << "Default audio output device:" << defaultSpeaker.description();
    qDebug() << "Default audio output device id:" << defaultSpeaker.id();

    // 列出所有可用的音频输出设备（调试用）
    const auto devices = QMediaDevices::audioOutputs();
    qDebug() << "Available audio output devices:";
    for (const auto& device : devices) {
        qDebug() << "  -" << device.description() << (device.isDefault() ? "(default)" : "");
    }

    // ========== 初始化 BGM 播放器 ==========
    bgmPlayer = new QMediaPlayer();
    audioOutput = new QAudioOutput();

    // 显式设置为默认扬声器设备
    audioOutput->setDevice(defaultSpeaker);
    audioOutput->setVolume(0.15); // BGM 音量设为 15%，作为氛围底音

    bgmPlayer->setAudioOutput(audioOutput);
    bgmPlayer->setLoops(QMediaPlayer::Infinite); // 无限循环播放

    // ========== 备用 BGM 效果器（QSoundEffect方式，当前未启用） ==========
    bgmEffect = new QSoundEffect();
    bgmEffect->setVolume(0.15);
    bgmEffect->setLoopCount(QSoundEffect::Infinite);

    // ========== 调试信号连接：监控BGM播放状态 ==========
    QObject::connect(bgmPlayer, &QMediaPlayer::errorOccurred, [](QMediaPlayer::Error error, const QString& errorString) {
        qDebug() << "BGM Player Error:" << error << errorString;
        });

    QObject::connect(bgmPlayer, &QMediaPlayer::playbackStateChanged, [](QMediaPlayer::PlaybackState state) {
        qDebug() << "BGM PlaybackState changed to:" << state;
        });

    QObject::connect(bgmPlayer, &QMediaPlayer::mediaStatusChanged, [](QMediaPlayer::MediaStatus status) {
        qDebug() << "BGM MediaStatus changed to:" << status;
        });
}

// ============================================================================
// 析构函数：释放所有音效对象和播放器资源
// ============================================================================
SoundManager::~SoundManager() {
    // 释放所有短促音效
    for (auto& sound : sounds) {
        delete sound;
    }
    sounds.clear();

    // 释放BGM播放组件
    delete bgmPlayer;
    delete audioOutput;
    delete bgmEffect;
}

// ============================================================================
// 加载所有短促音效
// 从qrc资源文件中预加载14个WAV音效，绑定至扬声器设备
// ============================================================================
void SoundManager::loadAll() {
    // 获取默认扬声器设备，用于所有音效
    QAudioDevice defaultSpeaker = QMediaDevices::defaultAudioOutput();

    // 全部音效文件名列表（对应 res/ 目录下的 .wav 文件）
    QStringList names = {
        "buy_item", "empty_click", "explode_he", "explode_molotov",
        "explode_smoke", "footstep", "hit_body", "knife_swing",
        "reload_mag", "shoot_pistol", "shoot_rifle", "shoot_shotgun",
        "shoot_smg", "shoot_sniper"
    };

    // 逐个加载音效
    for (const QString& name : names) {
        QSoundEffect* effect = new QSoundEffect();
        QString path = "qrc:/GameWindow/res/" + name + ".wav";  // qrc资源路径
        effect->setSource(QUrl(path));
        effect->setVolume(0.5);  // 音效音量50%

        // 设置音频输出设备为扬声器
        effect->setAudioDevice(defaultSpeaker);

        sounds[name] = effect;  // 存入映射表

        // 检查音效是否加载成功
        if (effect->status() == QSoundEffect::Error) {
            qDebug() << "Failed to load sound:" << name;
        }
    }
}

// ============================================================================
// 播放指定短促音效
// @param name 音效名称（如 "shoot_pistol", "explode_he" 等）
// ============================================================================
void SoundManager::play(const QString& name) {
    if (sounds.contains(name)) {
        QSoundEffect* effect = sounds[name];

        // 确保使用扬声器设备
        QAudioDevice defaultSpeaker = QMediaDevices::defaultAudioOutput();
        effect->setAudioDevice(defaultSpeaker);

        effect->play();  // 触发播放（QSoundEffect会自动处理重叠播放）
    }
}

// ============================================================================
// 智能播放BGM：如果已经在播同一首歌，不会打断它（无缝循环）
// 搜索路径优先级：程序目录 → 工作目录 → 上级目录 → qrc资源
// @param fileName BGM文件名（如 "bgm.mp3"）
// ============================================================================
void SoundManager::playBGM(const QString& fileName) {
    QStringList searchPaths;

    // 按优先级构建搜索路径列表
    searchPaths << QCoreApplication::applicationDirPath() + "/res/" + fileName;  // 1. 应用程序所在目录
    searchPaths << QDir::currentPath() + "/res/" + fileName;                     // 2. 当前工作目录
    searchPaths << QDir::currentPath() + "/../res/" + fileName;                  // 3. 上级目录（开发环境常见）
    searchPaths << ":/GameWindow/res/" + fileName;                               // 4. qrc内嵌资源

    // 查找第一个存在的文件
    QString foundPath;
    for (const QString& path : searchPaths) {
        qDebug() << "Trying BGM path:" << path;
        if (QFile::exists(path)) {
            foundPath = path;
            qDebug() << "Found BGM at:" << foundPath;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        qDebug() << "BGM file not found in any search path:" << fileName;
        qDebug() << "Searched paths:" << searchPaths;
        return;
    }

    // 确保音频输出设备是扬声器
    QAudioDevice defaultSpeaker = QMediaDevices::defaultAudioOutput();
    audioOutput->setDevice(defaultSpeaker);

    // 构建QUrl（区分本地文件路径和qrc资源路径）
    QUrl newUrl;
    if (foundPath.startsWith(":")) {
        newUrl = QUrl("qrc" + foundPath);     // qrc资源
    }
    else {
        newUrl = QUrl::fromLocalFile(foundPath);  // 本地文件
    }

    qDebug() << "playBGM URL:" << newUrl.toString();

    // 智能判断：仅当源不同或不在播放状态时才重新设置并播放
    if (bgmPlayer->source() != newUrl || bgmPlayer->playbackState() != QMediaPlayer::PlayingState) {
        bgmPlayer->setSource(newUrl);
        bgmPlayer->play();
        qDebug() << "BGM started playing:" << fileName;
        qDebug() << "BGM using audio device:" << audioOutput->device().description();
        qDebug() << "BGM playback state:" << bgmPlayer->playbackState();
        qDebug() << "BGM error:" << bgmPlayer->error();
        qDebug() << "BGM error string:" << bgmPlayer->errorString();
    }
}

// ============================================================================
// 强制重头播放BGM（先stop再play，用于比赛重置等需要重新开始的场景）
// @param fileName BGM文件名
// ============================================================================
void SoundManager::restartBGM(const QString& fileName) {
    QStringList searchPaths;

    // 搜索逻辑与 playBGM 一致
    searchPaths << QCoreApplication::applicationDirPath() + "/res/" + fileName;
    searchPaths << QDir::currentPath() + "/res/" + fileName;
    searchPaths << QDir::currentPath() + "/../res/" + fileName;
    searchPaths << ":/GameWindow/res/" + fileName;

    QString foundPath;
    for (const QString& path : searchPaths) {
        qDebug() << "Trying BGM path:" << path;
        if (QFile::exists(path)) {
            foundPath = path;
            qDebug() << "Found BGM at:" << foundPath;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        qDebug() << "BGM file not found in any search path:" << fileName;
        return;
    }

    // 确保音频输出设备是扬声器
    QAudioDevice defaultSpeaker = QMediaDevices::defaultAudioOutput();
    audioOutput->setDevice(defaultSpeaker);

    QUrl newUrl;
    if (foundPath.startsWith(":")) {
        newUrl = QUrl("qrc" + foundPath);
    }
    else {
        newUrl = QUrl::fromLocalFile(foundPath);
    }

    qDebug() << "restartBGM URL:" << newUrl.toString();

    // 强制停止后重新播放
    bgmPlayer->stop();
    bgmPlayer->setSource(newUrl);
    bgmPlayer->play();
    qDebug() << "BGM restarted:" << fileName;
    qDebug() << "BGM using audio device:" << audioOutput->device().description();
    qDebug() << "BGM playback state:" << bgmPlayer->playbackState();
    qDebug() << "BGM error:" << bgmPlayer->error();
}

// ============================================================================
// 停止BGM播放
// ============================================================================
void SoundManager::stopBGM() {
    bgmPlayer->stop();
}