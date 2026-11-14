extends SceneTree

const DOCUMENT: String = """
<!doctype html><html><head>
<style>
  body { margin: 0; }
  .probe { display: block; width: 10px; height: 4px; }
  .from_link { width: 12px; }
  .order { width: 12px; }
  .import_order { width: 12px; }
  .link_media { width: 12px; }
  .after_link { width: 12px; }
</style>
<link rel="stylesheet" href="styles/sheet.css">
<link rel="stylesheet" href="styles/media.css" media="(min-width: 400px)">
<style>
  .after_link { width: 34px; }
  #specific { width: 32px; }
  .tie { width: 12px; }
  .rule_important { width: 36px !important; }
  #important_specific { width: 38px !important; }
  #normal_specific { width: 12px; }
  .repeat { width: 12px; width: 42px; width: bogus; }
  #var_specific { --Width: 43px; }
  .var_important { --Width: 44px !important; }
  .inherit_var { --Width: 45px; }
  .shorthand { width: 20px; border: var(--border) !important; }
  .shorthand { border-left-width: 9px; }
  .invalid_var { width: 42px; width: var(--bad); }
  .ignored_var { width: var(--bad); }
  :is(.is_case, #absent) { width: 46px; }
  .not_case:not(#absent, .absent) { width: 47px; }
  .nth_case:nth-child(1 of .nth_case, #absent) { width: 48px; }
  .nth_last_case:nth-last-child(1 of .nth_last_case, #absent) { width: 49px; }
  #pseudo::before { content: "SPECIFIC"; }
  #pseudo::after { content: var(--after); }
  ::before { color: #123456; }
  section::before { content: ""; }
  section::after { content: ""; }
  #aligned { text-align: left; }
  @media (min-width: 400px) { #specific { width: 50px; } }
</style>
</head><body>
<div id="from_link" class="probe from_link"></div>
<div id="order" class="probe order"></div>
<div id="import_order" class="probe import_order"></div>
<div id="after_link" class="probe after_link"></div>
<div id="link_media" class="probe link_media"></div>
<div id="specific" class="probe specific"></div>
<div id="tie" class="probe tie"></div>
<div id="inline_normal" class="probe tie" style="width: 35px"></div>
<div id="rule_important" class="probe rule_important" style="width: 12px"></div>
<div id="inline_important" class="probe rule_important" style="width: 37px !important; width: 12px"></div>
<div id="inline_important_vs_normal" class="probe tie" style="width: 39px !important"></div>
<div id="important_specific" class="probe important_specific"></div>
<div id="normal_specific" class="probe normal_specific"></div>
<div id="repeat" class="probe repeat"></div>
<div id="var_specific" class="probe variable" style="width: var(--Width)"></div>
<div id="var_important" class="probe var_important" style="width: var(--Width); --Width: 1px"></div>
<div class="inherit_var"><div id="inherit_var" class="probe" style="--Width: 1px; --Width: revert; width: var(--Width)"></div></div>
<div id="shorthand" class="probe shorthand" style="--border: 2px solid black; border-right-width: 5px !important"></div>
<div id="invalid_var" class="probe invalid_var" style="--bad: bogus"></div>
<div id="ignored_var" class="probe ignored_var" style="width: 41px; --bad: bogus"></div>
<div id="unset_rollback" class="probe" style="width: revert"></div>
<div id="is_case" class="probe is_case extra"></div>
<div id="not_case" class="probe not_case extra"></div>
<section><div id="nth_case" class="probe nth_case extra"></div></section>
<section><div id="nth_last_case" class="probe nth_last_case extra"></div></section>
<div id="pseudo" class="pseudo" style='--after: "AFTER"'>middle</div>
<p id="aligned" align="right" style="text-align: var(--missing, revert-layer)"><span id="aligned_text">align</span></p>
<p align="right"><span id="reference_text">align</span></p>
</body></html>
"""

const ADDITIONAL: String = """
@import "styles/additional.css";
.probe.specific { width: 12px; }
.tie { width: 33px; }
.important_specific { width: 12px !important; }
.normal_specific { width: 40px !important; }
.variable { --Width: 1px; }
.is_case.extra, .not_case.extra, .nth_case.extra, .nth_last_case.extra { width: 12px; }
.pseudo::before { content: "OVERRIDDEN"; }
.unused { width: bogus; }
"""

