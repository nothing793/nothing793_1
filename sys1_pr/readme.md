# sys1_pr：RV64I 单周期 CPU

这是一个使用 SystemVerilog 编写的 RV64I 单周期处理器。实现采用经典的“取指 - 译码 - 执行 - 访存 - 写回”数据通路：一条指令在一个时钟周期内完成组合逻辑计算，周期结束时更新 PC 和寄存器堆。

项目适合作为 CPU/计算机组成课程中的处理器核心提交代码。`Core` 只负责处理器本身，指令存储器和数据存储器通过 `Mem_ift` 接口由外部 testbench 或 SoC 提供。

## 设计概览

### 数据通路

```
PC ──> 指令存储器 ──> 指令译码 ──> 立即数生成器
 │                         │              │
 └─────────────────────────┴──────┐       │
                                  ▼       ▼
                           寄存器堆 ──> ALU ──> 数据存储器
                              │          │          │
                              └──────────┴── 写回选择器 ──> 寄存器堆
```

- 数据宽度 `xLen = 64`，地址宽度为 64 位，指令宽度为 32 位。
- PC 复位为 `0`，默认每周期执行 `PC + 4`。
- 分支和跳转目标由 ALU 计算；条件分支由 `Cmp` 判断是否成立。
- `x0` 永远读为零，写入 `x0` 会被丢弃；`x1` 至 `x31` 在复位时清零。
- 指令存储器按 8 字节对齐请求，`pc[2]` 用于选择返回数据中的低 32 位或高 32 位指令。
- 全零指令（`0x00000000`）和 `EBREAK`（`0x00100073`）被视为停机指令，PC 保持不变。

### 支持的指令

控制器按 opcode、funct3 和 funct7 译码，当前覆盖以下指令类别：

| 类别 | 指令 |
| --- | --- |
| 寄存器运算（R） | `ADD`、`SUB`、`SLL`、`SLT`、`SLTU`、`XOR`、`SRL`、`SRA`、`OR`、`AND` |
| 立即数运算（I） | `ADDI`、`SLLI`、`SLTI`、`SLTIU`、`XORI`、`SRLI`、`SRAI`、`ORI`、`ANDI` |
| 32 位运算（W） | `ADDW`、`SUBW`、`SLLW`、`SRLW`、`SRAW`、`ADDIW`、`SLLIW`、`SRLIW`、`SRAIW` |
| 访存 | `LB`、`LH`、`LW`、`LD`、`LBU`、`LHU`、`LWU`、`SB`、`SH`、`SW`、`SD` |
| 分支 | `BEQ`、`BNE`、`BLT`、`BGE`、`BLTU`、`BGEU` |
| 跳转 | `JAL`、`JALR` |
| 高位立即数 | `LUI`、`AUIPC` |

未列出的 opcode 会使用默认控制信号，不产生寄存器写回或访存操作。

## 目录结构

```
include/
├── core_struct.vh   # CorePack 类型、枚举、opcode/funct 常量
├── decoupled.vh     # Decoupled 信号宏
├── mem_ift.vh       # 存储器成员和 modport 宏
└── initial_mem.vh   # 仿真时 testcase.hex 的路径配置
submit/
├── Core.sv          # CPU 顶层和主数据通路
├── Controller.sv    # 指令译码与控制信号生成
├── ALU.sv           # 64 位及 RV64I W 类运算
├── Cmp.sv           # 分支条件比较
├── RegFile.sv       # 32 个通用寄存器（x0 为常零）
├── DataPkg.sv       # Store 数据按字节 lane 打包
├── DataTrunc.sv     # Load 数据截取与符号/零扩展
└── MaskGen.sv       # Store 字节写掩码生成
```

## 模块说明

### `Core`

`Core` 是处理器顶层，连接控制器、寄存器堆、ALU、比较器和访存数据处理模块，并输出用于协同仿真的 `CoreInfo`：PC、指令、源/目的寄存器、ALU 结果、访存信息、分支结果和下一 PC。

其主要接口如下：

| 信号 | 方向 | 说明 |
| --- | --- | --- |
| `clk`、`rst` | 输入 | 时钟和高电平有效异步复位 |
| `imem_ift` | `Mem_ift.Master` | 指令存储器请求/响应接口 |
| `dmem_ift` | `Mem_ift.Master` | 数据存储器读写接口 |
| `cosim_valid` | 输出 | 复位释放后有效 |
| `cosim_core_info` | 输出 | 当前周期的协同仿真信息 |

