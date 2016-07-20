/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/mpi-interface.h"
#include "point-to-point-net-device.h"
#include "point-to-point-channel.h"
#include "ppp-header.h"

NS_LOG_COMPONENT_DEFINE ("PointToPointNetDevice");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (PointToPointNetDevice);

TypeId 
PointToPointNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointNetDevice")
    .SetParent<NetDevice> ()
    .AddConstructor<PointToPointNetDevice> ()
    .AddAttribute ("Mtu", "The MAC-level Maximum Transmission Unit",
                   UintegerValue (DEFAULT_MTU),
                   MakeUintegerAccessor (&PointToPointNetDevice::SetMtu,
                                         &PointToPointNetDevice::GetMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Address", 
                   "The MAC address of this device.",
                   Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
                   MakeMac48AddressAccessor (&PointToPointNetDevice::m_address),
                   MakeMac48AddressChecker ())
    .AddAttribute ("DataRate", 
                   "The default data rate for point to point links",
                   DataRateValue (DataRate ("32768b/s")),
                   MakeDataRateAccessor (&PointToPointNetDevice::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("ReceiveErrorModel", 
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    .AddAttribute ("InterframeGap", 
                   "The time to wait between packet (frame) transmissions",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&PointToPointNetDevice::m_tInterframeGap),
                   MakeTimeChecker ())

    //
    // Transmit queueing discipline for the device which includes its own set
    // of trace hooks.
    //
    .AddAttribute ("TxQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointNetDevice::m_queue),
                   MakePointerChecker<Queue> ())

    //
    // Trace sources at the "top" of the net device, where packets transition
    // to/from higher layers.
    //
    .AddTraceSource ("MacTx", 
                     "Trace source indicating a packet has arrived for transmission by this device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macTxTrace))
    .AddTraceSource ("MacTxDrop", 
                     "Trace source indicating a packet has been dropped by the device before transmission",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macTxDropTrace))
    .AddTraceSource ("MacPromiscRx", 
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macPromiscRxTrace))
    .AddTraceSource ("MacRx", 
                     "A packet has been received by this device, has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macRxTrace))
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("MacRxDrop", 
                     "Trace source indicating a packet was dropped before being forwarded up the stack",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macRxDropTrace))
#endif
    //
    // Trace souces at the "bottom" of the net device, where packets transition
    // to/from the channel.
    //
    .AddTraceSource ("PhyTxBegin", 
                     "Trace source indicating a packet has begun transmitting over the channel",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxBeginTrace))
    .AddTraceSource ("PhyTxEnd", 
                     "Trace source indicating a packet has been completely transmitted over the channel",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxEndTrace))
    .AddTraceSource ("PhyTxDrop", 
                     "Trace source indicating a packet has been dropped by the device during transmission",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxDropTrace))
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("PhyRxBegin", 
                     "Trace source indicating a packet has begun being received by the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxBeginTrace))
#endif
    .AddTraceSource ("PhyRxEnd", 
                     "Trace source indicating a packet has been completely received by the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxEndTrace))
    .AddTraceSource ("PhyRxDrop", 
                     "Trace source indicating a packet has been dropped by the device during reception",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxDropTrace))

    //
    // Trace sources designed to simulate a packet sniffer facility (tcpdump).
    // Note that there is really no difference between promiscuous and 
    // non-promiscuous traces in a point-to-point link.
    //
    .AddTraceSource ("Sniffer", 
                     "Trace source simulating a non-promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_snifferTrace))
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer attached to the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_promiscSnifferTrace))


	//scc, xl  
	.AddAttribute ("SCC_mode", 
				   "Set switch-based congestion control for switch.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PointToPointNetDevice::m_scc_mode),
                   MakeUintegerChecker<uint8_t> ())

	.AddAttribute ("SCC_K", 
				   "Set the valve of K in byte for SCC switch.",
                   UintegerValue (60000), //40*1500B packets
                   MakeUintegerAccessor (&PointToPointNetDevice::m_scc_k),
                   MakeUintegerChecker<uint32_t> ())


    .AddTraceSource ("IsAck", 
                     "Determine a packet is ack packet.",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_scc_isack))
    .AddTraceSource ("IsFin", 
                     "Determine a packet is fin packet.",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_scc_isfin))
    .AddTraceSource ("Assign_win", 
                     "Assign congetions windows for ack packets.",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_scc_assign_win))
  ;
  return tid;
}


