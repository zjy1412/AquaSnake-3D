#include "music.h"
#include <QRandomGenerator>
#include <QUrl>
#include <QDebug>
#include <QCoreApplication>

MusicManager::MusicManager(QObject *parent)
    : QObject(parent)
    , player(new QMediaPlayer(this))
    , audioOutput(new QAudioOutput(this))
    , delayTimer(new QTimer(this))
    , currentGameMusicIndex(-1)
    , isInGame(false)
{
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.8); // 增加音量到80%
    
    // 设置延迟计时器
    delayTimer->setInterval(5000); // 5秒延迟
    delayTimer->setSingleShot(true);
    
    connect(player, &QMediaPlayer::mediaStatusChanged,
            this, &MusicManager::onMediaStatusChanged);
    connect(delayTimer, &QTimer::timeout,
            this, &MusicManager::playNextGameMusic);
            
    // 添加错误处理
    connect(player, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error error, const QString &errorString) {
        qDebug() << "Media player error:" << error << errorString;
    });
            
    loadMusicFiles();
}

MusicManager::~MusicManager()
{
    stopMusic();
}

void MusicManager::loadMusicFiles()
{
    // 获取应用程序目录
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 加载游戏音乐列表
    QDir musicDir(appDir + "/music/gaming");
    qDebug() << "Looking for gaming music in:" << musicDir.absolutePath();
    
    if (musicDir.exists()) {
        QStringList filters;
        filters << "*.mp3" << "*.wav" << "*.ogg" << "*.MP3";
        QStringList musicFiles = musicDir.entryList(filters, QDir::Files);
        qDebug() << "Found gaming music files:" << musicFiles;
        
        for (const QString& file : musicFiles) {
            gameMusicList.push_back(musicDir.absoluteFilePath(file));
        }
    } else {
        qDebug() << "Gaming music directory does not exist!";
    }
}

void MusicManager::playMenuMusic()
{
    isInGame = false;
    delayTimer->stop();
    
    // 获取应用程序目录
    QString appDir = QCoreApplication::applicationDirPath();
    QDir menuMusicDir(appDir + "/music/menu");
    qDebug() << "Looking for menu music in:" << menuMusicDir.absolutePath();
    
    if (menuMusicDir.exists()) {
        QStringList filters;
        filters << "*.mp3" << "*.wav" << "*.ogg" << "*.MP3";
        QStringList musicFiles = menuMusicDir.entryList(filters, QDir::Files);
        qDebug() << "Found menu music files:" << musicFiles;
        
        if (!musicFiles.isEmpty()) {
            QString menuMusic = menuMusicDir.absoluteFilePath(musicFiles.first());
            qDebug() << "Playing menu music:" << menuMusic;
            player->setSource(QUrl::fromLocalFile(menuMusic));
            player->play();
        } else {
            qDebug() << "No menu music files found!";
        }
    } else {
        qDebug() << "Menu music directory does not exist!";
    }
}

void MusicManager::startGameMusic()
{
    isInGame = true;
    if (!gameMusicList.empty()) {
        qDebug() << "Starting game music, available tracks:" << gameMusicList.size();
        currentGameMusicIndex = -1;
        playNextGameMusic();
    } else {
        qDebug() << "No game music available!";
    }
}

void MusicManager::stopMusic()
{
    isInGame = false;
    delayTimer->stop();
    player->stop();
    qDebug() << "Music stopped";
}

QString MusicManager::getRandomGameMusic()
{
    if (gameMusicList.empty()) {
        qDebug() << "No game music available for random selection";
        return QString();
    }
    
    int newIndex;
    if (gameMusicList.size() == 1) {
        newIndex = 0;
    } else {
        do {
            newIndex = QRandomGenerator::global()->bounded(gameMusicList.size());
        } while (newIndex == currentGameMusicIndex);
    }
    
    currentGameMusicIndex = newIndex;
    qDebug() << "Selected game music track:" << currentGameMusicIndex << gameMusicList[currentGameMusicIndex];
    return gameMusicList[currentGameMusicIndex];
}

void MusicManager::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "Media status changed:" << status;
    if (status == QMediaPlayer::EndOfMedia) {
        if (isInGame) {
            qDebug() << "Starting delay timer for next track";
            delayTimer->start(); // 启动5秒延迟
        }
    }
}

void MusicManager::playNextGameMusic()
{
    if (!isInGame) return;
    
    QString nextMusic = getRandomGameMusic();
    if (!nextMusic.isEmpty()) {
        qDebug() << "Playing next game music:" << nextMusic;
        player->setSource(QUrl::fromLocalFile(nextMusic));
        player->play();
    }
} 