#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <vector>


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ppp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"

#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

#include "ns3/mymacros.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <vector>
#include <ctime>
#include <cstdlib>

#define ICTCP_MODE_DEF					  0
#define RTCP_MODE_DEF					  0
#define DCTCP_MODE_DEF					  0
#define RDTCP_MODE_DEF					  0
#define RDTCP_CONTROLLER_MODE_DEF		  0
#define ECN_MODE_DEF                      1
#define USE_RED_QUEUE_DEF                 1
#define MACHINES_PER_RACK_DEF             40
#define NUMBER_OF_RACKS_DEF               25
#define MACHINES_TO_TOR_RATE_DEF          1
#define LINK_DELAY_DEF                    "40us"
#define NUMBER_OF_APPS_DEF                5
#define QUEUELENGTH_TOR_DEF               80 // interms of packets
#define QUEUELENGTH_FABRIC_DEF            80  // interms of packets
#define THRESHOLD_TOR_DEF_MIN             20 // interms of % occupancy
#define THRESHOLD_TOR_DEF_MAX             40  // interms of % occupancy
//#define THRESHOLD_FABRIC_DEF              10  // interms of % occupancy
#define QUERIES_PER_APP_DEF               2
#define INTERARRIVAL_MULTIPLIER_DEF       2.0
#define STOP_DEF                          10
#define IGNORE_ECN_PORT                   1000
#define LONG_FLOW_PORT                    900
#define USE_LONG_FLOWS                    1
#define TIGHT_DEADLINE                    0
#define PRINT_STATUS                      1
#define SEGMENT_SIZE                      1446
#define RTO_MIN                           10


using namespace std;

// A typical data center has racks of machines connected to a ToR switch using p2p links
// Each ToR swicth connects to another fabric swicth, it is a 2-level tree

using namespace ns3;

// Primary knobs
uint32_t        ICTCPMode                 = ICTCP_MODE_DEF;
uint32_t        RTCPMode                  = RTCP_MODE_DEF;
uint32_t        DCTCPMode                 = DCTCP_MODE_DEF;
uint32_t        RDTCPMode                 = RDTCP_MODE_DEF;
uint32_t        RDTCP_CONTROLLER_Mode     = RDTCP_CONTROLLER_MODE_DEF		  ;
uint32_t        SCCMode                   = 0;

uint32_t        ECNMode                   = ECN_MODE_DEF;
uint32_t        Use_RED_Mode              = USE_RED_QUEUE_DEF;
uint32_t        MachinesPerRack           = MACHINES_PER_RACK_DEF;
uint32_t        NumberofRacks             = NUMBER_OF_RACKS_DEF;
uint32_t        Machines2ToRRate          = MACHINES_TO_TOR_RATE_DEF;
string          LinkDelay                 = LINK_DELAY_DEF;
uint32_t        NumberofApps              = NUMBER_OF_APPS_DEF;
uint32_t        QueuelengthToR            = QUEUELENGTH_TOR_DEF;
uint32_t        QueuelengthFabric         = QUEUELENGTH_FABRIC_DEF;
uint32_t        ThresholdToR_min          = THRESHOLD_TOR_DEF_MIN;
uint32_t        ThresholdToR_max          = THRESHOLD_TOR_DEF_MAX;
//uint32_t        ThresholdFabric           = THRESHOLD_FABRIC_DEF;
//uint32_t        QueriesPerApp             = QUERIES_PER_APP_DEF;
double          InterarrivalMultiplier    = INTERARRIVAL_MULTIPLIER_DEF; 
uint32_t        Stop                      = STOP_DEF;
bool            UseLongFlows              = USE_LONG_FLOWS;
int             TightDeadlines            = TIGHT_DEADLINE;
int             Print_st                  = PRINT_STATUS;
uint32_t        SegmentSize               = SEGMENT_SIZE;
uint32_t        rto_min                   = RTO_MIN;
uint32_t        delack_timeout            = RTO_MIN;
uint16_t			no_priority		  	  = 0;

// TODO: If these change per app, may need to parameterize that

// derived knobs
uint32_t        NumberofEndPoints; 
uint32_t        ToR2FabricRate;
std::string     Machines2ToRRateStr;
std::string     ToR2FabricRateStr;
uint32_t        NodesPerApp;

//declare a set of deadlines/one per app - used as ports too
//uint16_t Deadlines[]      = {30,   30,   30,   30,   30,  30,  30,30,30,30};
//note that do not exceed 128k
uint16_t Deadlines[]      = {20,           30,           35,           40,           45,  };
uint32_t MaxBytesPerApp[] = {32 * 1024,   32 * 1024,   32 * 1024,   32 * 1024,   32 * 1024};
//uint32_t delaytime = 0; //add by xl for two 4M long flows test.
//uint32_t MaxBytesPerApp[] = {15 * 1024,   20 * 1024,   25 * 1024,   30 * 1024,   35 * 1024};
//uint32_t MaxBytesPerApp[] = {90 * 1024,   120 * 1024,   140 * 1024,   160 * 1024,   180 * 1024};
//uint32_t MaxBytesPerApp[] = {2 * 1024,   6 * 1024,   10 * 1024,   14 * 1024,   18 * 1024};

// declare the node pointers
vector < Ptr <Node> > nEnd;
vector < Ptr <Node> > nToR;
Ptr<Node> nFabric;

NS_LOG_COMPONENT_DEFINE ("DataCenter");

string uint2str(uint32_t number)
{

  std::stringstream out;
  out << number;
  return out.str();
}

void QueueBytes (uint32_t oldbytes, uint32_t newbytes)
{//xl
	  PRINT_SIMPLE(newbytes   <<"   =newbytes,   "<< oldbytes<<  "  =oldbytes");
}
void QueuePackets (uint32_t oldpackets, uint32_t newpackets)
{// by xl
	  PRINT_SIMPLE (newpackets<<"   =newpackets, "<< oldpackets<<"  =oldpackets");
}

void ArbiQueueBytes (uint32_t oldbytes, uint32_t newbytes)
{//xl
	  PRINT_SIMPLE(newbytes   <<"   =arbbytes,   "<< oldbytes<<  "  =oldbytes");
}
void ArbiQueuePackets (uint32_t oldpackets, uint32_t newpackets)
{// by xl
	  PRINT_SIMPLE (newpackets<<"   =arbpackets, "<< oldpackets<<"  =oldpackets");
}


