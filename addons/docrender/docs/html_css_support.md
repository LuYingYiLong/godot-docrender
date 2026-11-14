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
| Layout | Block and inline layout, lists, tables, Flexbox, block Grid, dimensions, margins, padding, and static, relative, absolute, and fixed positioning. Sticky positioning is unsupported. |
| 2D transforms | `transform` accepts `none` and lists of `translate()`, `translateX()`, `translateY()`, `rotate()`, `scale()`, `scaleX()`, and `scaleY()`. `transform-origin` supports one or two position components. See the 2D transforms section below. |
| Grid | Track lists support named lines and `repeat()`, including `auto-fill` and `auto-fit`. Grid items support placement by name, line number, or span. Grid supports automatic placement, patterned implicit tracks, item alignment, and track distribution. |
| Flexbox spacing | `row-gap`, `column-gap`, and one- or two-value `gap` work with wrapping and reverse directions. The shorthand sets the row gap first and the column gap second. Values include `normal` (zero in Flexbox), non-negative lengths, and percentages. Percentage gaps use the container's inner size on their axis and resolve to zero when that size depends on the flex items. [CSS Gaps](https://www.w3.org/TR/css-gaps-1/#gaps) |
| Lengths | `px`, `em`, `rem`, `ex`, `ch`, `vw`, `vh`, `vmin`, `vmax`, `in`, `cm`, `mm`, `pt`, and `pc`. `calc()`, `min()`, `max()`, and `clamp()` support the layout properties listed below. |
| Colors | Named and hex colors, `rgb()` and `hsl()` variants, and `currentColor` where applicable. `oklch()` and `color-mix()` are unsupported. |
| Backgrounds | Solid colors, local images, and linear, radial, and conic gradients. |
| Borders and shadows | `solid`, `dashed`, `dotted`, `double`, per-side widths and colors, and `border-radius`. DocRender adds `box-shadow`, including `inset` and multiple layers. `groove`, `ridge`, `inset`, and `outset` border styles parse but have no drawing implementation. |
| Text | Theme fonts, font size and weight, italic text, wrapping, alignment, and line height. `font-family` accepts a prioritized list of installed family names and generic families, including in the `font` shorthand. `text-decoration` supports underline, overline, and line-through in five styles, with `auto`, `from-font`, length, and percentage thickness. `text-shadow` supports offsets, blur, and multiple layers on glyphs and decorations. Text emphasis marks parse but have no drawing implementation. |
| Cursor | `HTMLLabel` maps common CSS keywords to Godot cursor shapes. Image cursors and unmapped keywords use the control's fallback. |

## Variables and length expressions

```css
:root { --gutter: 16px; }
.panel {
  width: min(100%, calc(48rem - 2 * var(--gutter, 16px)));
  padding: clamp(8px, 2vw, 24px);
}
```

Custom property names are case-sensitive. Values resolve before inheritance. `var()` supports repeated references, nested and empty fallbacks, and detects cycles, including references in fallback arguments. Invalid substitutions use the property's inherited or initial value. Declaration diagnostics identify the affected element and retain the original declaration. [CSS Custom Properties](https://www.w3.org/TR/css-variables-1/#using-variables)