### `Controller`

根据指令生成以下控制信号：

| 信号 | 作用 |
| --- | --- |
| `we_reg` | 寄存器写使能 |
| `we_mem`、`re_mem` | 数据存储器写/读请求有效 |
| `npc_sel` | 分支/跳转控制（主通路使用 `br_taken` 形成下一 PC） |
| `immgen_op` | I/S/B/U/UJ 立即数格式选择 |
| `alu_op` | ALU 运算选择 |
| `cmp_op` | 分支比较类型 |
| `alu_asel`、`alu_bsel` | ALU A/B 输入选择（寄存器、PC、立即数或零） |
| `wb_sel` | ALU、内存或 `PC+4` 写回选择 |
| `mem_op` | Byte/Half/Word/Double Word 及无符号 Load 类型 |

### `ALU` 与 `Cmp`

`ALU` 支持 64 位整数运算和 RV64I 的 32 位 `W` 运算。`ADDW`、`SUBW`、`SLLW`、`SRLW`、`SRAW` 先在低 32 位计算，再按指令语义进行符号扩展或零扩展。`Cmp` 支持有符号/无符号的等于、不等和大小比较。

### 访存数据处理

`DataPkg`、`DataTrunc` 和 `MaskGen` 使用地址低三位选择 64 位存储器字中的字节 lane：

- Store 数据放置在 `dmem_waddr[2:0]` 指定的 byte lane。
- 写掩码宽度为 8 bit，分别对应 8 个字节；`SB/SH/SW/SD` 产生 1/2/4/8 个连续的有效字节。
- `LB/LH/LW` 符号扩展，`LBU/LHU/LWU` 零扩展，`LD` 直接返回 64 位数据。

## 存储器接口依赖

仓库中的 `Core` 使用 `Mem_ift.Master`，但 `Mem_ift` 接口定义由外部课程框架提供；`include/mem_ift.vh` 仅包含辅助宏。因此，集成时需要确保编译器能够找到该接口及其请求/响应结构。`Core` 使用的字段为：

```text
imem_ift.r_request_valid
imem_ift.r_request_bits.raddr
imem_ift.r_reply_bits.rdata

dmem_ift.r_request_valid
dmem_ift.r_request_bits.raddr
dmem_ift.r_reply_bits.rdata
dmem_ift.w_request_valid
dmem_ift.w_request_bits.waddr / wdata / wmask
```

当前数据通路按单周期模型使用读响应，未实现等待状态或 back-pressure 处理。外部存储器应在课程框架规定的时序内返回指令和 Load 数据。

## 仿真与集成

本仓库没有独立 testbench、构建脚本或完整的 `Mem_ift` 接口实现，不能脱离课程框架直接运行。集成到已有工程时：

1. 将 `include/` 加入 Verilog/SystemVerilog include 路径。
2. 编译 `submit/*.sv`，并提供外部 `Mem_ift` 定义和存储器模型。
3. 按框架要求准备 `testcase.hex`。定义 `VERILATE` 时使用当前目录下的 `testcase.hex`；否则 `initial_mem.vh` 中的默认路径是示例 Windows 路径，需要按本机环境修改。
4. 通过 `cosim_core_info` 检查每周期执行结果，重点覆盖分支、跳转、不同 byte lane 的 byte/half/word 访存以及有符号 Load。

## 当前限制

- `MultiFSM` 仅保留为 bonus 接口，当前未实现多周期访存状态机、stall 或 ready/valid 控制。
- 未实现异常、CSR、特权级、缓存、流水线和中断机制。
- 访存地址和存储器响应时序依赖外部框架；本实现不负责处理存储器等待周期。
- `decoupled.vh` 和 `mem_ift.vh` 中的宏是接口拼装辅助代码，不是可独立实例化的存储器模型。

## 参考资料

- [RISC-V Unprivileged ISA Manual](https://github.com/riscv/riscv-isa-manual/releases/latest/download/riscv-unprivileged.pdf)
- [RISC-V International](https://riscv.org/technical/specifications/)
