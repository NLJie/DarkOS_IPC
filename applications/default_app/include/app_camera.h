#ifndef APP_CAMERA_H
#define APP_CAMERA_H

#include "hal_camera.h"

void* open_camera(const HalCamConfig& cfg);
void  read_frame_once(void* cam);

#endif // APP_CAMERA_H
