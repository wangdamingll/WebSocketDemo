# WebSocketDemo

Linux 下的 **C++ WebSocket 服务端** 示例：基于 **epoll** 与 **非阻塞 I/O**，支持 **多进程 + `SO_REUSEPORT`** 扩展连接处理能力。完成握手后提供 **文本回显** 与 **同进程内广播**（多 worker 时广播不跨进程）。

## 环境要求

- Linux（需 `epoll`）
- CMake ≥ 2.6、GCC/G++（支持 C++11 即可）
- Python 3（仅用于自带测试脚本）
- 大量并发时建议：`ulimit -n` ≥ 65535（脚本会尝试调高）

## 编译

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

可执行文件路径：

```text
build/src/websocket_master/websocketserver
```

## 运行服务

默认监听 **`0.0.0.0:9000`**（见 `network_interface.h` 中的 `PORT`）。

### 单进程（默认，适合调试 / 需要全局广播）

```bash
./build/src/websocket_master/websocketserver
# 或显式
WS_WORKERS=1 ./build/src/websocket_master/websocketserver
```

### 多进程（`SO_REUSEPORT`，内核负载均衡新连接）

```bash
export WS_WORKERS=4
./build/src/websocket_master/websocketserver
```

或使用命令行（优先级高于环境变量）：

```bash
./build/src/websocket_master/websocketserver --workers=4
./build/src/websocket_master/websocketserver --workers=8
```

- **合法范围**：`WS_WORKERS` 为 **1～64**，非法值会被夹紧。
- **注意**：每个子进程有独立的客户端表；**广播只发给与本进程建立连接的客户端**。跨进程“全网广播”需要另行引入 Redis、消息队列等。

## 客户端联调

- 浏览器可使用 `src/websocket_master/test.html`，将其中地址改为 **`ws://<服务器IP>:9000`**。
- 任意支持 RFC 6455 的 WebSocket 客户端均可连接本端口。

## 自动化测试（推荐）

一键：**编译 + 多阶段严格测试**（多 worker 下的握手/变长帧/并行回显 + 单 worker 下的多客户端广播）。

```bash
chmod +x scripts/run_tests.sh scripts/ws_varlen_test.py  # 仅需一次
./scripts/run_tests.sh
```

测试阶段说明：

1. **多 worker**（默认 `WS_MP_WORKERS=4`）：单连接握手、**多长度帧**（顺序 + 单连接链式 + 并行多客户端 drain-until-echo）。
2. **单 worker**（`WS_WORKERS=1`）：**多客户端**负载与**全员广播**语义校验。

常用环境变量：

| 变量 | 含义 |
|------|------|
| `WS_MP_WORKERS` | 第一阶段 worker 数（默认 `4`） |
| `WS_LOAD_CLIENTS` | 第二阶段广播测试客户端数（默认 `100`） |
| `WS_TEST_HOST` / `WS_TEST_PORT` | 测试目标地址（默认 `127.0.0.1:9000`） |
| `WS_VARLEN_MAX` | 变长测试最大 payload 字节（默认约 768KiB） |
| `WS_VARLEN_CONCURRENCY` | 变长顺序阶段并发（默认 `1`，避免广播乱序） |
| `WS_VARLEN_PAR_CONC` | 变长并行阶段并发（默认 `24`） |

单独跑子测试示例：

```bash
# 服务需已启动
python3 scripts/ws_selftest.py
WS_LOAD_CLIENTS=200 python3 scripts/ws_loadtest.py
python3 scripts/ws_varlen_test.py
```

## 项目结构（核心）

```text
src/websocket_master/   # WebSocket 服务：main、epoll、握手、帧处理
src/common/           # 静态库 common（JSON、MongoDB 等，本 demo 主要链接调试/工具）
scripts/              # ws_selftest.py、ws_loadtest.py、ws_varlen_test.py、run_tests.sh
```

## 行为摘要

- **文本帧（opcode 1）**：回显给发送方，并向**本进程**其他已连接客户端广播同一条二进制帧。
- **Ping**：回复 Pong（同连接）。
- **Close**：断开并清理注册信息。

更多实现细节见源码顶部与各头文件中的模块注释。
