#include "Teleport.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

/////////////
// modules //
/////////////

//TODO: undo history for label/source change

struct TeleportInModule : Teleport {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1,
		INPUT_2,
		INPUT_3,
		INPUT_4,
		INPUT_5,
		INPUT_6,
		INPUT_7,
		INPUT_8,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Generate random, unique label for this teleport endpoint. Don't modify the sources map.
	std::string getLabel() {
		std::string l;
		do {
			l = randomString(EditableTextBox::defaultMaxTextLength);
		} while(sourceExists(l)); // if the label exists, regenerate
		return l;
	}

	// Change the label of this input, if the label doesn't exist already.
	// Return whether the label was updated.
	bool updateLabel(std::string lbl) {
		if(lbl.empty() || sourceExists(lbl)) {
			return false;
		}
		sources.erase(label); //TODO: mutex for this and erase() calls below?
		label = lbl;
		addSource(this);
		return true;
	}

	TeleportInModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_INPUTS == NUM_TELEPORT_INPUTS);
		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			configInput(i, string::f("Port %d", i + 1));
		}
		label = getLabel();
		addSource(this);
	}

	~TeleportInModule() {
		sources.erase(label);
	}

	// process() is not needed for a teleport source, values are read directly from inputs by teleport out

	json_t* dataToJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			// remove previous label randomly generated in constructor
			sources.erase(label);
			label = std::string(json_string_value(label_json));
			if(sourceExists(label)) {
				// Label already exists in sources, this means that dataFromJson()
				// was called due to duplication instead of loading from file.
				// Generate new label.
				label = getLabel();
			}
		} else {
			// label couldn't be read from json for some reason, generate new one
			label = getLabel();
		}

		addSource(this);

	}

};

struct TeleportOutModule : Teleport {

	bool sourceIsValid;

	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUT_5,
		OUTPUT_6,
		OUTPUT_7,
		OUTPUT_8,
		NUM_OUTPUTS
	};
	enum LightIds {
		OUTPUT_1_LIGHTG,
		OUTPUT_1_LIGHTR,
		OUTPUT_2_LIGHTG,
		OUTPUT_2_LIGHTR,
		OUTPUT_3_LIGHTG,
		OUTPUT_3_LIGHTR,
		OUTPUT_4_LIGHTG,
		OUTPUT_4_LIGHTR,
		OUTPUT_5_LIGHTG,
		OUTPUT_5_LIGHTR,
		OUTPUT_6_LIGHTG,
		OUTPUT_6_LIGHTR,
		OUTPUT_7_LIGHTG,
		OUTPUT_7_LIGHTR,
		OUTPUT_8_LIGHTG,
		OUTPUT_8_LIGHTR,

		NUM_LIGHTS
	};

	TeleportOutModule() : Teleport(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_OUTPUTS == NUM_TELEPORT_INPUTS);
		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			configOutput(i, string::f("Port %d", i + 1));
		}
		if(sources.size() > 0) {
			if(sourceExists(lastInsertedKey)) {
				label = lastInsertedKey;
			} else {
				// the lastly added input doesn't exist anymore,
				// pick first input in alphabetical order
				label = sources.begin()->first;
			}
			sourceIsValid = true;
		} else {
			label = "";
			sourceIsValid = false;
		}
	}

	void process(const ProcessArgs &args) override {

		if(sourceExists(label)){
			TeleportInModule *src = sources[label];
			for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
				Input input = src->inputs[TeleportInModule::INPUT_1 + i];
				const int channels = input.getChannels();
				outputs[OUTPUT_1 + i].setChannels(channels);
				for(int c = 0; c < channels; c++) {
					outputs[OUTPUT_1 + i].setVoltage(input.getVoltage(c), c);
				}
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness( input.isConnected());
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(!input.isConnected());
			}
			sourceIsValid = true;
		} else {
			for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
				outputs[i].setChannels(1);
				outputs[OUTPUT_1 + i].setVoltage(0.f);
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness(0.f);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(0.f);
			}
			sourceIsValid = false;
		}
	};

	json_t* dataToJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			label = json_string_value(label_json);
		}
	}
};

