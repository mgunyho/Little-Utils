#include "plugin.hpp"
#include <vector>
#include <map>

#define NUM_TELEPORT_INPUTS 8

struct TeleportInModule;

struct Teleport : Module {
	std::string label;
	Teleport(int numParams, int numInputs, int numOutputs, int numLights = 0) {
		config(numParams, numInputs, numOutputs, numLights);
	}

	// This static map is used for keeping track of all existing Teleport instances.
	// We're using a map instead of a set because it's easier to search.
	static std::map<std::string, TeleportInModule*> sources;
	static std::string lastInsertedKey; // this is used to assign the label of an output initially

	void addSource(TeleportInModule *t);

	inline bool sourceExists(std::string lbl) {
		return sources.find(lbl) != sources.end();
	}
};


// these have to be forward-declared here to make the implementation of step() possible, see cpp for details
struct TeleportOutPortWidget;
struct TeleportOutPortTooltip : ui::Tooltip {
	TeleportOutPortWidget* portWidget;
	void step() override;
};


std::map<std::string, TeleportInModule*> Teleport::sources = {};
std::string Teleport::lastInsertedKey = "";
