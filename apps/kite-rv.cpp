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

#include "kite-rv.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include "helper/ndn-stack-helper.hpp"

#include <memory>
#include <ctime>

NS_LOG_COMPONENT_DEFINE("ndn.kite.KiteRv");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KiteRv);

TypeId
KiteRv::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KiteRv")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<KiteRv>()

      .AddAttribute("RvPrefix", "Prefix of Rendezvous Point", StringValue("/rv"),
                    MakeNameAccessor(&KiteRv::m_rvPrefix), MakeNameChecker())
      // .AddAttribute("MobilePrefix", "Prefix of Mobile Producer", StringValue("/alice"),
      //               MakeNameAccessor(&KiteRv::m_mobilePrefix), MakeNameChecker())
      .AddAttribute("InstancePrefix", "Unique prefix of the instance", StringValue("/rv"),
                    MakeNameAccessor(&KiteRv::m_instancePrefix), MakeNameChecker())

      .AddTraceSource("AttachedCallback", "AttachedCallback",
                      MakeTraceSourceAccessor(&KiteRv::m_attachCallback),
                      "ns3::ndn::KiteRv::AttachCallback");
  return tid;
}

KiteRv::KiteRv()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_isn(0)
  , m_attached(false)
  , m_attachedPrefix("/")
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
KiteRv::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_rvPrefix, m_face, 0);
  // FibHelper::AddRoute(GetNode(), m_mobilePrefix, m_face, 0);
  FibHelper::AddRoute(GetNode(), m_instancePrefix, m_face, 0);
}

void
KiteRv::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
KiteRv::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << *interest);

  if (!m_active)
    return;

  Name dataName(interest->getName());
  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(0));

  Name negPrefix = m_rvPrefix;
  negPrefix.append("negotiate");

  Name tracePrefix = m_rvPrefix;
  tracePrefix.append("trace");

  if (negPrefix.isPrefixOf(dataName)) {
    // negotiation interest
    m_isn = m_rand->GetValue(1, std::numeric_limits<uint32_t>::max()); // must > 0

    data->setContent((uint8_t *)&m_isn, sizeof(m_isn));
  }
  else if (tracePrefix.isPrefixOf(dataName)) {
    // received TI
    uint64_t seq = dataName.at(-1).toSequenceNumber();
    if (seq < m_isn) {
      NS_LOG_ERROR("Invalid sequence number, ignoring...");
      return;
    }

    data->setContent(make_shared< ::ndn::Buffer>(32)); // td 

    sendBuffered(); // new trace, send out bufferd interests

    m_attachCallback(this); // update attachment information globally
  }
  else if (m_rvPrefix.isPrefixOf(dataName)) {
    // consumer Interest for the MP
    if (!interest->hasLink()) {
      // this is the access RV
      if (m_attached || m_attachedPrefix == "/") {
        // also the attach RV, do nothing, will be forwarded according to traces
        return;
      }

      // not attach RV, append forwarding hint, send out
      shared_ptr<Interest> hinted = make_shared<Interest>(interest->wireEncode());
      hinted->setNonce(interest->getNonce() + 1);
      ::ndn::Link link;
      link.addDelegation(1, m_attachedPrefix);
      ndn::StackHelper::getKeyChain().sign(link, ::ndn::security::SigningInfo(::ndn::security::SigningInfo::SIGNER_TYPE_SHA256));
      hinted->setLink(link.wireEncode());

      m_appLink->onReceiveInterest(*hinted);
      NS_LOG_DEBUG("Redirected consumer Interest with hint: " << link.getDelegations().begin()->second);
    }
    else {
      // should be for this RV
      BOOST_ASSERT(interest->getLink().getDelegations().begin()->second == m_instancePrefix);
      if (!m_attached) {
        BOOST_ASSERT(m_attachedPrefix != m_instancePrefix);
        // correct the hint and send out
        shared_ptr<Interest> hinted = make_shared<Interest>(interest->wireEncode());
        hinted->setNonce(interest->getNonce() + 1);
        hinted->unsetLink();
        ::ndn::Link link;
        link.addDelegation(1, m_attachedPrefix);
        ndn::StackHelper::getKeyChain().sign(link,
                                             ::ndn::security::SigningInfo(
                                               ::ndn::security::SigningInfo::SIGNER_TYPE_SHA256));
        hinted->setLink(link.wireEncode());

        m_appLink->onReceiveInterest(*hinted);
        NS_LOG_DEBUG(
          "Redirected consumer Interest with corrected hint: " << link.getDelegations().begin()->second);
        return;
      }
      else {
        // trace is dead, buffer and send out after TI comes, or update comes
      NS_LOG_DEBUG("To be buffered: " << interest->getName());        
        if (m_bufferedInterests.size() < 40) {
          m_bufferedInterests.push_back(std::make_pair<shared_ptr<Interest>, Time>(make_shared<Interest>(interest->wireEncode()), Simulator::Now()));
        }
        return;
      }
      // remove hint, and forward
      shared_ptr<Interest> raw = make_shared<Interest>(interest->wireEncode());
      raw->unsetLink();
      raw->setNonce(interest->getNonce() + 1);

      m_appLink->onReceiveInterest(*raw);
      NS_LOG_DEBUG("Removed hint from consumer Interest and sent out: " << m_instancePrefix);
    }
    // never send data back
    return;
  }
  else {
    return;
  }

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  // if (m_keyLocator.size() > 0) {
  //   signatureInfo.setKeyLocator(m_keyLocator);
  // }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, 0));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName() << ", ISN=" << m_isn);

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);

}

void
KiteRv::sendBuffered()
{
  ::ndn::Link link;
  link.addDelegation(1, m_attachedPrefix);
  ndn::StackHelper::getKeyChain().sign(link, ::ndn::security::SigningInfo(
                                               ::ndn::security::SigningInfo::SIGNER_TYPE_SHA256));
  for (auto it = m_bufferedInterests.begin(); it != m_bufferedInterests.end(); ++it) {
    if (Simulator::Now() - it->second > Seconds(1))
      continue;
    auto p = it->first;
    p->setNonce(p->getNonce() + 1);
    p->unsetLink();
    p->setLink(link.wireEncode());
    m_appLink->onReceiveInterest(*p);
  }
  m_bufferedInterests.clear();
}

} // namespace ndn
} // namespace ns3
