# 仿真预览连接排查

[English](simulator-preview.md) | [返回索引](README_CN.md)

先启动项目仿真，记录服务实际监听的地址和端口。浏览器与服务在同一台机器时使用
`127.0.0.1`；通过 SSH 开发时，远端的 `localhost` 不等于用户电脑的 `localhost`。
先使用已有端口转发，必要时在用户电脑运行下列命令，替换大写占位符并保持终端运行：

```sh
ssh -N -o ExitOnForwardFailure=yes -L 127.0.0.1:LOCAL_PORT:127.0.0.1:REMOTE_PORT USER@HOST
```

浏览器打开 `http://127.0.0.1:LOCAL_PORT/`。沿用已有的 SSH 别名、跳板和端口配置。
若使用双方可达的远端私有 IP，服务也需监听相应网卡；`0.0.0.0` 是监听地址，
浏览器应使用实际主机地址。无需为预览关闭防火墙或开放公网。

页面打不开时，先检查远端服务，再在用户电脑检查转发端口及 API：

```powershell
# Windows PowerShell；将 LOCAL_PORT 替换为实际数值。
Test-NetConnection 127.0.0.1 -Port LOCAL_PORT
curl.exe --noproxy "*" --max-time 5 http://127.0.0.1:LOCAL_PORT/api/v1/state
```

```sh
# macOS / Linux
nc -vz 127.0.0.1 LOCAL_PORT
curl --noproxy '*' --max-time 5 http://127.0.0.1:LOCAL_PORT/api/v1/state
```

连接被拒绝时检查 SSH 进程和本地监听；端口可连接但 HTTP 失败时检查 SSH 日志与
远端服务端口。若内置浏览器改写链接，对比最终网址和实际转发映射。
API 能返回游戏身份后，再验证目标浏览器的画面、点击和键盘操作。

页面显示但点击无响应时，查看 JS 错误、输入请求和游戏状态变化。输入实现应支持
变化的 pointerId、按实际图像区域映射坐标，并保持按下/移动/释放请求的顺序。
这些行为由代码和测试保证；排障时不必重新执行整套检查。
