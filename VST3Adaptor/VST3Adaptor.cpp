#include "Processor.h"

using namespace gmpi;

struct VST3Adaptor final : public Processor
{
	BoolInPin pinPowerBypass;
	FloatInPin pinHostBPM;
	FloatInPin pinHostSP;
	BoolInPin pinHostTransport;
	IntInPin pinNumerator;
	IntInPin pinDenominator;
	FloatInPin pinHostBarStart;
	IntInPin pin;
	BlobInPin pineffectptr;
	MidiInPin pinParamBuss;
	AudioInPin pinSignalIn;
	AudioInPin pinSignalIn2;
	AudioOutPin pinSignalOut;
	AudioOutPin pinSignalOut2;

	VST3Adaptor()
	{
	}

	void subProcess( int sampleFrames )
	{
		// get pointers to in/output buffers.
		auto signalIn = getBuffer(pinSignalIn);
		auto signalIn2 = getBuffer(pinSignalIn2);
		auto signalOut = getBuffer(pinSignalOut);
		auto signalOut2 = getBuffer(pinSignalOut2);

		for( int s = sampleFrames; s > 0; --s )
		{
			// TODO: Signal processing goes here.

			// Increment buffer pointers.
			++signalIn;
			++signalIn2;
			++signalOut;
			++signalOut2;
		}
	}

	void onSetPins() override
	{
		// Check which pins are updated.
		if( pinPowerBypass.isUpdated() )
		{
		}
		if( pinHostBPM.isUpdated() )
		{
		}
		if( pinHostSP.isUpdated() )
		{
		}
		if( pinHostTransport.isUpdated() )
		{
		}
		if( pinNumerator.isUpdated() )
		{
		}
		if( pinDenominator.isUpdated() )
		{
		}
		if( pinHostBarStart.isUpdated() )
		{
		}
		if( pin.isUpdated() )
		{
		}
		if( pineffectptr.isUpdated() )
		{
		}
		if( pinSignalIn.isStreaming() )
		{
		}
		if( pinSignalIn2.isStreaming() )
		{
		}

		// Set state of output audio pins.
		pinSignalOut.setStreaming(true);
		pinSignalOut2.setStreaming(true);

		// Set processing method.
		setSubProcess(&VST3Adaptor::subProcess);
	}
};

namespace
{
auto r = Register<VST3Adaptor>::withXml(R"XML(
<?xml version="1.0" encoding="UTF-8"?>
<ShellPlugin/>
)XML");
}
