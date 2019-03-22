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

#ifndef KITE_PULL_CONSUMER_H
#define KITE_PULL_CONSUMER_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-consumer.hpp"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief Ndn application for sending out Interest packets at a "constant" rate (Poisson process)
 */
class KitePullConsumer : public Consumer {
public:
  static TypeId
  GetTypeId();

  /**
   * \brief Default constructor
   * Sets up randomizer function and packet sequence number
   */
  KitePullConsumer();
  virtual ~KitePullConsumer();

  virtual void
  OnData(shared_ptr<const Data> data);

private:
  virtual void
  StartApplication(); ///< @brief Called at time specified by Start

  virtual void
  StopApplication(); ///< @brief Called at time specified by Stop

protected:
  /**
   * \brief Constructs the Interest packet and sends it using a callback to the underlying NDN
   * protocol
   */
  virtual void
  ScheduleNextPacket();

  /**
   * @brief Set type of frequency randomization
   * @param value Either 'none', 'uniform', or 'exponential'
   */
  void
  SetRandomize(const std::string& value);

  /**
   * @brief Get type of frequency randomization
   * @returns either 'none', 'uniform', or 'exponential'
   */
  std::string
  GetRandomize() const;

protected:
  double m_frequency; // Frequency of interest packets (in hertz)
  bool m_firstTime;
  Ptr<RandomVariableStream> m_random;
  std::string m_randomType;

  int m_interestSent;
  int m_dataReceived;
  int m_totalHopCount;
  int m_totalOpHopCount;

  int paths[11][11];

  int m_attachNode;
};

} // namespace ndn
} // namespace ns3

#endif