Length expressions are supported in `width`, `height`, their `min-` and `max-` variants, `margin`, `padding`, `top`, `right`, `bottom`, `left`, `flex-basis`, Flexbox and Grid gaps, and length entries in Grid track lists. Addition and subtraction require spaces around the operator. Multiplication takes a number on at least one side, and division takes a numeric divisor. Percentages use the basis supplied by each layout property. [CSS Values and Units](https://www.w3.org/TR/css-values-4/#math)

## 2D transforms

```css
.card {
  transform-origin: left top;
  transform: translate(10%, 12px) rotate(8deg) scale(1.1);
}
```

Translation and origin offsets accept lengths, percentages, and length expressions. Percentages use the border-box width or height. The default origin is the center of that box. Rotation accepts `deg`, `grad`, `rad`, `turn`, and unitless zero. Scale accepts finite factors, including negative factors. A singular transform such as `scale(0)` hides the subtree while preserving its place in the flow.

Transforms affect painting, element rectangles, links, selection, and scrollable overflow. Non-replaced inline boxes and table columns or column groups can inherit an ancestor's transform as part of its subtree. Use an inline-block for a transform on an inline element itself. In Markdown, downward overflow expands the HTML block and the column bounds clip horizontal overflow. Skew, 3D transforms, `transform-box`, standalone transform properties, and animation are outside the supported set. [CSS Transforms Level 1](https://www.w3.org/TR/css-transforms-1/)

## Grid

```css
.cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
  grid-template-rows: auto;
  gap: 12px 16px;
}

.cards .featured {
  grid-column: 1 / -1;
  grid-row: 1;
}
```

Use a positive integer in `repeat()` to repeat one or more track sizes. For example, `repeat(2, 80px minmax(0, 1fr))` creates four tracks. An axis can declare up to 10,000 explicit tracks. `minmax()` accepts non-negative lengths, percentages, and `auto` on either side, with fractions also accepted as the maximum. [CSS Grid track sizing](https://www.w3.org/TR/css-grid-2/#track-sizes)

Each axis accepts one `repeat(auto-fill, …)` or `repeat(auto-fit, …)`. Every track in a template containing an automatic repeat needs a length or percentage as at least one of its sizing bounds. Each layout pass counts repetitions using the container's content box and gaps. With only a definite minimum size, it rounds the count up to meet that minimum. Without a definite size constraint, it repeats once. [CSS Grid automatic repetition](https://www.w3.org/TR/css-grid-2/#auto-repeat)

`auto-fill` keeps every repeated track. `auto-fit` collapses repeated tracks that no in-flow item occupies or spans. The remaining tracks have one gap between them, and the grid retains its original line numbers after collapse. [CSS Grid gutters](https://www.w3.org/TR/css-grid-2/#gutters)

Use brackets to name a grid line, as in `[outer content-start] 80px [content-end]`. Line names are case-sensitive. `repeat(3, [card] 80px [card])` produces four distinct `card` lines. `grid-column: 2 card / -1 card` uses the second matching line from the start and the last matching line. A definite edge can anchor a named span such as `grid-column: 1 / span 2 card`. `auto-fit` retains line names when it collapses empty tracks. [CSS Grid line names](https://www.w3.org/TR/css-grid-2/#named-lines) · [CSS Grid line placement](https://www.w3.org/TR/css-grid-2/#line-placement)

```css
.dashboard {
  display: grid;
  grid-template-columns: [frame] 120px minmax(0, 1fr);
  grid-template-areas: "title title" "nav main" "nav .";
}

.title { grid-area: title; }
.main { grid-area: 2 / main-start / span 2 / -1; }
```

Each string in `grid-template-areas` defines one row. Consecutive dots represent one empty cell. Rows must have equal lengths and each named area must fill a rectangle. An area adds `name-start` and `name-end` lines to both axes. The template can extend the explicit grid. `grid-area` sets row start, column start, row end, and column end, in that order. A name on its own applies to all four edges. [CSS Grid named areas](https://www.w3.org/TR/css-grid-2/#grid-template-areas-property) · [CSS Grid placement shorthands](https://www.w3.org/TR/css-grid-2/#placement-shorthands)

`grid-row`, `grid-column`, and their `-start` and `-end` longhands accept names, nonzero integer line numbers, `span`, and `auto`. Positive line numbers count from the start of the explicit grid. Negative line numbers count back from its end, so `grid-column: 1 / -1` spans the explicit grid even when implicit columns exist. Placement coordinates are limited to 10,000 tracks on each side of the explicit origin. An area entirely beyond the limit uses the nearest edge track. [CSS Grid placement](https://www.w3.org/TR/css-grid-2/#placement) · [Large grid limits](https://www.w3.org/TR/css-grid-2/#overlarge-grids)

Explicit areas may overlap. Automatic placement skips occupied areas and processes items in `order` and document order. `grid-auto-flow` defaults to `row`, which fills each row and then adds rows as needed. `column` fills each column and then adds columns. Add `dense` to either direction to revisit earlier holes, or use `dense` alone for `row dense`. Consecutive direct text forms an anonymous item. Whitespace-only runs and out-of-flow children take no cells. A numeric `z-index` establishes a stacking context on a Grid item, including one with `position: static`. [CSS Grid auto-placement](https://www.w3.org/TR/css-grid-2/#auto-placement-algo) · [CSS Grid painting order](https://www.w3.org/TR/css-grid-2/#z-order)

`grid-auto-rows` and `grid-auto-columns` accept a space-separated list of the supported track sizes and default to `auto`. For `grid-auto-columns: 60px 100px`, the first column after the sized tracks takes `60px`, and the closest implicit column before the explicit grid takes `100px`. The pattern repeats in each direction. The forward pattern also sizes any tracks added by `grid-template-areas` that have no size from `grid-template-columns` or `grid-template-rows`. [CSS Grid implicit track sizing](https://www.w3.org/TR/css-grid-2/#auto-tracks)

`row-gap`, `column-gap`, and the `gap` shorthand add spacing between tracks, including between tracks in a spanning item's area. `normal` resolves to zero. Percentage tracks use `auto` and the percentage portion of a gap resolves against zero during intrinsic sizing. Final layout resolves percentages against the content box. [CSS percentage gaps](https://www.w3.org/TR/css-gaps-1/#gap-percent)

`justify-items` and `align-items` set the horizontal and vertical alignment within each grid area. Individual items can override these defaults with `justify-self` and `align-self`. Positions include `start`, `end`, and `center`, with `left` and `right` available on the horizontal axis. Add `safe` before a position to use start alignment when the item overflows its area. `stretch` fills the area along the chosen axis when the item has an automatic size and no automatic margins on that axis. Automatic margins absorb positive free space before alignment. [CSS Grid item alignment](https://www.w3.org/TR/css-grid-2/#grid-align)

`justify-content` and `align-content` align or distribute the tracks within the content box. In addition to positional alignment and `stretch`, they support `space-between`, `space-around`, and `space-evenly`. Distribution adds to the declared gap, and spanning items include the added spacing inside their areas. Collapsed `auto-fit` tracks retain their line numbers and contribute no spacing. [CSS Grid content distribution](https://www.w3.org/TR/css-align-3/#distribution-grid)

`align-items`, `align-self`, `justify-items`, and `justify-self` support `baseline` (equivalent to `first baseline`) and `last baseline` in horizontal, left-to-right layout. Row alignment uses the first or last text baseline, with the bottom border edge supplying one for images and empty boxes. Column alignment synthesizes a baseline at the left border edge. Spanning items join the group at their first or last track, and the alignment adjustment contributes to the size needed across the span. Auto margins take precedence. Percentage sizes and limits that depend on content-sized tracks use safe start or end alignment. [CSS Grid baseline alignment](https://www.w3.org/TR/css-grid-2/#grid-align) · [Baseline grouping](https://www.w3.org/TR/css-align-3/#baseline-sharing-group)

Grid items with `align-content: first baseline` or `last baseline` can align their contents with neighboring items in the same row. Participation requires a content baseline and self-alignment that anchors the matching edge. Extra space is applied inside the box. When the content cannot join the group, the corresponding safe edge applies. DocRender implements this for its Grid layout. Browser behavior for these values varies. [CSS baseline content alignment](https://www.w3.org/TR/css-align-3/#baseline-align-content) · [CSS Working Group resolution](https://github.com/w3c/csswg-drafts/issues/13421)

`place-items`, `place-self`, and `place-content` set the corresponding `align-*` and `justify-*` properties in that order. A single value applies to both axes, as in `place-items: center`. With a single baseline value in `place-content`, the second axis defaults to `start`. Multi-keyword values such as `place-self: safe end unsafe center` use the grammar of the corresponding longhand. [CSS Box Alignment shorthands](https://www.w3.org/TR/css-align-3/#place-content)

The `grid` and `grid-template` shorthands, `subgrid`, and `inline-grid` remain unsupported.

## Font families

```css
body { font-family: "Segoe UI", Arial, sans-serif; }
code { font-family: "Cascadia Mono", Consolas, monospace; }
```

The renderer tries the listed families in order and lets Godot shape text with the next font when a glyph is missing. Quoted names are literal, including punctuation and names such as `"monospace"`. Unquoted generic families use theme fonts or installed fonts, depending on the family and platform. The project or Markdown theme supplies the default text font and `sans-serif`. A configured Markdown code font supplies `monospace`. `initial` restores the default, `inherit` and `unset` inherit the parent's list, and `revert` rolls back to the default stylesheet and presentational hints. `revert-layer` behaves like `revert` for the supported unlayered styles.

Family names use Unicode default case folding without normalization. Godot and the OS provide installed font discovery, localized aliases, and face matching. When a generic family has no available candidate, rendering proceeds through the rest of the list and the document default. [CSS Fonts Level 4](https://www.w3.org/TR/css-fonts-4/#font-family-prop)

## Declaration diagnostics

The editor's HTML source preview outlines visible elements with CSS declaration diagnostics. Hover over an outlined element to read the details in an HTML tooltip. For your own `HTMLLabel`, enable **Debug → CSS Diagnostics Enabled** in the Inspector or set `css_diagnostics_enabled` from GDScript. The same setting on a standalone `HTMLLayout` enables outlines in `draw()`.

`HTMLElement.get_css_diagnostics()` returns the declarations associated with that element. `HTMLLabel.get_css_diagnostics()` and `HTMLLayout.get_css_diagnostics()` include the full document's results, including declarations in rules that match no visible element. Each entry includes the property, value, and source. Local stylesheets also include their `res://` path.

| Kind | Example |
| --- | --- |
| `unknown_property` | `grid-template: auto / 1fr` |
| `unsupported_value` | `grid-template-areas: "head head" "head ."` |
| `not_rendered` | `border-top-style: groove` |

Diagnostics cover declarations reached by litehtml's parser. For a declaration using `var()`, the value is checked when litehtml resolves it on an element. Selector and at-rule support is listed above. Use `get_render_error()` for parse and layout failures.
