# Mosaico MiniHub

这是跑在 **ESP-Mosaico**（ESP32-S31，480×480 触屏）上的家庭中枢。屏自己连家里 Wi-Fi，本机就是 MiniHub：添加灯、开关、插座，一键场景，条件自动化。不需要再配一台 CoreBoard。

界面默认 **English**，首次开机可选 **中文**。下文按中文界面写，英文标签写在括号里。

---

## 你会用到的东西

| | |
|---|---|
| 设备 | ESP-Mosaico 一块，USB 供电 |
| 网络 | 2.4 GHz Wi-Fi（屏只做 STA，不发热点） |
| 灯 / 开关 / 插座 | Matter（Wi-Fi）、Zigbee，或 MQTT / Modbus TCP |
| 电脑（可选） | 同一 Wi-Fi 下用浏览器打开屏的 IP，进完整控制台 |

当前固件是 **Zigbee 版**：可以同时用 Wi-Fi + Zigbee。Thread 是另一套固件，不能和 Zigbee 装在同一镜像里。

---

## 第一次开机

顺序固定，配完才会进首页。

1. **欢迎页**
   约 3 秒后自动进入语言页。
2. **语言 Language**
   默认选中 English（标 Default）。点 **中文** 再 **继续 Continue**。之后所有界面跟这次选择走，设置里还能改。
3. **Wi-Fi**
   等列表刷出附近网络，点 SSID。加密网络会弹出键盘；连上显示 **已连接 Connected**。密码错会停在密码页，不会偷偷跳走。
4. **选择模式 Choose mode**
   - **Hub**：在这台 Mosaico 上运行 MiniHub。日常用这个。
   - **Panel**：当遥控器，去连另一台已经在跑的 MiniHub。接着填对方地址（例如 `192.168.1.103`），需要时再填 Bearer Token。

配过一次后，重启会记住 Wi-Fi 和模式，直接进首页。要重新走向导，只能清设备上的配网数据（见文末）。

---

## 首页：四个页签

底栏从左到右：**设备** / **场景** / **自动化** / **设置**。

### 设备 Devices

- 每页最多 4 张卡片，左右滑翻页。
- 空白圆加号 = **添加设备**。
- 灯 / 开关 / 插座：点卡片进详情。底部大按钮开关；离线时按钮变成 **重连设备**，点下去会告诉你连上了还是没有。
- 传感器只显示状态，不能开关。
- 设备名里如果有字库没有的汉字，会显示成 `设备 · <id>`，避免出现方块字。

点灯开关后，灯应该马上变。若卡片显示 **离线**，先重连，不要反复猛点。Matter 灯偶发离线是协议保活超时，不是你没点到。

### 添加设备

加号 → **选择设备类型**。只列出当前固件真正能用的协议。

| 类型 | 你要准备的 | 说明 |
|---|---|---|
| **Matter** | 11 或 21 位配对码，或 `MT:` 开头的二维码内容；名称；房间可选 | 码不对会立刻报错，不会空转「正在发现」。官方配网看门狗约 5 分钟。Matter over Wi-Fi 可以和本机 Zigbee 一起用。 |
| **Zigbee** | 让设备进入配网；IEEE / 安装码可空；名称；房间可选 | 需要 Zigbee 版固件。 |
| **BLE Mesh** | 名称；房间可选 | 屏已经占用 Wi-Fi + 802.15.4 时，**不会出现** 这一项。即使出现，和 Matter 的 BLE 配网会抢同一套蓝牙，不可靠。 |
| **MQTT** | Broker、端口、Point ID、类型、主题等 | 超时未上线会自动删掉这条。 |
| **Modbus TCP** | 网关、端口、从站、寄存器等 | 同上。 |

「123」这种无效 Matter 码应当立刻失败。发现失败就返回，不会在首页留一张 PAIR 卡片。

**射频规则（2.4 GHz 最多两路同时）：**

```
Wi-Fi  + Zigbee      ✅ 这版默认
Wi-Fi  + Matter-Wi-Fi ✅ Matter 灯走家里 Wi-Fi，不另占 802.15.4
Wi-Fi  + BLE Mesh    ❌ 已经占了 Wi-Fi 和 Zigbee，Mesh 让路
Matter BLE 配网 + BLE Mesh  ❌ 共用一个蓝牙主机，会打架
```

屏占着 Wi-Fi **不等于** 不能加 Wi-Fi Matter 设备。不能加的是再开一路 BLE Mesh。

### 场景 Scenes

场景 = **一组设备的开/关快照**，轻触执行一次。不是「房间」，也不是自动化。

