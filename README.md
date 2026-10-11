# 南开大学 2024 级 操作系统实验作业

本仓库用于存放本组在《操作系统》课程实验（riscv64-ucore，lab2026 指导书）中的实验代码与实验报告。

## 小组成员

| 学号 | 姓名 |
| --- | --- |
| 2412362 | 雷帛霖 |
| 2412422 | 贾钧涵 |

## 目录结构

```
.
├── lab1/                 # 实验一：比麻雀更小的麻雀（最小可执行内核）
│   ├── Makefile
│   ├── kern/             # 内核源码（init、libs、driver、mm）
│   ├── libs/             # 基础库（sbi、printfmt、string、riscv 等）
│   ├── tools/            # 链接脚本 kernel.ld 与构建脚本 function.mk
│   └── 实验报告.md
└── README.md
```

后续实验将以 `lab2/`、`lab3/` … 的形式依次添加。

## 实验环境

| 项目 | 版本 / 说明 |
| --- | --- |
| 宿主系统 | Windows 11 + WSL2 |
| Linux 发行版 | Ubuntu 24.04.4 LTS（内核 6.6.87.2-microsoft-standard-WSL2） |
| 交叉编译器 | `riscv64-unknown-elf-gcc` 13.2.0 |
| 模拟器 | QEMU 8.2.2（`qemu-system-riscv64`，自带 OpenSBI v1.3） |
| 调试器 | GDB 15.1（`gdb-multiarch`，软链为 `riscv64-unknown-elf-gdb`） |
| 终端复用 | tmux（左右分屏分别运行 QEMU 与 GDB） |

## 构建与运行

```bash
cd lab1
make            # 交叉编译，生成 bin/kernel（ELF）与 bin/ucore.img（裸镜像）
make qemu       # 在 QEMU 中运行内核；退出方式为 Ctrl+a，再单独按 x
```

GDB 远程调试（需要两个终端，推荐用 tmux 左右分屏）：

```bash
make debug      # 左窗格：QEMU 以 -S -s 启动，CPU 暂停并开放 1234 端口
make gdb        # 右窗格：GDB 加载符号并连接 localhost:1234
```

> **与指导书的差异说明：** 指导书中 `qemu` 目标使用
> `-device loader,file=$(UCOREIMG),addr=0x80200000` 启动，该写法对应旧版
> OpenSBI（`fw_jump`，固定跳转 0x80200000）。本机 QEMU 8.2.2 自带的是
> OpenSBI v1.3（`fw_dynamic`），若仍用 `-device loader`，`Domain0 Next Address`
> 会是 `0x0`，内核无任何输出并静默挂死。因此本仓库将 `qemu` 与 `debug` 两个目标
> 改用 `-kernel $(UCOREIMG)`，其余参数未变。

## 实验报告

每次实验的报告（Markdown 格式）与该次实验的代码放在同一目录下，例如 `lab1/实验报告.md`。
