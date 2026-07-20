// Include application, user and local libraries
#include <SPI.h>

#include <LCD_screen.h>
#include <LCD_screen_font.h>
#include <LCD_utilities.h>
#include <Screen_HX8353E.h>
Screen_HX8353E myScreen;

const int FIELD_X = 30, FIELD_Y = 30;
const int CELL_SIZE = 5;

typedef struct{
  bool state = false;
  bool stateNext = false; // 下一个状态（临时变量）

  // 显示缓存，用于淡入淡出
  // 0: 灭；1: 亮
  uint8_t buf[CELL_SIZE][CELL_SIZE] = { 0 };
} Cell_t;
Cell_t cell[FIELD_X][FIELD_Y];

void init(Cell_t &cell){
  randomSeed(analogRead(2));

  int liveNum = random(FIELD_X * FIELD_Y / 10, FIELD_X * FIELD_Y / 4);
  for(int i = 0; i < liveNum; ++i){
    int x = random(FIELD_X), y = random(FIELD_Y);
    while(cell[x][y].state){
      x = random(FIELD_X), y = random(FIELD_Y);
    }
    
    // 让细胞直接亮起
    cell[x][y].state = true;
    for(int i = 0; i < CELL_SIZE; ++i){
      for(int j = 0; j < CELL_SIZE; ++j){
        cell[x][y].buf[i][j] = 1;
      }
    }
  }
}

// 此处的运行指“淡入淡出”
void run(Cell_t &cell){
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

        // ToDo: 更新显示屏
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

        // ToDo: 更新显示屏
      }
    }
  }
}

void step(Cell_t &cell){
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      int liveNum = 0;
      for(int dx = -1; dx <= 1; ++dx){
        for(int dy = -1; dy <= 1; ++dy){
          if(dx == 0 && dy == 0) continue;
          int x = (i + dx + FIELD_X) % FIELD_X; // 如果超出边界，则从另一侧计算
          int y = (j + dy + FIELD_Y) % FIELD_Y;
          if(cell[x][y].state) ++liveNum;
        }
      }

      if(cell[i][j].state){
        // 如果细胞存活
        if(liveNum < 2 || liveNum > 3){
          cell[i][j].stateNext = false;
        }else{
          cell[i][j].stateNext = true;
        }
      }else{
        // 如果细胞死亡
        if(liveNum == 3){
          cell[i][j].stateNext = true;
        }else{
          cell[i][j].stateNext = false;
        }
      }
    }
  }

  // 更新状态
  for(int i = 0; i < FIELD_X; ++i){
    for(int j = 0; j < FIELD_Y; ++j){
      cell[i][j].state = cell[i][j].stateNext;
    }
  }
}


void setup() {
  // put your setup code here, to run once:
  myScreen.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  
}