void DropTrace (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("droptrace: original packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<", pppheader size:"<<ppp.GetSerializedSize() <<", ipv4header size:"<<iph.GetSerializedSize() 
			  <<", tcpheader size:"<<tcph.GetSerializedSize() 
			  //<<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  ppph:"<<ppp <<",  ipv4h:"<<iph <<",  tcph:"<<tcph);
}
void EnQTrace (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("enqtrace: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph << "main, EnQTrace, 174.");
	//if (tcph.IsAck()){
	  	//PRINT_SIMPLE ("this is a ack.");
		//return true;
	//}
	//return false;
}
void DeQTrace (Ptr<Packet const> p)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("deqtrace: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph);
}


void queueIsAck (Ptr<Packet const> p, uint32_t* ret)
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  PRINT_SIMPLE ("enqtrace: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
			  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  <<",  tcph:"<<tcph << ", main(), queueIsAck 194");
	if (tcph.IsAck())
	  *ret = 1;
	else
	  *ret = 0;
	
	PRINT_SIMPLE ("p2p tx ack? ret="<<ret <<",*ret="<<*ret << ", main(), queueIsAck 201");
}

void queueCompPkt (Ptr<Packet const> p1, Ptr< Packet > p2, uint32_t* ret )
{// by xl
	PppHeader ppph1, ppph2;
	Ipv4Header iph1, iph2;
	TcpHeader tcph1, tcph2;
	Ptr <Packet> np1 = p1->Copy();
	Ptr <Packet> np2 = p2->Copy();
	np1->RemoveHeader (ppph1);
	np1->RemoveHeader (iph1);
	np1->RemoveHeader (tcph1);
	np2->RemoveHeader (ppph2);
	np2->RemoveHeader (iph2);
	np2->RemoveHeader (tcph2);
	
	uint32_t ip1s = iph1.GetSource().Get();
	uint32_t ip1d = iph1.GetDestination().Get();
	uint32_t ip2s = iph2.GetSource().Get();
	uint32_t ip2d = iph2.GetDestination().Get();

	if (ip1s == ip2s && ip1d == ip2d && tcph1.GetSourcePort() == tcph2.GetSourcePort() && tcph1.GetDestinationPort() == tcph2.GetDestinationPort() )
	  *ret = 1;
	else
	  *ret = 0;
	

	char tmp[200] = {0}, tmp2[200]={0}; 
	sprintf(tmp, "packet1: %d.%d.%d.%d:%d->%d.%d.%d.%d:%d, seq %d, ack %d, win %d. ", (ip1s>>24)&0xff, (ip1s>>16)&0xff, (ip1s>>8)&0xff, (ip1s>>0)&0xff, tcph1.GetSourcePort(), 
			(ip1d>>24)&0xff, (ip1d>>16)&0xff, (ip1d>>8)&0xff, (ip1d>>0)&0xff, tcph1.GetDestinationPort(),
			tcph1.GetSequenceNumber().GetValue(), tcph1.GetAckNumber().GetValue(), tcph1.GetWindowSize() );
	sprintf(tmp2, "packet2: %d.%d.%d.%d:%d->%d.%d.%d.%d:%d, seq %d, ack %d, win %d. ", (ip2s>>24)&0xff, (ip2s>>16)&0xff, (ip2s>>8)&0xff, (ip2s>>0)&0xff, tcph2.GetSourcePort(), 
			(ip2d>>24)&0xff, (ip2d>>16)&0xff, (ip2d>>8)&0xff, (ip2d>>0)&0xff, tcph2.GetDestinationPort(),
			tcph2.GetSequenceNumber().GetValue(), tcph2.GetAckNumber().GetValue(), tcph2.GetWindowSize() );
	PRINT_SIMPLE ("compare ack. ret="<<*ret << tmp <<"\n\t\t" << tmp2);

}

void p2pIsAck (Ptr<Packet const> p, uint32_t* ret, std::string &ip_port )
{// by xl
	PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  //PRINT_SIMPLE ("enqtrace: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
	//		  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  //<<",  tcph:"<<tcph);
	char tmp[200] = {0}; 
	uint32_t ip1 = iph.GetSource().Get();
	uint32_t ip2 = iph.GetDestination().Get();
	sprintf(tmp, "%d.%d.%d.%d:%d->%d.%d.%d.%d:%d, seq=%d, ack=%d, adv_win=%d ", (ip1>>24)&0xff, (ip1>>16)&0xff, (ip1>>8)&0xff, (ip1>>0)&0xff, tcph.GetSourcePort(), 
			(ip2>>24)&0xff, (ip2>>16)&0xff, (ip2>>8)&0xff, (ip2>>0)&0xff, tcph.GetDestinationPort(),
			tcph.GetSequenceNumber().GetValue(), tcph.GetAckNumber().GetValue(), tcph.GetWindowSize() );
	std::string stmp(tmp);
	ip_port.append(stmp);
	//PRINT_SIMPLE ("main ip_port= "<<ip_port.c_str() << ", main(), p2pIsAck, 252.");
	if (tcph.IsAck() && !tcph.IsFin()) //fin pkt is not ack, no need to buffer fin pkt and assign win to fin pkt.
		//fin pkt does not trigger data pkt. don't worry.
	  *ret = 1;
	else
	  *ret = 0;
	
	PRINT_SIMPLE ("p2p rx. Is this a ack? ret="<<ret <<",*ret="<<*ret << ", "<< ip_port <<", "<< tcph <<", main(), p2pIsAck, 258.");
}

void p2pIsFin (Ptr<Packet const> p, uint32_t* ret )
{// by xl
	//PppHeader ppp;
	Ipv4Header iph;
	TcpHeader tcph;
	Ptr <Packet> packet = p->Copy();
	//packet->RemoveHeader (ppp);
	packet->RemoveHeader (iph);
	packet->RemoveHeader (tcph);
	  //PRINT_SIMPLE ("enqtrace: packet size:"<<p->GetSize() << ", data size:"<<packet->GetSize()
	//		  <<",  ip source("<<iph.GetSource() <<") to ip destination("<<iph.GetDestination() 
			  //<<"), source port:"<<tcph.GetSourcePort() <<", destination port:"<<tcph.GetDestinationPort() 
			  //<<", seq:"<<tcph.GetSequenceNumber() <<", ack:"<<tcph.GetAckNumber() 
			  //<<",  tcph:"<<tcph);
	char tmp[200] = {0}; 
	uint32_t ip1 = iph.GetSource().Get();
	uint32_t ip2 = iph.GetDestination().Get();
	sprintf(tmp, "%d.%d.%d.%d:%d->%d.%d.%d.%d:%d, seq=%d, ack=%d, adv_win=%d ", (ip1>>24)&0xff, (ip1>>16)&0xff, (ip1>>8)&0xff, (ip1>>0)&0xff, tcph.GetSourcePort(), 
			(ip2>>24)&0xff, (ip2>>16)&0xff, (ip2>>8)&0xff, (ip2>>0)&0xff, tcph.GetDestinationPort(),
			tcph.GetSequenceNumber().GetValue(), tcph.GetAckNumber().GetValue(), tcph.GetWindowSize() );
	std::string stmp(tmp);
	if (tcph.IsFin())
	  *ret = 1;
	else
	  *ret = 0;
	
	PRINT_SIMPLE ("p2p, Is this a fin? ret="<<ret <<",*ret="<<*ret << ", "<< stmp <<", "<< tcph <<", main(), p2pIsFin, 258.");
}


void p2pAssignwin (Ptr<Packet const> p, uint16_t win_size,  Ptr<Packet> &newp)
{//by xl
	//PppHeader ppph;
	Ipv4Header iph;
	TcpHeader tcph;
	newp = p->Copy();

	//newp->RemoveHeader (ppph);
	newp->RemoveHeader (iph);
	newp->RemoveHeader (tcph);

	tcph.SetWindowSize(std::min(win_size, tcph.GetWindowSize()));

	newp->AddHeader(tcph);
	newp->AddHeader(iph);
	//newp->AddHeader(ppph);
	
	PRINT_SIMPLE ("p2p assign win "<< std::min(win_size, tcph.GetWindowSize()) <<", "<< tcph <<", main(), p2pAssignwin, 278.");
	 
}

void queueAssignwin (Ptr<Packet const> p, uint16_t win_size,  Ptr<Packet> &newp)
{//by xl
	PppHeader ppph;
	Ipv4Header iph;
	TcpHeader tcph;
	newp = p->Copy();

	newp->RemoveHeader (ppph);
	newp->RemoveHeader (iph);
	newp->RemoveHeader (tcph);

	tcph.SetWindowSize(std::min(win_size, tcph.GetWindowSize()));

	newp->AddHeader(tcph);
	newp->AddHeader(iph);
	newp->AddHeader(ppph);
	
	PRINT_SIMPLE ("arbiqueue assign win "<< std::min(win_size, tcph.GetWindowSize()) <<", "<< tcph <<", main(), p2pAssignwin, 278.");
	 
}

void SetupToR()
{
  for (uint32_t i=0;i<NumberofEndPoints;i++)
  {
    // Each EndPoint 1 (ToR) + 1 (default)
    for (uint8_t j=1;j<nEnd[i]->GetNDevices();j++)
    {
      Ptr <PointToPointNetDevice> P2PNetDevice = nEnd[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
      NS_ASSERT (P2PNetDevice);
      Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
      NS_ASSERT (dtq);
      dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthToR));
    }
  }
  for (uint32_t i=0;i<NumberofRacks;i++)
  {
    // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
    for (uint8_t j=1;j<nToR[i]->GetNDevices() - 1;j++)
    {
      Ptr <PointToPointNetDevice> P2PNetDevice = nToR[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
      NS_ASSERT (P2PNetDevice);
  
	  P2PNetDevice->TraceConnectWithoutContext ("IsAck", MakeCallback (p2pIsAck )); //xl for SCC
	  P2PNetDevice->TraceConnectWithoutContext ("IsFin", MakeCallback (p2pIsFin )); //xl for SCC
	  //P2PNetDevice->TraceConnectWithoutContext ("Assign_win", MakeCallback (p2pAssignwin )); //xl for SCC
	  P2PNetDevice->SetAttribute ("SCC_mode",UintegerValue(SCCMode));

	  if (Use_RED_Mode  ==  1) {
          Ptr<RedQueue> redq = CreateObject<RedQueue> ();
          P2PNetDevice->SetQueue (redq);
	  }
	  else if (SCCMode  ==  1) {
          Ptr<Arbi_Queue> arbq = CreateObject<Arbi_Queue> ();
	  	  arbq->TraceConnectWithoutContext ("Comp_pkt", MakeCallback (queueCompPkt )); //xl for SCC
	  	  arbq->TraceConnectWithoutContext ("Assign_win", MakeCallback (queueAssignwin )); //xl for SCC
          P2PNetDevice->SetInputQueue (arbq); //scc input queue.
		  P2PNetDevice->SetAttribute ("SCC_K",UintegerValue(10 * (SegmentSize+42+4+4+4) ));
	  }
	  else {

          Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
          NS_ASSERT (dtq);
          dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthToR));

          //Ptr<DropTailQueue> inputdtq = P2PNetDevice->GetInputQueue()->GetObject <DropTailQueue> (); //for sdtcp
          //NS_ASSERT (inputdtq);
          //inputdtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthToR));

	  }
    }
  }
}

