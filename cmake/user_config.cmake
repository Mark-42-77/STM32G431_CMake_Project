# ============================================
# 用户自定义配置文件
# 此文件不会被 STM32CubeMX 覆盖
# ============================================

# 添加用户应用目录到包含路径
list(APPEND MX_Include_Dirs
    ${CMAKE_SOURCE_DIR}/APP
)

# 自动扫描 APP 目录下的所有 .c 文件
file(GLOB_RECURSE APP_Sources ${CMAKE_SOURCE_DIR}/APP/*.c)

# 添加到应用源文件列表
list(APPEND MX_Application_Src ${APP_Sources})

# 输出提示信息
message(STATUS "用户配置已加载：APP 目录已添加")
message(STATUS "自动扫描到 ${CMAKE_CURRENT_LIST_DIR}/../APP 目录下的源文件")