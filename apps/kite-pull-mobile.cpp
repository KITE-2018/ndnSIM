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

#include "kite-pull-mobile.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/wifi-module.h"
#include "ns3/node-list.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>
#include <ctime>

NS_LOG_COMPONENT_DEFINE("ndn.kite.KitePullMobile");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KitePullMobile);

TypeId
KitePullMobile::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KitePullMobile")
      .SetGroupName("Ndn")
      .SetParent<Producer>()
      .AddConstructor<KitePullMobile>()

      .AddAttribute("RvPrefix", "Prefix of Rendezvous Point", StringValue("/rv"),
                    MakeNameAccessor(&KitePullMobile::m_rvPrefix), MakeNameChecker())
      .AddAttribute("DataPrefix", "Prefix of the data to publish", StringValue("/alice/photo"),
                    MakeNameAccessor(&KitePullMobile::m_dataPrefix), MakeNameChecker())
      .AddAttribute("TraceLifetime", "Lifetime for trace Interest packet", StringValue("2s"),
                    MakeTimeAccessor(&KitePullMobile::m_traceLifetime), MakeTimeChecker())
      .AddAttribute("RefreshInterval",
                    "Interval between trace interests, set to 0 to disable periodic sending",
                    StringValue("1s"), MakeTimeAccessor(&KitePullMobile::m_refreshInterval),
                    MakeTimeChecker());
  return tid;
}

KitePullMobile::KitePullMobile()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_traceRetryCnt(0)
  , m_rvInterests(0)
  , m_rvData(0)
  , m_interestForData(0)
  , m_data(0)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seq = 1; // don't negotiate
  m_negotiationDone = true;
}

void
KitePullMobile::OnAssociation()
{
  NS_LOG_INFO("> Association done with AP");
  if (!m_negotiationDone)
    StartNegotiation();
  else
    Simulator::Schedule(Seconds(0.01), &KitePullMobile::SendTrace, this); // send TI when relocate
}

// inherited from Application base class.
void
KitePullMobile::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  Producer::StartApplication(); // will register prefix
  // SendTrace(); // should be done in OnAssociation
}

void
KitePullMobile::StopApplication()
{
  // NS_LOG_UNCOND ("Tracing Overhead (%): " << (100.0 * (m_rvInterests + m_rvData) / (m_data +
  // + m_rvInterests + m_rvData + m_interestForData))););
  std::cerr << "Tracing Overhead (number of messages): " << m_rvInterests + m_rvData << std::endl;

  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
KitePullMobile::StartNegotiation()
{
  if (m_negotiationDone)
    return;
  // send ISN negotiation request to RV
  NS_LOG_FUNCTION_NOARGS();

  Name name(m_rvPrefix);

  name.append("negotiate");
  name.append(m_dataPrefix);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(name); // e.g. /rv/negotiate/alice/photo
  time::milliseconds interestLifeTime(2000);
  interest->setInterestLifetime(interestLifeTime);

  NS_LOG_INFO("> Negotiation request Interest sent to " << name.toUri());
  m_rvInterests++;
  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  // when to timeout
  m_negotiationTimeoutEvent =
    Simulator::Schedule(Seconds(1.0), &KitePullMobile::OnNegotiationTimeout, this);
}

void
KitePullMobile::SendTrace()
{
  NS_LOG_FUNCTION_NOARGS();

  BOOST_ASSERT(m_seq > 0);

  shared_ptr<Name> name = MakeTracePrefix();
  name->appendSequenceNumber(m_seq);

  m_seq++;

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*name);
  time::milliseconds interestLifeTime(m_traceLifetime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);
  m_rvInterests++;
  NS_LOG_INFO("> Trace Interest sent to " << name->toUri());

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  if (m_traceRefreshEvent.IsRunning())
    Simulator::Cancel(m_traceRefreshEvent);

  if (m_refreshInterval != 0)
    m_traceRefreshEvent =
      Simulator::Schedule(Seconds(m_refreshInterval.GetSeconds()), &KitePullMobile::SendTrace,
                          this); // Send out trace at fixed intervals
  
  if (m_traceTimeoutEvent.IsRunning())
    Simulator::Cancel(m_traceTimeoutEvent);

  m_traceTimeoutEvent =
    Simulator::Schedule(Seconds(m_traceLifetime.GetSeconds()), &KitePullMobile::OnTraceTimeout,
                        this); // Trace interest times out after a specified period, will retry with
                               // new seq
}

