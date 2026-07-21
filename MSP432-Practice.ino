// Include application, user and local libraries
#include <SPI.h>

#include <LCD_screen.h>
#include <LCD_screen_font.h>
#include <LCD_utilities.h>
#include <Screen_HX8353E.h>
Screen_HX8353E myScreen;

const int BUTTON_ONE = 33, BUTTON_TWO = 32;
const int Joystick_X = 2, Joystick_Y = 26;

const int FIELD_X = 42, FIELD_Y = 42;
const int CELL_SIZE = 2;

typedef struct{
  bool stateLast = false; // 上一次的状态
  bool state = false;

  // 显示缓存，用于淡入淡出
  // 0: 灭；1: 亮
  uint8_t buf[CELL_SIZE][CELL_SIZE] = { 0 };
}Cell_t;
Cell_t cell[FIELD_X][FIELD_Y];

typedef struct{
  uint8_t x = 10, y = 10;
  bool visible = false; // 是否可见
  int idieTime = 0;    // 空闲时间

  uint8_t x_last = 10, y_last = 10;
  bool visible_last = true; // 上一次是否可见
}Cursor;
Cursor cursor;

// 让细胞直接亮起
void makeItLive(int x, int y){
  cell[x][y].state = true;
  for(int i = 0; i < CELL_SIZE; ++i){
    for(int j = 0; j < CELL_SIZE; ++j){
      cell[x][y].buf[i][j] = 1;
      myScreen.point(x * (CELL_SIZE + 1) + j, y * (CELL_SIZE + 1) + i, whiteColour);
    }
  }
}

// 让细胞直接灭掉
void makeItDie(int x, int y){
  cell[x][y].state = false;
  for(int i = 0; i < CELL_SIZE; ++i){
    for(int j = 0; j < CELL_SIZE; ++j){
      cell[x][y].buf[i][j] = 0;
      myScreen.point(x * (CELL_SIZE + 1) + j, y * (CELL_SIZE + 1) + i, blackColour);
    }
  }
}

void clearCells(){
  for(int x = 0; x < FIELD_X; ++x){
    for(int y = 0; y < FIELD_Y; ++y){
      cell[x][y].stateLast = false;
      cell[x][y].state = false;

      for(int i = 0; i < CELL_SIZE; ++i){
        for(int j = 0; j < CELL_SIZE; ++j){
          cell[x][y].buf[i][j] = 0;
        }
      }
    }
  }

  myScreen.clear(blackColour);
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


// 此处的运行指“淡入淡出”
void runCells(){
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      if(cell[i][j].state){
        // 如果细胞存活
        int index = random(0, CELL_SIZE * CELL_SIZE), startIndex = index;
        while(cell[i][j].buf[index / CELL_SIZE][index % CELL_SIZE] == 1){
          index = (index + 1) % (CELL_SIZE * CELL_SIZE);
          if(index == startIndex){
            break;
          }
        }
        cell[i][j].buf[index / CELL_SIZE][index % CELL_SIZE] = 1;

        myScreen.point(i * (CELL_SIZE + 1) + index % CELL_SIZE, j * (CELL_SIZE + 1) + index / CELL_SIZE, whiteColour);
      }else{
        // 如果细胞死亡
        int index = random(0, CELL_SIZE * CELL_SIZE), startIndex = index;
        while(cell[i][j].buf[index / CELL_SIZE][index % CELL_SIZE] == 0){
          index = (index + 1) % (CELL_SIZE * CELL_SIZE);
          if(index == startIndex){
            break;
          }
        }
        cell[i][j].buf[index / CELL_SIZE][index % CELL_SIZE] = 0;

        myScreen.point(i * (CELL_SIZE + 1) + index % CELL_SIZE, j * (CELL_SIZE + 1) + index / CELL_SIZE, blackColour);
      }
    }
  }
}

// 非淡入淡出, 直接绘制矩形
void runCellsBlock(){
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      if(cell[i][j].state && !cell[i][j].stateLast){
        myScreen.dRectangle(i * (CELL_SIZE + 1), j * (CELL_SIZE + 1), CELL_SIZE, CELL_SIZE, whiteColour);
      }
      if(!cell[i][j].state && cell[i][j].stateLast){
        myScreen.dRectangle(i * (CELL_SIZE + 1), j * (CELL_SIZE + 1), CELL_SIZE, CELL_SIZE, blackColour);
      }
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


void setup() {
  // put your setup code here, to run once:
  myScreen.begin();

  pinMode(BUTTON_ONE, INPUT_PULLUP);
  pinMode(BUTTON_TWO, INPUT_PULLUP);
  pinMode(Joystick_X, INPUT);
  pinMode(Joystick_Y, INPUT);
  
  initCellsGosper();
}

int tick = 0;
int btnOneState = 0, btnTwoState = 0;

void loop() {
  tick++;


  // // 淡入淡出
  // if(tick <= CELL_SIZE * CELL_SIZE){
  //   runCells();
  // }else if(tick >= CELL_SIZE * CELL_SIZE + 2000/10){
  //   tick = 0;
  //   stepCells();
  // }

  // 非淡入淡出
  if(tick == 1){
    runCellsBlock();
  }else if(tick >= 1 + 50/10){
    tick = 0;
    stepCells();
  }


  // 按键状态机
  switch(btnOneState){
  case 0:
    if(digitalRead(BUTTON_ONE) == LOW){
      btnOneState = 1;
    }
    break;
  case 1:
    if(digitalRead(BUTTON_ONE) == LOW){
      btnOneState = 2;
      // 按下时的处理
      if(!cursor.visible){
        cursor.visible = true;
        cursor.x = FIELD_X / 2;
        cursor.y = FIELD_Y / 2;
      }else{
        if(cell[cursor.x][cursor.y].state){
          makeItDie(cursor.x, cursor.y);
        }else{
          makeItLive(cursor.x, cursor.y);
        }
      }
    }else{
      btnOneState = 0;
    }
    break;
  case 2:
    if(digitalRead(BUTTON_ONE) == HIGH){
      btnOneState = 0;
    }
    break;
  }

  if(cursor.visible){
    if(!cursor.visible_last || cursor.x != cursor.x_last || cursor.y != cursor.y_last){
      // 如果光标位置发生变化，则擦除上一个光标
      myScreen.dRectangle(cursor.x_last * (CELL_SIZE + 1), cursor.y_last * (CELL_SIZE + 1), CELL_SIZE, CELL_SIZE, blackColour);
    }
    // 光标闪烁
    uint32_t cursorColour = (tick % 50 < 25) ? redColour : blackColour;
    myScreen.dRectangle((cursor.x - 1) * (CELL_SIZE + 1) + CELL_SIZE, (cursor.y - 1) * (CELL_SIZE + 1) + CELL_SIZE, CELL_SIZE + 2, CELL_SIZE + 2, cursorColour);
  }


  delay(10);
}