PointToPointNetDevice::PointToPointNetDevice () 
  :
    m_txMachineState (READY),
    m_channel (0),
    m_linkUp (false),
    m_currentPkt (0),
	m_scc_x (0), //scc, xl
	m_scc_state (UCS) //scc, xl
{
  NS_LOG_FUNCTION (this);
 m_dev_global_not_initial = 1; //xl for ictcp
 m_all_connections = 0; //xl for all protocols
}

PointToPointNetDevice::~PointToPointNetDevice ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
PointToPointNetDevice::AddHeader (Ptr<Packet> p, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  PppHeader ppp;
  ppp.SetProtocol (EtherToPpp (protocolNumber));
  p->AddHeader (ppp);
}

bool
PointToPointNetDevice::ProcessHeader (Ptr<Packet> p, uint16_t& param)
{
  NS_LOG_FUNCTION_NOARGS ();
  PppHeader ppp;
  p->RemoveHeader (ppp);
  param = PppToEther (ppp.GetProtocol ());
  return true;
}

void
PointToPointNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_node = 0;
  m_channel = 0;
  m_receiveErrorModel = 0;
  m_currentPkt = 0;
  NetDevice::DoDispose ();
}

void
PointToPointNetDevice::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_bps = bps;
}

void
PointToPointNetDevice::SetInterframeGap (Time t)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_tInterframeGap = t;
}

bool
PointToPointNetDevice::TransmitStart (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ".");

  //
  // This function is called to start the process of transmitting a packet.
  // We need to tell the channel that we've started wiggling the wire and
  // schedule an event that will be executed when the transmission is complete.
  //
  NS_ASSERT_MSG (m_txMachineState == READY, "Must be READY to transmit");
  m_txMachineState = BUSY;
  m_currentPkt = p;
  m_phyTxBeginTrace (m_currentPkt);

  Time txTime = Seconds (m_bps.CalculateTxTime (p->GetSize ()));
  Time txCompleteTime = txTime + m_tInterframeGap;

  NS_LOG_LOGIC ("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds () << "sec");
  Simulator::Schedule (txCompleteTime, &PointToPointNetDevice::TransmitComplete, this);

  bool result = m_channel->TransmitStart (p, this, txTime);
  if (result == false)
    {
      m_phyTxDropTrace (p);
    }
  return result;
}