void SetupFabric()
{
  for (uint32_t i=0;i<NumberofRacks;i++)
  {
    // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
    for (uint8_t j=nToR[i]->GetNDevices() - 1;j<nToR[i]->GetNDevices();j++)
    {
      Ptr <PointToPointNetDevice> P2PNetDevice = nToR[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
      NS_ASSERT (P2PNetDevice);
	  
	  P2PNetDevice->TraceConnectWithoutContext ("IsAck", MakeCallback (p2pIsAck )); //xl for SCC
	  //P2PNetDevice->TraceConnectWithoutContext ("Assign_win", MakeCallback (p2pAssignwin )); //xl for SCC
      
	  if (Use_RED_Mode              == 1) {
	      Ptr<RedQueue> redq = CreateObject<RedQueue> ();
          P2PNetDevice->SetQueue (redq);
	  }
	  else {

	      Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
	      NS_ASSERT (dtq);
          dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthFabric));
          
		  Ptr<DropTailQueue> inputdtq = P2PNetDevice->GetInputQueue()->GetObject <DropTailQueue> (); //for sdtcp
          NS_ASSERT (inputdtq);
          inputdtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthToR));
	  }
    }
  }

  for (uint8_t j=1;j<nFabric->GetNDevices();j++)
  {
    Ptr <PointToPointNetDevice> P2PNetDevice = nFabric->GetDevice(j)->GetObject <PointToPointNetDevice> ();
    NS_ASSERT (P2PNetDevice);
	
	if (Use_RED_Mode              == 1) {
	    Ptr<RedQueue> redq = CreateObject<RedQueue> ();
	    redq->SetQueueLimit (QueuelengthFabric);
	    //redq->SetTh (ThresholdFabric, ThresholdFabric);
        P2PNetDevice->SetQueue (redq);
	}
	else {
        Ptr<DropTailQueue> dtq = P2PNetDevice->GetQueue()->GetObject <DropTailQueue> ();
        NS_ASSERT (dtq);
        dtq->SetAttribute ("MaxPackets",UintegerValue(QueuelengthFabric));
	}
  }
}