const FEATURE_QUERIES: String = """
<!doctype html><style>
@import "styles/conditional.css" supports(display: grid) (min-width: 300px);
@import "styles/rejected.css" supports(display: unsupported);
@import "styles/conditional.css" supports((color: color-mix(red, blue))) (max-width: 299px);
body { margin: 0; }
@supports (display: grid) and ((width: calc(5px + 5px)) or (unknown: yes)) {
  #conditional { width: 41px; }
  @media (min-width: 400px) {
    @supports (color: var(--color) !important) { #conditional { width: 43px; } }
  }
}
@supports not (display: unsupported) { #negated { width: 42px; } }
@supports (display: grid)and/**/(width: 2px) { #compact { width: 50px; } }
@supports (display: grid) and (width: 2px) or (display: grid) { #negated { width: 99px; } }
@supports not future-query() { #future { width: 44px; } }
@supports (width: 10px; height: 20px) { #future { width: 99px; } }
@supports (text-emphasis-style: dot) { #future { width: 99px; } }
@supports (border-top-style: groove) { #bevel_support { width: 52px; } }
@supports selector(.parent:has(> .leaf)) { #selector_support { width: 45px; } }
@supports selector(:is(.supported, :unsupported-example)) { #selector_support { width: 99px; } }
@supports selector(.supported, .other) { #selector_support { width: 99px; } }
@supports selector(.related:has(> :is(.leaf, q|div))) { #selector_support { width: 99px; } }
#specificity { width: 12px; }
:where(#specificity) { width: 99px; }
.related:has(> .leaf, #absent) { width: 46px; }
.related.special { width: 99px; }
.sibling:has(+ .next > .leaf) { width: 47px; }
.sibling:has(~ .later .leaf) { height: 48px; }
.related:has(.leaf:has(span)) { width: 99px; }
.related:has(::before) { width: 99px; }
.related:has(> .leaf)::before { content: "RELATIVE"; }
.empty:empty { width: 49px; }
.nonempty:empty { width: 99px; }
</style>
<div id="conditional"></div><div id="negated"></div><div id="future"></div><div id="compact"></div>
<div id="selector_support"></div><div id="conditional_import"></div><div id="specificity"></div><div id="bevel_support"></div>
<div id="related" class="related special"><span class="leaf"></span></div>
<div id="sibling" class="sibling"></div><!-- Adjacent selectors skip DOM comments -->
<div class="next"><span class="leaf"></span></div><div></div><div class="later"><span class="leaf"></span></div>
<div id="empty" class="empty"> <!-- No content --> </div><div class="nonempty" id="nonempty"><span></span></div>
"""

var _failures: int


func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	_check_layout()
	_check_css_features()
	_check_box_and_transform_functions()
	await _check_controls()
	print("CSS cascade checks finished with %d failure(s)" % _failures)
	quit(0 if _failures == 0 else 1)


