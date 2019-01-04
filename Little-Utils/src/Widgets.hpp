#include "rack.hpp"
#include "LittleUtils.hpp"

#include <iostream>

struct TextBox : TransparentWidget {
	// Kinda like TextField except not editable. Using Roboto Mono Bold font,
	// numbers look okay.
	// based on LedDisplayChoice
	std::string text;
	std::shared_ptr<Font> font;
	float font_size;
	float letter_spacing;
	Vec textOffset;
	NVGcolor textColor;
	NVGcolor backgroundColor;

	//TODO: create<...>() thing with position as argument?
	TextBox() {
		font = Font::load(assetPlugin(plugin, "res/RobotoMono-Bold.ttf"));
		textColor = nvgRGB(0x23, 0x23, 0x23);
		backgroundColor = nvgRGB(0xc8, 0xc8, 0xc8);
		box.size = Vec(30, 18);
		// size 20 with spacing -2 will fit 3 characters on a 30px box with Roboto mono
		font_size = 20;
		letter_spacing = 0.f;
		textOffset = Vec(box.size.x * 0.5f, 0.f);
	}

	virtual void setText(std::string s) { text = s; }

	virtual void draw(NVGcontext *vg) override {
		// based on LedDisplayChoice::draw() in Rack/src/app/LedDisplay.cpp
		nvgScissor(vg, 0, 0, box.size.x, box.size.y);

		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.0);
		nvgFillColor(vg, backgroundColor);
		nvgFill(vg);

		if (font->handle >= 0) {
			nvgFillColor(vg, textColor);
			nvgFontFaceId(vg, font->handle);

			nvgFontSize(vg, font_size);
			nvgTextLetterSpacing(vg, letter_spacing);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x, textOffset.y, text.c_str(), NULL);
		}

		nvgResetScissor(vg);
	}
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

	void onMouseMove(EventMouseMove &e) override {
		// onMouseMove from OpaqueWidget, needed to catch hover events
		Widget::onMouseMove(e);
		if(!e.target) {
			e.target = this;
		}
		e.consumed = true;
	}

	void onMouseEnter(EventMouseEnter &e) override {
		state = BND_HOVER;
	}
	void onMouseLeave(EventMouseLeave &e) override {
		state = BND_DEFAULT;
	}

	void draw(NVGcontext *vg) override {
		backgroundColor = state == BND_HOVER ? hoverColor : defaultColor;
		TextBox::draw(vg);
	}
};

/*
 * Basically a custom version of TextField.
 * TextField::text holds the string being edited and HoverableTextBox::text
 * holds the string being displayed. When the user stops editing (by pressing
 * enter or clicking away), onDefocus() is called, which by default just sets
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

	void draw(NVGcontext *vg) override;

	void onMouseDown(EventMouseDown &e) override {
		TextField::onMouseDown(e);
	}

	void onMouseUp(EventMouseUp &e) override {
		TextField::onMouseUp(e);
	}

	void onMouseMove(EventMouseMove &e) override {
		TextField::onMouseMove(e);
	}

	void onScroll(EventScroll &e) override {
		TextField::onScroll(e);
	}

	void onAction(EventAction &e) override;

	void onText(EventText &e) override {
		if(TextField::text.size() < maxTextLength) {
			TextField::onText(e);
		}
	}

	void onKey(EventKey &e) override;

	virtual void onDefocus() {
		HoverableTextBox::setText(TextField::text);
	}

	void step() override {
		bool wasFocused = isFocused;
		isFocused = gFocusedWidget == this; // should maybe use onFocus instead
		if(wasFocused && !isFocused) {
			// hacky
			onDefocus();
		}
	}

};

struct ToggleLEDButton : SVGSwitch, ToggleSwitch {
	BNDwidgetState state = BND_DEFAULT;
	NVGcolor defaultColor;
	NVGcolor hoverColor;

	ToggleLEDButton() {
		addFrame(SVG::load(assetGlobal("res/ComponentLibrary/LEDButton.svg")));
	}
};
