/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ARBITRARY_H
#define ARBITRARY_H

//#include <queue>
#include <vector>
#include "ns3/packet.h"
#include "ns3/queue.h"

namespace ns3 {

class TraceContainer;

/**
 * \ingroup queue
 *
 * \brief A FIFO packet queue that drops tail-end packets on overflow
 */
//for scc switch, xl
class Arbi_Queue : public Queue {
public:
  static TypeId GetTypeId (void);
  /**
   * \brief Arbi_Queue Constructor
   *
   * Creates a droptail queue with a maximum size of 100 packets by default
   */
  Arbi_Queue ();

  virtual ~Arbi_Queue();

  /**
   * Set the operating mode of this device.
   *
   * \param mode The operating mode of this device.
   *
   */
  void SetMode (Arbi_Queue::QueueMode mode);

  /**
   * Get the encapsulation mode of this device.
   *
   * \returns The encapsulation mode of this device.
   */
  Arbi_Queue::QueueMode GetMode (void);

  uint32_t Get_Droptimes()
  {
	  return m_droptimes; //xl for static
  }
private:
  virtual bool DoEnqueue (Ptr<Packet> p);
  virtual Ptr<Packet> DoDequeue (void);
  virtual Ptr<const Packet> DoPeek (void) const;

  //std::queue<Ptr<Packet> > m_packets;
  std::vector<Ptr<Packet> > m_packets; //for scc, xl
  uint32_t m_maxPackets;
  uint32_t m_maxBytes;
  uint32_t m_bytesInQueue;
  QueueMode m_mode;
  
  uint32_t m_droptimes; //xl for static

public:
  //for ecn
  uint32_t       m_ecn_mode;
  uint32_t       m_ecn_threshold;
  void Mark_ECN (Ptr<Packet> p); //xl
  uint32_t      m_ecns;  //mark ecn times. xl
  //for offered load
  uint8_t		m_offered_load;
  
public:
  virtual bool OverFill ();

  //for scc, xl
  //uint32_t       m_scc_k; //in byte
  TracedCallback< Ptr<const Packet>, Ptr<Packet>, uint32_t* > m_compare_pkt;
  
  TracedCallback< Ptr<const Packet>, uint16_t,  Ptr<Packet>& > m_scc_assign_win;
  virtual void AssignWin(uint16_t w);
};

} // namespace ns3

#endif /* ARBITRARY_H */
