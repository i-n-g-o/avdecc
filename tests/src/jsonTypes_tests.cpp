/*
* Copyright (C) 2016-2026, L-Acoustics and its contributors

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
* @file jsonTypes_tests.cpp
* @author Christophe Calmejane
*/

#ifdef ENABLE_AVDECC_FEATURE_JSON

// Public API
#	include <la/avdecc/avdecc.hpp>
#	include <la/avdecc/internals/jsonTypes.hpp>

#	include <gtest/gtest.h>

#	include <map>
#	include <string>

namespace
{
namespace model = la::avdecc::entity::model;

template<typename T>
T roundTrip(T const& value)
{
	json const j = value; // Must use operator= instead of constructor to force usage of the to_json overload
	return j.get<T>();
}

} // namespace

TEST(JsonTypes, EnumKnownValue)
{
	json const j = model::JackType::Speaker;
	EXPECT_EQ("SPEAKER", j.get<std::string>());
	EXPECT_EQ(model::JackType::Speaker, j.get<model::JackType>());
}

TEST(JsonTypes, EnumUnknownValue)
{
	{
		auto const value = static_cast<model::JackType>(0x1234);
		json const j = value;
		EXPECT_EQ("0x1234", j.get<std::string>());
		EXPECT_EQ(value, j.get<model::JackType>());
	}
	{
		auto const value = static_cast<model::MemoryObjectType>(0x0020);
		json const j = value;
		EXPECT_EQ("0x0020", j.get<std::string>());
		EXPECT_EQ(value, j.get<model::MemoryObjectType>());
	}
	// 1-byte underlying type
	{
		auto const value = static_cast<model::AudioClusterFormat>(0x41);
		json const j = value;
		EXPECT_EQ("0x41", j.get<std::string>());
		EXPECT_EQ(value, j.get<model::AudioClusterFormat>());
	}
	EXPECT_EQ(static_cast<model::DescriptorType>(0x0100), roundTrip(static_cast<model::DescriptorType>(0x0100)));
	EXPECT_EQ(static_cast<model::ClockSourceType>(0x0005), roundTrip(static_cast<model::ClockSourceType>(0x0005)));
	EXPECT_EQ(static_cast<model::TimingAlgorithm>(0x0005), roundTrip(static_cast<model::TimingAlgorithm>(0x0005)));
	EXPECT_EQ(static_cast<model::PtpPortType>(0x0020), roundTrip(static_cast<model::PtpPortType>(0x0020)));
	EXPECT_EQ(static_cast<model::ControlValueUnit::Unit>(0xF0), roundTrip(static_cast<model::ControlValueUnit::Unit>(0xF0)));
	EXPECT_EQ(static_cast<model::ControlValueType::Type>(0x0100), roundTrip(static_cast<model::ControlValueType::Type>(0x0100)));
	EXPECT_EQ(static_cast<model::MsrpFailureCode>(25), roundTrip(static_cast<model::MsrpFailureCode>(25)));
}

TEST(JsonTypes, EnumInvalidValue)
{
	// Unknown name
	EXPECT_THROW(json("NOT_A_JACK_TYPE").get<model::JackType>(), json::type_error);
	// Hexadecimal value too big for the underlying type
	EXPECT_THROW(json("0x123").get<model::AudioClusterFormat>(), json::type_error);
	// Not an hexadecimal value
	EXPECT_THROW(json("0xZZ").get<model::JackType>(), json::type_error);
	EXPECT_THROW(json("0x").get<model::JackType>(), json::type_error);
	// Negative number
	EXPECT_THROW(json(-1).get<model::JackType>(), json::type_error);
	// Not a string nor a number
	EXPECT_THROW(json(true).get<model::JackType>(), json::type_error);
	EXPECT_THROW(json::object().get<model::JackType>(), json::type_error);
}

TEST(JsonTypes, EnumFromNumber)
{
	// Enums previously serialized without a mapping table were stored as a number
	EXPECT_EQ(model::JackType::Headphone, json(1).get<model::JackType>());
	EXPECT_EQ(model::StreamOutputCounters::CounterType::IEEE17221_2021, json(2u).get<model::StreamOutputCounters::CounterType>());
	EXPECT_THROW(json(256).get<model::AudioClusterFormat>(), json::type_error);
}

TEST(JsonTypes, EnumLegacyName)
{
	// Legacy name can be read
	EXPECT_EQ(model::ControlValueType::Type::Expansion, json("EXAPNSION").get<model::ControlValueType::Type>());
	// But it's never written
	json const j = model::ControlValueType::Type::Expansion;
	EXPECT_EQ("EXPANSION", j.get<std::string>());
}

