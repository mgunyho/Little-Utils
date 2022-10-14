#include "plugin.hpp"
#include <vector>
#include <map>

#define NUM_TELEPORT_INPUTS 8

struct TeleportInModule;

struct Teleport : Module {
	std::string label;
	float portConfigTime = 0.f;
	PortWidget *portWidgets[NUM_TELEPORT_INPUTS];
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

	inline void setPortLabel(int portNum, std::string label) {
		configInput(portNum, label);
	}

	inline void setPortWidget(int portNum, PortWidget *widget) {
		portWidgets[portNum] = widget;
	}
};

std::map<std::string, TeleportInModule*> Teleport::sources = {};
std::string Teleport::lastInsertedKey = "";
