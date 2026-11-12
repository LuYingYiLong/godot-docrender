# Godot DocRender Simplified Chinese Localization

In Godot, open **Editor Settings → Interface → Editor → Localization → Editor Language**, select **Simplified Chinese**, and restart the editor. DocRender loads its translations when the editor starts. Include `addons/docrender/translations/` when installing the addon.

| File | Contents |
| --- | --- |
| `editor.zh_Hans.po` | Markdown preview menu |
| `properties.zh_Hans.po` | Inspector property names |
| `documentation.zh_Hans.po` | Built-in class reference |

Edit the `msgstr` entries directly in the PO files. Keep class names, property names, method names, and Godot documentation markup as written in the source.

After changing the English source text, run these commands from the repository root. Python and Babel maintain the catalogs, and Godot reads the PO files directly.

```powershell
python -m pip install Babel
python tools/localization.py update
python tools/localization.py check
```

`update` extracts messages from C++ and `doc_classes/*.xml` and keeps the translations for matching entries. Review new entries and entries marked `fuzzy`, then run `check`. To display translated property names in the Inspector, set **Editor Settings → Interface → Inspector → Default Property Name Style** to **Localized**.
