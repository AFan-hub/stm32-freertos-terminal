# Day 19 代码质量自查（STM32 + FreeRTOS）

按手册 Day 19 清单逐项检查，结论：**通过**，附两处已记录的已知限制。

## 1. magic number

| 位置 | 处理 |
|---|---|
| 协议常量（0xAA/0x55/VER/CMD） | `protocol.h` 宏集中定义，Pi 端 `Protocol.hpp` 常量对应 |
| 配置魔数 `0xA55AA55A` | 出现于 `bsp_flash.c`（校验）与 `app_main.c`（默认配置）；有意不去耦合默认配置与 flash 模块，**保留双处并注释** |
| 任务栈/优先级/缓冲尺寸 | `app_main.c` 的 `STACK_*`/`PRIO_*`、`bsp_uart.c` 的环缓冲尺寸均为命名常量 |
| TFT 屏幕布局坐标 | `task_display.c` 内联字面量，属 UI 布局参数，**接受**（改动频率高、命名收益低） |
| CLI 参数范围（10..5000） | `cli.c` 内联并带注释，属交互契约，接受 |

## 2. const / volatile

- `gHeartbeat[]`、`gFaultFlags`：`volatile`（任务间读取），32 位对齐写天然原子
- 环缓冲 `head/tail`：`volatile`（任务/ISR 双上下文），SPSC 模型，文档见 `ring_buffer.h`
- 只读指针参数全部 `const`；`sDefaultConfig` 为 `static const`
- RAW 串口发送的 HAL 形参无 `const`，调用点显式去 const 并注释（HAL 只读缓冲）

## 3. 错误码

- `bsp_mpu6050` init 返回分级码：`-1xx`（HAL 状态+错误码）、`-2xx`（读到错误 ID）、`-2/-3/-4`（复位/唤醒/配置写失败）——实测发挥了作用（-114 定位 AF）
- `bsp_flash`：0 成功 / -1 擦写或校验失败
- 回调/挂起失败路径统一置 `gFaultFlags` 对应位并同步事件组

## 4. 生命周期

- 5 个任务常驻（设计如此），无动态创建/删除
- 队列/互斥量/事件组一次性创建，`configASSERT` 保护
- 栈溢出钩子：打印任务名后关中断停机（最后防线）
- 无堆外裸 malloc（FreeRTOS heap_4 统一管理）

## 5. include 层级

`BSP → Components → App` 单向依赖成立：

- `Components/{ring_buffer,crc16,protocol}` 不含任何 HAL 头文件，可在 PC 上单测
- App 经 `app_main.h` 引入 FreeRTOS 类型；HAL 句柄经 `main.h`
- 头文件全部有 include guard

## 6. 全局变量

| 全局 | 保护方式 |
|---|---|
| `gSystemConfig` | ConfigMutex（写路径全部持锁；任务读采样周期为单字段读，可接受） |
| `gHeartbeat` / `gFaultFlags` | volatile + 32 位原子写 |

**已知限制（不修，记录）**

1. `App_SetFault` 的 `gFaultFlags |=` 为读-改-写，多任务并发置位理论上可丢一次位；后果仅是多等一个监控周期，且当前置位点均不在高频路径。若要严格，用 `taskENTER_CRITICAL` 包住，代价为零——留作面试可讲的改进点
2. `gSystemConfig` 单字段读未持锁（采样周期），与 CLI 写入存在理论竞态；`uint16_t` 读写原子，最坏情况取到新旧值之一，不影响功能

## 7. 本轮项目发现并修复的真 bug（质量证据）

| bug | 教训 |
|---|---|
| `sizeof(SensorPayload)` 对齐填充 vs 打包结构 | 线上长度用显式常量，不用 sizeof |
| `VMIN=0` tty 的 `read()==0` 二义性 | 用节点存在性判拔线 |
| TSVREFE 写到 CSR 而非 CCR | 裸寄存器前先查偏移表 |
| I2C 400kHz 长线边沿劣化 | 降 100kHz + 诊断四件套 |
| systemd 熔断 / USB 设备号漂移 | reset-failed + by-id |
