# DontStarveLuaJIT 安装与开发指南

本文面向使用者和贡献者，覆盖 DST 安装、源码编译、普通 Mod 开发，以及本项目 Lua/native Plugin 的扩展。本文以仓库当前实现为准；插件系统的完整生命周期、依赖和配置规则见 [Plugin 系统说明](plugin-system.md)。

## 目录

- [项目简介](#项目简介)
- [安装与卸载](#安装与卸载)
- [开发环境与编译](#开发环境与编译)
- [Mod 开发](#mod-开发)
- [Plugin 开发](#plugin-开发)
- [测试与调试](#测试与调试)
- [常见问题](#常见问题)

## 项目简介

DontStarveLuaJIT 通过游戏进程注入，将 DST 使用的 Lua 5.1 VM 替换为 LuaJIT，并提供可选的渲染、网络、存档和性能分析功能。仓库主要部分：

| 路径 | 内容 |
| --- | --- |
| `src/DontStarveInjector/` | Injector、原生 Plugin Host、动态插件和注入壳 |
| `src/lua51/`、`src/lua51original/` | Lua 5.1 兼容库及配置/工具使用的原版 VM |
| `luajit/` | LuaJIT 子模块 |
| `Mod/` | 游戏 Mod 源码、Lua 插件和本地安装脚本 |
| `tests/` | C++/Lua/Python 测试及游戏集成测试 |
| `tools/` | Steam 路径、依赖准备、打包和测试辅助工具 |
| `docs/` | 架构、逆向分析及开发文档 |

游戏侧的“注入壳”和 Mod 内的“真实 Injector”是不同文件：Windows 只将 `Winmm.dll` 放到游戏 `bin64`；真实 `Injector.dll`、动态插件及运行时依赖位于 Mod 根目录、`plugins/` 和 `deps/`。不要把整套插件或真实 Injector 复制进游戏 `bin64`。

## 安装与卸载

### 普通用户安装

详细的一键安装、手动安装、平台差异和卸载步骤以仓库 [README 安装说明](../README.md) 为准。建议优先使用其中的一键安装脚本，或下载对应平台的 Release 包。

从源码构建后进行本地安装：

1. 将 `Mod/` 的**内容**放入 DST 的一个 Mod 目录，确保 `modmain.lua` 直接位于该目录下，而不是 `Mod/modmain.lua`。
2. 在 Mod 根目录运行 `install.bat`（Windows）或 `./install_linux.sh`（Linux）。Linux 首次运行可能需要 `chmod +x ./install_linux.sh`。
3. 在游戏 Mod 列表中启用 DontStarveLuaJit2。
4. 更新 Mod 包后再次运行安装脚本，避免游戏中的注入壳与 Mod 内 Injector 版本不匹配。

安装脚本负责部署薄注入壳、将真实 Injector 放在 Mod 根目录并写入 Injector 路径 marker。卸载时可在 Windows 运行 `install.bat uninstall`，Linux 运行 `./install_linux.sh uninstall`。手动安装和 macOS 注意事项请查阅 README。

### 安装验证与风险

- 前端版本号应出现 `(LuaJIT)` 一类后缀；控制台可执行 `print(jit)`，返回 table 通常表示 LuaJIT 已加载。
- 注入解析及模块加载日志：`<游戏目录>/data/unsafedata/ds_luajit_boot.log`。
- 备份存档。Native Plugin 可安装进程级 Hook，错误可能导致崩溃或影响游戏状态。
- `DisableJITWhenServer` 仅作用于专用服务器进程；客户端开服不等于专用服务器。

## 开发环境与编译

### 前置条件

- Windows x64：Visual Studio 2022（MSVC C++ 工具链）、CMake、Ninja、Git、Python 3。
- Linux/macOS：CMake、Ninja、Git、Python 3 和本机 C/C++ 编译工具链。
- 拉取仓库时初始化 `luajit`、`vcpkg` 和 `3rd/frida-gum-src` 子模块。
- vcpkg manifest 会由 CMake toolchain 管理第三方 C/C++ 依赖；首次配置需要网络并可能花费较长时间。

从 Git 克隆时：

```powershell
git clone --recurse-submodules https://github.com/fesily/DontStarveLuaJIT2.git
cd DontStarveLuaJIT2
```

已有工作副本则执行：

```powershell
git submodule update --init --recursive
```

### 配置并编译（Windows 示例）

`CMakePresets.json` 提供 `ninja-multi-vcpkg` 配置 preset 和 Debug、Release、RelWithDebInfo build presets。先运行仓库的路径初始化脚本，再配置和编译：

```powershell
.\dev_init.bat
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-vcpkg-release --parallel
```

`dev_init.bat` / `dev_init.sh` 调用 `tools/update_steam_paths.py`，生成本机的 `cmake/GameDir.cmake`。该文件属于自动生成的本地路径配置，不要手工编辑；配置前确认脚本找到了正确的 Don't Starve Together 安装目录。若 CMake 报告找不到游戏目录，先检查 Steam 安装路径并重新运行初始化脚本。

Linux/macOS 使用相同的 CMake preset 命令（Linux/macOS 对应脚本为 `./dev_init.sh`）。工具链、编译器或依赖准备失败时，先按报错补齐本机开发工具，再重新配置。

可选构建配置：

```powershell
cmake --build --preset ninja-vcpkg-debug --parallel
cmake --build --preset ninja-vcpkg-release-dbg --parallel
```

首次配置若提示 Frida-Gum shared stage 不存在，运行以下命令准备依赖并重新配置：

```powershell
python tools/setup_frida_gum.py
cmake --preset ninja-multi-vcpkg
```

Frida-Gum 的构建需要上游工具链，具体错误信息以 `tools/setup_frida_gum.py` 输出为准。ANGLE 相关插件只在 Windows 且对应依赖可用时构建。

### 安装构建产物

构建完成后可将依赖和模块安装到包目录：

```powershell
cmake --install builds/ninja-multi-vcpkg --config Release
```

CMake 项目的安装前缀设为仓库的 `Mod/`；安装结果包括 Mod 根目录下的 Injector、`Mod/plugins/` 下的动态插件和 `Mod/deps/` 运行时依赖。若 `GAME_DIR` 有效，CMake 还会将薄注入壳部署到该游戏的 `bin64`。这会修改本机游戏目录；只想编译而不想部署时，不要运行 install 步骤。给测试 Mod 安装可运行 `Mod/install.bat`。

### 运行测试

最小插件 Host 回归测试示例：

```powershell
ctest --test-dir builds/ninja-multi-vcpkg -C Release -R "plugin_host_graph|plugin_option_rules|plugin_host_lua" --output-on-failure
```

运行完整 CTest：

```powershell
ctest --test-dir builds/ninja-multi-vcpkg -C Release --output-on-failure
```

需要真实游戏目录、图形桌面或服务器集群的集成测试可能被跳过；对应测试的前置条件见 `tests/` 下 README。不要将跳过的集成测试误认为游戏运行验证通过。

## Mod 开发

DST Mod 的入口是 `modinfo.lua` 和 `modmain.lua`。本仓库自身 Mod 可作为参考：`Mod/modinfo.lua` 声明 Mod 信息和游戏内配置项，`Mod/modmain.lua` 是主入口。

### 最小结构

```text
my_mod/
  modinfo.lua
  modmain.lua
  scripts/
    hello.lua
```

`modinfo.lua` 要提供 DST 所需的名称、版本、`api_version` 和兼容标记；需要给玩家配置的项目写在 `configuration_options` 中。以下为结构示例，按 Mod 实际兼容平台调整：

```lua
name = "My Mod"
description = "A small DST mod."
author = "Your Name"
version = "1.0.0"
api_version = 10

dont_starve_compatible = false
reign_of_giants_compatible = false
dst_compatible = true
client_only_mod = true
server_only_mod = false
all_clients_require_mod = false
```

`modmain.lua` 中用 `modimport` 加载脚本：

```lua
modimport("scripts/hello")
```

`scripts/hello.lua`：

```lua
AddPlayerPostInit(function(inst)
    if not TheWorld.ismastersim then
        return
    end

    inst:DoTaskInTime(0, function()
        print("[my_mod] player initialized:", inst.prefab)
    end)
end)
```

普通 Mod 的服务端/客户端代码应按 DST 生命周期放入相应回调；避免在顶层无条件访问只在游戏运行阶段存在的实体或网络对象。需要 LuaJIT 特定行为时，先考虑 VM 切换、加密 Mod/栈深兼容和专用服务器差异，再在真实客户端及专用服务器分别验证。

### LuaJIT 插件包 Mod

若要作为**外部 LuaJIT Plugin 包**向本项目注册功能，而不是普通 DST Mod，在外部 Mod 的根 `modinfo.lua` 增加：

```lua
luajit_plugin_pack = true
plugin_id = "example.my_feature"
```

`plugin_id` 必须是非空字符串；用户需要启用该 Mod 才会被发现。可选的 Lua 包面位于 `plugins/plugin_<name>/`，其中可包含 `modinfo.lua`、`modmain.lua`、`scripts/`；原生动态模块使用 `plugins/plugin_<name>/plugin_<name>.dll`（Linux/macOS 使用相应平台库扩展名）。外部包通过启用 Mod 的授权提示，不能绕过用户启用状态。完整规则见 [Plugin 系统说明：外部插件包](plugin-system.md)。

## Plugin 开发

本仓库有两种不同的 Plugin 扩展方式：

1. **Lua Plugin**：通过 Mod 内显式注册表加入 `Mod/plugins/init.lua`，在 `AfterModMain` 阶段运行，不需要编译 C++。
2. **Native Plugin**：编译成 `plugin_<name>` 动态模块，由 Injector 在 `EarlyNative` 阶段发现和加载。需要使用游戏原生 Hook 或导出 API 时才选择此方式。

它们可构成共享一个 `id` 的双面 Plugin：Native 部分负责早期原生初始化，Lua 包负责 Mod 配置及游戏脚本交互。插件管理、依赖顺序、选项 Gate、重载语义和 Host 状态见 [Plugin 系统说明](plugin-system.md)。

### 示例 A：Lua-only Plugin

新增 `Mod/plugins/hello.lua`，每个文件返回一个 Plugin 表：

```lua
return {
    id = "example.hello",
    version = "1.0.0",
    phases = "AfterModMain",
    priority = 80,
    depends = {},
    soft_depends = {},
    conflicts = {},
    support_reload = false,
    when = function(ctx)
        return ctx ~= nil and ctx.has_luajit
    end,
    load = function(ctx)
        if ctx ~= nil and type(ctx.config) == "function"
            and ctx.config("EnableHello") == true then
            print("[example.hello] enabled")
        end
    end,
    unload = function(_ctx)
    end,
}
```

在 `Mod/plugins/init.lua` 的显式返回列表中注册它：

```lua
return {
    -- 保留已有项目
    load_flat("hello"),
}
```

同时在 `Mod/modinfo.lua` 的 `configuration_options` 增加 `EnableHello` 配置项，才能从游戏 Mod 配置界面切换。`load_flat("hello")` 对应 `Mod/plugins/hello.lua`；它不会自动发现目录下的新文件。插件未列入 `init.lua` 就不会被 Host 加载。

### 示例 B：Native 动态 Plugin

最小 Native 样板可直接参考 `src/DontStarveInjector/plugins/plugin_dummy/`。新增 `src/DontStarveInjector/plugins/plugin_hello/`，其中 `plugin_hello.cpp` 实现 `IPlugin` 并导出模块入口：

```cpp
#include "core/PluginModuleAbi.hpp"
#include "core/PluginHost.hpp"
#include "core/PluginTypes.hpp"

namespace {
using namespace ds::plugin;

struct HelloPlugin final : IPlugin {
    PluginManifest info{};

    HelloPlugin() {
        info.id = "example.hello";
        info.version = "1.0.0";
        info.phases = PluginPhase::EarlyNative;
        info.options.kind = OptionRuleKind::AlwaysOn;
    }

    const PluginManifest &manifest() const override { return info; }
    bool can_load(const PluginContext &) const override { return true; }
    void load(PluginContext &) override {
        // 在此安装本 Plugin 的 Native 功能。
    }
    void unload(PluginContext &) override {}
};

HelloPlugin g_plugin;
}

DS_PLUGIN_MODULE_EXPORT const char *ds_plugin_module_abi_version() {
    return DS_PLUGIN_ABI_VERSION;
}

DS_PLUGIN_MODULE_EXPORT bool ds_plugin_module_init(ds::plugin::PluginHost *host) {
    if (host == nullptr) {
        return false;
    }
    host->register_plugin(&g_plugin);
    return true;
}
```

新增同目录 `CMakeLists.txt`：

```cmake
ds_add_dynamic_plugin(plugin_hello
    plugin_hello.cpp)
```

然后在 `src/DontStarveInjector/CMakeLists.txt` 的动态插件列表中添加：

```cmake
add_subdirectory(plugins/plugin_hello)
```

每个动态模块必须提供 `ds_plugin_module_init`，并应提供匹配 Host 主版本的 `ds_plugin_module_abi_version`。模块名使用 `plugin_` 前缀；插件对象应在模块存活期间保持有效（通常使用静态对象），且 `man.id` 在各面保持唯一、稳定。`load()` 中只执行本 Plugin 所有的初始化；不要把功能 Hook 重新塞回 Injector 主入口。

### 双面 Plugin 与配置

Native + Lua 双面功能优先使用 package 布局：

```text
src/DontStarveInjector/plugins/plugin_hello/
  CMakeLists.txt
  plugin_hello.cpp
  modinfo.lua
  modmain.lua
  scripts/
Mod/plugins/
  init.lua
```

`ds_add_dynamic_plugin` 会将同目录的 `modinfo.lua`、`modmain.lua` 和 `scripts/*.lua` 安装到 `Mod/plugins/plugin_hello/`。Lua 注册表中用 `load_package("plugin_hello")` 加载 package，而非 `load_flat`。原生 `PluginManifest.id` 与 package 的 `plugin_id` 一致。配置项应定义在 Mod/package `modinfo.lua`，Lua 侧在 `modmain.lua` 读取；新增双面功能时同步更新对应 option schema，并运行身份检查：

```powershell
python tools/check_plugin_package_identity.py --source-root .
```

现有功能包可参考 `src/DontStarveInjector/plugins/plugin_network_sim/`（Native + Lua + 配置）或 `src/DontStarveInjector/plugins/plugin_debug_profiler/`（复杂 Native 服务和 Lua 配置）。

## 测试与调试

- 原生插件新增依赖/选项时，补充 Host 选项规则和启用/禁用测试；运行 `plugin_host_graph`、`plugin_option_rules` 等相关 CTest。
- Lua Plugin 变更可参考 `tests/plugin/` 的 Lua Host 测试。
- 真实注入与专用服务器稳定性测试见 [plugin_server 测试说明](../tests/plugin_server/README.md)；客户端注入测试见 [plugin_client 测试说明](../tests/plugin_client/README.md)。这些测试需要真实游戏和预先准备的集群/桌面环境。
- 游戏 Lua 调试需要 VS Code 的 Lua 调试扩展。可通过游戏参数 `-enable_lua_debugger` 开启调试端口，源码映射和 launch 配置示例见 [README 调试章节](../README.md)。
- 首先检查 `<游戏目录>/data/unsafedata/ds_luajit_boot.log`；游戏 Mod 日志可搜索 `[luajit][plugin]`。如果模块未载入，核对平台库名、Mod 的 `plugins/` 目录层级以及安装脚本是否在最新构建后重新执行。

## 常见问题

| 现象 | 检查项 |
| --- | --- |
| 编译时找不到游戏目录 | 执行 `dev_init.bat` / `./dev_init.sh`，确认 Steam 路径正确生成到 `cmake/GameDir.cmake`。 |
| 编译成功但游戏仍是 Lua 5.1 | 确认 Mod 已启用；检查真实 Injector 在 Mod 根目录，且游戏侧只有对应平台的注入壳；查看 `ds_luajit_boot.log`。 |
| 找不到动态 Plugin | 检查模块是否放在 `Mod/plugins/<plugin_package>/`，包目录/库名是否与构建一致。 |
| Native Plugin 已加载但 Lua 面未运行 | 检查 `Mod/plugins/init.lua` 是否注册 `load_package`，package `plugin_id` 和 Host id 是否相同，以及 `AfterModMain` Gate/配置项是否允许启用。 |
| 专用服务器开服后 VM 没有切换 | `DisableJITWhenServer` 仅针对专用服务器进程；客户端主持的服务器进程不适用。 |

崩溃或游戏内部函数分析应优先依据反汇编，并按项目的跨二进制逆向流程从带符号参考二进制验证映射；不能仅凭源码行号推断运行时崩溃位置。
