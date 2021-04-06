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

#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SocketOptionsIpv4");

int counter = 0;

char *GeneratePayload(int len) {
    srand (time(NULL));
    char s[len+1];
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    s[len] = 0;
    char *str= new char[len];
    strcpy(str,s);
    return str;
}

void ReceivePacket (Ptr<Socket> socket)
{
  NS_LOG_INFO ("Received one packet!");
  Ptr<Packet> packet = socket->Recv ();
  uint8_t *buffer = new uint8_t[packet->GetSize ()];
  packet->CopyData(buffer, packet->GetSize ());
  std::string s = std::string(buffer, buffer+packet->GetSize());
  //std::cout<<"Received payload: "<< s << std::endl;
  std::ostringstream counter_str;
     counter_str << counter;
  std::string file_name = "packet_payload_received" + counter_str.str() + ".txt"; 
  std::ofstream payload_rec_file ( file_name ); 
  payload_rec_file << s;
  // Close the file
  payload_rec_file.close();
  counter++;
}

static void SendFragment (Ptr<Socket> socket, Ptr<Packet> frag)
{
  NS_LOG_INFO ("Sending fragment!");
  socket->Send (frag);
  NS_LOG_INFO ("Sent!");
}

static void SendPacket (Ptr<Socket> socket, uint32_t pktSize, 
                        uint32_t pktCount, Time pktInterval_i )
{
  // Generate payload
  char *data = GeneratePayload(pktSize);
	std::string payload(data);
  //std::cout<<"Created payload: "<< payload << std::endl;
  
  std::ofstream payload_file ( "packet_payload_sent.txt" ); 
  payload_file << payload;
  // Close the file
  payload_file.close();

  
  if (pktCount > 0)
    {
      Ptr<Packet> packet = Create<Packet>((uint8_t*) payload.c_str(), pktSize + 1);

      // Packet fragmentation
      Ptr<Packet> frag0 = packet->CreateFragment (0, pktSize/4);
      Ptr<Packet> frag1 = packet->CreateFragment (pktSize/4, pktSize/4);
      Ptr<Packet> frag2 = packet->CreateFragment (pktSize/2, pktSize/4);
      Ptr<Packet> frag3 = packet->CreateFragment ((3*pktSize)/4, pktSize/4); 
      
      //Schedule Fragments
      double pktInterval = 2.0;
      Time interPacketInterval = Seconds (pktInterval);
      Simulator::Schedule (interPacketInterval, &SendFragment, 
                           socket, frag0);
      interPacketInterval = Seconds (pktInterval+1);
      Simulator::Schedule (interPacketInterval, &SendFragment, 
                           socket, frag0);
      interPacketInterval = Seconds (pktInterval+3);
      Simulator::Schedule (interPacketInterval, &SendFragment, 
                           socket, frag1);
      interPacketInterval = Seconds (pktInterval+2);
      Simulator::Schedule (interPacketInterval, &SendFragment, 
                           socket, frag2);
      interPacketInterval = Seconds (pktInterval+3);
      Simulator::Schedule (interPacketInterval, &SendFragment, 
                           socket, frag3);
         
    }
  else
    {
      socket->Close ();
    }
}

int 
main (int argc, char *argv[])
{
  uint32_t packetSize = 60000; // 87.36kb, 65536, 4096 default
  uint32_t packetCount = 1;
  double packetInterval = 1.0;

  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (2);

  InternetStackHelper internet;
  internet.Install (n);

  Address serverAddress;

  NS_LOG_INFO ("Create channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("4Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("0.5ms"));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500)); //1400 original
  NetDeviceContainer d = csma.Install (n);


  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (d);
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

  source->Connect (remote);

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("socket_60k.tr"));
  csma.EnablePcapAll ("socket_60k", false);

  //Schedule SendPacket
  Time interPacketInterval = Seconds (packetInterval);
  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &SendPacket, 
                                  source, packetSize, packetCount, interPacketInterval);

  NS_LOG_INFO ("Run Simulation.");
  //Simulator::Stop (Seconds (100));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
