#pragma once
#include "Waveform.h"
#include <map>

namespace GLUI {

	class ScalePlot : public PlotChartBase {
	public:
		ScalePlot(const char* surffix);

		/* set data ptr from stretcher/rubberband, given label can be data type + channel combination */
		void SetScaleInfo(int type, int ch, int fftsize, double* dataptr, int size);

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
			ScaleData(int type, int ch, double* dataptr = nullptr, int bufsize = 0) :
				dataType(type), channel(ch), dataPtr(dataptr), bufSize(bufsize) {
				label = std::string("type:")
					.append(std::to_string(type))
					.append(" ch:")
					.append(std::to_string(ch));
			};

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