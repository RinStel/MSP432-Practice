// Include application, user and local libraries
#include <SPI.h>

#include <LCD_screen.h>
#include <LCD_screen_font.h>
#include <LCD_utilities.h>
#include <Screen_HX8353E.h>
Screen_HX8353E myScreen;

#define BUTTON_ONE 33
#define BUTTON_TWO 32
#define Joystick_X 2
#define Joystick_Y 26

#define FIELD_X 110
#define FIELD_Y 110
#define MAX_CELL_SIZE 5
int CELL_SIZE = 3;       // 最小细胞尺寸约定为 2

#define MAX_VIEW_W 42
#define MAX_VIEW_H 42
#define DISP_BUF_BITS (MAX_VIEW_W * MAX_VIEW_H)
#define DISP_BUF_WORDS ((DISP_BUF_BITS + 31) / 32)

typedef struct{
  uint32_t buf       : 25;  // 淡入淡出缓存 (位0-24)
  uint32_t stateLast : 1;   // 上一帧状态 (位25)
  uint32_t state     : 1;   // 当前状态 (位26)
}Cell_t;
Cell_t cell[FIELD_X][FIELD_Y];

typedef struct{
  int W, H;                          // 可见区域大小（细胞数）
  int camX, camY;                    // 可见区域左上角细胞坐标
  int lastCamX, lastCamY;            // 上一帧相机位置
  int camTick;                       // 摇杆速率限制
  uint32_t dispBuf[DISP_BUF_WORDS];  // 显示缓冲区
}Viewport;
Viewport view;

typedef struct{
  int tick = 0;
  int displayMode = 0;  // 0=非淡入淡出, 1=淡入淡出
  int liveRatio = 30;   // 随机初始化的活细胞比例(%)
  int Choose = 1;       // 菜单选项
  int Change = 0;       // 0=主菜单, 1=游戏中, 2=退出菜单
}Game;
Game game;

bool dispBufGet(int vx, int vy){
  int bit = vy * MAX_VIEW_W + vx;
  return view.dispBuf[bit >> 5] & (1UL << (bit & 31));
}
void dispBufSet(int vx, int vy, bool val){
  int bit = vy * MAX_VIEW_W + vx;
  if(val)
    view.dispBuf[bit >> 5] |= 1UL << (bit & 31);
  else
    view.dispBuf[bit >> 5] &= ~(1UL << (bit & 31));
}


// 判断细胞是否在当前可见区域内
bool isCellVisible(int x, int y){
  return (x >= view.camX && x < view.camX + view.W && y >= view.camY && y < view.camY + view.H);
}

// 将世界坐标转为屏幕坐标
int worldToScreenX(int worldX){ return (worldX - view.camX) * (CELL_SIZE + 1); }
int worldToScreenY(int worldY){ return (worldY - view.camY) * (CELL_SIZE + 1); }

// 在屏幕上绘制单个细胞（使用可见区域相对坐标）
void drawCell(int x, int y, uint16_t colour){
  int sx = worldToScreenX(x);
  int sy = worldToScreenY(y);
  myScreen.dRectangle(sx, sy, CELL_SIZE, CELL_SIZE, colour);
}


// 显示更新
void updateDisplay(){
  for(int vx = 0; vx < view.W; ++vx){
    for(int vy = 0; vy < view.H; ++vy){
      int wx = view.camX + vx;
      int wy = view.camY + vy;
      if(wx >= FIELD_X || wy >= FIELD_Y) continue;

      bool shouldBe = cell[wx][wy].state;
      if(shouldBe != dispBufGet(vx, vy)){
        dispBufSet(vx, vy, shouldBe);
        drawCell(wx, wy, shouldBe ? whiteColour : blackColour);
      }
    }
  }
}

// 让细胞直接亮起（仅设置状态，不直接绘制像素）
void makeItLive(int x, int y){
  cell[x][y].state = true;
  cell[x][y].buf = 0x1FFFFFF;  // 全部点亮
}

