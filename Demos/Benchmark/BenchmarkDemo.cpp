// Copyright (c) Vector Informatik GmbH. All rights reserved.

#include <iostream>
#include <sstream>
#include <thread>
#include <fstream>

#include "ib/IntegrationBus.hpp"
#include "ib/sim/all.hpp"
#include "ib/mw/sync/all.hpp"
#include "ib/mw/sync/string_utils.hpp"

#include "ib/cfg/Config.hpp"
#include "ib/cfg/FastRtpsConfigBuilder.hpp"

using namespace ib::mw;
using namespace ib::sim::generic;
using namespace std::chrono_literals;

void PublishMessage(IGenericPublisher* publisher, unsigned int payloadSize)
{
	std::vector<uint8_t> data(payloadSize, '*');
	publisher->Publish(std::move(data));
}

void ReceiveMessage(IGenericSubscriber* subscriber, const std::vector<uint8_t>& data)
{
	std::string message{ data.begin(), data.end() };

	std::cout
		<< ">> Received new " << subscriber->Config().name
		<< " Message: with data=\"" << message << "\"" << std::endl;

	//std::cout << ".";
}

void SystemStateHandler(sync::ISystemController* controller, sync::SystemState newState, const ib::cfg::Config& ibConfig)
{
	switch (newState)
	{
	case ib::mw::sync::SystemState::Idle:
	{
		for (auto&& participant : ibConfig.simulationSetup.participants)
		{
			if (participant.name == "master")
				continue;
			controller->Initialize(participant.id);
		}

		break;
	}

	case ib::mw::sync::SystemState::Initialized:
		controller->Run();
		break;

	case ib::mw::sync::SystemState::Stopped:
		controller->Shutdown();
		break;

	default:
		std::cout << "New SystemState " << to_string(newState) << std::endl;
	}
}

auto buildConfig(unsigned int participantCount) -> ib::cfg::Config
{
	using namespace ib::cfg;

	ConfigBuilder benchmarkConfig("BenchmarkConfigGenerated");
	auto&& simulationSetup = benchmarkConfig.SimulationSetup();

	for (unsigned int participantCounter = 0; participantCounter < participantCount; ++participantCounter)
	{
		std::stringstream participantNameBuilder;
		participantNameBuilder << "Participant";
		participantNameBuilder << participantCounter;

		auto &&participantBuilder = simulationSetup.AddParticipant(participantNameBuilder.str());
		participantBuilder.WithParticipantId(participantCounter);
		participantBuilder.WithSyncType(ib::cfg::SyncType::DiscreteTime);

		for (unsigned int otherParticipantsCounter = 0; otherParticipantsCounter < participantCount; ++otherParticipantsCounter)
		{
			// skip self
			if (participantCounter == otherParticipantsCounter)
			{
				continue;
			}

			std::stringstream publisherNameBuilder;
			publisherNameBuilder << "PublisherOfParticipant";
			publisherNameBuilder << participantCounter;
			publisherNameBuilder << "ToParticipant";
			publisherNameBuilder << otherParticipantsCounter;

			std::stringstream subscriberNameBuilder;
			subscriberNameBuilder << "SubscriberOfParticipant";
			subscriberNameBuilder << participantCounter;
			subscriberNameBuilder << "FromParticipant";
			subscriberNameBuilder << otherParticipantsCounter;

			std::stringstream linkNameBuilder;
			linkNameBuilder << "LinkBetweenParticipant";
			linkNameBuilder << participantCounter;
			linkNameBuilder << "ToParticipant";
			linkNameBuilder << otherParticipantsCounter;

			participantBuilder->AddGenericPublisher(publisherNameBuilder.str()).WithLink(linkNameBuilder.str());
			participantBuilder->AddGenericSubscriber(subscriberNameBuilder.str()).WithLink(linkNameBuilder.str());
		}
	}

	simulationSetup.AddParticipant("Master")
		.AsSyncMaster();

	simulationSetup.ConfigureTimeSync()
		.WithLooseSyncPolicy()
		.WithTickPeriod(1000000ns);

	auto config = benchmarkConfig.Build();

	return config;
}

/**************************************************************************************************
* Main Function
**************************************************************************************************/

int main(int argc, char** argv)
{
	std::this_thread::sleep_for(5s);  // DEBUG

	auto ibConfig = buildConfig(20);

	try
	{
		uint32_t domainId = 42;
		//if (argc >= 2)
		//{
		//	domainId = static_cast<uint32_t>(std::stoul(argv[1]));
		//}

		std::vector<std::thread> threads;

		for (auto &&thisParticipant : ibConfig.simulationSetup.participants)
		{
			if (thisParticipant.name == "Master")
			{
				threads.push_back(std::thread([&ibConfig, &thisParticipant, &domainId] {

					auto comAdapter = ib::CreateComAdapter(ibConfig, thisParticipant.name, domainId);

					auto controller = comAdapter->GetSystemController();
					auto monitor = comAdapter->GetSystemMonitor();

					monitor->RegisterSystemStateHandler([controller, &ibConfig](sync::SystemState newState) {
						SystemStateHandler(controller, newState, ibConfig);
					});

					std::cout << "Press enter to stop and shutdown!" << std::endl;
					std::cin.ignore();
					controller->Stop();
					std::this_thread::sleep_for(1s);
					std::cout << "exiting..." << std::endl;

				}));

				continue;
			}

			threads.push_back(std::thread([&ibConfig, &thisParticipant, &domainId] {

				auto comAdapter = ib::CreateComAdapter(ibConfig, thisParticipant.name, domainId);
				auto&& participantController = comAdapter->GetParticipantController();
				participantController->SetPeriod(1ms);

				std::vector<ib::sim::generic::IGenericPublisher*> publishers;
				std::vector<ib::sim::generic::IGenericSubscriber*> subscribers;

				for (auto &genericPublisher : thisParticipant.genericPublishers)
				{
					publishers.push_back(comAdapter->CreateGenericPublisher(genericPublisher.name));
				}
				for (auto &genericSubscriber : thisParticipant.genericSubscribers)
				{
					auto subscriber = comAdapter->CreateGenericSubscriber(genericSubscriber.name);
					subscriber->SetReceiveMessageHandler(ReceiveMessage);
					subscribers.push_back(subscriber); // TODO: look at reference and move semantic again
				}

				participantController->SetSimulationTask([&publishers](std::chrono::nanoseconds now) {
					for (auto &publisher : publishers)
					{
						PublishMessage(publisher, 100); // TODO: make changeable payload size
					}
				});

				participantController->Run();
			}));
		}

		//std::cout << "Press enter to stop and shutdown!" << std::endl;
		//std::cin.ignore();
		//controller->Stop();
		//std::this_thread::sleep_for(1s);
		//std::cout << "exiting..." << std::endl;

		for (auto &thread : threads)
		{
			thread.join();
		}
	}
	catch (const ib::cfg::Misconfiguration& error)
	{
		std::cerr << "Invalid configuration: " << error.what() << std::endl;
		std::cout << "Press enter to stop the process..." << std::endl;
		std::cin.ignore();
		return -2;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Something went wrong: " << error.what() << std::endl;
		std::cout << "Press enter to stop the process..." << std::endl;
		std::cin.ignore();
		return -3;
	}

	return 0;
}
