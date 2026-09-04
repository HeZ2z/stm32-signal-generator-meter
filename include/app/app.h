#ifndef APP_H
#define APP_H

/* 完成板级和业务模块初始化。 */
void app_init(void);

/* 执行一次主循环轮询。 */
void app_run_once(void);

#endif
