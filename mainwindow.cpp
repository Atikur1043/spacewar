#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QRandomGenerator>
#include <QDebug>
#include <QFont>

const QString LEADERBOARD_FILE = "leaderboard.dat";
const int SCORE_TO_UNLOCK_MEDIUM = 500;
const int SCORE_TO_UNLOCK_HARD = 1000;
const int SCORE_TO_UNLOCK_NIGHTMARE = 2000;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    instructions_label = new QLabel(ui->frame);
    instructions_label->setVisible(false);

    // DO NOT add items to the combo box here anymore.
    // The original ui->difficulty_comboBox->addItems(...) line MUST be deleted.

    // --- NEW LOGIC ---
    campaignProgress = 0;
    currentMode = Arcade; // Default to Arcade

    // Connect the radio buttons from the .ui file
    connect(ui->arcadeModeButton, &QRadioButton::toggled, this, &MainWindow::onGameModeChanged);
    connect(ui->campaignModeButton, &QRadioButton::toggled, this, &MainWindow::onGameModeChanged);

    // Set the initial state of the difficulty dropdown
    // This will populate the combo box for Arcade mode
    updateDifficultyComboBox();
    // -----------------

    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &MainWindow::gameLoop);

    // Now it is safe to call resetGame() for the first time
    resetGame();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resetGame()
{
    gameRunning = false;
    gameTimer->stop();

    instructions_label->setVisible(false);

    int gridSize = ui->spinBox_gridSize->value();
    if (gridSize <= 0) return;

    worldWidth = ui->frame->width() / gridSize;
    worldHeight = ui->frame->height() / gridSize;

    playerHealth = 5;
    maxPlayerHealth = 5;
    powerupThreshold = 3;
    asteroidSpawnChance = 10;
    enemyShipSpawnChance = 2;
    enemyFireChance = 5;

    int difficulty = ui->difficulty_comboBox->currentIndex();
    switch(difficulty) {
    case 0: // Easy
        playerHealth = 5;
        maxPlayerHealth = 5;
        powerupThreshold = 2;
        asteroidSpawnChance = 7;
        enemyShipSpawnChance = 1;
        enemyFireChance = 3;
        break;
    case 1: // Medium
        playerHealth = 3;
        maxPlayerHealth = 3;
        powerupThreshold = 3;
        asteroidSpawnChance = 10;
        enemyShipSpawnChance = 2;
        enemyFireChance = 5;
        break;
    case 2: // Hard
        playerHealth = 2;
        maxPlayerHealth = 2;
        powerupThreshold = 4;
        asteroidSpawnChance = 13;
        enemyShipSpawnChance = 3;
        enemyFireChance = 7;
        break;
    case 3: // NIGHTMARE
        playerHealth = 1;
        maxPlayerHealth = 1;
        powerupThreshold = 5;
        asteroidSpawnChance = 16;
        enemyShipSpawnChance = 4;
        enemyFireChance = 10;
        break;
    }
    ui->health_label_value->setText(QString::number(playerHealth));

    playerScore = 0;
    ui->score_label_value->setText(QString::number(playerScore));

    killCount = 0;
    ui->kill_label_value->setText(QString::number(killCount));

    enemiesDestroyedCount = 0;
    gameTickCounter = 0;

    asteroids.clear();
    enemyShips.clear();
    enemyLasers.clear();
    playerLasers.clear();
    powerups.clear();

    playerPosition = QPoint(worldWidth / 2, worldHeight - 2);

    drawGame();
}

QVector<QPoint> MainWindow::getPlayerShipCells() const
{
    QVector<QPoint> cells;
    int px = playerPosition.x();
    int py = playerPosition.y();

    for(int i = -1; i <= 1; ++i) cells.append(QPoint(px + i, py));
    cells.append(QPoint(px, py - 1));

    return cells;
}

QVector<QPoint> MainWindow::getEnemyShipCells(const EnemyShip& ship) const
{
    QVector<QPoint> cells;
    int ex = ship.position.x();
    int ey = ship.position.y();

    for(int i = -1; i <= 1; ++i) cells.append(QPoint(ex + i, ey));
    cells.append(QPoint(ex, ey + 1));

    return cells;
}