void
KitePullMobile::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  m_rvData++;

  Name negPrefix = m_rvPrefix;
  negPrefix.append("negotiate");
  if (negPrefix.isPrefixOf(data->getName())) {
    // potential response to negotiation request
    if (!m_negotiationTimeoutEvent.IsRunning()) {
      // no negotiation in process
      NS_LOG_FUNCTION("Data from RV, but no negotiation in process.");
      return;
    }
    Simulator::Cancel(m_negotiationTimeoutEvent);
    Block content = data->getContent();
    if (content.value_size() != sizeof(uint64_t)) {
      NS_LOG_FUNCTION("Invalid data from RV, retrying negotiation...");
      StartNegotiation();
      return;
    }
    uint64_t isn = *(uint64_t*)content.value();
    NS_LOG_INFO("< Negotiation done, DATA for " << data->getName() << ", ISN=" << isn);
    m_seq = isn;
    SendTrace(); // send or not?
    return;
  }

  // data for trace Interest
  NS_LOG_INFO("< DATA for " << data->getName());
  uint64_t seq = data->getName().at(-1).toSequenceNumber();
  if (seq != m_seq - 1) {
    NS_LOG_INFO("Wrong seq: " << seq);
    return;
  }

  Simulator::Cancel(m_traceTimeoutEvent);       // stop timeout timer
  Simulator::Cancel(m_negotiationTimeoutEvent); // no need to retry negotiation

  m_traceRetryCnt = 0;
}

void
KitePullMobile::OnTraceTimeout()
{
  NS_LOG_INFO("Trace interest timed out " << m_seq - 1);
  Simulator::Cancel(m_traceRefreshEvent);

  if (m_traceRetryCnt >= 3) {
    m_traceRetryCnt = 0;
    m_seq = 0;
    NS_LOG_INFO("Starting negotation after trace interest timed out for 3 times");
    StartNegotiation();
    return;
  }

  if (m_seq > 0) // negotiation done?
    SendTrace();
  ++m_traceRetryCnt;
}

void
KitePullMobile::OnNegotiationTimeout()
{
  // simply retry until data is returned
  // maybe previous interest still in pit?
  NS_LOG_INFO("Negotiation interest timed out, retrying...");
  StartNegotiation();
}

shared_ptr<Name>
KitePullMobile::MakeTracePrefix()
{
  shared_ptr<Name> tracePrefix = make_shared<Name>(m_rvPrefix); // e.g. /rv
  tracePrefix->append("trace");
  tracePrefix->append(m_dataPrefix); // e.g. /alice/photo
  return tracePrefix;                // e.g. /rv/trace/alice/photo
}

void
KitePullMobile::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << *interest);

  if (!m_active)
    return;

  NS_LOG_INFO("Pull mobile received Interest for: " << interest->getName());
  m_interestForData++;
  m_data++;

  // if (m_traceRefreshEvent.IsRunning()) {
  //   Simulator::Cancel(m_traceRefreshEvent);
  //   NS_LOG_INFO("Cancel trace refresh upon receiving Interest, send trace after relocation");
  // }

  // Producer::OnInterest(interest);
  Name dataName(interest->getName());
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  dataName.appendSequenceNumber(m_current);

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared<::ndn::Buffer>(m_virtualPayloadSize));

  Signature signature;
  SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);
}

} // namespace ndn
} // namespace ns3
