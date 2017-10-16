/*
* Copyright (C) 2016-2017, L-Acoustics and its contributors

* This file is part of LA_avdecc.

* LA_avdecc is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* LA_avdecc is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with LA_avdecc.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
* @file logger.cpp
* @author Christophe Calmejane
*/

#include "la/avdecc/logger.hpp"
#include <vector>
#include <mutex>
#include <algorithm>
#include <cassert>

namespace la
{
namespace avdecc
{

class LoggerImpl final : public Logger
{
public:
	virtual void registerObserver(Observer* const observer) noexcept override
	{
		std::lock_guard<decltype(_lock)> const lg(_lock);
		_observers.push_back(observer);
	}

	virtual void unregisterObserver(Observer* const observer) noexcept override
	{
		std::lock_guard<decltype(_lock)> const lg(_lock);
		std::remove_if(std::begin(_observers), std::end(_observers), [observer](Observer* const o)
		{
			return o == observer;
		});
	}

	virtual void log(Layer const layer, Level const level, std::string const& message) noexcept override
	{
		// Check if message level is currently active
		if (level < _level)
			return;

		std::lock_guard<decltype(_lock)> const lg(_lock);
		for (auto const& o : _observers)
			o->onLog(layer, level, message);
	}

	virtual void setLevel(Level const level) noexcept override
	{
		_level = level;
#ifndef DEBUG
		// In release, we don't want Trace nor Debug levels, setting to next possible Level (Info)
		if (_level == Level::Trace || _level == Level::Debug)
			_level = Level::Info;
#endif // !DEBUG
	}

	virtual Level getLevel() const noexcept override
	{
		return _level;
	}

	virtual std::string layerToString(Layer const layer) const noexcept override
	{
		if (layer < Layer::FirstUserLayer)
		{
			switch (layer)
			{
				case Layer::Protocol:
					return "Protocol";
				case Layer::Controller:
					return "Controller";
				case Layer::Talker:
					return "Talker";
				case Layer::Listener:
					return "Listener";
				default:
					assert(false && "Layer not handled");
			}
		}
		return "Layer" + std::to_string(std::underlying_type_t<Layer>(layer));
	}

	virtual std::string levelToString(Level const level) const noexcept override
	{
		switch (level)
		{
			case Level::Trace:
				return "Trace";
			case Level::Debug:
				return "Debug";
			case Level::Info:
				return "Info";
			case Level::Warn:
				return "Warn";
			case Level::Error:
				return "Error";
			case Level::None:
				return "None";
			default:
				assert(false && "Level not handled");
		}
		return "Unknown Level";
	}

	// Defaulted compiler auto-generated methods
	LoggerImpl() noexcept {} /* = default; */ // Cannot '= default' due to clang4.0 bug
	virtual ~LoggerImpl() noexcept override = default;
	LoggerImpl(LoggerImpl&&) = default;
	LoggerImpl(LoggerImpl const&) = default;
	LoggerImpl& operator=(LoggerImpl const&) = default;
	LoggerImpl& operator=(LoggerImpl&&) = default;

private:
	std::mutex _lock{};
	std::vector<Observer*> _observers{};
	Level _level{ Level::None };
};

Logger& LA_AVDECC_CALL_CONVENTION Logger::getInstance() noexcept
{
	static LoggerImpl s_Instance{};

	return s_Instance;
}

} // namespace avdecc
} // namespace la