TEST(JsonTypes, EnumBitfield)
{
	// Known and unknown flags
	{
		auto flags = la::avdecc::entity::EntityCapabilities{};
		flags.assign(0x40000008);
		json const j = flags;
		EXPECT_EQ((json{ "AEM_SUPPORTED", "0x40000000" }), j);
		EXPECT_EQ(flags.value(), j.get<la::avdecc::entity::EntityCapabilities>().value());
	}
	// 1-byte underlying type
	{
		auto flags = la::avdecc::entity::AvbInfoFlags{};
		flags.assign(0x81);
		json const j = flags;
		EXPECT_EQ((json{ "AS_CAPABLE", "0x80" }), j);
		EXPECT_EQ(flags.value(), j.get<la::avdecc::entity::AvbInfoFlags>().value());
	}
	// Empty bitfield
	{
		json const j = la::avdecc::entity::JackFlags{};
		EXPECT_TRUE(j.is_null());
		EXPECT_TRUE(j.get<la::avdecc::entity::JackFlags>().empty());
	}
	// More than one bit in an hexadecimal value (not written by the library, but must not crash)
	EXPECT_EQ(0x0003u, (json{ "0x0003" }).get<la::avdecc::entity::JackFlags>().value());
	// Unknown name
	EXPECT_THROW((json{ "NOT_A_FLAG" }).get<la::avdecc::entity::JackFlags>(), json::type_error);
}

TEST(JsonTypes, DescriptorCounters)
{
	{
		auto counters = model::EntityCounters{};
		counters[static_cast<la::avdecc::entity::EntityCounterValidFlag>(0x00000001)] = 10u;
		counters[static_cast<la::avdecc::entity::EntityCounterValidFlag>(0x00000002)] = 20u;
		counters[la::avdecc::entity::EntityCounterValidFlag::EntitySpecific1] = 30u;
		json const j = counters;
		EXPECT_EQ((json{ { "0x00000001", 10u }, { "0x00000002", 20u }, { "ENTITY_SPECIFIC_1", 30u } }), j);
		EXPECT_EQ(counters, j.get<model::EntityCounters>());
	}
	{
		auto counters = model::AvbInterfaceCounters{};
		counters[static_cast<la::avdecc::entity::AvbInterfaceCounterValidFlag>(0x00008000)] = 10u;
		counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkUp] = 30u;
		EXPECT_EQ(counters, roundTrip(counters));
	}
	{
		auto counters = model::ClockDomainCounters{};
		counters[static_cast<la::avdecc::entity::ClockDomainCounterValidFlag>(0x00008000)] = 10u;
		counters[la::avdecc::entity::ClockDomainCounterValidFlag::Locked] = 30u;
		EXPECT_EQ(counters, roundTrip(counters));
	}
	{
		auto counters = model::StreamInputCounters{};
		counters[static_cast<la::avdecc::entity::StreamInputCounterValidFlag>(0x00008000)] = 10u;
		counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked] = 30u;
		EXPECT_EQ(counters, roundTrip(counters));
	}
	// Unknown name
	EXPECT_THROW((json{ { "NOT_A_COUNTER", 10u } }).get<model::EntityCounters>(), json::type_error);
}

TEST(JsonTypes, StreamOutputCounters)
{
	auto c = std::map<la::avdecc::entity::StreamOutputCounterValidFlag17221, model::DescriptorCounter>{};
	c[static_cast<la::avdecc::entity::StreamOutputCounterValidFlag17221>(0x00008000)] = 10u;
	c[la::avdecc::entity::StreamOutputCounterValidFlag17221::StreamStart] = 30u;
	auto const counters = model::StreamOutputCounters{ c };

	json const j = counters;
	EXPECT_EQ("IEEE17221_2021", j.at("counter_type").get<std::string>());
	EXPECT_EQ((json{ { "0x00008000", 10u }, { "STREAM_START", 30u } }), j.at("counters"));

	auto const back = j.get<model::StreamOutputCounters>();
	EXPECT_EQ(model::StreamOutputCounters::CounterType::IEEE17221_2021, back.getCounterType());
	EXPECT_EQ(c, back.getCounters<la::avdecc::entity::StreamOutputCounterValidFlags17221>());

	// Legacy format: counter_type stored as a number
	{
		auto legacy = j;
		legacy["counter_type"] = 2;
		auto const legacyBack = legacy.get<model::StreamOutputCounters>();
		EXPECT_EQ(model::StreamOutputCounters::CounterType::IEEE17221_2021, legacyBack.getCounterType());
		EXPECT_EQ(c, legacyBack.getCounters<la::avdecc::entity::StreamOutputCounterValidFlags17221>());
	}
}