// 让细胞直接灭掉（仅设置状态，不直接绘制像素）
void makeItDie(int x, int y){
  cell[x][y].state = false;
  cell[x][y].buf = 0;
}

void clearCells(){
  for(int x = 0; x < FIELD_X; ++x){
    for(int y = 0; y < FIELD_Y; ++y){
      cell[x][y].stateLast = false;
      cell[x][y].state = false;
      cell[x][y].buf = 0;
    }
  }

  // 重置相机到原点
  view.camX = 0; view.camY = 0;
  view.lastCamX = -1; view.lastCamY = -1;

  myScreen.clear(blackColour);
}

// 绘制可见区域内所有存活细胞（全局重绘）
void drawView(){
  myScreen.clear(blackColour);
  for(int i = view.camX; i < view.camX + view.W && i < FIELD_X; ++i){
    for(int j = view.camY; j < view.camY + view.H && j < FIELD_Y; ++j){
      if(cell[i][j].state){
        drawCell(i, j, whiteColour);
      }
    }
  }
  view.lastCamX = view.camX;
  view.lastCamY = view.camY;
}

// 通过摇杆移动相机
void updateCamera(){
  view.camTick++;
  if(view.camTick < 4) return;
  view.camTick = 0;

  int joyX = analogRead(Joystick_X);
  int joyY = 4096 - analogRead(Joystick_Y);

  const int CENTER = 2048;
  const int DEAD_ZONE = 390;

  int dx = 0, dy = 0;

  if(joyX < CENTER - DEAD_ZONE){
    dx = -((CENTER - DEAD_ZONE - joyX) / 400 + 1);
  }else if(joyX > CENTER + DEAD_ZONE){
    dx = (joyX - CENTER - DEAD_ZONE) / 400 + 1;
  }

  if(joyY < CENTER - DEAD_ZONE){
    dy = -((CENTER - DEAD_ZONE - joyY) / 400 + 1);
  }else if(joyY > CENTER + DEAD_ZONE){
    dy = (joyY - CENTER - DEAD_ZONE) / 400 + 1;
  }

  view.camX += dx;
  view.camY += dy;

  // 边界限制
  view.camX = max(0, view.camX);
  view.camX = min(view.camX, FIELD_X - view.W);
  view.camY = max(0, view.camY);
  view.camY = min(view.camY, FIELD_Y - view.H);
}

void initCellsRandom(){
  clearCells();

  // 随机生成指定数量的细胞
  int liveNum = FIELD_X * FIELD_Y * game.liveRatio / 100;
  for(int i = 0; i < liveNum; ++i){
    int x = random(FIELD_X), y = random(FIELD_Y);
    while(cell[x][y].state){
      x = random(FIELD_X), y = random(FIELD_Y);
    }
    // 让细胞直接亮起
    makeItLive(x, y);
  }
}

void initCellsGosper(){
  clearCells();

  // Gosper滑翔机枪，尺寸36×9
  static const uint8_t gun[][2] = {
    // 左侧方块
    {0, 4}, {0, 5},
    {1, 4}, {1, 5},

    // 左侧主体
    {10, 4}, {10, 5}, {10, 6},
    {11, 3}, {11, 7},
    {12, 2}, {12, 8},
    {13, 2}, {13, 8},
    {14, 5},
    {15, 3}, {15, 7},
    {16, 4}, {16, 5}, {16, 6},
    {17, 5},

    // 中间结构
    {20, 2}, {20, 3}, {20, 4},
    {21, 2}, {21, 3}, {21, 4},
    {22, 1}, {22, 5},
    {24, 0}, {24, 1}, {24, 5}, {24, 6},

    // 右侧方块
    {34, 2}, {34, 3},
    {35, 2}, {35, 3}
  };

  for(int i = 0; i < sizeof(gun) / sizeof(gun[0]); ++i){
    int x = gun[i][0] + 2;
    int y = gun[i][1] + 10;
    makeItLive(x, y);
  }
}

