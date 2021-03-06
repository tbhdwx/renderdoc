/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/


#include "common.h"
#include <stdarg.h>
#include <string.h>

#include "os/os_specific.h"
#include "common/threading.h"

#include "string_utils.h" 

#include <string>
using std::string;

template<> const wchar_t* pathSeparator() { return L"\\/"; }
template<> const char* pathSeparator() { return "\\/"; }

template<> const wchar_t* curdir() { return L"."; }
template<> const char* curdir() { return "."; }

std::wstring widen(std::string str)
{
	return std::wstring(str.begin(), str.end());
}

std::string narrow(std::wstring str)
{
	return std::string(str.begin(), str.end());
}

void rdcassert(const char *condition, const char *file, unsigned int line, const char *func)
{
	rdclog_int(RDCLog_Error, file, line, "Assertion failed: '%hs'", condition, file, line);
}

static wstring &logfile()
{
	static wstring fn;
	return fn;
}

const wchar_t *rdclog_getfilename()
{
	return logfile().c_str();
}

void rdclog_filename(const wchar_t *filename)
{
	logfile() = L"";
	if(filename && filename[0])
		logfile() = filename;
}

void rdclog_delete()
{
	if(!logfile().empty())
		FileIO::UnlinkFileW(logfile().c_str());
}

void rdclog_flush()
{
}

void rdclog_int(LogType type, const char *file, unsigned int line, const char *fmt, ...)
{
	if(type <= RDCLog_First || type >= RDCLog_NumTypes)
	{
		RDCFATAL("Unexpected log type");
		return;
	}
	
	va_list args;
	va_start(args, fmt);

	const char *name = "RENDERDOC: ";

	char timestamp[64] = {0};
#if defined(INCLUDE_TIMESTAMP_IN_LOG)
	StringFormat::sntimef(timestamp, 63, "[%H:%M:%S] ");
#endif
	
	char location[64] = {0};
#if defined(INCLUDE_LOCATION_IN_LOG)
	string loc;
	loc = basename(string(file));
	StringFormat::snprintf(location, 63, "% 20s(%4d) - ", loc.c_str(), line);
#endif

	const char *typestr[RDCLog_NumTypes] = {
		"Debug  ",
		"Log    ",
		"Warning",
		"Error  ",
		"Fatal  ",
	};
	
	const size_t outBufSize = 4*1024;
	char outputBuffer[outBufSize+1];
	outputBuffer[outBufSize] = 0;

	char *output = outputBuffer;
	size_t available = outBufSize;
	
	int numWritten = StringFormat::snprintf(output, available, "%hs %hs%hs%hs - ", name, timestamp, location, typestr[type]);

	if(numWritten < 0)
	{
		va_end(args);
		return;
	}

	output += numWritten;
	available -= numWritten;

	numWritten = StringFormat::vsnprintf(output, available, fmt, args);

	va_end(args);

	if(numWritten < 0)
		return;

	output += numWritten;
	available -= numWritten;

	if(available < 2)
		return;

	*output = '\n';
	*(output+1) = 0;
	
	{
		static Threading::CriticalSection lock;

		SCOPED_LOCK(lock);

#if defined(OUTPUT_LOG_TO_DEBUG_OUT)
		OSUtility::DebugOutputA(outputBuffer);
#endif
#if defined(OUTPUT_LOG_TO_STDOUT)
		fprintf(stdout, "%hs", outputBuffer); fflush(stdout);
#endif
#if defined(OUTPUT_LOG_TO_STDERR)
		fprintf(stderr, "%hs", outputBuffer); fflush(stderr);
#endif
#if defined(OUTPUT_LOG_TO_DISK)
		if(!logfile().empty())
		{
			FILE *f = FileIO::fopen(logfile().c_str(), L"a");
			if(f)
			{
				FileIO::fwrite(outputBuffer, 1, strlen(outputBuffer), f);
				FileIO::fclose(f);
			}
		}
#endif
	}
}
