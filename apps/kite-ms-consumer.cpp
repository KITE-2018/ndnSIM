/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "kite-ms-consumer.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include <ndn-cxx/lp/tags.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.kite.KiteMsConsumer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KiteMsConsumer);

TypeId
KiteMsConsumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KiteMsConsumer")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<KiteMsConsumer>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&KiteMsConsumer::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"), MakeStringAccessor(&KiteMsConsumer::SetRandomize,
                                                            &KiteMsConsumer::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&KiteMsConsumer::m_seqMax), MakeIntegerChecker<uint32_t>())

    ;

  return tid;
}

KiteMsConsumer::KiteMsConsumer()
  : m_frequency(1.0)
  , m_firstTime(true)
  , m_mapSeq(0)
  , m_outstandingMappingInterest(false)
  , m_interestSent(0)
  , m_dataReceived(0)
  , m_mapInterestSent(0)
  , m_mapDataReceived(0)
  , m_totalHopCount(0)
  , m_lastGood(-1)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

KiteMsConsumer::~KiteMsConsumer()
{
}

void
KiteMsConsumer::StartApplication()
{
  Consumer::StartApplication();

  UpdateLocator();
}

void
KiteMsConsumer::StopApplication()
{
  // NS_LOG_UNCOND("(Map consumer) Sent Interest: " << m_interestSent);
  // NS_LOG_UNCOND("(Map consumer) Received Data: " << m_dataReceived);
  // NS_LOG_UNCOND("(Map consumer) Sent map Interest: " << m_mapInterestSent);
  // NS_LOG_UNCOND("(Map consumer) Received map Data: " << m_mapDataReceived);
  // NS_LOG_UNCOND("(Map consumer) Average hop count: " << m_totalHopCount * 1.0 / m_dataReceived);

  std::cerr << "(Map consumer) Sent Interest: " << m_interestSent << std::endl;
  std::cerr << "(Map consumer) Received Data: " << m_dataReceived << std::endl;
  std::cerr << "(Map consumer) Sent map Interest: " << m_mapInterestSent << std::endl;
  std::cerr << "(Map consumer) Received map Data: " << m_mapDataReceived << std::endl;
  std::cerr << "(Map consumer) Average hop count: " << m_totalHopCount * 1.0 / m_dataReceived << std::endl;

  Consumer::StopApplication();
}

void
KiteMsConsumer::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_outstandingMappingInterest)
    return;

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Consumer::SendPacket, this);
    m_firstTime = false;
    ++m_interestSent;
  }
  else if (!m_sendEvent.IsRunning()) {
    m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
                                                      : Seconds(m_random->GetValue()),
                                      &Consumer::SendPacket, this);
    ++m_interestSent;
  }
}

void
KiteMsConsumer::OnData(shared_ptr<const Data> data)
{
  // get the new locator
  if (Name("/map").isPrefixOf(data->getName())) {
    m_mapDataReceived++;
    uint32_t seq = data->getName().at(-1).toSequenceNumber();
    NS_LOG_INFO("> Map Data for " << seq);
    Block content(data->getContent().value(), data->getContent().value_size());
    m_interestName.wireDecode(content);
    NS_LOG_INFO("New locator: " << m_interestName);
    m_outstandingMappingInterest = false;

    ScheduleNextPacket();

    return;
  }

  ++m_dataReceived;

  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  NS_LOG_DEBUG("Hop count: " << hopCount);
  m_totalHopCount += hopCount;

  uint32_t seq = data->getName().at(-1).toSequenceNumber();
  m_lastGood = seq;

  Consumer::OnData(data);
}

void
KiteMsConsumer::OnTimeout(uint32_t sequenceNumber)
{
  // update locator

  NS_LOG_FUNCTION_NOARGS();

  if (true || sequenceNumber > m_lastGood)
    UpdateLocator();

  // Consumer::OnTimeout(sequenceNumber);
}

void
KiteMsConsumer::UpdateLocator()
{
  if (!m_outstandingMappingInterest) {
    //
    shared_ptr<Name> nameWithSequence = make_shared<Name>("/map");
    nameWithSequence->appendSequenceNumber(m_mapSeq++);
    //

    shared_ptr<Interest> interest = make_shared<Interest>();
    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    interest->setName(*nameWithSequence);
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    interest->setInterestLifetime(interestLifeTime);

    NS_LOG_INFO("> Map Interest for " << m_mapSeq - 1);

    m_mapInterestSent++;

    m_transmittedInterests(interest, this, m_face);
    m_appLink->onReceiveInterest(*interest);
    m_outstandingMappingInterest = true;

    Simulator::Cancel(m_sendEvent);
  }
}

void
KiteMsConsumer::SetRandomize(const std::string& value)
{
  if (value == "uniform") {
    m_random = CreateObject<UniformRandomVariable>();
    m_random->SetAttribute("Min", DoubleValue(0.0));
    m_random->SetAttribute("Max", DoubleValue(2 * 1.0 / m_frequency));
  }
  else if (value == "exponential") {
    m_random = CreateObject<ExponentialRandomVariable>();
    m_random->SetAttribute("Mean", DoubleValue(1.0 / m_frequency));
    m_random->SetAttribute("Bound", DoubleValue(50 * 1.0 / m_frequency));
  }
  else
    m_random = 0;

  m_randomType = value;
}

std::string
KiteMsConsumer::GetRandomize() const
{
  return m_randomType;
}

} // namespace ndn
} // namespace ns3