void
PointToPointNetDevice::TransmitComplete (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  //
  // This function is called to when we're all done transmitting a packet.
  // We try and pull another packet off of the transmit queue.  If the queue
  // is empty, we are done, otherwise we need to start transmitting the
  // next packet.
  //
  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != 0, "PointToPointNetDevice::TransmitComplete(): m_currentPkt zero");

  m_phyTxEndTrace (m_currentPkt);
  m_currentPkt = 0;

  //scc, xl
  if ( m_scc_mode == 1 ) 
  {
  	  uint16_t protocol = 0;
	  uint16_t np = m_input_queue -> GetNPackets();
	  uint32_t sum_w = 0;
	  uint32_t max_w = m_scc_k;
	  uint16_t w=0;
   	  //uint32_t isfin=0;
  	  //NS_LOG_DEBUG ("transmitcomplete: Qr np=" << np <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< m_queue->GetNBytes() <<" bytes.");
	  if (m_queue->GetNBytes() <=m_scc_x && m_scc_state == CS ){//CS to LCS
  	  		//NS_LOG_DEBUG ("receive:CS TO LCS, input np=" << np <<", in p2p input queue.");
			m_scc_state = LCS;

			//send ack only once, wait until m_queue is empty.
			if (np>0){
	  		  w = (max_w/np > 1446)?max_w/np:1446;
			  m_input_queue -> AssignWin(w);
			}
  	  		NS_LOG_DEBUG ("transcomplete:CS TO LCS, Qr np=" << np <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< 
					m_queue->GetNBytes() <<" bytes,x="<<m_scc_x <<",w="<<w<<", k="<<m_scc_k <<".");
	  		while ( ! m_input_queue->IsEmpty() && sum_w <= max_w )
	  		{
  				Ptr<Packet> p = m_input_queue->Dequeue ();
				ProcessHeader (p, protocol);
				//Ptr<Packet> newp;
				//m_scc_assign_win (p, w, newp);
				//m_scc_isfin (p, &isfin);
				//if (isfin==0)
				sum_w += w;
        		if (!m_promiscCallback.IsNull ())
        		{
          		  m_macPromiscRxTrace (p);
          		  m_promiscCallback (this, p, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        		}

        		m_macRxTrace (p);
        		m_rxCallback (this, p, protocol, GetRemote ());

	  		}
			  //as normal operation.
	  } 
	  else if(m_queue->IsEmpty()){ //UCS 
			m_scc_state = UCS;
			m_scc_x = 0;
  	  		NS_LOG_DEBUG ("transcomplete: UCS,Qs is empty, Qr np=" << np <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< m_queue->GetNBytes() <<" bytes, w="<<w<<", k="<<m_scc_k <<".");
			
			//send all every time receive a packet.
			if (np>0){
	  		  w = (max_w/np > 1446)?max_w/np:1446;
			  m_input_queue -> AssignWin(w);
			}
	  		while ( ! m_input_queue->IsEmpty() && sum_w <= max_w )
	  		{
  				Ptr<Packet> p = m_input_queue->Dequeue ();
				ProcessHeader (p, protocol);
				//Ptr<Packet> newp;
				//m_scc_assign_win (p, w, newp);
				//m_scc_isfin (p, &isfin);
				//if (isfin==0)
				sum_w += w;
        		if (!m_promiscCallback.IsNull ())
        		{
          		  m_macPromiscRxTrace (p);
          		  m_promiscCallback (this, p, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        		}

        		m_macRxTrace (p);
        		m_rxCallback (this, p, protocol, GetRemote ());

	  		}//while
			  //as normal operation.
	 } 
	 else if (m_queue->GetNBytes() <= m_scc_x/2 && m_scc_state == LCS ){//LCS, x=x/2
		  m_scc_x = (m_scc_x/2<=1500*4)?0:m_scc_x/2; //only 4 pkts, no need to send ack pkts at all.
		  
		  if (np>0){
	  		  w = (max_w/np > 1446)?max_w/np:1446;
			  m_input_queue -> AssignWin(w);
			}
  	  		NS_LOG_DEBUG ("transcomplete:LCS, x=x/2, Qr np=" << np <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< 
					m_queue->GetNBytes() <<" bytes,x="<<m_scc_x <<",w="<<w<<", k="<<m_scc_k <<".");
	  		while ( ! m_input_queue->IsEmpty() && sum_w <= max_w )
	  		{
  				Ptr<Packet> p = m_input_queue->Dequeue ();
				ProcessHeader (p, protocol);
				//Ptr<Packet> newp;
				//m_scc_assign_win (p, w, newp);
				//m_scc_isfin (p, &isfin);
				//if (isfin==0)
				sum_w += w;
        		if (!m_promiscCallback.IsNull ())
        		{
          		  m_macPromiscRxTrace (p);
          		  m_promiscCallback (this, p, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        		}

        		m_macRxTrace (p);
        		m_rxCallback (this, p, protocol, GetRemote ());

	  		}
			  //as normal operation.
	  }
	 
 }//if scc mode

  
	  

  Ptr<Packet> p = m_queue->Dequeue ();
  if (p == 0)
    {
      //
      // No packet was on the queue, so we just exit.
      //
      return;
    }

  //
  // Got another packet off of the queue, so start the transmit process agin.
  //
  m_snifferTrace (p);
  m_promiscSnifferTrace (p);
  TransmitStart (p);
}

bool
PointToPointNetDevice::Attach (Ptr<PointToPointChannel> ch)
{
  NS_LOG_FUNCTION (this << &ch);

  m_channel = ch;

  m_channel->Attach (this);

  //
  // This device is up whenever it is attached to a channel.  A better plan
  // would be to have the link come up when both devices are attached, but this
  // is not done for now.
  //
  NotifyLinkUp ();
  return true;
}

void
PointToPointNetDevice::SetQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue = q;
}

void
PointToPointNetDevice::SetInputQueue (Ptr<Queue> q)
{
  NS_LOG_FUNCTION (this << q);
  m_input_queue = q;
}

void
PointToPointNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  NS_LOG_FUNCTION (this << em);
  m_receiveErrorModel = em;
}

