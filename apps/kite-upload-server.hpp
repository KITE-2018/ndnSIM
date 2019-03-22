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

#ifndef KITE_UPLOAD_SERVER_H
#define KITE_UPLOAD_SERVER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ns3/ndnSIM/apps/ndn-consumer.hpp"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief Ndn application that runs as the stationary server in upload scenarios supporting Kite
 * scheme.
 * This one is a server application, it waits for Interest packets from mobile nodes that serve as
 * upload requests,
 * It then sends out Interest towards the mobile node to pull the data it tries to upload.
 * Currently, the name of the uploading mobile node is fixed.
 * Eventually the upload request should include information about the mobile node,
 * and certain verification machanisms should be applied so that this won't be exploited to conduct
 * DDoS attacks.
 */
class KiteUploadServer : public Consumer {
public:
  static TypeId
  GetTypeId(void);

  KiteUploadServer();
  virtual ~KiteUploadServer(){};

  // inherited from NdnApp
  virtual void
  OnInterest(shared_ptr<const Interest> interest);

  /**
   * @brief Send an Interest to fetch data from mobile producer
   */
  void
  SendInterest(shared_ptr<const Interest> uploadInterest);

  // From App
  virtual void
  OnData(shared_ptr<const Data> data);

  /**
   * @brief Timeout event
   * @param sequenceNumber time outed sequence number
   */
  virtual void
  OnTimeout(uint32_t sequenceNumber);

protected:
  // from App
  virtual void
  StartApplication();

  virtual void
  StopApplication(); // Called at time specified by Stop

  /**
   * \brief Do nothing
   */
  virtual void
  ScheduleNextPacket() {};

  static bool
  isUploadInterest(shared_ptr<const Interest> interest);

  void
  SendInterestForData(const Name dataName);

protected:
  // m_interestName inherited from Consumer
  Name m_serverPrefix;
  int m_dataReceived; // data received for upload requests
  int m_interestSent;
  Time m_averagePacketDelay; // average packet delay
  Time m_totalPacketDelay;   // total packet delay
  std::vector<std::tuple<Name, Time, uint32_t>> m_outstandingExchanges;
  bool m_sendInterestForData;
  bool m_done;
  double m_frequency;
  Ptr<RandomVariableStream> m_random;
  double m_averageHopCount;
};

} // namespace ndn
} // namespace ns3

#endif
