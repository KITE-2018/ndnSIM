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

#ifndef KITE_MS_SERVER_H
#define KITE_MS_SERVER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/random-variable-stream.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include "ns3/ndnSIM/apps/ndn-app.hpp"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief A simple Interest-sink applia simple Interest-sink application
 *
 * A simple Interest-sink.
 * It also sends out trace interest periodically to corresponding RV to update it's location in the network.
 * In upload scenario, this should run on a mobile node.
 */
class KiteMsServer : public App {
public:
  static TypeId
  GetTypeId(void);

  KiteMsServer();

protected:
  // inherited from Application base class.
  virtual void
  StartApplication(); // Called at time specified by Start

  virtual void
  StopApplication(); // Called at time specified by Stop

  virtual void
  OnInterest(shared_ptr<const Interest> interest);

private:
  Name m_prefix; // prefix of mapping server
  // Name m_mobilePrefix; // prefix of MP, supports only one for now, RV needs to respond to trace Interests

  Ptr<UniformRandomVariable> m_rand; ///< @brief nonce generator

public:
  Name m_locator; // prefix of MN
  typedef void (*UpdateCallback)(Ptr<App>);
  TracedCallback<Ptr<App>> m_updateCallback;
};

} // namespace ndn
} // namespace ns3

#endif // KITE_MS_SERVER_H
