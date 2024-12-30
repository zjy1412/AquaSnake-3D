#ifndef MUSIC_H
#define MUSIC_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QDir>
#include <QTimer>
#include <vector>
#include <random>

class MusicManager : public QObject
{
    Q_OBJECT

public:
    explicit MusicManager(QObject *parent = nullptr);
    ~MusicManager();

    void playMenuMusic();
    void startGameMusic();
    void stopMusic();

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void playNextGameMusic();

private:
    QMediaPlayer* player;
    QAudioOutput* audioOutput;
    QTimer* delayTimer;
    std::vector<QString> gameMusicList;
    int currentGameMusicIndex;
    bool isInGame;
    
    void loadMusicFiles();
    QString getRandomGameMusic();
};

#endif // MUSIC_H 