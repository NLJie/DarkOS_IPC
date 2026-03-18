#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include "hal_camera.h"

// 开始录制视频
// width, height: 视频分辨率
// fps: 目标帧率（仅用于信息显示，实际取决于采集帧率）
bool video_recorder_start(int width, int height, int fps);

// 停止录制
void video_recorder_stop();

// 编码一帧
bool video_recorder_encode_frame(const HalVideoFrame* frame);

// 是否正在录制
bool video_recorder_is_recording();

#endif // VIDEO_RECORDER_H
