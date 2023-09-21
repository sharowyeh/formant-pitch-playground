#pragma once
/* common functions for plot chart */

#include "../imgui/implot.h"
#include "../imgui/implot_internal.h"

#include <string>

namespace GLUI {

class PlotChartBase {
public:
	PlotChartBase(const char* surf) {
		surffix = surf;
	}
	/* return min and max value from frame buffer from offset during the length */
	void GetRangeMinMax(int offset, int length, int frames, int channels, float* buf, int ch, float* maximum, float* minimum) {
		// out of range
		if (offset >= frames || offset + length <= 0) {
			return;
		}
		// find min/max during length from buffer
		for (int i = 0; i < length; i++) {
			if (offset + i <= 0) continue;
			if (offset + i >= frames) break;
			// buffer payload just follow sndfile stride by channels
			size_t idx = (size_t)(offset + i) * channels + ch;
			if (*maximum < buf[idx]) *maximum = buf[idx];
			if (*minimum > buf[idx]) *minimum = buf[idx];
		}
	}
protected:
	/* surffix for GUI componment identification */
	const char* surffix;
	/* NOTE: it's also ok if using PushID/PopID for GUI controls identification, but I like preserving labels for rendering */
	std::string IdenticalLabel(const char* label, const char* id = nullptr) {
		std::string labelid;
		if (label) labelid.append(label);
		labelid.append("##");
		if (id) labelid.append(id);
		labelid.append(surffix);
		return labelid;
	}
}; // class

} // namespace GLUI