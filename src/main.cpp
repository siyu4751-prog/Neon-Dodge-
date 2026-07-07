#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

const int WIDTH = 34;
const int HEIGHT = 18;

struct Item {
    int x;
    int y;
    char type; // 'X' obstacle, '$' coin, '+' life
};

int playerX = WIDTH / 2;
int score = 0;
int lives = 3;
int highScore = 0;
bool running = true;

mt19937 rng((unsigned)time(nullptr));

#ifdef _WIN32
void enableUtf8() {
    SetConsoleOutputCP(CP_UTF8);
}

void hideCursor() {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(out, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(out, &cursorInfo);
}

void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void resetColor() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
}

void playSound(const string& name) {
    if (name == "coin") Beep(900, 80);
    else if (name == "hit") Beep(220, 180);
    else if (name == "life") Beep(1200, 90);
    else if (name == "over") {
        Beep(330, 150);
        Beep(250, 220);
    }
}

bool keyHit() {
    return _kbhit();
}

char getKey() {
    return (char)_getch();
}
#else
termios oldTerm;

void enableUtf8() {}

void hideCursor() {
    cout << "\033[?25l";
}

void restoreCursor() {
    cout << "\033[?25h";
}

void setColor(int color) {
    // Simple Windows-color-number to ANSI-color mapping
    int ansi = 37;
    if (color == 10) ansi = 32;      // green
    else if (color == 11) ansi = 36; // cyan
    else if (color == 12) ansi = 31; // red
    else if (color == 14) ansi = 33; // yellow
    else if (color == 13) ansi = 35; // magenta
    else if (color == 9) ansi = 34;  // blue
    cout << "\033[" << ansi << "m";
}

void resetColor() {
    cout << "\033[0m";
}

void playSound(const string&) {
    cout << '\a' << flush; // terminal bell fallback
}

void initKeyboard() {
    tcgetattr(STDIN_FILENO, &oldTerm);
    termios newTerm = oldTerm;
    newTerm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void closeKeyboard() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
    restoreCursor();
}

bool keyHit() {
    timeval tv{0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

char getKey() {
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}
#endif

void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

void loadHighScore() {
    ifstream fin("highscore.txt");
    if (fin) fin >> highScore;
}

void saveHighScore() {
    if (score > highScore) {
        ofstream fout("highscore.txt");
        fout << score;
    }
}

void drawBorderLine() {
    setColor(9);
    cout << "+";
    for (int i = 0; i < WIDTH; i++) cout << "-";
    cout << "+\n";
    resetColor();
}

void draw(const vector<Item>& items, int level) {
    clearScreen();

    setColor(11);
    cout << "NEON DODGE  彩色躲避小游戏\n";
    resetColor();

    cout << "Score: ";
    setColor(14);
    cout << score;
    resetColor();
    cout << "   Lives: ";
    setColor(12);
    for (int i = 0; i < lives; i++) cout << "♥";
    resetColor();
    cout << "   Level: ";
    setColor(10);
    cout << level;
    resetColor();
    cout << "   Best: " << max(highScore, score) << "\n";

    drawBorderLine();

    for (int y = 0; y < HEIGHT; y++) {
        setColor(9);
        cout << "|";
        resetColor();

        for (int x = 0; x < WIDTH; x++) {
            bool printed = false;

            if (y == HEIGHT - 1 && x == playerX) {
                setColor(11);
                cout << "A";
                resetColor();
                printed = true;
            } else {
                for (const auto& item : items) {
                    if (item.x == x && item.y == y) {
                        if (item.type == 'X') {
                            setColor(12);
                            cout << "X";
                        } else if (item.type == '$') {
                            setColor(14);
                            cout << "$";
                        } else {
                            setColor(10);
                            cout << "+";
                        }
                        resetColor();
                        printed = true;
                        break;
                    }
                }
            }

            if (!printed) cout << " ";
        }

        setColor(9);
        cout << "|\n";
        resetColor();
    }

    drawBorderLine();
    cout << "操作：A/← 左移，D/→ 右移，Q 退出。接金币 $，躲障碍 X，捡 + 加命。\n";
}

void addRandomItem(vector<Item>& items, int tick) {
    uniform_int_distribution<int> xDist(0, WIDTH - 1);
    uniform_int_distribution<int> chance(1, 100);

    int probability = 28 + min(score / 80, 18);
    if (chance(rng) <= probability || tick % 7 == 0) {
        int r = chance(rng);
        char type = 'X';
        if (r <= 30) type = '$';
        if (r >= 96) type = '+';
        items.push_back({xDist(rng), 0, type});
    }
}

void handleInput() {
    while (keyHit()) {
        char key = getKey();
        if (key == 'a' || key == 'A') playerX = max(0, playerX - 1);
        else if (key == 'd' || key == 'D') playerX = min(WIDTH - 1, playerX + 1);
        else if (key == 'q' || key == 'Q') running = false;
#ifdef _WIN32
        else if (key == -32 || key == 0) {
            char arrow = getKey();
            if (arrow == 75) playerX = max(0, playerX - 1);
            if (arrow == 77) playerX = min(WIDTH - 1, playerX + 1);
        }
#endif
    }
}

void updateItems(vector<Item>& items) {
    for (auto& item : items) {
        item.y++;
    }

    vector<Item> next;
    for (auto& item : items) {
        if (item.y == HEIGHT - 1 && item.x == playerX) {
            if (item.type == 'X') {
                lives--;
                playSound("hit");
                if (lives <= 0) running = false;
            } else if (item.type == '$') {
                score += 10;
                playSound("coin");
            } else if (item.type == '+') {
                if (lives < 5) lives++;
                score += 5;
                playSound("life");
            }
        } else if (item.y < HEIGHT) {
            next.push_back(item);
        }
    }
    items = next;
}

void startScreen() {
    clearScreen();
    setColor(11);
    cout << "======================================\n";
    cout << "        NEON DODGE 彩色躲避游戏       \n";
    cout << "======================================\n";
    resetColor();
    cout << "\n玩法说明：\n";
    setColor(14);
    cout << "  $ 金币：+10 分，有提示音\n";
    setColor(12);
    cout << "  X 障碍：扣 1 条命，有碰撞音\n";
    setColor(10);
    cout << "  + 能量：回血并加分\n";
    resetColor();
    cout << "\n按 Enter 开始游戏...";
    cin.get();
}

void gameOverScreen() {
    saveHighScore();
    playSound("over");
    clearScreen();
    setColor(12);
    cout << "\n========== GAME OVER ==========\n";
    resetColor();
    cout << "你的得分：";
    setColor(14);
    cout << score << "\n";
    resetColor();
    cout << "历史最高分：" << max(highScore, score) << "\n";
    cout << "\n提示：把这个项目上传到 GitHub，README 里可以写“C++ 控制台彩色音效小游戏”。\n";
}

int main() {
    enableUtf8();
    hideCursor();
#ifndef _WIN32
    initKeyboard();
#endif

    loadHighScore();
    startScreen();

    vector<Item> items;
    int tick = 0;

    while (running) {
        int level = 1 + score / 100;
        int speed = max(55, 160 - level * 12);

        handleInput();
        addRandomItem(items, tick);
        updateItems(items);
        draw(items, level);

        tick++;
        this_thread::sleep_for(chrono::milliseconds(speed));
    }

    gameOverScreen();

#ifndef _WIN32
    closeKeyboard();
#endif
    resetColor();
    return 0;
}
