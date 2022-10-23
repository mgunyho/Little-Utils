#include "plugin.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#include <algorithm> // std::replace

//TODO: when cv has been recently adjusted, tweaking the main knob should switch the display to the non-cv view.

const float MIN_EXPONENT = -3.0f;
const float MAX_EXPONENT = 1.0f;

// based on PulseGeneraotr in include/util/digital.hpp
struct CustomPulseGenerator {
	float time;
	float triggerDuration;
	bool finished; // the output is the inverse of this

	CustomPulseGenerator() {
		reset();
	}
	/** Immediately resets the state to LOW */
	void reset() {
		time = 0.f;
		triggerDuration = 0.f;
		finished = true;
	}
	/** Advances the state by `deltaTime`. Returns whether the pulse is in the HIGH state. */
	bool process(float deltaTime) {
		time += deltaTime;
		if(!finished) finished = time >= triggerDuration;
		return !finished;
	}
	/** Begins a trigger with the given `triggerDuration`. */
	void trigger(float triggerDuration) {
		// retrigger even with a shorter duration
		time = 0.f;
		finished = false;
		this->triggerDuration = triggerDuration;
	}
};


// the module is called PulseGenModule to avoid confusion with dsp::PulseGenerator
struct PulseGenModule : Module {
	enum ParamIds {
		GATE_LENGTH_PARAM,
		CV_AMT_PARAM,
		LIN_LOG_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		GATE_LENGTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		FINISH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATE_LIGHT,
		FINISH_LIGHT,
		NUM_LIGHTS
	};

	// not using SIMD here, doesn't seem to affect performance much
	dsp::SchmittTrigger inputTrigger[MAX_POLY_CHANNELS], finishTrigger[MAX_POLY_CHANNELS];
	CustomPulseGenerator gateGenerator[MAX_POLY_CHANNELS], finishTriggerGenerator[MAX_POLY_CHANNELS];
	float gate_base_duration = 0.5f; // gate duration without CV
	float gate_duration;
	bool realtimeUpdate = true; // whether to display gate_duration or gate_base_duration
	float cv_scale = 0.f; // cv_scale = +- 1 -> 10V CV changes duration by +-10s
	bool allowRetrigger = true; // whether to allow the pulse to be retriggered if it is already outputting

	PulseGenModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		//TODO: consider overriding ParamQuantity::getDisplayValueString
		configParam(PulseGenModule::GATE_LENGTH_PARAM, 0.f, 10.f,
					// 0.5s in log scale
					//rescale(-0.30103f, MIN_EXPONENT, MAX_EXPONENT, 0.f,10.f)
					5.f // 0.1s in log mode, 5s in lin mode
					, "Pulse duration");
		configSwitch(PulseGenModule::LIN_LOG_MODE_PARAM, 0.f, 1.f, 1.f, "Duration mod mode", {"Linear", "Logarithmic"});
		configParam(PulseGenModule::CV_AMT_PARAM, -1.f, 1.f, 0.f, "CV amount");

		configInput(TRIG_INPUT, "Trigger");
		configInput(GATE_LENGTH_INPUT, "Gate length CV modulation");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(FINISH_OUTPUT, "Finish trigger");

		gate_duration = gate_base_duration;
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *root = json_object();
		json_object_set_new(root, "realtimeUpdate", json_boolean(realtimeUpdate));
		json_object_set_new(root, "allowRetrigger", json_boolean(allowRetrigger));
		return root;
	}

	void dataFromJson(json_t *root) override {
		json_t *realtimeUpdate_J = json_object_get(root, "realtimeUpdate");
		json_t *allowRetrigger_J = json_object_get(root, "allowRetrigger");
		if(realtimeUpdate_J) {
			realtimeUpdate = json_boolean_value(realtimeUpdate_J);
		}
		if(allowRetrigger_J) {
			allowRetrigger = json_boolean_value(allowRetrigger_J);
		}
	}

};