1. 点 **创建**。
2. 给场景里的设备选 **跳过 / 关 / 开**。至少选一个动作。
3. 没有第二台设备时，**添加设备** 入口仍在，点了会提示 **无其它设备**。
4. 保存后，列表上轻触执行；右侧删除。

官方 MiniHub 就是这个模型：场景不负责「如果…那么…」。那是自动化。

### 自动化 Automation

自动化 = MiniHub 的 **规则**：某个设备变成开或关之后，去开/关另一台。

- 至少两台可控制设备（灯 / 开关 / 插座）才能创建。
- 第一步选触发设备及其当前状态，第二步选目标设备和要变成的开或关。
- 列表上轻触启用或停用，右侧删除。
- 更复杂的条件、多步、Store，去电脑上的 Web 控制台。

机载体感（IMU）**这版未启用**：晃动会把规则执行打进 Matter 加密栈并重启，判定也不够稳。

### 设置 Settings

- **Wi-Fi**：换网络。
- **模式**：Hub ↔ Panel。
- **语言**：English / 中文。
- **重启 MiniHub**：设备会短暂离线。系统更新走 Mosaico Vibe Mode，不在这个按钮里。

---

## 电脑上的控制台

Hub 模式下，屏和电脑连同一 Wi-Fi 后，浏览器打开：

```text
http://<屏的 IP>/
```

这是完整 MiniHub Web：设备目录、规则、场景、适配器。屏上没有的高级编辑都在这里。Web 里的 **规则** 就是屏上的 **自动化**。

首次打开控制台会使用官方 MiniHub 登录页设置 admin 密码。屏内本机控制使用
首次启动时生成的设备独立 token；token 只保存在 NVS，不会编译进固件或显示在
日志中。控制台文件在本机存储里；若页面提示未安装，需要重新写入工厂
`storage` 分区（会清掉已添加的设备）。

---

## 常见问题

**灯显示开了但没亮**
先看卡片是不是离线。在线仍不动：详情里重连一次。Matter 灯刚配上网有时要等 CASE 会话起来。

**设备时不时离线**
Matter 保活超时。点 **重连设备**，等结果，不要连点。物理灯要通电、和屏在同一 2.4 GHz 网上。

**场景点了没反应**
场景必须包含至少一台在线设备的开或关。只有名字、全是「跳过」等于什么都不做。

**为什么场景不能只绑一台灯**
可以绑一台，只是没意义。入口仍在，方便以后加第二台。

**关掉电脑 / 拔 USB 后设备没了**
正常运行时设备写在屏的 LittleFS 里，不断 USB 电、不整片擦除就不会丢。整包 `system-update` 如果带了工厂 `storage.bin`，会把已配设备清掉。日常应用更新不要带这块。

**BLE Mesh 为什么没有**
这版 Zigbee 固件已经占用 Wi-Fi + 802.15.4。要 Mesh 就得关掉其中一路，当前产品选择保留 Zigbee。

**中文变成问号**
动态名字超出附带字库。改成字库里有的字，或接受 `设备 · <id>`。

---

## 开发与预览

日常用户不用看这一节。

工程直接编译官方 MiniHub 与 ESP-Matter 源码，不复制它们的业务实现。目录需要是：

```text
<workspace>/esp-mosaico-vibe/projects/minihub_panel
<workspace>/esp-minihub
<workspace>/esp-matter
```

MiniHub JS 组件直接使用 `<workspace>/esp-minihub/components/minihub_js`；
只有 Thread 变体对官方 `minihub_thread` 做构建清单适配，使它与 ESP-Iris
0.2.0 共用 `espressif/mdns` 1.12.0。

首次构建先按当前 ESP-Matter checkout 初始化 GN/Pigweed，之后每个构建 shell
都需导出 Matter 环境：

```sh
cd <workspace>/esp-matter/connectedhomeip/connectedhomeip
source scripts/bootstrap.sh -p all,esp32
source ../../export.sh
```

```sh
python mosaico.py project sim --project projects/minihub_panel --interactive
python mosaico.py recover
python mosaico.py iris system-update --project projects/minihub_panel
```

空白或未校验的板先 `recover`。改了 UI / 分区 / 资源用 `system-update`；只改 C 且分区表一致用 `app-update`。不要 `idf.py flash`。

工程结构：`main` 只做组装；`app_hub` 管 Wi-Fi 和 MiniHub HTTP；`app_panel` 管 480×480 界面；`app_display` 接 BSP。组件约定见各自 `README.md`。