void MainWindow::on_startGame_button_clicked()
{
    if (!gameRunning) {
        if(playerHealth <= 0){
            resetGame();
        }
        // --- ADD THIS SECTION ---
        // Show a pop-up with the goal in Campaign mode
        if (currentMode == Campaign && campaignProgress < 3) {
            int scoreNeeded = 0;
            QString levelName;
            if (campaignProgress == 0) {
                scoreNeeded = SCORE_TO_UNLOCK_MEDIUM;
                levelName = "Medium";
            } else if (campaignProgress == 1) {
                scoreNeeded = SCORE_TO_UNLOCK_HARD;
                levelName = "Hard";
            } else if (campaignProgress == 2) {
                scoreNeeded = SCORE_TO_UNLOCK_NIGHTMARE;
                levelName = "NIGHTMARE";
            }

            QMessageBox::information(this, "Campaign Goal",
                                     QString("You must score at least %1 points on this level to unlock %2.").arg(scoreNeeded).arg(levelName));
        }
        instructions_label->setVisible(false);

        gameRunning = true;
        int difficulty = ui->difficulty_comboBox->currentIndex();
        int gameSpeed = 100;
        switch(difficulty) {
        case 0: gameSpeed = 120; break;
        case 1: gameSpeed = 100; break;
        case 2: gameSpeed = 80; break;
        case 3: gameSpeed = 60; break;
        }
        gameTimer->start(gameSpeed);
        this->setFocus();
    }
}

void MainWindow::on_reset_button_clicked() {
    if (currentMode == Campaign) {
        campaignProgress = 0;
        updateDifficultyComboBox(); // Re-lock the combo box
    }
    resetGame();
}
void MainWindow::on_spinBox_gridSize_valueChanged(int arg1) { Q_UNUSED(arg1); resetGame(); }
void MainWindow::on_difficulty_comboBox_currentIndexChanged(int index) { Q_UNUSED(index); resetGame(); }

void MainWindow::on_leaderboard_button_clicked()
{
    displayLeaderboard();
}

void MainWindow::on_instructions_button_clicked()
{
    QString instructionsText =
        "--- GAME MODES ---\n\n"
        "**ARCADE MODE:**\n"
        "* Play any difficulty level you want.\n"
        "* Your scores are saved to the Leaderboard!\n"
        "* This is the classic, endless high-score experience.\n\n"
        "**CAMPAIGN MODE:**\n"
        "* Start at 'Easy' and unlock harder levels.\n"
        "* You must reach a target score to unlock the next level:\n"
        "    - Unlock Medium: 500 points on Easy\n"
        "    - Unlock Hard: 1000 points on Medium\n"
        "    - Unlock NIGHTMARE: 2000 points on Hard\n"
        "* This mode does NOT save to the leaderboard.\n"
        "* Your progress resets if you close the game or press 'Reset'.\n\n"
        "--- CONTROLS ---\n\n"
        "  A / Left Arrow : Move Left\n"
        "  D / Right Arrow : Move Right\n"
        "  Space / Enter : Shoot Laser (Cyan)\n\n"
        "--- HAZARDS ---\n\n"
        "  Asteroids (Gray): Fall straight down. Shoot them for 10 points.\n"
        "  Enemy Ships (Red): Move side to side and shoot lasers (Yellow).\n"
        "  Shoot them for 50 points.\n"
        "  Enemy Lasers (Yellow): Travel downwards.\n\n"
        "--- POWER-UPS ---\n\n"
        "  Health (Pink/Flashing): Falls slowly after enough enemies are destroyed.\n"
        "  Cannot be shot. Restores 1 health (up to max) and gives 100 points.\n\n"
        "--- GOAL ---\n\n"
        "Survive as long as possible and get a high score!";

    QMessageBox::information(this, "Instructions", instructionsText);
}