func _check_layout() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(DOCUMENT, "res://", ADDITIONAL) == OK, "Parse cascade fixture")
	_check(layout.layout(Vector2i(320, 500)) == OK, "Lay out the narrow viewport")
	var widths: Dictionary[String, float] = {
		"from_link": 30.0,
		"order": 31.0,
		"import_order": 23.0,
		"after_link": 34.0,
		"link_media": 12.0,
		"specific": 32.0,
		"tie": 33.0,
		"inline_normal": 35.0,
		"rule_important": 36.0,
		"inline_important": 37.0,
		"inline_important_vs_normal": 39.0,
		"important_specific": 38.0,
		"normal_specific": 40.0,
		"repeat": 42.0,
		"var_specific": 43.0,
		"var_important": 44.0,
		"inherit_var": 45.0,
		"shorthand": 27.0,
		"invalid_var": 320.0,
		"ignored_var": 41.0,
		"unset_rollback": 320.0,
		"is_case": 46.0,
		"not_case": 47.0,
		"nth_case": 48.0,
		"nth_last_case": 49.0,
	}
	for id: String in widths:
		var actual: float = _rect(layout, id).size.x
		_check(is_equal_approx(actual, widths[id]), "%s width is %s, expected %s" % [id, actual, widths[id]])
	var rendered: String = layout.get_plain_text()
	_check("SPECIFICmiddleAFTER" in rendered and "OVERRIDDEN" not in rendered, "Pseudo-element rules and variables")
	var alignment: float = _rect(layout, "aligned_text").position.x
	_check(alignment > 100.0 and is_equal_approx(alignment, _rect(layout, "reference_text").position.x),
		"Variable revert-layer restores the HTML align hint")
	_check(_has_diagnostic(layout, "width", "bogus", "style_element"), "Invalid literal reported despite losing")
	_check(_has_diagnostic(layout, "width", "var(--bad)", "style_element"), "Invalid computed value retains its tokens")
	_check(layout.get_element_by_id("ignored_var").get_css_diagnostics().is_empty(),
		"A losing variable declaration is not evaluated")
	_check(_has_diagnostic(layout, "width", "bogus", "local_stylesheet", "res://styles/nested/inner.css"),
		"Nested import diagnostics retain the stylesheet path")
	_check(_has_diagnostic(layout, "width", "bogus", "stylesheet"), "Additional CSS diagnostics retain their source")

	var original: HTMLElement = layout.get_element_by_id("specific")
	_check(layout.layout(Vector2i(480, 500)) == OK, "Reflow across the media breakpoint")
	_check(is_equal_approx(original.get_bounding_rect().size.x, 50.0), "Matched media rules use the same cascade")
	_check(is_equal_approx(_rect(layout, "link_media").size.x, 29.0), "Linked stylesheet media condition")
	_check(original.set_attribute("id", "changed") == OK, "Update an element ID")
	_check(layout.layout(Vector2i(480, 500)) == OK and original.is_valid(), "Reflow after a DOM mutation")
	_check(is_equal_approx(original.get_bounding_rect().size.x, 12.0), "DOM mutation recomputes the rule winner")


func _check_css_features() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	layout.css_diagnostics_enabled = true
	_check(layout.parse(FEATURE_QUERIES) == OK and layout.layout(Vector2i(320, 480)) == OK,
		"Lay out the selector and feature-query fixture")
	var widths: Dictionary[String, float] = {
		"conditional": 41.0,
		"negated": 42.0,
		"future": 44.0,
		"compact": 50.0,
		"selector_support": 45.0,
		"conditional_import": 51.0,
		"specificity": 12.0,
		"related": 46.0,
		"sibling": 47.0,
		"empty": 49.0,
		"bevel_support": 52.0,
		"nonempty": 320.0,
	}
	for id: String in widths:
		_check(is_equal_approx(_rect(layout, id).size.x, widths[id]), "Feature query or selector width for %s" % id)
	_check(is_equal_approx(_rect(layout, "sibling").size.y, 48.0), "General sibling and descendant relative selector")
	_check(layout.query_selector("div:has(> .leaf)") == layout.get_element_by_id("related"), "Query with :has()")
	_check(layout.query_selector("div:has(> :is(.leaf, :unsupported-example))") == layout.get_element_by_id("related"),
		"An inner :is() remains forgiving during normal matching")
	_check(layout.query_selector_all(":where(.next, .later, :unsupported-example)").size() == 2,
		":where() parses forgiving selector lists")
	_check("RELATIVE" in layout.get_plain_text(), "A relational selector styles a pseudo-element")
	_check(layout.get_css_diagnostics().is_empty(), "Feature probes and rejected imports leave diagnostics empty")
	_check(layout.layout(Vector2i(480, 480)) == OK, "Lay out nested media and feature queries")
	_check(is_equal_approx(_rect(layout, "conditional").size.x, 43.0), "Nested @media and @supports")
	_check(layout.layout(Vector2i(280, 480)) == OK, "Lay out the alternate conditional import")
	_check(is_equal_approx(_rect(layout, "conditional_import").size.x, 51.0), "Parenthesized import supports condition")
	var leaf: HTMLElement = layout.query_selector(".related > .leaf")
	_check(leaf.set_attribute("class", "other") == OK and layout.layout(Vector2i(280, 480)) == OK,
		"Mutate a relative selector's matched child")
	_check(not layout.get_element_by_id("related").matches(":has(> .leaf)"), "Relative matching follows DOM changes")
	_check(is_equal_approx(_rect(layout, "related").size.x, 99.0), "Relational styles update on mutation")
	_check("RELATIVE" not in layout.get_plain_text(), "Generated content disappears with its relative match")


