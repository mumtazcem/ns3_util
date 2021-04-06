/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

// Network topology
//
//       n0              n1
//       |               |
//       =================
//              LAN
//
// - UDP flows from n0 to n1

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SocketOptionsIpv4");

void ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet = socket->Recv ();
  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  packet->CopyData(buffer, packet->GetSize ());
  std::string s = std::string(buffer, buffer+packet->GetSize());
  std::cout<<"Received payload: "<< s << std::endl;
}

static void SendPacket (Ptr<Socket> socket, uint32_t pktSize, 
                        uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
	    std::ostringstream msg; msg << "Federated Learning packet number: " << std::to_string(pktCount) << '\0';
      uint16_t packetSize = msg.str().length()+1;
      Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), packetSize);
      socket->Send (packet);
      Simulator::Schedule (pktInterval, &SendPacket, 
                           socket, packetSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

int 
main (int argc, char *argv[])
{
//
// Allow the user to override any of the defaults and the above Bind() at
// run-time, via command-line arguments
//
  uint32_t packetSize = 1024;
  uint32_t packetCount = 50;
  double packetInterval = 1.0;
  // this is the default error rate of our link, that is, the the probability of a single
  // byte being 'corrupted' during transfer.
  double errRate = 0.005;   //0.000001 original


  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);

  Address serverAddress;

  NS_LOG_INFO ("Create channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("0.5s"));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500)); //1400 original
  NetDeviceContainer devices = csma.Install (n);


  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);
  serverAddress = Address(i.GetAddress (1));
   
  NS_LOG_INFO ("Create sockets.");
  //Receiver socket on n1
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (n.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 4477);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
  
  //Sender socket on n0
  Ptr<Socket> source = Socket::CreateSocket (n.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (i.GetAddress (1), 4477);

  // Error model
  DoubleValue rate (errRate);
  Ptr<RateErrorModel> em1 = 
    CreateObjectWithAttributes<RateErrorModel> ("RanVar", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"), "ErrorRate", rate);
  Ptr<RateErrorModel> em2 = 
    CreateObjectWithAttributes<RateErrorModel> ("RanVar", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"), "ErrorRate", rate);

  // This enables the specified errRate on both link endpoints.
  devices.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em2));

  //Set socket options, it is also possible to set the options after the socket has been created/connected.
  source->Connect (remote);

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("socket_edited.tr"));
  csma.EnablePcapAll ("socket_edited", false);

  //Schedule SendPacket
  Time interPacketInterval = Seconds (packetInterval);
  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &SendPacket, 
                                  source, packetSize, packetCount, interPacketInterval);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