void
PointToPointNetDevice::Receive (Ptr<Packet> packet)
{//receive from link
  NS_LOG_FUNCTION (this << packet);
  uint16_t protocol = 0;
  uint32_t isack=0;
  std::string ip_port_str;
  m_scc_isack(packet, &isack, ip_port_str);
  NS_LOG_DEBUG ("receive: isack="<<isack<<", pkt info:"<<ip_port_str);

  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) ) 
    {
      // 
      // If we have an error model and it indicates that it is time to lose a
      // corrupted packet, don't forward this packet up, let it go.
      //
      m_phyRxDropTrace (packet);
    }
  else 
    {
      // 
      // Hit the trace hooks.  All of these hooks are in the same place in this 
      // device becuase it is so simple, but this is not usually the case in 
      // more complicated devices.
      //
      m_snifferTrace (packet);
      m_promiscSnifferTrace (packet);
      m_phyRxEndTrace (packet);
	  m_received_devicedataN += packet->GetSize(); //xl for ictcp

	  //scc,xl--------------------------------------------------
	  if (m_scc_mode == 1){
		  int32_t x = m_queue->GetNBytes() - m_scc_k;
		  if (x >= 0) { //CS
			  m_scc_state = CS;
			  if ((uint32_t)x > m_scc_x) m_scc_x = (uint32_t)x;
			  if (m_scc_x > m_scc_k) m_scc_x = m_scc_k;
  	  		  NS_LOG_DEBUG ("receive: CS, Qr np=" << m_input_queue->GetNPackets() <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< 
					  m_queue->GetNBytes() <<" bytes,isack="<<isack <<",m_scc_k="<<m_scc_k <<",m_scc_x="<< m_scc_x<<".");
			  if (isack == 1){ //buffer ACK in input queue.
      			m_input_queue->Enqueue (packet);
				return;
			  }
			  //else {//non-ack packet
				  //as normal operation
			  //}
		  }
		  else if (m_scc_state == LCS){//LCS
  	  		  NS_LOG_DEBUG ("receive: LCS, Qr np=" << m_input_queue->GetNPackets() <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< m_queue->GetNBytes() <<" bytes,isack="<<isack <<".");
			  if (isack == 1){ //buffer ACK in input queue.
      			m_input_queue->Enqueue (packet);
				return;
			  }
			  //else {//non-ack packet
				  //as normal operation
			  //}
		  }
		  else if (m_queue->GetNBytes() <= m_scc_x  && m_scc_state == CS ){//CS to LCS
			m_scc_state = LCS;

			//send ack only once, wait until m_queue is empty.
	  		uint16_t np = m_input_queue -> GetNPackets();
	  		uint32_t sum_w = 0;
	  		uint32_t max_w = m_scc_k;
			uint16_t w=0;
   			//uint32_t isfin=0;
			if (np>0){
	  		  w = (max_w/np > 1446)?max_w/np:1446;
			  m_input_queue -> AssignWin(w);
			}
  	  		NS_LOG_DEBUG ("receive:CS TO LCS, Qr np=" << np <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< 
					m_queue->GetNBytes() <<" bytes,x="<<m_scc_x <<",w="<<w<<", k="<<m_scc_k <<".");
	  		while ( ! m_input_queue->IsEmpty() && sum_w <= max_w )
	  		{
  				Ptr<Packet> p = m_input_queue->Dequeue ();
				ProcessHeader (p, protocol);
				//Ptr<Packet> newp;
				//m_scc_assign_win (p, w, newp);
				//m_scc_isfin (p, &isfin);
				//if (isfin==0)
				sum_w += w;
        		if (!m_promiscCallback.IsNull ())
        		{
          		  m_macPromiscRxTrace (p);
          		  m_promiscCallback (this, p, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        		}

        		m_macRxTrace (p);
        		m_rxCallback (this, p, protocol, GetRemote ());

	  		}
			  //as normal operation.
		  } 
		  else if(m_queue->IsEmpty()) {//UCS
			m_scc_state = UCS;
			m_scc_x = 0;
			
			//send all every time receive a packet.
			uint16_t np = m_input_queue -> GetNPackets();
	  		uint32_t sum_w = 0;
	  		uint32_t max_w = m_scc_k;
			uint16_t w=0;
   			//uint32_t isfin=0;
			if (np>0){
	  		  w = (max_w/np > 1446)?max_w/np:1446;
			  m_input_queue -> AssignWin(w);
			}
  	  		NS_LOG_DEBUG ("receive: UCS,Qs is empty, Qr np=" << np <<" pkt="<< m_input_queue->GetNBytes() <<" bytes, Qs="<< m_queue->GetNBytes() <<" bytes, w="<<w<<", k="<<m_scc_k <<".");
	  		while ( ! m_input_queue->IsEmpty() && sum_w <= max_w )
	  		{
  				Ptr<Packet> p = m_input_queue->Dequeue ();
				ProcessHeader (p, protocol);
				//Ptr<Packet> newp;
				//m_scc_assign_win (p, w, newp);
				//m_scc_isfin (p, &isfin);
				//if (isfin==0)
				sum_w += w;
        		if (!m_promiscCallback.IsNull ())
        		{
          		  m_macPromiscRxTrace (p);
          		  m_promiscCallback (this, p, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        		}

        		m_macRxTrace (p);
        		m_rxCallback (this, p, protocol, GetRemote ());

	  		}
			  //as normal operation.
		  }
	  }
	  //scc,xl--------------------------------------------------
      //
      // Strip off the point-to-point protocol header and forward this packet
      // up the protocol stack.  Since this is a simple point-to-point link,
      // there is no difference in what the promisc callback sees and what the
      // normal receive callback sees.
      //
	  //normal operation
      ProcessHeader (packet, protocol);

      if (!m_promiscCallback.IsNull ())
        {
          m_macPromiscRxTrace (packet);
          m_promiscCallback (this, packet, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        }

      m_macRxTrace (packet);
      m_rxCallback (this, packet, protocol, GetRemote ());
    }
}

Ptr<Queue>
PointToPointNetDevice::GetQueue (void) const
{ 
  NS_LOG_FUNCTION_NOARGS ();
  return m_queue;
}

Ptr<Queue>
PointToPointNetDevice::GetInputQueue (void) const
{ 
  NS_LOG_FUNCTION_NOARGS ();
  return m_input_queue;
}


void
PointToPointNetDevice::NotifyLinkUp (void)
{
  m_linkUp = true;
  m_linkChangeCallbacks ();
}

void
PointToPointNetDevice::SetIfIndex (const uint32_t index)
{
  m_ifIndex = index;
}

uint32_t
PointToPointNetDevice::GetIfIndex (void) const
{
  return m_ifIndex;
}

Ptr<Channel>
PointToPointNetDevice::GetChannel (void) const
{
  return m_channel;
}

//
// This is a point-to-point device, so we really don't need any kind of address
// information.  However, the base class NetDevice wants us to define the
// methods to get and set the address.  Rather than be rude and assert, we let
// clients get and set the address, but simply ignore them.

void
PointToPointNetDevice::SetAddress (Address address)
{
  m_address = Mac48Address::ConvertFrom (address);
}

Address
PointToPointNetDevice::GetAddress (void) const
{
  return m_address;
}

bool
PointToPointNetDevice::IsLinkUp (void) const
{
  return m_linkUp;
}

void
PointToPointNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  m_linkChangeCallbacks.ConnectWithoutContext (callback);
}

//
// This is a point-to-point device, so every transmission is a broadcast to
// all of the devices on the network.
//
bool
PointToPointNetDevice::IsBroadcast (void) const
{
  return true;
}

//
// We don't really need any addressing information since this is a 
// point-to-point device.  The base class NetDevice wants us to return a
// broadcast address, so we make up something reasonable.
//
Address
PointToPointNetDevice::GetBroadcast (void) const
{
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
PointToPointNetDevice::IsMulticast (void) const
{
  return true;
}

Address
PointToPointNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  return Mac48Address ("01:00:5e:00:00:00");
}

Address
PointToPointNetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address ("33:33:00:00:00:00");
}