func _check_box_and_transform_functions() -> void:
	var layout: HTMLLayout = HTMLLayout.new()
	var html: String = """
<!doctype html><style>
body { margin: 0; }
div { position: absolute; width: 20px; height: 10px; transform-origin: 0 0; }
#inset { --offsets: 5px auto auto calc(2px + 3px); inset: var(--offsets); }
#matrix { inset: 10px auto auto 50px; transform: matrix(2, 0, 0, 3, 4, 6); }
#skew_x { inset: 60px auto auto 20px; transform: skewX(45deg); }
#skew_y { inset: 80px auto auto 20px; transform: skewY(45deg); }
#skew { inset: 100px auto auto 20px; transform: skew(45deg, 45deg); }
#combined { inset: 145px auto auto 20px; transform: translateX(5px) skew(45deg, 0); }
#invalid { inset: 170px auto auto 20px; transform: translateX(5px); transform: matrix(1,0,0,1,5px,0); }
</style>
<div id="inset" style="left: 15px; inset: 1px bad"></div>
<div id="matrix"></div><div id="skew_x"></div><div id="skew_y"></div><div id="skew"></div>
<div id="combined"></div><div id="invalid"></div>
"""
	_check(layout.parse(html) == OK and layout.layout(Vector2i(250, 240)) == OK, "Lay out affine and inset fixture")
	var rectangles: Dictionary[String, Rect2] = {
		"inset": Rect2(15.0, 5.0, 20.0, 10.0),
		"matrix": Rect2(54.0, 16.0, 40.0, 30.0),
		"skew_x": Rect2(20.0, 60.0, 30.0, 10.0),
		"skew_y": Rect2(20.0, 80.0, 20.0, 30.0),
		"skew": Rect2(20.0, 100.0, 30.0, 30.0),
		"combined": Rect2(25.0, 145.0, 30.0, 10.0),
		"invalid": Rect2(25.0, 170.0, 20.0, 10.0),
	}
	for id: String in rectangles:
		var actual: Rect2 = _rect(layout, id)
		_check(actual.is_equal_approx(rectangles[id]), "%s bounds are %s, expected %s" % [id, actual, rectangles[id]])
	_check(layout.get_element_at(Vector2(22.0, 68.0)) != layout.get_element_by_id("skew_x"),
		"Hit testing skips the skewed rectangle's empty corner")
	_check(layout.get_element_at(Vector2(36.0, 68.0)) == layout.get_element_by_id("skew_x"),
		"Hit testing finds the skewed rectangle")


