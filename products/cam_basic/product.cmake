# products/cam_basic/product.cmake
# 基础摄像头产品: 只用核心采集 + 日志, 不依赖未实现的组件

set(PG_ENABLE_MEDIA_CORE     OFF)
set(PG_ENABLE_AI_ENGINE      OFF)
set(PG_ENABLE_ALARM_SYSTEM   OFF)
set(PG_ENABLE_NETWORK_STACK  OFF)
set(PG_ENABLE_OTA            OFF)
set(PG_ENABLE_STORAGE        OFF)