// 红绿灯：周期为2的振荡器
// 脉冲星：周期为3的大型振荡器，尺寸13×13
void initCellsPulsar(){
  clearCells();

  static const uint8_t pattern[][2] = {
    // 上方横线
    {2, 0}, {3, 0}, {4, 0},
    {8, 0}, {9, 0}, {10, 0},

    // 上半部分竖线
    {0, 2}, {5, 2}, {7, 2}, {12, 2},
    {0, 3}, {5, 3}, {7, 3}, {12, 3},
    {0, 4}, {5, 4}, {7, 4}, {12, 4},

    // 中上横线
    {2, 5}, {3, 5}, {4, 5},
    {8, 5}, {9, 5}, {10, 5},

    // 中下横线
    {2, 7}, {3, 7}, {4, 7},
    {8, 7}, {9, 7}, {10, 7},

    // 下半部分竖线
    {0, 8}, {5, 8}, {7, 8}, {12, 8},
    {0, 9}, {5, 9}, {7, 9}, {12, 9},
    {0, 10}, {5, 10}, {7, 10}, {12, 10},

    // 下方横线
    {2, 12}, {3, 12}, {4, 12},
    {8, 12}, {9, 12}, {10, 12}
  };

  for(int i = 0; i < sizeof(pattern) / sizeof(pattern[0]); ++i){
    makeItLive(pattern[i][0] + 10,
               pattern[i][1] + 10);
  }
}

// 轻量级飞船：沿水平方向移动
void initCellsLWSS(){
  clearCells();

  static const uint8_t pattern[][2] = {
    {0, 0}, {3, 0},
    {4, 1},
    {0, 2}, {4, 2},
    {1, 3}, {2, 3}, {3, 3}, {4, 3}
  };

  for(int i = 0; i < sizeof(pattern) / sizeof(pattern[0]); ++i){
    makeItLive(pattern[i][0] + 2,
               pattern[i][1] + 10);
  }
}

// Diehard：会演化130代后完全消失
void initCellsDiehard(){
  clearCells();

  static const uint8_t pattern[][2] = {
                            {6, 0},
    {0, 1}, {1, 1},
                    {5, 1},
                            {6, 1}, {7, 1},
            {1, 2}
  };

  for(int i = 0; i < sizeof(pattern) / sizeof(pattern[0]); ++i){
    makeItLive(pattern[i][0] + 17,
               pattern[i][1] + 17);
  }
}

uint32_t cellBufMask(int index){
  return 1UL << ((index / CELL_SIZE) * MAX_CELL_SIZE + index % CELL_SIZE);
}

// 此处的运行指”淡入淡出”
void runCells(){
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      uint32_t colour, index, startIndex;
      if(cell[i][j].state){
        // 如果细胞存活
        index = random(0, CELL_SIZE * CELL_SIZE), startIndex = index;
        while(cell[i][j].buf & cellBufMask(index)){
          index = (index + 1) % (CELL_SIZE * CELL_SIZE);
          if(index == startIndex) break;
        }
        cell[i][j].buf |= cellBufMask(index);

        colour = whiteColour;
      }else{
        // 如果细胞死亡
        index = random(0, CELL_SIZE * CELL_SIZE), startIndex = index;
        while(!(cell[i][j].buf & cellBufMask(index))){
          index = (index + 1) % (CELL_SIZE * CELL_SIZE);
          if(index == startIndex) break;
        }
        cell[i][j].buf &= ~cellBufMask(index);

        colour = blackColour;
      }
      myScreen.point(i * (CELL_SIZE + 1) + index % CELL_SIZE, j * (CELL_SIZE + 1) + index / CELL_SIZE, colour);
    }
  }
}

