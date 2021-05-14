 /**
 * ns3 Project 
 * 
 * by Mumtaz Cem Eris
 * 
 * BLG517 Modelling and Performance Analysis of Networks
 * */
 
 
 #include <iostream>
 #include "ns3/core-module.h"
 #include "ns3/network-module.h"
 #include "ns3/internet-module.h"
 #include "ns3/point-to-point-module.h"
 #include "ns3/netanim-module.h"
 #include "ns3/applications-module.h"
 #include "ns3/point-to-point-layout-module.h"
 #include "ns3/flow-monitor-module.h"
 #include "ns3/traffic-control-module.h"
 
 using namespace ns3;
 
 
void droprate(Ptr<FlowMonitor> monitor) {
  uint32_t packetsDroppedByQueueDisc = 0;
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
	  if (i->second.packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
      {
		  packetsDroppedByQueueDisc = i->second.packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
	  }
	  std::cout <<":" << packetsDroppedByQueueDisc << ":";
  }
  std::cout << std::endl;
  Simulator::Schedule(Seconds(1), &droprate, monitor);
}

 int main (int argc, char *argv[])
 {
   Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
   Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("500Kbps"));
   uint32_t    nLeftLeaf = 2;
   uint32_t    nRightLeaf = 0;
   double simulationTime = 20; //seconds//
   // Seed number of the simulation
   int SeedRunNumber   = 444;
   std::string animFile = "hw-dumbbell-animation.xml" ;  // Name of file for animation output
   SeedManager::SetRun(SeedRunNumber);
   
   // Create the point-to-point link helpers
   PointToPointHelper pointToPointRouter, p2p_all;
   pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue ("500kbps"));
   pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("1ms"));
   pointToPointRouter.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue("1p"));
   PointToPointHelper pointToPointLeaf;
   pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("500kbps"));
   pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("1ms"));
   pointToPointLeaf.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue("1p"));
 
   PointToPointDumbbellHelper d (nLeftLeaf, pointToPointLeaf,
                                 nRightLeaf, pointToPointLeaf,
                                 pointToPointRouter);

   NodeContainer nodes = NodeContainer (d.GetLeft(), d.GetRight ());
   NetDeviceContainer devices = p2p_all.Install (nodes);
   
   // Install Stack
   InternetStackHelper stack;
   d.InstallStack (stack);
   
   // Assign IP Addresses
   d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                          Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                          Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));
 
   // Install on/off app on all right side nodes
   OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
   clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.05]"));
   // the packet arrival rate is 100 pkt/sec, so we set mean value as 1/100
   clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=0.01|Bound=0]"));
   ApplicationContainer clientApps, clientApps2;
 
   AddressValue remoteAddress (InetSocketAddress (Ipv4Address("10.3.1.2"), 1000));
   clientHelper.SetAttribute ("Remote", remoteAddress);
   clientApps.Add (clientHelper.Install (d.GetLeft (0)));
   //clientApps.Add (clientHelper.Install (d.GetLeft (1)));
   clientApps.Start (Seconds (0.0));
   clientApps.Stop (Seconds (20.0));
   
   clientApps2.Add (clientHelper.Install (d.GetLeft (1)));
   clientApps2.Start (Seconds (5.0));
   clientApps2.Stop (Seconds (15.0));
 
   /**
    * Monitoring part
    * */
   AsciiTraceHelper ascii;
   pointToPointRouter.EnableAsciiAll (ascii.CreateFileStream ("hw_dumbbell.tr"));
   //pointToPointRouter.EnablePcapAll ("hw_dumbbell", false);
   FlowMonitorHelper flowmon;
   Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
   uint32_t packetsDroppedByQueueDisc = 0;
   uint64_t bytesDroppedByQueueDisc = 0;
   // Set the bounding box for animation
   d.BoundingBox (1, 1, 100, 100);
 
   // Create the animation object and configure for specified output
   AnimationInterface anim (animFile);
   anim.EnablePacketMetadata (); // Optional
   anim.EnableIpv4L3ProtocolCounters (Seconds (0), Seconds (simulationTime)); // Optional*
   anim.EnableQueueCounters (Seconds (0), Seconds (simulationTime), Seconds (1)); // Optional
   
   Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
   
   std::cout << "Packets Dropped by Queue Disc: " <<  std::endl;
   Simulator::Schedule(Seconds(1), &droprate, monitor);
   // Set up the actual simulation
   Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
   Simulator::Stop (Seconds (simulationTime));
   Simulator::Run ();
   monitor->CheckForLostPackets ();
   FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
   for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / (i->second.timeLastTxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
      if (i->second.packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE_DISC)
      {
		  packetsDroppedByQueueDisc = i->second.packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
		  bytesDroppedByQueueDisc = i->second.bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
      }
	  std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc
					<< " / " << bytesDroppedByQueueDisc << std::endl;
	  uint32_t packetsDroppedByNetDevice = 0;
	  uint64_t bytesDroppedByNetDevice = 0;
	  if (i->second.packetsDropped.size () > Ipv4FlowProbe::DROP_QUEUE)
      {
		  packetsDroppedByNetDevice = i->second.packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
		  bytesDroppedByNetDevice = i->second.bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
      }
		std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice
            << " / " << bytesDroppedByNetDevice << std::endl;
  
    }
   std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
   Simulator::Destroy ();
   return 0;
 }