void MainWindow::on_about_button_clicked()
{
    // --- THIS IS THE FUNCTION TO EDIT ---
    QMessageBox::about(this, "About",
                       "SPACEWAR!\n\n"
                       "ABOUT SECTION : "
                       //"Version 1.0 (October 2025)\n\n"
                       //"Created by: [Your Name Here]\n\n"
                       //"Built using Qt."
                       );
    // ------------------------------------
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!gameRunning) return;

    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        if (playerPosition.x() - 1 > 0) {
            playerPosition.setX(playerPosition.x() - 1);
        }
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        if (playerPosition.x() + 1 < worldWidth - 1) {
            playerPosition.setX(playerPosition.x() + 1);
        }
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
        playerLasers.append(QPoint(playerPosition.x(), playerPosition.y() - 2));
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void MainWindow::gameLoop()
{
    if (!gameRunning) return;

    updatePositions();
    spawnEnemies();
    checkCollisions();
    drawGame();
    gameTickCounter++;
}

void MainWindow::updatePositions()
{
    for (int i = 0; i < asteroids.size(); ++i) {
        asteroids[i].setY(asteroids[i].y() + 1);
        if (asteroids[i].y() >= worldHeight) {
            asteroids.remove(i--);
        }
    }

    for (int i = 0; i < enemyShips.size(); ++i) {
        enemyShips[i].position.setX(enemyShips[i].position.x() + enemyShips[i].direction);
        if (enemyShips[i].position.x() - 1 <= 0 || enemyShips[i].position.x() + 1 >= worldWidth - 1) {
            enemyShips[i].direction *= -1;
        }
        if (QRandomGenerator::global()->bounded(100) < enemyFireChance) {
            enemyLasers.append(QPoint(enemyShips[i].position.x(), enemyShips[i].position.y() + 1));
        }
    }

    for (int i = 0; i < enemyLasers.size(); ++i) {
        enemyLasers[i].setY(enemyLasers[i].y() + 2);
        if (enemyLasers[i].y() >= worldHeight) {
            enemyLasers.remove(i--);
        }
    }

    for (int i = 0; i < playerLasers.size(); ++i) {
        playerLasers[i].setY(playerLasers[i].y() - 2);
        if (playerLasers[i].y() < 0) {
            playerLasers.remove(i--);
        }
    }

    for (int i = 0; i < powerups.size(); ++i) {
        if (gameTickCounter % 5 != 0) {
            powerups[i].setY(powerups[i].y() + 1);
        }
        if (powerups[i].y() >= worldHeight) {
            powerups.remove(i--);
        }
    }
}

void MainWindow::spawnEnemies()
{
    if (QRandomGenerator::global()->bounded(100) < asteroidSpawnChance) {
        int spawnX = QRandomGenerator::global()->bounded(worldWidth);
        asteroids.append(QPoint(spawnX, 0));
    }

    if (QRandomGenerator::global()->bounded(100) < enemyShipSpawnChance) {
        int spawnX = QRandomGenerator::global()->bounded(1, worldWidth - 2);
        int spawnY = QRandomGenerator::global()->bounded(worldHeight / 4);
        enemyShips.append({QPoint(spawnX, spawnY), 1});
    }
}

void MainWindow::spawnPowerup()
{
    if (powerups.isEmpty()) {
        int spawnX = QRandomGenerator::global()->bounded(worldWidth);
        powerups.append(QPoint(spawnX, 0));
    }
}


void MainWindow::updateScore(int points, bool countsAsKill)
{
    playerScore += points;
    ui->score_label_value->setText(QString::number(playerScore));

    if (countsAsKill) {
        killCount++;
        ui->kill_label_value->setText(QString::number(killCount));

        enemiesDestroyedCount++;
        if(enemiesDestroyedCount >= powerupThreshold) {
            spawnPowerup();
            enemiesDestroyedCount = 0;
        }
    }
}

void MainWindow::handlePlayerHit()
{
    playerHealth--;
    ui->health_label_value->setText(QString::number(playerHealth));
    if (playerHealth <= 0) {
        gameRunning = false;
        gameTimer->stop();
        if (currentMode == Campaign) {
            checkForCampaignUnlock();
        }
        promptForLeaderboard();
    }
}

