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
* @file commandStateMachine_tests.cpp
* @author Christophe Calmejane
*/

// Public API
#include <la/avdecc/executor.hpp>
#include <la/avdecc/internals/protocolAemAecpdu.hpp>

// Internal API
#include "stateMachine/commandStateMachine.hpp"
#include "entity/controllerEntityImpl.hpp"
#include "protocolInterface/protocolInterface_virtual.hpp"

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>

static auto constexpr DefaultExecutorName = "avdecc::protocol::PI";

/*
 * When an entity answered all its inflight commands, the next queued command must be sent right away.
 * It used to wait for the send interval, which was only checked again on the next state machine tick (5 msec, 15.6 msec on Windows), limiting the enumeration of an entity to one command per tick.
 */
TEST(CommandStateMachine, QueuedAecpCommandSentWhenResponseReceived)
{
	static auto constexpr CommandsCount = std::uint32_t{ 50u };
	static auto constexpr MaxDuration = std::chrono::milliseconds{ 100 }; // Half of what one command per state machine tick would take
	static auto const ControllerEntityID = la::avdecc::UniqueIdentifier{ 0x0102030405060708 };
	static auto const TargetEntityID = la::avdecc::UniqueIdentifier{ 0x1112131415161718 };
	static auto const ControllerMacAddress = la::networkInterface::MacAddress{ { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 } };
	static auto const TargetMacAddress = la::networkInterface::MacAddress{ { 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b } };

	// Answers every AEM command it sees on the virtual network
	class Responder : public la::avdecc::protocol::ProtocolInterface::Observer
	{
	private:
		// la::avdecc::protocol::ProtocolInterface::Observer overrides
		virtual void onAecpduReceived(la::avdecc::protocol::ProtocolInterface* const pi, la::avdecc::protocol::Aecpdu const& aecpdu) noexcept override
		{
			if (aecpdu.getMessageType() != la::avdecc::protocol::AecpMessageType::AemCommand)
			{
				return;
			}
			auto const& command = static_cast<la::avdecc::protocol::AemAecpdu const&>(aecpdu);
			auto response = la::avdecc::protocol::AemAecpdu::create(true);
			auto& aem = static_cast<la::avdecc::protocol::AemAecpdu&>(*response);
			aem.setSrcAddress(TargetMacAddress);
			aem.setDestAddress(command.getSrcAddress());
			aem.setStatus(la::avdecc::protocol::AecpStatus::Success);
			aem.setTargetEntityID(command.getTargetEntityID());
			aem.setControllerEntityID(command.getControllerEntityID());
			aem.setSequenceID(command.getSequenceID());
			aem.setUnsolicited(false);
			aem.setCommandType(command.getCommandType());
			pi->sendAecpMessage(aem);
		}

		DECLARE_AVDECC_OBSERVER_GUARD(Responder);
	};

	auto responder = Responder{};
	auto const executorWrapper = la::avdecc::ExecutorManager::getInstance().registerExecutor(DefaultExecutorName, la::avdecc::ExecutorWithDispatchQueue::create(DefaultExecutorName, la::avdecc::utils::ThreadPriority::Highest));
	auto controllerPI = std::unique_ptr<la::avdecc::protocol::ProtocolInterfaceVirtual>(la::avdecc::protocol::ProtocolInterfaceVirtual::createRawProtocolInterfaceVirtual("VirtualInterface", ControllerMacAddress, DefaultExecutorName));
	auto responderPI = std::unique_ptr<la::avdecc::protocol::ProtocolInterfaceVirtual>(la::avdecc::protocol::ProtocolInterfaceVirtual::createRawProtocolInterfaceVirtual("VirtualInterface", TargetMacAddress, DefaultExecutorName));
	responderPI->registerObserver(&responder);

	// AECP commands can only be sent on behalf of a registered local entity
	auto const commonInformation = la::avdecc::entity::Entity::CommonInformation{ ControllerEntityID, la::avdecc::UniqueIdentifier{ 0x1122334455667788 }, la::avdecc::entity::EntityCapabilities{ la::avdecc::entity::EntityCapability::AemSupported }, 0u, la::avdecc::entity::TalkerCapabilities{}, 0u, la::avdecc::entity::ListenerCapabilities{}, la::avdecc::entity::ControllerCapabilities{ la::avdecc::entity::ControllerCapability::Implemented }, std::nullopt, std::nullopt };
	auto const interfaceInfo = la::avdecc::entity::Entity::InterfaceInformation{ ControllerMacAddress, 31u, 0u, std::nullopt, std::nullopt };
	auto const controllerGuard = std::make_unique<la::avdecc::entity::LocalEntityGuard<la::avdecc::entity::ControllerEntityImpl>>(controllerPI.get(), commonInformation, la::avdecc::entity::Entity::InterfacesInformation{ { la::avdecc::entity::Entity::GlobalAvbInterfaceIndex, interfaceInfo } }, nullptr, nullptr);

	// Queue all the commands at once (only the first one can be sent right away, the others are sent as responses are received)
	auto responsesCount = std::atomic<std::uint32_t>{ 0u };
	auto allResponsesPromise = std::promise<void>{};
	auto const startTime = std::chrono::steady_clock::now();
	for (auto i = std::uint32_t{ 0u }; i < CommandsCount; ++i)
	{
		auto command = la::avdecc::protocol::AemAecpdu::create(false);
		auto& aem = static_cast<la::avdecc::protocol::AemAecpdu&>(*command);
		aem.setSrcAddress(ControllerMacAddress);
		aem.setDestAddress(TargetMacAddress);
		aem.setStatus(la::avdecc::protocol::AecpStatus::Success);
		aem.setTargetEntityID(TargetEntityID);
		aem.setControllerEntityID(ControllerEntityID);
		aem.setUnsolicited(false);
		aem.setCommandType(la::avdecc::protocol::AemCommandType::EntityAvailable);
		ASSERT_TRUE(!controllerPI->sendAecpCommand(std::move(command),
			[&responsesCount, &allResponsesPromise](la::avdecc::protocol::Aecpdu const* const response, la::avdecc::protocol::ProtocolInterface::Error const error)
			{
				EXPECT_NE(nullptr, response);
				EXPECT_EQ(la::avdecc::protocol::ProtocolInterface::Error::NoError, error);
				if (++responsesCount == CommandsCount)
				{
					allResponsesPromise.set_value();
				}
			}));
	}

	auto const status = allResponsesPromise.get_future().wait_for(std::chrono::seconds{ 5 });
	auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
	ASSERT_NE(std::future_status::timeout, status) << "Not all responses received";
	EXPECT_LT(duration.count(), MaxDuration.count()) << CommandsCount << " commands took " << duration.count() << " msec";
}