func _check_controls() -> void:
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(500, 350)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)

	var label: HTMLLabel = HTMLLabel.new()
	label.size = Vector2(250, 280)
	label.text = """
		<style>
		  body { margin: 0; }
		  #reverted { background: red; }
		  #parent { color: red; background-color: color-mix(in srgb, currentColor, white); }
		  .bevel { display: inline-block; width: 28px; height: 25px; border: 8px solid #808080; }
		</style>
		<button id="reverted" type="button" style="background: var(--missing, revert)">Button</button>
		<button id="reference" type="button">Button</button>
		<div id="parent" style="width: 100px; height: 30px">
		  <div id="child" style="color: blue; background-color: inherit; width: 50px; height: 30px"></div>
		</div>
		<div><div id="groove" class="bevel" style="border-style: groove; border-radius: 12px"></div>
		  <div id="ridge" class="bevel" style="border-style: ridge"></div>
		  <div id="inset" class="bevel" style="border-style: inset"></div>
		  <div id="outset" class="bevel" style="border-style: outset"></div></div>
	"""
	viewport.add_child(label)

	var markdown: MarkdownLabel = MarkdownLabel.new()
	markdown.position = Vector2(250, 0)
	markdown.size = Vector2(240, 240)
	var theme_font: SystemFont = SystemFont.new()
	theme_font.font_names = PackedStringArray(["Consolas"])
	markdown.add_theme_font_override("text_font", theme_font)
	markdown.add_theme_font_size_override("text_font_size", 28)
	var test_texture: ImageTexture = ImageTexture.create_from_image(Image.create_empty(1, 1, false, Image.FORMAT_RGBA8))
	for icon: StringName in [&"task_checked", &"task_unchecked", &"file_broken"]:
		markdown.add_theme_icon_override(icon, test_texture)
	markdown.parse_html = true
	markdown.text = """
<div>
<style>#themed { font: 10px serif; } .themed { background: #edc231; }</style>
<div id="themed" class="themed" style="font: var(--missing, revert); width: 5ch; height: 1em">Theme</div>
</div>
"""
	viewport.add_child(markdown)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = viewport.get_texture().get_image()
	var reverted: Rect2 = _label_rect(label, "reverted")
	var reference: Rect2 = _label_rect(label, "reference")
	_check(reverted.has_area() and reverted.size.is_equal_approx(reference.size), "Button rollback restores default geometry")
	_check_pixel(image, reverted.position + Vector2(5.0, 5.0), Color("#f3f4f6"), "Button rollback restores the default fill")
	_check_pixel(image, reference.position + Vector2(5.0, 5.0), Color("#f3f4f6"), "Button default fill")
	var parent: Rect2 = _label_rect(label, "parent")
	_check_pixel(image, parent.position + Vector2(15.0, 15.0), Color("#8080ff"), "Inherited mix uses the child's currentColor")
	_check_pixel(image, parent.position + Vector2(75.0, 15.0), Color("#ff8080"), "Parent mix uses its own currentColor")
	var dark: Color = Color("#535353")
	var light: Color = Color("#acacac")
	var groove: Vector2 = _label_rect(label, "groove").position
	var ridge: Vector2 = _label_rect(label, "ridge").position
	var inset: Vector2 = _label_rect(label, "inset").position
	var outset: Vector2 = _label_rect(label, "outset").position
	_check_pixel(image, groove + Vector2(20.0, 2.0), dark, "Rounded groove outer band")
	_check_pixel(image, groove + Vector2(20.0, 6.0), light, "Rounded groove inner band")
	_check_pixel(image, ridge + Vector2(20.0, 2.0), light, "Ridge outer band")
	_check_pixel(image, ridge + Vector2(20.0, 6.0), dark, "Ridge inner band")
	_check_pixel(image, inset + Vector2(20.0, 2.0), dark, "Inset top edge")
	_check_pixel(image, inset + Vector2(41.0, 20.0), light, "Inset right edge")
	_check_pixel(image, outset + Vector2(20.0, 2.0), light, "Outset top edge")
	_check_pixel(image, outset + Vector2(41.0, 20.0), dark, "Outset right edge")
	var theme_box: Rect2i = _color_bounds(image, Rect2i(250, 0, 240, 200), Color("#edc231"))
	var theme_width: int = roundi(5.0 * theme_font.get_string_size("0", HORIZONTAL_ALIGNMENT_LEFT, -1.0, 28).x)
	_check(theme_box.size == Vector2i(theme_width, 28), "Markdown HTML rollback restores the theme font, box=%s" % theme_box)


func _rect(layout: HTMLLayout, id: String) -> Rect2:
	var element: HTMLElement = layout.get_element_by_id(id)
	return element.get_bounding_rect() if element != null else Rect2()


func _label_rect(label: HTMLLabel, id: String) -> Rect2:
	var element: HTMLElement = label.get_element_by_id(id)
	return element.get_bounding_rect() if element != null else Rect2()


func _has_diagnostic(layout: HTMLLayout, property: String, value: String, source: String, path: String = "") -> bool:
	for entry: Dictionary in layout.get_css_diagnostics():
		if entry["kind"] == "unsupported_value" and entry["property"] == property and entry["value"] == value:
			if entry["source"] == source and entry["path"] == path:
				return true
	return false


func _check_pixel(image: Image, position: Vector2, expected: Color, description: String) -> void:
	var actual: Color = image.get_pixelv(Vector2i(position))
	_check(_same_color(actual, expected), "%s at %s is %s, expected %s" % [description, position, actual, expected])


func _color_bounds(image: Image, region: Rect2i, expected: Color) -> Rect2i:
	var bounds: Rect2i
	for y: int in range(region.position.y, region.end.y):
		for x: int in range(region.position.x, region.end.x):
			if _same_color(image.get_pixel(x, y), expected):
				var pixel: Rect2i = Rect2i(x, y, 1, 1)
				bounds = bounds.merge(pixel) if bounds.has_area() else pixel
	return bounds


func _same_color(first: Color, second: Color) -> bool:
	const TOLERANCE: float = 2.1 / 255.0
	return absf(first.r - second.r) <= TOLERANCE and absf(first.g - second.g) <= TOLERANCE and absf(first.b - second.b) <= TOLERANCE


func _check(condition: bool, description: String) -> void:
	if not condition:
		_failures += 1
		push_error(description)
