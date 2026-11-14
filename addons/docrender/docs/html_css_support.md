# HTML CSS support

DocRender parses CSS declarations with its own C++ modules. litehtml v0.10 (`9bc84b8`) provides tokens, stylesheet rules, selector matching, the cascade, and basic layout. This reference applies to `HTMLLayout`, `HTMLLabel`, and HTML blocks in `MarkdownLabel`.

## Selectors and rules

| Feature | Support |
| --- | --- |
| Basic selectors | Type, class, ID, attribute, selector lists, descendant, child, and sibling combinators |
| Pseudo-classes | `:root`, `:empty`, `:is()`, `:where()`, `:not()`, `:has()`, `:nth-child()` and related structural selectors. `HTMLLabel` supplies `:hover`. Its `<button type="button">` elements also support `:active`, `:focus`, `:focus-visible`, `:enabled`, and `:disabled`. |
| Pseudo-elements | `::before` and `::after` with generated content. `::first-line` and `::first-letter` are unsupported. |
| Cascade | Specificity, source order, `!important`, custom properties, and `var()`. Supported declarations share the CSS-wide keywords `inherit`, `initial`, `unset`, `revert`, and `revert-layer`. |
| Style sources | Inline `style`, `<style>`, the extra `stylesheet` string, and local project stylesheets loaded through `<link>` or `@import`. |
| At-rules | `@media` supports viewport conditions such as width, height, and orientation. `@supports` tests declarations and selectors. `@import` supports a local path, an optional `supports(…)` condition and an optional media condition. `@layer`, `@font-face`, and `@keyframes` are unsupported. |

## Properties and values

| Area | Support |
| --- | --- |
| Layout | Block and inline layout, lists, tables, Flexbox, block and inline Grid, dimensions, margins, padding, and static, relative, absolute, and fixed positioning. The `inset` shorthand sets `top`, `right`, `bottom`, and `left`. Sticky positioning is unsupported. |
| 2D transforms | `transform` accepts `none` and lists of `matrix()`, `translate()`, `translateX()`, `translateY()`, `rotate()`, `scale()`, `scaleX()`, `scaleY()`, `skew()`, `skewX()`, and `skewY()`. `transform-origin` supports one or two position components. See the 2D transforms section below. |
| Grid | Track lists support named lines and `repeat()`, including `auto-fill` and `auto-fit`. Grid items support placement by name, line number, or span. Grid supports automatic placement, patterned implicit tracks, item alignment, and track distribution. |
| Flexbox spacing | `row-gap`, `column-gap`, and one- or two-value `gap` work with wrapping and reverse directions. The shorthand sets the row gap first and the column gap second. Values include `normal` (zero in Flexbox), non-negative lengths, and percentages. Percentage gaps use the container's inner size on their axis and resolve to zero when that size depends on the flex items. [CSS Gaps](https://www.w3.org/TR/css-gaps-1/#gaps) |
| Lengths | `px`, `em`, `rem`, `ex`, `ch`, `vw`, `vh`, `vmin`, `vmax`, `in`, `cm`, `mm`, `pt`, and `pc`. `calc()`, `min()`, `max()`, and `clamp()` support the layout properties listed below. |
| Colors | Named and hex colors, `rgb()` and `hsl()` variants, absolute `oklch()`, two-color `color-mix()`, and `currentColor` where applicable. |
| Backgrounds | Solid colors, local images, and linear, radial, and conic gradients. |
| Borders and shadows | `solid`, `dashed`, `dotted`, `double`, `groove`, `ridge`, `inset`, and `outset`, with per-side widths and colors and `border-radius`. `box-shadow` supports `inset` and multiple layers. |
| Text | Theme fonts, font size and weight, italic text, wrapping, alignment, and line height. `font-family` accepts a prioritized list of installed family names and generic families, including in the `font` shorthand. `text-decoration` supports underline, overline, and line-through in five styles, with `auto`, `from-font`, length, and percentage thickness. `text-shadow` supports offsets, blur, and multiple layers on glyphs and decorations. Text emphasis marks parse but have no drawing implementation. |
| Cursor | `HTMLLabel` maps common CSS keywords to Godot cursor shapes. Image cursors and unmapped keywords use the control's fallback. |

## Relational selectors and feature queries

