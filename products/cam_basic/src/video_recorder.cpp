#include "video_recorder.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>

#include "log_system.h"

#define MAX_FRAMES 1000  // 最大录制帧数

struct VideoRecorderCtx {
    bool            is_recording;
    char            output_dir[256];
    int             frame_count;
    int             max_frames;
    int             width;
    int             height;
};

static VideoRecorderCtx g_recorder = {0};

// 获取当前时间字符串
static void get_timestamp_str(char* buf, size_t len)
{
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    snprintf(buf, len, "%04d%02d%02d_%02d%02d%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

// 创建目录
static int mkdir_p(const char* path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0755);
    }
    return 0;
}

bool video_recorder_start(int width, int height, int /*fps*/)
{
    if (g_recorder.is_recording) {
        LOG_WARN("Already recording");
        return false;
    }
    
    // 创建输出目录
    char timestamp[32];
    get_timestamp_str(timestamp, sizeof(timestamp));
    snprintf(g_recorder.output_dir, sizeof(g_recorder.output_dir),
             "/home/cat/mpp/video_%s", timestamp);
    
    if (mkdir_p(g_recorder.output_dir) != 0) {
        LOG_ERROR("Failed to create directory: %s", g_recorder.output_dir);
        return false;
    }
    
    g_recorder.width = width;
    g_recorder.height = height;
    g_recorder.frame_count = 0;
    g_recorder.max_frames = MAX_FRAMES;
    g_recorder.is_recording = true;
    
    LOG_INFO("Video recording started");
    LOG_INFO("  Output: %s/", g_recorder.output_dir);
    LOG_INFO("  Format: frame_0000.bmp - frame_%04d.bmp", MAX_FRAMES - 1);
    LOG_INFO("  Max frames: %d (about %d seconds @ 30fps)", MAX_FRAMES, MAX_FRAMES / 30);
    
    return true;
}

void video_recorder_stop()
{
    if (!g_recorder.is_recording) {
        return;
    }
    
    g_recorder.is_recording = false;
    
    LOG_INFO("Video recording stopped");
    LOG_INFO("  Saved %d frames to: %s/", g_recorder.frame_count, g_recorder.output_dir);
    LOG_INFO("  Convert to video with:");
    LOG_INFO("    ffmpeg -framerate 30 -i %s/frame_%%04d.bmp -c:v libx264 -pix_fmt yuv420p output.mp4",
             g_recorder.output_dir);
}

// NV12 to RGB24 转换（用于保存 BMP）
static void nv12_to_rgb24(const uint8_t* nv12, uint8_t* rgb24, int width, int height)
{
    const uint8_t* y_plane = nv12;
    const uint8_t* uv_plane = nv12 + width * height;
    
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int y_idx = y * width + x;
            int uv_idx = (y / 2) * width + (x / 2) * 2;
            
            int Y = y_plane[y_idx];
            int U = uv_plane[uv_idx] - 128;
            int V = uv_plane[uv_idx + 1] - 128;
            
            int r = Y + (int)(1.402f * V);
            int g = Y - (int)(0.344f * U + 0.714f * V);
            int b = Y + (int)(1.772f * U);
            
            r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
            g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
            b = (b < 0) ? 0 : ((b > 255) ? 255 : b);
            
            int rgb_idx = ((height - 1 - y) * width + x) * 3;
            rgb24[rgb_idx + 0] = (uint8_t)b;
            rgb24[rgb_idx + 1] = (uint8_t)g;
            rgb24[rgb_idx + 2] = (uint8_t)r;
        }
    }
}

// 保存单帧为 BMP
static void save_bmp_frame(const uint8_t* nv12, int width, int height, const char* filename)
{
    #pragma pack(push, 1)
    struct BmpHeader {
        uint16_t type = 0x4D42;
        uint32_t size;
        uint16_t reserved1 = 0;
        uint16_t reserved2 = 0;
        uint32_t offset = 54;
    } fileHeader;
    
    struct BmpInfo {
        uint32_t size = 40;
        int32_t width;
        int32_t height;
        uint16_t planes = 1;
        uint16_t bitCount = 24;
        uint32_t compression = 0;
        uint32_t imageSize;
        int32_t xPPM = 0;
        int32_t yPPM = 0;
        uint32_t colorsUsed = 0;
        uint32_t colorsImportant = 0;
    } infoHeader;
    #pragma pack(pop)
    
    int rowSize = ((width * 3 + 3) / 4) * 4;
    int imageSize = rowSize * height;
    
    fileHeader.size = 54 + imageSize;
    infoHeader.width = width;
    infoHeader.height = height;
    infoHeader.imageSize = imageSize;
    
    uint8_t* rgb24 = (uint8_t*)malloc(imageSize);
    if (!rgb24) return;
    
    nv12_to_rgb24(nv12, rgb24, width, height);
    
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        fwrite(&fileHeader, 1, sizeof(fileHeader), fp);
        fwrite(&infoHeader, 1, sizeof(infoHeader), fp);
        fwrite(rgb24, 1, imageSize, fp);
        fclose(fp);
    }
    
    free(rgb24);
}

bool video_recorder_encode_frame(const HalVideoFrame* frame)
{
    if (!g_recorder.is_recording || !frame) {
        return false;
    }
    
    if (g_recorder.frame_count >= g_recorder.max_frames) {
        LOG_INFO("Reached max frames (%d), stopping recorder", g_recorder.max_frames);
        video_recorder_stop();
        return false;
    }
    
    // 生成文件名
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.bmp",
             g_recorder.output_dir, g_recorder.frame_count);
    
    // 保存帧
    save_bmp_frame((const uint8_t*)frame->vir_addr, frame->width, frame->height, filename);
    
    g_recorder.frame_count++;
    
    // 每 30 帧输出一次日志
    if (g_recorder.frame_count % 30 == 0) {
        LOG_INFO("Recorded %d frames (%.1f seconds)",
                 g_recorder.frame_count, g_recorder.frame_count / 30.0f);
    }
    
    return true;
}

bool video_recorder_is_recording()
{
    return g_recorder.is_recording;
}
