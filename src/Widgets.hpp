#include "plugin.hpp"

struct TextBox : TransparentWidget {
	// Kinda like TextField except not editable. Using Roboto Mono Bold font,
	// numbers look okay.
	// based on LedDisplayChoice
	std::string text;
	std::string fontPath = "res/fonts/RobotoMono-Bold.ttf";
	float font_size;
	float letter_spacing;
	Vec textOffset;
	NVGcolor defaultTextColor;
	NVGcolor textColor; // This can be used to temporarily override text color
	NVGcolor backgroundColor;

	TextBox() {
		defaultTextColor = nvgRGB(0x23, 0x23, 0x23);
		textColor = defaultTextColor;
		backgroundColor = nvgRGB(0xc8, 0xc8, 0xc8);
		box.size = Vec(30, 18);
		// size 20 with spacing -2 will fit 3 characters on a 30px box with Roboto mono
		font_size = 20;
		letter_spacing = 0.f;
		textOffset = Vec(box.size.x * 0.5f, 0.f);
	}

	virtual void setText(std::string s) { text = s; }

	virtual void draw(const DrawArgs &args) override;

};

// a TextBox that changes its background color when hovered
struct HoverableTextBox : TextBox {
	BNDwidgetState state = BND_DEFAULT;
	NVGcolor defaultColor;
	NVGcolor hoverColor;

	HoverableTextBox(): TextBox() {
		defaultColor = backgroundColor;
		hoverColor = nvgRGB(0xd8, 0xd8, 0xd8);
	}

	void onHover(const event::Hover &e) override {
		e.consume(this); // to catch onEnter and onLeave
	}

	void onEnter(const event::Enter &e) override {
		state = BND_HOVER;
	}
	void onLeave(const event::Leave &e) override {
		state = BND_DEFAULT;
	}

	void draw(const DrawArgs &args) override {
		backgroundColor = state == BND_HOVER ? hoverColor : defaultColor;
		TextBox::draw(args);
	}
};

/*
 * Basically a custom version of TextField.
 * TextField::text holds the string being edited and HoverableTextBox::text
 * holds the string being displayed. When the user stops editing (by pressing
 * enter or clicking away), onDeselect() is called, which by default just sets
 * the display text to the edited text. Override this to e.g. validate the text
 * content.
 */
struct EditableTextBox : HoverableTextBox, TextField {

	bool isFocused = false;
	const static unsigned int defaultMaxTextLength = 4;
	unsigned int maxTextLength;

	EditableTextBox(): HoverableTextBox(), TextField() {
		maxTextLength = defaultMaxTextLength;
	}

	void draw(const DrawArgs &args) override;

	void onButton(const event::Button &e) override {
		TextField::onButton(e); // this handles consuming the event
	}

	void onDragHover(const event::DragHover &e) override {
		TextField::onDragHover(e);
	}

	void onHover(const event::Hover &e) override {
		TextField::onHover(e);
		HoverableTextBox::onHover(e);
	}

	void onHoverScroll(const event::HoverScroll &e) override {
		TextField::onHoverScroll(e);
	}

	void onAction(const event::Action &e) override;

	void onSelectText(const event::SelectText &e) override;
	void onSelectKey(const event::SelectKey &e) override;

	void onSelect(const event::Select &e) override {
		isFocused = true;
		e.consume(static_cast<TextField*>(this));
	}
	void onDeselect(const event::Deselect &e) override {
		isFocused = false;
		HoverableTextBox::setText(TextField::text);
		e.consume(NULL);
	}

	void step() override {
		TextField::step();
	}

};
