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

#include "kite-upload-server.hpp"
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

#include "helper/ndn-fib-helper.hpp"

#include <ndn-cxx/lp/tags.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.kite.KiteUploadServer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KiteUploadServer);

TypeId
KiteUploadServer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KiteUploadServer")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<KiteUploadServer>()

      .AddAttribute("ServerPrefix", "Prefix of this stationary server", StringValue("/"),
                    MakeNameAccessor(&KiteUploadServer::m_serverPrefix), MakeNameChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&KiteUploadServer::m_seqMax),
                    MakeIntegerChecker<uint32_t>())

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("30.0"),
                    MakeDoubleAccessor(&KiteUploadServer::m_frequency), MakeDoubleChecker<double>())

    ;

  return tid;
}

KiteUploadServer::KiteUploadServer()
  : m_dataReceived(0)
  , m_interestSent(0)
  , m_sendInterestForData(false)
  , m_done(false)
  , m_random(0)
  , m_averageHopCount(0)
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
KiteUploadServer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_serverPrefix, m_face, 0);
}

void
KiteUploadServer::StopApplication()
{
  NS_LOG_UNCOND("Data received: " << m_dataReceived);
  NS_LOG_UNCOND("Interests sent: " << m_interestSent);
  NS_LOG_UNCOND("Average packet delay: " << m_averagePacketDelay.GetMicroSeconds() / 1000.0);
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
KiteUploadServer::OnInterest(shared_ptr<const Interest> interest)
{
  if (m_done)
    return;

  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  m_sendInterestForData = false; // don't think this is necessary

  if (!m_active)
    return;

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") received Interest for: " << interest->getName());

  if (isUploadInterest(interest)) {
    // retransmit outstanding Interests
    // for (auto i = m_outstandingExchanges.begin(); i != m_outstandingExchanges.end(); i++) {
    //   shared_ptr<Interest> interest = make_shared<Interest>();
    //   interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    //   interest->setName(i->first);
    //   time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    //   interest->setInterestLifetime(interestLifeTime);
    //
    //   // uint32_t seq = interest->getName().at(-1).toSequenceNumber();
    //   // WillSendOutInterest(seq);
    //
    //   NS_LOG_INFO("> Retransmitting Interest with name=" << interest->getName());
    //
    //   i->second = Simulator::Now();
    //
    //   m_transmittedInterests(interest, this, m_face);
    //   m_appLink->onReceiveInterest(*interest);
    // }
    SendInterest(interest); // send out an Interest packet to fetch data from mobile producer
  }
  else {
    return;
  }
}

void
KiteUploadServer::SendInterest(shared_ptr<const Interest> uploadInterest)
{
  if (m_done)
    return;

  if (!m_active)
    return;

  NS_LOG_FUNCTION_NOARGS();

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") received upload interest with name: "
                      << uploadInterest->getName() << ", nonce: " << uploadInterest->getNonce());

  shared_ptr<Name> dataName = make_shared<Name>(
    uploadInterest->getName().getSubName(2, uploadInterest->getName().size() - 3));

  shared_ptr<Name> nameWithSequence = make_shared<Name>(*dataName);
  nameWithSequence->appendSequenceNumber(m_seq);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  // NS_LOG_INFO ("Requesting Interest: \n" << *interest);
  NS_LOG_INFO("> Interest name=" << interest->getName());

  WillSendOutInterest(m_seq);

  m_outstandingExchanges.push_back(std::make_tuple(interest->getName(), Simulator::Now(), m_seq));
  m_seq++;
  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  m_interestSent++;

  ScheduleNextPacket();
  m_sendInterestForData = true;
  Simulator::Schedule(Seconds(0.01), &KiteUploadServer::SendInterestForData, this, *dataName);
}

void
KiteUploadServer::SendInterestForData(const Name dataName)
{
  if (m_done)
    return;

  if (!m_sendInterestForData) {
    return;
  }

  Name newName(dataName);
  newName.appendSequenceNumber(m_seq);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(newName);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);

  NS_LOG_INFO("> Send Interest For Data: Interest name=" << interest->getName());

  WillSendOutInterest(m_seq);

  m_outstandingExchanges.push_back(std::make_tuple(interest->getName(), Simulator::Now(), m_seq));
  m_seq++;
  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  m_interestSent++;

  if (m_sendInterestForData) {
    Simulator::Schedule(Seconds(0.01), &KiteUploadServer::SendInterestForData, this, dataName);
    // m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
    //                                                   : Seconds(m_random->GetValue()),
    //                                   &KiteUploadServer::SendInterestForData, this, dataName);
  }
}

bool
KiteUploadServer::isUploadInterest(shared_ptr<const Interest> interest)
{
  return (interest->getName()[1].toUri() == "upload");
}

void
KiteUploadServer::OnTimeout(uint32_t sequenceNumber)
{
  if (m_done)
    return;
  //std::cerr << "Estimated RTT: " << m_rtt->GetCurrentEstimate().GetMilliSeconds() / 1000.0 << " sec " << std::endl;
  m_rtt->IncreaseMultiplier(); // Double the next RTO
  m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
  m_retxSeqs.insert(sequenceNumber);

  for (auto i = m_outstandingExchanges.begin(); i != m_outstandingExchanges.end(); i++) {
    if (std::get<2>(*i) == sequenceNumber) {
      shared_ptr<Interest> interest = make_shared<Interest>();
      interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
      interest->setName(std::get<0>(*i));
      time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
      interest->setInterestLifetime(interestLifeTime);
      //WillSendOutInterest(sequenceNumber);
      //
      NS_LOG_INFO("> Retransmitting Interest with name=" << interest->getName());
      //
      std::get<1>(*i) = Simulator::Now();
      //
      m_transmittedInterests(interest, this, m_face);
      m_appLink->onReceiveInterest(*interest);
      break;
    }
  }
}

void
KiteUploadServer::OnData(shared_ptr<const Data> data)
{
  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);
  m_dataReceived++;

  if (m_dataReceived == 1000) {
    std::cerr << "Time to upload file: " << Simulator::Now().GetMilliSeconds() / 1000.0 << " sec" << std::endl;
    std::cerr << "Average hop count: " << m_averageHopCount / 1000.0 << " hops" << std::endl;
    this->StopApplication();
    m_done = true;
    return;
  }
  // Get round trip time
  // bool dup = false;
  auto i = m_outstandingExchanges.begin();
  while(i != m_outstandingExchanges.end()) {
    if (data->getName() == std::get<0>(*i)) {
      // NS_LOG_UNCOND("Delay for " << i->first << ": " << (Simulator::Now() - i->second).GetSeconds());
      m_totalPacketDelay += (Simulator::Now() - std::get<1>(*i));
      m_averagePacketDelay = m_totalPacketDelay / m_dataReceived;
      i = m_outstandingExchanges.erase(i);
      break;
      // if (dup) {
      //   NS_LOG_UNCOND("Dup found!");
      // }
      // else {
      //   dup = true;
      // }
    }
    else
      i++;
  }

  Consumer::OnData(data);
  NS_LOG_INFO("< Upload server got DATA for " << data->getName());

  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }
  NS_LOG_DEBUG("Hop count: " << hopCount);
  m_averageHopCount += hopCount;
}

} // namespace ndn
} // namespace ns3