void Teleport::addSource(TeleportInModule *t) {
	std::string key = t->label;
	sources[key] = t; //TODO: mutex?
	lastInsertedKey = key;
}


////////////////////////////////////
// some teleport-specific widgets //
////////////////////////////////////

struct TeleportLabelDisplay {
	NVGcolor errorTextColor = nvgRGB(0xd8, 0x0, 0x0);
};

struct EditableTeleportLabelTextbox : EditableTextBox, TeleportLabelDisplay {
	TeleportInModule *module;
	std::string errorText = "!err";
	GUITimer errorDisplayTimer;
	float errorDuration = 3.f;

	EditableTeleportLabelTextbox(TeleportInModule *m): EditableTextBox() {
		assert(errorText.size() <= maxTextLength);
		module = m;
	}

	void onDeselect(const event::Deselect &e) override {
		if(module->updateLabel(TextField::text) || module->label.compare(TextField::text) == 0) {
			errorDisplayTimer.reset();
		} else {
			errorDisplayTimer.trigger(errorDuration);
		}

		isFocused = false;
		e.consume(NULL);
	}

	void step() override {
		EditableTextBox::step();
		if(!module) return;
		if(errorDisplayTimer.process()) {
			textColor = isFocused ? defaultTextColor : errorTextColor;
			HoverableTextBox::setText(errorText);
		} else {
			textColor = defaultTextColor;
			HoverableTextBox::setText(module->label);
			if(!isFocused) {
				TextField::setText(module->label);
			}
		}
	}

};

struct TeleportLabelMenuItem : MenuItem {
	TeleportOutModule *module;
	std::string label;
	void onAction(const event::Action &e) override {
		module->label = label;
	}
};

struct TeleportSourceSelectorTextBox : HoverableTextBox, TeleportLabelDisplay {
	TeleportOutModule *module;

	TeleportSourceSelectorTextBox() : HoverableTextBox() {}

	void onAction(const event::Action &e) override {
		// based on AudioDeviceChoice::onAction in src/app/AudioWidget.cpp
		Menu *menu = createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select source"));

		{
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = "";
			item->text = "(none)";
			item->rightText = CHECKMARK(module->label.empty());
			menu->addChild(item);
		}

		if(!module->sourceIsValid && !module->label.empty()) {
			// the source of the module doesn't exist, it shouldn't appear in sources, so display it as unavailable
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = module->label;
			item->text = module->label;
			item->text += " (missing)";
			item->rightText = CHECKMARK("true");
			menu->addChild(item);
		}

		auto src = module->sources;
		for(auto it = src.begin(); it != src.end(); it++) {
			TeleportLabelMenuItem *item = new TeleportLabelMenuItem();
			item->module = module;
			item->label = it->first;
			item->text = it->first;
			item->rightText = CHECKMARK(item->label == module->label);
			menu->addChild(item);
		}
	}

	void onButton(const event::Button &e) override {
		HoverableTextBox::onButton(e);
		bool l = e.button == GLFW_MOUSE_BUTTON_LEFT;
		bool r = e.button == GLFW_MOUSE_BUTTON_RIGHT;
		if(e.action == GLFW_RELEASE && (l || r)) {
			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}
	}

	void step() override {
		HoverableTextBox::step();
		if(!module) return;
		setText(module->label);
		textColor = module->sourceIsValid ? defaultTextColor : errorTextColor;
	}

};

// Custom PortWidget for teleport outputs, with custom tooltip behavior
struct TeleportOutPortWidget;

struct TeleportOutPortTooltip : ui::Tooltip {
	TeleportOutPortWidget* portWidget;

	void step() override {
		printf("TeleportOutPortTooltip::step()\n");
	}
};

struct TeleportOutPortWidget : PJ301MPort {
	TeleportOutPortTooltip* customTooltip = NULL;

