# HTML CSS support

DocRender uses litehtml v0.10 (`9bc84b8`) for CSS parsing, the cascade, and layout. The Godot drawing code supplies text, borders, images, gradients, shadows, and clips. This reference covers common declarations and known limits. It applies to `HTMLLayout`, `HTMLLabel`, and HTML blocks in `MarkdownLabel`.

## Selectors and rules

| Feature | Support |
| --- | --- |
| Basic selectors | Type, class, ID, attribute, selector lists, descendant, child, and sibling combinators |
| Pseudo-classes | `:root`, `:is()`, `:not()`, `:nth-child()` and related structural selectors. `HTMLLabel` supplies the state for `:hover`. `:has()` and `:where()` are unsupported. |
| Pseudo-elements | `::before` and `::after` with generated content. `::first-line` and `::first-letter` are unsupported. |
| Cascade | Specificity, source order, `!important`, custom properties, and `var()`. Support for CSS-wide keywords varies by property. |
| Style sources | Inline `style`, `<style>`, the extra `stylesheet` string, and local project stylesheets loaded through `<link>` or `@import`. |
| At-rules | `@media` supports viewport conditions such as width, height, and orientation. `@import` supports a local path and optional media condition. `@supports`, `@layer`, `@font-face`, and `@keyframes` are unsupported. |

## Properties and values

| Area | Support |
| --- | --- |
| Layout | Block and inline layout, lists, tables, Flexbox, dimensions, margins, padding, and static, relative, absolute, and fixed positioning. Grid and sticky positioning are unsupported. |
| Flexbox spacing | `row-gap`, `column-gap`, and one- or two-value `gap` work with wrapping and reverse directions. The shorthand sets the row gap first and the column gap second. Values include `normal` (zero in Flexbox), non-negative lengths, and percentages. Percentage gaps use the container's inner size on their axis and resolve to zero when that size depends on the flex items. [CSS Gaps](https://www.w3.org/TR/css-gaps-1/#gaps) |
| Lengths | `px`, `em`, `rem`, `ex`, `ch`, `vw`, `vh`, `vmin`, `vmax`, `in`, `cm`, `mm`, `pt`, and `pc`. Percentages depend on the property. `calc()`, `min()`, `max()`, and `clamp()` length expressions are unsupported. |
| Colors | Named and hex colors, `rgb()` and `hsl()` variants, and `currentColor` where applicable. `oklch()` and `color-mix()` are unsupported. |
| Backgrounds | Solid colors, local images, and linear, radial, and conic gradients. |
| Borders and shadows | `solid`, `dashed`, `dotted`, `double`, per-side widths and colors, and `border-radius`. DocRender adds `box-shadow`, including `inset` and multiple layers. `groove`, `ridge`, `inset`, and `outset` border styles parse but have no drawing implementation. |
| Text | Theme fonts, font size and weight, italic text, wrapping, alignment, and line height. Godot theme fonts and the renderer's code font mappings determine which font is used. `text-decoration` supports underline, overline, and line-through in five styles, with `auto`, `from-font`, length, and percentage thickness. `text-shadow` supports offsets, blur, and multiple layers on glyphs and decorations. Text emphasis marks parse but have no drawing implementation. |
| Cursor | `HTMLLabel` maps common CSS keywords to Godot cursor shapes. Image cursors and unmapped keywords use the control's fallback. |

## Declaration diagnostics

The editor's HTML source preview outlines visible elements with CSS declaration diagnostics. Hover over an outlined element to read the details in an HTML tooltip. For your own `HTMLLabel`, enable **Debug → CSS Diagnostics Enabled** in the Inspector or set `css_diagnostics_enabled` from GDScript. The same setting on a standalone `HTMLLayout` enables outlines in `draw()`.

`HTMLElement.get_css_diagnostics()` returns the declarations associated with that element. `HTMLLabel.get_css_diagnostics()` and `HTMLLayout.get_css_diagnostics()` include the full document's results, including declarations in rules that match no visible element. Each entry includes the property, value, and source. Local stylesheets also include their `res://` path.

| Kind | Example |
| --- | --- |
| `unknown_property` | `grid-template-columns: 1fr 1fr` |
| `unsupported_value` | `width: calc(100% - 1em)` |
| `not_rendered` | `border-top-style: groove` |

Diagnostics cover declarations reached by litehtml's parser. For a declaration using `var()`, the value is checked when litehtml resolves it on an element. Selector and at-rule support is listed above. Use `get_render_error()` for parse and layout failures.
