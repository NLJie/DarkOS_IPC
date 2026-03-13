# x86_64 编译全部
./compile.sh

# aarch64 交叉编译全部 (自动使用 aarch64-linux-gnu- 前缀)
./compile.sh --arch=aarch64

# 只编译某几个组件
./compile.sh --arch=aarch64 mpp ffmpeg

# 清理后重新编译
./compile.sh --clean x264

# 指定自定义工具链
./compile.sh --arch=aarch64 --cross=aarch64-rockchip-linux-gnu-

# 控制线程数
./compile.sh --jobs=4