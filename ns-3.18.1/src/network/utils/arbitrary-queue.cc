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

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "arbitrary-queue.h"

#include "ns3/ecn-tag.h" // by xl on Mon May 13 20:35:42 CST 2013

NS_LOG_COMPONENT_DEFINE ("Arbi_Queue");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Arbi_Queue);

TypeId Arbi_Queue::GetTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::Arbi_Queue")
    .SetParent<Queue> ()
    .AddConstructor<Arbi_Queue> ()
    .AddAttribute ("Mode", 
                   "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&Arbi_Queue::SetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets accepted by this Arbi_Queue.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&Arbi_Queue::m_maxPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBytes", 
                   "The maximum number of bytes accepted by this Arbi_Queue.",
                   UintegerValue (100 * 65535),
                   MakeUintegerAccessor (&Arbi_Queue::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
	//ECN XL
	.AddAttribute ("ECN", 
                   "The ECN mode switch for DropQueue in the device",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Arbi_Queue::m_ecn_mode),
                   MakeUintegerChecker<uint32_t> ())
	
	.AddAttribute ("ECN_Threshold", 
                   "The mark threshold for ECN mode ",
                   UintegerValue (40),
                   MakeUintegerAccessor (&Arbi_Queue::m_ecn_threshold),
                   MakeUintegerChecker<uint32_t> ())

	.AddAttribute ("Offer_Load", 
                   "for offered load ",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Arbi_Queue::m_offered_load),
                   MakeUintegerChecker<uint8_t> ())
	
	//scc, xl
	//40 1500-B packets
	//.AddAttribute ("SCC_K", 
    //               "for scc K in byte",
     //              UintegerValue (60000),
      //             MakeUintegerAccessor (&Arbi_Queue::m_scc_k),
       //            MakeUintegerChecker<uint32_t> ())

	//add by xl for SCC
    .AddTraceSource ("Comp_pkt", 
                     "Compare pkt by tuple ",
                     MakeTraceSourceAccessor (&Arbi_Queue::m_compare_pkt))

	.AddTraceSource ("Assign_win", 
                     "Assign congetions windows for ack packets.",
                     MakeTraceSourceAccessor (&Arbi_Queue::m_scc_assign_win))


  ;

  return tid;
}

Arbi_Queue::Arbi_Queue () :
  Queue (),
  m_packets (),
  m_bytesInQueue (0),
  m_droptimes(0),
  m_ecns(0),
  m_offered_load(0)
{
  NS_LOG_FUNCTION (this);
}

Arbi_Queue::~Arbi_Queue ()
{
  NS_LOG_FUNCTION (this);
}

void
Arbi_Queue::SetMode (Arbi_Queue::QueueMode mode)
{
  NS_LOG_FUNCTION (this << mode);
  m_mode = mode;
}

Arbi_Queue::QueueMode
Arbi_Queue::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

bool Arbi_Queue::OverFill ()
{
  uint32_t orig_ecn_thresh ;
  bool over_tag=false;
  if (m_mode == QUEUE_MODE_PACKETS) {
    uint16_t reduce_packet_size = (double)m_offered_load/100 * m_maxPackets;
	m_maxPackets -= reduce_packet_size ;
	if (m_ecn_threshold < reduce_packet_size) { 
		orig_ecn_thresh = m_ecn_threshold ;
		m_ecn_threshold = 0;
	}
	else
	    m_ecn_threshold -= reduce_packet_size ;

	if ( m_bytesInQueue >= m_maxPackets * 1500 )
    {
      //Drop (p);
  	  over_tag=true;
    }
	else if (m_bytesInQueue >= m_ecn_threshold * 1500 ) {
	  if (m_ecn_mode) {
	      //Mark_ECN (p); 
  	  	  over_tag=true;
		}
	 }
	
	m_maxPackets += reduce_packet_size ;
	if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	else m_ecn_threshold += reduce_packet_size ;
  }

  return over_tag;
}

