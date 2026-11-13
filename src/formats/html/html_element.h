#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <memory>

namespace litehtml {
	class element;
}

class HTMLLayout;

class HTMLElement : public godot::RefCounted {
	GDCLASS(HTMLElement, godot::RefCounted)

	friend class HTMLLayout;

	struct DetachedTree;
	godot::ObjectID layout_id;
	uint64_t parse_version = 0;
	std::weak_ptr<litehtml::element> element;
	std::shared_ptr<DetachedTree> detached_tree;

	HTMLLayout* get_owner() const;
	std::shared_ptr<litehtml::element> lock_element() const;

protected:
	static void _bind_methods();

public:
	bool is_valid() const;															// 检查句柄是否仍指向原布局中的元素
	godot::String get_tag_name() const;												// 返回小写 HTML 标签名
	bool has_attribute(const godot::String& p_name) const;							// 检查指定属性是否存在，空属性也视为存在
	godot::String get_attribute(
		const godot::String& p_name,
		const godot::String& p_default_value = {}
	) const;																		// 读取属性值，缺失时返回默认值
	godot::String get_text_content() const;											// 按文档顺序读取后代文本，跳过注释和 CSS 生成内容
	godot::Ref<HTMLElement> get_parent() const;										// 返回父元素，没有父元素时返回 null
	godot::TypedArray<HTMLElement> get_children() const;							// 按文档顺序返回直接子元素
	godot::TypedArray<godot::Rect2> get_client_rects() const;						// 返回各排版片段在文档坐标中的边框盒
	godot::Rect2 get_bounding_rect() const;											// 返回包围所有排版片段的矩形
	godot::TypedArray<godot::Dictionary> get_css_diagnostics() const;				// 返回本元素的 CSS 声明诊断快照

	// 同一布局内的同一元素复用仍在使用的句柄，重新解析后旧句柄失效
	godot::Ref<HTMLElement> query_selector(const godot::String& p_selector) const;	// 返回第一个匹配的后代元素，:scope 指向当前元素
	godot::TypedArray<HTMLElement> query_selector_all(
		const godot::String& p_selector
	) const;																		// 按文档顺序返回所有匹配后代的数组快照
	bool matches(const godot::String& p_selector) const;							// 检查当前元素是否匹配选择器
	godot::Ref<HTMLElement> closest(const godot::String& p_selector) const;			// 从当前元素起向父级查找，返回第一个匹配项
	godot::Dictionary get_attributes() const;										// 返回全部属性名称和值的字典快照
	godot::Dictionary get_dataset() const;											// 返回 data-* 属性的字典快照，将 data-item-id 映射为 itemId
	godot::PackedStringArray get_class_list() const;								// 返回按 class 属性顺序排列的类名快照
	bool has_class(const godot::String& p_class_name) const;						// 检查当前元素是否包含指定类名

	// 修改作用于当前布局实例，同帧更新合并重排
	godot::Error set_text_content(const godot::String& p_text);						// 将子内容替换为纯文本并使原有子元素的句柄失效
	godot::Error set_attribute(
		const godot::String& p_name,
		const godot::String& p_value
	);																				// 添加或更新指定属性
	godot::Error remove_attribute(const godot::String& p_name);						// 移除指定属性
	godot::Error add_class(const godot::String& p_class_name);						// 添加指定类名，重复添加时保留一个
	godot::Error remove_class(const godot::String& p_class_name);					// 移除指定类名
	godot::Error toggle_class(
		const godot::String& p_class_name,
		const godot::Variant& p_force = {}
	);																				// 切换指定类名，可传入布尔值指定是否保留该类名

	// HTML 片段和元素树编辑
	godot::String get_inner_html() const;											// 将当前元素的子内容序列化为 HTML 片段
	godot::Error set_inner_html(const godot::String& p_html);						// 将 HTML 片段解析为新的子内容并使原有子元素的句柄失效
	godot::Error append_child(const godot::Ref<HTMLElement>& p_child);				// 将同一布局中的新元素或已有元素追加到当前元素的子元素末尾
	godot::Error remove();															// 从父元素中移除当前元素，使该元素及其后代的句柄失效
};
