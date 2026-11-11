# Godot DocRender 中文本地化

在 Godot 的「编辑器设置 → 界面 → 本地化」中选择简体中文并重启。插件随编辑器加载翻译，安装时请一并复制 `addons/docrender/translations` 目录。

| 文件 | 内容 |
| --- | --- |
| `editor.zh_Hans.po` | Markdown 预览菜单 |
| `properties.zh_Hans.po` | 检查器属性名 |
| `documentation.zh_Hans.po` | 编辑器内置类参考 |

直接编辑 PO 文件中的 `msgstr`。类名、属性名、方法名和 Godot 文档标记应保留原拼写。

修改英文源码后，在仓库根目录运行以下命令。Python 和 Babel 用于维护目录，编辑器可以直接读取 PO。

```powershell
python -m pip install Babel
python tools/localization.py update
python tools/localization.py check
```

`update` 从 C++ 和 `doc_classes/*.xml` 更新词条，保留匹配的译文。审阅新增和标记为 `fuzzy` 的译文后运行 `check`。检查器的属性名样式设为「本地化」时会显示中文属性名。
