# fx-CG50 小游戏插件:2048 + 扫雷

为卡西欧 fx-CG50(Prizm)交叉编译的两个经典小游戏 Add-in。

## 产物(output/)

| 文件 | 说明 |
|---|---|
| `2048-CG50.g3a` | 2048 数字合并游戏(仅 fx-CG50,VRAM 地址 0xAC000000) |
| `Minesweeper-CG50.g3a` | 扫雷游戏(fx-CG10/20/50 通用) |

## 源码来源与许可证

两个项目均来自 Cemetech Prizm 社区的开源示例(GitHub 仓库),已作为"示例"参考下载并适配:

1. **2048** — [wjtje/2048-fx-cg-10-20-50](https://github.com/wjtje/2048-fx-cg-10-20-50)
   - Cemetech 下载页:https://www.cemetech.net/downloads/files/1912/x1912
   - 许可证:**GPL v3 / AGPL v3**(见项目 LICENSE 与源文件头注释)
2. **扫雷** — [undefined06855/casio-minesweeper](https://github.com/undefined06855/casio-minesweeper)
   - Cemetech 同站亦有多个扫雷示例(如 1836/x1836)
   - **注意:该仓库未声明许可证**,仅供个人学习/编辑使用;若需再分发请先联系作者确认。

## 针对 PrizmSDK-win-0.6 的适配修改(build/ 副本)

上游代码面向旧版 PrizmSDK(`common/prizm_rules`),新版 SDK 目录改为 `toolchain/`,故在构建副本中做了以下最小改动:

- `build/2048/Makefile`
  - `common/prizm_rules` → `toolchain/prizm_rules`
  - `common/prizm.ld` → `toolchain/prizm.x`(链接脚本路径)
  - `$(BUILD):` 规则中的 `[ -d $@ ] || mkdir $@` → `-mkdir $@`(Windows 无 `[` 命令)
- `build/2048/src/`
  - 新增占位 `color.h`(新版 libfxcg 已把 `color_t` 移入 `<fxcg/display.h>`)
  - `sprite.h` 从项目根复制到 `src/`,`main.c` 中 `#include <sprite.h>`/`<color.h>` 改为相对引用

## 真机反馈修复(2026-09-09)

### 0. 默认及最大棋盘尺寸:16x8

- 扫雷默认棋盘设为 **16x8**;
- 宽度上限为 **16**、高度上限为 **8**,设置界面输入更大的数字时会自动限制到上限;
- `Board_create()` 也执行同样限制,防止其他调用路径创建越过屏幕范围的棋盘。

### 小棋盘稳定性、AC Break 与 Personal Best

- 首次翻开不再通过无限重生成来寻找零邻雷格;改为有限候选位置洗牌,空间足够时保证首格及周围无雷,密集小棋盘则至少保证首格安全;
- `AC/ON` 和 `EXIT` 都可立即中止当前游戏并返回设置页,退出时会安全卸载计时器;
- 每个 **宽度 x 高度 x 雷数** 组合分别记录 Personal Best,保存在计算器存储根目录的 `MINESPB.DAT` 中;
- 设置页显示当前组合的历史最佳时间,刷新纪录时胜利提示会显示 `NEW PB!`。
- Add-in 在计算器主菜单中显示为 **Mine**,避免名称过长溢出。

### 1. 扫雷方向键导致整个界面移动(已修复)

- **根因**:`board.c` 的 `Board_updateOffset()` 用 `height*24+24 > LCD_HEIGHT_PX` 判断是否滚动,默认 9×9 棋盘高 216px(恰等于屏幕高 216px)被判为"超屏";再加 `pad=80` 的大边距和负的初始 `offsetY`,每次按方向键 `offsetY` 大幅漂移 → 整个棋盘移动。
- **修复**:改为仅当棋盘**实际超过屏幕**(`width*24 > LCD_WIDTH_PX` / `height*24 > LCD_HEIGHT_PX`)时才滚动;滚动以整格(24px)为单位,且保证光标所在格完全可见。9×9 默认棋盘从此不再滚动,光标正常移动。
- 大棋盘(如自定义 12×12)滚动行为保留,且更平滑。

### 2. fx-CG50 官方风格图标

- 两个游戏的 `selected.bmp` / `unselected.bmp` 均为 **92×64、24-bit RGB、未压缩 BMP**。
- 图标采用白色外层画布、约 `x=5..86,y=5..42` 的彩色圆角磁贴，以及下方留给系统名称的空白区域。
- Mine 使用绿色磁贴和白色雷球主体;2048 使用橙红磁贴和白色数字合并符号,图像内不绘制插件名称。
- 选中状态保持主体位置和轮廓不变,使用官方风格的青蓝色圆角底板;底板四角保留白色,不改变主体位置。
- `make_icons.ps1` 会同步生成 `build/` 与 `src/` 中两款游戏的四组图标,可执行以下命令重新生成:
  `powershell -ExecutionPolicy Bypass -File make_icons.ps1`

### 3. 按键习惯适配:Shift=翻开,Alpha=插旗

- `Board_handleKeypress()` 新增:**Shift(KEY_PRGM_SHIFT)翻开格子**,**Alpha(KEY_PRGM_ALPHA)插旗**,原 F6(翻开)/F1(插旗)保留,两套并存;
- 开局 5 秒的状态栏提示更新为 `F1/ALPHA Flag | F6/SHIFT Reveal`。

## 2048 重构(按官方规则, 2026-09-28)

参考官方实现 [gabrielecirulli/2048](https://github.com/gabrielecirulli/2048)(`js/game_manager.js`)完整重写了 `build/2048/src/main.c`。

### 规则修正
- 一次按键 = 一次移动:方块滑到底,相邻同数合并一次;**合并产生的新方块在同一次移动内不再参与合并**(旧版按 4 遍逐格挪动,会连锁合并,如 [2,2,2,2] 左移得到 4,2 而非 4,4,还会重复加分);
- 只有棋盘发生变化才生成新方块,概率与官方一致:90% 出 2、10% 出 4(旧版无效移动也会生成);
- 得分 = 每次合并后方块的数值;
- 合成 **2048** 显示 YOU WIN 提示,再按方向键继续(可继续冲 4096+)(旧版没有胜利判定);
- **无空格且无可合并的相邻方块**才算 GAME OVER(旧版棋盘满即判负,提前终止游戏);
- 无效移动(该方向上无可动、无可合)不产生任何效果。

### 界面与操作
- 采用官方 2048 配色的圆角棋盘,右侧为 Casio 2048 标志、SCORE/BEST 计分框和按键说明;
- 数字用自绘 3x5 点阵字体按位数缩放居中,替换了坐标行为不直观的 PrintMini;
- `方向键` 移动;`F1` **撤回上一步**(单步,可救回刚输掉的局);`F2` **设置页**;`SHIFT` 新游戏;`EXIT` 存档并退出。
- 2026-09-29 界面优化:**新增 VIVID 高对比配色并设为默认**(官方配色的 2 和 4 在小屏上难以区分);全局小字加大一倍(5px→10px),标题加大到 15px。
- **设置页(`F2`)**:配色(VIVID/CLASSIC)、按住方向键连发(开/关)、自动存档(停顿时写/仅退出时写)、SHIFT 新局二次确认(开/关);设置随存档文件持久化,断电保留。

### 存档
- 存档保存在计算器存储根目录 `G2048.DAT`(棋盘、分数、胜利状态);写入时机:**停止操作约 2 秒后自动写**、`EXIT`/`MENU` 退出必写、胜利/终局/新开局立即写,游戏中按键响应不再有写盘卡顿;
- 重新启动 Add-in 自动恢复上局;游戏结束后按官方行为清空局内存档,**最高分跨局保留**;
- 存档带魔数和版本号,大小/内容损坏时自动按新游戏处理;
- **2026-09-29 修复存档卡顿**:旧版每步写盘导致每次移动后转圈、频繁 Bfile 重试有死机风险(Bfile 写类调用会触发闪存存储器优化,参见 WikiPrizm《Incompatibility between Bfile Syscalls and Timers》);现改为上述时机写入,写失败当轮即降级、不再重试(与扫雷 PersonalBest 同策略),恢复存档时若棋盘已无可走步直接判 GAME OVER。
- **2026-09-29 修复读档停顿**:启动读档改为**先画界面再读档**(读档期间显示 RESTORING,不再黑屏死等),并把启动路径的 Bfile 调用从 4 个减到 3 个。若读档仍然很慢,请在计算器执行一次「存储器(Memory)→ 优化」还清文件系统欠账,通常即可恢复。
- **2026-09-29 修复按键无反应 + 新增撤回**:输入层原用 `GetKeyWait_OS`(未在本机型验证),在文件系统操作之后会卡死(表现为停在 `STEP 3 DONE`、所有按键无反应、画面冻结);现改用扫雷同款的非阻塞 `PRGM_GetKey` 轮询 + 每次重画后显式 `Bdisp_PutDisp_DD` 推屏。同时新增 **`F1` 单步撤回**(与官方一致,可撤回刚输掉的那一步)。注意:原始矩阵扫描没有系统连发,按住方向键每次只走一步(可选的连发见设置页)。

## 构建环境与一键编译

- 工具链:**PrizmSDK-win-0.6**(内含 sh3eb-elf-gcc 10.1.0、GNU make 4.3、mkg3a),已解压于 `PrizmSDK/PrizmSDK-win-0.6`
- 一键脚本(需设置环境变量 `FXCGSDK` 指向 SDK 根目录):

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

- 手动构建示例:

```powershell
$env:FXCGSDK = "D:\...\PrizmSDK\PrizmSDK-win-0.6"
$env:PATH = "$env:FXCGSDK\bin;$env:PATH"
# 2048(目标名 build,旧式 Makefile)
cd build\2048; make build
# 扫雷
cd build\minesweeper; make all
```

## 安装到计算器

1. 用 USB 数据线连接 fx-CG50 与电脑;
2. 计算器弹出"USB 闪存"模式(确认连接)后,在电脑上把 `.g3a` 文件拖入计算器的存储根目录;
3. 断开连接,在计算器主菜单(Main Menu)即可看到新 Add-in 图标。

## 游戏操作

- **2048**:方向键移动数字方块,相邻同数合并;`F1` 撤回上一步(单步),`F2` 打开设置页;`SHIFT` 新游戏,`EXIT` 存档退出。自动存档(停顿约 2 秒或退出时写入)、记录最高分,重启后自动继续上局(详见上方"2048 重构")。
- **扫雷**:方向键移动光标;`Alpha` 或 `F1` 插旗标记;`Shift` 或 `F6` 翻开格子。再次翻开已显示的数字格时,若周围旗帜数量匹配数字则自动翻开其余相邻格;否则短暂高亮可能有雷的相邻格。避开地雷翻开全部安全格即胜利。

## 注意事项

- **真机验证**:本项目已在本机交叉编译并核验 g3a 头部信息,但未在真机/模拟器上跑通;请先在计算器上试运行。免费模拟器 casio-emu 不支持所有 g3a 特性,建议以真机为准。
- 2048 的 `sprite.h` 将 VRAM 地址写死为 `0xAC000000`(fx-CG50),故 `2048-CG50.g3a` 仅适用于 CG50;扫雷基于 Bdisp 系列 API,通用于 CG10/20/50。
