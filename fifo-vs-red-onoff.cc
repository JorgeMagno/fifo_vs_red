#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <iomanip>
#include <map>

using namespace ns3;

// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;

int main (int argc, char *argv[])
{
  global_start_time = 0.0;
  global_stop_time = 320.0;
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time;
  client_start_time = sink_start_time + 1.0;
  client_stop_time = global_stop_time - 1.0;

  uint32_t    nLeaf = 120;
  uint32_t    maxPackets = 100;
  uint32_t    modeBytes  = 0;
  uint32_t    queueDiscLimitPackets = 100;
  double      minTh = 40;
  double      maxTh = 80;
  uint32_t    pktSize = 512;
  std::string appDataRate = "20Mbps";
  std::string queueDiscType = "RED";
  uint16_t port = 5001;
  std::string bottleNeckLinkBw = "3Mbps";
  std::string bottleNeckLinkDelay = "20ms";

  CommandLine cmd;
  cmd.AddValue ("nLeaf",     "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("maxPackets","Max Packets allowed in the device queue", maxPackets);
  cmd.AddValue ("queueDiscLimitPackets","Max Packets allowed in the queue disc", queueDiscLimitPackets);
  cmd.AddValue ("queueDiscType", "Set QueueDisc type to Fifo or RED", queueDiscType);
  cmd.AddValue ("appPktSize", "Set OnOff App Packet Size", pktSize);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("modeBytes", "Set QueueDisc mode to Packets <0> or bytes <1>", modeBytes);

  cmd.AddValue ("redMinTh", "RED queue minimum threshold", minTh);
  cmd.AddValue ("redMaxTh", "RED queue maximum threshold", maxTh);
  cmd.Parse (argc,argv);

  if ((queueDiscType != "RED") && (queueDiscType != "Fifo"))
    {
      NS_ABORT_MSG ("Invalid queue disc type: Use --queueDiscType=RED or --queueDiscType=PfifoFast");
    }

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));

  //Config::SetDefault ("ns3::QueueBase::MaxSize",
    //                  QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, maxPackets)));

  if (!modeBytes)
  {
    Config::SetDefault ("ns3::RedQueueDisc::MaxSize",
                        QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
    Config::SetDefault ("ns3::FifoQueueDisc::MaxSize",
                        QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
  }
  else
  {
    Config::SetDefault ("ns3::RedQueueDisc::MaxSize",
                        QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, queueDiscLimitPackets * pktSize)));
    minTh *= pktSize;
    maxTh *= pktSize;
  }

  if (queueDiscType == "RED")
  {   
    Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (minTh));
    Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (maxTh));
    Config::SetDefault ("ns3::RedQueueDisc::LinkBandwidth", StringValue (bottleNeckLinkBw));
    Config::SetDefault ("ns3::RedQueueDisc::LinkDelay", StringValue (bottleNeckLinkDelay));
    Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (pktSize));
  }

  // Create the point-to-point link helpers
  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute  ("DataRate", StringValue (bottleNeckLinkBw));
  bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("20Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("1ms"));

  PointToPointDumbbellHelper d (nLeaf, pointToPointLeaf,
                                nLeaf, pointToPointLeaf,
                                bottleNeckLink);

  // Install Stack
  InternetStackHelper stack;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
     stack.Install (d.GetLeft (i));
    }
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
     stack.Install (d.GetRight (i));
    }

  stack.Install (d.GetLeft ());
  stack.Install (d.GetRight ());
  TrafficControlHelper tchBottleneck;

  if (queueDiscType == "Fifo")
    {
      tchBottleneck.SetRootQueueDisc ("ns3::FifoQueueDisc");
    }
  else if (queueDiscType == "RED")
    {
      tchBottleneck.SetRootQueueDisc ("ns3::RedQueueDisc");
    }

  QueueDiscContainer queueDiscs =  tchBottleneck.Install (d.GetLeft ()->GetDevice (0));
  tchBottleneck.Install (d.GetRight ()->GetDevice (0));

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // Install on/off app on all left side nodes
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps; 
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      sinkApps.Add (packetSinkHelper.Install (d.GetRight (i)));
    }
  sinkApps.Start (Seconds (sink_start_time));
  sinkApps.Stop (Seconds (sink_stop_time));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      // Create an on/off app sending packets to the left side
      AddressValue remoteAddress (InetSocketAddress (d.GetRightIpv4Address (i), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (d.GetLeft (i)));
    }
  clientApps.Start (Seconds (client_start_time)); // Start after sink
  clientApps.Stop (Seconds (client_stop_time)); // Stop before the sink

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  AsciiTraceHelper ascii;
  bottleNeckLink.EnableAscii (ascii.CreateFileStream ("40-80.tr"), 0, 0);

  std::cout << "Running the simulation" << std::endl;
  Simulator::Run ();

  uint64_t totalRxBytesCounter = 0;
  for (uint32_t i = 0; i < sinkApps.GetN (); i++)
    {
      Ptr <Application> app = sinkApps.Get (i);
      Ptr <PacketSink> pktSink = DynamicCast <PacketSink> (app);
      totalRxBytesCounter += pktSink->GetTotalRx ();
    }

  std::cout << "\nGoodput Mbps:" << std::endl;
  std::cout << (totalRxBytesCounter * 8/ 1e6)/Simulator::Now ().GetSeconds () << std::endl;

  NS_LOG_UNCOND ("----------------------------\nQueueDisc Type:" 
                 << queueDiscType 
                 << "\nGoodput Bytes/sec:" 
                 << totalRxBytesCounter/Simulator::Now ().GetSeconds ()); 
  NS_LOG_UNCOND ("----------------------------");


  QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();
  std::cout << "*** RED stats from Node left queue disc ***" << std::endl;
  std::cout << st << std::endl;

  std::cout << "Destroying the simulation" << std::endl;
  Simulator::Destroy ();
  return 0;
}
