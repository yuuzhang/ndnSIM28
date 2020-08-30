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
 * 2020-8-29 试图直接使用2018版的代码，发现不行。又从random-strategy.hpp复制后
 * 参考2018版的代码修改得到
 */

#include "randomized-rounding-strategy.hpp"
#include "algorithm.hpp"

#include <ndn-cxx/util/random.hpp>

namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY(RandomizedRoundingStrategy);
NFD_LOG_INIT(RandomizedRoundingStrategy);

RandomizedRoundingStrategy::RandomizedRoundingStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    NDN_THROW(std::invalid_argument("Randomized Rounding Strategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "Randomized Rounding Strategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
RandomizedRoundingStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/randomized-rounding/%FD%01");
  return strategyName;
}

void
RandomizedRoundingStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                     const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const Face& inFace = ingress.face;
  fib::NextHopList nhs;

  std::copy_if(fibEntry.getNextHops().begin(), fibEntry.getNextHops().end(), std::back_inserter(nhs),
               [&] (const auto& nh) { return isNextHopEligible(inFace, interest, nh, pitEntry); });

  if (nhs.empty()) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " no nexthop");

    lp::NackHeader nackHeader;
    nackHeader.setReason(lp::NackReason::NO_ROUTE);
    this->sendNack(pitEntry, ingress, nackHeader);
    this->rejectPendingInterest(pitEntry);
    return;
  }
  // ZhangYu 2020-8-30
  boost::random::uniform_01<boost::random::mt19937&> dist(m_randomGenerator);
  //dist.reset();
  //std::cout << "ZhangYu 2018-3-25 randomValue: " << dist() << std::endl;
  const uint64_t randomValue =std::round(dist() *1000000); //和 global-routing-help中的一样
  //std::cout << "ZhangYu 2018-3-25 randomValue: " << randomValue << std::endl;
  uint64_t probabilitySum=0;
  fib::NextHopList::const_iterator selected;	//端口变量
  uint64_t index=0;
  for(selected=nhs.begin(); selected !=nhs.end(); ++selected) {
	  index=index+1;
	  probabilitySum+=selected->getProbability();
	  //ZhangYu 2018-4-6这里是否加=应该影响不大，主要担心的是概率为0和1的记录，但都是小概率事件。
	  if(randomValue<=probabilitySum){
		  this->sendInterest(pitEntry, FaceEndpoint(selected->getFace(),0), interest);
		  /*
		  std::cout << "      ZhangYu 2018-2-1 afterReceiveInterest-- "
				  << " face: " << FaceEndpoint(selected->getFace(),0)
				  << " cost: " << selected->getCost()
				  << " probability: " << selected->getProbability() << std::endl;

		  std::cout << "!!ZhangYu 2018-3-25, index:" << index << std::endl;
		  */
		  return;
	  }
  }
  //std::shuffle(nhs.begin(), nhs.end(), ndn::random::getRandomNumberEngine());
  //this->sendInterest(pitEntry, FaceEndpoint(nhs.front().getFace(), 0), interest);
}

void
RandomizedRoundingStrategy::afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                                 const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(ingress.face, nack, pitEntry);
}

} // namespace fw
} // namespace nfd