void MainWindow::promptForLeaderboard()
{
    if (currentMode == Campaign) {
        return;
    }
    if (playerScore == 0 && killCount == 0) return;

    bool ok;
    QString name = QInputDialog::getText(this, "Game Over - Save Score",
                                         "Enter your name (max 5 letters):",
                                         QLineEdit::Normal, "", &ok);
    while (ok && (name.isEmpty() || name.length() > 5)) {
        name = QInputDialog::getText(this, "Game Over - Save Score",
                                     "Invalid. Enter 1-5 letters:",
                                     QLineEdit::Normal, "", &ok);
    }

    if (ok && !name.isEmpty()) {
        QString difficulty = ui->difficulty_comboBox->currentText();
        saveScore(name.toUpper(), playerScore, killCount, difficulty);
    }
}

void MainWindow::saveScore(const QString& name, int score, int kills, const QString& difficulty)
{
    QFile file(LEADERBOARD_FILE);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << name << "," << score << "," << kills << "," << difficulty << "\n";
        file.close();
    } else {
        QMessageBox::warning(this, "Error", "Could not save score to leaderboard file.");
    }
}

struct ScoreEntry {
    QString name;
    int score;
    int kills;
    QString difficulty;
};

bool compareScores(const ScoreEntry &a, const ScoreEntry &b) {
    return a.score > b.score;
}

void MainWindow::displayLeaderboard()
{
    QVector<ScoreEntry> allScores;
    QFile file(LEADERBOARD_FILE);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split(',');
            if (parts.size() == 4) {
                allScores.append({parts[0], parts[1].toInt(), parts[2].toInt(), parts[3]});
            } else if (parts.size() == 3) {
                allScores.append({parts[0], parts[1].toInt(), 0, parts[2]});
            }
        }
        file.close();
    } else {
        QMessageBox::information(this, "Leaderboard", "No scores yet. Play a game!");
        return;
    }

    std::sort(allScores.begin(), allScores.end(), compareScores);

    QMap<QString, QVector<ScoreEntry>> diffScores;
    for(const auto& entry : allScores) {
        diffScores[entry.difficulty].append(entry);
    }

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Leaderboard");
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTextBrowser *browser = new QTextBrowser();
    browser->setFont(QFont("Monospace", 10));

    QString html = "<html><body>";

    QStringList difficulties = {"NIGHTMARE", "Hard", "Medium", "Easy"};
    for(const QString& diff : difficulties) {
        html += QString("<h2><u>%1</u></h2>").arg(diff.toUpper());
        if(diffScores.contains(diff)) {
            html += "<pre>";
            html += QString(" #  NAME   SCORE   KILLS\n");
            html += QString("--- ----- ------- -------\n");
            int count = 0;
            for(const auto& entry : diffScores[diff]) {
                if(++count > 10) break;
                html += QString("%1. %2 %3 %4\n")
                            .arg(count, 2, 10, QChar(' '))
                            .arg(entry.name.leftJustified(5, ' '))
                            .arg(entry.score, 7, 10, QChar(' '))
                            .arg(entry.kills, 7, 10, QChar(' '));
            }
            html += "</pre>";
        } else {
            html += "<p>No scores yet.</p>";
        }
    }

    html += "</body></html>";
    browser->setHtml(html);
    layout->addWidget(browser);
    dialog->setLayout(layout);
    dialog->resize(450, 500);
    dialog->exec();
}