	void createTooltip() {
		// same as PortWidget::craeteTooltip(), but internal->tooltip replaced with customTooltip
		if (!settings::tooltips)
			return;
		if (customTooltip)
			return;
		if (!module)
			return;

		TeleportOutPortTooltip* tooltip = new TeleportOutPortTooltip;
		tooltip->portWidget = this;
		APP->scene->addChild(tooltip);
		customTooltip = tooltip;
	}

	void destroyTooltip() {
		if(!customTooltip)
			return;
		APP->scene->removeChild(customTooltip);
		delete customTooltip;
		customTooltip = NULL;
	}

	// createTooltip cannot be overridden, so we have to manually reimplement all the methods that call createTooltip, because these can be overridden.
	void onEnter(const EnterEvent& e) override {
		createTooltip();
		// don't call superclass onEnter, it calls its own createTooltip()
	}

	void onLeave(const LeaveEvent& e) override {
		destroyTooltip();
		// don't call superclass onLeave, it calls its own destroyTooltip()
	}

	void onDragDrop(const DragDropEvent& e) override {
		if(e.origin == this) {
			createTooltip();
		}
		DragDropEvent e2 = e;
		e2.origin = NULL;
		PJ301MPort::onDragDrop(e2);
	}

	void onDragEnter(const DragEnterEvent& e) override {
		PortWidget* pw = dynamic_cast<PortWidget*>(e.origin);
		if (pw) {
			createTooltip();
		}
		DragEnterEvent e2 = e;
		e2.origin = NULL;
		PJ301MPort::onDragEnter(e2);
	}

	void onDragLeave(const DragLeaveEvent& e) override {
		destroyTooltip();
		PJ301MPort::onDragLeave(e);
	}

	~TeleportOutPortWidget() {
		destroyTooltip();
	}

};



////////////////////
// module widgets //
////////////////////

struct TeleportModuleWidget : ModuleWidget {
	HoverableTextBox *labelDisplay;
	Teleport *module;

	virtual void addLabelDisplay(HoverableTextBox *disp) {
		disp->font_size = 14;
		disp->box.size = Vec(30, 14);
		disp->textOffset.x = disp->box.size.x * 0.5f;
		disp->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.0f);
		labelDisplay = disp;
		addChild(labelDisplay);
	}

	float getPortYCoord(int i) {
		return 57.f + 37.f * i;
	}

	TeleportModuleWidget(Teleport *module, std::string panelFilename) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, panelFilename)));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}
};


struct TeleportInModuleWidget : TeleportModuleWidget {

	TeleportInModuleWidget(TeleportInModule *module) : TeleportModuleWidget(module, "res/TeleportIn.svg") {
		addLabelDisplay(new EditableTeleportLabelTextbox(module));
		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			addInput(createInputCentered<PJ301MPort>(Vec(22.5, getPortYCoord(i)), module, TeleportInModule::INPUT_1 + i));
		}
	}

};


struct TeleportOutModuleWidget : TeleportModuleWidget {
	TeleportSourceSelectorTextBox *labelDisplay;

	TeleportOutModuleWidget(TeleportOutModule *module) : TeleportModuleWidget(module, "res/TeleportOut.svg") {
		labelDisplay = new TeleportSourceSelectorTextBox();
		labelDisplay->module = module;
		addLabelDisplay(labelDisplay);

		for(int i = 0; i < NUM_TELEPORT_INPUTS; i++) {
			float y = getPortYCoord(i);
			addOutput(createOutputCentered<TeleportOutPortWidget>(Vec(22.5, y), module, TeleportOutModule::OUTPUT_1 + i));
			addChild(createTinyLightForPort<GreenRedLight>(Vec(22.5, y), module, TeleportOutModule::OUTPUT_1_LIGHTG + 2*i));
		}
	}

};


Model *modelTeleportInModule = createModel<TeleportInModule, TeleportInModuleWidget>("TeleportIn");
Model *modelTeleportOutModule = createModel<TeleportOutModule, TeleportOutModuleWidget>("TeleportOut");
