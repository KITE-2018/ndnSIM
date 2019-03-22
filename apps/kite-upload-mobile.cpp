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

#include "kite-upload-mobile.hpp"
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

NS_LOG_COMPONENT_DEFINE("ndn.kite.KiteUploadMobile");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KiteUploadMobile);

TypeId
KiteUploadMobile::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KiteUploadMobile")
      .SetGroupName("Ndn")
      .SetParent<Producer>()
      .AddConstructor<KiteUploadMobile>()

      .AddAttribute("RvPrefix", "Prefix of Rendezvous Point", StringValue("/rv"),
                    MakeNameAccessor(&KiteUploadMobile::m_rvPrefix), MakeNameChecker())
      .AddAttribute("ServerPrefix", "Prefix of stationary server", StringValue("/server"),
                    MakeNameAccessor(&KiteUploadMobile::m_serverPrefix), MakeNameChecker())
      .AddAttribute("DataPrefix", "Prefix of the data to be uploaded", StringValue("/alice/photo"),
                    MakeNameAccessor(&KiteUploadMobile::m_dataPrefix), MakeNameChecker())
      .AddAttribute("TraceLifetime", "Lifetime for trace Interest packet", StringValue("2s"),
                    MakeTimeAccessor(&KiteUploadMobile::m_traceLifetime), MakeTimeChecker())
      .AddAttribute("RefreshInterval",
                    "Interval between trace interests, set to 0 to disable periodic sending",
                    StringValue("1s"), MakeTimeAccessor(&KiteUploadMobile::m_refreshInterval),
                    MakeTimeChecker());
  return tid;
}

// set m_seq=1, m_negotiationDone=false to disable initial negotiation
KiteUploadMobile::KiteUploadMobile()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_seq(0)
  , m_dataSeq(0)
  , m_traceRetryCnt(0)
  , m_negotiationDone(false)
  , m_uploadRequests(0)
  , m_rvInterests(0)
  , m_rvData(0)
  , m_interestForData(0)
  , m_data(0)
  , m_uploadSuccess(false)
{
  NS_LOG_FUNCTION_NOARGS();
}

void
KiteUploadMobile::OnAssociation()
{
  NS_LOG_INFO("> Association done with AP");
  if (!m_negotiationDone)
    StartNegotiation();
  else
    Simulator::Schedule(Seconds(0), &KiteUploadMobile::SendTrace, this); // send TI when relocate
}

// inherited from Application base class.
void
KiteUploadMobile::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  Producer::StartApplication(); // will register prefix
  StartNegotiation();
}

