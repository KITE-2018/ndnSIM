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

#include "kite-ms-server.hpp"
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

#include <memory>
#include <ctime>

NS_LOG_COMPONENT_DEFINE("ndn.kite.KiteMsServer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(KiteMsServer);

TypeId
KiteMsServer::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::ndn::KiteMsServer")
                        .SetGroupName("Ndn")
                        .SetParent<App>()
                        .AddConstructor<KiteMsServer>()

                        .AddAttribute("Prefix", "Prefix of this server", StringValue("/map"),
                                      MakeNameAccessor(&KiteMsServer::m_prefix), MakeNameChecker())
                        .AddAttribute("Locator", "Current prefix of MN", StringValue("/"),
                                      MakeNameAccessor(&KiteMsServer::m_locator), MakeNameChecker())

                        .AddTraceSource("UpdateCallback", "UpdateCallback",
                                        MakeTraceSourceAccessor(&KiteMsServer::m_updateCallback),
                                        "ns3::ndn::KiteRv::UpdateCallback")
    ;
  return tid;
}

KiteMsServer::KiteMsServer()
  : m_rand(CreateObject<UniformRandomVariable>())
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
KiteMsServer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
}

void
KiteMsServer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
KiteMsServer::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << *interest);

  if (!m_active)
    return;

  if (Name("/map/update").isPrefixOf(interest->getName())) {
    // mapping udpate interest
    m_locator = interest->getName().getSubName(2, 2);
    NS_LOG_INFO("New locator: " << m_locator);
    m_updateCallback(this);
    // return;
  }

  Name dataName(interest->getName());
  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(0));

  data->setContent((uint8_t*)m_locator.wireEncode().wire(), m_locator.wireEncode().size());

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, 0));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName() << ", locator=" << m_locator);

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);

}

} // namespace ndn
} // namespace ns3
