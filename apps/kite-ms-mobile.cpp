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

#include "kite-ms-mobile.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.kite.KiteMsMobile");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KiteMsMobile);

TypeId
KiteMsMobile::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::KiteMsMobile")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<KiteMsMobile>()
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&KiteMsMobile::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&KiteMsMobile::m_postfix), MakeNameChecker())
      .AddAttribute("ServerPrefix", "Prefix of the mapping server", StringValue("/map"),
                    MakeNameAccessor(&KiteMsMobile::m_serverPrefix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&KiteMsMobile::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&KiteMsMobile::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&KiteMsMobile::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&KiteMsMobile::m_keyLocator), MakeNameChecker());
  return tid;
}

KiteMsMobile::KiteMsMobile()
  : m_rand(CreateObject<UniformRandomVariable>())
  , m_locator("/")
  , m_mapInterestSent(0)
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
KiteMsMobile::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
}

void
KiteMsMobile::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  std::cerr << "(Map mobile) Sent map Interest: " << m_mapInterestSent << std::endl;

  App::StopApplication();
}

void
KiteMsMobile::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  if (!m_active)
    return;

  Name dataName(interest->getName());
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

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

void
KiteMsMobile::UpdateMapping()
{
  Name locator(m_locator);
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_serverPrefix);
  nameWithSequence->append("update");
  nameWithSequence->append(locator);
  nameWithSequence->appendSequenceNumber(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));

  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(*nameWithSequence);
  time::milliseconds interestLifeTime(2000);
  interest->setInterestLifetime(interestLifeTime);

  m_mapInterestSent++;
  NS_LOG_INFO("Send mapping update: " << interest->getName());

  m_appLink->onReceiveInterest(*interest);
}

} // namespace ndn
} // namespace ns3
