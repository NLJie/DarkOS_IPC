include option.mk

# ====== 编译器配置 ======
CC := $(CPPCOMPILE)
STRIP := $(STRIP)

# ====== 目录结构 ======
SRC_DIR := pg_source
INC_DIRS := rk_include pg_include
OBJ_DIR := obj
BIN_DIR := bin
TARGET := $(BIN_DIR)/pg_main

# ====== 查找所有 .c 和 .cpp 文件（最多 5 层） ======
C_SRCS   := $(shell find $(SRC_DIR) -maxdepth 5 -name "*.c")
CPP_SRCS := $(shell find $(SRC_DIR) -maxdepth 5 -name "*.cpp")
SRCS     := $(C_SRCS) $(CPP_SRCS)

# ====== 生成 .o 文件路径（保持目录结构） ======
OBJS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(SRCS:.c=.o))
OBJS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(OBJS:.cpp=.o))

# ====== 头文件路径 ======
ALL_INC_DIRS := $(foreach d, $(INC_DIRS), $(shell find ./$(d) -maxdepth 5 -type d))
CFLAGS := -Wall -D__LINUX__ -fpermissive -std=c++11
CFLAGS += $(foreach dir, $(ALL_INC_DIRS), -I$(dir))

# ====== 库收集与链接参数 ======
LIB_DIRS := lib rk_lib pg_ir_lib pg_ir_lib/bp_v3
LDFLAGS := $(foreach dir, $(LIB_DIRS), -L$(PWD)/$(dir))
LIB_NAMES := $(foreach dir, $(LIB_DIRS), $(notdir $(wildcard $(dir)/lib*.a) $(wildcard $(dir)/lib*.so)))
LDFLAGS += $(addprefix -l, $(patsubst lib%.so,%, $(patsubst lib%.a,%, $(LIB_NAMES))))
LDFLAGS += -lstdc++ -lm -ldl -lpthread  -rdynamic

# ====== 默认目标 ======
.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "==> Linking $(TARGET)"
	@mkdir -p $(BIN_DIR)
	$(CC) -O3 $(CFLAGS) -o $@ $^ -Xlinker --start-group $(LDFLAGS) -Xlinker --end-group
	$(STRIP) $@

# ====== 编译规则：.c 和 .cpp ======
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "==> Compiling $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo "==> Compiling $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# ====== 清理 ======
clean:
	@echo "==> Cleaning..."
	@rm -rf $(OBJ_DIR) $(TARGET)