void MainWindow::checkCollisions()
{
    for (int i = 0; i < playerLasers.size(); ++i) {
        bool laserHit = false;
        QPoint laserPos = playerLasers[i];
        QPoint skippedLaserPos(laserPos.x(), laserPos.y() + 1);

        for (int j = 0; j < asteroids.size(); ++j) {
            QPoint asteroidPos = asteroids[j];
            QPoint oldAsteroidPos(asteroidPos.x(), asteroidPos.y() - 1);

            if (asteroidPos == laserPos || asteroidPos == skippedLaserPos || oldAsteroidPos == laserPos) {
                playerLasers.remove(i--);
                asteroids.remove(j);
                laserHit = true;
                updateScore(10, true);
                break;
            }
        }
        if (laserHit) continue;

        for (int j = 0; j < enemyShips.size(); ++j) {
            QVector<QPoint> shipCells = getEnemyShipCells(enemyShips[j]);
            if (shipCells.contains(laserPos) || shipCells.contains(skippedLaserPos)) {
                playerLasers.remove(i--);
                enemyShips.remove(j);
                laserHit = true;
                updateScore(50, true);
                break;
            }
        }
    }

    QVector<QPoint> playerCells = getPlayerShipCells();

    for (int i = 0; i < asteroids.size(); ++i) {
        QPoint asteroidPos = asteroids[i];
        QPoint oldAsteroidPos(asteroidPos.x(), asteroidPos.y() - 1);

        if (playerCells.contains(asteroidPos) || playerCells.contains(oldAsteroidPos)) {
            asteroids.remove(i--);
            handlePlayerHit();
            if (!gameRunning) return;
        }
    }

    for (int i = 0; i < enemyShips.size(); ++i) {
        QVector<QPoint> enemyCells = getEnemyShipCells(enemyShips[i]);
        bool shipsCollide = false;
        for(const QPoint& p_cell : playerCells) {
            if(enemyCells.contains(p_cell)) {
                shipsCollide = true;
                break;
            }
        }
        if(shipsCollide) {
            enemyShips.remove(i--);
            handlePlayerHit();
            if (!gameRunning) return;
        }
    }

    for (int i = 0; i < enemyLasers.size(); ++i) {
        QPoint laserPos = enemyLasers[i];
        QPoint skippedLaserPos(laserPos.x(), laserPos.y() - 1);

        if (playerCells.contains(laserPos) || playerCells.contains(skippedLaserPos)) {
            enemyLasers.remove(i--);
            handlePlayerHit();
            if (!gameRunning) return;
        }
    }

    for (int i = 0; i < powerups.size(); ++i) {
        QPoint powerupPos = powerups[i];
        QPoint oldPowerupPos(powerupPos.x(), powerupPos.y() - (gameTickCounter % 5 != 0 ? 1: 0));

        if (playerCells.contains(powerupPos) || playerCells.contains(oldPowerupPos)) {
            powerups.remove(i--);
            playerHealth = qMin(playerHealth + 1, maxPlayerHealth);
            ui->health_label_value->setText(QString::number(playerHealth));
            updateScore(100, false);
        }
    }
}


void MainWindow::drawGame()
{
    clearCanvas();
    int gridSize = ui->spinBox_gridSize->value();
    if (gridSize <= 0) return;

    QPixmap pm = ui->frame->pixmap();
    QPainter painter(&pm);
    drawGrid(gridSize);

    if(gameRunning) {
        for(const QPoint& cell : getPlayerShipCells()) {
            drawCell(painter, cell.x(), cell.y(), gridSize, Qt::green);
        }
    }

    for (const QPoint &laser : playerLasers) {
        drawCell(painter, laser.x(), laser.y(), gridSize, Qt::cyan);
    }

    for (const QPoint &asteroid : asteroids) {
        drawCell(painter, asteroid.x(), asteroid.y(), gridSize, Qt::gray);
    }

    for (const EnemyShip &ship : enemyShips) {
        for(const QPoint& cell : getEnemyShipCells(ship)) {
            drawCell(painter, cell.x(), cell.y(), gridSize, Qt::red);
        }
    }

    for (const QPoint &laser : enemyLasers) {
        drawCell(painter, laser.x(), laser.y(), gridSize, Qt::yellow);
    }

    for (const QPoint &powerup : powerups) {
        QColor powerupColor = (gameTickCounter / 10) % 2 == 0 ? Qt::magenta : QColor(200, 0, 200);
        drawCell(painter, powerup.x(), powerup.y(), gridSize, powerupColor);
    }

    if (!gameRunning && playerHealth <= 0) {
        int textWidth = 15;
        int startY = (worldHeight / 2) - 5;
        int textX = (worldWidth - textWidth + 1) / 2;
        drawBlockyText(painter, "GAME", textX, startY, Qt::red);
        drawBlockyText(painter, "OVER", textX, startY + 6, Qt::red);
    }

    painter.end();
    ui->frame->setPixmap(pm);
}

void MainWindow::drawCell(QPainter &painter, int cellX, int cellY, int gridSize, QColor color) {
    if (gridSize <= 0) return;
    painter.fillRect(cellX * gridSize, cellY * gridSize, gridSize, gridSize, color);
}