/*void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  PRINT_SIMPLE (">>>>>>>>>>>>>>>>>>>>\t" << newCwnd);
  //cout << ">>>>>>>>>>>>>>>>>" << newCwnd; by xl on Sun May 19 02:14:01 CST 2013
}*/

int main (int argc, char *argv[])
{
  CommandLine cmd;
  
  // add cmd line arguments here
  cmd.AddValue ("RTCPMode",               "use RTCP  or not   (0 or 1)",                  RTCPMode);
  cmd.AddValue ("ICTCPMode",              "use ICTCP or not   (0 or 1)",                  ICTCPMode);
  cmd.AddValue ("DCTCPMode",              "use TCP   or DCTCP (0 or 1)",                  DCTCPMode);
  cmd.AddValue ("RTCPMode",               "use TCP   or RTCP (0 or 1)",                    RTCPMode);
  cmd.AddValue ("RDTCPMode",              "use TCP   or RDTCP (0 or 1)",                  RDTCPMode);
  cmd.AddValue ("RDTCP_CONTROLLER_Mode",  "use RDTCP or RDTCP controller(0 or 1)",        RDTCP_CONTROLLER_Mode);
  cmd.AddValue ("SCCMode",                "SCC Mode  (0 or 1)",                           SCCMode);
  cmd.AddValue ("ECNMode",                "ECN Mode  (0 or 1)",                           ECNMode);
  cmd.AddValue ("Use_RED_Mode",           "Use RED queue for queue in switch.",           Use_RED_Mode);
  cmd.AddValue ("MachinesPerRack",        "Number of nodes",                              MachinesPerRack);
  cmd.AddValue ("NumberofRacks",          "Number of Racks",                              NumberofRacks);
  cmd.AddValue ("Machines2ToRRate",       "Link rate from machines to ToR switch",        Machines2ToRRate);
  cmd.AddValue ("LinkDelay",              "Propagation delay in all links",               LinkDelay);
  cmd.AddValue ("NumberofApps",           "Number of Apps",                               NumberofApps);
  cmd.AddValue ("QueuelengthToR",         "Queuelength of ToR Switch",                    QueuelengthToR);
  cmd.AddValue ("QueuelengthFabric",      "Queuelength of Fabric Switch",                 QueuelengthFabric);
  cmd.AddValue ("ThresholdToR_min",       "Threshold min of ToR queue",                   ThresholdToR_min);
  cmd.AddValue ("ThresholdToR_max",       "Threshold max of ToR queue",                   ThresholdToR_max);
//  cmd.AddValue ("ThresholdFabric",        "Threshold of Fabric queue",                    ThresholdFabric);
  cmd.AddValue ("InterarrivalMultiplier", "InterarrivalMultiplier",                       InterarrivalMultiplier);
  cmd.AddValue ("Stop",                   "Time to stop the simulation",                  Stop);
  cmd.AddValue ("UseLongFlows",           "Inject long flows as welln",                   UseLongFlows);
  cmd.AddValue ("TightDeadlines",         "% to shave off deadlines",                     TightDeadlines);
  cmd.AddValue ("SegmentSize",            "SegmentSize for tcp",                          SegmentSize);
  cmd.AddValue ("rto_min",                "rto_min for retransmission",                   rto_min);
  cmd.AddValue ("no_priority",            "do not use flow priority.",  no_priority);

  cmd.Parse (argc,argv);

  // calculate the derived knobs
  NumberofEndPoints     = MachinesPerRack * NumberofRacks;
  ToR2FabricRate        = MachinesPerRack * Machines2ToRRate;

  Machines2ToRRateStr   = uint2str(Machines2ToRRate);
  Machines2ToRRateStr   += "Gbps";
  ToR2FabricRateStr     = uint2str(ToR2FabricRate);
  ToR2FabricRateStr     += "Gbps";

  NodesPerApp           = NumberofEndPoints/NumberofApps;

  assert(NumberofEndPoints % NumberofApps == 0);

  for (uint32_t i = 0; i < NumberofApps; i++) {
    Deadlines[i] = ceil(((double)(100 - TightDeadlines) / (double)(100)) * Deadlines[i]);
  }

  PRINT_SIMPLE("RTCPMode                          : " << RTCPMode);
  PRINT_SIMPLE("ICTCPMode                         : " << ICTCPMode);
  PRINT_SIMPLE("DCTCPMode                         : " << DCTCPMode);
  PRINT_SIMPLE("RDTCPMode                         : " << RDTCPMode);
  PRINT_SIMPLE("RDTCP_CONTROLLER_Mode             : " << RDTCP_CONTROLLER_Mode);
  PRINT_SIMPLE("SCCMode                           : " << SCCMode);
  PRINT_SIMPLE("ECNMode                           : " << ECNMode);
  PRINT_SIMPLE("Use_RED_Mode                      : " << Use_RED_Mode);
  PRINT_SIMPLE("MachinesPerRack                   : " << MachinesPerRack);
  PRINT_SIMPLE("NumberofRacks(ToR Swicthes)       : " << NumberofRacks);
  PRINT_SIMPLE("Machines2ToRRate                  : " << Machines2ToRRateStr.c_str());
  PRINT_SIMPLE("LinkDelay                         : " << LinkDelay.c_str());
  PRINT_SIMPLE("ToR2FabricRate                    : " << ToR2FabricRateStr.c_str());
  PRINT_SIMPLE("NumberofEndPoints                 : " << NumberofEndPoints);
  PRINT_SIMPLE("NumberofApps                      : " << NumberofApps);
  PRINT_SIMPLE("NodesPerApp                       : " << NodesPerApp);
  PRINT_SIMPLE("QueuelengthToR                    : " << QueuelengthToR);
  PRINT_SIMPLE("QueuelengthFabric                 : " << QueuelengthFabric);
  PRINT_SIMPLE("ThresholdToR_min                  : " << ThresholdToR_min);
  PRINT_SIMPLE("ThresholdToR_max                  : " << ThresholdToR_max);
//  PRINT_SIMPLE("ThresholdFabric                   : " << ThresholdFabric);
  PRINT_SIMPLE("InterarrivalMultiplier            : " << InterarrivalMultiplier);
  PRINT_SIMPLE("Stop                              : " << Stop);
  PRINT_SIMPLE("UseLongFLows                      : " << UseLongFlows);
  PRINT_SIMPLE("TightDeadlines                    : " << TightDeadlines);
  PRINT_SIMPLE("SegmentSize                       : " << SegmentSize);
  PRINT_SIMPLE("rto_min                           : " << rto_min);
  PRINT_SIMPLE("no_priority                       : " << no_priority);

  //LogComponentEnable("Buffer", LOG_LEVEL_ALL);
  //LogComponentEnable("Packet", LOG_LEVEL_ALL);
  //LogComponentEnable("RedQueue", LOG_LEVEL_ALL);
  //LogComponentEnable("Queue", LOG_LEVEL_ALL);
  //LogComponentEnable("DropTailQueue", LOG_LEVEL_ALL);
  //LogComponentEnable("BulkSendApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable("PacketSink", LOG_LEVEL_DEBUG);
  LogComponentEnable("TcpNewReno", LOG_LEVEL_DEBUG);
  //LogComponentEnable("TcpSocketBase", LOG_LEVEL_ALL);
  LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);
  //LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
  //LogComponentEnable("TcpTxBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable("TcpRxBuffer", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
  LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_DEBUG);
  //LogComponentEnable("PointToPointChannel", LOG_LEVEL_ALL);
  //LogComponentEnable("Node", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4ListRouting", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_ALL);
  //LogComponentEnable("Ipv4EndPointDemux", LOG_LEVEL_ALL);


  // Configure ECN QueueThresholds
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (SegmentSize));
  Config::SetDefault ("ns3::PointToPointNetDevice::Mtu", UintegerValue (SegmentSize+42+4+4+4)); //4 for tcp option header for flowsize for rdtcp controller.
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1024* 128)); //buffer size 128k
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1024* 128));
  Config::SetDefault ("ns3::Ipv4L3Protocol::Offer_Load", UintegerValue (0));

  //if (RTCPMode) 
   // delack_timeout            = rto_min / 2;
 // else delack_timeout            = rto_min;
    delack_timeout            = rto_min;
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue(MilliSeconds(rto_min))); // by xl on Mon May  6 01:28:40 CST 2013
  Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(delack_timeout))); // by xl on Mon May  6 01:28:40 CST 2013
  Config::SetDefault ("ns3::RttEstimator::MinRTO", TimeValue(MilliSeconds(rto_min))); // by xl on Mon May  6 01:28:40 CST 2013
  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue(MilliSeconds(rto_min))); // by xl on Mon May  6 01:28:40 CST 2013
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  //Config::SetDefault ("ns3::TcpSocketBase::DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
  //Config::SetDefault ("ns3::TcpSocketBase::RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl

  Config::SetDefault ("ns3::RedQueue::Mode", StringValue ("QUEUE_MODE_PACKETS")); //real world configure by xl  //as a matter of fact, it's in bytes, in droptail.cc
  Config::SetDefault ("ns3::RedQueue::MeanPktSize", UintegerValue (1500));
  //Config::SetDefault ("ns3::RedQueue::Wait", BooleanValue (true));
  //Config::SetDefault ("ns3::RedQueue::Gentle", BooleanValue (true));
  //Config::SetDefault ("ns3::RedQueue::QW", DoubleValue (0.002));
  Config::SetDefault ("ns3::RedQueue::MinTh", DoubleValue (ThresholdToR_min)); //10 default, low queue latency is important, by xl
  Config::SetDefault ("ns3::RedQueue::MaxTh", DoubleValue (ThresholdToR_max)); // max has no effect, and when use byte, min=128kb/2.
  Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (QueuelengthToR));//128*1024B buffer, 128*1024/1500 = 87
  Config::SetDefault ("ns3::RedQueue::ECN", UintegerValue (ECNMode));

  Config::SetDefault ("ns3::DropTailQueue::Mode", StringValue ("QUEUE_MODE_PACKETS")); //real world configure by xl  //as a matter of fact, it's in bytes, in droptail.cc
  Config::SetDefault ("ns3::DropTailQueue::ECN", UintegerValue (ECNMode));
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (QueuelengthToR));
  Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue (128*1024));
  Config::SetDefault ("ns3::DropTailQueue::Offer_Load", UintegerValue (0));
  //for QUEUE_MODE_PACKETS mode
  Config::SetDefault ("ns3::DropTailQueue::ECN_Threshold", UintegerValue (ThresholdToR_max));

  //scc
  Config::SetDefault ("ns3::Arbi_Queue::Mode", StringValue ("QUEUE_MODE_BYTES")); //scc in byte.
  Config::SetDefault ("ns3::Arbi_Queue::MaxBytes", UintegerValue (QueuelengthToR*1500));
  



  
  // Declare EndPoints
  for (uint32_t i=0;i<NumberofEndPoints;i++)
    nEnd.push_back(CreateObject<Node> ());

  PRINT_SIMPLE("=====Allocated " << nEnd.size() << " EndPoint nodes..");

  // Declare ToR Swictch
  for (uint32_t i=0;i<NumberofRacks;i++)
    nToR.push_back(CreateObject<Node> ());
  
  PRINT_SIMPLE("=====Allocated " << nToR.size() << " ToR nodes..");
  
  // Declare Fabric switch
  nFabric = CreateObject<Node> ();
  
  PRINT_SIMPLE("=====Allocated Fabric node..");

  // Install IP
  PRINT_SIMPLE("=====Install IP..");
  InternetStackHelper internet;
  for (uint32_t i=0;i<NumberofEndPoints;i++)
    internet.Install (nEnd[i]);
  for (uint32_t i=0;i<NumberofRacks;i++)
    internet.Install (nToR[i]);
  internet.Install (nFabric);

  // Install p2p links between EndPoints and ToR Swicthes
  PRINT_SIMPLE("=====Install p2p links between points and switches..");
  PointToPointHelper p2pEnd2ToR;
  p2pEnd2ToR.SetDeviceAttribute           ("DataRate",  StringValue (Machines2ToRRateStr.c_str()));
  p2pEnd2ToR.SetChannelAttribute          ("Delay",     StringValue (LinkDelay.c_str()));
  vector <NetDeviceContainer> dEnddToR;
  for (uint32_t i=0;i<NumberofRacks;i++)
    for (uint32_t j=0;j<MachinesPerRack;j++)
      dEnddToR.push_back(p2pEnd2ToR.Install (nEnd[(i * MachinesPerRack) + j],nToR[i]));

  // Install p2p links between ToR Swicthes and Fabric Switch
  PRINT_SIMPLE("=====Install p2p links between switches and fabric..");
  PointToPointHelper p2pToR2Fabric;
  p2pToR2Fabric.SetDeviceAttribute        ("DataRate",  StringValue (ToR2FabricRateStr.c_str()));
  p2pToR2Fabric.SetChannelAttribute       ("Delay",     StringValue (LinkDelay.c_str()));
  vector <NetDeviceContainer> dToRdFabric;
  for (uint32_t i=0;i<NumberofRacks;i++)
    dToRdFabric.push_back(p2pToR2Fabric.Install (nToR[i],nFabric));

  // Assign IP address at each interface - first on the interface between EndPoints and TOR switches
  PRINT_SIMPLE("=====Assign IP address at each interface..");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");

  // At the interface between Ends and ToR
  PRINT_SIMPLE("=====At the interface between Ends and ToR..");
  vector <Ipv4InterfaceContainer> iEndiToR;
  for (uint32_t i=0;i<NumberofRacks;i++)
    for (uint32_t j=0;j<MachinesPerRack;j++)
    {
      iEndiToR.push_back(ipv4.Assign (dEnddToR[(i * MachinesPerRack) + j]));
      ipv4.NewNetwork();
    }

  // At the interface between TOR and Fabric
  PRINT_SIMPLE("=====At the interface between TOR and Fabric..");
  vector <Ipv4InterfaceContainer> iToRiFabric;
  for (uint32_t i=0;i<NumberofRacks;i++)
  {
      iToRiFabric.push_back(ipv4.Assign (dToRdFabric[i]));
      ipv4.NewNetwork();
  }

  PRINT_SIMPLE("=====SetupToR: red or droptail..");
  SetupToR();
  PRINT_SIMPLE("=====SetupFabric: red or droptail..");
  SetupFabric();

  // Walk thro all the nodes, and print their IP addresses
  PRINT_SIMPLE("=====IP Assignment at EndPoints=========");
  for (uint32_t i=0;i<NumberofEndPoints;i++)
    for (uint32_t j=1;j < nEnd[i]->GetObject<Ipv4>()->GetNInterfaces();j++)
      PRINT_SIMPLE("Node " << nEnd[i]->GetId() << ": " << nEnd[i]->GetObject<Ipv4>()->GetAddress(j,0).GetLocal());

  PRINT_SIMPLE("=====IP Assignment at ToR=========");
  for (uint32_t i=0;i<NumberofRacks;i++)
    for (uint32_t j=1;j < nToR[i]->GetObject<Ipv4>()->GetNInterfaces();j++)
      PRINT_SIMPLE("Node " << nToR[i]->GetId() << ": " << nToR[i]->GetObject<Ipv4>()->GetAddress(j,0).GetLocal());
  
  PRINT_SIMPLE("=====IP Assignment at Fabric=========");
  for (uint32_t j=1;j < nFabric->GetObject<Ipv4>()->GetNInterfaces();j++)
      PRINT_SIMPLE("Node " << nFabric->GetId() << ": " << nFabric->GetObject<Ipv4>()->GetAddress(j,0).GetLocal());

  // setup ip routing tables to get total ip-level connectivity.
  PRINT_SIMPLE("=====Globalrouting table set=========");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  // Now we need to instal the apps onto the nodes, this is where the game is!

  // First get a random shuffle of EndPoints 
  PRINT_SIMPLE("=====app set=========");
  vector <uint32_t> AppIdx;
  for (uint32_t i=0;i<NumberofEndPoints;i++)
  {
    AppIdx.push_back(i);
  }

  random_shuffle(AppIdx.begin(),AppIdx.end());

  // Now create a map from Node -> App, and App -> Node
  map<uint32_t, uint32_t> Node2App;
  map<uint32_t, uint32_t> App2Node;

  for (uint32_t i=0;i<NumberofEndPoints;i++)
  {
    Node2App[i] = AppIdx[i];
    App2Node[AppIdx[i]] = i;
  }

  // There are "NumberofApps" sink nodes, and "NodesPerApp"-1 source apps

  // make "NumberofApps" random processes that can generate delays for us
  vector <ExponentialVariable> StartDelay;
  vector <ExponentialVariable> LongFlowStartDelay;
  for (uint32_t i=0;i<NumberofApps;i++)
  {
    ExponentialVariable tmp (InterarrivalMultiplier * Deadlines[i], 2 * InterarrivalMultiplier * Deadlines[i]);
    StartDelay.push_back(tmp);
    ExponentialVariable tmp2 (300, 2 * 300); // start a long flow every 300 msec on avg
    LongFlowStartDelay.push_back(tmp2);
  }

  PRINT_SIMPLE("=====App to Node Assignment=========");
  
  uint32_t NodeIdx;
  //uint32_t AppCount = 0; by xl on Mon May 20 01:29:21 CST 2013
  uint32_t SinkNodeIdx;
  uint32_t Count = 0;
  uint32_t CountSinkIdx = 0;
  bool first_leaf = false;

  int num_flows = 0;

  vector<Time> delay;
  vector<Time> long_flow_delay;
  Time delay_prev;
  Time long_flow_delay_prev;
  // Just walk thro the app-nodes and designate source and sink nodes! 
  for (uint32_t i=0;i<NumberofEndPoints;i++)
  {
    NodeIdx = App2Node[i];
    if (i % NodesPerApp == 0)
    {
      // This is a sink node, install the sink apps here
      PRINT_SIMPLE("============================================================");
      //PRINT_SIMPLE("App : " << AppCount++); by xl on Mon May 20 01:29:29 CST 2013
      PRINT_SIMPLE("============================================================");
      PRINT_SIMPLE("[" << i << "] Sink: " << NodeIdx);
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), Deadlines[Count]));

      sink.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      sink.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      sink.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      sink.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      sink.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      sink.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl
      sink.SetAttribute ("RDTCP_no_priority", UintegerValue (no_priority)); // by xl

      ApplicationContainer sinkApps1 = sink.Install (nEnd[NodeIdx]);
      sinkApps1.Start  (Seconds (0.0));
      PacketSinkHelper long_flow_sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), LONG_FLOW_PORT));

      long_flow_sink.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      long_flow_sink.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      long_flow_sink.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      long_flow_sink.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      long_flow_sink.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      long_flow_sink.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl
      long_flow_sink.SetAttribute ("RDTCP_no_priority", UintegerValue (no_priority)); // by xl

      ApplicationContainer sinkApps2 = long_flow_sink.Install (nEnd[NodeIdx]);
      sinkApps2.Start  (Seconds (0.0));
      // remeber this node idx as sink (target for other app nodes)
      SinkNodeIdx = NodeIdx;
      CountSinkIdx = Count;
      
      // Set the start time for all the source nodes, at each "query" iteration
      delay.clear();
      //ExponentialVariable start_ex (5, 10); by xl on Tue Jun 25 16:19:42 CST 2013
      //delay_prev = MilliSeconds(start_ex); //mod by xl for shuffle by xl on Tue Jun 25 16:19:47 CST 2013
      delay_prev = MilliSeconds(0.0);
      int j = 0;
      while (delay_prev < MilliSeconds(Stop))
      {
        // create a new start delay for each app
        //delay.push_back(delay_prev + MilliSeconds (StartDelay[Count].GetValue()));
        delay.push_back(delay_prev + MilliSeconds (10)); //for incast, the first app is at 10ms, not 0ms.
        delay_prev = delay[j];
        j++;
      }
      long_flow_delay.clear();
      long_flow_delay_prev = MilliSeconds(10.0);
      j = 0;
      long_flow_delay.push_back(long_flow_delay_prev);   //add by xl for longflow
      /*while (long_flow_delay_prev < MilliSeconds(Stop))
      {
        long_flow_delay.push_back(long_flow_delay_prev + MilliSeconds (LongFlowStartDelay[Count].GetValue()));
        long_flow_delay_prev = long_flow_delay[j];
        j++;
      }*/
      Count++;
      first_leaf = UseLongFlows;
    }
    else
    {
      // One leaf will be a long flow sender all else are OLDI leaves
      if (first_leaf) {
        first_leaf = false;
        for (uint32_t j = 0; j < long_flow_delay.size(); j++) {
          PRINT_SIMPLE("[" << i << "] Long Flow Source Node " << NodeIdx << ": ---> Sink Node " << SinkNodeIdx << ", size:30* 4024 * 1024B");
          BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress(nEnd[SinkNodeIdx]->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), LONG_FLOW_PORT));
          source.SetAttribute ("MaxBytes", UintegerValue (30 * 1024 * 1024)); // send 1 MB
          source.SetAttribute ("SendSize", UintegerValue (30 * 1024 * 1024)); // cannot call send() for size larger than TCP buffer = 128 MB
          source.SetAttribute ("Deadline", UintegerValue (0)); // cannot call send() for size larger than TCP buffer = 128 MB
          source.SetAttribute ("FlowId", UintegerValue (num_flows)); // cannot call send() for size larger than TCP buffer = 128 MB

      source.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      source.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      source.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      source.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl
      source.SetAttribute ("RDTCP_no_priority", UintegerValue (no_priority)); // by xl

          ApplicationContainer sourceApps = source.Install (nEnd[NodeIdx]);
          PRINT_SIMPLE("Start longflow at " << NodeIdx << " (Iteration = " << j << "), at time = " << long_flow_delay[j]<<", flowid= "<<num_flows);
          sourceApps.Start(long_flow_delay[j]);
          num_flows++;
        }
      } else {
        // This is an OLDI leafe node
        for (uint32_t j=0;j<delay.size();j++)
        {
          PRINT_SIMPLE("[" << i << "] Source Node " << NodeIdx << ": ---> Sink Node " << SinkNodeIdx << ", size: "<<MaxBytesPerApp[CountSinkIdx]<<" B");
          uint16_t port_num = Deadlines[CountSinkIdx];
          BulkSendHelper source ("ns3::TcpSocketFactory",InetSocketAddress(nEnd[SinkNodeIdx]->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port_num));
          source.SetAttribute ("MaxBytes", UintegerValue (MaxBytesPerApp[CountSinkIdx]));
          source.SetAttribute ("SendSize", UintegerValue (MaxBytesPerApp[CountSinkIdx]));
          source.SetAttribute ("Deadline", UintegerValue (Deadlines[CountSinkIdx]*1000000)); // cannot call send() for size larger than TCP buffer = 128 MB
          source.SetAttribute ("FlowId", UintegerValue (num_flows)); // cannot call send() for size larger than TCP buffer = 128 MB
      source.SetAttribute ("DCTCP_MODE", UintegerValue (DCTCPMode)); // by xl
      source.SetAttribute ("RTCP_MODE",  UintegerValue (RTCPMode)); // by xl
      source.SetAttribute ("ICTCP_MODE", UintegerValue (ICTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MODE", UintegerValue (RDTCPMode)); // by xl
      source.SetAttribute ("RDTCP_MAXQUEUE_SIZE", UintegerValue (ThresholdToR_max)); // by xl
      source.SetAttribute ("RDTCP_CONTROLLER_MODE", UintegerValue (RDTCP_CONTROLLER_Mode)); // by xl
      source.SetAttribute ("RDTCP_no_priority", UintegerValue (no_priority)); // by xl
          ApplicationContainer sourceApps = source.Install (nEnd[NodeIdx]);
		  //delay[j] += MilliSeconds (delaytime); //xl for two 4M longflows 
		  //delaytime += 5;
          PRINT_SIMPLE("Start app at " << NodeIdx << " (Iteration = " << j << "), at time = " << delay[j]<<", flowid= "<<num_flows);
          sourceApps.Start  (delay[j]);
		  
		  //Ptr<Application> app_p = sourceApps.Get (0);
          //Ptr<BulkSendApplication> bs = sourceApps.Get (0)->GetObject<BulkSendApplication>();
		  //bs->GetSocket()->SetAttribute ("RcvBufSize", UintegerValue (MaxBytesPerApp[CountSinkIdx]));
		  //bs->GetSocket()->SetAttribute ("SndBufSize", UintegerValue (MaxBytesPerApp[CountSinkIdx]));

          num_flows++;
		  
		  //trace congestion win by xl by xl on Sun May 19 17:10:02 CST 2013
          //PRINT_SIMPLE("appcontainer = "<< sourceApps.GetN()<<", app node = "<<app_p->GetNode()->GetId () ); by xl on Sun May 19 20:55:41 CST 2013
		  
		  //->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange)); by xl on Sun May 19 20:48:03 CST 2013

		  //PRINT_SIMPLE ("bs->getsocket="<<bs->GetSocket()<<", bs->m_socket="<<bs->m_socket); by xl on Sun May 19 20:55:13 CST 2013

		  //if (sk = 0) PRINT_SIMPLE("no sk returned! "); //sk is private member function, so cann't be used for trace. by xl on Sun May 19 20:44:28 CST 2013
			
		  //PRINT_SIMPLE("sk = "<<sk<<", m_socket = " << ); by xl on Sun May 19 20:44:33 CST 2013
		  //sk->GetTxAvailable (); by xl on Sun May 19 20:36:00 CST 2013
		  //sk->GetIpTos (); by xl on Sun May 19 17:31:59 CST 2013
          //sk->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange)); by xl on Sun May 19 17:23:58 CST 2013



        }
      } // if OLDI leaf
    } // if leaf
  } // for all nodes
 
  //xl calculate throughput using flowmonitor 
  //FlowMonitorHelper flowmonhelp;
  //Ptr<FlowMonitor> monitor = flowmonhelp.Install(nEnd[1]);
  
  
    Print_st = 1;
    if (Print_st)
	{
  Ptr<Queue> tor_to_sink_device_queue = nToR[0]->GetDevice(SinkNodeIdx + 1 )->GetObject <PointToPointNetDevice> ()->GetQueue(); 
  Ptr<Queue> tor_to_sink_device_inputqueue = nToR[0]->GetDevice(SinkNodeIdx + 1 )->GetObject <PointToPointNetDevice> ()->GetInputQueue(); 
  tor_to_sink_device_queue ->TraceConnectWithoutContext ("QueueBytes", MakeCallback (QueueBytes )); //xl
  tor_to_sink_device_queue ->TraceConnectWithoutContext ("QueuePackets", MakeCallback (QueuePackets )); //xl
  
  tor_to_sink_device_inputqueue ->TraceConnectWithoutContext ("QueueBytes", MakeCallback (ArbiQueueBytes )); //xl
  tor_to_sink_device_inputqueue ->TraceConnectWithoutContext ("QueuePackets", MakeCallback (ArbiQueuePackets )); //xl


  tor_to_sink_device_queue ->TraceConnectWithoutContext ("Drop", MakeCallback (DropTrace )); //xl
  tor_to_sink_device_queue ->TraceConnectWithoutContext ("Enqueue", MakeCallback (EnQTrace )); //xl
  //tor_to_sink_device_queue ->TraceConnectWithoutContext ("IsAck", MakeCallback (queueIsAck )); //xl
  tor_to_sink_device_queue ->TraceConnectWithoutContext ("Dequeue", MakeCallback (DeQTrace )); //xl
	}





  // test
  Simulator::Run ();


  /*monitor->CheckForLostPackets (); 
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonhelp.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
  	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    if ((t.sourceAddress=="10.1.1.1" && t.destinationAddress == "10.1.15.1"))
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
     	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
      }
  }*/

  //monitor->SerializeToXmlFile("lab-1.flowmon", true, true); by xl on Tue May 28 17:02:02 CST 2013

    if (Print_st)
    {
       for (uint32_t i=0;i<NumberofRacks;i++)
	   {
          cout << "*** all ToR=["<< NumberofRacks <<"], from ToR["<<i<<"] device num = ["<<nToR[i]->GetNDevices() - 1<<"]" << endl;
         // Each ToR switch has "MachinesPerRack" + 1 (Fabric) + 1 (default) p2p devices
         for (uint8_t j=1; j<nToR[i]->GetNDevices() - 1; j++)
       {
          Ptr <PointToPointNetDevice> nd = nToR[i]->GetDevice(j)->GetObject <PointToPointNetDevice> ();
		  if (Use_RED_Mode              == 1){
          RedQueue::Stats st = StaticCast<RedQueue> (nd->GetQueue ())->GetStats ();
          cout << "*** RED stats from ToR["<<i<<"] device[ "<<(uint32_t)j<<" ] queue ***" << endl;
          //cout << "\t " << st.unforcedDrop << " drops due prob mark" << endl;
          cout << "\t " << st.forcedDrop << " drops due hard mark" << endl;
          //cout << "\t " << st.qLimDrop << " drops due queue full" << endl; //forcedrop >= qLimDrop, because force drop may no be queue full.
          cout << "\t " << st.ecns << " marks of ecn" << endl;
		  }
		  else {
          cout << "*** DropTailQueue stats from ToR["<<i<<"] device[ "<<(uint32_t)j<<" ] queue ***" << endl;
          cout << "\t " << StaticCast<DropTailQueue> (nd->GetQueue ())->Get_Droptimes ()  << " drops due queue full" << endl;
          cout << "\t " << StaticCast<DropTailQueue> (nd->GetQueue ())->m_ecns << " marks of ecn" << endl;
		  }
       }
	   }
	   
	   //for incast 
		cout << "***sink side of tor device :["<<  SinkNodeIdx + 1  <<"] node["<< nToR[0]->GetId()  <<"],tor ip:"<< nToR[0]->GetObject<Ipv4>()->GetAddress(SinkNodeIdx + 1,0).GetLocal() <<", end ip : " << nEnd[SinkNodeIdx]->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << endl;
          Ptr <PointToPointNetDevice> nd = nToR[0]->GetDevice(SinkNodeIdx + 1 )->GetObject <PointToPointNetDevice> ();
		  if (Use_RED_Mode              == 1)
		  {
          RedQueue::Stats st = StaticCast<RedQueue> (nd->GetQueue ())->GetStats ();
          //cout << "\t " << st.unforcedDrop << " drops due prob mark" << endl;
          cout << "\t " << st.forcedDrop << " drops due hard mark at bottleneck" << endl;
          //cout << "\t " << st.qLimDrop << " drops due queue full" << endl;
          cout << "\t " << st.ecns << " marks of ecn at bottleneck" << endl;
		  }
		  else {
          cout << "\t " << StaticCast<DropTailQueue> (nd->GetQueue ())->Get_Droptimes ()  << " drops due queue full at bottleneck" << endl;
          cout << "\t " << StaticCast<DropTailQueue> (nd->GetQueue ())->m_ecns << " marks of ecn at bottleneck" << endl;
		  }
 
    }
    



  Simulator::Destroy ();

  PRINT_SIMPLE("Num Flows: " << num_flows);

  return 0;
}
