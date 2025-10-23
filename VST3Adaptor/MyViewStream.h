#pragma once

#include "base/source/fobject.h"
#include "pluginterfaces/base/ibstream.h"

// for writing.
class MyBufferStream : public Steinberg::FObject, public Steinberg::IBStream
{
public:
	MyBufferStream() {}
	virtual ~MyBufferStream() {}

	//---from IBStream------------------
	Steinberg::tresult PLUGIN_API read (void* /*buffer*/, Steinberg::int32 /*numBytes*/, Steinberg::int32* /*numBytesRead*/ = nullptr) SMTG_OVERRIDE
	{
		return 0;
	}
	Steinberg::tresult PLUGIN_API write(void* buffer, Steinberg::int32 numBytes, Steinberg::int32* numBytesWritten = nullptr) SMTG_OVERRIDE
	{
		if(numBytesWritten)
		{
			*numBytesWritten = numBytes;
		}

		writePos_ += numBytes;
		buffer_.insert(buffer_.end(), (uint8_t*)buffer, ((uint8_t*)buffer) + numBytes);
		return 0;
	}
	Steinberg::tresult PLUGIN_API seek(Steinberg::int64 /*pos*/, Steinberg::int32 /*mode*/, Steinberg::int64* /*result*/ = nullptr) SMTG_OVERRIDE
	{
		return 0;
	}
	Steinberg::tresult PLUGIN_API tell(Steinberg::int64* /*pos*/) SMTG_OVERRIDE
	{
		return 0;
	}

	std::vector<uint8_t> buffer_;
	int writePos_ = {};

	//---Interface---------
	OBJ_METHODS (MyBufferStream, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::IBStream)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

// for reading.
class MyViewStream : public Steinberg::FObject, public Steinberg::IBStream
{
public:
	MyViewStream(uint8_t* buffer, int32_t size) : buffer_(buffer), size_(size) {}
	virtual ~MyViewStream() {}

	//---from IBStream------------------
	Steinberg::tresult PLUGIN_API read (void* buffer, Steinberg::int32 numBytes, Steinberg::int32* numBytesRead = nullptr) SMTG_OVERRIDE
	{
		const auto remaining = size_ - readPos_;
		numBytes = (std::min)((int)numBytes, remaining);
		if(numBytesRead)
		{
			*numBytesRead = numBytes;
		}
		memcpy(buffer, buffer_ + readPos_, numBytes);
		readPos_ += numBytes;

		return 0;
	}
	Steinberg::tresult PLUGIN_API write(void* /*buffer*/, Steinberg::int32 /*numBytes*/, Steinberg::int32* /*numBytesWritten*/ = nullptr) SMTG_OVERRIDE
	{
		return 0;
	}
	Steinberg::tresult PLUGIN_API seek(Steinberg::int64 pos, Steinberg::int32 mode, Steinberg::int64* result = nullptr) SMTG_OVERRIDE
	{
		switch(mode)
		{
		case kIBSeekSet:
			readPos_ = (std::min)((int)pos, size_);
			break;

		case kIBSeekCur:
			{
				readPos_ += pos;
				readPos_ = (std::min)(readPos_, size_);
				readPos_ = (std::max)(readPos_, 0);
			}
			break;

		case kIBSeekEnd:
			readPos_ = size_;
			break;

		default:
			return 1;
			break;
		}

		if (result)
		{
			*result = readPos_;
		}

		return 0;
	}

	Steinberg::tresult PLUGIN_API tell(Steinberg::int64* pos) SMTG_OVERRIDE
	{
		if(pos)
		{
			*pos = readPos_;
		}
		return 0;
	}

	const uint8_t* buffer_ = {};
	int readPos_ = {};
	int size_ = {};

	//---Interface---------
	OBJ_METHODS (MyBufferStream, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::IBStream)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};