void MainWindow::clearCanvas() {
    QPixmap pm(ui->frame->size());
    pm.fill(Qt::black);
    ui->frame->setPixmap(pm);
}

void MainWindow::drawGrid(int gridSize) {
    if (gridSize <= 0) return;
    QPixmap pm = ui->frame->pixmap();
    QPainter painter(&pm);
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    for(int i = 0; i <= ui->frame->width(); i += gridSize) painter.drawLine(i, 0, i, ui->frame->height());
    for(int i = 0; i <= ui->frame->height(); i += gridSize) painter.drawLine(0, i, ui->frame->width(), i);
    painter.end();
    ui->frame->setPixmap(pm);
}

void MainWindow::drawBlockyText(QPainter &painter, const QString& text, int startX, int startY, QColor color)
{
    int gridSize = ui->spinBox_gridSize->value();
    int currentX = startX;

    for (QChar c : text)
    {
        switch (c.toUpper().unicode())
        {
        case 'G':
            drawCell(painter, currentX + 1, startY, gridSize, color);
            drawCell(painter, currentX + 2, startY, gridSize, color);
            drawCell(painter, currentX, startY + 1, gridSize, color);
            drawCell(painter, currentX, startY + 2, gridSize, color);
            drawCell(painter, currentX, startY + 3, gridSize, color);
            drawCell(painter, currentX + 2, startY + 3, gridSize, color);
            drawCell(painter, currentX + 1, startY + 4, gridSize, color);
            drawCell(painter, currentX + 2, startY + 4, gridSize, color);
            currentX += 4;
            break;
        case 'A':
            for(int i=0; i<5; ++i) drawCell(painter, currentX, startY+i, gridSize, color);
            for(int i=0; i<5; ++i) drawCell(painter, currentX+2, startY+i, gridSize, color);
            drawCell(painter, currentX+1, startY, gridSize, color);
            drawCell(painter, currentX+1, startY+2, gridSize, color);
            currentX += 4;
            break;
        case 'M':
            for(int i=0; i<5; ++i) drawCell(painter, currentX, startY+i, gridSize, color);
            for(int i=0; i<5; ++i) drawCell(painter, currentX+2, startY+i, gridSize, color);
            drawCell(painter, currentX+1, startY+1, gridSize, color);
            currentX += 4;
            break;
        case 'E':
            for(int i=0; i<5; ++i) drawCell(painter, currentX, startY+i, gridSize, color);
            for(int i=0; i<3; ++i) drawCell(painter, currentX+i, startY, gridSize, color);
            for(int i=0; i<2; ++i) drawCell(painter, currentX+i, startY+2, gridSize, color);
            for(int i=0; i<3; ++i) drawCell(painter, currentX+i, startY+4, gridSize, color);
            currentX += 4;
            break;
        case 'O':
            for(int i=0; i<5; ++i) drawCell(painter, currentX, startY+i, gridSize, color);
            for(int i=0; i<5; ++i) drawCell(painter, currentX+2, startY+i, gridSize, color);
            drawCell(painter, currentX+1, startY, gridSize, color);
            drawCell(painter, currentX+1, startY+4, gridSize, color);
            currentX += 4;
            break;
        case 'V':
            for(int i=0; i<4; ++i) drawCell(painter, currentX, startY+i, gridSize, color);
            for(int i=0; i<4; ++i) drawCell(painter, currentX+2, startY+i, gridSize, color);
            drawCell(painter, currentX+1, startY+4, gridSize, color);
            currentX += 4;
            break;
        case 'R':
            for(int i=0; i<5; ++i) drawCell(painter, currentX, startY+i, gridSize, color);
            drawCell(painter, currentX+1, startY, gridSize, color);
            drawCell(painter, currentX+2, startY+1, gridSize, color);
            drawCell(painter, currentX+1, startY+2, gridSize, color);
            drawCell(painter, currentX+2, startY+3, gridSize, color);
            drawCell(painter, currentX+2, startY+4, gridSize, color);
            currentX += 4;
            break;
        }
    }
}

/**
 * @brief Slot triggered when a game mode radio button is toggled.
 */
