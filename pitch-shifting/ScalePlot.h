#pragma once
/*
 * class naming is for scale data but just implemented with combination plot charts in waveform graph
 */
#include "Waveform.h"
#include <map>

namespace GLUI {

	class ScalePlot : public PlotChartBase {
	public:
		ScalePlot(const char* surffix);

		/*
		 * set data ptr from stretcher/rubberband, given label can be data type + channel combination
		 * this function will append a new plot chart into the waveform graph
		 */
		void SetPlotInfo(const char* name, int type, int ch, int fftsize, double* dataptr, int size);
		/* become overload function for backward compatible lazy give plot chart a meaningful name */
		void SetPlotInfo(int type, int ch, int fftsize, double* dataptr, int size);

		void UpdatePlot() override;

	protected:
		virtual ~ScalePlot() { };

	private:
		std::string dataPlotTitle; // for FFT size identify, base on ctor surffix

		int fftSize;
		
		/* the scale data is channel individual, we need a structure pair the plot and data buffer */
		struct ScaleData {
			std::string label;
			int dataType;
			int channel;
			double* dataPtr;
			int bufSize;
			/* just init all fields by given parameters */
			ScaleData(const char* name, int type, int ch, double* dataptr = nullptr, int bufsize = 0) :
				label((name ? std::string(name) : "Unnamed")), 
				dataType(type), channel(ch), dataPtr(dataptr), bufSize(bufsize) { }

			bool operator==(const ScaleData& d) const {
				return dataType == d.dataType && channel == d.channel;
			};
			bool operator<(const ScaleData& d) const {
				return dataType < d.dataType || channel < d.channel;;
			}

		};

		/* label, plots */
		std::map<ScaleData, AmplitudeBuffer> plotBuffers;

		/* overload for map iteration */
		void UpdatePlot(const ScaleData& data, AmplitudeBuffer& plotBuffer);
	};

} // namespace GLUI