void
KiteUploadMobile::StopApplication()
{
  NS_LOG_UNCOND("Upload Requests sent: " << m_uploadRequests);
  // NS_LOG_UNCOND ("Tracing Overhead (%): " << (100.0 * (m_rvInterests + m_rvData) / (m_data +
  // m_uploadRequests + m_rvInterests + m_rvData + m_interestForData)));
  NS_LOG_UNCOND("Tracing Overhead (number of messages): " << m_rvInterests + m_rvData);

  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
KiteUploadMobile::StartNegotiation()
{
  if (m_negotiationDone)
    return;

  if (m_negotiationTimeoutEvent.IsRunning())
    Simulator::Cancel(m_negotiationTimeoutEvent);

  // send ISN negotiation request to RV
  NS_LOG_FUNCTION_NOARGS();
  Name name(m_rvPrefix); // consumer is actually a stationary server under upload scenario

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
    Simulator::Schedule(Seconds(1.0), &KiteUploadMobile::OnNegotiationTimeout, this);
}

void
KiteUploadMobile::OnNegotiationTimeout()
{
  // simply retry until data is returned
  // maybe previous interest still in pit?
  NS_LOG_INFO("Negotiation interest timed out, retrying...");
  StartNegotiation();
}

void
KiteUploadMobile::SendTrace()
{
  NS_LOG_FUNCTION_NOARGS();

  if (!m_negotiationDone)
    return;

  BOOST_ASSERT(m_seq > 0);

  if (m_traceRefreshEvent.IsRunning())
    Simulator::Cancel(m_traceRefreshEvent);

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

  if (m_refreshInterval != 0)
    m_traceRefreshEvent =
      Simulator::Schedule(Seconds(m_refreshInterval.GetSeconds()), &KiteUploadMobile::SendTrace,
                          this); // Send out trace at fixed intervals
  m_traceTimeoutEvent =
    Simulator::Schedule(Seconds(m_traceLifetime.GetSeconds()), &KiteUploadMobile::OnTraceTimeout,
                        this); // Trace interest times out after a specified period, will retry with
                               // new seq
}

void
KiteUploadMobile::OnTraceTimeout()
{
  NS_LOG_INFO("Trace interest timed out " << m_seq - 1);
  Simulator::Cancel(m_traceRefreshEvent);

  if (m_traceRetryCnt >= 3) {
    m_traceRetryCnt = 0;
    m_seq = 0;
    m_negotiationDone = false;
    NS_LOG_INFO("Starting negotation after trace interest timed out for 3 times");
    StartNegotiation();
    return;
  }

  SendTrace();
  ++m_traceRetryCnt;
}

void
KiteUploadMobile::SendUploadRequest()
{
  NS_LOG_FUNCTION_NOARGS();

  Name name(m_serverPrefix); // consumer is actually a stationary server under upload scenario

  name.append("upload");
  name.append(m_rvPrefix); // the new setup, rv announces its own prefix, mn has a prefix under that
  name.append(m_dataPrefix);
  name.appendSequenceNumber(m_dataSeq++);

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(name);                // e.g. /server/upload/alice/photo/1
  time::milliseconds interestLifeTime(0); // this doesn't need data returned
  interest->setInterestLifetime(interestLifeTime);

  NS_LOG_INFO("> Upload request Interest sent to " << name.toUri());

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);

  m_uploadRequests++;
}

void
KiteUploadMobile::OnData(shared_ptr<const Data> data)
{
  if (!m_active)
    return;

  App::OnData(data); // tracing inside

  NS_LOG_FUNCTION(this << data);

  Name negPrefix = m_rvPrefix;
  negPrefix.append("negotiate");
  if (negPrefix.isPrefixOf(data->getName())) {
    // potential response to negotiation request
    m_rvData++;
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
    m_negotiationDone = true;
    m_seq = isn;

    SendTrace(); // always refresh immediately after negotiation

    return;
  }

  // data for trace Interests
  NS_LOG_INFO("< DATA for " << data->getName());
  uint64_t seq = data->getName().at(-1).toSequenceNumber();
  if (seq != m_seq - 1) {
    NS_LOG_INFO("Wrong seq: " << seq);
    return;
  }
  Simulator::Cancel(m_traceTimeoutEvent);       // stop timeout timer
  Simulator::Cancel(m_negotiationTimeoutEvent); // no need to retry negotiation

  m_traceRetryCnt = 0;
  if (!m_uploadSuccess) {
    SendUploadRequest(); // send after receiving TD (trace is set up)
  }
}

shared_ptr<Name>
KiteUploadMobile::MakeTracePrefix()
{
  shared_ptr<Name> tracePrefix = make_shared<Name>(m_rvPrefix); // e.g. /rv
  tracePrefix->append("trace");
  tracePrefix->append(m_dataPrefix); // e.g. /alice/photo
  return tracePrefix;                // e.g. /rv/trace/alice/photo
}

void
KiteUploadMobile::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << *interest);

  if (!m_active)
    return;

  NS_LOG_INFO("Upload mobile received Interest for: " << interest->getName());
  m_interestForData++;
  m_data++;
  m_uploadSuccess = true;
  Producer::OnInterest(interest);
}

} // namespace ndn
} // namespace ns3
