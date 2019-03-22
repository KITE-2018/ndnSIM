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

#include "kite-pull-consumer.hpp"
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

NS_LOG_COMPONENT_DEFINE("ndn.kite.KitePullConsumer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KitePullConsumer);

TypeId
KitePullConsumer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KitePullConsumer")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<KitePullConsumer>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&KitePullConsumer::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"), MakeStringAccessor(&KitePullConsumer::SetRandomize,
                                                            &KitePullConsumer::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&KitePullConsumer::m_seqMax),
                    MakeIntegerChecker<uint32_t>())
      .AddAttribute("attachNode", "",
                    IntegerValue(0),
                    MakeIntegerAccessor(&KitePullConsumer::m_attachNode),
                    MakeIntegerChecker<uint32_t>())

    ;

  return tid;
}

KitePullConsumer::KitePullConsumer()
  : m_frequency(1.0)
  , m_firstTime(true)
  , m_interestSent(0)
  , m_dataReceived(0)
  , m_totalHopCount(0)
  , m_totalOpHopCount(0)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();

  for (int i = 0; i < 11; i++)
    paths[i][i] = 0;

  paths[0][1] = 1;
  paths[0][2] = 2;
  paths[0][3] = 1;
  paths[0][4] = 3;
  paths[0][5] = 2;
  paths[0][6] = 4;
  paths[0][7] = 3;
  paths[0][8] = 4;
  paths[0][9] = 5;
  paths[0][10] = 5;
  paths[1][0] = 1;
  paths[1][2] = 1;
  paths[1][3] = 1;
  paths[1][4] = 2;
  paths[1][5] = 2;
  paths[1][6] = 3;
  paths[1][7] = 3;
  paths[1][8] = 4;
  paths[1][9] = 4;
  paths[1][10] = 5;
  paths[2][0] = 2;
  paths[2][1] = 1;
  paths[2][3] = 2;
  paths[2][4] = 1;
  paths[2][5] = 2;
  paths[2][6] = 2;
  paths[2][7] = 3;
  paths[2][8] = 4;
  paths[2][9] = 3;
  paths[2][10] = 4;
  paths[3][0] = 1;
  paths[3][1] = 1;
  paths[3][2] = 2;
  paths[3][4] = 2;
  paths[3][5] = 1;
  paths[3][6] = 3;
  paths[3][7] = 2;
  paths[3][8] = 3;
  paths[3][9] = 4;
  paths[3][10] = 4;
  paths[4][0] = 3;
  paths[4][1] = 2;
  paths[4][2] = 1;
  paths[4][3] = 2;
  paths[4][5] = 1;
  paths[4][6] = 1;
  paths[4][7] = 2;
  paths[4][8] = 3;
  paths[4][9] = 2;
  paths[4][10] = 3;
  paths[5][0] = 2;
  paths[5][1] = 2;
  paths[5][2] = 2;
  paths[5][3] = 1;
  paths[5][4] = 1;
  paths[5][6] = 2;
  paths[5][7] = 1;
  paths[5][8] = 2;
  paths[5][9] = 3;
  paths[5][10] = 3;
  paths[6][0] = 4;
  paths[6][1] = 3;
  paths[6][2] = 2;
  paths[6][3] = 3;
  paths[6][4] = 1;
  paths[6][5] = 2;
  paths[6][7] = 1;
  paths[6][8] = 2;
  paths[6][9] = 1;
  paths[6][10] = 2;
  paths[7][0] = 3;
  paths[7][1] = 3;
  paths[7][2] = 3;
  paths[7][3] = 2;
  paths[7][4] = 2;
  paths[7][5] = 1;
  paths[7][6] = 1;
  paths[7][8] = 1;
  paths[7][9] = 2;
  paths[7][10] = 2;
  paths[8][0] = 4;
  paths[8][1] = 4;
  paths[8][2] = 4;
  paths[8][3] = 3;
  paths[8][4] = 3;
  paths[8][5] = 2;
  paths[8][6] = 2;
  paths[8][7] = 1;
  paths[8][9] = 2;
  paths[8][10] = 1;
  paths[9][0] = 5;
  paths[9][1] = 4;
  paths[9][2] = 3;
  paths[9][3] = 4;
  paths[9][4] = 2;
  paths[9][5] = 3;
  paths[9][6] = 1;
  paths[9][7] = 2;
  paths[9][8] = 2;
  paths[9][10] = 1;
  paths[10][0] = 5;
  paths[10][1] = 5;
  paths[10][2] = 4;
  paths[10][3] = 4;
  paths[10][4] = 3;
  paths[10][5] = 3;
  paths[10][6] = 2;
  paths[10][7] = 2;
  paths[10][8] = 1;
  paths[10][9] = 1;
}

KitePullConsumer::~KitePullConsumer()
{
}

void
KitePullConsumer::StartApplication()
{
  Consumer::StartApplication();
}

void
KitePullConsumer::StopApplication()
{
  // NS_LOG_UNCOND("(Pull consumer) Sent Interest: " << m_interestSent);
  // NS_LOG_UNCOND("(Pull consumer) Received Data: " << m_dataReceived);
  // NS_LOG_UNCOND("(Pull consumer) Average hop count: " << m_totalHopCount * 1.0 / m_dataReceived);
  // NS_LOG_UNCOND("(Pull consumer) Average best hop count: " << m_totalOpHopCount * 1.0 / m_dataReceived);

  std::cerr << "(Pull consumer) Sent Interest: " << m_interestSent << std::endl;
  std::cerr << "(Pull consumer) Received Data: " << m_dataReceived << std::endl;
  std::cerr << "(Pull consumer) Average hop count: " << m_totalHopCount * 1.0 / m_dataReceived << std::endl;
  std::cerr << "(Pull consumer) Average best hop count: " << m_totalOpHopCount * 1.0 / m_dataReceived << std::endl;

  Consumer::StopApplication();
}

void
KitePullConsumer::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

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
KitePullConsumer::OnData(shared_ptr<const Data> data)
{
  ++m_dataReceived;

  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  Name dataName = data->getName();
  int opHopCount = paths[m_attachNode][dataName[-1].toSequenceNumber()] + 2;
  NS_LOG_DEBUG("Hop count: " << hopCount << ", best: " << opHopCount);

  if (hopCount < opHopCount) {
    // because PIT entry for pulled Interest expired, data is cached and fetched with a shorter hop count
    // set to the optimal path hop count to mitigate the effect
    hopCount = opHopCount;
  }

  m_totalHopCount += hopCount;

  m_totalOpHopCount += (opHopCount);

  shared_ptr<Data> newData = make_shared<Data>(data->wireEncode());
  newData->setName(dataName.getPrefix(dataName.size() - 1));

  Consumer::OnData(newData);
}

void
KitePullConsumer::SetRandomize(const std::string& value)
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
KitePullConsumer::GetRandomize() const
{
  return m_randomType;
}

} // namespace ndn
} // namespace ns3
