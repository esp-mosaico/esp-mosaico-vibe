# FAQ / Troubleshooting（索引）

> **能力边界：** 仅供本 Skill 已启动的全新安装流程进行有限恢复；不得用于响应独立排障请求，也不得用于修复、迁移或切换已有环境。

> **用法：** 先按失败类型打开对应专题文件，再按题号处理。
> **子模块失败 / `git status` 不干净（最常见）→ 立刻读 [troubleshooting-git.md](troubleshooting-git.md) § Q2**（先镜像，再按 status 路径定点重拉；禁止跳过、禁止只瞎重跑一整遍）。

---

## 按类型分流

| 失败类型 | 文件 | 题号 |
|----------|------|------|
| `git config --global` Permission denied / 写不了 `.gitconfig` | [troubleshooting-git.md](troubleshooting-git.md) | **Q0** |
| clone 超时 / 404 / 离线包（**父仓**） | [troubleshooting-git.md](troubleshooting-git.md) | Q1 |
| submodule 拉失败 / 中断 / wifi lib·openthread 超时 / status dirty | [troubleshooting-git.md](troubleshooting-git.md) | **Q2**（tag 仍失败 → Q2 方案四离线包） |
| 子模块目录为空（尚未跑第三步） | [troubleshooting-git.md](troubleshooting-git.md) | Q3 |
| 工具链 install 失败 / CDN / checksum | [troubleshooting-toolchain.md](troubleshooting-toolchain.md) | Q4 |
| `IDF_PATH is not set` / export 未生效 | [troubleshooting-toolchain.md](troubleshooting-toolchain.md) | Q6, Q12, Q12b |
| macOS SSL / Apple Silicon Rosetta | [troubleshooting-toolchain.md](troubleshooting-toolchain.md) | Q8, Q10 |
| Windows 环境变量陷阱 / 绿盾 version unknown | [troubleshooting-toolchain.md](troubleshooting-toolchain.md) | Q11, Q15 |
| build 无 bin / set-target（unknown / 需 `--preview`） | [troubleshooting-build.md](troubleshooting-build.md) | Q5, Q7 |
| Windows 路径过长 / 编译慢 | [troubleshooting-build.md](troubleshooting-build.md) | Q13, Q14 |
| AT `factory/` 空、install 拉 IDF 超时、`esp_idf_monitor` 缺包 | [troubleshooting-at-adf.md](troubleshooting-at-adf.md) | Q-AT1, Q-AT2, Q-AT4 |
| ADF `ADF_PATH`、master 无内置 IDF、submodule 超时 | [troubleshooting-at-adf.md](troubleshooting-at-adf.md) | Q-ADF1～Q-ADF3 |

---

## 快速入口（Agent）

```
git config Permission denied / .gitconfig     → troubleshooting-git.md Q0
git clone 父仓失败 / 极狐残留      → troubleshooting-git.md Q1
submodule / wifi lib / openthread  → troubleshooting-git.md Q2（强制；勿走 Q1）
install.sh / install.bat 失败        → troubleshooting-toolchain.md Q4
export 后找不到 idf.py / 工具链      → troubleshooting-toolchain.md Q12 / Q12b
idf.py build 成功但无 bin            → troubleshooting-build.md Q5
idf.py set-target 失败 / 需 --preview → troubleshooting-build.md Q7
AT build/factory 空                  → troubleshooting-at-adf.md Q-AT1
AT No module named esp_idf_monitor → troubleshooting-at-adf.md Q-AT4（补 install-python-env，勿只 export）
ADF 找不到组件                       → troubleshooting-at-adf.md Q-ADF1
```

镜像命令见 [mirrors.md](mirrors.md)；网络决策见 [common.md](common.md) § 第二步。
