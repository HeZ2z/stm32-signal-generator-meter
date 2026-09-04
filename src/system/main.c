#include "app/app.h"

/* 固件入口只负责初始化应用并持续轮询主循环。 */
int main(void) {
  app_init();

  while (1) {
    app_run_once();
  }
}