bool 
Arbi_Queue::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  uint32_t orig_ecn_thresh ;

  if (m_mode == QUEUE_MODE_PACKETS) {
    uint16_t reduce_packet_size = (double)m_offered_load/100 * m_maxPackets;
	m_maxPackets -= reduce_packet_size ;
	if (m_ecn_threshold < reduce_packet_size) { 
		orig_ecn_thresh = m_ecn_threshold ;
		m_ecn_threshold = 0;
	}
	else
	    m_ecn_threshold -= reduce_packet_size ;
	if (/*m_packets.size () >= m_maxPackets && */ m_bytesInQueue >= m_maxPackets * 1500 )
    {
      //NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
      NS_LOG_LOGIC ("Queue full (at max packets["<< m_maxPackets << "]) -- droppping pkt for "<< m_droptimes <<" times" );
      NS_LOG_DEBUG ("Queue full size("<< m_packets.size ()<<") (at max packets["<< m_maxPackets 
			  << "]) -- droppping pkt for "<< m_droptimes <<" times" 
			  << ", p->GetSize ()="<<p->GetSize ()<< ", m_bytesInQueue("<<m_bytesInQueue<<") >("<< m_maxPackets * 1500 <<") " );
      Drop (p);
	  m_droptimes++;
	  m_maxPackets += reduce_packet_size ;
	  if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	  else m_ecn_threshold += reduce_packet_size ;
      return false;
    }
	else if (/*m_packets.size () >= m_ecn_threshold*/m_bytesInQueue >= m_ecn_threshold * 1500 ) {
	  if (m_ecn_mode) {
      	  NS_LOG_DEBUG ("Marking ecn in droptail queue, packet mode. m_packets.size ()["
				<< m_packets.size ()<<"] >= m_ecn_threshold["<< m_ecn_threshold <<"]. "
				<< ", m_bytesInQueue( " << m_bytesInQueue <<") >= m_ecn_threshold * 1500 ("
				<< m_ecn_threshold * 1500 << ")"
				);
	      Mark_ECN (p); 
		}
        m_ecns++;
	 }
	
	m_maxPackets += reduce_packet_size ;
	if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	else m_ecn_threshold += reduce_packet_size ;
  }

  if (m_mode == QUEUE_MODE_BYTES) {//scc, in byte
    /*uint16_t reduce_byte_size = (double)m_offered_load/100 * m_maxBytes;
	m_maxBytes -= reduce_byte_size ;
	if (m_ecn_threshold < reduce_byte_size ) { 
		orig_ecn_thresh = m_ecn_threshold ;
		m_ecn_threshold = 0;
	}
	else
	    m_ecn_threshold -= reduce_byte_size ;
	if (m_bytesInQueue + p->GetSize () >= m_maxBytes)
    {
      NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
      NS_LOG_DEBUG ("Queue full (packet would exceed max bytes["<<m_maxBytes<<"]) -- droppping pkt for "<< m_droptimes <<" times" );
      Drop (p);
	  m_droptimes++;
	  m_maxBytes += reduce_byte_size ;
	  if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	  else m_ecn_threshold += reduce_byte_size ;
      return false;
    }
	  else if (m_ecn_mode && m_bytesInQueue + p->GetSize () >= m_ecn_threshold) {
      	NS_LOG_DEBUG ("Marking ecn in droptail queue, bytes mode. m_bytesInQueue + p->GetSize ()["
				<< m_bytesInQueue + p->GetSize () <<"] >= m_ecn_threshold["<< m_ecn_threshold << "]." );
	    Mark_ECN (p); 
        m_ecns++;
	  }
	m_maxBytes += reduce_byte_size ;
	if (m_ecn_threshold == 0) m_ecn_threshold = orig_ecn_thresh ;
	else m_ecn_threshold += reduce_byte_size ;
	*/
  }

  //delete repeated and old ack packets
  for (std::vector< Ptr<Packet> > ::iterator it = m_packets.begin() ; it != m_packets.end(); ++it)
  {
	  uint32_t ret;
	  m_compare_pkt(p, (*it), &ret);

	  if ( ret == 1 ){ //the same packet
		  (*it) = p; //replace the old ack packet
		  return false;
	  }
  }
  if (m_bytesInQueue + p->GetSize () >= m_maxBytes)
  {
      Drop (p);
	  m_droptimes++;
	  return false;
  }
  

  m_bytesInQueue += p->GetSize ();
  m_packets.push_back (p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return true;
}


void Arbi_Queue::AssignWin(uint16_t w)
{
  for (std::vector< Ptr<Packet> > ::iterator it = m_packets.begin() ; it != m_packets.end(); ++it)
  {
	  Ptr<Packet> newp;
	  m_scc_assign_win ((*it), w, newp);
	  (*it) = newp; //replace the old ack packet
  }
}

Ptr<Packet>
Arbi_Queue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.back (); //modified by xl
  m_packets.pop_back ();
  m_bytesInQueue -= p->GetSize ();

  NS_LOG_LOGIC ("Popped " << p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return p;
}

Ptr<const Packet>
Arbi_Queue::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.front ();

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return p;
}
	
void
Arbi_Queue::Mark_ECN (Ptr<Packet> p)
{
          EcnTag ecntag;
          p->AddPacketTag(ecntag);
}

} // namespace ns3

