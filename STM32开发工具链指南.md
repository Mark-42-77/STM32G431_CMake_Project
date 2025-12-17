# STM32 开发工具链完全指南

> 基于 VSCode + CMake + OpenOCD 的免费开源 STM32 开发环境

---

## 目录

1. [工具链概述](#工具链概述)
2. [环境安装](#环境安装)
3. [项目结构](#项目结构)
4. [开发流程](#开发流程)
5. [添加新模块](#添加新模块)
6. [编译与烧录](#编译与烧录)
7. [调试配置](#调试配置)
8. [常见问题](#常见问题)
9. [最佳实践](#最佳实践)

---

## 工具链概述

### 架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        开发流程                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐      │
│  │ 源代码    │───▶│  CMake   │───▶│  Ninja   │───▶│ .elf文件 │      │
│  │ .c/.h    │    │ 构建系统  │    │ 编译执行  │    │ 可执行   │      │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘      │
│       │                                               │             │
│       │                                               ▼             │
│       │         ┌──────────┐    ┌──────────┐    ┌──────────┐      │
│       │         │ OpenOCD  │◀───│   GDB    │◀───│ Cortex-  │      │
│       │         │ 调试服务器│    │ 调试器   │    │ Debug    │      │
│       │         └────┬─────┘    └──────────┘    └──────────┘      │
│       │              │                                             │
│       ▼              ▼                                             │
│  ┌──────────────────────────────────────────┐                      │
│  │              STM32 开发板                 │                      │
│  │         (通过 调试器 连接)                 │                      │
│  └──────────────────────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 核心组件

| 组件 | 作用 | 路径示例 |
|------|------|----------|
| **ARM GCC** | C/C++ 交叉编译器 | `stm32cube/bundles/gnu-tools-for-stm32/` |
| **CMake** | 构建系统生成器 | `stm32cube/bundles/cmake/` |
| **Ninja** | 快速构建执行器 | 内置于 CMake |
| **OpenOCD** | 调试服务器 | `xpack-openocd-0.12.0-7/` |
| **Cortex-Debug** | VSCode 调试扩展 | VSCode 扩展市场安装 |

### 与 Keil MDK 对比

| 特性 | Keil MDK | 本工具链 |
|------|----------|----------|
| 编译器 | ARMCC/ARM Clang | GCC |
| IDE | Keil μVision | VSCode |
| 构建系统 | 内置 | CMake + Ninja |
| 调试器 | 内置 | OpenOCD + GDB |
| 授权 | 商业付费 | **全部免费开源** |
| 代码大小限制 | 32KB（免费版） | **无限制** |
| 跨平台 | 仅 Windows | Win/Mac/Linux |

### Keil 代码移植注意事项

从 Keil 项目移植代码到 GCC 工具链时，需要注意以下编译器差异：

#### 1. NOP 指令写法不同

| 编译器 | NOP 写法 | 说明 |
|--------|----------|------|
| Keil/ARMCC | `__nop()` | Keil 专有函数（小写） |
| GCC/ARM | `__NOP()` | CMSIS 标准宏（大写） |

**解决方法**：将所有 `__nop()` 替换为 `__NOP()`

```c
// Keil 代码（移植前）
__nop();
__nop();
__nop();

// GCC 代码（移植后）
__NOP();
__NOP();
__NOP();
```

> 💡 `__NOP()` 是 CMSIS 库定义的跨编译器兼容宏，STM32 HAL 库已包含此定义。

#### 2. 数据类型定义

Keil 的老代码可能使用自定义类型，GCC 推荐使用标准类型：

| Keil 老写法 | 标准写法 | 头文件 |
|-------------|----------|--------|
| `u8`, `u16`, `u32` | `uint8_t`, `uint16_t`, `uint32_t` | `<stdint.h>` |
| `s8`, `s16`, `s32` | `int8_t`, `int16_t`, `int32_t` | `<stdint.h>` |
| `vu8`, `vu16` | `volatile uint8_t`, `volatile uint16_t` | - |

如果移植的库文件自带类型定义（如 `lcd.h`），可以保留不改。

#### 3. 位操作整数提升陷阱

C 语言在进行位操作时会自动将 `uint8_t` 提升为 `int`，这可能导致意外行为：

```c
// ⚠️ 危险代码
uint8_t temp = 0x08;
GPIOC->ODR |= ~(temp << 8);  // 结果不是预期的 0xF7FF！

// ✅ 正确代码
GPIOC->ODR |= (~temp & 0xFF) << 8;  // 先取反再掩码
```

**原理**：`~(temp << 8)` 会先将 `temp` 提升为 32 位 `int`，取反后得到 `0xFFFFF7FF`，而不是预期的 `0xF7FF`。

#### 4. 编译器特定属性

| Keil 写法 | GCC 写法 | 用途 |
|-----------|----------|------|
| `__attribute__((at(addr)))` | `__attribute__((section(".ARM.__at_addr")))` | 变量定位 |
| `__packed` | `__attribute__((packed))` | 紧凑结构体 |
| `__align(n)` | `__attribute__((aligned(n)))` | 内存对齐 |

#### 5. printf 重定向差异

| 编译器 | 重定向函数 | 说明 |
|--------|------------|------|
| Keil/ARMCC | `fputc()` | 重写此函数即可 |
| GCC | `_write()` | 必须重写此函数 |

**Keil 版本**（不适用于 GCC）：
```c
int fputc(int ch, FILE *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}
```

**GCC 版本**（必须添加）：
```c
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, 100);
    return len;
}
```

> 💡 建议：两个函数都保留，这样代码在两个环境都能编译。

#### 6. UART 中断接收启动

HAL 库的中断接收是"推动式"的，必须主动调用才会开始：

```c
// 在初始化代码中必须调用（通常放在 main.c 的 USER CODE BEGIN 2 区域）
HAL_UART_Receive_IT(&huart1, &rx_buffer[0], 1);
```

**工作流程**：
```
HAL_UART_Receive_IT() → 等待数据 → 收到1字节 → 触发中断 → 调用回调函数
         ↑                                                      │
         └──────────── 回调中必须再次调用才能继续接收 ←──────────┘
```

如果第一次 `HAL_UART_Receive_IT()` 从未调用，整个链条就不会启动！

#### 7. 移植检查清单

从 Keil 移植代码时，建议按以下顺序检查：

- [ ] 搜索并替换 `__nop()` → `__NOP()`
- [ ] 检查是否有 `__packed`、`__align` 等 Keil 专有关键字
- [ ] 检查位操作是否需要添加类型掩码
- [ ] 确认所有类型定义存在或已替换
- [ ] **添加 `_write()` 函数实现 printf 重定向**
- [ ] **确保 UART 中断接收已启动**
- [ ] 编译并修复剩余警告

---

## 环境安装

### 1. 安装 STM32CubeIDE（获取工具链）

下载并安装 [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)，它会自动安装：
- ARM GCC 编译器
- CMake
- STM32CubeMX

安装后工具位于：
```
C:\Users\<用户名>\AppData\Local\stm32cube\bundles\
├── gnu-tools-for-stm32\   # ARM GCC
└── cmake\                 # CMake
```

### 2. 安装 OpenOCD

下载 [xPack OpenOCD](https://github.com/xpack-dev-tools/openocd-xpack/releases)

解压到固定位置，如：
```
D:\xpack-openocd-0.12.0-7-win32-x64\
├── bin\openocd.exe
└── share\openocd\scripts\
```

### 3. 安装 VSCode 扩展

必装扩展：
- **C/C++** (ms-vscode.cpptools)
- **Cortex-Debug** (marus25.cortex-debug)
- **CMake Tools** (ms-vscode.cmake-tools)

可选扩展：
- **CMake** (twxs.cmake) - CMake 语法高亮

### 4. 安装调试器驱动

CMSIS-DAP 调试器通常需要 WinUSB 驱动：
1. 下载 [Zadig](https://zadig.akeo.ie/)
2. 菜单 `Options` → `List All Devices`
3. 选择调试器，替换驱动为 **WinUSB**

---

## 项目结构

### 标准目录结构

```
YourProject/
├── APP/                    # 👈 用户应用代码（自己创建）
│   ├── led.h
│   ├── led.c
│   ├── key.h
│   └── key.c
├── Core/                   # STM32CubeMX 生成
│   ├── Inc/                # 头文件
│   │   ├── main.h
│   │   ├── gpio.h
│   │   └── stm32g4xx_it.h
│   └── Src/                # 源文件
│       ├── main.c          # 👈 主程序入口
│       ├── gpio.c
│       └── stm32g4xx_it.c
├── Drivers/                # HAL 库（不要修改）
│   ├── CMSIS/
│   └── STM32G4xx_HAL_Driver/
├── cmake/
│   └── stm32cubemx/
│       └── CMakeLists.txt  # 构建配置（已配置自动扫描）
├── build/Debug/            # 编译输出
│   └── xxx.elf             # 可执行文件
├── .vscode/
│   └── launch.json         # 👈 调试配置
├── CMakeLists.txt          # 主构建文件
├── CMakePresets.json       # 构建预设
└── startup_stm32g431xx.s   # 启动汇编
```

### 文件职责说明

| 文件/目录 | 职责 | 是否可修改 |
|-----------|------|------------|
| `APP/` | 用户应用代码 | ✅ 自由修改，自动编译 |
| `Core/Src/main.c` | 主程序 | ⚠️ 仅在 USER CODE 区域修改 |
| `Core/Src/gpio.c` | GPIO 初始化 | ❌ CubeMX 自动生成 |
| `Drivers/` | HAL 库 | ❌ 不要修改 |
| `cmake/stm32cubemx/CMakeLists.txt` | 构建配置 | ⚠️ 一般无需修改 |
| `.vscode/launch.json` | 调试配置 | ✅ 根据硬件修改 |

---

## 开发流程

### 完整开发流程

```
1. STM32CubeMX 配置
   └── 配置时钟、引脚、外设
   └── 生成代码 (Toolchain: CMake)

2. 编写应用代码
   └── 在 APP/ 目录创建 .h 和 .c 文件
   └── 在 main.c 的 USER CODE 区域调用

3. 重新配置 CMake（仅添加新文件时需要）
   └── 删除 build 目录 或 运行 cmake --preset Debug

4. 编译
   └── cmake --build build/Debug

5. 烧录运行
   └── openocd -c "program xxx.elf verify reset exit"

6. 调试（可选）
   └── VSCode 按 F5
```

> 💡 **注意**：APP 目录已配置自动扫描，新增 .c 文件后只需重新配置 cmake，无需手动注册！

---

## 添加新模块

管理源文件有多种方案，这是一个逐步优化的过程。

### 方案演进

| 方案 | 优点 | 缺点 | 状态 |
| :--- | :--- | :--- | :--- |
| **方案 A：手动注册** | 精确控制编译内容 | 每次添加文件都要改 `CMakeLists.txt` | 已废弃 |
| **方案 B：自动扫描** | 无需改配置文件 | CubeMX 会覆盖配置 | 已废弃 |
| **方案 C：独立配置文件** | **防止 CubeMX 覆盖，一劳永逸** | 需一次性初始化配置 | **当前使用** ✅ |

---

### 方案 C：独立配置文件（当前方案）

> 🔥 **核心优势**：彻底解决 STM32CubeMX 重新生成代码时覆盖自定义配置的问题。

#### 原理

1.  **配置分离**：将用户自定义的配置（如 `APP` 目录的路径和源文件）放在一个独立的文件 `cmake/user_config.cmake` 中。这个文件 CubeMX **不会**修改。
2.  **变量追加**：`user_config.cmake` 使用 `list(APPEND)` 命令，在 CubeMX 生成的变量（如 `MX_Include_Dirs`）基础上**追加**新内容，而不是覆盖。
3.  **提前加载**：在主 `CMakeLists.txt` 中，通过 `include(user_config.cmake)` 命令提前加载我们的自定义配置，确保在编译目标生成前，所有路径和源文件都已准备就绪。

#### 手动操作指南

**第一步：创建 `cmake/user_config.cmake` 文件（一次性操作）**

1.  在 `cmake` 文件夹下新建文件 `user_config.cmake`。
2.  将以下内容粘贴进去：
    ```cmake
    # ============================================
    # 用户自定义配置文件 (此文件不会被 CubeMX 覆盖)
    # ============================================
    # 1. 添加 APP 目录到头文件搜索路径
    list(APPEND MX_Include_Dirs ${CMAKE_SOURCE_DIR}/APP)
    # 2. 自动扫描 APP 目录下的所有 .c 源文件
    file(GLOB_RECURSE APP_Sources ${CMAKE_SOURCE_DIR}/APP/*.c)
    # 3. 将扫描到的源文件追加到编译列表
    list(APPEND MX_Application_Src ${APP_Sources})
    # 4. 输出提示信息，方便调试
    message(STATUS "用户配置已加载：APP 目录已添加")
    ```

**第二步：修改 `cmake/stm32cubemx/CMakeLists.txt`（一次性操作）**

1.  打开 `cmake/stm32cubemx/CMakeLists.txt`。
2.  在 `add_library(stm32cubemx INTERFACE)` 这一行的**正上方**，添加 `include` 命令：
    ```cmake
    # ... 其他配置 ...
    # ============================================
    # 加载用户自定义配置 (在设置 target 之前)
    # ============================================
    include(${CMAKE_SOURCE_DIR}/cmake/user_config.cmake)

    # Interface library for includes and symbols
    add_library(stm32cubemx INTERFACE)
    # ... 其他配置 ...
    ```

**第三步：日常开发流程**

*   **添加新模块**：
    1.  在 `APP` 文件夹中创建 `.h` 和 `.c` 文件。
    2.  删除 `build` 目录（或使用 `--clean-first` 参数重新编译）。
*   **使用 CubeMX 后**：
    1.  重新生成代码。
    2.  **唯一要做的**：检查 `cmake/stm32cubemx/CMakeLists.txt` 中 `include(...)` 那一行是否还在，如果被删了就**重新加上**。

---

### (已废弃) 方案 B：自动扫描

> **问题**：此方案的配置直接写在 `CMakeLists.txt` 中，会被 CubeMX 无情覆盖。

```cmake
# 直接在 CMakeLists.txt 中添加
file(GLOB_RECURSE APP_Sources ${CMAKE_SOURCE_DIR}/APP/*.c)
set(MX_Application_Src ... ${APP_Sources})
```

---

### (已废弃) 方案 A：手动注册

> **问题**：最原始的方法，每次添加/删除文件都需要手动修改 `CMakeLists.txt`，非常繁琐且容易出错。

```cmake
# 每次都要手动添加
set(MX_Application_Src
    ...
    ${CMAKE_SOURCE_DIR}/APP/led.c
    ${CMAKE_SOURCE_DIR}/APP/key.c
)
```

---

### 创建模块示例

#### 步骤 1：创建头文件

```c
// APP/led.h
#ifndef LED_H
#define LED_H

#include "main.h"  // 👈 包含 HAL 库基础定义

void LED_Init(void);
void LED_On(uint8_t index);
void LED_Off(uint8_t index);
void LED_Toggle(uint8_t index);

#endif
```

#### 步骤 2：创建源文件

```c
// APP/led.c
#include "led.h"

void LED_Init(void) {
    // 初始化代码
}

void LED_On(uint8_t index) {
    // 点亮 LED
}

void LED_Off(uint8_t index) {
    // 熄灭 LED
}

void LED_Toggle(uint8_t index) {
    // 翻转 LED
}
```

#### 步骤 3：在 main.c 中使用

```c
// Core/Src/main.c

/* USER CODE BEGIN Includes */
#include "led.h"
/* USER CODE END Includes */

int main(void) {
    // ...
    
    /* USER CODE BEGIN 2 */
    LED_Init();
    /* USER CODE END 2 */

    while (1) {
        /* USER CODE BEGIN 3 */
        LED_Toggle(0);
        HAL_Delay(500);
        /* USER CODE END 3 */
    }
}
```

---

## 编译与烧录

### 编译命令

```bash
# 方法 1：使用完整路径
"C:\Users\<用户名>\AppData\Local\stm32cube\bundles\cmake\4.0.1+st.3\bin\cmake.exe" --build build/Debug

# 方法 2：如果 CMake 在 PATH 中
cmake --build build/Debug

# 清理后重新编译
cmake --build build/Debug --clean-first
```

### 烧录命令

```bash
# 基本烧录命令
"D:\xpack-openocd-0.12.0-7-win32-x64\bin\openocd.exe" ^
    -s "D:\xpack-openocd-0.12.0-7-win32-x64\share\openocd\scripts" ^
    -f interface/cmsis-dap.cfg ^
    -f target/stm32g4x.cfg ^
    -c "program build/Debug/demo251216.elf verify reset exit"
```

### 一键编译烧录

```bash
# Windows CMD（用 && 连接）
cmake --build build/Debug && openocd -f interface/cmsis-dap.cfg -f target/stm32g4x.cfg -c "program build/Debug/xxx.elf verify reset exit"
```

### OpenOCD 命令参数说明

| 参数 | 说明 |
|------|------|
| `-s` | scripts 目录路径 |
| `-f interface/xxx.cfg` | 调试器接口配置 |
| `-f target/xxx.cfg` | 目标芯片配置 |
| `-c "program ..."` | 执行烧录命令 |
| `verify` | 校验烧录结果 |
| `reset` | 复位运行 |
| `exit` | 完成后退出 |

### 常用接口配置文件

| 调试器类型 | 配置文件 |
|------------|----------|
| CMSIS-DAP / DAP-Link | `interface/cmsis-dap.cfg` |
| ST-Link | `interface/stlink.cfg` |
| J-Link | `interface/jlink.cfg` |

### 常用芯片配置文件

| 芯片系列 | 配置文件 |
|----------|----------|
| STM32F1 | `target/stm32f1x.cfg` |
| STM32F4 | `target/stm32f4x.cfg` |
| STM32G4 | `target/stm32g4x.cfg` |
| STM32H7 | `target/stm32h7x.cfg` |

---

## 调试配置

### launch.json 配置

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug with OpenOCD",
            "type": "cortex-debug",
            "request": "launch",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/Debug/demo251216.elf",
            "servertype": "openocd",
            "serverpath": "D:\\xpack-openocd-0.12.0-7-win32-x64\\bin\\openocd.exe",
            "searchDir": ["D:\\xpack-openocd-0.12.0-7-win32-x64\\share\\openocd\\scripts"],
            "configFiles": [
                "interface/cmsis-dap.cfg",
                "target/stm32g4x.cfg"
            ],
            "gdbPath": "C:\\Users\\peng\\AppData\\Local\\stm32cube\\bundles\\gnu-tools-for-stm32\\14.3.1+st.2\\bin\\arm-none-eabi-gdb.exe",
            "device": "STM32G431CB",
            "runToEntryPoint": "main",
            "showDevDebugOutput": "raw"
        }
    ]
}
```

### 配置项说明

| 配置项 | 说明 |
|--------|------|
| `servertype` | 必须是 `openocd`（不是 pyocd） |
| `serverpath` | OpenOCD 可执行文件路径 |
| `searchDir` | OpenOCD scripts 目录 |
| `configFiles` | 接口和芯片配置文件 |
| `gdbPath` | GDB 调试器路径 |
| `device` | 具体芯片型号 |
| `showDevDebugOutput` | 设为 `raw` 可看详细日志 |

---

## 常见问题

### Q1: 编译报错 "找不到头文件"

**原因**：头文件目录未添加到包含路径

**解决**：修改 `cmake/stm32cubemx/CMakeLists.txt`，在 `MX_Include_Dirs` 中添加目录：
```cmake
set(MX_Include_Dirs${CMAKE_SOURCE_DIR}/APP)   # 添加这行
```

### Q2: 链接报错 "undefined reference to xxx"

**原因**：
1. 源文件不在 APP 目录
2. 添加新文件后未重新配置 CMake

**解决**：
```bash
# 1. 确保 .c 文件在 APP 目录下

# 2. 重新配置 CMake
rd /s /q build
cmake --preset Debug
cmake --build build/Debug
```

> 💡 APP 目录已配置自动扫描，无需手动注册文件！

### Q3: OpenOCD 报错 "unable to find CMSIS-DAP device"

**原因**：
1. 调试器未连接
2. 驱动未安装
3. 被其他软件占用

**解决**：
1. 检查 USB 连接
2. 使用 Zadig 安装 WinUSB 驱动
3. 关闭 Keil、STM32CubeIDE 等软件

### Q4: OpenOCD 报错 "target not halted"

**原因**：目标芯片无法停止

**解决**：
1. 检查 SWD 连接（SWDIO、SWCLK、GND）
2. 检查目标板是否供电
3. 尝试按住复位键再连接

### Q5: 调试时 servertype 配置错误

**错误**：设置 `servertype: "pyocd"` 但使用 OpenOCD

**解决**：确保 `servertype` 与实际使用的调试服务器匹配：
```json
"servertype": "openocd"   // 使用 OpenOCD
"servertype": "pyocd"     // 使用 PyOCD
"servertype": "jlink"     // 使用 J-Link
```

### Q6: 头文件循环包含警告

**错误示例**：
```c
// mydefine.h
#include "led.h"    // mydefine.h 包含 led.h

// led.h
#include "mydefine.h"  // led.h 又包含 mydefine.h ← 循环！
```

**解决**：打破循环，让依赖关系单向：
```c
// led.h - 只包含必需的基础头文件
#include "main.h"   // ✅ 正确

// mydefine.h - 作为统一入口包含各模块
#include "led.h"
#include "key.h"
```

### Q7: CMakeLists.txt 中误放 .h 文件

**错误**：
```cmake
set(MX_Application_Src
    ${CMAKE_SOURCE_DIR}/APP/led.c
    ${CMAKE_SOURCE_DIR}/APP/led.h   # ❌ 头文件不应该在这里
)
```

**解决**：源文件列表只放 `.c` 和 `.s` 文件，头文件通过 `MX_Include_Dirs` 指定目录即可。

### Q8: VSCode 显示警告但 Keil 不显示

**现象**：同样的代码在 Keil 中没有警告，但在 VSCode 中显示循环包含等警告。

**原因**：警告来源不同

| 工具 | 行为 |
|------|------|
| Keil ARMCC | 有 `#ifndef` 保护就不报警告 |
| GCC 编译器 | 同样不报错，能正常编译 |
| **clangd (VSCode)** | 静态分析工具，会检测潜在问题 ⚠️ |

VSCode 的警告来自 **clangd**（C/C++ 智能提示引擎），不是编译器。代码实际上能正常编译运行。

**解决**：在项目根目录的 `.clangd` 文件中添加警告抑制：

```yaml
CompileFlags:
  Add:
    - '-ferror-limit=0'
    - '-Wno-implicit-int'
  CompilationDatabase: build/Debug
Diagnostics:
  Suppress:
    - unused-includes
    - unknown_typename
    - unknown_typename_suggest
    - typename_requires_specqual
    - pp_including_mainfile_in_preamble  # 循环包含警告
    - recursive_include                   # 递归包含警告
```

修改后重启 VSCode 或执行命令 `clangd: Restart language server` 生效。

### Q9: 添加新文件后编译没生效

**现象**：添加了新的 `.c` 文件，但编译还是运行旧代码。

**原因**：使用 `file(GLOB)` 自动扫描时，添加新文件后没有重新配置 CMake。

**解决**：使用 `--clean-first` 强制重新配置和编译：
```bash
"C:\Users\<用户名>\AppData\Local\stm32cube\bundles\cmake\4.0.1+st.3\bin\cmake.exe" --build build/Debug --clean-first
```

或者手动删除 build 目录：
```bash
# PowerShell
Remove-Item -Recurse -Force build

# CMD
rmdir /s /q build

# 然后重新编译
cmake --build build/Debug
```

### Q10: STM32CubeMX 重新生成代码后配置丢失

**现象**：使用 CubeMX 重新生成代码后，APP 目录找不到，编译失败。

**原因**：STM32CubeMX 会**完全覆盖** `cmake/stm32cubemx/CMakeLists.txt`，导致自定义配置丢失。

**解决方案**：使用独立配置文件（方案 C）

1. **创建** `cmake/user_config.cmake`（参见"添加新模块 - 方案 C"）
2. **在 `cmake/stm32cubemx/CMakeLists.txt` 中添加一行**：
   ```cmake
   include(${CMAKE_SOURCE_DIR}/cmake/user_config.cmake)
   ```
3. **每次 CubeMX 重新生成代码后，检查并重新添加这一行**

**预防措施**：
- 将 `include(user_config.cmake)` 这一行记录在项目文档中
- 使用版本控制跟踪 `cmake/user_config.cmake` 文件
- CubeMX 重新生成代码后，第一时间检查 CMakeLists.txt

---

## 最佳实践

### 代码组织

1. **模块化设计**
   - 每个功能独立成 .h/.c 文件
   - 头文件使用 `#ifndef` 保护
   - 避免全局变量，使用 `static` 限制作用域

2. **目录规范**
   ```
   APP/
   ├── led.h / led.c      # LED 控制
   ├── key.h / key.c      # 按键扫描
   ├── uart.h / uart.c    # 串口通信
   └── mydefine.h         # 统一包含入口（可选）
   ```

3. **命名规范**
   - 函数名：`模块_动作()`，如 `LED_On()`、`KEY_Scan()`
   - 宏定义：全大写，如 `LED_PIN_0`
   - 局部变量：小写驼峰，如 `ledIndex`

4. **头文件包含规范**
   - 避免循环包含
   - 模块头文件只包含 `main.h`
   - 可创建统一入口文件（如 `mydefine.h`）包含所有模块

### 调试技巧

1. **查看详细日志**
   ```json
   "showDevDebugOutput": "raw"
   ```

2. **添加 SVD 文件查看外设寄存器**
   ```json
   "svdFile": "${workspaceFolder}/STM32G431xx.svd"
   ```

3. **使用条件断点**
   - 右键断点 → 编辑条件
   - 输入条件表达式，如 `i == 100`

### 版本控制

`.gitignore` 推荐配置：
```gitignore
build/
*.elf
*.map
*.o
*.obj
.cache/
```

---

## 快速参考卡片

### 常用命令

```bash
# 编译
cmake --build build/Debug

# 烧录
openocd -f interface/cmsis-dap.cfg -f target/stm32g4x.cfg -c "program build/Debug/xxx.elf verify reset exit"

# 编译 + 烧录
cmake --build build/Debug && openocd -f interface/cmsis-dap.cfg -f target/stm32g4x.cfg -c "program build/Debug/xxx.elf verify reset exit"
```

### 添加新模块检查清单

**方案 A（自动扫描）**：
- [ ] 创建 `APP/xxx.h`（头文件，包含 `main.h`）
- [ ] 创建 `APP/xxx.c`（源文件）
- [ ] 删除 `build` 目录
- [ ] 在 `main.c` 中 `#include "xxx.h"`
- [ ] 编译：`cmake --build build/Debug`

**方案 B（手动注册）**：
- [ ] 创建 `APP/xxx.h`（头文件）
- [ ] 创建 `APP/xxx.c`（源文件）
- [ ] 修改 `CMakeLists.txt` 添加 `.c` 文件
- [ ] 在 `main.c` 中 `#include "xxx.h"`
- [ ] 编译：`cmake --build build/Debug`

**方案 C（独立配置 - 推荐）**：
- [ ] 【首次配置】创建 `cmake/user_config.cmake`
- [ ] 【首次配置】在 `CMakeLists.txt` 中添加 `include(user_config.cmake)`
- [ ] 创建 `APP/xxx.h`（头文件）
- [ ] 创建 `APP/xxx.c`（源文件）
- [ ] 删除 `build` 目录
- [ ] 编译：`cmake --build build/Debug --clean-first`
- [ ] 【CubeMX后】检查 `include` 这一行是否还在

---

*最后更新：2025-12-17*