bool
PointToPointNetDevice::IsPointToPoint (void) const
{
  return true;
}

bool
PointToPointNetDevice::IsBridge (void) const
{
  return false;
}

bool
PointToPointNetDevice::Send (
  Ptr<Packet> packet, 
  const Address &dest, 
  uint16_t protocolNumber)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_LOGIC ("p=" << packet << ", dest=" << &dest);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());

  //
  // If IsLinkUp() is false it means there is no channel to send any packet 
  // over so we just hit the drop trace on the packet and return an error.
  //
  if (IsLinkUp () == false)
    {
      m_macTxDropTrace (packet);
      return false;
    }

  //
  // Stick a point to point protocol header on the packet in preparation for
  // shoving it out the door.
  //
  AddHeader (packet, protocolNumber);

  m_macTxTrace (packet);

  //
  // If there's a transmission in progress, we enque the packet for later
  // transmission; otherwise we send it now.
  //
  if (m_txMachineState == READY) 
    {
      // 
      // Even if the transmitter is immediately available, we still enqueue and
      // dequeue the packet to hit the tracing hooks.
      //
      if (m_queue->Enqueue (packet) == true)
        {
          packet = m_queue->Dequeue ();
          m_snifferTrace (packet);
          m_promiscSnifferTrace (packet);
          return TransmitStart (packet);
        }
      else
        {
          // Enqueue may fail (overflow)
          m_macTxDropTrace (packet);
          return false;
        }
    }
  else
    {
      return m_queue->Enqueue (packet);
    }
}