void stepCells(){
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      cell[i][j].stateLast = cell[i][j].state;
    }
  }

  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      int liveNum = 0;
      for(int dx = -1; dx <= 1; ++dx){
        for(int dy = -1; dy <= 1; ++dy){
          if(dx == 0 && dy == 0) continue;
          int x = i + dx;
          int y = j + dy;

          // 规则A：超出边界视为死亡
          if(x < 0 || x >= FIELD_X || y < 0 || y >= FIELD_Y) continue;
          // // 规则B：如果超出边界，则从另一侧计算
          // int x = (x + FIELD_X) % FIELD_X;
          // int y = (y + FIELD_Y) % FIELD_Y;

          if(cell[x][y].stateLast){
            ++liveNum;
          }
        }
      }

      if(cell[i][j].stateLast){
        // 如果细胞存活
        if(liveNum < 2 || liveNum > 3){
          cell[i][j].state = false;
        }else{
          cell[i][j].state = true;
        }
      }else{
        // 如果细胞死亡
        if(liveNum == 3){
          cell[i][j].state = true;
        }else{
          cell[i][j].state = false;
        }
      }
    }
  }
}


#define X 2
#define Y 26

// game.Choose/game.Change/game.displayMode 已移入 Game 结构体
int SettingChoose = 1;
int presetChoose = 1;
#define PRESET_COUNT 4

void showText(String text)
{
    myScreen.clear(blackColour);
    myScreen.setFontSize(0);

    for (uint16_t i = 0; i < text.length(); i += 21)
    {
        myScreen.gText(0, i / 21 * 8, text.substring(i, i + 21), whiteColour);
    }
}

