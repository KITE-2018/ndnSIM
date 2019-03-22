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

#ifndef KITE_UPLOAD_MOBILE_H
#define KITE_UPLOAD_MOBILE_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/random-variable-stream.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include "ns3/ndnSIM/apps/ndn-producer.hpp"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief A simple Interest-sink applia simple Interest-sink application
 *
 * A simple Interest-sink.
 * It also sends out trace interest periodically to corresponding RV to update it's location in the
 *network.
 * In upload scenario, this should run on a mobile node.
 */
class KiteUploadMobile : public Producer {
public:
  static TypeId
  GetTypeId(void);

  KiteUploadMobile();

  void
  OnAssociation(); // actions when associated with a new AP

  void
  SendTrace(); // periodically, or on demand, send TI

  void
  SendUploadRequest(); // explicitly inform server of an upload request, server will then send
                       // interest to MP

  void
  StartNegotiation(); // sync the timestamp with the RV

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest);

  virtual void
  OnData(shared_ptr<const Data> data);

  void
  OnTraceTimeout(); // TI times out, should not happen, and if due to relocation, will send when
                    // associated

  // TODO: OnNack()

  void
  OnNegotiationTimeout(); // retry negotiation until success

  shared_ptr<Name>
  MakeTracePrefix();

protected:
  // inherited from Application base class.
  virtual void
  StartApplication(); // Called at time specified by Start

  virtual void
  StopApplication(); // Called at time specified by Stop

private:
  Name m_rvPrefix;     // prefix of RV, /rv
  Name m_serverPrefix; // prefix of stationary server, to which data is uploaded
  Name m_dataPrefix;   // prefix of data to be uploaded, e.g. /alice/photo, is producer prefix in
                       // paper, the full data prefix is m_rvPrefix + m_dataPrefix

  Time m_traceLifetime;   // lifeTime for trace interest
  Time m_refreshInterval; // interval between trace interests

  Ptr<UniformRandomVariable> m_rand; ///< @brief nonce generator

  uint64_t m_seq; // increments each time new trace interest is sent, initiated after negotiation
  uint64_t m_dataSeq;

  EventId m_traceRefreshEvent; // send new trace interest at fixed interval, unless previous trace
                               // interest fetched invalid data or times out.
  EventId m_traceTimeoutEvent; // triggered when interest times out, use trace Interest lifetime

  int m_traceRetryCnt; // retry up to 3 times for sending trace Interest, and then start negotiation

  EventId m_negotiationTimeoutEvent;

  bool m_negotiationDone;

  int m_uploadRequests;  // count upload requests
  int m_rvInterests;     // interests sent to RV
  int m_rvData;          // data received from RV
  int m_interestForData; // Interest for Data from consumer
  int m_data;            // data packets sent to the consumer
  bool m_uploadSuccess;  // has the server received the upload request?
};

} // namespace ndn
} // namespace ns3

#endif // NDN_PRODUCER_H