`:has()` matches relative to its subject. Use `:has(.icon)` for descendants, `:has(> .icon)` for children, and `:has(+ .item)` or `:has(~ .item)` for following siblings. A relative selector may continue with more combinators. The argument list contributes its highest specificity. `:where()` matches its selector list with zero specificity. `:empty` follows Selectors Level 4 and ignores comments, generated content, and document whitespace. [Selectors Level 4](https://www.w3.org/TR/selectors-4/#relational)

Use `@supports (display: grid)` to test a declaration and `@supports selector(.card:has(> img))` to test a selector. Conditions accept `not`, `and`, `or`, and parentheses for grouping. Nest `@supports` and `@media` to require both conditions. Declaration probes use the CSS parser and the known rendering limits. A well-formed `var()` probe checks the declaration syntax before substitution. Selector probes reject unsupported arguments inside otherwise forgiving lists. [CSS Conditional Rules](https://www.w3.org/TR/css-conditional-3/#at-supports) · [Selector feature queries](https://www.w3.org/TR/css-conditional-4/#supports-selector-fn)

## Cascade

Document styles from `<style>` and `<link rel="stylesheet">` follow document order. Each `@import` contributes its rules at the import location. The additional `stylesheet` contributes author rules after the document's rules.

DocRender compares declarations for each longhand in this order.

1. Author declarations marked `!important` precede normal author declarations.
2. A `style` attribute precedes stylesheet rules of the same importance.
3. Among stylesheet rules with equal importance, higher selector specificity wins. For instance, a document rule for `#card` takes precedence over an additional rule for `.card`.
4. When the preceding criteria match, the later declaration wins. For equally specific stylesheet rules, the additional stylesheet comes last.

The pre-author baseline contains litehtml defaults, control and theme defaults, and HTML presentational hints. Both `revert` and `revert-layer` restore this baseline for the affected longhands. [CSS Cascade](https://www.w3.org/TR/css-cascade-5/#cascade-sort)

## Variables and length expressions

```css
:root { --gutter: 16px; }
.panel {
  width: min(100%, calc(48rem - 2 * var(--gutter, 16px)));
  padding: clamp(8px, 2vw, 24px);
}
```

Custom property names are case-sensitive. Values resolve before inheritance. `var()` supports repeated references, nested and empty fallbacks, and detects cycles, including references in fallback arguments. Invalid substitutions use the property's inherited or initial value. Declaration diagnostics identify the affected element and retain the original declaration. [CSS Custom Properties](https://www.w3.org/TR/css-variables-1/#using-variables)

Shorthands validate the full value before applying their longhands. Omitted components reset to their shorthand defaults. A shorthand with `var()` resolves once per element and updates the longhands it still wins. A winning declaration with an invalid substitution uses `unset`. In DocRender's unlayered cascade, `revert` and `revert-layer` both restore the default stylesheet or HTML presentational hint for each property. A property without a default declaration uses `unset`. [CSS Cascading and Inheritance](https://www.w3.org/TR/css-cascade-5/)

Length expressions are supported in `width`, `height`, their `min-` and `max-` variants, `margin`, `padding`, `top`, `right`, `bottom`, `left`, `flex-basis`, Flexbox and Grid gaps, and length entries in Grid track lists. Addition and subtraction require spaces around the operator. Multiplication takes a number on at least one side, and division takes a numeric divisor. Percentages use the basis supplied by each layout property. [CSS Values and Units](https://www.w3.org/TR/css-values-4/#math)

## OkLCh colors

`oklch(65% 0.15 40deg / 80%)` is valid wherever DocRender accepts a CSS color. Lightness, chroma, and alpha accept numbers and percentages. A chroma of `100%` represents `0.4`. Hue accepts a number in degrees or an angle in `deg`, `grad`, `rad`, or `turn`. Each component accepts `none`, treated as zero when displayed directly. [CSS Color 4](https://www.w3.org/TR/css-color-4/#ok-lab)

`color-mix(in oklch shorter hue, currentColor 30%, white)` mixes two colors in `srgb`, `srgb-linear`, `oklab`, or `oklch`. Omitting the interpolation method uses `oklab`. In `oklch`, hue can follow the `shorter`, `longer`, `increasing`, or `decreasing` arc. Missing components follow the CSS interpolation rules, and non-hue components are interpolated with premultiplied alpha. [CSS Color 4 interpolation](https://www.w3.org/TR/css-color-4/#interpolation)

Each input can include a weight from `0%` to `100%`. Omitted weights share the remainder. Explicit weights are normalized, and when the total is below `100%`, it also scales the output alpha. A total of zero yields a transparent color. Custom properties can supply the inputs or weights. Mixes that contain `currentColor` are resolved in the element that uses the property. In the `color` property itself, they use the inherited text color. Nested mixes retain floating-point colors until painting, when channels are clipped to DocRender's 8-bit sRGB output. Relative colors, math in weights or color components, and mixes with more than two inputs are outside the supported grammar. [CSS Color 5](https://www.w3.org/TR/css-color-5/#color-mix)

Markdown HTML blocks use the full HTML color pipeline. Markdown's inline `<span style>` supports constant mixes in `color` and `background-color`.

## 2D transforms

```css
.card {
  transform-origin: left top;
  transform: translate(10%, 12px) rotate(8deg) scale(1.1);
}
```

Translation and origin offsets accept lengths, percentages, and length expressions. Percentages use the border-box width or height. The default origin is the center of that box. Rotation accepts `deg`, `grad`, `rad`, `turn`, and unitless zero. Scale accepts finite factors, including negative factors. A singular transform such as `scale(0)` hides the subtree while preserving its place in the flow.

Transforms affect painting, element rectangles, links, selection, and scrollable overflow. Non-replaced inline boxes and table columns or column groups can inherit an ancestor's transform as part of its subtree. Use an inline-block for a transform on an inline element itself. In Markdown, downward overflow expands the HTML block and the column bounds clip horizontal overflow. 3D transforms, `transform-box`, standalone transform properties, and animation are outside the supported set. [CSS Transforms Level 1](https://www.w3.org/TR/css-transforms-1/)

Painting and hit testing use the exact clip shapes of up to 16 rounded or transformed overflow ancestors, starting with the closest ancestor. Farther clips use their document-space bounding rectangles. An element's own rounded shape is evaluated separately.

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

`display: inline-grid` uses the same tracks and item placement as `grid`. An automatic width fits the content within the containing block, subject to intrinsic sizes and `min-width` and `max-width`. The whole grid moves to the next line if it cannot fit alongside preceding text. Text inside each item can wrap within its grid area. The surrounding line uses the grid's first baseline by default, and `vertical-align` can change how the box lines up with its neighbors. [CSS Grid container sizing and baselines](https://www.w3.org/TR/css-grid-2/#grid-container-sizing) · [CSS Inline baseline source](https://www.w3.org/TR/css-inline-3/#baseline-source)

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

`grid-template` sets the explicit rows, columns, and named areas. Use `none`, a pair of track lists such as `40px auto / repeat(3, 1fr)`, or quoted area rows such as `"head head" 40px "nav main" / 12rem 1fr`. In the quoted form, omitted row sizes are `auto`, the optional column list uses individual tracks, and line names on either side of a row boundary share that line. The implicit grid settings remain in effect. [CSS Grid template shorthand](https://www.w3.org/TR/css-grid-2/#grid-template-property)

`grid` accepts the same explicit forms and adds automatic placement forms such as `auto-flow dense 24px / repeat(3, 1fr)` and `40px 40px / auto-flow 100px`. Put `auto-flow` on the left of the slash to create rows or on the right to create columns. The optional `dense` keyword can appear immediately before or after it. Any sizes that follow form the repeating implicit track pattern on that axis. Omitted longhands reset to their initial values: `none` for the explicit grid, `auto` for implicit track sizes, and `row` for the flow direction. Both shorthands preserve the existing gaps. [CSS Grid definition shorthand](https://www.w3.org/TR/css-grid-2/#grid-shorthand)

`subgrid` remains unsupported.

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
| `unknown_property` | `scrollbar-width: thin` |
| `unsupported_value` | `grid-template-areas: "head head" "head ."` |
| `not_rendered` | `text-emphasis-style: dot` |

Diagnostics cover declarations reached by the stylesheet parser. For a declaration using `var()`, the value is checked when DocRender resolves it on an element. Drawing diagnostics identify the winning declaration after shorthand expansion. Use `get_render_error()` for parse and layout failures.
