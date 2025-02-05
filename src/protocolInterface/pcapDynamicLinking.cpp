/*
* Copyright (C) 2016-2022, L-Acoustics and its contributors

* This file is part of LA_avdecc.

* LA_avdecc is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* LA_avdecc is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with LA_avdecc.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
* @file pcapDynamicLinking.cpp
* @author Christophe Calmejane
*/

#include "la/avdecc/internals/logItems.hpp"

#include "pcapInterface.hpp"

#include <cassert>
#include <string>

#ifdef _WIN32
#	include <windows.h>
#	define PCAP_LIBRARY "wpcap.dll"
#	define DL_HANDLE HMODULE
#	define DL_OPEN(x) LoadLibrary(x)
#	define DL_CLOSE(x) FreeLibrary(x)
#	define DL_SYM(x, y) GetProcAddress((x), (y))
#	define DL_ERROR() la_dlerror()
#else
#	include <dlfcn.h>
#	include <signal.h>
#	ifdef __APPLE__
#		define PCAP_LIBRARY "/usr/lib/libpcap.dylib" /* Due to macOS hardened runtime, we have to specify the absolute path for the pcap library */
#	else /* !__APPLE__ */
#		define PCAP_LIBRARY "libpcap.so"
#	endif /* __APPLE__ */
#	define DL_HANDLE void*
#	define DL_OPEN(x) dlopen((x), RTLD_LAZY)
#	define DL_CLOSE(x) dlclose(x)
#	define DL_SYM(x, y) dlsym((x), (y))
#	define DL_ERROR() dlerror()
#endif

#ifdef _WIN32
static std::string wideCharToUTF8(PWCHAR const wide) noexcept
{
	// All APIs calling this method have to provide a NULL-terminated PWCHAR
	auto const wideLength = wcsnlen_s(wide, 1024); // Compute the size, in characters, of the wide string

	if (wideLength != 0)
	{
		auto const sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(wideLength), nullptr, 0, nullptr, nullptr);
		auto result = std::string(static_cast<std::string::size_type>(sizeNeeded), std::string::value_type{ 0 }); // Brace-initialization constructor prevents the use of {}

		if (WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(wideLength), result.data(), sizeNeeded, nullptr, nullptr) > 0)
		{
			return result;
		}
	}

	return {};
}

static std::string la_dlerror()
{
	constexpr auto ErrorMessageMaxLength = 512;
	WCHAR errMessage[ErrorMessageMaxLength];

	auto const errorCode = GetLastError();
	if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errMessage, ErrorMessageMaxLength, NULL) == 0)
	{
		return std::to_string(errorCode);
	}

	return wideCharToUTF8(errMessage);
}
#endif // _WIN32

