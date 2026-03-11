#include "app_camera.h"

#include <cstring>

#include "log_system.h"

void* open_camera(const HalCamConfig& cfg)
{
    LOG_INFO("Opening camera device (interface=%d): %s",
             cfg.interface,
             cfg.interface == HAL_CAM_IF_RTSP ? cfg.rtsp.url : cfg.usb.dev_path);
    void* cam = hal_cam_open(&cfg);
    if (cam == nullptr) {
        LOG_ERROR("hal_cam_open failed");
        return nullptr;
    }
    LOG_INFO("Camera opened successfully");
    return cam;
}

void read_frame_once(void* cam)
{
    HalVideoFrame frame;
    std::memset(&frame, 0, sizeof(frame));
    LOG_DEBUG("Attempting to read frame...");
    if (hal_cam_read_frame(cam, &frame) == 0) {
        LOG_INFO("Frame received: %dx%d, size=%zu, ts=%lld",
                 frame.width, frame.height, frame.size,
                 static_cast<long long>(frame.timestamp_us));
    } else {
        LOG_WARN("hal_cam_read_frame not available in current backend");
    }
}