void Menu()
{
    myScreen.gText(45, 60, "START",
                   game.Choose == 1 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(39, 70, "Setting",
                   game.Choose == 2 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(39, 80, "Preset",
                   game.Choose == 3 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(27, 90, "Introduction",
                   game.Choose == 4 ? redColour : whiteColour,
                   blackColour, 1, 1);
}

void MainScreen()
{
    myScreen.clear(blackColour);

    myScreen.gText(0, 10, "Cellular", cyanColour, blackColour, 2, 2);
    myScreen.gText(30, 30, "Automata", cyanColour, blackColour, 2, 2);

    Menu();
}

void Start()
{
    initCellsRandom();
    if (game.displayMode == 1)
    {
        for (int x = 0; x < FIELD_X; x++)
        {
            for (int y = 0; y < FIELD_Y; y++)
            {
                cell[x][y].buf = 0;
            }
        }
    }
    for(int i = 0; i < DISP_BUF_WORDS; ++i) view.dispBuf[i] = 0;
    game.tick = -1;
}

void ExitMenu()
{
    myScreen.gText(45, 30, "EXIT",
                   game.Choose == 1 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(35, 65, "RESTART",
                   game.Choose == 2 ? redColour : whiteColour,
                   blackColour, 1, 1);
}

void Setting()
{
    myScreen.gText(35, 20, "SETTING", whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 40, "CELL_SIZE", SettingChoose == 1 ? redColour : whiteColour, blackColour, 1, 1);
    myScreen.gText(90, 40, String(CELL_SIZE), whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 55, "DISPLAY_MODE", SettingChoose == 2 ? redColour : whiteColour, blackColour, 1, 1);
    myScreen.gText(90, 55, String(game.displayMode), whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 70, "DENSITY", SettingChoose == 3 ? redColour : whiteColour, blackColour, 1, 1);
    myScreen.gText(90, 70, String(game.liveRatio) + "%", whiteColour, blackColour, 1, 1);
}

void PresetScreen()
{
    myScreen.gText(35, 20, "PRESET", whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 32, "Gosper Gun",
                   presetChoose == 1 ? redColour : whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 40, "Pulsar",
                   presetChoose == 2 ? redColour : whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 48, "LWSS",
                   presetChoose == 3 ? redColour : whiteColour, blackColour, 1, 1);
    myScreen.gText(5, 56, "Diehard",
                   presetChoose == 4 ? redColour : whiteColour, blackColour, 1, 1);
}

void initPreset(int n)
{
    clearCells();
    switch(n){
        case 1: initCellsGosper();   break;
        case 2: initCellsPulsar();   break;
        case 3: initCellsLWSS();     break;
        case 4: initCellsDiehard();  break;
    }
}
void startWithPreset(int n)
{
    initPreset(n);
    for(int i = 0; i < DISP_BUF_WORDS; ++i) view.dispBuf[i] = 0;
    game.tick = -1;
}

void Keyboard_Control()
{
    if (digitalRead(BUTTON_ONE) == LOW)
    {
        if (game.Change == 0)
        {
            game.Change = 1;

            switch (game.Choose)
            {
                case 1:
                    Start();
                    break;

                case 2:
                    myScreen.clear(blackColour);
                    Setting();
                    SettingChoose = 1;
                    break;

                case 3:
                    game.Change = 3;
                    myScreen.clear(blackColour);
                    presetChoose = 1;
                    PresetScreen();
                    break;

                case 4:
                    showText(
                        "Cellular automata use a grid of living and dead cells. "
                        "Each frame, cells change by neighbor rules: fewer than 2 die, "
                        "2 or 3 survive, more than 3 die, and dead cells with exactly "
                        "3 neighbors become alive. Simple local rules can create "
                        "complex patterns."
                    );
                    break;
            }
        }
        else if (game.Change == 3)
        {
            // 预设选择界面：确认选择并开始游戏
            startWithPreset(presetChoose);
            game.Change = 1;
            game.Choose = 1;
        }
        else if (game.Change == 2)
        {
            if (game.Choose == 1)
            {
                game.Change = 0;
                game.Choose = 1;
                MainScreen();
            }
            else if (game.Choose == 2)
            {
                game.Change = 1;
                game.Choose = 1;
                Start();
            }
        }

        while (digitalRead(BUTTON_ONE) == LOW);
        delay(30);
    }

    if (digitalRead(BUTTON_TWO) == LOW && game.Change == 1)
    {
        if (game.Choose == 1)
        {
            game.Change = 2;
            myScreen.clear(blackColour);
            ExitMenu();
        }
        else
        {
            game.Change = 0;
            MainScreen();
        }

        while (digitalRead(BUTTON_TWO) == LOW);
        delay(30);
    }

    if (digitalRead(BUTTON_TWO) == LOW && game.Change == 3)
    {
        game.Change = 0;
        game.Choose = 4;
        MainScreen();

        while (digitalRead(BUTTON_TWO) == LOW);
        delay(30);
    }
}

void Joystick_Control()
{
    if (game.Change == 1 && game.Choose == 2 && analogRead(Joystick_X) > 3300 &&
        ((SettingChoose == 1 && CELL_SIZE < 5) ||
         (SettingChoose == 2 && game.displayMode < 1) ||
         (SettingChoose == 3 && game.liveRatio < 50)))
    {
        if (SettingChoose == 1)
        {
            CELL_SIZE ++;
            view.W = 128 / (CELL_SIZE + 1);
            view.H = 128 / (CELL_SIZE + 1);
            view.camX = 0;
            view.camY = 0;
            view.lastCamX = -1;
            view.lastCamY = -1;
        }
        else if (SettingChoose == 2) game.displayMode = 1;
        else game.liveRatio += 5;
        Setting();
        while (analogRead(Joystick_X) > 3000);
        delay(30);
    }

    if (game.Change == 1 && game.Choose == 2 && analogRead(Joystick_X) < 300 &&
        ((SettingChoose == 1 && CELL_SIZE > 2) ||
         (SettingChoose == 2 && game.displayMode > 0) ||
         (SettingChoose == 3 && game.liveRatio > 15)))
    {
        if (SettingChoose == 1)
        {
            CELL_SIZE --;
            view.W = 128 / (CELL_SIZE + 1);
            view.H = 128 / (CELL_SIZE + 1);
            view.camX = 0;
            view.camY = 0;
            view.lastCamX = -1;
            view.lastCamY = -1;
        }
        else if (SettingChoose == 2) game.displayMode = 0;
        else game.liveRatio -= 5;
        Setting();
        while (analogRead(Joystick_X) < 1000);
        delay(30);
    }

    if (game.Change == 1 && game.Choose == 2 && analogRead(Joystick_Y) > 3300 && SettingChoose > 1)
    {
        SettingChoose --;
        Setting();
        while (analogRead(Joystick_Y) > 3000);
        delay(30);
    }

    if (game.Change == 1 && game.Choose == 2 && analogRead(Joystick_Y) < 300 && SettingChoose < 3)
    {
        SettingChoose ++;
        Setting();
        while (analogRead(Joystick_Y) < 1000);
        delay(30);
    }

    if (game.Change == 0 && analogRead(Joystick_Y) > 3300 && game.Choose > 1)
    {
        game.Choose --;
        Menu();

        while (analogRead(Joystick_Y) > 3000);
        delay(30);
    }

    if (game.Change == 0 && analogRead(Joystick_Y) < 300 && game.Choose < 4)
    {
        game.Choose ++;
        Menu();

        while (analogRead(Joystick_Y) < 1000);
        delay(30);
    }

    if (game.Change == 3 && analogRead(Joystick_Y) > 3300 && presetChoose > 1)
    {
        presetChoose --;
        PresetScreen();
        while (analogRead(Joystick_Y) > 3000);
        delay(30);
    }

    if (game.Change == 3 && analogRead(Joystick_Y) < 300 && presetChoose < PRESET_COUNT)
    {
        presetChoose ++;
        PresetScreen();
        while (analogRead(Joystick_Y) < 1000);
        delay(30);
    }

    if (game.Change == 2 && analogRead(Joystick_Y) > 3300 && game.Choose > 1)
    {
        game.Choose --;
        ExitMenu();

        while (analogRead(Joystick_Y) > 3000);
        delay(30);
    }

    if (game.Change == 2 && analogRead(Joystick_Y) < 300 && game.Choose < 2)
    {
        game.Choose ++;
        ExitMenu();

        while (analogRead(Joystick_Y) < 1000);
        delay(30);
    }
}


void setup() {
  // put your setup code here, to run once:
  myScreen.begin();
  myScreen.setPenSolid(true);

  pinMode(BUTTON_ONE, INPUT_PULLUP);
  pinMode(BUTTON_TWO, INPUT_PULLUP);
  pinMode(Joystick_X, INPUT);
  pinMode(Joystick_Y, INPUT);

  Serial.begin(9600);
  analogReadResolution(12);
  randomSeed(analogRead(2));

  view.W = 128 / (CELL_SIZE + 1);
  view.H = 128 / (CELL_SIZE + 1);

  MainScreen();
}

void loop() {
  game.tick++;

  Keyboard_Control();

  if(game.Change == 1 && game.Choose == 1){
    updateCamera();

    bool camMoved = (view.camX != view.lastCamX || view.camY != view.lastCamY);

    if(game.displayMode == 0){
      // 非淡入淡出
      if(camMoved){
        updateDisplay();
        view.lastCamX = view.camX;
        view.lastCamY = view.camY;
      }
      if(game.tick > 40/10){
        game.tick = 0;
        stepCells();
        updateDisplay();
      }
    }else{
      // 淡入淡出
      if(game.tick <= CELL_SIZE * CELL_SIZE){
        runCells();
      }else if(game.tick >= CELL_SIZE * CELL_SIZE + 2000/10){
        game.tick = 0;
        stepCells();
      }
    }
  }else{
    Joystick_Control();
  }


  delay(10);
}
