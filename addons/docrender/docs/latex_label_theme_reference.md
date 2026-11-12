# LatexLabel Theme Reference

The example theme uses a dark background, light-colored formulas at 24 px, and a blue focus border. You can edit it in Godot's theme editor. [`latex_label_theme.tres`](../example/latex_label_theme.tres)

## Apply the Theme

1. Add a `LatexLabel` node to your scene and enter a LaTeX expression such as `\frac{1}{\sqrt{x^2+1}}` in **Text** in the Inspector.
2. Drag `latex_label_theme.tres` from the FileSystem dock to **Theme → Theme** in the Inspector. To share the theme across several formulas, assign it to their parent `Control`.
3. Open the theme resource and select the `LatexLabel` type in the theme editor to adjust the formula color, font size, and StyleBoxes.

## Theme Properties

| Type | Property | Example Theme | Purpose |
| --- | --- | --- | --- |
| Font Size | `font_size` | `24` | Base font size for formulas in pixels. |
| Color | `font_color` | Light color | Foreground color of the formula. |
| StyleBox | `normal` | Dark rounded background | Draws the default background. Its content margins affect the minimum size and formula alignment. |
| StyleBox | `focus` | Dark background with a blue border | Draws the focused background in place of `normal`. Formula layout still uses the content margins of `normal`. |

To see the focus style, set **Focus Mode** to **All** and press Tab to focus the node. Formulas use the bundled math fonts. The `font_size` and `font_color` in `LabelSettings` take precedence over the theme and the node's theme overrides. The control falls back to the corresponding `Label` theme item for each missing item.

## Override Theme Items on One Node

Attach this script to a `LatexLabel` to load the example theme and set the font size and color for that node.

```gdscript
extends LatexLabel

const EXAMPLE_THEME: Theme = preload("uid://fuqrhbfft60e")


func _ready() -> void:
	theme = EXAMPLE_THEME
	text = r"\frac{1}{\sqrt{x^2+1}}"
	add_theme_font_size_override(&"font_size", 30)
	add_theme_color_override(&"font_color", Color(0.95, 0.97, 1.0))
```
