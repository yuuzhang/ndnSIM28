/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NFD_DAEMON_FW_RANDOMIZEDROUNDING_STRATEGY_HPP
#define NFD_DAEMON_FW_RANDOMIZEDROUNDING_STRATEGY_HPP

#include "strategy.hpp"
#include "process-nack-traits.hpp"

#include <boost/random.hpp>
#include <boost/random/mersenne_twister.hpp>
#include "face/face.hpp"
#include "algorithm.hpp"

namespace nfd {
namespace fw {

/** \ZhangYu 2020-8-30,使用路由表提供的端口转发概率进行随机转发，可以实现 traffic split。
 * ndnSIM自带的多转发是多份副本，不是traffic split。
 * Sends an incoming interest to a random outgoing face,
 * excluding the incoming face.
 */
class RandomizedRoundingStrategy : public Strategy
                     , public ProcessNackTraits<RandomizedRoundingStrategy>
{
public:
  explicit
  RandomizedRoundingStrategy(Forwarder& forwarder, const Name& name = getStrategyName());

  static const Name&
  getStrategyName();

  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  void
  afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                   const shared_ptr<pit::Entry>& pitEntry) override;

private:
  friend ProcessNackTraits<RandomizedRoundingStrategy>;

protected:
  boost::random::mt19937 m_randomGenerator;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_RANDOMIZEDROUNDING_STRATEGY_HPP
