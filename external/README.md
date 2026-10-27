# 硬件资料索引

> **本目录不入 Git 仓库。** `.gitignore` 忽略了 `external/*`（PDF/图片体积大、可再获取），
> 只保留本 README 作为索引。克隆仓库后这里会是空的，按下方清单自行放置文件。
>
> 本目录存放**只读**的第三方资料，分两个子目录：

| 子目录 | 内容 | 定位 |
|---|---|---|
| `使用手册/` | 芯片数据手册、参考手册 | **写固件要翻的** |
| `硬件引脚/` | 三块板原理图 + 整机渲染图 | **查引脚连接要翻的** |

> 这里的文件不参与编译，不进构建产物，只作为查证依据。
> 引用时请写清「文件名 + 章节/页码」，方便复核。
> 引脚级整理（不用翻原理图）见 `docs/IUO112_引脚配置清单.md`。

---

## 1. 必留：写固件要翻的（5 份）

| 文件 | 查什么 |
|---|---|
| `使用手册/STM32F405RGT6_Datasheet.pdf`（DS8626 Rev12，206 页） | **复用功能表**（哪个引脚能出 TIM3_CH4）、电气特性、封装、时钟树上限 |
| `使用手册/STM32F405_RM0090_ReferenceManual.pdf`（Rev22，1741 页） | **寄存器级权威**：TIM1/3/4 高级定时器与死区、ADC 注入/规则组、SPI2、USART2/5、DMA、RCC、IWDG、Flash 编程 |
| `使用手册/ICM-42688-P_Datasheet.pdf`（DS-000347，109 页） | 寄存器映射、SPI **只支持模式 3**（CPOL=1/CPHA=1）、WHO_AM_I = `0x47`、量程与零偏、启动时间 |
| `使用手册/MT9101ET_Datasheet.pdf` | **唯一的电角度来源**。这是 MT910X **系列**手册（已核对首页，内容为 Ratiometric Linear Hall IC），MT9101ET 是系列成员，不必另找单型号版。要点：无 A/B/Z 数字输出，只能 ADC 采样，没有霍尔中断可用 |
| `使用手册/C5370899_..._SS6343M_..._中文规格书.PDF`（≈10 页） | 单 PWM 每相真值表、内部互补 + 40 ns 死区、nSLEEP 建立时间、V3P3 LDO 电流、过流/过温保护行为。文件名是供应商原始命名，未改名以免与采购记录对不上 |

**查证顺序**：先 `RM0090` 定寄存器，再回数据手册确认这个引脚/封装上确实有这个功能。

## 2. 备查：留着但基本不翻（2 份）

| 文件 | 什么时候才用 |
|---|---|
| `使用手册/STM32F4_PM0214_ProgrammingManual.pdf`（Rev10，262 页） | RM0090 **不含**内核寄存器。配置 NVIC 优先级分组、SCB、SysTick、FPU（CPACR）时必须翻这份 |
| `使用手册/SY8303A_Buck_Datasheet.pdf`（6 页） | 板载 BOM 用的是 **SY8303AAIC**，固件不碰电源，只供硬件查证（反馈电阻公式、开关频率、最小压差） |

## 3. 已删除的资料（避免再找回来）

2026-10-27 清理，共 3 份。判断标准：**这些器件没有任何引脚连到 STM32**，固件读不到也控不了，其结论已写进 `docs/硬件设计总结.md`。

| 已删文件 | 器件 | 为什么不需要 | 结论在哪 |
|---|---|---|---|
| `AMS1117_Datasheet.pdf` | AMS1117-3.3 | 固定输出、无使能脚、与 MCU 无任何连接 | `硬件设计总结.md` B5（压差 3.9 V，耗散 0.78 W） |
| `DS8242_LDO_Datasheet.pdf` | DS8242-33S3 | IMU 板本地 LDO，MCU 不控制、不测量 | `硬件设计总结.md` 电源树 |
| `SY8303_Buck_Datasheet.pdf`（10 页） | SY8303 | 与 `SY8303A` 重复；带 Oneyac 水印，非原件；BOM 用的是 A 版 | `硬件设计总结.md` 电源树 |

## 4. 硬件引脚（原理图）

| 板卡 | 文件 | 内容 |
|---|---|---|
| IUO112 主控板 | `硬件引脚/IUO112-SCH.pdf` | STM32F405 + 3×SS6343M + 电源 + 接口，引脚级整理见 `docs/IUO112_引脚配置清单.md` |
| BOD12-11 电机板 | `硬件引脚/BOD12-11-SCH.pdf` | SS6343M + 2×MT9101ET + 40P FPC |
| R14AQ IMU 板 | `硬件引脚/R14AQ-SCH.pdf` | ICM-42688-P + DS8242-33S3 + 加热电阻 |
| 整机 | `硬件引脚/云台整机渲染图.jpg` | 产品外观参考，非技术资料 |

## 5. 已知缺口

| 缺什么 | 影响 | 备注 |
|---|---|---|
| `STM32F405.svd` | 调试时看不到外设寄存器名，只能看地址 | 可从 STM32CubeF4 仓库取，放到 `硬件引脚/` 或新建 `调试/` 后在 `launch.json` 配 `svdFile` |
| SS6343M 英文原版手册 | 中文版个别参数表译名不一致 | 现用中文版够用 |
| ICM-42688-P Register Map 单行本 | 寄存器表在数据手册 §9 内，够用 | 若嫌翻页慢可另下 `rm-000048` |

## 6. 下载记录

| 日期 | 文件 | 来源 |
|---|---|---|
| 2026-10-27 | `STM32F405RGT6_Datasheet.pdf` | st.com `resource/en/datasheet/stm32f405rg.pdf` |
| 2026-10-27 | `STM32F405_RM0090_ReferenceManual.pdf` | st.com `resource/en/reference_manual/rm0090-...pdf` |
| 2026-10-27 | `STM32F4_PM0214_ProgrammingManual.pdf` | st.com `resource/en/programming_manual/pm0214-...pdf` |
| 2026-10-27 | `ICM-42688-P_Datasheet.pdf` | InvenSense DS-000347（Octopart 镜像） |
| 2026-10-27 | `SY8303A_Buck_Datasheet.pdf` | Silergy（Promelec 镜像） |
| 本地既有 | `MT9101ET_Datasheet.pdf`、`SS6343M 中文规格书`、三份原理图、整机渲染图 | 非本轮下载 |