bool
PointToPointNetDevice::SendFrom (Ptr<Packet> packet, 
                                 const Address &source, 
                                 const Address &dest, 
                                 uint16_t protocolNumber)
{
  return false;
}

Ptr<Node>
PointToPointNetDevice::GetNode (void) const
{
  return m_node;
}

void
PointToPointNetDevice::SetNode (Ptr<Node> node)
{
  m_node = node;
}

bool
PointToPointNetDevice::NeedsArp (void) const
{
  return false;
}

void
PointToPointNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  m_rxCallback = cb;
}

void
PointToPointNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

bool
PointToPointNetDevice::SupportsSendFrom (void) const
{
  return false;
}

void
PointToPointNetDevice::DoMpiReceive (Ptr<Packet> p)
{
  Receive (p);
}

Address 
PointToPointNetDevice::GetRemote (void) const
{
  NS_ASSERT (m_channel->GetNDevices () == 2);
  for (uint32_t i = 0; i < m_channel->GetNDevices (); ++i)
    {
      Ptr<NetDevice> tmp = m_channel->GetDevice (i);
      if (tmp != this)
        {
          return tmp->GetAddress ();
        }
    }
  NS_ASSERT (false);
  // quiet compiler.
  return Address ();
}

bool
PointToPointNetDevice::SetMtu (uint16_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;
  return true;
}

uint16_t
PointToPointNetDevice::GetMtu (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mtu;
}

uint16_t
PointToPointNetDevice::PppToEther (uint16_t proto)
{
  switch(proto)
    {
    case 0x0021: return 0x0800;   //IPv4
    case 0x0057: return 0x86DD;   //IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}

uint16_t
PointToPointNetDevice::EtherToPpp (uint16_t proto)
{
  switch(proto)
    {
    case 0x0800: return 0x0021;   //IPv4
    case 0x86DD: return 0x0057;   //IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}


} // namespace ns3