void MainWindow::onGameModeChanged()
{
    // Check which button is now active
    if (ui->arcadeModeButton->isChecked()) {
        currentMode = Arcade;
    } else {
        currentMode = Campaign;
        campaignProgress = 0;
    }

    // Update the difficulty combo box to reflect new mode's rules
    updateDifficultyComboBox();

    // Reset the game to apply changes
    resetGame();
}

/**
 * @brief Updates the difficulty combo box based on game mode and progress.
 */
void MainWindow::updateDifficultyComboBox()
{
    // CRITICAL: Block signals to prevent resetGame() from
    // being called when we clear() the combo box.
    ui->difficulty_comboBox->blockSignals(true);
    ui->difficulty_comboBox->clear();

    if (currentMode == Arcade) {
        // Arcade mode has all levels unlocked
        ui->difficulty_comboBox->addItems({"Easy", "Medium", "Hard", "NIGHTMARE"});
        ui->difficulty_comboBox->setEnabled(true);
        ui->difficulty_comboBox->setCurrentIndex(0); // Default to Easy
    } else {
        // Campaign mode unlocks levels progressively
        ui->difficulty_comboBox->addItem("Easy");
        if (campaignProgress >= 1) ui->difficulty_comboBox->addItem("Medium");
        if (campaignProgress >= 2) ui->difficulty_comboBox->addItem("Hard");
        if (campaignProgress >= 3) ui->difficulty_comboBox->addItem("NIGHTMARE");

        if (campaignProgress < 3) {
            // If not all levels are unlocked, lock to the highest available
            ui->difficulty_comboBox->setCurrentIndex(campaignProgress);
            ui->difficulty_comboBox->setEnabled(false); // Disable dropdown
        } else {
            // All levels unlocked, allow user to choose
            ui->difficulty_comboBox->setEnabled(true);
            ui->difficulty_comboBox->setCurrentIndex(campaignProgress); // Default to highest
        }
    }

    // CRITICAL: Unblock signals now that we are done
    ui->difficulty_comboBox->blockSignals(false);
}

/**
 * @brief Checks if the player's score is high enough to unlock the next level.
 */
void MainWindow::checkForCampaignUnlock()
{
    int currentDifficulty = ui->difficulty_comboBox->currentIndex();
    int newProgress = campaignProgress;
    bool unlocked = false;
    QString nextLevelName;

    // Check for unlock only if we just played the highest unlocked level
    if (currentDifficulty == campaignProgress && campaignProgress < 3) {
        if (currentDifficulty == 0 && playerScore >= SCORE_TO_UNLOCK_MEDIUM) {
            newProgress = 1; // Unlock Medium
            nextLevelName = "Medium";
            unlocked = true;
        } else if (currentDifficulty == 1 && playerScore >= SCORE_TO_UNLOCK_HARD) {
            newProgress = 2; // Unlock Hard
            nextLevelName = "Hard";
            unlocked = true;
        } else if (currentDifficulty == 2 && playerScore >= SCORE_TO_UNLOCK_NIGHTMARE) {
            newProgress = 3; // Unlock Nightmare
            nextLevelName = "NIGHTMARE";
            unlocked = true;
        }
    }

    if (unlocked) {
        campaignProgress = newProgress;

        // Notify player
        QMessageBox::information(this, "Level Unlocked!",
                                 QString("Congratulations! You scored %1 points and unlocked the %2 difficulty level!").arg(playerScore).arg(nextLevelName));

        // Update the UI to reflect the unlock
        updateDifficultyComboBox();
    } else if (currentDifficulty == campaignProgress && campaignProgress < 3) {
        // Player failed to unlock the next level
        int scoreNeeded = 0;
        if (currentDifficulty == 0) scoreNeeded = SCORE_TO_UNLOCK_MEDIUM;
        if (currentDifficulty == 1) scoreNeeded = SCORE_TO_UNLOCK_HARD;
        if (currentDifficulty == 2) scoreNeeded = SCORE_TO_UNLOCK_NIGHTMARE;

        QMessageBox::information(this, "Game Over",
                                 QString("You scored %1 points. You need %2 points on this level to unlock the next one.").arg(playerScore).arg(scoreNeeded));
    }
}
