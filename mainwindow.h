#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QPainter>
#include <QVector>
#include <QKeyEvent>
#include <QTimer>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextBrowser>
#include <QDialog>
#include <QVBoxLayout>
#include <algorithm>

enum GameMode {
    Arcade,
    Campaign
};

struct EnemyShip {
    QPoint position;
    int direction;
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void on_spinBox_gridSize_valueChanged(int arg1);
    void on_startGame_button_clicked();
    void on_reset_button_clicked();
    void on_difficulty_comboBox_currentIndexChanged(int index);
    void on_leaderboard_button_clicked();
    void on_instructions_button_clicked();
    void on_about_button_clicked(); // Added slot
    void gameLoop();
    void onGameModeChanged();

private:
    Ui::MainWindow *ui;
    QLabel *instructions_label;

    QTimer *gameTimer;
    bool gameRunning;
    int worldWidth;
    int worldHeight;
    int playerHealth;
    int playerScore;
    int killCount;
    int gameTickCounter;
    int enemiesDestroyedCount;
    int powerupThreshold;
    int maxPlayerHealth;
    int asteroidSpawnChance;
    int enemyShipSpawnChance;
    int enemyFireChance;
    GameMode currentMode;      // Stores whether we are in Arcade or Campaign
    int campaignProgress;

    QPoint playerPosition;
    QVector<QPoint> asteroids;
    QVector<EnemyShip> enemyShips;
    QVector<QPoint> enemyLasers;
    QVector<QPoint> playerLasers;
    QVector<QPoint> powerups;

    void resetGame();
    void drawGame();
    void updatePositions();
    void spawnEnemies();
    void spawnPowerup();
    void checkCollisions();
    void handlePlayerHit();
    void updateScore(int points, bool countsAsKill);
    void promptForLeaderboard();
    void saveScore(const QString& name, int score, int kills, const QString& difficulty);
    void displayLeaderboard();
    void updateDifficultyComboBox();
    void checkForCampaignUnlock();

    QVector<QPoint> getPlayerShipCells() const;
    QVector<QPoint> getEnemyShipCells(const EnemyShip& ship) const;

    void clearCanvas();
    void drawGrid(int gridSize);
    void drawCell(QPainter &painter, int cellX, int cellY, int gridSize, QColor color);
    void drawBlockyText(QPainter &painter, const QString& text, int startX, int startY, QColor color);
};

#endif // MAINWINDOW_H
