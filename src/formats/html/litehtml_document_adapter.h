#pragma once

#include <godot_cpp/variant/char_string.hpp>

#include <litehtml/document.h>
#include <litehtml/element.h>

namespace docrender {
	class LiteHTMLContainer;
}

namespace litehtml {
	struct box_shadow;
	// The staged litehtml headers grant this adapter access to native document and element state.
	struct docrender_editor {
		static document::ptr parse(const godot::CharString& p_html, docrender::LiteHTMLContainer& p_container,
			const string& p_default_styles, const string& p_user_styles); // 解析 UTF-8 文档并应用初始样式
		static bool media_changed(document& p_document); // 检查媒体条件的匹配结果是否改变
		static void restyle(const document::ptr& p_document, docrender::LiteHTMLContainer& p_container,
			const string& p_default_styles, const string& p_user_styles); // 在原有元素树上重算样式并重建渲染树
		static bool has_hover_styles(const document& p_document); // 检查已解析的样式表是否引用悬停伪类
		static void set_hover(element& p_element, bool p_hovered); // 更新元素的悬停伪类状态
		static const std::vector<box_shadow>* get_box_shadows(const html_tag& p_element); // 读取层叠后的阴影列表

		static bool set_attribute(element& p_element, const char* p_name, const char* p_value); // 更新属性和链接状态，空指针表示移除属性
		static elements_list parse_fragment(document& p_document, const element& p_parent,
			const godot::CharString& p_html); // 按父标签的上下文解析 HTML 片段
		static elements_list create_text(const document::ptr& p_document, const char* p_text,
			bool p_split); // 创建纯文本节点，可按排版需要拆分文本
		static void replace_children(element& p_parent, elements_list p_children); // 替换父元素的子节点
		static bool move_child(const element::ptr& p_child, element& p_parent); // 将节点追加到父元素末尾并保留节点身份

	private:
		static document::ptr parse_tree(const godot::CharString& p_html, document_container& p_container);
		static void reset_link(html_tag& p_element, bool p_has_href);
		static void remove_attribute(html_tag& p_element, const char* p_name);
		static void reset_tree(element& p_element);
	};
}
