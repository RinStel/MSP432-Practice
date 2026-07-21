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

#define FIELD_X 100
#define FIELD_Y 100
#define MAX_CELL_SIZE 4  // 最大细胞尺寸, 不要改
int CELL_SIZE = 3;

// 可见区域大小，单位为细胞数
#define MAX_VIEW_W 42
#define MAX_VIEW_H 42
int VIEW_W = 128 / (CELL_SIZE + 1), VIEW_H = 128 / (CELL_SIZE + 1);
// 相机偏移（区域左上角对应的细胞坐标）
int camX = 0, camY = 0;
int lastCamX = -1, lastCamY = -1;  // 用于检测相机移动
int camTick = 0;                    // 相机移动速率限制

int tick = 0;

// 压缩的细胞状态，应对变量存储空间不足的问题
typedef struct{
  uint32_t buf       : 25;  // 淡入淡出缓存 (位0-24)
  uint32_t stateLast : 1;   // 上一帧状态 (位25)
  uint32_t state     : 1;   // 当前状态 (位26)
}Cell_t;
Cell_t cell[FIELD_X][FIELD_Y];

// 显示缓冲区, 同样使用压缩来应对内存问题
#define DISP_BUF_BITS (MAX_VIEW_W * MAX_VIEW_H)
#define DISP_BUF_WORDS ((DISP_BUF_BITS + 31) / 32)
uint32_t dispBuf[DISP_BUF_WORDS] = { 0 };

static inline bool dispBufGet(int vx, int vy){
  int bit = vy * MAX_VIEW_W + vx;
  return (dispBuf[bit >> 5] >> (bit & 31)) & 1;
}
static inline void dispBufSet(int vx, int vy, bool val){
  int bit = vy * MAX_VIEW_W + vx;
  if(val)
    dispBuf[bit >> 5] |= 1UL << (bit & 31);
  else
    dispBuf[bit >> 5] &= ~(1UL << (bit & 31));
}


// 判断细胞是否在当前视口内
bool isCellVisible(int x, int y){
  return (x >= camX && x < camX + VIEW_W && y >= camY && y < camY + VIEW_H);
}

// 将世界坐标转为屏幕坐标
int worldToScreenX(int worldX){ return (worldX - camX) * (CELL_SIZE + 1); }
int worldToScreenY(int worldY){ return (worldY - camY) * (CELL_SIZE + 1); }

// 在屏幕上绘制单个细胞（使用视口相对坐标）
void drawCell(int x, int y, uint16_t colour){
  int sx = worldToScreenX(x);
  int sy = worldToScreenY(y);
  myScreen.dRectangle(sx, sy, CELL_SIZE, CELL_SIZE, colour);
}


// 显示更新
void updateDisplay(){
  for(int vx = 0; vx < VIEW_W; ++vx){
    for(int vy = 0; vy < VIEW_H; ++vy){
      int wx = camX + vx;
      int wy = camY + vy;
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
  for(int i = 0; i < CELL_SIZE; ++i){
    for(int j = 0; j < CELL_SIZE; ++j){
      cell[x][y].buf |= 1UL << (MAX_CELL_SIZE * i + j);
    }
  }
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
  camX = 0; camY = 0;
  lastCamX = -1; lastCamY = -1;

  myScreen.clear(blackColour);
}

// 绘制可见区域内所有存活细胞（全局重绘）
void drawView(){
  myScreen.clear(blackColour);
  for(int i = camX; i < camX + VIEW_W && i < FIELD_X; ++i){
    for(int j = camY; j < camY + VIEW_H && j < FIELD_Y; ++j){
      if(cell[i][j].state){
        drawCell(i, j, whiteColour);
      }
    }
  }
  lastCamX = camX;
  lastCamY = camY;
}

// 通过摇杆移动相机
void updateCamera(){
  camTick++;
  if(camTick < 4) return;  // 速率限制
  camTick = 0;

  int joyX = analogRead(Joystick_X);
  int joyY = 4096 - analogRead(Joystick_Y);

  const int CENTER = 2048;
  const int DEAD_ZONE = 400;
  const int SPEED_DIV = 400;

  int dx = 0, dy = 0;

  if(joyX < CENTER - DEAD_ZONE){
    dx = -((CENTER - DEAD_ZONE - joyX) / SPEED_DIV + 1);
  }else if(joyX > CENTER + DEAD_ZONE){
    dx = (joyX - CENTER - DEAD_ZONE) / SPEED_DIV + 1;
  }

  if(joyY < CENTER - DEAD_ZONE){
    dy = -((CENTER - DEAD_ZONE - joyY) / SPEED_DIV + 1);
  }else if(joyY > CENTER + DEAD_ZONE){
    dy = (joyY - CENTER - DEAD_ZONE) / SPEED_DIV + 1;
  }

  camX += dx;
  camY += dy;

  // 边界限制
  if(camX < 0) camX = 0;
  if(camX > FIELD_X - VIEW_W) camX = FIELD_X - VIEW_W;
  if(camY < 0) camY = 0;
  if(camY > FIELD_Y - VIEW_H) camY = FIELD_Y - VIEW_H;
}

void initCellsRandom(){
  clearCells();
  randomSeed(analogRead(2));

  int liveNum = random(FIELD_X * FIELD_Y / 8, FIELD_X * FIELD_Y / 3);
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

  const int offsetX = 2;
  const int offsetY = 10;

  const int pointCount = sizeof(gun) / sizeof(gun[0]);

  for(int i = 0; i < pointCount; ++i){
    int x = gun[i][0] + offsetX;
    int y = gun[i][1] + offsetY;

    makeItLive(x, y);
  }
}


// 此处的运行指”淡入淡出”
void runCells(){
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      if(cell[i][j].state){
        // 如果细胞存活
        int index = random(0, CELL_SIZE * CELL_SIZE), startIndex = index;
        while(cell[i][j].buf & (1UL << ((index / CELL_SIZE) * MAX_CELL_SIZE + index % CELL_SIZE))){
          index = (index + 1) % (CELL_SIZE * CELL_SIZE);
          if(index == startIndex){
            break;
          }
        }
        cell[i][j].buf |= 1UL << ((index / CELL_SIZE) * MAX_CELL_SIZE + index % CELL_SIZE);

        myScreen.point(i * (CELL_SIZE + 1) + index % CELL_SIZE, j * (CELL_SIZE + 1) + index / CELL_SIZE, whiteColour);
      }else{
        // 如果细胞死亡
        int index = random(0, CELL_SIZE * CELL_SIZE), startIndex = index;
        while(!(cell[i][j].buf & (1UL << ((index / CELL_SIZE) * MAX_CELL_SIZE + index % CELL_SIZE)))){
          index = (index + 1) % (CELL_SIZE * CELL_SIZE);
          if(index == startIndex){
            break;
          }
        }
        cell[i][j].buf &= ~(1UL << ((index / CELL_SIZE) * MAX_CELL_SIZE + index % CELL_SIZE));

        myScreen.point(i * (CELL_SIZE + 1) + index % CELL_SIZE, j * (CELL_SIZE + 1) + index / CELL_SIZE, blackColour);
      }
    }
  }
}