TEST(JsonTypes, DescriptorCounts)
{
	auto counts = model::DescriptorCounts{};
	counts[model::DescriptorType::Locale] = 1u;
	counts[static_cast<model::DescriptorType>(0x0100)] = 2u;
	counts[static_cast<model::DescriptorType>(0x0200)] = 3u;
	EXPECT_EQ(counts, roundTrip(counts));
}

TEST(JsonTypes, ControlValueTypeAndUnit)
{
	{
		auto const value = model::ControlValueType{ false, false, static_cast<model::ControlValueType::Type>(0x0100) };
		EXPECT_EQ(value.getValue(), roundTrip(value).getValue());
	}
	{
		auto const value = model::ControlValueUnit{ -2, static_cast<model::ControlValueUnit::Unit>(0xF0) };
		EXPECT_EQ(value.getValue(), roundTrip(value).getValue());
	}
}

TEST(JsonTypes, TypedDefine)
{
	// Known value
	{
		json const j = la::avdecc::protocol::AcmpStatus::NoSuchConnection;
		EXPECT_EQ("NO_SUCH_CONNECTION", j.get<std::string>());
		EXPECT_EQ(la::avdecc::protocol::AcmpStatus::NoSuchConnection, j.get<la::avdecc::protocol::AcmpStatus>());
	}
	// Unknown value
	{
		auto const value = la::avdecc::protocol::AcmpStatus{ 15u };
		json const j = value;
		EXPECT_EQ("0x0F", j.get<std::string>());
		EXPECT_EQ(value, j.get<la::avdecc::protocol::AcmpStatus>());
	}
	// Invalid values
	EXPECT_THROW(json("INVALID_STATUS").get<la::avdecc::protocol::AcmpStatus>(), json::type_error);
	EXPECT_THROW(json("0x123").get<la::avdecc::protocol::AcmpStatus>(), json::type_error);
	EXPECT_THROW(json(15).get<la::avdecc::protocol::AcmpStatus>(), json::type_error);
}

TEST(JsonTypes, StreamDynamicInfo)
{
	auto info = model::StreamDynamicInfo{};
	info.msrpFailureCode = static_cast<model::MsrpFailureCode>(25);
	info.probingStatus = static_cast<model::ProbingStatus>(7);
	info.acmpStatus = la::avdecc::protocol::AcmpStatus{ 15u };

	auto const back = roundTrip(info);
	EXPECT_EQ(info.msrpFailureCode, back.msrpFailureCode);
	EXPECT_EQ(info.probingStatus, back.probingStatus);
	EXPECT_EQ(info.acmpStatus, back.acmpStatus);
}

TEST(JsonTypes, AvdeccFixedString)
{
	// Valid UTF-8 string
	{
		auto const str = model::AvdeccFixedString{ "Valid UTF-8 \xc3\xa9" };
		json const j = str;
		EXPECT_EQ("Valid UTF-8 \xc3\xa9", j.get<std::string>());
		EXPECT_EQ(str, j.get<model::AvdeccFixedString>());
	}
	// Not a valid UTF-8 string (latin-1 encoded)
	{
		auto const str = model::AvdeccFixedString{ "Latin-1 \xe9" };
		json const j = str;
		EXPECT_EQ((json{ 76, 97, 116, 105, 110, 45, 49, 32, 233 }), j);
		EXPECT_NO_THROW(j.dump());
		EXPECT_EQ(str, j.get<model::AvdeccFixedString>());
	}
	// All bytes used (not NULL terminated)
	{
		auto const bytes = std::string(model::AvdeccFixedString::MaxLength, '\xff');
		auto const str = model::AvdeccFixedString{ bytes };
		json const j = str;
		EXPECT_EQ(model::AvdeccFixedString::MaxLength, j.size());
		EXPECT_EQ(str, j.get<model::AvdeccFixedString>());
	}
	// Invalid byte values
	EXPECT_THROW((json{ 97, 256 }).get<model::AvdeccFixedString>(), json::type_error);
	EXPECT_THROW((json{ 97, -1 }).get<model::AvdeccFixedString>(), json::type_error);
	EXPECT_THROW((json{ 97, "b" }).get<model::AvdeccFixedString>(), json::type_error);
}

#endif // ENABLE_AVDECC_FEATURE_JSON