void PulseGenModule::process(const ProcessArgs &args) {
	float deltaTime = args.sampleTime;
	const int channels = inputs[TRIG_INPUT].getChannels();

	// handle duration knob and CV
	float knob_value = params[GATE_LENGTH_PARAM].getValue();
	float cv_amt = params[CV_AMT_PARAM].getValue();
	float cv_voltage = inputs[GATE_LENGTH_INPUT].getVoltage();

	if(params[LIN_LOG_MODE_PARAM].getValue() < 0.5f) {
		// linear mode
		cv_scale = cv_amt;
		gate_base_duration = knob_value;
	} else {
		// logarithmic mode
		float exponent = rescale(knob_value,
				0.f, 10.f, MIN_EXPONENT, MAX_EXPONENT);

		float cv_exponent = rescale(fabs(cv_amt), 0.f, 1.f,
				MIN_EXPONENT, MAX_EXPONENT);

		// decrease exponent by one so that 10V maps to 1.0 (100%) CV.
		cv_scale = powf(10.0f, cv_exponent - 1.f) * signum(cv_amt); // take sign into account

		gate_base_duration = powf(10.0f, exponent);
	}
	//TODO: make duration polyphonic? how to display it?
	gate_duration = clamp(gate_base_duration + cv_voltage * cv_scale, 0.f, 10.f);

	for(int c = 0; c < channels; c++) {

		bool triggered = inputTrigger[c].process(rescale(inputs[TRIG_INPUT].getVoltage(c),
					0.1f, 2.f, 0.f, 1.f));

		if(triggered && gate_duration > 0.f) {
			if(gateGenerator[c].finished || allowRetrigger) {
				gateGenerator[c].trigger(gate_duration);
			}
		}

		// update trigger duration even in the middle of a trigger
		gateGenerator[c].triggerDuration = gate_duration;

		bool gate = gateGenerator[c].process(deltaTime);

		if(finishTrigger[c].process(gate ? 0.f : 1.f)) {
			finishTriggerGenerator[c].trigger(1.e-3f);
		}

		float gate_v = gate ? 10.0f : 0.0f;
		float finish_v = finishTriggerGenerator[c].process(deltaTime) ? 10.f : 0.f;
		outputs[GATE_OUTPUT].setVoltage(gate_v, c);
		outputs[FINISH_OUTPUT].setVoltage(finish_v, c);

		//TODO: fix lights for polyphonic mode...
		lights[GATE_LIGHT].setSmoothBrightness(gate_v, deltaTime);
		lights[FINISH_LIGHT].setSmoothBrightness(finish_v, deltaTime);

	}

	outputs[GATE_OUTPUT].setChannels(channels);
	outputs[FINISH_OUTPUT].setChannels(channels);

}

// TextBox defined in ./Widgets.hpp
struct MsDisplayWidget : TextBox {
	PulseGenModule *module;
	bool msLabelStatus = false; // 0 = 'ms', 1 = 's'
	bool cvLabelStatus = false; // whether to show 'cv'
	float previous_displayed_value = -1.f;
	float cvDisplayTime = 2.f;

	GUITimer cvDisplayTimer;

	MsDisplayWidget(PulseGenModule *m) : TextBox() {
		module = m;
		box.size = Vec(30, 27);
		letter_spacing = -2.0f;
	}

	void updateDisplayValue(float v) {
		// only update/do string::f if value is changed
		if(v != previous_displayed_value) {
			std::string s;
			previous_displayed_value = v;
			if(v <= 0.0995) {
				v *= 1e3f;
				s = string::f("%#.2g", v < 1.f ? 0.f : v);
				msLabelStatus = false;
			} else {
				s = string::f("%#.2g", v);
				msLabelStatus = true;
				if(s.at(0) == '0') s.erase(0, 1);
			}
			// hacky way to make monospace font prettier
			std::replace(s.begin(), s.end(), '0', 'O');
			setText(s);
		}
	}