// // 非淡入淡出, 直接绘制矩形（仅绘制可见区域内变化的细胞）, 弃用
// void runCellsBlock(){
//   for(int i = camX; i < camX + VIEW_W && i < FIELD_X; ++i){
//     for(int j = camY; j < camY + VIEW_H && j < FIELD_Y; ++j){
//       if(cell[i][j].state && !cell[i][j].stateLast){
//         drawCell(i, j, whiteColour);
//       }
//       if(!cell[i][j].state && cell[i][j].stateLast){
//         drawCell(i, j, blackColour);
//       }
//     }
//   }
// }

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

int Choose = 1;
int Change = 0;
int displayMode = 0;  // 0=非淡入淡出, 1=淡入淡出

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
    myScreen.gText(45, 65, "START",
                   Choose == 1 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(39, 75, "Setting",
                   Choose == 2 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(27, 85, "Introduction",
                   Choose == 3 ? redColour : whiteColour,
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
    for(int i = 0; i < DISP_BUF_WORDS; ++i) dispBuf[i] = 0;
    tick = 0;
}

void ExitMenu()
{
    myScreen.gText(45, 30, "EXIT",
                   Choose == 1 ? redColour : whiteColour,
                   blackColour, 1, 1);

    myScreen.gText(35, 65, "RESTART",
                   Choose == 2 ? redColour : whiteColour,
                   blackColour, 1, 1);
}

void Keyboard_Control()
{
    if (digitalRead(BUTTON_ONE) == LOW)
    {
        if (Change == 0)
        {
            Change = 1;

            switch (Choose)
            {
                case 1:
                    Start();
                    break;

                case 2:
                    myScreen.clear(blackColour);
                    myScreen.gText(35, 55, "SETTING", whiteColour);
                    break;

                case 3:
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
        else if (Change == 2)
        {
            if (Choose == 1)
            {
                Change = 0;
                Choose = 1;
                MainScreen();
            }
            else if (Choose == 2)
            {
                Change = 1;
                Choose = 1;
                Start();
            }
        }

        while (digitalRead(BUTTON_ONE) == LOW);
        delay(30);
    }

    if (digitalRead(BUTTON_TWO) == LOW && Change == 1)
    {
        if (Choose == 1)
        {
            Change = 2;
            myScreen.clear(blackColour);
            ExitMenu();
        }
        else
        {
            Change = 0;
            MainScreen();
        }

        while (digitalRead(BUTTON_TWO) == LOW);
        delay(30);
    }
}

void Joystick_Control()
{
    if (Change == 0 && analogRead(Y) > 3300 && Choose > 1)
    {
        Choose --;
        Menu();

        while (analogRead(Y) > 3000);
        delay(30);
    }

    if (Change == 0 && analogRead(Y) < 300 && Choose < 3)
    {
        Choose ++;
        Menu();

        while (analogRead(Y) < 1000);
        delay(30);
    }

    if (Change == 2 && analogRead(Y) > 3300 && Choose > 1)
    {
        Choose --;
        ExitMenu();

        while (analogRead(Y) > 3000);
        delay(30);
    }

    if (Change == 2 && analogRead(Y) < 300 && Choose < 2)
    {
        Choose ++;
        ExitMenu();

        while (analogRead(Y) < 1000);
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

  MainScreen();
}

void loop() {
  tick++;

  Keyboard_Control();

  // 游戏模式下摇杆控制相机，菜单模式下摇杆控制菜单选择
  if(Change == 1){
    updateCamera();

    bool camMoved = (camX != lastCamX || camY != lastCamY);

    if(displayMode == 0){
      // 非淡入淡出：仅相机移动或细胞演化后才刷新
      if(camMoved){
        updateDisplay();
        lastCamX = camX;
        lastCamY = camY;
      }
      if(tick >= 1 + 30/10){
        tick = 0;
        stepCells();
        updateDisplay();  // 演化后立即刷新变化的细胞
      }
    }else{
      // 淡入淡出
      if(tick <= CELL_SIZE * CELL_SIZE){
        runCells();
      }else if(tick >= CELL_SIZE * CELL_SIZE + 2000/10){
        tick = 0;
        stepCells();
      }
    }
  }else{
    Joystick_Control();
  }


  delay(10);
}
