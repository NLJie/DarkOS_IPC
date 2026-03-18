#include "app_camera.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "log_system.h"

// BMP 文件头结构
#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t type = 0x4D42;  // 'BM'
    uint32_t size = 0;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t offset = 54;  // 文件头 + 信息头大小
};

struct BmpInfoHeader {
    uint32_t size = 40;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 1;
    uint16_t bitCount = 24;  // RGB24
    uint32_t compression = 0;
    uint32_t imageSize = 0;
    int32_t xPixelsPerMeter = 0;
    int32_t yPixelsPerMeter = 0;
    uint32_t colorsUsed = 0;
    uint32_t colorsImportant = 0;
};
#pragma pack(pop)

// NV12 (YUV420SP) 转 RGB24
static void nv12_to_rgb24(const uint8_t* nv12, uint8_t* rgb24, int width, int height)
{
    const uint8_t* y_plane = nv12;
    const uint8_t* uv_plane = nv12 + width * height;
    
    for (int y = height - 1; y >= 0; y--) {  // BMP 是从下往上存储
        for (int x = 0; x < width; x++) {
            int y_idx = y * width + x;
            int uv_idx = (y / 2) * width + (x / 2) * 2;
            
            uint8_t Y = y_plane[y_idx];
            uint8_t U = uv_plane[uv_idx];
            uint8_t V = uv_plane[uv_idx + 1];
            
            // YUV to RGB
            int r = Y + 1.402 * (V - 128);
            int g = Y - 0.344136 * (U - 128) - 0.714136 * (V - 128);
            int b = Y + 1.772 * (U - 128);
            
            // 裁剪到 0-255
            r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
            g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
            b = (b < 0) ? 0 : ((b > 255) ? 255 : b);
            
            int rgb_idx = ((height - 1 - y) * width + x) * 3;
            rgb24[rgb_idx + 0] = (uint8_t)b;  // B
            rgb24[rgb_idx + 1] = (uint8_t)g;  // G
            rgb24[rgb_idx + 2] = (uint8_t)r;  // R
        }
    }
}

// 保存为 BMP 文件
static void save_frame_as_bmp(const HalVideoFrame* frame, const char* filename)
{
    if (!frame || !frame->vir_addr) {
        LOG_ERROR("Invalid frame data");
        return;
    }
    
    int width = frame->width;
    int height = frame->height;
    int rowSize = ((width * 3 + 3) / 4) * 4;  // 4字节对齐
    int imageSize = rowSize * height;
    
    // 分配 RGB 缓冲区
    uint8_t* rgb24 = (uint8_t*)malloc(imageSize);
    if (!rgb24) {
        LOG_ERROR("Failed to allocate RGB buffer");
        return;
    }
    
    // 转换 NV12 到 RGB24
    nv12_to_rgb24((const uint8_t*)frame->vir_addr, rgb24, width, height);
    
    // 填充 BMP 头
    BmpFileHeader fileHeader;
    BmpInfoHeader infoHeader;
    
    fileHeader.size = sizeof(fileHeader) + sizeof(infoHeader) + imageSize;
    infoHeader.width = width;
    infoHeader.height = height;
    infoHeader.imageSize = imageSize;
    
    // 写入文件
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        LOG_ERROR("Failed to create file: %s", filename);
        free(rgb24);
        return;
    }
    
    fwrite(&fileHeader, sizeof(fileHeader), 1, fp);
    fwrite(&infoHeader, sizeof(infoHeader), 1, fp);
    fwrite(rgb24, 1, imageSize, fp);
    fclose(fp);
    
    free(rgb24);
    
    LOG_INFO("Frame saved to: %s (%dx%d)", filename, width, height);
}

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
        
        // 保存为 BMP 图片
        const char* filename = "/home/cat/mpp/capture_frame.bmp";
        save_frame_as_bmp(&frame, filename);
    } else {
        LOG_WARN("hal_cam_read_frame not available in current backend");
    }
}