	void draw(const DrawArgs &args) override {
		TextBox::draw(args);
		auto vg = args.vg;
		nvgScissor(vg, 0, 0, box.size.x, box.size.y);

		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontPath));

		if(font && font->handle >= 0) {
			nvgFillColor(vg, textColor);
			nvgFontFaceId(vg, font->handle);

			// draw 'ms' or 's' on bottom, depending on msLabelStatus
			nvgFontSize(vg, 12);
			nvgTextLetterSpacing(vg, 0.f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(vg, textOffset.x + 2, textOffset.y + 14,
					msLabelStatus ? " s" : "ms", NULL);

			if(cvLabelStatus) {
				nvgText(vg, 3, textOffset.y + 14, "cv", NULL);
			}
		}

		nvgResetScissor(vg);
	}

	void triggerCVDisplay() {
		cvDisplayTimer.trigger(cvDisplayTime);
	}

	void step() override {
		TextBox::step();
		cvLabelStatus = cvDisplayTimer.process();
		if(module) {
			if(cvLabelStatus){
				updateDisplayValue(fabs(module->cv_scale * 10.f));
			}else{
				//TODO: disable realtimeUpdate if main knob is being turned
				updateDisplayValue(module->realtimeUpdate ? module->gate_duration : module->gate_base_duration);
			}
		} else {
			updateDisplayValue(0.f);
		}
	}

};

struct CustomTrimpot : Trimpot {
	MsDisplayWidget *display;
	CustomTrimpot(): Trimpot() {};

	void onDragMove(const event::DragMove &e) override {
		Trimpot::onDragMove(e);
		display->triggerCVDisplay();
	}
};

// generic menu item that toggles a boolean attribute provided in the constructor
struct PulseGeneratorToggleMenuItem: MenuItem {
	bool& attr;
	PulseGeneratorToggleMenuItem(bool& pAttr) : MenuItem(), attr(pAttr) {
		rightText = CHECKMARK(attr);
	};
	void onAction(const event::Action &e) override {
		attr = !attr;
	}
};

struct PulseGeneratorWidget : ModuleWidget {
	PulseGenModule *module;
	MsDisplayWidget *msDisplay;

	PulseGeneratorWidget(PulseGenModule *module) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PulseGenerator.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(Vec(22.5, 37.5), module, PulseGenModule::GATE_LENGTH_PARAM));

		addParam(createParam<CKSS>(Vec(7.5, 60), module, PulseGenModule::LIN_LOG_MODE_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 151), module, PulseGenModule::GATE_LENGTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(22.5, 192), module, PulseGenModule::TRIG_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 240), module, PulseGenModule::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.5, 288), module, PulseGenModule::FINISH_OUTPUT));

		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 240), module, PulseGenModule::GATE_LIGHT));
		addChild(createTinyLightForPort<GreenLight>(Vec(22.5, 288), module, PulseGenModule::FINISH_LIGHT));

		msDisplay = new MsDisplayWidget(module);
		msDisplay->box.pos = Vec(7.5, 308);
		addChild(msDisplay);

		auto cvKnob = createParamCentered<CustomTrimpot>(Vec(22.5, 110), module, PulseGenModule::CV_AMT_PARAM);
		cvKnob->display = msDisplay;
		addParam(cvKnob);

	}

	void appendContextMenu(ui::Menu* menu) override {

		menu->addChild(new MenuLabel());

		{
			auto *toggleItem = new PulseGeneratorToggleMenuItem(this->module->realtimeUpdate);
			toggleItem->text = "Update display in real time";
			menu->addChild(toggleItem);
		}
		{
			auto *toggleItem = new PulseGeneratorToggleMenuItem(this->module->allowRetrigger);
			toggleItem->text = "Allow retrigger while gate is on";
			//toggleItem->rightText = CHECKMARK(toggleItem->attr);
			menu->addChild(toggleItem);
		}

	}

};


Model *modelPulseGenerator = createModel<PulseGenModule, PulseGeneratorWidget>("PulseGenerator");