namespace la
{
namespace avdecc
{
namespace protocol
{
using open_live_t = pcap_t* (*)(const char*, int, int, int, char*);
using fileno_t = int (*)(pcap_t*);
using close_t = void (*)(pcap_t*);
using compile_t = int (*)(pcap_t*, bpf_program*, const char*, int, bpf_u_int32);
using setfilter_t = int (*)(pcap_t*, bpf_program*);
using freecode_t = void (*)(bpf_program*);
using next_ex_t = int (*)(pcap_t*, pcap_pkthdr**, const u_char**);
using loop_t = int (*)(pcap_t*, int, pcap_handler, u_char*);
using breakloop_t = void (*)(pcap_t*);
using sendpacket_t = int (*)(pcap_t*, const u_char*, int);

struct PcapInterface::pImpl
{
	DL_HANDLE libraryHandle{ nullptr };
	open_live_t open_live_ptr{ nullptr };
	fileno_t fileno_ptr{ nullptr };
	close_t close_ptr{ nullptr };
	compile_t compile_ptr{ nullptr };
	setfilter_t setfilter_ptr{ nullptr };
	freecode_t freecode_ptr{ nullptr };
	next_ex_t next_ex_ptr{ nullptr };
	loop_t loop_ptr{ nullptr };
	breakloop_t breakloop_ptr{ nullptr };
	sendpacket_t sendpacket_ptr{ nullptr };
};

PcapInterface::PcapInterface()
	: _pImpl(std::make_unique<pImpl>())
{
	bool foundAllFunctions{ false };
	if (auto const handle = DL_OPEN(PCAP_LIBRARY))
	{
		std::string version{};
		using lib_version_t = const char* (*)();
		if (auto const lib_version_ptr = reinterpret_cast<lib_version_t>(DL_SYM(handle, "pcap_lib_version")))
		{
			version = lib_version_ptr();

			_pImpl->open_live_ptr = reinterpret_cast<open_live_t>(DL_SYM(handle, "pcap_open_live"));
			_pImpl->fileno_ptr = reinterpret_cast<fileno_t>(DL_SYM(handle, "pcap_fileno"));
			_pImpl->close_ptr = reinterpret_cast<close_t>(DL_SYM(handle, "pcap_close"));
			_pImpl->compile_ptr = reinterpret_cast<compile_t>(DL_SYM(handle, "pcap_compile"));
			_pImpl->setfilter_ptr = reinterpret_cast<setfilter_t>(DL_SYM(handle, "pcap_setfilter"));
			_pImpl->freecode_ptr = reinterpret_cast<freecode_t>(DL_SYM(handle, "pcap_freecode"));
			_pImpl->next_ex_ptr = reinterpret_cast<next_ex_t>(DL_SYM(handle, "pcap_next_ex"));
			_pImpl->loop_ptr = reinterpret_cast<loop_t>(DL_SYM(handle, "pcap_loop"));
			_pImpl->breakloop_ptr = reinterpret_cast<breakloop_t>(DL_SYM(handle, "pcap_breakloop"));
			_pImpl->sendpacket_ptr = reinterpret_cast<sendpacket_t>(DL_SYM(handle, "pcap_sendpacket"));

			foundAllFunctions = _pImpl->open_live_ptr && _pImpl->fileno_ptr && _pImpl->close_ptr && _pImpl->compile_ptr && _pImpl->setfilter_ptr && _pImpl->freecode_ptr && _pImpl->next_ex_ptr && _pImpl->loop_ptr && _pImpl->breakloop_ptr && _pImpl->sendpacket_ptr;
		}

		if (foundAllFunctions)
		{
			auto const item = la::avdecc::logger::LogItemGeneric{ "Using " PCAP_LIBRARY ": " + version };
			la::avdecc::logger::Logger::getInstance().logItem(la::avdecc::logger::Level::Info, &item);
			_pImpl->libraryHandle = handle;
		}
		else
		{
			auto const item = la::avdecc::logger::LogItemGeneric{ "Cannot find all the required functions in " PCAP_LIBRARY };
			la::avdecc::logger::Logger::getInstance().logItem(la::avdecc::logger::Level::Error, &item);
			DL_CLOSE(handle);
		}
	}
	else
	{
		auto const item = la::avdecc::logger::LogItemGeneric{ std::string("Cannot load " PCAP_LIBRARY ": ") + std::string(DL_ERROR()) };
		la::avdecc::logger::Logger::getInstance().logItem(la::avdecc::logger::Level::Error, &item);
	}
}

PcapInterface::~PcapInterface()
{
	assert(_pImpl != nullptr);
	if (_pImpl->libraryHandle != nullptr)
	{
		DL_CLOSE(_pImpl->libraryHandle);
	}
}

bool PcapInterface::is_available() const
{
	assert(_pImpl != nullptr);
	return (_pImpl->libraryHandle != nullptr);
}

pcap_t* PcapInterface::open_live(const char* device, int snaplen, int promisc, int to_ms, char* ebuf) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->open_live_ptr != nullptr));
	return _pImpl->open_live_ptr(device, snaplen, promisc, to_ms, ebuf);
}

int PcapInterface::fileno(pcap_t* p) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->fileno_ptr != nullptr));
	return _pImpl->fileno_ptr(p);
}

void PcapInterface::close(pcap_t* p) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->close_ptr != nullptr));
	_pImpl->close_ptr(p);
}

int PcapInterface::compile(pcap_t* p, struct bpf_program* fp, const char* str, int optimize, bpf_u_int32 netmask) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->compile_ptr != nullptr));
	return _pImpl->compile_ptr(p, fp, str, optimize, netmask);
}

int PcapInterface::setfilter(pcap_t* p, bpf_program* fp) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->setfilter_ptr != nullptr));
	return _pImpl->setfilter_ptr(p, fp);
}

void PcapInterface::freecode(bpf_program* fp) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->freecode_ptr != nullptr));
	_pImpl->freecode_ptr(fp);
}

int PcapInterface::next_ex(pcap_t* p, struct pcap_pkthdr** pkt_header, const u_char** pkt_data) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->next_ex_ptr != nullptr));
	return _pImpl->next_ex_ptr(p, pkt_header, pkt_data);
}

int PcapInterface::loop(pcap_t* p, int cnt, pcap_handler callback, u_char* user) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->loop_ptr != nullptr));
	return _pImpl->loop_ptr(p, cnt, callback, user);
}

void PcapInterface::breakloop(pcap_t* p) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->breakloop_ptr != nullptr));
	_pImpl->breakloop_ptr(p);
}

int PcapInterface::sendpacket(pcap_t* p, const u_char* buf, int size) const
{
	assert((_pImpl != nullptr) && (_pImpl->libraryHandle != nullptr) && (_pImpl->sendpacket_ptr != nullptr));
	return _pImpl->sendpacket_ptr(p, buf, size);
}

} // namespace protocol
} // namespace avdecc
} // namespace la
