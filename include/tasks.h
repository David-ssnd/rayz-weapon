#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void control_task(void* pvParameters);
    void laser_task(void* pvParameters);
    void ws_task(void* pvParameters);
    void game_task(void* pvParameters);
    void espnow_task(void* pvParameters);
    void wifi_task(void* pvParameters);

#ifdef __cplusplus
}
#endif
