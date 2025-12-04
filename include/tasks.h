#ifndef TASKS_H
#define TASKS_H

void ble_task(void* pvParameters);
void control_task(void* pvParameters);
void display_task(void* pvParameters);
void laser_task(void* pvParameters);

#endif // TASKS_H
